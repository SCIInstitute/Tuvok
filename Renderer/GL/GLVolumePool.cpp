#include <sstream>

#include "GLVolumePool.h"
#include "Basics/MathTools.h"
#include <stdexcept>

using namespace tuvok;

static uint32_t GetLoDCount(const UINTVECTOR3& volumeSize) {
  uint32_t maxP2 = MathTools::NextPow2( volumeSize.maxVal() );
  return MathTools::Log2(maxP2);
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
    m_poolSize(poolSize),
    m_maxBrickSize(maxBrickSize),
    m_overlap(overlap),
    m_volumeSize(volumeSize),
    m_internalformat(internalformat),
    m_format(format),
    m_type(type),
    m_iMetaTextureUnit(0),
    m_iDataTextureUnit(1),
    m_bUseGLCore(bUseGLCore)
{

  // fill the pool slot information
  const UINTVECTOR3 slotLayout = poolSize/maxBrickSize;
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
    iOffset += GetBrickLayout(m_volumeSize, m_maxBrickSize, i).volume();
  }

  CreateGLResources();
  UpdateMetadata();
  UploadMetaData();
}

uint32_t GLVolumePool::GetIntegerBrickID(const UINTVECTOR4& vBrickID) const {
  UINTVECTOR3 bricks = GetBrickLayout(m_volumeSize, m_maxBrickSize, vBrickID.w);

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
     << "#define BI_MISSING 0" << std::endl
     << "#define BI_EMPTY   1" << std::endl
     << "" << std::endl
     << "layout(binding = " << m_iDataTextureUnit << ") uniform sampler3D volumePool;" << std::endl
     << "const ivec3 iPoolSize = ivec3(" << m_PoolDataTexture->GetSize().x << ", " 
                                         << m_PoolDataTexture->GetSize().y << ", " 
                                         << m_PoolDataTexture->GetSize().z <<");" << std::endl
     << "const ivec3 poolCapacity = ivec3(" << m_PoolDataTexture->GetSize().x/m_maxBrickSize.x << ", " << 
                                               m_PoolDataTexture->GetSize().y/m_maxBrickSize.y << ", " <<
                                               m_PoolDataTexture->GetSize().z/m_maxBrickSize.z <<");" << std::endl
     << "const ivec3 maxBrickSize = ivec3(" << m_maxBrickSize.x << ", " 
                                            << m_maxBrickSize.y << ", " 
                                            << m_maxBrickSize.z <<");" << std::endl
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
    FLOATVECTOR3 vLoDSize = GetFloatBrickLayout(m_volumeSize, m_maxBrickSize, i);    
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
     << "ivec2 GetBrickIndex(uvec4 brickCoords) {" << std::endl
     << "  uint iLODOffset = vLODOffset[brickCoords.w];"<< std::endl
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
     << "  corners[0] = vec3(brickCoords.xyz) / vLODLayout[brickCoords.w];" << std::endl
     << "  fullBrickSize = vec3(brickCoords.xyz+1) / vLODLayout[brickCoords.w];" << std::endl
     << "  corners[1] = min(vec3(1.0,1.0,1.0), fullBrickSize);" << std::endl
     << "}" << std::endl
     << "" << std::endl   
     << "vec3 BrickExit(vec3 pointInBrick, vec3 dir, in vec3 corners[2]) {" << std::endl
	   << "  vec3 div = 1.0 / dir;" << std::endl
	   << "  ivec3 side = ivec3(step(0.0,div));" << std::endl
	   << "  vec3 tIntersect;" << std::endl
	   << "  tIntersect.x = (corners[side[0]].x - pointInBrick.x) * div.x * (corners[1].x-corners[0].x);" << std::endl
	   << "  tIntersect.y = (corners[side[1]].y - pointInBrick.y) * div.y * (corners[1].y-corners[0].y);" << std::endl
	   << "  tIntersect.z = (corners[side[2]].z - pointInBrick.z) * div.z * (corners[1].z-corners[0].z);" << std::endl
	   << "  return pointInBrick + min(min(tIntersect.x, tIntersect.y), tIntersect.z) * dir;" << std::endl
     << "}" << std::endl
     << " " << std::endl
     << "uvec3 InfoToCoords(in uint brickInfo) {" << std::endl
     << "  uint index = brickInfo-2;" << std::endl
     << "  uvec3 vBrickCoords;" << std::endl
	   << "  vBrickCoords.x = index % poolCapacity.x;" << std::endl
	   << "  vBrickCoords.y = (index / poolCapacity.x) % poolCapacity.y;" << std::endl
	   << "  vBrickCoords.z = index / (poolCapacity.x*poolCapacity.y);" << std::endl
	   << "  return vBrickCoords;" << std::endl
     << "}" << std::endl
     << " " << std::endl
     << "void BrickPoolCoords(in uint brickInfo,  out vec3 corners[2]) {" << std::endl
     << "  uvec3 poolVoxelPos = InfoToCoords(brickInfo) * maxBrickSize;" << std::endl
     << "  corners[0] = vec3(poolVoxelPos) / vec3(iPoolSize);" << std::endl
     << "  corners[1] = vec3(poolVoxelPos+1) / vec3(iPoolSize);" << std::endl
     << "}" << std::endl
     << " " << std::endl
     << "void NormCoordsToPoolCoords(in vec3 normEntryCoords, in vec3 normExitCoords, in vec3 corners[2]," << std::endl
     << "                            in uint brickInfo, out vec3 poolEntryCoods, out vec3 poolExitCoords) {" << std::endl
     << "  vec3 poolCorners[2];" << std::endl
     << "  BrickPoolCoords(brickInfo, poolCorners);" << std::endl
     << "  vec3 normToPoolScale = (poolCorners[1]-poolCorners[0])/(corners[1]-corners[0]);" << std::endl
     << "  vec3 normToPoolTrans = poolCorners[0]-corners[0]*normToPoolScale;" << std::endl
     << "  poolEntryCoods = normEntryCoords + normToPoolTrans * normToPoolScale;" << std::endl
     << "  poolExitCoords = normExitCoords + normToPoolTrans * normToPoolScale;" << std::endl
     << "}" << std::endl
     << "" << std::endl
     << "bool GetBrick(in vec3 normEntryCoords, in uint iLOD, in vec3 direction," << std::endl
     << "              out vec3 poolEntryCoods, out vec3 poolExitCoords," << std::endl
     << "              out vec3 normExitCoods) {" << std::endl
     << "  uvec4 brickCoords = ComputeBrickCoords(normEntryCoords, iLOD);" << std::endl
     << "  uint  brickInfo   = GetBrickInfo(brickCoords);" << std::endl
     << "  bool bFoundRequestedResolution = (brickInfo != BI_MISSING);" << std::endl
     << "  if (!bFoundRequestedResolution) {" << std::endl
     << "    // when the requested resolution is not present find look for a lower one" << std::endl
     << "    Hash(brickCoords);" << std::endl
     << "    do {" << std::endl
     << "      iLOD++;" << std::endl
     << "      brickCoords = ComputeBrickCoords(normEntryCoords, iLOD);" << std::endl
     << "      brickInfo   = GetBrickInfo(brickCoords);" << std::endl
     << "    } while (brickInfo != BI_MISSING);" << std::endl
     << "  } else {" << std::endl
     << "    // when we found an empty brick check if lower resolutions are also empty" << std::endl
     << "    while (iLOD > iMaxLOD && brickInfo != BI_EMPTY) {" << std::endl
     << "      iLOD++;" << std::endl
     << "      uvec4 lowResBrickCoords = ComputeBrickCoords(normEntryCoords, iLOD);" << std::endl
     << "      uint lowResBrickInfo = GetBrickInfo(lowResBrickCoords);" << std::endl
     << "      if (lowResBrickInfo == BI_EMPTY) {" << std::endl
     << "        brickCoords = lowResBrickCoords;" << std::endl
     << "        brickInfo = lowResBrickInfo;" << std::endl
     << "      }" << std::endl
     << "    }" << std::endl
     << "  }" << std::endl
     << "  vec3 corners[2];" << std::endl
     << "  vec3 fullBrickCorner;" << std::endl
     << "  GetBrickCorners(brickCoords, corners, fullBrickCorner);" << std::endl
     << "  normExitCoods = BrickExit(normEntryCoords, direction, corners);" << std::endl
     << "  corners[1] = fullBrickCorner;" << std::endl
     << "  NormCoordsToPoolCoords(normEntryCoords, normExitCoods, corners," << std::endl
     << "                         brickInfo, poolEntryCoods, normExitCoods);" << std::endl
     << "  return bFoundRequestedResolution;" << std::endl
     << "}" << std::endl
     << "" << std::endl
     << "vec3 TransformToPoolSpace(in vec3 direction, in vec3 volumeAspect, in float sampleRateModifier) {" << std::endl
     << "  // get rid of the volume aspect ratio, and trace in cubic voxel space" << std::endl
     << "  direction /= volumeAspect;" << std::endl
     << "  // normalize the direction" << std::endl
     << "  direction = normalize(direction);" << std::endl
     << "  // scale to volume pool's norm coodinates" << std::endl
     << "  direction /= vec3(iPoolSize);" << std::endl
     << "  // do (roughly) two samples per voxel and apply user defined sample density" << std::endl
     << "  return direction / 2.0*sampleRateModifier;" << std::endl
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
     << "}" << std::endl;


  std::stringstream debug(ss.str());
  std::string line;
  unsigned int iLine = 1;
  while(std::getline(debug, line)) {
      T_ERROR("%i %s", iLine++, line.c_str());
  }

  return ss.str();
}

bool GLVolumePool::UploadBrick(const BrickElemInfo& metaData, void* pData) {
  // find free position in pool
  UINTVECTOR3 vPoolCoordinates = FindNextPoolPosition();

  // update bricks in pool list to include the new brick
  VolumePoolElemInfo vpei(metaData, vPoolCoordinates);
  m_BricksInPool.push_back(vpei);

  // upload CPU metadata (does not update the 2D texture yet)
  // this is done by the explicit UploadMetaData call to 
  // only upload the updated data once all bricks have been 
  // updated
  UpdateMetadata();

  // upload brick to 3D texture
  m_PoolDataTexture->SetData(vPoolCoordinates*m_maxBrickSize,
                             metaData.m_vVoxelSize, pData);

  return true;
}

bool GLVolumePool::IsBrickResident(const UINTVECTOR4& vBrickID) const {

  int32_t iBrickID = GetIntegerBrickID(vBrickID);
  for (auto slot = m_PoolSlotData.begin(); slot<m_PoolSlotData.end();++slot) {
    if (int32_t(slot->m_iBrickID) == iBrickID) return true;
  }

  return false;
}

void GLVolumePool::BindTexures() const {
  m_PoolMetadataTexture->Bind(m_iMetaTextureUnit);
  m_PoolDataTexture->Bind(m_iDataTextureUnit);
}

GLVolumePool::~GLVolumePool() {
  FreeGLResources();
}

void GLVolumePool::CreateGLResources() {
  m_PoolDataTexture = new GLTexture3D(m_poolSize.x, m_poolSize.y, m_poolSize.z,
                                      m_internalformat, m_format, m_type);
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


UINTVECTOR3 GLVolumePool::FindNextPoolPosition() const {
  // TODO
  return UINTVECTOR3();
}

void GLVolumePool::UploadMetaData() {
  m_PoolMetadataTexture->SetData(&m_brickMetaData[0]);
}

void GLVolumePool::UpdateMetadata() {
  // TODO
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

   Copyright (c) 2011 Interactive Visualization and Data Analysis Group.


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
