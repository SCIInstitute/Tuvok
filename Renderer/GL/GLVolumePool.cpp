#include <sstream>

#include "GLVolumePool.h"
#include "Basics/MathTools.h"

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

  // compute the offset table
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
  m_iMetaTextureUnit = iMetaTextureUnit;
  m_iDataTextureUnit = iDataTextureUnit;

  std::stringstream ss;

  if (m_bUseGLCore)
    ss << "#version 420 core" << std::endl;  
  else
    ss << "#version 420 compatibility" << std::endl;

  uint32_t iLodCount = GetLoDCount(m_volumeSize);

  ss << "" << std::endl
     << "layout(binding = " << m_iMetaTextureUnit << ") uniform sampler2D metaData;" << std::endl
     << "layout(binding = " << m_iDataTextureUnit << ") uniform sampler3D volumeData;" << std::endl
     << "" << std::endl
     << "uint Hash(uvec4 brick);" << std::endl
     << "" << std::endl
     << "const uint iLODOffset[" << m_vLoDOffsetTable.size() << "] = uint[](";
  for (uint32_t i = 0;i<m_vLoDOffsetTable.size();++i) {
    ss << m_vLoDOffsetTable[i];
    if (i<iLodCount-1) {
      ss << ", ";
    }
  }
  ss << ");" << std::endl;

  
  /*

  ss << ");" << std::endl
     << "vec4 SampleVolume(vec3 vOffset, vec3 coords) {" << std::endl
     << "  return sample3D(volumeData, coords+vOffset);" << std::endl
     << "}" << std::endl
     << "" << std::endl
     << "int HashValue(uint serializedValue) {" << std::endl
     << "  return int(serializedValue % " << m_iTableSize << ");" << std::endl
     << "}" << std::endl
     << "" << std::endl
     << "uint Hash(uvec4 bd) {" << std::endl
     << "  uint rehashCount = 0;" << std::endl
     << "  uint serializedValue = Serialize(bd);" << std::endl
     << "" << std::endl
     << "  do {" << std::endl
     << "    int hash = HashValue(serializedValue+rehashCount);" << std::endl
     << "    uint valueInImage = imageAtomicCompSwap(hashTable, hash, uint(0), serializedValue);" << std::endl
     << "    if (valueInImage == 0 || valueInImage == serializedValue) return rehashCount;" << std::endl
     << "  } while (++rehashCount < " << m_iRehashCount << ") ;" << std::endl
     << "" << std::endl
     << "  return uint(" << m_iRehashCount << ");" << std::endl
     << "}" << std::endl;

     */

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
  const uint32_t max_texture_size = std::min(4096, gpumax);

  // TODO: compute proper size

  m_PoolMetadataTexture = new GLTexture2D(
    max_texture_size, max_texture_size, GL_RGBA32F,
    GL_RGBA, GL_FLOAT
  );
}


UINTVECTOR3 GLVolumePool::FindNextPoolPosition() const {
  return UINTVECTOR3();
}

void GLVolumePool::UploadMetaData() {
  // TODO
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
  return 0;
//  return m_iSizePerElement * m_poolTexSize.volume() +
//         m_metaTexSize.area(); // TODO: * metatex elemen size
}

uint64_t GLVolumePool::GetGPUSize() const {
  return GetCPUSize();
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
