/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2008 Scientific Computing and Imaging Institute,
   University of Utah.


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

/**
  \file    GPUMemManDataStructs.cpp
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    August 2008
*/

#include <cstring>
#include <new>
#include <typeinfo>
#include <IO/IOManager.h>
#include "GPUMemManDataStructs.h"
#include "Basics/MathTools.h"
#include "Controller/Controller.h"
#include "IO/uvfDataset.h"
#include "Renderer/GL/GLTexture3D.h"
#include "Renderer/GL/GLTexture2D.h"
#include "Renderer/GL/GLVolume3DTex.h"
#include "Renderer/GL/GLVolume2DTex.h"

using namespace tuvok;


GLVolumeListElem::GLVolumeListElem(Dataset* _pDataset, const BrickKey& key,
                                     bool bIsPaddedToPowerOfTwo,
                                     bool bIsDownsampledTo8Bits,
                                     bool bDisableBorder,
                                     bool bEmulate3DWith2DStacks, 
                                     UINT64 iIntraFrameCounter,
                                     UINT64 iFrameCounter,
                                     MasterController* pMasterController,
                                     const CTContext &ctx,
                                     std::vector<unsigned char>& vUploadHub) :
  pGLVolume(NULL),
  pDataset(_pDataset),
  iUserCount(1),
  m_iIntraFrameCounter(iIntraFrameCounter),
  m_iFrameCounter(iFrameCounter),
  m_pMasterController(pMasterController),
  m_Context(ctx),
  m_Key(key),
  m_bIsPaddedToPowerOfTwo(bIsPaddedToPowerOfTwo),
  m_bIsDownsampledTo8Bits(bIsDownsampledTo8Bits),
  m_bDisableBorder(bDisableBorder),
  m_bEmulate3DWith2DStacks(bEmulate3DWith2DStacks),
  m_bUsingHub(false)
{
  if (!CreateTexture(vUploadHub) && pGLVolume) {
    FreeTexture();
  }
}

GLVolumeListElem::~GLVolumeListElem() {
  FreeData();
  FreeTexture();
}

bool GLVolumeListElem::Equals(const Dataset* _pDataset, const BrickKey& key,
                               bool bIsPaddedToPowerOfTwo,
                               bool bIsDownsampledTo8Bits, bool bDisableBorder,
                               bool bEmulate3DWith2DStacks, 
                               const CTContext &cid)
{
  if (_pDataset != pDataset ||
      m_Key != key ||
      m_bIsPaddedToPowerOfTwo != bIsPaddedToPowerOfTwo ||
      m_bIsDownsampledTo8Bits != bIsDownsampledTo8Bits ||
      m_bDisableBorder != bDisableBorder ||
      m_bEmulate3DWith2DStacks != bEmulate3DWith2DStacks ||
      m_Context != cid) {
    return false;
  }

  return true;
}

GLVolume* GLVolumeListElem::Access(UINT64& iIntraFrameCounter, UINT64& iFrameCounter) {
  m_iIntraFrameCounter = iIntraFrameCounter;
  m_iFrameCounter = iFrameCounter;
  iUserCount++;

  return pGLVolume;
}

bool GLVolumeListElem::BestMatch(const UINTVECTOR3& vDimension,
                                  bool bIsPaddedToPowerOfTwo,
                                  bool bIsDownsampledTo8Bits,
                                  bool bDisableBorder,
                                  bool bEmulate3DWith2DStacks, 
                                  UINT64& iIntraFrameCounter,
                                  UINT64& iFrameCounter,
                                  const CTContext &cid)
{
  if (!Match(vDimension) || iUserCount > 0
      || m_bIsPaddedToPowerOfTwo != bIsPaddedToPowerOfTwo
      || m_bIsDownsampledTo8Bits != bIsDownsampledTo8Bits
      || m_bDisableBorder != bDisableBorder
      || m_bEmulate3DWith2DStacks != bEmulate3DWith2DStacks
      || m_Context != cid) {
    return false;
  }

  // framewise older data as before found -> use this object
  if (iFrameCounter > m_iFrameCounter) {
      iFrameCounter = m_iFrameCounter;
      iIntraFrameCounter = m_iIntraFrameCounter;

      return true;
  }

  // framewise older data as before found -> use this object
  if (iFrameCounter == m_iFrameCounter &&
      iIntraFrameCounter < m_iIntraFrameCounter) {

      iFrameCounter = m_iFrameCounter;
      iIntraFrameCounter = m_iIntraFrameCounter;

      return true;
  }

  return false;
}



bool GLVolumeListElem::Match(const UINTVECTOR3& vDimension) {
  if (pGLVolume == NULL) return false;

  const UINTVECTOR3 vSize = pDataset->GetBrickVoxelCounts(m_Key);

  for (size_t i=0; i < 3; i++) {
    if (vSize[i] != vDimension[i]) {
      return false;
    }
  }

  return true;
}

bool GLVolumeListElem::Replace(Dataset* _pDataset,
                                const BrickKey& key,
                                bool bIsPaddedToPowerOfTwo,
                                bool bIsDownsampledTo8Bits,
                                bool bDisableBorder,
                                bool bEmulate3DWith2DStacks, 
                                UINT64 iIntraFrameCounter,
                                UINT64 iFrameCounter, const CTContext &cid,
                                std::vector<unsigned char>& vUploadHub) {
  if (pGLVolume == NULL) return false;
  if (m_Context != cid) {
    T_ERROR("Trying to replace texture in one context"
            "with a texture from a second context!");
    return false;
  }

  pDataset = _pDataset;
  m_Key    = key;
  m_bIsPaddedToPowerOfTwo  = bIsPaddedToPowerOfTwo;
  m_bIsDownsampledTo8Bits  = bIsDownsampledTo8Bits;
  m_bDisableBorder         = bDisableBorder;
  m_bEmulate3DWith2DStacks = bEmulate3DWith2DStacks;

  m_iIntraFrameCounter = iIntraFrameCounter;
  m_iFrameCounter = iFrameCounter;

  if (!LoadData(vUploadHub)) {
    T_ERROR("LoadData call failed, system may be out of memory");
    return false;
  }
  while (glGetError() != GL_NO_ERROR) {};  // clear gl error flags
  
  pGLVolume->SetData(m_bUsingHub ? &vUploadHub.at(0) : &vData.at(0));

  return GL_NO_ERROR==glGetError();
}


bool GLVolumeListElem::LoadData(std::vector<unsigned char>& vUploadHub) {
  const UINTVECTOR3 vSize = pDataset->GetBrickVoxelCounts(m_Key);
  UINT64 iByteWidth  = pDataset->GetBitWidth()/8;
  UINT64 iCompCount = pDataset->GetComponentCount();

  UINT64 iBrickSize = vSize[0]*vSize[1]*vSize[2]*iByteWidth * iCompCount;

  if (!vUploadHub.empty() && iBrickSize <= UINT64(m_pMasterController->IOMan()->GetIncoresize()*4)) {
    m_bUsingHub = true;
    return pDataset->GetBrick(m_Key, vUploadHub);
  } else {
    return pDataset->GetBrick(m_Key, vData);
  }
}

void  GLVolumeListElem::FreeData() {
  vData.resize(0);
}


bool GLVolumeListElem::CreateTexture(std::vector<unsigned char>& vUploadHub,
                                      bool bDeleteOldTexture) {
  if (bDeleteOldTexture) FreeTexture();

  if (vData.empty()) {
    if (!LoadData(vUploadHub)) { return false; }
  }

  unsigned char* pRawData = (m_bUsingHub) ? &vUploadHub.at(0) : &vData.at(0);

  // Figure out how big this is going to be.
  const UINTVECTOR3 vSize = pDataset->GetBrickVoxelCounts(m_Key);

  bool bToggleEndian = !pDataset->IsSameEndianness();
  UINT64 iBitWidth  = pDataset->GetBitWidth();
  UINT64 iCompCount = pDataset->GetComponentCount();

  MESSAGE("%llu components of width %llu", iCompCount, iBitWidth);

  GLint glInternalformat;
  GLenum glFormat;
  GLenum glType;

  if (m_bIsDownsampledTo8Bits && iBitWidth != 8) {
    // here we assume that data which is not 8 bit is 16 bit
    if (iBitWidth != 16) {
      FreeData();
      return false;
    }

    double fMin = pDataset->GetRange().first;
    double fMax = pDataset->GetRange().second;

    for (size_t i = 0;i<vSize[0]*vSize[1]*vSize[2]*iCompCount;i++) {
      unsigned char iQuantizedVal = (unsigned char)((255.0*((unsigned short*)pRawData)[i] - fMin) / (fMax-fMin));
      pRawData[i] = iQuantizedVal;
    }

    iBitWidth = 8;
  }


  switch (iCompCount) {
    case 1 : glFormat = GL_LUMINANCE; break;
    case 3 : glFormat = GL_RGB; break;
    case 4 : glFormat = GL_RGBA; break;
    default : FreeData(); return false;
  }

  if (iBitWidth == 8) {
      glType = GL_UNSIGNED_BYTE;
      switch (iCompCount) {
        case 1 : glInternalformat = GL_LUMINANCE8; break;
        case 3 : glInternalformat = GL_RGB8; break;
        case 4 : glInternalformat = GL_RGBA8; break;
        default : FreeData(); return false;
      }
  } else {
    if (iBitWidth == 16) {
      glType = GL_UNSIGNED_SHORT;

      if (bToggleEndian) {
        /// @todo BROKEN for N-dimensional data; we're assuming we only get 3D
        /// data here.
        UINT64 iElemCount = vSize[0] * vSize[1] * vSize[2];
        short* pShorData = (short*)pRawData;
        for (UINT64 i = 0;i<iCompCount*iElemCount;i++) {
          EndianConvert::Swap<short>(pShorData+i);
        }
      }

      switch (iCompCount) {
        case 1 : glInternalformat = GL_LUMINANCE16; break;
        case 3 : glInternalformat = GL_RGB16; break;
        case 4 : glInternalformat = GL_RGBA16; break;
        default : FreeData(); return false;
      }
    } else {
      if(iBitWidth == 32) {
        glType = GL_FLOAT;
        glInternalformat = GL_LUMINANCE32F_ARB;
        glFormat = GL_LUMINANCE;
      } else {
        T_ERROR("Cannot handle data of width %d", iBitWidth);
        FreeData();
        return false;
      }
    }
  }

  glGetError();
  if (!m_bIsPaddedToPowerOfTwo ||
      (MathTools::IsPow2(UINT32(vSize[0])) &&
       MathTools::IsPow2(UINT32(vSize[1])) &&
       MathTools::IsPow2(UINT32(vSize[2])))) {

    if (m_bEmulate3DWith2DStacks) {
      pGLVolume = new GLVolume2DTex(UINT32(vSize[0]), UINT32(vSize[1]),
                                UINT32(vSize[2]),
                                glInternalformat, glFormat, glType,
                                UINT32(iBitWidth/8*iCompCount), pRawData,
                                GL_LINEAR, GL_LINEAR,
                                m_bDisableBorder ? GL_CLAMP_TO_EDGE : GL_CLAMP,
                                m_bDisableBorder ? GL_CLAMP_TO_EDGE : GL_CLAMP,
                                m_bDisableBorder ? GL_CLAMP_TO_EDGE : GL_CLAMP);
    } else {
      pGLVolume = new GLVolume3DTex(UINT32(vSize[0]), UINT32(vSize[1]),
                                UINT32(vSize[2]),
                                glInternalformat, glFormat, glType,
                                UINT32(iBitWidth/8*iCompCount), pRawData,
                                GL_LINEAR, GL_LINEAR,
                                m_bDisableBorder ? GL_CLAMP_TO_EDGE : GL_CLAMP,
                                m_bDisableBorder ? GL_CLAMP_TO_EDGE : GL_CLAMP,
                                m_bDisableBorder ? GL_CLAMP_TO_EDGE : GL_CLAMP);
    }
  } else {
    // pad the data to a power of two
    UINTVECTOR3 vPaddedSize(MathTools::NextPow2(UINT32(vSize[0])), MathTools::NextPow2(UINT32(vSize[1])), MathTools::NextPow2(UINT32(vSize[2])));

    size_t iTarget = 0;
    size_t iSource = 0;
    size_t iElementSize = iBitWidth/8*iCompCount;
    size_t iRowSizeSource = vSize[0]*iElementSize;
    size_t iRowSizeTarget = vPaddedSize[0]*iElementSize;

    unsigned char* pPaddedData;
    try {
      pPaddedData = new unsigned char[iRowSizeTarget *
                                      vPaddedSize[1] *
                                      vPaddedSize[2]];
    } catch(std::bad_alloc&) {
      return false;
    }
    memset(pPaddedData, 0, iRowSizeTarget*vPaddedSize[1]*vPaddedSize[2]);

    for (size_t z = 0;z<vSize[2];z++) {
      for (size_t y = 0;y<vSize[1];y++) {
        memcpy(pPaddedData+iTarget, pRawData+iSource, iRowSizeSource);

        // if the x sizes differ, duplicate the last element to make the
        // texture behave like clamp
        if (!m_bDisableBorder && iRowSizeTarget > iRowSizeSource)
          memcpy(pPaddedData+iTarget+iRowSizeSource,
                 pPaddedData+iTarget+iRowSizeSource-iElementSize,
                 iElementSize);
        iTarget += iRowSizeTarget;
        iSource += iRowSizeSource;
      }
      // if the y sizes differ, duplicate the last element to make the texture
      // behave like clamp
      if (vPaddedSize[1] > vSize[1]) {
        if (!m_bDisableBorder)
          memcpy(pPaddedData+iTarget, pPaddedData+iTarget-iRowSizeTarget, iRowSizeTarget);
        iTarget += (vPaddedSize[1]-vSize[1])*iRowSizeTarget;
      }
    }
    // if the z sizes differ, duplicate the last element to make the texture
    // behave like clamp
    if (!m_bDisableBorder && vPaddedSize[2] > vSize[2]) {
      memcpy(pPaddedData+iTarget, pPaddedData+(iTarget-vPaddedSize[1]*iRowSizeTarget), vPaddedSize[1]*iRowSizeTarget);
    }

    MESSAGE("Actually creating new texture %u x %u x %u, bitsize=%llu, "
            "componentcount=%llu due to compatibility settings",
            vPaddedSize[0], vPaddedSize[1], vPaddedSize[2],
            iBitWidth, iCompCount);

    if (m_bEmulate3DWith2DStacks) {
      // TODO
    } else {
      pGLVolume = new GLVolume3DTex(vPaddedSize[0], vPaddedSize[1], vPaddedSize[2],
                                 glInternalformat, glFormat, glType,
                                 UINT32(iBitWidth/8*iCompCount), pPaddedData,
                                 GL_LINEAR, GL_LINEAR,
                                 m_bDisableBorder ? GL_CLAMP_TO_EDGE : GL_CLAMP,
                                 m_bDisableBorder ? GL_CLAMP_TO_EDGE : GL_CLAMP,
                                 m_bDisableBorder ? GL_CLAMP_TO_EDGE : GL_CLAMP);
    }
    delete [] pPaddedData;
  }

  FreeData();
  return GL_NO_ERROR==glGetError();
}

void GLVolumeListElem::FreeTexture() {
  if (pGLVolume) {
    pGLVolume->FreeGLResources();
    delete pGLVolume;
    pGLVolume = NULL;
  }

}
