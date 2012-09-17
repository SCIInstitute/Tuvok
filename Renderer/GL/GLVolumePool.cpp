#include <sstream>

#include "GLVolumePool.h"
#include "Basics/MathTools.h"
#include "Basics/TuvokException.h"
#include "Basics/Threads.h"
#include <stdexcept>
#include <algorithm>
#include <limits>
#include <iomanip>
#include "GLSLProgram.h"
#include "IO/uvfDataset.h"
#include "Renderer/VisibilityState.h"

enum BrickIDFlags {
  BI_CHILD_EMPTY = 0,
  BI_EMPTY,
  BI_MISSING,
  BI_FLAG_COUNT
};

using namespace tuvok;

namespace tuvok {

  class AsyncVisibilityUpdater : public ThreadClass {
  public:
    AsyncVisibilityUpdater(GLVolumePool& m_Parent);
    ~AsyncVisibilityUpdater();

    void Restart(const VisibilityState& visibility, size_t iTimeStep);
    bool Pause(); // returns true if thread is busy, false means thread is idle
    virtual void Resume();

    // call pause before using any getter and probably resume or restart afterwards
    const VisibilityState& GetVisibility() const { return m_Visibility; }
    size_t GetTimestep() const { return m_iTimestep; }
#ifdef PROFILE_GLVOLUMEPOOL
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
    size_t m_iTimestep;

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

#ifdef PROFILE_GLVOLUMEPOOL
    Timer m_Timer;
    Stats m_Stats;
#endif
  };

} // namespace tuvok

static uint32_t GetLoDCount(const UINTVECTOR3& volumeSize) {
  uint32_t maxP2 = MathTools::NextPow2( volumeSize.maxVal() );
  // add 1 here as we also have the 1x1x1 level (which is 2^0)
  return MathTools::Log2(maxP2)+1; 
}

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

GLVolumePool::GLVolumePool(const UINTVECTOR3& poolSize, UVFDataset* dataset, GLenum filter, bool bUseGLCore)
  : m_PoolMetadataTexture(NULL),
    m_PoolDataTexture(NULL),
    m_vPoolCapacity(0,0,0),
    m_poolSize(poolSize),
    m_maxInnerBrickSize(UINTVECTOR3(dataset->GetMaxUsedBrickSizes())-UINTVECTOR3(dataset->GetBrickOverlapSize())*2),
    m_maxTotalBrickSize(dataset->GetMaxUsedBrickSizes()),
    m_volumeSize(dataset->GetDomainSize()),
    m_filter(filter),
    m_iTimeOfCreation(2),
    m_iMetaTextureUnit(0),
    m_iDataTextureUnit(1),
    m_bUseGLCore(bUseGLCore),
    m_iInsertPos(0),
    m_pDataset(dataset),
    m_pUpdater(NULL),
    m_bVisibilityUpdated(false)
#ifdef PROFILE_GLVOLUMEPOOL
    , m_Timer()
    , m_TimesMetaTextureUpload(100)
    , m_TimesRecomputeVisibility(100)
#endif
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
    default : throw Exception("Invalid Component Count", _func_, __LINE__);
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
  uint32_t iLoDCount = GetLoDCount(m_volumeSize);
  m_vLoDOffsetTable.resize(iLoDCount);
  for (uint32_t i = 0;i<m_vLoDOffsetTable.size();++i) {
    m_vLoDOffsetTable[i] = iOffset;
    iOffset += GetBrickLayout(m_volumeSize, m_maxInnerBrickSize, i).volume();
  }

  CreateGLResources();

  m_pUpdater = new AsyncVisibilityUpdater(*this);
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

std::string GLVolumePool::GetShaderFragment(uint32_t iMetaTextureUnit, uint32_t iDataTextureUnit) {
  // must have created GL resources before asking for shader
  if (!m_PoolMetadataTexture  || !m_PoolDataTexture) return "";

  m_iMetaTextureUnit = iMetaTextureUnit;
  m_iDataTextureUnit = iDataTextureUnit;

  std::stringstream ss;

  if (m_bUseGLCore)
    ss << "#version 420 core" << std::endl;  
  else
    ss << "#version 420 compatibility" << std::endl;

  uint32_t iLodCount = GetLoDCount(m_volumeSize);
  FLOATVECTOR3 poolAspect(m_PoolDataTexture->GetSize());
  poolAspect /= poolAspect.minVal();

  ss << std::setprecision(36); // get the maximum precision for floats (larger precisions would just append zeros)
  ss << "" << std::endl
     << "layout(binding = " << m_iMetaTextureUnit << ") uniform usampler2D metaData;" << std::endl
     << "#define iMetaTextureWidth " << m_PoolMetadataTexture->GetSize().x << std::endl
     << "" << std::endl
     << "#define BI_CHILD_EMPTY " << BI_CHILD_EMPTY << std::endl
     << "#define BI_EMPTY "       << BI_EMPTY << std::endl
     << "#define BI_MISSING "     << BI_MISSING << std::endl
     << "#define BI_FLAG_COUNT "  << BI_FLAG_COUNT << std::endl
     << "" << std::endl
     << "layout(binding = " << m_iDataTextureUnit << ") uniform sampler3D volumePool;" << std::endl
     << "#define iPoolSize ivec3(" << m_PoolDataTexture->GetSize().x << ", " 
                                   << m_PoolDataTexture->GetSize().y << ", " 
                                   << m_PoolDataTexture->GetSize().z <<")" << std::endl
     << "#define volumeSize vec3(" << m_volumeSize.x << ", " 
                                         << m_volumeSize.y << ", " 
                                         << m_volumeSize.z <<")" << std::endl
     << "#define poolAspect vec3(" << poolAspect.x << ", " 
                                         << poolAspect.y << ", " 
                                         << poolAspect.z <<")" << std::endl
     << "#define poolCapacity ivec3(" << m_vPoolCapacity.x << ", " << 
                                               m_vPoolCapacity.y << ", " <<
                                               m_vPoolCapacity.z <<")" << std::endl
     << "// the total size of a brick in the pool, including the boundary" << std::endl
     << "#define maxTotalBrickSize ivec3(" << m_maxTotalBrickSize.x << ", " 
                                                 << m_maxTotalBrickSize.y << ", " 
                                                 << m_maxTotalBrickSize.z <<")" << std::endl
     << "// just the addressable (inner) size of a brick" << std::endl
     << "#define maxInnerBrickSize  ivec3(" << m_maxInnerBrickSize.x << ", " 
                                            << m_maxInnerBrickSize.y << ", " 
                                            << m_maxInnerBrickSize.z <<")" << std::endl
     << "// brick overlap voxels (in pool texcoords)" << std::endl
     << "#define overlap vec3(" << (m_maxTotalBrickSize.x-m_maxInnerBrickSize.x)/(2.0f*m_PoolDataTexture->GetSize().x) << ", " 
                                << (m_maxTotalBrickSize.y-m_maxInnerBrickSize.y)/(2.0f*m_PoolDataTexture->GetSize().y) << ", " 
                                << (m_maxTotalBrickSize.z-m_maxInnerBrickSize.z)/(2.0f*m_PoolDataTexture->GetSize().z) <<")" << std::endl
     << "uniform float fLoDFactor;" << std::endl
     << "uniform float fLevelZeroWorldSpaceError;" << std::endl
     << "uniform vec3 volumeAspect;" << std::endl
     << "#define iMaxLOD " << iLodCount-1 << std::endl
     << "uniform uint vLODOffset[" << iLodCount << "] = uint[](";
  for (uint32_t i = 0;i<iLodCount;++i) {
    ss << "uint(" << m_vLoDOffsetTable[i] << ")";
    if (i<iLodCount-1) {
      ss << ", ";
    }
  }
  ss << ");" << std::endl
     << "uniform vec3 vLODLayout[" << iLodCount << "] = vec3[](" << std::endl;
  for (uint32_t i = 0;i<m_vLoDOffsetTable.size();++i) {
    FLOATVECTOR3 vLoDSize = GetFloatBrickLayout(m_volumeSize, m_maxInnerBrickSize, i);
    ss << "   vec3(" << vLoDSize.x << ", " << vLoDSize.y << ", " << vLoDSize.z << ")";
    if (i<iLodCount-1) {
      ss << ",";
    }
    ss << "// Level " << i << std::endl;
  }
  ss << ");" << std::endl
     << "uniform uvec2 iLODLayoutSize[" << iLodCount << "] = uvec2[](" << std::endl;
  for (uint32_t i = 0;i<m_vLoDOffsetTable.size();++i) {
    FLOATVECTOR3 vLoDSize = GetFloatBrickLayout(m_volumeSize, m_maxInnerBrickSize, i);    
    ss << "   uvec2(" << unsigned(ceil(vLoDSize.x)) << ", " << unsigned(ceil(vLoDSize.x)) * unsigned(ceil(vLoDSize.y)) << ")";
    if (i<iLodCount-1) {
      ss << ",";
    }
    ss << "// Level " << i << std::endl;
  }

  ss << ");" << std::endl
     << "" << std::endl
     << "uint Hash(uvec4 brick);" << std::endl
     << "" << std::endl
     << "uint ReportMissingBrick(uvec4 brick) {" << std::endl
     << "  return Hash(brick);" << std::endl
     << "}" << std::endl
     << "" << std::endl
     << "ivec2 GetBrickIndex(uvec4 brickCoords) {" << std::endl
     << "  uint iLODOffset  = vLODOffset[brickCoords.w];"<< std::endl
     << "  uint iBrickIndex = iLODOffset + brickCoords.x + brickCoords.y * iLODLayoutSize[brickCoords.w].x + brickCoords.z * iLODLayoutSize[brickCoords.w].y;" << std::endl
     << "  return ivec2(iBrickIndex%iMetaTextureWidth, iBrickIndex/iMetaTextureWidth);"<< std::endl
     << "}" << std::endl
     << "" << std::endl
     << "uint GetBrickInfo(uvec4 brickCoords) {" << std::endl
     << "  return texelFetch(metaData, GetBrickIndex(brickCoords), 0).r;" << std::endl
     << "}" << std::endl
     << "" << std::endl
     << "uvec4 ComputeBrickCoords(vec3 normEntryCoords, uint iLOD) {" << std::endl
     << "  return uvec4(normEntryCoords*vLODLayout[iLOD], iLOD);" << std::endl
     << "}" << std::endl
     << "" << std::endl
     << "void GetBrickCorners(uvec4 brickCoords, out vec3 corners[2], out vec3 fullBrickSize) {" << std::endl
     << "  corners[0]    = vec3(brickCoords.xyz)   / vLODLayout[brickCoords.w];" << std::endl
     << "  fullBrickSize = vec3(brickCoords.xyz+1) / vLODLayout[brickCoords.w];" << std::endl
     << "  corners[1]    = min(vec3(1.0), fullBrickSize);" << std::endl
     << "}" << std::endl
     << "" << std::endl   
     << "vec3 BrickExit(vec3 pointInBrick, vec3 dir, in vec3 corners[2]) {" << std::endl
     << "  vec3 div = 1.0 / dir;" << std::endl
     << "  ivec3 side = ivec3(step(0.0,div));" << std::endl
     << "  vec3 tIntersect;" << std::endl
     << "  tIntersect.x = (corners[side.x].x - pointInBrick.x) * div.x;" << std::endl
     << "  tIntersect.y = (corners[side.y].y - pointInBrick.y) * div.y;" << std::endl
     << "  tIntersect.z = (corners[side.z].z - pointInBrick.z) * div.z;" << std::endl
     << "  return pointInBrick + min(min(tIntersect.x, tIntersect.y), tIntersect.z) * dir;" << std::endl
     << "}" << std::endl
     << " " << std::endl
     << "uvec3 InfoToCoords(in uint brickInfo) {" << std::endl
     << "  uint index = brickInfo-BI_FLAG_COUNT;" << std::endl
     << "  uvec3 vBrickCoords;" << std::endl
     << "  vBrickCoords.x = index % poolCapacity.x;" << std::endl
     << "  vBrickCoords.y = (index / poolCapacity.x) % poolCapacity.y;" << std::endl
     << "  vBrickCoords.z = index / (poolCapacity.x*poolCapacity.y);" << std::endl
     << "  return vBrickCoords;" << std::endl
     << "}" << std::endl
     << " " << std::endl
     << "void BrickPoolCoords(in uint brickInfo,  out vec3 corners[2]) {" << std::endl
     << "  uvec3 poolVoxelPos = InfoToCoords(brickInfo) * maxTotalBrickSize;" << std::endl
     << "  corners[0] = (vec3(poolVoxelPos)                   / vec3(iPoolSize))+ overlap;" << std::endl
     << "  corners[1] = (vec3(poolVoxelPos+maxTotalBrickSize) / vec3(iPoolSize))- overlap;" << std::endl
     << "}" << std::endl
     << " " << std::endl
     << "void NormCoordsToPoolCoords(in vec3 normEntryCoords, in vec3 normExitCoords, in vec3 corners[2]," << std::endl
     << "                            in uint brickInfo, out vec3 poolEntryCoords, out vec3 poolExitCoords," << std::endl
     << "                            out vec3 normToPoolScale, out vec3 normToPoolTrans) {" << std::endl
     << "  vec3 poolCorners[2];" << std::endl
     << "  BrickPoolCoords(brickInfo, poolCorners);" << std::endl
     << "  normToPoolScale = (poolCorners[1]-poolCorners[0])/(corners[1]-corners[0]);" << std::endl
     << "  normToPoolTrans = poolCorners[0]-corners[0]*normToPoolScale;" << std::endl
     << "  poolEntryCoords  = (normEntryCoords * normToPoolScale + normToPoolTrans);" << std::endl
     << "  poolExitCoords   = (normExitCoords  * normToPoolScale + normToPoolTrans);" << std::endl
     << "}" << std::endl
     << "" << std::endl
     << "bool GetBrick(in vec3 normEntryCoords, inout uint iLOD, in vec3 direction," << std::endl
     << "              out vec3 poolEntryCoords, out vec3 poolExitCoords," << std::endl
     << "              out vec3 normExitCoords, out bool bEmpty," << std::endl
     << "              out vec3 normToPoolScale, out vec3 normToPoolTrans, out uvec4 brickCoords) {" << std::endl
     << "  normEntryCoords = clamp(normEntryCoords, 0.0, 1.0);" << std::endl
     << "  bool bFoundRequestedResolution = true;" << std::endl
     << "  brickCoords = ComputeBrickCoords(normEntryCoords, iLOD);" << std::endl
     << "  uint  brickInfo   = GetBrickInfo(brickCoords);" << std::endl
     << "  if (brickInfo == BI_MISSING) {" << std::endl
     << "    uint iStartLOD = iLOD;" << std::endl
     << "    ReportMissingBrick(brickCoords);" << std::endl
     << "    // when the requested resolution is not present look for lower res" << std::endl
     << "    bFoundRequestedResolution = false;" << std::endl
     << "    do {" << std::endl
     << "      iLOD++;" << std::endl
     << "      brickCoords = ComputeBrickCoords(normEntryCoords, iLOD);" << std::endl
     << "      brickInfo   = GetBrickInfo(brickCoords);" << std::endl
     << "      if (brickInfo == BI_MISSING && iStartLOD+2 == iLOD) ReportMissingBrick(brickCoords);" << std::endl
     << "    } while (brickInfo == BI_MISSING);" << std::endl
     << "  }" << std::endl
     << "  bEmpty = (brickInfo <= BI_EMPTY);" << std::endl
     << "  if (bEmpty) {" << std::endl
     << "    // when we find an empty brick check if the lower resolutions are also empty" << std::endl
     << "    for (uint ilowResLOD = iLOD+1; ilowResLOD<iMaxLOD;++ilowResLOD) {" << std::endl
     << "      uvec4 lowResBrickCoords = ComputeBrickCoords(normEntryCoords, ilowResLOD);" << std::endl
     << "      uint lowResBrickInfo = GetBrickInfo(lowResBrickCoords);" << std::endl
     << "      if (lowResBrickInfo == BI_CHILD_EMPTY) {" << std::endl
     << "        brickCoords = lowResBrickCoords;" << std::endl
     << "        brickInfo = lowResBrickInfo;" << std::endl
     << "        iLOD = ilowResLOD;" << std::endl
     << "      } else {" << std::endl
     << "        break;" << std::endl
     << "      }" << std::endl
     << "    }" << std::endl
     << "  }" << std::endl
     << "  vec3 corners[2];" << std::endl
     << "  vec3 fullBrickCorner;" << std::endl
     << "  GetBrickCorners(brickCoords, corners, fullBrickCorner);" << std::endl
     << "  normExitCoords = BrickExit(normEntryCoords, direction, corners);" << std::endl
     << "  if (bEmpty) " << std::endl
     << "    return bFoundRequestedResolution;" << std::endl
     << "  corners[1] = fullBrickCorner;" << std::endl
     << "  NormCoordsToPoolCoords(normEntryCoords, normExitCoords, corners," << std::endl
     << "                         brickInfo, poolEntryCoords, poolExitCoords," << std::endl
     << "                         normToPoolScale, normToPoolTrans);" << std::endl
     << "  return bFoundRequestedResolution;" << std::endl
     << "}" << std::endl
     << "" << std::endl
     << "vec3 GetSampleDelta() {" << std::endl
     << "  return 1.0/vec3(iPoolSize);" << std::endl
     << "}" << std::endl
     << "" << std::endl
     << "vec3 TransformToPoolSpace(in vec3 direction, in float sampleRateModifier) {" << std::endl
     << "  // normalize the direction" << std::endl
     << "  direction *= volumeSize;" << std::endl
     << "  direction = normalize(direction);" << std::endl
     << "  // scale to volume pool's norm coordinates" << std::endl
     << "  direction /= vec3(iPoolSize);" << std::endl
     << "  // do (roughly) two samples per voxel and apply user defined sample density" << std::endl
     << "  return direction / (2.0*sampleRateModifier);" << std::endl
     << "}" << std::endl
     << " " << std::endl
     << "float samplePool(vec3 coords) {" << std::endl
     << " return texture(volumePool, coords).r;" << std::endl
     << "}" << std::endl
     << " " << std::endl
     << "vec3 samplePool3(vec3 coords) {" << std::endl
     << " return texture(volumePool, coords).rgb;" << std::endl
     << "}" << std::endl
     << " " << std::endl
     << "vec4 samplePool4(vec3 coords) {" << std::endl
     << " return texture(volumePool, coords);" << std::endl
     << "}" << std::endl
     << " " << std::endl
     << "uint ComputeLOD(float dist) {" << std::endl
     << "  // opengl -> negative z-axis hence the minus" << std::endl
     << "  return min(iMaxLOD, uint(log2(fLoDFactor*(-dist)/fLevelZeroWorldSpaceError)));" << std::endl
     << "}" << std::endl;

/*
  // DEBUG code
  std::stringstream debug(ss.str());
  std::string line;
  unsigned int iLine = 1;
  while(std::getline(debug, line)) {
    MESSAGE("%i %s", iLine++, line.c_str());
  }
  // DEBUG code end
*/

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
  m_PoolDataTexture->SetData(slot.PositionInPool() * m_maxTotalBrickSize, vVoxelSize, pData);
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
  m_PoolMetadataTexture->Bind(m_iMetaTextureUnit);
  m_PoolDataTexture->Bind(m_iDataTextureUnit);

  pShaderProgram->Enable();
  pShaderProgram->Set("fLoDFactor",fLoDFactor);
//  pShaderProgram->Set("volumeAspect",vAspect.x, vAspect.y, vAspect.z);


  float fLevelZeroWorldSpaceError = (vExtend/FLOATVECTOR3(m_volumeSize)).maxVal();
  pShaderProgram->Set("fLevelZeroWorldSpaceError",fLevelZeroWorldSpaceError);
}

GLVolumePool::~GLVolumePool() {
  delete m_pUpdater;

  FreeGLResources();
}

void GLVolumePool::CreateGLResources() {
  m_PoolDataTexture = new GLTexture3D(m_poolSize.x, m_poolSize.y, m_poolSize.z,
                                      m_internalformat, m_format, m_type, 0, GL_LINEAR, GL_LINEAR);
  m_vPoolCapacity = UINTVECTOR3(m_PoolDataTexture->GetSize().x/m_maxTotalBrickSize.x,
                                m_PoolDataTexture->GetSize().y/m_maxTotalBrickSize.y,
                                m_PoolDataTexture->GetSize().z/m_maxTotalBrickSize.z);

  MESSAGE("Creating brick pool of size [%u,%u,%u] to hold a "
        "max of [%u,%u,%u] bricks of size [%u,%u,%u] ("
        "adressable size [%u,%u,%u]) and smaler.",
         m_PoolDataTexture->GetSize().x,
         m_PoolDataTexture->GetSize().y,
         m_PoolDataTexture->GetSize().z,
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

  // this is very unlikely but not impossible
  if (m_iTotalBrickCount > uint32_t(gpumax*gpumax)) {
    std::stringstream ss;    
    
    ss << "Unable to create brick metadata texture, as it needs to hold "
       << m_iTotalBrickCount << "entries but the max 2D texture size on this "
       << "machine is only " << gpumax << " x " << gpumax << "allowing for "
       << " a maximum of " << gpumax*gpumax << " indices.";

    T_ERROR(ss.str().c_str());
    throw std::runtime_error(ss.str().c_str()); 
  }
  
  UINTVECTOR2 vTexSize;
  vTexSize.x = uint32_t(ceil(sqrt(double(m_iTotalBrickCount))));
  vTexSize.y = uint32_t(ceil(double(m_iTotalBrickCount)/double(vTexSize.x)));
  m_vBrickMetadata.resize(vTexSize.area());

  std::fill(m_vBrickMetadata.begin(), m_vBrickMetadata.end(), BI_MISSING);

  std::stringstream ss;
  ss << "Creating brick metadata texture of size " << vTexSize.x << " x " 
     << vTexSize.y << " to effectively hold  " << m_iTotalBrickCount << " entries. "
     << "Consequently, " << vTexSize.area() - m_iTotalBrickCount << " entries in "
     << "texture are wasted due to the 2D extension process.";
  MESSAGE(ss.str().c_str());

  m_PoolMetadataTexture = new GLTexture2D(
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
/*
#ifdef PROFILE_GLVOLUMEPOOL
  double const t = m_Timer.Elapsed();
#endif
*/

  m_PoolMetadataTexture->SetData(&m_vBrickMetadata[0]);

/*
#ifdef PROFILE_GLVOLUMEPOOL
  m_TimesMetaTextureUpload.Push(static_cast<float>(m_Timer.Elapsed() - t));
#endif
*/
}

void GLVolumePool::UploadMetadataTexel(uint32_t iBrickID) {

  uint32_t const iMetaTextureWidth = m_PoolMetadataTexture->GetSize().x;
  UINTVECTOR2 const vSize(1, 1); // size of single texel
  UINTVECTOR2 const vOffset(iBrickID % iMetaTextureWidth, iBrickID / iMetaTextureWidth);
  m_PoolMetadataTexture->SetData(vOffset, vSize, &m_vBrickMetadata[iBrickID]);
}

void GLVolumePool::PrepareForPaging() {

  std::sort(m_vPoolSlotData.begin(), m_vPoolSlotData.end(), PoolSorter);
  m_iInsertPos = 0;
}

namespace {

  template<AbstrRenderer::ERenderMode eRenderMode>
  bool ContainsData(VisibilityState const& visibility, UVFDataset const* pDataset, BrickKey const& key)
  {
    assert(eRenderMode == visibility.GetRenderMode());
    static_assert(eRenderMode == AbstrRenderer::RM_1DTRANS ||
                  eRenderMode == AbstrRenderer::RM_2DTRANS ||
                  eRenderMode == AbstrRenderer::RM_ISOSURFACE, "render mode not supported");
    switch (eRenderMode) {
    case AbstrRenderer::RM_1DTRANS:
      return pDataset->ContainsData(key, visibility.Get1DTransfer().fMin,
                                         visibility.Get1DTransfer().fMax);
      break;
    case AbstrRenderer::RM_2DTRANS:
      return pDataset->ContainsData(key, visibility.Get2DTransfer().fMin,
                                         visibility.Get2DTransfer().fMax,
                                         visibility.Get2DTransfer().fMinGradient,
                                         visibility.Get2DTransfer().fMaxGradient);
      break;
    case AbstrRenderer::RM_ISOSURFACE:
      return pDataset->ContainsData(key, visibility.GetIsoSurface().fIsoValue);
      break;
    }
    return true;
  }

  template<AbstrRenderer::ERenderMode eRenderMode>
  void RecomputeVisibilityForBrickPool(VisibilityState const& visibility, UVFDataset const* pDataset, size_t iTimestep, GLVolumePool const& pool,
                                       std::vector<uint32_t>& vBrickMetadata, std::vector<PoolSlotData>& vBrickPool)
  {
    assert(eRenderMode == visibility.GetRenderMode());
    for (auto slot = vBrickPool.begin(); slot < vBrickPool.end(); slot++) {
      if (slot->WasEverUsed()) {
        UINTVECTOR4 const vBrickID = pool.GetVectorBrickID(slot->m_iBrickID);
        BrickKey const key = pDataset->TOCVectorToKey(vBrickID, iTimestep);
        bool const bContainsData = ContainsData<eRenderMode>(visibility, pDataset, key);
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

  template<bool bEvaluatePredicate, AbstrRenderer::ERenderMode eRenderMode>
  void RecomputeVisibilityForOctree(VisibilityState const& visibility, UVFDataset const* pDataset, size_t iTimestep, GLVolumePool const& pool,
                                    std::vector<uint32_t>& vBrickMetadata, tuvok::ThreadClass::PredicateFunction pContinue = tuvok::ThreadClass::PredicateFunction()) {

    uint32_t const iLodCount = GetLoDCount(pool.GetVolumeSize());
    UINTVECTOR3 iChildLayout = GetBrickLayout(pool.GetVolumeSize(), pool.GetMaxInnerBrickSize(), 0);

    // evaluate child visibility for finest level
    for (uint32_t z = 0; z < iChildLayout.z; z++) {
      for (uint32_t y = 0; y < iChildLayout.y; y++) {
        for (uint32_t x = 0; x < iChildLayout.x; x++) {

          if (bEvaluatePredicate && pContinue)
            if (!pContinue())
              return;

          UINTVECTOR4 const vBrickID(x, y, z, 0);
          uint32_t const brickIndex = pool.GetIntegerBrickID(vBrickID);
          if (vBrickMetadata[brickIndex] <= BI_MISSING) // only check bricks that are not cached in the pool
          {
            BrickKey const key = pDataset->TOCVectorToKey(vBrickID, iTimestep);
            bool const bContainsData = ContainsData<eRenderMode>(visibility, pDataset, key);
            if (!bContainsData)
              vBrickMetadata[brickIndex] = BI_CHILD_EMPTY; // finest level bricks are all child empty by definition
          }
        } // for x
      } // for y
    } // for z

    // walk up hierarchy (from finest to coarsest level) and propagate child empty visibility
    for (uint32_t iLoD = 1; iLoD < iLodCount; iLoD++)
    {
      UINTVECTOR3 const iLayout = GetBrickLayout(pool.GetVolumeSize(), pool.GetMaxInnerBrickSize(), iLoD);

      // process even-sized volume
      UINTVECTOR3 const iEvenLayout = iChildLayout / 2;
      for (uint32_t z = 0; z < iEvenLayout.z; z++) {
        for (uint32_t y = 0; y < iEvenLayout.y; y++) {
          for (uint32_t x = 0; x < iEvenLayout.x; x++) {

            if (bEvaluatePredicate && pContinue)
              if (!pContinue())
                return;

            UINTVECTOR4 const vBrickID(x, y, z, iLoD);
            uint32_t const brickIndex = pool.GetIntegerBrickID(vBrickID);
            if (vBrickMetadata[brickIndex] <= BI_MISSING) // only check bricks that are not cached in the pool
            {
              BrickKey const key = pDataset->TOCVectorToKey(vBrickID, iTimestep);
              bool const bContainsData = ContainsData<eRenderMode>(visibility, pDataset, key);
              if (!bContainsData) {
                vBrickMetadata[brickIndex] = BI_CHILD_EMPTY; // flag parent brick to be child empty for now so that we can save a couple of tests below

                UINTVECTOR4 const childPosition(x*2, y*2, z*2, iLoD-1);
                if ((vBrickMetadata[pool.GetIntegerBrickID(childPosition)] > BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(0, 0, 1, 0))] > BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(0, 1, 0, 0))] > BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(0, 1, 1, 0))] > BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(1, 0, 0, 0))] > BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(1, 0, 1, 0))] > BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(1, 1, 0, 0))] > BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(1, 1, 1, 0))] > BI_CHILD_EMPTY))
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

            if (bEvaluatePredicate && pContinue)
              if (!pContinue())
                return;

            uint32_t const x = iLayout.x - 1;
            UINTVECTOR4 const vBrickID(x, y, z, iLoD);
            uint32_t const brickIndex = pool.GetIntegerBrickID(vBrickID);
            if (vBrickMetadata[brickIndex] <= BI_MISSING) // only check bricks that are not cached in the pool
            {
              BrickKey const key = pDataset->TOCVectorToKey(vBrickID, iTimestep);
              bool const bContainsData = ContainsData<eRenderMode>(visibility, pDataset, key);
              if (!bContainsData) {
                vBrickMetadata[brickIndex] = BI_CHILD_EMPTY; // flag parent brick to be child empty for now so that we can save a couple of tests below

                UINTVECTOR4 const childPosition(x*2, y*2, z*2, iLoD-1);
                if ((vBrickMetadata[pool.GetIntegerBrickID(childPosition)] > BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(0, 0, 1, 0))] > BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(0, 1, 0, 0))] > BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(0, 1, 1, 0))] > BI_CHILD_EMPTY))
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

            if (bEvaluatePredicate && pContinue)
              if (!pContinue())
                return;

            uint32_t const y = iLayout.y - 1;
            UINTVECTOR4 const vBrickID(x, y, z, iLoD);
            uint32_t const brickIndex = pool.GetIntegerBrickID(vBrickID);
            if (vBrickMetadata[brickIndex] <= BI_MISSING) // only check bricks that are not cached in the pool
            {
              BrickKey const key = pDataset->TOCVectorToKey(vBrickID, iTimestep);
              bool const bContainsData = ContainsData<eRenderMode>(visibility, pDataset, key);
              if (!bContainsData) {
                vBrickMetadata[brickIndex] = BI_CHILD_EMPTY; // flag parent brick to be child empty for now so that we can save a couple of tests below

                UINTVECTOR4 const childPosition(x*2, y*2, z*2, iLoD-1);
                if ((vBrickMetadata[pool.GetIntegerBrickID(childPosition)] > BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(0, 0, 1, 0))] > BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(1, 0, 0, 0))] > BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(1, 0, 1, 0))] > BI_CHILD_EMPTY))
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

            if (bEvaluatePredicate && pContinue)
              if (!pContinue())
                return;

            uint32_t const z = iLayout.z - 1;
            UINTVECTOR4 const vBrickID(x, y, z, iLoD);
            uint32_t const brickIndex = pool.GetIntegerBrickID(vBrickID);
            if (vBrickMetadata[brickIndex] <= BI_MISSING) // only check bricks that are not cached in the pool
            {
              BrickKey const key = pDataset->TOCVectorToKey(vBrickID, iTimestep);
              bool const bContainsData = ContainsData<eRenderMode>(visibility, pDataset, key);
              if (!bContainsData) {
                vBrickMetadata[brickIndex] = BI_CHILD_EMPTY; // flag parent brick to be child empty for now so that we can save a couple of tests below

                UINTVECTOR4 const childPosition(x*2, y*2, z*2, iLoD-1);
                if ((vBrickMetadata[pool.GetIntegerBrickID(childPosition)] > BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(0, 1, 0, 0))] > BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(1, 0, 0, 0))] > BI_CHILD_EMPTY) ||
                    (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(1, 1, 0, 0))] > BI_CHILD_EMPTY))
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

          if (bEvaluatePredicate && pContinue)
            if (!pContinue())
              return;

          uint32_t const y = iLayout.y - 1;
          uint32_t const x = iLayout.x - 1;
          UINTVECTOR4 const vBrickID(x, y, z, iLoD);
          uint32_t const brickIndex = pool.GetIntegerBrickID(vBrickID);
          if (vBrickMetadata[brickIndex] <= BI_MISSING) // only check bricks that are not cached in the pool
          {
            BrickKey const key = pDataset->TOCVectorToKey(vBrickID, iTimestep);
            bool const bContainsData = ContainsData<eRenderMode>(visibility, pDataset, key);
            if (!bContainsData) {
              vBrickMetadata[brickIndex] = BI_CHILD_EMPTY; // flag parent brick to be child empty for now so that we can save a couple of tests below
            
              UINTVECTOR4 const childPosition(x*2, y*2, z*2, iLoD-1);
              if ((vBrickMetadata[pool.GetIntegerBrickID(childPosition)] > BI_CHILD_EMPTY) ||
                  (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(0, 0, 1, 0))] > BI_CHILD_EMPTY))
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

          if (bEvaluatePredicate && pContinue)
            if (!pContinue())
              return;

          uint32_t const z = iLayout.z - 1;
          uint32_t const x = iLayout.x - 1;
          UINTVECTOR4 const vBrickID(x, y, z, iLoD);
          uint32_t const brickIndex = pool.GetIntegerBrickID(vBrickID);
          if (vBrickMetadata[brickIndex] <= BI_MISSING) // only check bricks that are not cached in the pool
          {
            BrickKey const key = pDataset->TOCVectorToKey(vBrickID, iTimestep);
            bool const bContainsData = ContainsData<eRenderMode>(visibility, pDataset, key);
            if (!bContainsData) {
              vBrickMetadata[brickIndex] = BI_CHILD_EMPTY; // flag parent brick to be child empty for now so that we can save a couple of tests below

              UINTVECTOR4 const childPosition(x*2, y*2, z*2, iLoD-1);
              if ((vBrickMetadata[pool.GetIntegerBrickID(childPosition)] > BI_CHILD_EMPTY) ||
                  (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(0, 1, 0, 0))] > BI_CHILD_EMPTY))
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

          if (bEvaluatePredicate && pContinue)
            if (!pContinue())
              return;

          uint32_t const z = iLayout.z - 1;
          uint32_t const y = iLayout.y - 1;
          UINTVECTOR4 const vBrickID(x, y, z, iLoD);
          uint32_t const brickIndex = pool.GetIntegerBrickID(vBrickID);
          if (vBrickMetadata[brickIndex] <= BI_MISSING) // only check bricks that are not cached in the pool
          {
            BrickKey const key = pDataset->TOCVectorToKey(vBrickID, iTimestep);
            bool const bContainsData = ContainsData<eRenderMode>(visibility, pDataset, key);
            if (!bContainsData) {
              vBrickMetadata[brickIndex] = BI_CHILD_EMPTY; // flag parent brick to be child empty for now so that we can save a couple of tests below

              UINTVECTOR4 const childPosition(x*2, y*2, z*2, iLoD-1);
              if ((vBrickMetadata[pool.GetIntegerBrickID(childPosition)] > BI_CHILD_EMPTY) ||
                  (vBrickMetadata[pool.GetIntegerBrickID(childPosition + UINTVECTOR4(1, 0, 0, 0))] > BI_CHILD_EMPTY))
              {
                vBrickMetadata[brickIndex] = BI_EMPTY; // downgrade parent brick if we found a non-empty child
              }
            }
          }
        } // for x
      } // if y and z are odd

      // single brick at the x/y/z corner
      if (iChildLayout.x % 2 && iChildLayout.y % 2 && iChildLayout.z % 2) {

        if (bEvaluatePredicate && pContinue)
          if (!pContinue())
            return;

        uint32_t const z = iLayout.z - 1;
        uint32_t const y = iLayout.y - 1;
        uint32_t const x = iLayout.x - 1;
        UINTVECTOR4 const vBrickID(x, y, z, iLoD);
        uint32_t const brickIndex = pool.GetIntegerBrickID(vBrickID);
        if (vBrickMetadata[brickIndex] <= BI_MISSING) // only check bricks that are not cached in the pool
        {
          BrickKey const key = pDataset->TOCVectorToKey(vBrickID, iTimestep);
          bool const bContainsData = ContainsData<eRenderMode>(visibility, pDataset, key);
          if (!bContainsData) {
            vBrickMetadata[brickIndex] = BI_CHILD_EMPTY; // flag parent brick to be child empty for now so that we can save a couple of tests below

            UINTVECTOR4 const childPosition(x*2, y*2, z*2, iLoD-1);
            if (vBrickMetadata[pool.GetIntegerBrickID(childPosition)] > BI_CHILD_EMPTY)
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
  void PotentiallyUploadBricksToBrickPool(VisibilityState const& visibility, UVFDataset const* pDataset, size_t iTimestep, GLVolumePool& pool,
                                          std::vector<uint32_t>& vBrickMetadata, std::vector<UINTVECTOR4> const& vBrickIDs, std::vector<unsigned char>& vUploadMem)
  {
    // now iterate over the missing bricks and upload them to the GPU
    // todo: consider batching this if it turns out to make a difference
    //       from submitting each brick separately
    for (auto missingBrick = vBrickIDs.cbegin(); missingBrick < vBrickIDs.cend(); missingBrick++) {
      UINTVECTOR4 const& vBrickID = *missingBrick;
      BrickKey const key = pDataset->TOCVectorToKey(vBrickID, iTimestep);
      UINTVECTOR3 const vVoxelSize = pDataset->GetBrickVoxelCounts(key);

      uint32_t const brickIndex = pool.GetIntegerBrickID(vBrickID);
      // the brick could be flagged as empty by now if the async updater tested the brick after we ran the last render pass
      if (vBrickMetadata[brickIndex] == BI_MISSING) {
        // we might not have tested the brick for visibility yet since the updater's still running and we do not have a BI_UNKNOWN flag for now
        bool const bContainsData = ContainsData<eRenderMode>(visibility, pDataset, key);
        if (bContainsData) {
          pDataset->GetBrick(key, vUploadMem);
          if (!pool.UploadBrick(BrickElemInfo(vBrickID, vVoxelSize), &vUploadMem[0]))
            return;
        } else {
          vBrickMetadata[brickIndex] = BI_EMPTY;
          pool.UploadMetadataTexel(brickIndex);
        }
      } else if (vBrickMetadata[brickIndex] < BI_MISSING) {
        // if the updater touched the brick in the meanwhile, we need to upload the meta texel
        pool.UploadMetadataTexel(brickIndex);
      } else {
        assert(false); // should never happen
      }
    }
  }

} // anonymous namespace

void GLVolumePool::RecomputeVisibility(VisibilityState const& visibility, size_t iTimestep)
{
#ifdef PROFILE_GLVOLUMEPOOL
  m_Timer.Start();
#endif

  // pause async updater because we will touch the meta data
  m_pUpdater->Pause();

  // reset meta data for all bricks (BI_MISSING means that we haven't test the data for visibility until the async updater finishes)
  std::fill(m_vBrickMetadata.begin(), m_vBrickMetadata.end(), BI_MISSING);

  // recompute visibility for cached bricks immediately
  switch (visibility.GetRenderMode()) {
  case AbstrRenderer::RM_1DTRANS:
    RecomputeVisibilityForBrickPool<AbstrRenderer::RM_1DTRANS>(visibility, m_pDataset, iTimestep, *this, m_vBrickMetadata, m_vPoolSlotData);
    break;
  case AbstrRenderer::RM_2DTRANS:
    RecomputeVisibilityForBrickPool<AbstrRenderer::RM_2DTRANS>(visibility, m_pDataset, iTimestep, *this, m_vBrickMetadata, m_vPoolSlotData);
    break;
  case AbstrRenderer::RM_ISOSURFACE:
    RecomputeVisibilityForBrickPool<AbstrRenderer::RM_ISOSURFACE>(visibility, m_pDataset, iTimestep, *this, m_vBrickMetadata, m_vPoolSlotData);
    break;
  default:
    T_ERROR("Unhandled rendering mode.");
    return;
  }

  // upload new meta data to GPU
  UploadMetadataTexture();

  // restart async updater because visibility changed
  m_pUpdater->Restart(visibility, iTimestep);
  m_bVisibilityUpdated = false;
  OTHER("computed visibility for %d bricks in volume pool and started async visibility update for the entire hierarchy", m_vPoolSlotData.size());

#ifdef PROFILE_GLVOLUMEPOOL
  m_TimesRecomputeVisibility.Push(static_cast<float>(m_Timer.Elapsed()));
#endif
}

void GLVolumePool::UploadBricks(const std::vector<UINTVECTOR4>& vBrickIDs, std::vector<unsigned char>& vUploadMem)
{
  // pause async updater because we will touch the meta data
  bool const bBusy = m_pUpdater->Pause();

  if (!vBrickIDs.empty())
  {
    PrepareForPaging();

    VisibilityState const& visibility = m_pUpdater->GetVisibility();
    size_t iTimestep = m_pUpdater->GetTimestep();

    if (bBusy || !m_bVisibilityUpdated) {
      switch (visibility.GetRenderMode()) {
      case AbstrRenderer::RM_1DTRANS:
        PotentiallyUploadBricksToBrickPool<AbstrRenderer::RM_1DTRANS>(visibility, m_pDataset, iTimestep, *this, m_vBrickMetadata, vBrickIDs, vUploadMem);
        break;
      case AbstrRenderer::RM_2DTRANS:
        PotentiallyUploadBricksToBrickPool<AbstrRenderer::RM_2DTRANS>(visibility, m_pDataset, iTimestep, *this, m_vBrickMetadata, vBrickIDs, vUploadMem);
        break;
      case AbstrRenderer::RM_ISOSURFACE:
        PotentiallyUploadBricksToBrickPool<AbstrRenderer::RM_ISOSURFACE>(visibility, m_pDataset, iTimestep, *this, m_vBrickMetadata, vBrickIDs, vUploadMem);
        break;
      default:
        T_ERROR("Unhandled rendering mode.");
        return;
      }
    } else {
      for (auto missingBrick = vBrickIDs.cbegin(); missingBrick < vBrickIDs.cend(); missingBrick++) {
        UINTVECTOR4 const& vBrickID = *missingBrick;
        BrickKey const key = m_pDataset->TOCVectorToKey(vBrickID, iTimestep);
        UINTVECTOR3 const vVoxelSize = m_pDataset->GetBrickVoxelCounts(key);

        m_pDataset->GetBrick(key, vUploadMem);
        if (!UploadBrick(BrickElemInfo(vBrickID, vVoxelSize), &vUploadMem[0]))
          break;
      }
    }
  }
  
  if (bBusy) {
    m_pUpdater->Resume(); // resume async updater if we were busy
  } else {
    if (!m_bVisibilityUpdated)
    {
      m_bVisibilityUpdated = true; // must be set one frame delayed otherwise we might upload empty bricks
      UploadMetadataTexture(); // we want to upload the whole meta texture when async updater is done

#ifdef PROFILE_GLVOLUMEPOOL
      AsyncVisibilityUpdater::Stats const& stats = m_pUpdater->GetStats();
      OTHER("async visibility update completed for %d bricks in %.2f ms including %d interruptions that cost %.3f ms (%.2f bricks/ms)"
        , m_iTotalBrickCount, stats.fTimeTotal, stats.iInterruptions, stats.fTimeInterruptions, m_iTotalBrickCount/stats.fTimeTotal);
      OTHER("meta texture (%.4f MB) upload cost [avg: %.2f, min: %.2f, max: %.2f, samples: %d]"
        , m_PoolMetadataTexture->GetCPUSize() / 1024.0f / 1024.0f, m_TimesMetaTextureUpload.GetAvg(), m_TimesMetaTextureUpload.GetMin(), m_TimesMetaTextureUpload.GetMax(), m_TimesMetaTextureUpload.GetHistroryLength());
      OTHER("recompute visibility cost [avg: %.2f, min: %.2f, max: %.2f, samples: %d]"
        , m_TimesRecomputeVisibility.GetAvg(), m_TimesRecomputeVisibility.GetMin(), m_TimesRecomputeVisibility.GetMax(), m_TimesRecomputeVisibility.GetHistroryLength());
#else
      OTHER("async visibility update completed for %d bricks", m_iTotalBrickCount);
#endif
    }
  }
}

AsyncVisibilityUpdater::AsyncVisibilityUpdater(GLVolumePool& parent)
  : m_Pool(parent)
  , m_Visibility()
  , m_iTimestep(0)
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

void AsyncVisibilityUpdater::Restart(VisibilityState const& visibility, size_t iTimeStep)
{
  SCOPEDLOCK(m_StateGuard);
  Pause();
  m_Visibility = visibility;
  m_iTimestep = iTimeStep;
#if 1 // set to 0 to prevent the async worker to do anything useful
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
  bool const bIsBusy = m_eState != Idle;
  return bIsBusy;
}

void AsyncVisibilityUpdater::Resume()
{
  m_Worker.WakeOne(); // resume worker
}

bool AsyncVisibilityUpdater::Continue()
{
  if (!m_bContinue) // check if thread should terminate
    return false;

#ifdef PROFILE_GLVOLUMEPOOL
  double t = m_Timer.Elapsed();
#endif

  SCOPEDLOCK(m_StateGuard);
  if (m_eState == PauseRequested) {
    m_eState = Paused;
    m_Parent.WakeOne(); // wake up parent because worker just paused
    m_Worker.Wait(m_StateGuard); // wait until parent wakes worker to continue

#ifdef PROFILE_GLVOLUMEPOOL
    m_Stats.fTimeInterruptions = m_Timer.Elapsed() - t;
    m_Stats.iInterruptions++;
#endif
  }
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

#ifdef PROFILE_GLVOLUMEPOOL
    m_Stats.fTimeInterruptions = 0.0;
    m_Stats.fTimeTotal = 0.0;
    m_Stats.iInterruptions = 0;
    m_Timer.Start();
#endif

    switch (m_Visibility.GetRenderMode()) {
    case AbstrRenderer::RM_1DTRANS:
      RecomputeVisibilityForOctree<true, AbstrRenderer::RM_1DTRANS>(m_Visibility, m_Pool.m_pDataset, m_iTimestep, m_Pool, m_Pool.m_vBrickMetadata, pContinue);
      break;
    case AbstrRenderer::RM_2DTRANS:
      RecomputeVisibilityForOctree<true, AbstrRenderer::RM_2DTRANS>(m_Visibility, m_Pool.m_pDataset, m_iTimestep, m_Pool, m_Pool.m_vBrickMetadata, pContinue);
      break;
    case AbstrRenderer::RM_ISOSURFACE:
      RecomputeVisibilityForOctree<true, AbstrRenderer::RM_ISOSURFACE>(m_Visibility, m_Pool.m_pDataset, m_iTimestep, m_Pool, m_Pool.m_vBrickMetadata, pContinue);
      break;
    default:
      assert(false); //T_ERROR("Unhandled rendering mode.");
      break;
    }

#ifdef PROFILE_GLVOLUMEPOOL
    m_Stats.fTimeTotal = m_Timer.Elapsed();
#endif
  }
}

void GLVolumePool::FreeGLResources() {
  if (m_PoolMetadataTexture) {
    m_PoolMetadataTexture->Delete();
    delete m_PoolMetadataTexture;
  }
  if (m_PoolDataTexture) {
    m_PoolDataTexture->Delete();
    delete m_PoolDataTexture;
  }
}

uint64_t GLVolumePool::GetCPUSize() const {
  return m_PoolMetadataTexture->GetCPUSize() + m_PoolDataTexture->GetCPUSize();
}

uint64_t GLVolumePool::GetGPUSize() const {
  return m_PoolMetadataTexture->GetGPUSize() + m_PoolDataTexture->GetGPUSize();
}

void GLVolumePool::SetFilterMode(GLenum filter) {
  m_filter = filter;
  m_PoolDataTexture->SetFilter(filter, filter);
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
