#include <sstream>

#include "GLVolumePool.h"
#include "Basics/MathTools.h"
#include <stdexcept>
#include <algorithm>
#include "GLSLProgram.h"

enum BrickIDFlags {
  BI_CHILD_EMPTY = 0,
  BI_EMPTY,
  BI_MISSING,
  BI_FLAG_COUNT
};

using namespace tuvok;

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

static FLOATVECTOR3 GetFloatBrickLayout(const UINTVECTOR3& volumeSize, const UINTVECTOR3& maxBrickSize, uint32_t iLoD) {
  FLOATVECTOR3 baseBrickCount(float(volumeSize.x)/maxBrickSize.x, 
                              float(volumeSize.y)/maxBrickSize.y,
                              float(volumeSize.z)/maxBrickSize.z);
  return baseBrickCount/float(MathTools::Pow2(iLoD));
}


static UINTVECTOR3 GetBrickLayout(const UINTVECTOR3& volumeSize, const UINTVECTOR3& maxBrickSize, uint32_t iLoD) {
  UINTVECTOR3 baseBrickCount(uint32_t(ceil(double(volumeSize.x)/maxBrickSize.x)), 
                             uint32_t(ceil(double(volumeSize.y)/maxBrickSize.y)),
                             uint32_t(ceil(double(volumeSize.z)/maxBrickSize.z)));
  return GetLoDSize(baseBrickCount, iLoD);
}

GLVolumePool::GLVolumePool(const UINTVECTOR3& poolSize, const UINTVECTOR3& maxBrickSize,
                           const UINTVECTOR3& overlap, const UINTVECTOR3& volumeSize,
                           GLint internalformat, GLenum format, GLenum type, 
                            bool bUseGLCore)
  : m_PoolMetadataTexture(NULL),
    m_PoolDataTexture(NULL),
    m_vPoolCapacity(0,0,0),
    m_poolSize(poolSize),
    m_maxInnerBrickSize(maxBrickSize-overlap*2),
    m_maxTotalBrickSize(maxBrickSize),
    m_volumeSize(volumeSize),
    m_internalformat(internalformat),
    m_format(format),
    m_type(type),
    m_iTimeOfCreation(1),
    m_iMetaTextureUnit(0),
    m_iDataTextureUnit(1),
    m_bUseGLCore(bUseGLCore),
    m_iInsertPos(0)
{

  // fill the pool slot information
  const UINTVECTOR3 slotLayout = poolSize/m_maxTotalBrickSize;
  for (uint32_t z = 0;z<slotLayout.z;++z) {
    for (uint32_t y = 0;y<slotLayout.y;++y) {
      for (uint32_t x = 0;x<slotLayout.x;++x) {
        m_PoolSlotData.push_back(PoolSlotData(UINTVECTOR3(x,y,z)));
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
}

uint32_t GLVolumePool::GetIntegerBrickID(const UINTVECTOR4& vBrickID) const {
  UINTVECTOR3 bricks = GetBrickLayout(m_volumeSize, m_maxInnerBrickSize, vBrickID.w);
  return vBrickID.x + vBrickID.y * bricks.x + vBrickID.z * bricks.x * bricks.y + m_vLoDOffsetTable[vBrickID.w];
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

  ss << "" << std::endl
     << "layout(binding = " << m_iMetaTextureUnit << ") uniform usampler2D metaData;" << std::endl
     << "const int iMetaTextureWidth = " << m_PoolMetadataTexture->GetSize().x << ";" << std::endl
     << "" << std::endl
     << "#define BI_CHILD_EMPTY " << BI_CHILD_EMPTY << std::endl
     << "#define BI_EMPTY "       << BI_EMPTY << std::endl
     << "#define BI_MISSING "     << BI_MISSING << std::endl
     << "#define BI_FLAG_COUNT "  << BI_FLAG_COUNT << std::endl
     << "" << std::endl
     << "layout(binding = " << m_iDataTextureUnit << ") uniform sampler3D volumePool;" << std::endl
     << "const ivec3 iPoolSize = ivec3(" << m_PoolDataTexture->GetSize().x << ", " 
                                         << m_PoolDataTexture->GetSize().y << ", " 
                                         << m_PoolDataTexture->GetSize().z <<");" << std::endl
     << "const ivec3 poolCapacity = ivec3(" << m_vPoolCapacity.x << ", " << 
                                               m_vPoolCapacity.y << ", " <<
                                               m_vPoolCapacity.z <<");" << std::endl
     << "// the total size of a brick in the pool, including the boundary" << std::endl
     << "const ivec3 maxTotalBrickSize = ivec3(" << m_maxTotalBrickSize.x << ", " 
                                                 << m_maxTotalBrickSize.y << ", " 
                                                 << m_maxTotalBrickSize.z <<");" << std::endl
     << "// just the addressable (inner) size of a brick" << std::endl
     << "const ivec3 maxInnerBrickSize = ivec3(" << m_maxInnerBrickSize.x << ", " 
                                            << m_maxInnerBrickSize.y << ", " 
                                            << m_maxInnerBrickSize.z <<");" << std::endl
     << "// brick overlap voxels (in pool texcoords" << std::endl
     << "const vec3 overlap = vec3(" << (m_maxTotalBrickSize.x-m_maxInnerBrickSize.x)/(2.0f*m_PoolDataTexture->GetSize().x) << ", " 
                                     << (m_maxTotalBrickSize.y-m_maxInnerBrickSize.y)/(2.0f*m_PoolDataTexture->GetSize().y) << ", " 
                                     << (m_maxTotalBrickSize.z-m_maxInnerBrickSize.z)/(2.0f*m_PoolDataTexture->GetSize().z) <<");" << std::endl
     << "uniform float fLoDFactor;" << std::endl
     << "uniform float fLevelZeroWorldSpaceError;" << std::endl
     << "uniform vec3 volumeAspect;" << std::endl
     << "const uint iMaxLOD = " << iLodCount-1 << ";" << std::endl
     << "const uint vLODOffset[" << iLodCount << "] = uint[](";
  for (uint32_t i = 0;i<iLodCount;++i) {
    ss << m_vLoDOffsetTable[i];
    if (i<iLodCount-1) {
      ss << ", ";
    }
  }
  ss << ");" << std::endl
     << "const vec3 vLODLayout[" << iLodCount << "] = vec3[](" << std::endl;
  for (uint32_t i = 0;i<m_vLoDOffsetTable.size();++i) {
    FLOATVECTOR3 vLoDSize = GetFloatBrickLayout(m_volumeSize, m_maxInnerBrickSize, i);    
    ss << "   vec3(" << vLoDSize.x << ", " << vLoDSize.y << ", " << vLoDSize.z << ")";
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
     << "  uvec2 iLODLayout = uvec2(ceil(vLODLayout[brickCoords.w].x), ceil(vLODLayout[brickCoords.w].y));"<< std::endl
     << "  uint iBrickIndex = iLODOffset + brickCoords.x + brickCoords.y * iLODLayout.x + brickCoords.z * iLODLayout.x * iLODLayout.y;" << std::endl
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
	   << "  tIntersect.x = (corners[side.x].x - pointInBrick.x) * div.x;// * (corners[1].x-corners[0].x);" << std::endl
	   << "  tIntersect.y = (corners[side.y].y - pointInBrick.y) * div.y;// * (corners[1].y-corners[0].y);" << std::endl
	   << "  tIntersect.z = (corners[side.z].z - pointInBrick.z) * div.z;// * (corners[1].z-corners[0].z);" << std::endl
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
     << "                            in uint brickInfo, out vec3 poolEntryCoords, out vec3 poolExitCoords) {" << std::endl
     << "  vec3 poolCorners[2];" << std::endl
     << "  BrickPoolCoords(brickInfo, poolCorners);" << std::endl
     << "  vec3 normToPoolScale = (poolCorners[1]-poolCorners[0])/(corners[1]-corners[0]);" << std::endl
     << "  vec3 normToPoolTrans = poolCorners[0]-corners[0]*normToPoolScale;" << std::endl
     << "  poolEntryCoords  = (normEntryCoords * normToPoolScale + normToPoolTrans);" << std::endl
     << "  poolExitCoords   = (normExitCoords  * normToPoolScale + normToPoolTrans);" << std::endl
     << "}" << std::endl
     << "" << std::endl
     << "bool GetBrick(in vec3 normEntryCoords, in uint iLOD, in vec3 direction," << std::endl
     << "              out vec3 poolEntryCoords, out vec3 poolExitCoords," << std::endl
     << "              out vec3 normExitCoords, out bool bEmpty, out uvec4 brickCoords) {" << std::endl
     << "  bool bFoundRequestedResolution = true;" << std::endl
     << "  normEntryCoords = max(vec3(0.),min(normEntryCoords,vec3(1.)));" << std::endl
     << "  brickCoords = ComputeBrickCoords(normEntryCoords, iLOD);" << std::endl
     << "  uint  brickInfo   = GetBrickInfo(brickCoords);" << std::endl
     << "  if (brickInfo == BI_MISSING) {" << std::endl
     << "    // when the requested resolution is not present find look for a lower one" << std::endl
     << "    bFoundRequestedResolution = false;" << std::endl
     << "    ReportMissingBrick(brickCoords);" << std::endl
     << "    do {" << std::endl
     << "      iLOD++;" << std::endl
     << "      brickCoords = ComputeBrickCoords(normEntryCoords, iLOD);" << std::endl
     << "      brickInfo   = GetBrickInfo(brickCoords);" << std::endl
     << "    } while (brickInfo == BI_MISSING);" << std::endl
     << "  }" << std::endl
     << "  bEmpty = (brickInfo <= BI_EMPTY);" << std::endl
     << "  if (bEmpty) {" << std::endl
     << "    // when we find an empty brick check if the lower resolutions are also empty" << std::endl
     << "    while (iLOD+1 < iMaxLOD) {" << std::endl
     << "      iLOD++;" << std::endl
     << "      uvec4 lowResBrickCoords = ComputeBrickCoords(normEntryCoords, iLOD);" << std::endl
     << "      uint lowResBrickInfo = GetBrickInfo(lowResBrickCoords);" << std::endl
     << "      if (lowResBrickInfo == BI_CHILD_EMPTY) {" << std::endl
     << "        brickCoords = lowResBrickCoords;" << std::endl
     << "        brickInfo = lowResBrickInfo;" << std::endl
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
     << "                         brickInfo, poolEntryCoords, poolExitCoords);" << std::endl
     << "  return bFoundRequestedResolution;" << std::endl
     << "}" << std::endl
     << "" << std::endl
     << "vec3 TransformToPoolSpace(in vec3 direction, in float sampleRateModifier) {" << std::endl
     << "  // get rid of the volume aspect ratio, and trace in cubic voxel space" << std::endl
     << "  direction /= volumeAspect;" << std::endl
     << "  // normalize the direction" << std::endl
     << "  direction = normalize(direction);" << std::endl
     << "  // scale to volume pool's norm coodinates" << std::endl
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


  // DEBUG code
  std::stringstream debug(ss.str());
  std::string line;
  unsigned int iLine = 1;
  while(std::getline(debug, line)) {
    MESSAGE("%i %s", iLine++, line.c_str());
  }
  // DEBUG code end

  return ss.str();
}


void GLVolumePool::UploadBrick(uint32_t iBrickID, const UINTVECTOR3& vVoxelSize, void* pData, 
                               size_t iInsertPos, uint64_t iTimeOfCreation) {
  if (m_PoolSlotData[iInsertPos].m_iBrickID >= 0) {
    // mark data that was previously paged-in as missing, but only if it 
    // is not empty
    if (m_brickMetaData[m_PoolSlotData[iInsertPos].m_iBrickID] > BI_MISSING) {
      m_brickMetaData[m_PoolSlotData[iInsertPos].m_iBrickID] = BI_MISSING;

      OTHER("Removing brick %i at queue position %i from pool", 
         int(m_PoolSlotData[iInsertPos].m_iBrickID), 
         int(iInsertPos));

    } 
  }

  m_PoolSlotData[iInsertPos].m_iBrickID = iBrickID;
  m_PoolSlotData[iInsertPos].m_iTimeOfCreation = iTimeOfCreation;

  uint32_t vPoolCoordinate = m_PoolSlotData[iInsertPos].PositionInPool().x +
                             m_PoolSlotData[iInsertPos].PositionInPool().y * m_vPoolCapacity.x +
                             m_PoolSlotData[iInsertPos].PositionInPool().z * m_vPoolCapacity.x * m_vPoolCapacity.y;

  MESSAGE("Loading brick %i at queue position %i (pool coord: %i=[%i, %i, %i]) at time %llu", 
          int(m_PoolSlotData[iInsertPos].m_iBrickID), 
          int(iInsertPos),
          int(vPoolCoordinate),
          int(m_PoolSlotData[iInsertPos].PositionInPool().x), 
          int(m_PoolSlotData[iInsertPos].PositionInPool().y), 
          int(m_PoolSlotData[iInsertPos].PositionInPool().z),
          iTimeOfCreation);

  // update metadata (does NOT update the texture on the GPU)
  // this is done by the explicit UploadMetaData call to 
  // only upload the updated data once all bricks have been 
  // updated  
  m_brickMetaData[m_PoolSlotData[iInsertPos].m_iBrickID] = vPoolCoordinate+BI_FLAG_COUNT;

  // upload brick to 3D texture
  m_PoolDataTexture->SetData(m_PoolSlotData[iInsertPos].PositionInPool()*m_maxTotalBrickSize,
                             vVoxelSize, pData);
}

void GLVolumePool::UploadFirstBrick(const UINTVECTOR3& m_vVoxelSize, void* pData) {
  uint32_t iLastBrickIndex = *(m_vLoDOffsetTable.end()-1);
  UploadBrick(iLastBrickIndex, m_vVoxelSize, pData, m_PoolSlotData.size()-1, std::numeric_limits<uint64_t>::max());
}

bool GLVolumePool::UploadBrick(const BrickElemInfo& metaData, void* pData) {
  // in this frame we already replaced all bricks (except the single low-res brick)
  // in the pool so now we should render them first
  if (m_iInsertPos >= m_PoolSlotData.size()-1) return false;

  int32_t iBrickID = GetIntegerBrickID(metaData.m_vBrickID);
  UploadBrick(iBrickID, metaData.m_vVoxelSize, pData, m_iInsertPos, m_iTimeOfCreation++);
  m_iInsertPos++;
  return true;
}

bool GLVolumePool::IsBrickResident(const UINTVECTOR4& vBrickID) const {

  int32_t iBrickID = GetIntegerBrickID(vBrickID);
  for (auto slot = m_PoolSlotData.begin(); slot<m_PoolSlotData.end();++slot) {
    if (int32_t(slot->m_iBrickID) == iBrickID) return true;
  }

  return false;
}

void GLVolumePool::Enable(float fLoDFactor, const FLOATVECTOR3& volumeAspect,
                          GLSLProgram* pShaderProgram) const {
  m_PoolMetadataTexture->Bind(m_iMetaTextureUnit);
  m_PoolDataTexture->Bind(m_iDataTextureUnit);

  pShaderProgram->Enable();
  pShaderProgram->Set("fLoDFactor",fLoDFactor);
  pShaderProgram->Set("volumeAspect",volumeAspect.x, volumeAspect.y, volumeAspect.z);

  float fLevelZeroWorldSpaceError = (volumeAspect/FLOATVECTOR3(m_volumeSize)).maxVal();
  pShaderProgram->Set("fLevelZeroWorldSpaceError",fLevelZeroWorldSpaceError);
}

GLVolumePool::~GLVolumePool() {
  FreeGLResources();
}

void GLVolumePool::CreateGLResources() {
  m_PoolDataTexture = new GLTexture3D(m_poolSize.x, m_poolSize.y, m_poolSize.z,
                                      m_internalformat, m_format, m_type, 0, GL_LINEAR, GL_LINEAR);
  m_vPoolCapacity = UINTVECTOR3(m_PoolDataTexture->GetSize().x/m_maxTotalBrickSize.x,
                                m_PoolDataTexture->GetSize().y/m_maxTotalBrickSize.y,
                                m_PoolDataTexture->GetSize().z/m_maxTotalBrickSize.z);

  OTHER("Creating brick pool of size [%u,%u,%u] to hold a "
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
  uint32_t iTotalBrickCount = *(m_vLoDOffsetTable.end()-1)+1;

  // this is very unlikely but not impossible
  if (iTotalBrickCount > uint32_t(gpumax*gpumax)) {
    std::stringstream ss;    
    
    ss << "Unable to create brick metadata texture, as it needs to hold "
       << iTotalBrickCount << "entries but the max 2D texture size on this "
       << "machine is only " << gpumax << " x " << gpumax << "allowing for "
       << " a maximum of " << gpumax*gpumax << " indices.";

    T_ERROR(ss.str().c_str());
    throw std::runtime_error(ss.str().c_str()); 
  }
  
  UINTVECTOR2 vTexSize;
  vTexSize.x = uint32_t(ceil(sqrt(double(iTotalBrickCount))));
  vTexSize.y = uint32_t(ceil(double(iTotalBrickCount)/double(vTexSize.x)));
  m_brickMetaData.resize(vTexSize.area());
  std::fill(m_brickMetaData.begin(), m_brickMetaData.end(), BI_MISSING);

  std::stringstream ss;        
  ss << "Creating brick metadata texture of size " << vTexSize.x << " x " 
     << vTexSize.y << " to effectively hold  " << iTotalBrickCount << " entries. "
     << "Consequently, " << vTexSize.area() - iTotalBrickCount << " entries in "
     << "texture are wasted due to the 2D extions process.";
  MESSAGE(ss.str().c_str());

  m_PoolMetadataTexture = new GLTexture2D(
    vTexSize.x, vTexSize.x, GL_R32UI,
    GL_RED_INTEGER, GL_UNSIGNED_INT
  );
}

void GLVolumePool::UploadMetaData() {

  // DEBUG code
/*  MESSAGE("Brickpool Metadata entries:");
  for (size_t i = 0; i<m_PoolSlotData.size();++i) {
    switch (m_brickMetaData[i]) {
      case BI_MISSING     : break;
      case BI_EMPTY       : MESSAGE("  %i is empty",i); break;
      case BI_CHILD_EMPTY : MESSAGE("  %i is empty (including all children)",i); break;
      default             : MESSAGE("  %i loaded at position %i", i, int(m_brickMetaData[i]-BI_FLAG_COUNT)); break;
    }
  }
  */
  uint32_t used=0;
  for (size_t i = 0; i<m_PoolSlotData.size();++i) {
    if (m_brickMetaData[i] > BI_MISSING)
      used++;
  }
  OTHER("Pool Utilization %u/%u %g%%", used, uint32_t(m_PoolSlotData.size()), 100.0f*used/float(m_PoolSlotData.size()));
  // DEBUG code end

  m_PoolMetadataTexture->SetData(&m_brickMetaData[0]);
  std::sort(m_PoolSlotData.begin(), m_PoolSlotData.end());
  m_iInsertPos = 0;
}

void GLVolumePool::BrickIsVisible(uint32_t iLoD, uint32_t iIndexInLoD, bool bVisible, bool bChildrenVisible) {
  size_t index = iIndexInLoD+m_vLoDOffsetTable[iLoD];

  if (bVisible)  {
    // if this brick was previously empty then it is now 
    // missing, i.e. not empty and not cached
    if (m_brickMetaData[index] <= BI_EMPTY) {
      if (index < m_PoolSlotData.size()) {// restore the first brick
        m_brickMetaData[index] = BI_MISSING;
      } else {
        m_brickMetaData[index] = uint32_t(m_PoolSlotData.size()-1)+BI_FLAG_COUNT;
      }
    }
  } else {
    // if brick is in the pool -> clear that pool entry
    if (m_brickMetaData[index] > BI_MISSING) {
      if (index < m_PoolSlotData.size()-1) {// don't wipe the first low-res brick
        m_PoolSlotData[m_brickMetaData[index]-BI_FLAG_COUNT].Clear();
      }
    }
    // mark brick as empty
    m_brickMetaData[index] = bChildrenVisible ? BI_EMPTY : BI_CHILD_EMPTY;
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
