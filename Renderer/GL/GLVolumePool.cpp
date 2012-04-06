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

//!    File   : GLVolumePool.h
//!    Author : Jens Krueger
//!             IVDA, MMCI, DFKI Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : November 2011
//
//!    Copyright (C) 2011 IVDA, MMCI, DFKI, SCI Institute
#include <stdexcept>
#ifdef _MSC_VER
# include <memory>
#else
# include <tr1/memory>
#endif

#include "GLVolumePool.h"

using namespace tuvok;

UINTVECTOR3 ExtendedOctreeInfo::GetLODCount(UINTVECTOR3, UINTVECTOR3,
                                            uint32_t) {
  // TODO
  return UINTVECTOR3();
}

UINTVECTOR3 ExtendedOctreeInfo::GetLODSize(uint32_t, UINTVECTOR3, UINTVECTOR3,
                                           uint32_t) {
  // TODO
  return UINTVECTOR3();
}

UINTVECTOR3 GetBrickSize(UINTVECTOR3, uint32_t, UINTVECTOR3, UINTVECTOR3,
                         uint32_t) {
  // TODO
  return UINTVECTOR3();
}


GLVolumePool::GLVolumePool(UINTVECTOR3 poolSize, UINTVECTOR3 brickSize,
                          GLint internalformat, GLenum format, GLenum type)
  : m_StaticLODLUT(NULL),
    m_PoolMetadataTexture(NULL),
    m_PoolDataTexture(NULL),
    m_poolSize(poolSize),
    m_brickSize(brickSize),
    m_internalformat(internalformat),
    m_format(format),
    m_type(type),
    m_allocPos(0,0,0)
{
  CreateGLResources();
  UpdateMetadataTexture();
}

bool GLVolumePool::UploadBrick(const BrickElemInfo& metaData, void* pData) {
  // find free position in pool
  UINTVECTOR3 vPoolCoordinates = FindNextPoolPosition();

  // update bricks in pool list to include the new brick
  VolumePoolElemInfo vpei(metaData, vPoolCoordinates, 0);
  m_BricksInPool.push_back(vpei);

  // upload metadata change to 2D texture
  UpdateMetadataTexture();

  // upload brick to 3D texture
  m_PoolDataTexture->SetData(vPoolCoordinates*m_brickSize,
                             metaData.m_vVoxelSize, pData);
  // save alloc position for where we were last time.
  m_allocPos = vPoolCoordinates;
  return true;
}

bool GLVolumePool::IsBrickResident(const UINTVECTOR4& vBrickID) const {
  for (std::deque<VolumePoolElemInfo>::const_iterator iter =
         m_BricksInPool.begin();
       iter != m_BricksInPool.end();
       iter++) {
         if (iter->m_vBrickID == vBrickID)  return true;
  }

  return false;
}

void GLVolumePool::BindTexures(unsigned int iMetaTextureUnit,
                               unsigned int iDataTextureUnit) const {
  m_PoolMetadataTexture->Bind(iMetaTextureUnit);
  m_PoolDataTexture->Bind(iDataTextureUnit);
}

GLVolumePool::~GLVolumePool() {
  FreeGLResources();
}

static uint32_t bytes_per_elem(GLenum gltype) {
  switch(gltype) {
    case GL_UNSIGNED_BYTE:
    case GL_BYTE:
    case GL_UNSIGNED_BYTE_3_3_2:
    case GL_UNSIGNED_BYTE_2_3_3_REV:
    case GL_BITMAP: return 1;
    case GL_UNSIGNED_SHORT:
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_5_6_5_REV:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_4_4_4_4_REV:
    case GL_UNSIGNED_SHORT_5_5_5_1:
    case GL_UNSIGNED_SHORT_1_5_5_5_REV:
    case GL_SHORT: return 2;
    case GL_UNSIGNED_INT:
    case GL_INT:
    case GL_FLOAT:
    case GL_UNSIGNED_INT_8_8_8_8:
    case GL_UNSIGNED_INT_8_8_8_8_REV:
    case GL_UNSIGNED_INT_10_10_10_2:
    case GL_UNSIGNED_INT_2_10_10_10_REV: return 4; break;
  }
  return 0;
}

void GLVolumePool::CreateGLResources() {
  m_PoolDataTexture = new GLTexture3D(m_poolSize.x, m_poolSize.y, m_poolSize.z,
                                      m_internalformat, m_format, m_type,
                                      bytes_per_elem(m_type));
  // we should really query GL_MAX_TEXTURE_SIZE, but for now ...
  const uint32_t max_texture_size = 4096;
  m_PoolMetadataTexture = new GLTexture2D(
    max_texture_size, max_texture_size, GL_LUMINANCE,
    GL_LUMINANCE, GL_UNSIGNED_INT, 4
  );
}

static UINTVECTOR3 increment(UINTVECTOR3 v, const UINTVECTOR3 base) {
  do {
    v[2]++;
    if(v[2] == base[2]) {
      v[2] = 0;
      v[1]++;
      if(v[1] == base[1]) {
        v[1] = 0;
        v[0]++;
        if(v[0] >= base[0]) {
          throw std::overflow_error("no brick space left");
        }
      }
    }
  } while(v[0] < base[0]);
  return v;
}

UINTVECTOR3 GLVolumePool::FindNextPoolPosition() const {
  // our maximum *logical* index.
  const UINTVECTOR3 logMax = m_poolSize / m_brickSize;

  // now we simply perform 'base logMax' arithmetic on our logical index until
  // we either overflow (i.e. we're out of space) or we find a good index.
  UINTVECTOR3 idx;
  try {
    idx = increment(m_allocPos, logMax);
  } catch(const std::overflow_error&) {
    // we ran out of space.. what should we replace?
    // whatever's at the top of the queue.
    const VolumePoolElemInfo& vei = m_BricksInPool.front();
    m_BricksInPool.front();
    return UINTVECTOR3(vei.m_vBrickID.x, vei.m_vBrickID.y, vei.m_vBrickID.z);
  }
  return idx;
}

void GLVolumePool::UpdateMetadataTexture() {
  // TODO
}

void GLVolumePool::FreeGLResources() {
  delete m_PoolMetadataTexture;
  delete m_PoolDataTexture;
}

uint64_t GLVolumePool::GetCPUSize() const {
  return 0;
//  return m_iSizePerElement * m_poolTexSize.volume() +
//         m_metaTexSize.area(); // TODO: * metatex elemen size
}

uint64_t GLVolumePool::GetGPUSize() const {
  return GetCPUSize();
}
