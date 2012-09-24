#include "StdTuvokDefines.h"
#include <algorithm>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <sstream>
#ifdef WRITE_SHADERS
# include <fstream>
# include <iterator>
#endif

#include "GLVolumePool.h"
#include "Basics/MathTools.h"
#include "Basics/TuvokException.h"
#include "Basics/Threads.h"
#include "GLSLProgram.h"
#include "IO/uvfDataset.h"
#include "Renderer/VisibilityState.h"

enum BrickIDFlags {
  BI_MISSING = 0,
  BI_CHILD_EMPTY,
  BI_EMPTY,
  BI_FLAG_COUNT
};

using namespace tuvok;

namespace tuvok {

  class AsyncVisibilityUpdater : public ThreadClass {
  public:
    AsyncVisibilityUpdater(GLVolumePool& m_Parent);
    ~AsyncVisibilityUpdater();

    void Restart(const VisibilityState& visibility);
    bool Pause(); // returns true if thread is busy, false means thread is idle
    virtual void Resume();

    // call pause before using any getter and probably resume or restart afterwards
    const VisibilityState& GetVisibility() const { return m_Visibility; }
#ifdef GLVOLUMEPOOL_PROFILE
    struct Stats {
      double fTimeTotal;
      double fTimeInterruptions;
      uint32_t iInterruptions;
    };        
    Stats const& GetStats() const { return m_Stats; }
#endif

  private:
    bool Continue(); // returns true if thread should continue its work, false signals restart request
    virtual void ThreadMain(void*);

    GLVolumePool& m_Pool;
    VisibilityState m_Visibility;

    enum State {
      RestartRequested,
      PauseRequested,
      Paused,
      Busy,
      Idle
    } m_eState;
    CriticalSection m_StateGuard;
    WaitCondition m_Parent;
    WaitCondition m_Worker;

#ifdef GLVOLUMEPOOL_PROFILE
    Timer m_Timer;
    Stats m_Stats;
#endif
  };

} // namespace tuvok

static UINTVECTOR3 GetLoDSize(const UINTVECTOR3& volumeSize, uint32_t iLoD) {
  UINTVECTOR3 vLoDSize(uint32_t(ceil(double(volumeSize.x)/MathTools::Pow2(iLoD))), 
                       uint32_t(ceil(double(volumeSize.y)/MathTools::Pow2(iLoD))),
                       uint32_t(ceil(double(volumeSize.z)/MathTools::Pow2(iLoD))));
  return vLoDSize;
}

static FLOATVECTOR3 GetFloatBrickLayout(const UINTVECTOR3& volumeSize,
                                        const UINTVECTOR3& maxInnerBrickSize,
                                        uint32_t iLoD) {
  FLOATVECTOR3 baseBrickCount(float(volumeSize.x)/maxInnerBrickSize.x, 
                              float(volumeSize.y)/maxInnerBrickSize.y,
                              float(volumeSize.z)/maxInnerBrickSize.z);

  baseBrickCount /= float(MathTools::Pow2(iLoD));

  // subtract smallest possible floating point epsilon from integer values that would mess up the brick index computation in the shader
  if (float(uint32_t(baseBrickCount.x)) == baseBrickCount.x)
    baseBrickCount.x -= baseBrickCount.x * std::numeric_limits<float>::epsilon();
  if (float(uint32_t(baseBrickCount.y)) == baseBrickCount.y)
    baseBrickCount.y -= baseBrickCount.y * std::numeric_limits<float>::epsilon();
  if (float(uint32_t(baseBrickCount.z)) == baseBrickCount.z)
    baseBrickCount.z -= baseBrickCount.z * std::numeric_limits<float>::epsilon();

  return baseBrickCount;
}


static UINTVECTOR3 GetBrickLayout(const UINTVECTOR3& volumeSize,
                                  const UINTVECTOR3& maxInnerBrickSize,
                                  uint32_t iLoD) {
  UINTVECTOR3 baseBrickCount(uint32_t(ceil(double(volumeSize.x)/maxInnerBrickSize.x)), 
                             uint32_t(ceil(double(volumeSize.y)/maxInnerBrickSize.y)),
                             uint32_t(ceil(double(volumeSize.z)/maxInnerBrickSize.z)));
  return GetLoDSize(baseBrickCount, iLoD);
}

GLVolumePool::GLVolumePool(const UINTVECTOR3& poolSize, UVFDataset* pDataset, GLenum filter, bool bUseGLCore)
  : m_pPoolMetadataTexture(NULL),
    m_pPoolDataTexture(NULL),
    m_vPoolCapacity(0,0,0),
    m_poolSize(poolSize),
    m_maxInnerBrickSize(UINTVECTOR3(pDataset->GetMaxUsedBrickSizes())-UINTVECTOR3(pDataset->GetBrickOverlapSize())*2),
    m_maxTotalBrickSize(pDataset->GetMaxUsedBrickSizes()),
    m_volumeSize(pDataset->GetDomainSize()),
    m_iLoDCount(uint32_t(pDataset->GetLargestSingleBrickLod(0)+1)),
    m_filter(filter),
    m_iTimeOfCreation(2),
    m_iMetaTextureUnit(0),
    m_iDataTextureUnit(1),
    m_bUseGLCore(bUseGLCore),
    m_iInsertPos(0),
    m_pDataset(pDataset),
    m_pUpdater(NULL),
    m_bVisibilityUpdated(false)
#ifdef GLVOLUMEPOOL_PROFILE
    , m_Timer()
    , m_TimesRecomputeVisibilityForBrickPool(100)
    , m_TimesMetaTextureUpload(100)
    , m_TimesRecomputeVisibility(100)
#endif
    , m_iMinMaxScalarTimestep(0)
    , m_iMinMaxGradientTimestep(0)
    , m_BrickIOTime(0.0)
    , m_BrickIOBytes(0)
{
  const uint64_t iBitWidth  = m_pDataset->GetBitWidth();
  const uint64_t iCompCount = m_pDataset->GetComponentCount();

  switch (iCompCount) {
    case 1 : m_format = GL_LUMINANCE; break;
    case 3 : m_format = GL_RGB; break;
    case 4 : m_format = GL_RGBA; break;
    default : throw Exception("Invalid Component Count", _func_, __LINE__);
  }

  switch (iBitWidth) {
    case 8 :  {
        m_type = GL_UNSIGNED_BYTE;
        switch (iCompCount) {
          case 1 : m_internalformat = GL_LUMINANCE8; break;
          case 3 : m_internalformat = GL_RGB8; break;
          case 4 : m_internalformat = GL_RGBA8; break;
          default : throw Exception("Invalid Component Count", _func_, __LINE__);
        }
      } 
      break;
    case 16 :  {
        m_type = GL_UNSIGNED_SHORT;
        switch (iCompCount) {
          case 1 : m_internalformat = GL_LUMINANCE16; break;
          case 3 : m_internalformat = GL_RGB16; break;
          case 4 : m_internalformat = GL_RGBA16; break;
          default : throw Exception("Invalid Component Count", _func_, __LINE__);
        }
      } 
      break;
    case 32 :  {
        m_type = GL_FLOAT;
        switch (iCompCount) {
          case 1 : m_internalformat = GL_LUMINANCE32F_ARB; break;
          case 3 : m_internalformat = GL_RGB32F; break;
          case 4 : m_internalformat = GL_RGBA32F; break;
          default : throw Exception("Invalid Component Count", _func_, __LINE__);
        }
      } 
      break;
    default : throw Exception("Invalid bit width", _func_, __LINE__);
  }

  // fill the pool slot information
  const UINTVECTOR3 slotLayout = poolSize/m_maxTotalBrickSize;
  m_vPoolSlotData.reserve(slotLayout.volume());

  for (uint32_t z = 0;z<slotLayout.z;++z) {
    for (uint32_t y = 0;y<slotLayout.y;++y) {
      for (uint32_t x = 0;x<slotLayout.x;++x) {
        m_vPoolSlotData.push_back(PoolSlotData(UINTVECTOR3(x,y,z)));
      }
    }
  }

  // compute the LoD offset table, i.e. a table that holds
  // for each LoD the accumulated number of all bricks in
  // the lower levels, this is used to serialize a brick index
  uint32_t iOffset = 0;
  m_vLoDOffsetTable.resize(m_iLoDCount);
  for (uint32_t i = 0;i<m_vLoDOffsetTable.size();++i) {
    m_vLoDOffsetTable[i] = iOffset;
    iOffset += GetBrickLayout(m_volumeSize, m_maxInnerBrickSize, i).volume();
  }

  CreateGLResources();

  // duplicate minmax scalar data from dataset for efficient access
  m_vMinMaxScalar.resize(m_iTotalBrickCount);
  for (uint32_t i = 0; i < m_vMinMaxScalar.size(); i++) {
    UINTVECTOR4 const vBrickID = GetVectorBrickID(i);
    BrickKey const key = m_pDataset->TOCVectorToKey(vBrickID, m_iMinMaxScalarTimestep);
    InternalMaxMinComponent imme = m_pDataset->MaxMinForKey(key);
    m_vMinMaxScalar[i].min = imme.minScalar;
    m_vMinMaxScalar[i].max = imme.maxScalar;
  }

#ifndef GLVOLUMEPOOL_BUSY // if we want to simulate a busy async updater we need to make sure to instantiate it
  uint32_t const iAsyncUpdaterThreshold = 7500 * 5; // we can process 7500 bricks/ms (1500 running debug build)
  if (m_iTotalBrickCount > iAsyncUpdaterThreshold)
#endif
    m_pUpdater = new AsyncVisibilityUpdater(*this);
}

uint32_t GLVolumePool::GetLoDCount() const {
  return m_iLoDCount;
}

uint32_t GLVolumePool::GetIntegerBrickID(const UINTVECTOR4& vBrickID) const {
  UINTVECTOR3 bricks = GetBrickLayout(m_volumeSize, m_maxInnerBrickSize, vBrickID.w);
  return vBrickID.x + vBrickID.y * bricks.x + vBrickID.z * bricks.x * bricks.y + m_vLoDOffsetTable[vBrickID.w];
}

UINTVECTOR4 GLVolumePool::GetVectorBrickID(uint32_t iBrickID) const {
  auto up = std::upper_bound(m_vLoDOffsetTable.cbegin(), m_vLoDOffsetTable.cend(), iBrickID);
  uint32_t lod = uint32_t(up - m_vLoDOffsetTable.cbegin()) - 1;
  UINTVECTOR3 bricks = GetBrickLayout(m_volumeSize, m_maxInnerBrickSize, lod);
  iBrickID -= m_vLoDOffsetTable[lod];

  return UINTVECTOR4(iBrickID % bricks.x,
                     (iBrickID % (bricks.x*bricks.y)) / bricks.x,
                     iBrickID / (bricks.x*bricks.y),
                     lod);
}

UINTVECTOR3 const& GLVolumePool::GetPoolCapacity() const {
  return m_vPoolCapacity;
}

inline UINTVECTOR3 const& GLVolumePool::GetVolumeSize() const {
  return m_volumeSize;
}

inline UINTVECTOR3 const& GLVolumePool::GetMaxInnerBrickSize() const {
  return m_maxInnerBrickSize;
}

std::string GLVolumePool::GetShaderFragment(uint32_t iMetaTextureUnit,
                                            uint32_t iDataTextureUnit,
                                            MissingBrickStrategy strategy,
                                            const std::string& WsetPrefixName)
{
  // must have created GL resources before asking for shader
  if (!m_pPoolMetadataTexture  || !m_pPoolDataTexture) return "";

  m_iMetaTextureUnit = iMetaTextureUnit;
  m_iDataTextureUnit = iDataTextureUnit;

  std::stringstream ss;

#ifdef WRITE_SHADERS
  const char* shname = "volpool.glsl";
  std::ifstream shader(shname);
  if(shader) {
    MESSAGE("Reusing volpool.glsl shader on disk.");
    shader >> std::noskipws;
    std::string sh(
      (std::istream_iterator<char>(shader)),
      (std::istream_iterator<char>())
    );
    shader.close();
    return sh;
  }
#endif

  if (m_bUseGLCore)
    ss << "#version 420 core\n";  
  else
    ss << "#version 420 compatibility\n";

  FLOATVECTOR3 poolAspect(m_pPoolDataTexture->GetSize());
  poolAspect /= poolAspect.minVal();

 // get the maximum precision for floats (larger precisions would just append
 // zeroes)
  ss << std::setprecision(36);
  ss << "\n"
     << "layout(binding = " << m_iMetaTextureUnit << ") uniform usampler2D metaData;\n"
     << "#define iMetaTextureWidth " << m_pPoolMetadataTexture->GetSize().x << "\n"
     << "\n"
     << "#define BI_CHILD_EMPTY " << BI_CHILD_EMPTY << "\n"
     << "#define BI_EMPTY "       << BI_EMPTY << "\n"
     << "#define BI_MISSING "     << BI_MISSING << "\n"
     << "#define BI_FLAG_COUNT "  << BI_FLAG_COUNT << "\n"
     << "\n"
     << "layout(binding = " << m_iDataTextureUnit
     <<       ") uniform sampler3D volumePool;\n"
     << "#define iPoolSize ivec3(" << m_pPoolDataTexture->GetSize().x << ", " 
                                   << m_pPoolDataTexture->GetSize().y << ", " 
                                   << m_pPoolDataTexture->GetSize().z <<")\n"
     << "#define volumeSize vec3(" << m_volumeSize.x << ", " 
                                   << m_volumeSize.y << ", " 
                                   << m_volumeSize.z <<")\n"
     << "#define poolAspect vec3(" << poolAspect.x << ", " 
                                   << poolAspect.y << ", " 
                                   << poolAspect.z <<")\n"
     << "#define poolCapacity ivec3(" << m_vPoolCapacity.x << ", "
                                      << m_vPoolCapacity.y << ", "
                                      << m_vPoolCapacity.z <<")\n"
     << "// the total size of a brick in the pool, including the boundary\n"
     << "#define maxTotalBrickSize ivec3(" << m_maxTotalBrickSize.x << ", " 
                                           << m_maxTotalBrickSize.y << ", " 
                                           << m_maxTotalBrickSize.z <<")\n"
     << "// just the addressable (inner) size of a brick\n"
     << "#define maxInnerBrickSize  ivec3(" << m_maxInnerBrickSize.x << ", " 
                                            << m_maxInnerBrickSize.y << ", " 
                                            << m_maxInnerBrickSize.z <<")\n"
     << "// brick overlap voxels (in pool texcoords)\n"
     << "#define overlap vec3("
     << (m_maxTotalBrickSize.x - m_maxInnerBrickSize.x) /
        (2.0f*m_pPoolDataTexture->GetSize().x) << ", " 
     << (m_maxTotalBrickSize.y - m_maxInnerBrickSize.y) /
        (2.0f*m_pPoolDataTexture->GetSize().y) << ", " 
     << (m_maxTotalBrickSize.z - m_maxInnerBrickSize.z) /
        (2.0f*m_pPoolDataTexture->GetSize().z) <<")\n"
     << "uniform float fLoDFactor;\n"
     << "uniform float fLevelZeroWorldSpaceError;\n"
     << "uniform vec3 volumeAspect;\n"
     << "#define iMaxLOD " << m_iLoDCount-1 << "\n"
     << "uniform uint vLODOffset[" << m_iLoDCount << "] = uint[](";
  for (uint32_t i = 0;i<m_iLoDCount;++i) {
    ss << "uint(" << m_vLoDOffsetTable[i] << ")";
    if (i<m_iLoDCount-1) {
      ss << ", ";
    }
  }
  ss << ");\n"
     << "uniform vec3 vLODLayout[" << m_iLoDCount << "] = vec3[](\n";
  for (uint32_t i = 0;i<m_vLoDOffsetTable.size();++i) {
    FLOATVECTOR3 vLoDSize = GetFloatBrickLayout(m_volumeSize,
                                                m_maxInnerBrickSize, i);
    ss << "  vec3(" << vLoDSize.x << ", " << vLoDSize.y << ", "
       << vLoDSize.z << ")";
    if (i<m_iLoDCount-1) {
      ss << ",";
    }
    ss << "// Level " << i << "\n";
  }
  ss << ");\n"
     << "uniform uvec2 iLODLayoutSize[" << m_iLoDCount << "] = uvec2[](\n";
  for (uint32_t i = 0;i<m_vLoDOffsetTable.size();++i) {
    FLOATVECTOR3 vLoDSize = GetFloatBrickLayout(m_volumeSize,
                                                m_maxInnerBrickSize, i);    
    ss << "   uvec2(" << unsigned(ceil(vLoDSize.x)) << ", "
       << unsigned(ceil(vLoDSize.x)) * unsigned(ceil(vLoDSize.y)) << ")";
    if (i<m_iLoDCount-1) {
      ss << ",";
    }
    ss << "// Level " << i << "\n";
  }

  ss << ");\n"
     << "\n"
     << "uint Hash(uvec4 brick);\n"
     << "\n"
     << "uint ReportMissingBrick(uvec4 brick) {\n"
     << "  return Hash(brick);\n"
     << "}\n"
     << "\n";

  if (!WsetPrefixName.empty()) {
    ss << "uint " << WsetPrefixName << "Hash(uvec4 brick);\n"
       << "\n"
       << "uint ReportUsedBrick(uvec4 brick) {\n"
       << "  return " << WsetPrefixName << "Hash(brick);\n"
       << "}\n"
       << "\n";
  }

  ss << "ivec2 GetBrickIndex(uvec4 brickCoords) {\n"
     << "  uint iLODOffset  = vLODOffset[brickCoords.w];\n"
     << "  uint iBrickIndex = iLODOffset + brickCoords.x + "
        "brickCoords.y * iLODLayoutSize[brickCoords.w].x + "
        "brickCoords.z * iLODLayoutSize[brickCoords.w].y;\n"
        "  return ivec2(iBrickIndex % iMetaTextureWidth,"
                       "iBrickIndex / iMetaTextureWidth);\n"
     << "}\n"
     << "\n"
     << "uint GetBrickInfo(uvec4 brickCoords) {\n"
     << "  return texelFetch(metaData, GetBrickIndex(brickCoords), 0).r;\n"
     << "}\n"
     << "\n"
     << "uvec4 ComputeBrickCoords(vec3 normEntryCoords, uint iLOD) {\n"
     << "  return uvec4(normEntryCoords*vLODLayout[iLOD], iLOD);\n"
     << "}\n"
     << "\n"
     << "void GetBrickCorners(uvec4 brickCoords, out vec3 corners[2]) {\n"
     << "  corners[0] = vec3(brickCoords.xyz)   / vLODLayout[brickCoords.w];\n"
     << "  corners[1] = vec3(brickCoords.xyz+1) / vLODLayout[brickCoords.w];\n"
     << "}\n"
     << "\n"   
     << "vec3 BrickExit(vec3 pointInBrick, vec3 dir, in vec3 corners[2]) {\n"
     << "  vec3 div = 1.0 / dir;\n"
     << "  ivec3 side = ivec3(step(0.0,div));\n"
     << "  vec3 tIntersect;\n"
     << "  tIntersect.x = (corners[side.x].x - pointInBrick.x) * div.x;\n"
     << "  tIntersect.y = (corners[side.y].y - pointInBrick.y) * div.y;\n"
     << "  tIntersect.z = (corners[side.z].z - pointInBrick.z) * div.z;\n"
     << "  return pointInBrick +\n"
        "         min(min(tIntersect.x, tIntersect.y), tIntersect.z) * dir;\n"
     << "}\n"
     << " \n"
     << "uvec3 InfoToCoords(in uint brickInfo) {\n"
     << "  uint index = brickInfo-BI_FLAG_COUNT;\n"
     << "  uvec3 vBrickCoords;\n"
     << "  vBrickCoords.x = index % poolCapacity.x;\n"
     << "  vBrickCoords.y = (index / poolCapacity.x) % poolCapacity.y;\n"
     << "  vBrickCoords.z = index / (poolCapacity.x*poolCapacity.y);\n"
     << "  return vBrickCoords;\n"
     << "}\n"
     << " \n"
     << "void BrickPoolCoords(in uint brickInfo,  out vec3 corners[2]) {\n"
     << "  uvec3 poolVoxelPos = InfoToCoords(brickInfo) * maxTotalBrickSize;\n"
     << "  corners[0] = (vec3(poolVoxelPos)                   / vec3(iPoolSize))+ overlap;\n"
     << "  corners[1] = (vec3(poolVoxelPos+maxTotalBrickSize) / vec3(iPoolSize))- overlap;\n"
     << "}\n"
     << " \n"
     << "void NormCoordsToPoolCoords(in vec3 normEntryCoords,\n"
                                    "in vec3 normExitCoords,\n"
                                    "in vec3 corners[2],\n"
                                    "in uint brickInfo,\n"
                                    "out vec3 poolEntryCoords,\n"
                                    "out vec3 poolExitCoords,\n"
                                    "out vec3 normToPoolScale,\n"
                                    "out vec3 normToPoolTrans) {\n"
     << "  vec3 poolCorners[2];\n"
     << "  BrickPoolCoords(brickInfo, poolCorners);\n"
     << "  normToPoolScale = (poolCorners[1]-poolCorners[0])/(corners[1]-corners[0]);\n"
     << "  normToPoolTrans = poolCorners[0]-corners[0]*normToPoolScale;\n"
     << "  poolEntryCoords  = (normEntryCoords * normToPoolScale + normToPoolTrans);\n"
     << "  poolExitCoords   = (normExitCoords  * normToPoolScale + normToPoolTrans);\n"
     << "}\n"
     << "\n"
     << "bool GetBrick(in vec3 normEntryCoords, inout uint iLOD, in vec3 direction,\n"
     << "              out vec3 poolEntryCoords, out vec3 poolExitCoords,\n"
     << "              out vec3 normExitCoords, out bool bEmpty,\n"
     << "              out vec3 normToPoolScale, out vec3 normToPoolTrans, out uvec4 brickCoords) {\n"
     << "  normEntryCoords = clamp(normEntryCoords, 0.0, 1.0);\n"
     << "  bool bFoundRequestedResolution = true;\n"
     << "  brickCoords = ComputeBrickCoords(normEntryCoords, iLOD);\n"
     << "  uint  brickInfo   = GetBrickInfo(brickCoords);\n"
     << "  if (brickInfo == BI_MISSING) {\n"
     << "    uint iStartLOD = iLOD;\n"
     << "    ReportMissingBrick(brickCoords);\n"
     << "    // when the requested resolution is not present look for lower res\n"
     << "    bFoundRequestedResolution = false;\n"
     << "    do {\n"
     << "      iLOD++;\n"
     << "      brickCoords = ComputeBrickCoords(normEntryCoords, iLOD);\n"
     << "      brickInfo   = GetBrickInfo(brickCoords);\n"
     << "      ";
  switch(strategy) {
    case OnlyNeeded: /* nothing. */ break;
    case RequestAll:
      ss << "if(brickInfo == BI_MISSING) ReportMissingBrick(brickCoords);\n";
      break;
    case SkipOneLevel:
      ss << "if(brickInfo == BI_MISSING && iStartLOD+1 == iLOD) {\n      "
         << "  ReportMissingBrick(brickCoords);\n      "
         << "}\n";
      break;
    case SkipTwoLevels:
      ss << "if(brickInfo == BI_MISSING && iStartLOD+2 == iLOD) {\n      "
         << "  ReportMissingBrick(brickCoords);\n      "
         << "}\n";
      break;
  }
  ss << "    } while (brickInfo == BI_MISSING);\n"
     << "  }\n"
     << "  // next line check for BI_EMPTY or BI_CHILD_EMPTY (BI_MISSING is\n"
        "  // excluded by code above!)\n"
     << "  bEmpty = (brickInfo <= BI_EMPTY);\n"
     << "  if (bEmpty) {\n"
     << "    // when we find an empty brick check if the lower resolutions are also empty\n"
     << "    for (uint ilowResLOD = iLOD+1; ilowResLOD<iMaxLOD;++ilowResLOD) {\n"
     << "      uvec4 lowResBrickCoords = ComputeBrickCoords(normEntryCoords, ilowResLOD);\n"
     << "      uint lowResBrickInfo = GetBrickInfo(lowResBrickCoords);\n"
     << "      if (lowResBrickInfo == BI_CHILD_EMPTY) {\n"
     << "        brickCoords = lowResBrickCoords;\n"
     << "        brickInfo = lowResBrickInfo;\n"
     << "        iLOD = ilowResLOD;\n"
     << "      } else {\n"
     << "        break;\n"
     << "      }\n"
     << "    }\n"
     << "  }\n"
     << "  vec3 corners[2];\n"
     << "  GetBrickCorners(brickCoords, corners);\n"
     << "  normExitCoords = BrickExit(normEntryCoords, direction, corners);\n"
     << "  if (bEmpty) \n"
     << "    return bFoundRequestedResolution;\n"
     << "  NormCoordsToPoolCoords(normEntryCoords, normExitCoords, corners,\n"
     << "                         brickInfo, poolEntryCoords, poolExitCoords,\n"
     << "                         normToPoolScale, normToPoolTrans);\n";

  if (!WsetPrefixName.empty()) {
    ss << "  if (bFoundRequestedResolution) \n"
       << "    ReportUsedBrick(brickCoords);\n";
  }

  ss << "  return bFoundRequestedResolution;\n"
     << "}\n"
     << "\n"
     << "vec3 GetSampleDelta() {\n"
     << "  return 1.0/vec3(iPoolSize);\n"
     << "}\n"
     << "\n"
     << "vec3 TransformToPoolSpace(in vec3 direction, "
                                  "in float sampleRateModifier) {\n"
     << "  // normalize the direction\n"
     << "  direction *= volumeSize;\n"
     << "  direction = normalize(direction);\n"
     << "  // scale to volume pool's norm coordinates\n"
     << "  direction /= vec3(iPoolSize);\n"
     << "  // do (roughly) two samples per voxel and apply user defined sample density\n"
     << "  return direction / (2.0*sampleRateModifier);\n"
     << "}\n"
     << " \n"
     << "float samplePool(vec3 coords) {\n"
     << " return texture(volumePool, coords).r;\n"
     << "}\n"
     << " \n"
     << "float samplePoolAlpha(vec3 coords) {\n"
     << " return texture(volumePool, coords).a;\n"
     << "}\n"
     << " \n"
     << "vec3 samplePool3(vec3 coords) {\n"
     << " return texture(volumePool, coords).rgb;\n"
     << "}\n"
     << " \n"
     << "vec4 samplePool4(vec3 coords) {\n"
     << " return texture(volumePool, coords);\n"
     << "}\n"
     << " \n"
     << "uint ComputeLOD(float dist) {\n"
     << "  // opengl -> negative z-axis hence the minus\n"
     << "  return min(iMaxLOD, uint(log2(fLoDFactor*(-dist)/fLevelZeroWorldSpaceError)));\n"
     << "}\n";

#ifdef WRITE_SHADERS
  std::ofstream vpool(shname, std::ios::trunc);
  if(vpool) {
    MESSAGE("Writing new volpool shader.");
    const std::string& s = ss.str();
    std::copy(s.begin(), s.end(), std::ostream_iterator<char>(vpool, ""));
  }
  vpool.close();
#endif


  return ss.str();
}


void GLVolumePool::UploadBrick(uint32_t iBrickID, const UINTVECTOR3& vVoxelSize, void* pData, 
                               size_t iInsertPos, uint64_t iTimeOfCreation)
{
  PoolSlotData& slot = m_vPoolSlotData[iInsertPos];

  if (slot.ContainsVisibleBrick()) {
    m_vBrickMetadata[slot.m_iBrickID] = BI_MISSING;

    // upload paged-out meta texel
    UploadMetadataTexel(slot.m_iBrickID);

//    MESSAGE("Removing brick %i at queue position %i from pool", 
//        int(m_PoolSlotData[iInsertPos].m_iBrickID), 
//        int(iInsertPos));
  }

  slot.m_iBrickID = iBrickID;
  slot.m_iTimeOfCreation = iTimeOfCreation;

  uint32_t iPoolCoordinate = slot.PositionInPool().x +
                             slot.PositionInPool().y * m_vPoolCapacity.x +
                             slot.PositionInPool().z * m_vPoolCapacity.x * m_vPoolCapacity.y;

/*  MESSAGE("Loading brick %i at queue position %i (pool coord: %i=[%i, %i, %i]) at time %llu", 
          int(m_PoolSlotData[iInsertPos].m_iBrickID), 
          int(iInsertPos),
          int(iPoolCoordinate),
          int(m_PoolSlotData[iInsertPos].PositionInPool().x), 
          int(m_PoolSlotData[iInsertPos].PositionInPool().y), 
          int(m_PoolSlotData[iInsertPos].PositionInPool().z),
          iTimeOfCreation);
*/
  // update metadata (does NOT update the texture on the GPU)
  // this is done by the explicit UploadMetaData call to 
  // only upload the updated data once all bricks have been 
  // updated  
  m_vBrickMetadata[slot.m_iBrickID] = iPoolCoordinate + BI_FLAG_COUNT;

  // upload paged-in meta texel
  UploadMetadataTexel(slot.m_iBrickID);

  // upload brick to 3D texture
  m_pPoolDataTexture->SetData(slot.PositionInPool() * m_maxTotalBrickSize, vVoxelSize, pData);
}

void GLVolumePool::UploadFirstBrick(const UINTVECTOR3& m_vVoxelSize, void* pData) {
  uint32_t iLastBrickIndex = *(m_vLoDOffsetTable.end()-1);
  UploadBrick(iLastBrickIndex, m_vVoxelSize, pData, m_vPoolSlotData.size()-1, std::numeric_limits<uint64_t>::max());
}

bool GLVolumePool::UploadBrick(const BrickElemInfo& metaData, void* pData) {
  // in this frame we already replaced all bricks (except the single low-res brick)
  // in the pool so now we should render them first
  if (m_iInsertPos >= m_vPoolSlotData.size()-1)
    return false;

  int32_t iBrickID = GetIntegerBrickID(metaData.m_vBrickID);
  UploadBrick(iBrickID, metaData.m_vVoxelSize, pData, m_iInsertPos, m_iTimeOfCreation++);
  m_iInsertPos++;
  return true;
}

bool GLVolumePool::IsBrickResident(const UINTVECTOR4& vBrickID) const {

  int32_t iBrickID = GetIntegerBrickID(vBrickID);
  for (auto slot = m_vPoolSlotData.begin(); slot<m_vPoolSlotData.end();++slot) {
    if (int32_t(slot->m_iBrickID) == iBrickID) return true;
  }

  return false;
}

void GLVolumePool::Enable(float fLoDFactor, const FLOATVECTOR3& vExtend,
                          const FLOATVECTOR3& /*vAspect */,
                          GLSLProgram* pShaderProgram) const {
  m_pPoolMetadataTexture->Bind(m_iMetaTextureUnit);
  m_pPoolDataTexture->Bind(m_iDataTextureUnit);

  pShaderProgram->Enable();
  pShaderProgram->Set("fLoDFactor",fLoDFactor);
//  pShaderProgram->Set("volumeAspect",vAspect.x, vAspect.y, vAspect.z);


  float fLevelZeroWorldSpaceError = (vExtend/FLOATVECTOR3(m_volumeSize)).maxVal();
  pShaderProgram->Set("fLevelZeroWorldSpaceError",fLevelZeroWorldSpaceError);
}

void GLVolumePool::Disable() const {
  /*m_pPoolMetadataTexture->FinishRead();*/
}

GLVolumePool::~GLVolumePool() {
  if (m_pUpdater)
    delete m_pUpdater;

  FreeGLResources();
}

void GLVolumePool::CreateGLResources() {
  m_pPoolDataTexture = new GLTexture3D(m_poolSize.x, m_poolSize.y, m_poolSize.z,
                                      m_internalformat, m_format, m_type, 0, GL_LINEAR, GL_LINEAR);
  m_vPoolCapacity = UINTVECTOR3(m_pPoolDataTexture->GetSize().x/m_maxTotalBrickSize.x,
                                m_pPoolDataTexture->GetSize().y/m_maxTotalBrickSize.y,
                                m_pPoolDataTexture->GetSize().z/m_maxTotalBrickSize.z);

  MESSAGE("Creating brick pool of size [%u,%u,%u] to hold a "
          "max of [%u,%u,%u] bricks of size [%u,%u,%u] ("
          "addressable size [%u,%u,%u]) and smaller.",
           m_pPoolDataTexture->GetSize().x,
           m_pPoolDataTexture->GetSize().y,
           m_pPoolDataTexture->GetSize().z,
           m_vPoolCapacity.x,
           m_vPoolCapacity.y,
           m_vPoolCapacity.z,
           m_maxTotalBrickSize.x,
           m_maxTotalBrickSize.y,
           m_maxTotalBrickSize.z,
           m_maxInnerBrickSize.x,
           m_maxInnerBrickSize.y,
           m_maxInnerBrickSize.z);

  int gpumax; 
  GL(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gpumax));

  // last element in the offset table contains all bricks until the
  // last level + that last level itself contains one brick
  m_iTotalBrickCount = *(m_vLoDOffsetTable.end()-1)+1;

  UINTVECTOR2 vTexSize;
  try {
    vTexSize = VolumeTools::Fit1DIndexTo2DArray(m_iTotalBrickCount, gpumax);
  } catch (std::runtime_error const& e) {
    // this is very unlikely but not impossible
    T_ERROR(e.what());
    throw;
  }
  m_vBrickMetadata.resize(vTexSize.area());

  std::fill(m_vBrickMetadata.begin(), m_vBrickMetadata.end(), BI_MISSING);

  std::stringstream ss;
  ss << "Creating brick metadata texture of size " << vTexSize.x << " x " 
     << vTexSize.y << " to effectively hold  " << m_iTotalBrickCount << " entries. "
     << "Consequently, " << vTexSize.area() - m_iTotalBrickCount << " entries in "
     << "texture are wasted due to the 2D extension process.";
  MESSAGE(ss.str().c_str());

  m_pPoolMetadataTexture = new GLTexture2D(
    vTexSize.x, vTexSize.y, GL_R32UI,
    GL_RED_INTEGER, GL_UNSIGNED_INT, &m_vBrickMetadata[0], m_filter, m_filter
  );
}

struct {
  bool operator() (const PoolSlotData& i, const PoolSlotData& j) { 
    return (i.m_iTimeOfCreation < j.m_iTimeOfCreation);
  }
} PoolSorter;

void GLVolumePool::UploadMetadataTexture() {

  // DEBUG code
/*  MESSAGE("Brickpool Metadata entries:");
  for (size_t i = 0; i<m_iTotalBrickCount;++i) {
    switch (m_brickMetaData[i]) {
      case BI_MISSING     : break;
      case BI_EMPTY       : MESSAGE("  %i is empty",i); break;
      case BI_CHILD_EMPTY : MESSAGE("  %i is empty (including all children)",i); break;
      default             : MESSAGE("  %i loaded at position %i", i, int(m_brickMetaData[i]-BI_FLAG_COUNT)); break;
    }
  }

  uint32_t used=0;
  for (size_t i = 0; i<m_iTotalBrickCount;++i) {
    if (m_brickMetaData[i] > BI_MISSING)
      used++;
  }
  MESSAGE("Pool Utilization %u/%u %g%%", used, m_PoolSlotData.size(), 100.0f*used/float(m_PoolSlotData.size()));
  // DEBUG code end
*/
#ifdef GLVOLUMEPOOL_PROFILE
  double const t = m_Timer.Elapsed();
#endif

  m_pPoolMetadataTexture->SetData(&m_vBrickMetadata[0]);

#ifdef GLVOLUMEPOOL_PROFILE
  m_TimesMetaTextureUpload.Push(static_cast<float>(m_Timer.Elapsed() - t));
#endif
}

void GLVolumePool::UploadMetadataTexel(uint32_t iBrickID) {

  uint32_t const iMetaTextureWidth = m_pPoolMetadataTexture->GetSize().x;
  UINTVECTOR2 const vSize(1, 1); // size of single texel
  UINTVECTOR2 const vOffset(iBrickID % iMetaTextureWidth, iBrickID / iMetaTextureWidth);
  m_pPoolMetadataTexture->SetData(vOffset, vSize, &m_vBrickMetadata[iBrickID]);
}

void GLVolumePool::PrepareForPaging() {

  std::sort(m_vPoolSlotData.begin(), m_vPoolSlotData.end(), PoolSorter);
  m_iInsertPos = 0;
}

namespace {

  template<AbstrRenderer::ERenderMode eRenderMode>
  bool ContainsData(VisibilityState const& visibility, uint32_t iBrickID,
                    std::vector<GLVolumePool::MinMax> const& vMinMaxScalar,
                    std::vector<GLVolumePool::MinMax> const& vMinMaxGradient)
  {
    assert(eRenderMode == visibility.GetRenderMode());
    static_assert(eRenderMode == AbstrRenderer::RM_1DTRANS ||
                  eRenderMode == AbstrRenderer::RM_2DTRANS ||
                  eRenderMode == AbstrRenderer::RM_ISOSURFACE, "render mode not supported");
    switch (eRenderMode) {
    case AbstrRenderer::RM_1DTRANS:
      return (visibility.Get1DTransfer().fMax >= vMinMaxScalar[iBrickID].min &&
              visibility.Get1DTransfer().fMin <= vMinMaxScalar[iBrickID].max);
      break;
    case AbstrRenderer::RM_2DTRANS:
      return (visibility.Get2DTransfer().fMax >= vMinMaxScalar[iBrickID].min &&
              visibility.Get2DTransfer().fMin <= vMinMaxScalar[iBrickID].max)
              &&
             (visibility.Get2DTransfer().fMaxGradient >= vMinMaxGradient[iBrickID].min &&
              visibility.Get2DTransfer().fMinGradient <= vMinMaxGradient[iBrickID].max);
      break;
    case AbstrRenderer::RM_ISOSURFACE:
      return (visibility.GetIsoSurface().fIsoValue <= vMinMaxScalar[iBrickID].max);
      break;
    }
    return true;
  }

  template<AbstrRenderer::ERenderMode eRenderMode>
  void RecomputeVisibilityForBrickPool(VisibilityState const& visibility, GLVolumePool const& pool,
                                       std::vector<uint32_t>& vBrickMetadata, std::vector<PoolSlotData>& vBrickPool,
                                       std::vector<GLVolumePool::MinMax> const& vMinMaxScalar,
                                       std::vector<GLVolumePool::MinMax> const& vMinMaxGradient)
  {
    assert(eRenderMode == visibility.GetRenderMode());
    for (auto slot = vBrickPool.begin(); slot < vBrickPool.end(); slot++) {
      if (slot->WasEverUsed()) {
        bool const bContainsData = ContainsData<eRenderMode>(visibility, slot->m_iBrickID, vMinMaxScalar, vMinMaxGradient);
        bool const bContainedData = slot->ContainsVisibleBrick();

        if (bContainsData) {
          if (!bContainedData)
            slot->Restore();
          uint32_t const iPoolCoordinate = slot->PositionInPool().x +
                                           slot->PositionInPool().y * pool.GetPoolCapacity().x +
                                           slot->PositionInPool().z * pool.GetPoolCapacity().x * pool.GetPoolCapacity().y;
          vBrickMetadata[slot->m_iBrickID] = iPoolCoordinate + BI_FLAG_COUNT;
        } else {
          if (bContainedData)
            slot->FlagEmpty();
          vBrickMetadata[slot->m_iBrickID] = BI_EMPTY;
        }
      }
    } // for all slots in brick pool
  }

  template<bool bInterruptable, AbstrRenderer::ERenderMode eRenderMode>
  void RecomputeVisibilityForOctree(VisibilityState const& visibility, GLVolumePool const& pool,
                                    std::vector<uint32_t>& vBrickMetadata,
                                    std::vector<GLVolumePool::MinMax> const& vMinMaxScalar,
                                    std::vector<GLVolumePool::MinMax> const& vMinMaxGradient,
                                    tuvok::ThreadClass::PredicateFunction pContinue = tuvok::ThreadClass::PredicateFunction())
  {
#ifndef _DEBUG
    uint32_t const iContinue = 375; // we approximately process 7500 bricks/ms, checking for interruption every 375 bricks allows us to pause in 0.05 ms (worst case)
#else
    uint32_t const iContinue = 75; // we'll just get 1500 bricks/ms running a debug build
#endif
    uint32_t const iLoDCount = pool.GetLoDCount();
    UINTVECTOR3 iChildLayout = GetBrickLayout(pool.GetVolumeSize(), pool.GetMaxInnerBrickSize(), 0);

    // evaluate child visibility for finest level
    for (uint32_t z = 0; z < iChildLayout.z; z++) {
      for (uint32_t y = 0; y < iChildLayout.y; y++) {
        for (uint32_t x = 0; x < iChildLayout.x; x++) {

          if (bInterruptable && !(x % iContinue)/* && pContinue*/)
            if (!pContinue())
              return;

          UINTVECTOR4 const vBrickID(x, y, z, 0);
          uint32_t const brickIndex = pool.GetIntegerBrickID(vBrickID);
          if (vBrickMetadata[brickIndex] < BI_FLAG_COUNT) // only check bricks that are not cached in the pool
          {
            bool const bContainsData = ContainsData<eRenderMode>(visibility, brickIndex, vMinMaxScalar, vMinMaxGradient);
            if (!bContainsData)
              vBrickMetadata[brickIndex] = BI_CHILD_EMPTY; // finest level bricks are all child empty by definition
          }
        } // for x
      } // for y
    } // for z

    // walk up hierarchy (from finest to coarsest level) and propagate child empty visibility
    for (uint32_t iLoD = 1; iLoD < iLoDCount; iLoD++)
    {
      UINTVECTOR3 const iLayout = GetBrickLayout(pool.GetVolumeSize(), pool.GetMaxInnerBrickSize(), iLoD);

      // process even-sized volume
      UINTVECTOR3 const iEvenLayout = iChildLayout / 2;
      for (uint32_t z = 0; z < iEvenLayout.z; z++) {
        for (uint32_t y = 0; y < iEvenLayout.y; y++) {
          for (uint32_t x = 0; x < iEvenLayout.x; x++) {

            if (bInterruptable && !(x % iContinue)/* && pContinue*/)
              if (!pContinue())
                return;

            UINTVECTOR4 const vBrickID(x, y, z, iLoD);
            uint32_t const brickIndex = pool.GetIntegerBrickID(vBrickID);
            if (vBrickMetadata[brickIndex] < BI_FLAG_COUNT) // only check bricks that are not cached in the pool
            {
              bool const bContainsData = ContainsData<eRenderMode>(visibility, brickIndex, vMinMaxScalar, vMinMaxGradient);
              if (!bContainsData) {
                vBrickMetadata[brickIndex] = BI_CHILD_EMPTY; // flag parent brick to be child empty for now so that we can save a couple of tests below

                UINTVECTOR4 const childPosition(x*2, y*2, z*2, iLoD-1);
                if ((vBrickMetadata[pool.GetIntegerBrickID(childPosition)] != BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(0, 0, 1, 0))] != BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(0, 1, 0, 0))] != BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(0, 1, 1, 0))] != BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(1, 0, 0, 0))] != BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(1, 0, 1, 0))] != BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(1, 1, 0, 0))] != BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(1, 1, 1, 0))] != BI_CHILD_EMPTY))
                {
                  vBrickMetadata[brickIndex] = BI_EMPTY; // downgrade parent brick if we found a non child empty child
                }
              }
            }
          } // for x
        } // for y
      } // for z

      // process odd boundaries (if any)

      // plane at the end of the x-axis
      if (iChildLayout.x % 2) {
        for (uint32_t z = 0; z < iEvenLayout.z; z++) {
          for (uint32_t y = 0; y < iEvenLayout.y; y++) {

            if (bInterruptable && !(y % iContinue)/* && pContinue*/)
              if (!pContinue())
                return;

            uint32_t const x = iLayout.x - 1;
            UINTVECTOR4 const vBrickID(x, y, z, iLoD);
            uint32_t const brickIndex = pool.GetIntegerBrickID(vBrickID);
            if (vBrickMetadata[brickIndex] < BI_FLAG_COUNT) // only check bricks that are not cached in the pool
            {
              bool const bContainsData = ContainsData<eRenderMode>(visibility, brickIndex, vMinMaxScalar, vMinMaxGradient);
              if (!bContainsData) {
                vBrickMetadata[brickIndex] = BI_CHILD_EMPTY; // flag parent brick to be child empty for now so that we can save a couple of tests below

                UINTVECTOR4 const childPosition(x*2, y*2, z*2, iLoD-1);
                if ((vBrickMetadata[pool.GetIntegerBrickID(childPosition)] != BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(0, 0, 1, 0))] != BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(0, 1, 0, 0))] != BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(0, 1, 1, 0))] != BI_CHILD_EMPTY))
                {
                  vBrickMetadata[brickIndex] = BI_EMPTY; // downgrade parent brick if we found a non child empty child
                }
              }
            }
          } // for y
        } // for z
      } // if x is odd

      // plane at the end of the y-axis
      if (iChildLayout.y % 2) {
        for (uint32_t z = 0; z < iEvenLayout.z; z++) {
          for (uint32_t x = 0; x < iEvenLayout.x; x++) {

            if (bInterruptable && !(x % iContinue)/* && pContinue*/)
              if (!pContinue())
                return;

            uint32_t const y = iLayout.y - 1;
            UINTVECTOR4 const vBrickID(x, y, z, iLoD);
            uint32_t const brickIndex = pool.GetIntegerBrickID(vBrickID);
            if (vBrickMetadata[brickIndex] < BI_FLAG_COUNT) // only check bricks that are not cached in the pool
            {
              bool const bContainsData = ContainsData<eRenderMode>(visibility, brickIndex, vMinMaxScalar, vMinMaxGradient);
              if (!bContainsData) {
                vBrickMetadata[brickIndex] = BI_CHILD_EMPTY; // flag parent brick to be child empty for now so that we can save a couple of tests below

                UINTVECTOR4 const childPosition(x*2, y*2, z*2, iLoD-1);
                if ((vBrickMetadata[pool.GetIntegerBrickID(childPosition)] != BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(0, 0, 1, 0))] != BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(1, 0, 0, 0))] != BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(1, 0, 1, 0))] != BI_CHILD_EMPTY))
                {
                  vBrickMetadata[brickIndex] = BI_EMPTY; // downgrade parent brick if we found a non-empty child
                }
              }
            }
          } // for x
        } // for z
      } // if y is odd

      // plane at the end of the z-axis
      if (iChildLayout.z % 2) {
        for (uint32_t y = 0; y < iEvenLayout.y; y++) {
          for (uint32_t x = 0; x < iEvenLayout.x; x++) {

            if (bInterruptable && !(x % iContinue)/* && pContinue*/)
              if (!pContinue())
                return;

            uint32_t const z = iLayout.z - 1;
            UINTVECTOR4 const vBrickID(x, y, z, iLoD);
            uint32_t const brickIndex = pool.GetIntegerBrickID(vBrickID);
            if (vBrickMetadata[brickIndex] < BI_FLAG_COUNT) // only check bricks that are not cached in the pool
            {
              bool const bContainsData = ContainsData<eRenderMode>(visibility, brickIndex, vMinMaxScalar, vMinMaxGradient);
              if (!bContainsData) {
                vBrickMetadata[brickIndex] = BI_CHILD_EMPTY; // flag parent brick to be child empty for now so that we can save a couple of tests below

                UINTVECTOR4 const childPosition(x*2, y*2, z*2, iLoD-1);
                if ((vBrickMetadata[pool.GetIntegerBrickID(childPosition)] != BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(0, 1, 0, 0))] != BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(1, 0, 0, 0))] != BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(1, 1, 0, 0))] != BI_CHILD_EMPTY))
                {
                  vBrickMetadata[brickIndex] = BI_EMPTY; // downgrade parent brick if we found a non-empty child
                }
              }
            }
          } // for x
        } // for y
      } // if z is odd

      // line at the end of the x/y-axes
      if (iChildLayout.x % 2 && iChildLayout.y % 2) {
        for (uint32_t z = 0; z < iEvenLayout.z; z++) {

          if (bInterruptable && !(z % iContinue)/* && pContinue*/)
            if (!pContinue())
              return;

          uint32_t const y = iLayout.y - 1;
          uint32_t const x = iLayout.x - 1;
          UINTVECTOR4 const vBrickID(x, y, z, iLoD);
          uint32_t const brickIndex = pool.GetIntegerBrickID(vBrickID);
          if (vBrickMetadata[brickIndex] < BI_FLAG_COUNT) // only check bricks that are not cached in the pool
          {
            bool const bContainsData = ContainsData<eRenderMode>(visibility, brickIndex, vMinMaxScalar, vMinMaxGradient);
            if (!bContainsData) {
              vBrickMetadata[brickIndex] = BI_CHILD_EMPTY; // flag parent brick to be child empty for now so that we can save a couple of tests below
            
              UINTVECTOR4 const childPosition(x*2, y*2, z*2, iLoD-1);
              if ((vBrickMetadata[pool.GetIntegerBrickID(childPosition)] != BI_CHILD_EMPTY) ||
                  (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(0, 0, 1, 0))] != BI_CHILD_EMPTY))
              {
                vBrickMetadata[brickIndex] = BI_EMPTY; // downgrade parent brick if we found a non-empty child
              }
            }
          }
        } // for z
      } // if x and y are odd

      // line at the end of the x/z-axes
      if (iChildLayout.x % 2 && iChildLayout.z % 2) {
        for (uint32_t y = 0; y < iEvenLayout.y; y++) {

          if (bInterruptable && !(y % iContinue)/* && pContinue*/)
            if (!pContinue())
              return;

          uint32_t const z = iLayout.z - 1;
          uint32_t const x = iLayout.x - 1;
          UINTVECTOR4 const vBrickID(x, y, z, iLoD);
          uint32_t const brickIndex = pool.GetIntegerBrickID(vBrickID);
          if (vBrickMetadata[brickIndex] < BI_FLAG_COUNT) // only check bricks that are not cached in the pool
          {
            bool const bContainsData = ContainsData<eRenderMode>(visibility, brickIndex, vMinMaxScalar, vMinMaxGradient);
            if (!bContainsData) {
              vBrickMetadata[brickIndex] = BI_CHILD_EMPTY; // flag parent brick to be child empty for now so that we can save a couple of tests below

              UINTVECTOR4 const childPosition(x*2, y*2, z*2, iLoD-1);
              if ((vBrickMetadata[pool.GetIntegerBrickID(childPosition)] != BI_CHILD_EMPTY) ||
                  (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(0, 1, 0, 0))] != BI_CHILD_EMPTY))
              {
                vBrickMetadata[brickIndex] = BI_EMPTY; // downgrade parent brick if we found a non-empty child
              }
            }
          }
        } // for y
      } // if x and z are odd

      // line at the end of the y/z-axes
      if (iChildLayout.y % 2 && iChildLayout.z % 2) {
        for (uint32_t x = 0; x < iEvenLayout.x; x++) {

          if (bInterruptable && !(x % iContinue)/* && pContinue*/)
            if (!pContinue())
              return;

          uint32_t const z = iLayout.z - 1;
          uint32_t const y = iLayout.y - 1;
          UINTVECTOR4 const vBrickID(x, y, z, iLoD);
          uint32_t const brickIndex = pool.GetIntegerBrickID(vBrickID);
          if (vBrickMetadata[brickIndex] < BI_FLAG_COUNT) // only check bricks that are not cached in the pool
          {
            bool const bContainsData = ContainsData<eRenderMode>(visibility, brickIndex, vMinMaxScalar, vMinMaxGradient);
            if (!bContainsData) {
              vBrickMetadata[brickIndex] = BI_CHILD_EMPTY; // flag parent brick to be child empty for now so that we can save a couple of tests below

              UINTVECTOR4 const childPosition(x*2, y*2, z*2, iLoD-1);
              if ((vBrickMetadata[pool.GetIntegerBrickID(childPosition)] != BI_CHILD_EMPTY) ||
                  (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(1, 0, 0, 0))] != BI_CHILD_EMPTY))
              {
                vBrickMetadata[brickIndex] = BI_EMPTY; // downgrade parent brick if we found a non-empty child
              }
            }
          }
        } // for x
      } // if y and z are odd

      // single brick at the x/y/z corner
      if (iChildLayout.x % 2 && iChildLayout.y % 2 && iChildLayout.z % 2) {

        if (bInterruptable /* && pContinue*/)
          if (!pContinue())
            return;

        uint32_t const z = iLayout.z - 1;
        uint32_t const y = iLayout.y - 1;
        uint32_t const x = iLayout.x - 1;
        UINTVECTOR4 const vBrickID(x, y, z, iLoD);
        uint32_t const brickIndex = pool.GetIntegerBrickID(vBrickID);
        if (vBrickMetadata[brickIndex] < BI_FLAG_COUNT) // only check bricks that are not cached in the pool
        {
          bool const bContainsData = ContainsData<eRenderMode>(visibility, brickIndex, vMinMaxScalar, vMinMaxGradient);
          if (!bContainsData) {
            vBrickMetadata[brickIndex] = BI_CHILD_EMPTY; // flag parent brick to be child empty for now so that we can save a couple of tests below

            UINTVECTOR4 const childPosition(x*2, y*2, z*2, iLoD-1);
            if (vBrickMetadata[pool.GetIntegerBrickID(childPosition)] != BI_CHILD_EMPTY)
            {
              vBrickMetadata[brickIndex] = BI_EMPTY; // downgrade parent brick if we found a non-empty child
            }
          }
        }
      } // if x, y and z are odd

      iChildLayout = iLayout;
    } // for all levels
  }

  template<AbstrRenderer::ERenderMode eRenderMode>
  uint32_t PotentiallyUploadBricksToBrickPool(VisibilityState const& visibility, UVFDataset const* pDataset, size_t iTimestep, GLVolumePool& pool,
                                              std::vector<uint32_t>& vBrickMetadata, std::vector<UINTVECTOR4> const& vBrickIDs,
                                              std::vector<GLVolumePool::MinMax> const& vMinMaxScalar,
                                              std::vector<GLVolumePool::MinMax> const& vMinMaxGradient,                                          
                                              std::vector<unsigned char>& vUploadMem)
  {
    uint32_t iPagedBricks = 0;

    // now iterate over the missing bricks and upload them to the GPU
    // todo: consider batching this if it turns out to make a difference
    //       from submitting each brick separately
    Timer t;
    for (auto missingBrick = vBrickIDs.cbegin(); missingBrick < vBrickIDs.cend(); missingBrick++) {
      UINTVECTOR4 const& vBrickID = *missingBrick;
      BrickKey const key = pDataset->TOCVectorToKey(vBrickID, iTimestep);
      UINTVECTOR3 const vVoxelSize = pDataset->GetBrickVoxelCounts(key);

      uint32_t const brickIndex = pool.GetIntegerBrickID(vBrickID);
      // the brick could be flagged as empty by now if the async updater tested the brick after we ran the last render pass
      if (vBrickMetadata[brickIndex] == BI_MISSING) {
        // we might not have tested the brick for visibility yet since the updater's still running and we do not have a BI_UNKNOWN flag for now
        bool const bContainsData = ContainsData<eRenderMode>(visibility, brickIndex, vMinMaxScalar, vMinMaxGradient);
        if (bContainsData) {
          t.Start();
          pDataset->GetBrick(key, vUploadMem);
          pool.PH_SetBrickIOTime(pool.PH_BrickIOTime() + t.Elapsed());
          pool.PH_SetBrickIOBytes(pool.PH_BrickIOBytes() + vUploadMem.size());
          if (!pool.UploadBrick(BrickElemInfo(vBrickID, vVoxelSize), &vUploadMem[0]))
            return iPagedBricks;
          else
            iPagedBricks++;
        } else {
          vBrickMetadata[brickIndex] = BI_EMPTY;
          pool.UploadMetadataTexel(brickIndex);
        }
      } else if (vBrickMetadata[brickIndex] < BI_FLAG_COUNT) {
        // if the updater touched the brick in the meanwhile, we need to upload the meta texel
        pool.UploadMetadataTexel(brickIndex);
      } else {
        assert(false); // should never happen
      }
    }
    return iPagedBricks;
  }

} // anonymous namespace

void GLVolumePool::RecomputeVisibility(VisibilityState const& visibility, size_t iTimestep, bool bForceSynchronousUpdate)
{
#ifdef GLVOLUMEPOOL_PROFILE
  m_Timer.Start();
#endif

  // pause async updater because we will touch the meta data
  if (m_pUpdater)
    m_pUpdater->Pause();
  
  // fill minmax scalar acceleration data structure if timestep changed
  if (m_iMinMaxScalarTimestep != iTimestep) {
    m_iMinMaxScalarTimestep = iTimestep;
    for (uint32_t iBrickID = 0; iBrickID < m_vMinMaxScalar.size(); iBrickID++) {
      UINTVECTOR4 const vBrickID = GetVectorBrickID(iBrickID);
      BrickKey const key = m_pDataset->TOCVectorToKey(vBrickID, m_iMinMaxScalarTimestep);
      InternalMaxMinComponent imme = m_pDataset->MaxMinForKey(key);
      m_vMinMaxScalar[iBrickID].min = imme.minScalar;
      m_vMinMaxScalar[iBrickID].max = imme.maxScalar;
    }
  }

  // fill minmax gradient acceleration data structure if needed and timestep changed
  if (visibility.GetRenderMode() == AbstrRenderer::RM_2DTRANS) {
    if (m_iMinMaxGradientTimestep != iTimestep || m_vMinMaxGradient.empty()) {
      if (m_vMinMaxGradient.empty())
        m_vMinMaxGradient.resize(m_iTotalBrickCount);
      m_iMinMaxGradientTimestep = iTimestep;
      for (uint32_t iBrickID = 0; iBrickID < m_vMinMaxScalar.size(); iBrickID++) {
        UINTVECTOR4 const vBrickID = GetVectorBrickID(iBrickID);
        BrickKey const key = m_pDataset->TOCVectorToKey(vBrickID, m_iMinMaxGradientTimestep);
        InternalMaxMinComponent imme = m_pDataset->MaxMinForKey(key);
        m_vMinMaxGradient[iBrickID].min = imme.minGradient;
        m_vMinMaxGradient[iBrickID].max = imme.maxGradient;
      }
    }
  }

  // reset meta data for all bricks (BI_MISSING means that we haven't test the data for visibility until the async updater finishes)
  std::fill(m_vBrickMetadata.begin(), m_vBrickMetadata.end(), BI_MISSING);

  // TODO: if metadata texture grows too large (14 ms CPU update time for approx 2000x2000 texture) consider to
  //       update texel regions efficiently that will be toched by RecomputeVisibilityForBrickPool()
  //       updating every single texel turned out to be not efficient in this case
/*
  m_pPoolMetadataTexture->Write();
  GL(glClearColor(0, 0, 0, 0)); // clears metadata texture to BI_MISSING that equals zero
  GL(glClear(GL_COLOR_BUFFER_BIT));
  m_pPoolMetadataTexture->FinishWrite();
*/

#ifdef GLVOLUMEPOOL_PROFILE
  double const t = m_Timer.Elapsed();
#endif
  // recompute visibility for cached bricks immediately
  switch (visibility.GetRenderMode()) {
  case AbstrRenderer::RM_1DTRANS:
    RecomputeVisibilityForBrickPool<AbstrRenderer::RM_1DTRANS>(visibility, *this, m_vBrickMetadata, m_vPoolSlotData, m_vMinMaxScalar, m_vMinMaxGradient);
    break;
  case AbstrRenderer::RM_2DTRANS:
    RecomputeVisibilityForBrickPool<AbstrRenderer::RM_2DTRANS>(visibility, *this, m_vBrickMetadata, m_vPoolSlotData, m_vMinMaxScalar, m_vMinMaxGradient);
    break;
  case AbstrRenderer::RM_ISOSURFACE:
    RecomputeVisibilityForBrickPool<AbstrRenderer::RM_ISOSURFACE>(visibility, *this, m_vBrickMetadata, m_vPoolSlotData, m_vMinMaxScalar, m_vMinMaxGradient);
    break;
  default:
    T_ERROR("Unhandled rendering mode.");
    return;
  }
#ifdef GLVOLUMEPOOL_PROFILE
  m_TimesRecomputeVisibilityForBrickPool.Push(m_Timer.Elapsed() - t);
#endif

  if (!m_pUpdater || bForceSynchronousUpdate) {
    // recompute visibility for the entire hierarchy immediately
    switch (visibility.GetRenderMode()) {
    case AbstrRenderer::RM_1DTRANS:
      RecomputeVisibilityForOctree<false, AbstrRenderer::RM_1DTRANS>(visibility, *this, m_vBrickMetadata, m_vMinMaxScalar, m_vMinMaxGradient);
      break;
    case AbstrRenderer::RM_2DTRANS:
      RecomputeVisibilityForOctree<false, AbstrRenderer::RM_2DTRANS>(visibility, *this, m_vBrickMetadata, m_vMinMaxScalar, m_vMinMaxGradient);
      break;
    case AbstrRenderer::RM_ISOSURFACE:
      RecomputeVisibilityForOctree<false, AbstrRenderer::RM_ISOSURFACE>(visibility, *this, m_vBrickMetadata, m_vMinMaxScalar, m_vMinMaxGradient);
      break;
    default:
      T_ERROR("Unhandled rendering mode.");
      return;
    }
    m_bVisibilityUpdated = true; // will be true after we uploaded the metadata texture in the next line
  }

  // upload new metadata to GPU
  UploadMetadataTexture();

  // restart async updater because visibility changed
  if (m_pUpdater && !bForceSynchronousUpdate) {
    m_pUpdater->Restart(visibility);
    m_bVisibilityUpdated = false;
    OTHER("computed visibility for %d bricks in volume pool and started async visibility update for the entire hierarchy", m_vPoolSlotData.size());
  }
#ifdef GLVOLUMEPOOL_PROFILE
  m_TimesRecomputeVisibility.Push(static_cast<float>(m_Timer.Elapsed()));
  OTHER("recompute visibility for brick pool [avg: %.2f, min: %.2f, max: %.2f, samples: %d]"
    , m_TimesRecomputeVisibilityForBrickPool.GetAvg(), m_TimesRecomputeVisibilityForBrickPool.GetMin(), m_TimesRecomputeVisibilityForBrickPool.GetMax(), m_TimesRecomputeVisibilityForBrickPool.GetHistroryLength());
  OTHER("meta texture (%.4f MB) upload cost [avg: %.2f, min: %.2f, max: %.2f, samples: %d]"
    , m_pPoolMetadataTexture->GetCPUSize() / 1024.0f / 1024.0f, m_TimesMetaTextureUpload.GetAvg(), m_TimesMetaTextureUpload.GetMin(), m_TimesMetaTextureUpload.GetMax(), m_TimesMetaTextureUpload.GetHistroryLength());
  OTHER("recompute visibility cost [avg: %.2f, min: %.2f, max: %.2f, samples: %d]"
    , m_TimesRecomputeVisibility.GetAvg(), m_TimesRecomputeVisibility.GetMin(), m_TimesRecomputeVisibility.GetMax(), m_TimesRecomputeVisibility.GetHistroryLength());
#endif
}

uint32_t GLVolumePool::UploadBricks(const std::vector<UINTVECTOR4>& vBrickIDs,
                                    std::vector<unsigned char>& vUploadMem)
{
  // pause async updater because we will touch the meta data
  bool const bBusy = m_pUpdater && m_pUpdater->Pause();
  uint32_t iPagedBricks = 0;

  if (!vBrickIDs.empty())
  {
    PrepareForPaging();

    if (!m_bVisibilityUpdated) {
      VisibilityState const& visibility = m_pUpdater->GetVisibility();
      switch (visibility.GetRenderMode()) {
      case AbstrRenderer::RM_1DTRANS:
        iPagedBricks = PotentiallyUploadBricksToBrickPool<AbstrRenderer::RM_1DTRANS>(visibility, m_pDataset, m_iMinMaxScalarTimestep, *this, m_vBrickMetadata, vBrickIDs, m_vMinMaxScalar, m_vMinMaxGradient, vUploadMem);
        break;
      case AbstrRenderer::RM_2DTRANS:
        iPagedBricks = PotentiallyUploadBricksToBrickPool<AbstrRenderer::RM_2DTRANS>(visibility, m_pDataset, m_iMinMaxScalarTimestep, *this, m_vBrickMetadata, vBrickIDs, m_vMinMaxScalar, m_vMinMaxGradient, vUploadMem);
        break;
      case AbstrRenderer::RM_ISOSURFACE:
        iPagedBricks = PotentiallyUploadBricksToBrickPool<AbstrRenderer::RM_ISOSURFACE>(visibility, m_pDataset, m_iMinMaxScalarTimestep, *this, m_vBrickMetadata, vBrickIDs, m_vMinMaxScalar, m_vMinMaxGradient, vUploadMem);
        break;
      default:
        T_ERROR("Unhandled rendering mode.");
        return iPagedBricks;
      }
    } else {
      // visibility is updated guaranteeing that requested bricks do contain data
      Timer t;
      for (auto missingBrick = vBrickIDs.cbegin(); missingBrick < vBrickIDs.cend(); missingBrick++) {
        UINTVECTOR4 const& vBrickID = *missingBrick;
        BrickKey const key = m_pDataset->TOCVectorToKey(vBrickID, m_iMinMaxScalarTimestep);
        UINTVECTOR3 const vVoxelSize = m_pDataset->GetBrickVoxelCounts(key);

        t.Start();
        m_pDataset->GetBrick(key, vUploadMem);
        m_BrickIOTime += t.Elapsed();
        m_BrickIOBytes += vUploadMem.size();
        if (!UploadBrick(BrickElemInfo(vBrickID, vVoxelSize), &vUploadMem[0]))
          break;
        else
          iPagedBricks++;
      }
    }
  }
  
  if (bBusy && !m_bVisibilityUpdated) {
    m_pUpdater->Resume(); // resume async updater if we were busy
  } else {
    if (!m_bVisibilityUpdated)
    {
      UploadMetadataTexture(); // we want to upload the whole meta texture when async updater is done
      m_bVisibilityUpdated = true; // must be set one frame delayed otherwise we might upload empty bricks

#ifdef GLVOLUMEPOOL_PROFILE
      AsyncVisibilityUpdater::Stats const& stats = m_pUpdater->GetStats();
      OTHER("async visibility update completed for %d bricks in %.2f ms excluding %d interruptions that cost %.3f ms (%.2f bricks/ms)"
        , m_iTotalBrickCount, stats.fTimeTotal - stats.fTimeInterruptions, stats.iInterruptions, stats.fTimeInterruptions, m_iTotalBrickCount/(stats.fTimeTotal - stats.fTimeInterruptions));
#else
      OTHER("async visibility update completed for %d bricks", m_iTotalBrickCount);
#endif
    }
  }
  return iPagedBricks;
}

AsyncVisibilityUpdater::AsyncVisibilityUpdater(GLVolumePool& parent)
  : m_Pool(parent)
  , m_Visibility()
  , m_eState(Idle)
{
  StartThread();
}

AsyncVisibilityUpdater::~AsyncVisibilityUpdater()
{
  RequestThreadStop();
  Resume();
  JoinThread();
}

void AsyncVisibilityUpdater::Restart(VisibilityState const& visibility)
{
  SCOPEDLOCK(m_StateGuard);
  Pause();
  m_Visibility = visibility;
#ifndef GLVOLUMEPOOL_BUSY
  m_eState = RestartRequested;
  Resume(); // restart worker
#endif
}

bool AsyncVisibilityUpdater::Pause()
{
  SCOPEDLOCK(m_StateGuard);
  while (m_eState != Paused && m_eState != Idle) {
    m_eState = PauseRequested;
    Resume(); // wake up worker to update its state if necessary
    m_Parent.Wait(m_StateGuard); // wait until worker is paused
  }
#ifndef GLVOLUMEPOOL_BUSY
  return m_eState != Idle;
#else
  return true;
#endif
}

void AsyncVisibilityUpdater::Resume()
{
  m_Worker.WakeOne(); // resume worker
}

bool AsyncVisibilityUpdater::Continue()
{
  if (!m_bContinue) // check if thread should terminate
    return false;

#ifdef GLVOLUMEPOOL_PROFILE
  double t = m_Timer.Elapsed();
#endif

  SCOPEDLOCK(m_StateGuard);
  if (m_eState == PauseRequested) {
    m_eState = Paused;
    m_Parent.WakeOne(); // wake up parent because worker just paused
    m_Worker.Wait(m_StateGuard); // wait until parent wakes worker to continue

#ifdef GLVOLUMEPOOL_PROFILE
    m_Stats.iInterruptions++; // real interruption
#endif
  }
#ifdef GLVOLUMEPOOL_PROFILE
  m_Stats.fTimeInterruptions += m_Timer.Elapsed() - t; // time interruptions and interruption checks
#endif

  if (m_eState == RestartRequested) {
    return false;
  }
  m_eState = Busy;
  return true;
}

void AsyncVisibilityUpdater::ThreadMain(void*)
{
  PredicateFunction pContinue = std::bind(&AsyncVisibilityUpdater::Continue, this);

  while (m_bContinue) {
    {
      SCOPEDLOCK(m_StateGuard);
      while (m_eState != RestartRequested) {
        m_eState = Idle;
        m_Worker.Wait(m_StateGuard);
        if (!m_bContinue) // worker just was awaked, check if thread should be terminated
          return;
      }
      m_eState = Busy;
    }

#ifdef GLVOLUMEPOOL_PROFILE
    m_Stats.fTimeInterruptions = 0.0;
    m_Stats.fTimeTotal = 0.0;
    m_Stats.iInterruptions = 0;
    m_Timer.Start();
#endif

    switch (m_Visibility.GetRenderMode()) {
    case AbstrRenderer::RM_1DTRANS:
      RecomputeVisibilityForOctree<true, AbstrRenderer::RM_1DTRANS>(m_Visibility, m_Pool, m_Pool.m_vBrickMetadata, m_Pool.m_vMinMaxScalar, m_Pool.m_vMinMaxGradient, pContinue);
      break;
    case AbstrRenderer::RM_2DTRANS:
      RecomputeVisibilityForOctree<true, AbstrRenderer::RM_2DTRANS>(m_Visibility, m_Pool, m_Pool.m_vBrickMetadata, m_Pool.m_vMinMaxScalar, m_Pool.m_vMinMaxGradient, pContinue);
      break;
    case AbstrRenderer::RM_ISOSURFACE:
      RecomputeVisibilityForOctree<true, AbstrRenderer::RM_ISOSURFACE>(m_Visibility, m_Pool, m_Pool.m_vBrickMetadata, m_Pool.m_vMinMaxScalar, m_Pool.m_vMinMaxGradient, pContinue);
      break;
    default:
      assert(false); //T_ERROR("Unhandled rendering mode.");
      break;
    }

#ifdef GLVOLUMEPOOL_PROFILE
    m_Stats.fTimeTotal = m_Timer.Elapsed();
#endif
  }
}

void GLVolumePool::FreeGLResources() {
  if (m_pPoolMetadataTexture) {
    m_pPoolMetadataTexture->Delete();
    delete m_pPoolMetadataTexture;
  }
  if (m_pPoolDataTexture) {
    m_pPoolDataTexture->Delete();
    delete m_pPoolDataTexture;
  }
}

uint64_t GLVolumePool::GetCPUSize() const {
  return m_pPoolMetadataTexture->GetCPUSize() + m_pPoolDataTexture->GetCPUSize();
}

uint64_t GLVolumePool::GetGPUSize() const {
  return m_pPoolMetadataTexture->GetGPUSize() + m_pPoolDataTexture->GetGPUSize();
}

void GLVolumePool::SetFilterMode(GLenum filter) {
  m_filter = filter;
  m_pPoolDataTexture->SetFilter(filter, filter);
}

/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2012 Interactive Visualization and Data Analysis Group.


   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/
