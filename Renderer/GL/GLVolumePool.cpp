/*/*
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

//!    File   : GLVolumePool.h
//!    Author : Jens Krueger
//!             IVDA, MMCI, DFKI Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : November 2011
//
//!    Copyright (C) 2011 IVDA, MMC, DFKI, SCI Institute


#include "GLVolumePool.h"

using namespace tuvok;

UINTVECTOR3 ExtendedOctreeInfo::GetLODCount(UINTVECTOR3 , UINTVECTOR3 , uint32_t ) {
  // TODO
  return UINTVECTOR3();
}

UINTVECTOR3 ExtendedOctreeInfo::GetLODSize(uint32_t , UINTVECTOR3 , UINTVECTOR3 , uint32_t ) {
  // TODO
  return UINTVECTOR3();
}

UINTVECTOR3 GetBrickSize(UINTVECTOR3 , uint32_t , UINTVECTOR3 , UINTVECTOR3 , uint32_t ) {
  // TODO
  return UINTVECTOR3();
}


GLVolumePool::GLVolumePool(UINTVECTOR3 , UINTVECTOR3 , uint32_t ,
                           const UINTVECTOR3& poolTexSize, const UINTVECTOR3& brickSize,
                           GLint internalformat, GLenum format, GLenum type,
                           uint32_t iSizePerElement) :
m_StaticLODLUT(NULL),
m_PoolMetadataTexture(NULL),
m_PoolDataTexture(NULL),
m_poolTexSize(poolTexSize),
m_brickSize(brickSize),
m_internalformat(internalformat),
m_format(format),
m_type(type),
m_iSizePerElement(iSizePerElement)
{
  CreateGLResources();
  UpdateMetadataTexture();
}

bool GLVolumePool::UploadBrick(const BrickElemInfo& metaData, void* pData) {
  // find free position in pool
  UINTVECTOR3 vPoolCoordiantes = FindNextPoolPosition();

  // update bricks in pool list to include the new brick
  VolumePoolElemInfo vpei(metaData, vPoolCoordiantes, 0);
  m_BricksInPool.push_back(vpei);

  // upload metadata change to 2D texture
  UpdateMetadataTexture();

  // upload brick to 3D texture
  m_PoolDataTexture->SetData(vPoolCoordiantes*m_brickSize, metaData.m_vVoxelSize, pData);

  return true;
}

bool GLVolumePool::IsBrickResident(const UINTVECTOR4& vBrickID) const {  
  for (std::deque< VolumePoolElemInfo >::const_iterator iter = m_BricksInPool.begin();
       iter != m_BricksInPool.end();
       iter++) {
         if (iter->m_vBrickID == vBrickID)  return true;
  }

  return false;
}

void GLVolumePool::BindTexures(unsigned int iMetaTextureUnit, unsigned int iDataTextureUnit) const{
  m_PoolMetadataTexture->Bind(iMetaTextureUnit);
  m_PoolDataTexture->Bind(iDataTextureUnit);
}

GLVolumePool::~GLVolumePool() {
  FreeGLResources();
}

void GLVolumePool::CreateGLResources() {
  // TODO

  // create Level-of-Detail Look-up-Table


}

UINTVECTOR3 GLVolumePool::FindNextPoolPosition() {
  // TODO
  return UINTVECTOR3();
}

void GLVolumePool::UpdateMetadataTexture() {
  // TODO
}

void GLVolumePool::FreeGLResources() {
  delete m_PoolMetadataTexture;
  delete m_PoolDataTexture;
  m_poolTexSize = UINTVECTOR3(0,0,0);
  m_metaTexSize = UINTVECTOR2(0,0);
}

UINT64 GLVolumePool::GetCPUSize() {
  return m_iSizePerElement * m_poolTexSize.volume() +
         m_metaTexSize.area(); // TODO: * metatex elemen size         
}

UINT64 GLVolumePool::GetGPUSize() {
  return GetCPUSize();
}

