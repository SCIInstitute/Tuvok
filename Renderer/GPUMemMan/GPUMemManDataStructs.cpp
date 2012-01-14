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

#ifdef _MSC_VER
# include <functional>
#else
# include <tr1/functional>
#endif
#include <cstring>
#include <new>
#include <numeric>
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
using namespace std::tr1;

GLVolumeListElem::GLVolumeListElem(Dataset* _pDataset, const BrickKey& key,
                                   bool bIsPaddedToPowerOfTwo,
                                   bool bIsDownsampledTo8Bits,
                                   bool bDisableBorder,
                                   bool bEmulate3DWith2DStacks,
                                   uint64_t iIntraFrameCounter,
                                   uint64_t iFrameCounter,
                                   MasterController* pMasterController,
                                   std::vector<unsigned char>& vUploadHub,
                                   int iShareGroupID) :
  pDataset(_pDataset),
  iUserCount(1),
  m_iIntraFrameCounter(iIntraFrameCounter),
  m_iFrameCounter(iFrameCounter),
  m_pMasterController(pMasterController),
  m_Key(key),
  m_bIsPaddedToPowerOfTwo(bIsPaddedToPowerOfTwo),
  m_bIsDownsampledTo8Bits(bIsDownsampledTo8Bits),
  m_bDisableBorder(bDisableBorder),
  m_bEmulate3DWith2DStacks(bEmulate3DWith2DStacks),
  m_bUsingHub(false),
  m_iShareGroupID(iShareGroupID)
{
  // initialize the volumes to be null pointers.
  volume = NULL;
  if (!CreateTexture(vUploadHub) && volume) {
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
                              int iShareGroupID)
{
  if (_pDataset != pDataset ||
      m_Key != key ||
      m_bIsPaddedToPowerOfTwo != bIsPaddedToPowerOfTwo ||
      m_bIsDownsampledTo8Bits != bIsDownsampledTo8Bits ||
      m_bDisableBorder != bDisableBorder ||
      m_bEmulate3DWith2DStacks != bEmulate3DWith2DStacks ||
      m_iShareGroupID != iShareGroupID) {
    return false;
  }

  return true;
}

GLVolume* GLVolumeListElem::Access(uint64_t& iIntraFrameCounter, uint64_t& iFrameCounter) {
  m_iIntraFrameCounter = iIntraFrameCounter;
  m_iFrameCounter = iFrameCounter;
  iUserCount++;

  return volume;
}

bool GLVolumeListElem::BestMatch(const UINTVECTOR3& vDimension,
                                 bool bIsPaddedToPowerOfTwo,
                                 bool bIsDownsampledTo8Bits,
                                 bool bDisableBorder,
                                 bool bEmulate3DWith2DStacks,
                                 uint64_t& iIntraFrameCounter,
                                 uint64_t& iFrameCounter,
                                 int iShareGroupID)
{
  if (!Match(vDimension) || iUserCount > 0
      || m_bIsPaddedToPowerOfTwo != bIsPaddedToPowerOfTwo
      || m_bIsDownsampledTo8Bits != bIsDownsampledTo8Bits
      || m_bDisableBorder != bDisableBorder
      || m_bEmulate3DWith2DStacks != bEmulate3DWith2DStacks
      || m_iShareGroupID != iShareGroupID) {
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

namespace nonstd {
  // an accumulate which follows the standard accumulate, except instead of:
  //    result = result + *i
  // at each iteration, it performs:
  //    result = result + f(*i)
  // i.e. it applies a unary op to the item iterated over.  How is this not
  // part of the standard already??
  template<class InputIter, typename T, class UnaryFunc>
  T accumulate(InputIter first, InputIter last, T init, UnaryFunc uop) {
    T rv = init;
    for(; first != last; ++first) {
      rv = rv + T(uop(*first));
    }
    return rv;
  }
}

size_t GLVolumeListElem::GetGPUSize() const
{
  return size_t(volume->GetGPUSize());
/*
  // Apparently MSVC fixed this in 2010, but 2008 is broken.
#if defined(_MSC_VER) && _MSC_VER <= 1500
  size_t sz=0;
  for(size_t i=0; i < volumes.size(); ++i) {
    sz += size_t(volumes[i]->GetGPUSize());
  }
  return sz;
#else
  return static_cast<size_t>(nonstd::accumulate(
    this->volumes.begin(), this->volumes.end(), 0,
    std::tr1::mem_fn(&GLVolume::GetGPUSize))
  );
#endif
  */
}
size_t GLVolumeListElem::GetCPUSize() const
{
  return size_t(volume->GetCPUSize());
  /*

  #if defined(_MSC_VER) && _MSC_VER <= 1500
  size_t sz=0;
  for(size_t i=0; i < volumes.size(); ++i) {
    sz += size_t(volumes[i]->GetGPUSize());
  }
  return sz;
#else
  return static_cast<size_t>(nonstd::accumulate( 
    this->volumes.begin(), this->volumes.end(), 0,
    std::tr1::mem_fn(&GLVolume::GetCPUSize))
  );
#endif
  */
}



bool GLVolumeListElem::Match(const UINTVECTOR3& vDimension) {
  if(!volume) { return false; }

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
                               uint64_t iIntraFrameCounter,
                               uint64_t iFrameCounter,
                               std::vector<unsigned char>& vUploadHub,
                               int iShareGroupID) {
  
  if(!volume) { return false; }

  pDataset = _pDataset;
  m_Key    = key;
  m_bIsPaddedToPowerOfTwo  = bIsPaddedToPowerOfTwo;
  m_bIsDownsampledTo8Bits  = bIsDownsampledTo8Bits;
  m_bDisableBorder         = bDisableBorder;
  m_bEmulate3DWith2DStacks = bEmulate3DWith2DStacks;
  assert(m_iShareGroupID == iShareGroupID);
#ifdef NDEBUG
  // avoid 'unused variable' warning.
  (void)iShareGroupID;
#endif

  m_iIntraFrameCounter = iIntraFrameCounter;
  m_iFrameCounter = iFrameCounter;

  if (!LoadData(vUploadHub)) {
    T_ERROR("LoadData call failed, system may be out of memory");
    return false;
  }
  while (glGetError() != GL_NO_ERROR) {};  // clear gl error flags

  const UINTVECTOR3 vSize = pDataset->GetBrickVoxelCounts(m_Key);

  if (!m_bIsPaddedToPowerOfTwo ||
      (MathTools::IsPow2(uint32_t(vSize[0])) &&
       MathTools::IsPow2(uint32_t(vSize[1])) &&
       MathTools::IsPow2(uint32_t(vSize[2])))) {
    volume->SetData(m_bUsingHub ? &vUploadHub.at(0) : &vData.at(0));
  } else {
    std::pair<shared_ptr<unsigned char>, UINTVECTOR3> padded = PadData(
      m_bUsingHub ? &vUploadHub.at(0) : &vData.at(0),
      pDataset->GetBrickVoxelCounts(m_Key),
      pDataset->GetBitWidth(),
      pDataset->GetComponentCount()
    );

    volume->SetData(padded.first.get());
  }

  return GL_NO_ERROR==glGetError();
}


bool GLVolumeListElem::LoadData(std::vector<unsigned char>& vUploadHub) {
  const UINTVECTOR3 vSize = pDataset->GetBrickVoxelCounts(m_Key);
  uint64_t iByteWidth  = pDataset->GetBitWidth()/8;
  uint64_t iCompCount = pDataset->GetComponentCount();

  uint64_t iBrickSize = vSize[0]*vSize[1]*vSize[2]*iByteWidth * iCompCount;

  if (!vUploadHub.empty() && iBrickSize <=
      uint64_t(m_pMasterController->IOMan()->GetIncoresize()*4)) {
    m_bUsingHub = true;
    return pDataset->GetBrick(m_Key, vUploadHub);
  } else {
    return pDataset->GetBrick(m_Key, vData);
  }
}

void  GLVolumeListElem::FreeData() {
  vData.resize(0);
}

static void DeleteArray(unsigned char* p) { delete[] p; }

std::pair<shared_ptr<unsigned char>, UINTVECTOR3>
GLVolumeListElem::PadData(unsigned char* pRawData, UINTVECTOR3 vSize, uint64_t iBitWidth,
                          uint64_t iCompCount)
{
  // pad the data to a power of two
  UINTVECTOR3 vPaddedSize(MathTools::NextPow2(uint32_t(vSize[0])),
                          MathTools::NextPow2(uint32_t(vSize[1])),
                          MathTools::NextPow2(uint32_t(vSize[2])));
  size_t iTarget = 0;
  size_t iSource = 0;
  size_t iElementSize = static_cast<size_t>(iBitWidth/8*iCompCount);
  size_t iRowSizeSource = vSize[0]*iElementSize;
  size_t iRowSizeTarget = vPaddedSize[0]*iElementSize;

  shared_ptr<unsigned char> pPaddedData;
  try {
    pPaddedData = shared_ptr<unsigned char>(
      new unsigned char[iRowSizeTarget * vPaddedSize[1] * vPaddedSize[2]],
      DeleteArray
    );
  } catch(std::bad_alloc&) {
    return std::make_pair(shared_ptr<unsigned char>(), vPaddedSize);
  }
  memset(pPaddedData.get(), 0, iRowSizeTarget*vPaddedSize[1]*vPaddedSize[2]);

  for (size_t z = 0;z<vSize[2];z++) {
    for (size_t y = 0;y<vSize[1];y++) {
      memcpy(pPaddedData.get()+iTarget, pRawData+iSource, iRowSizeSource);

      // if the x sizes differ, duplicate the last element to make the
      // texture behave like clamp
      if (!m_bDisableBorder && iRowSizeTarget > iRowSizeSource)
        memcpy(pPaddedData.get()+iTarget+iRowSizeSource,
               pPaddedData.get()+iTarget+iRowSizeSource-iElementSize,
               iElementSize);
      iTarget += iRowSizeTarget;
      iSource += iRowSizeSource;
    }
    // if the y sizes differ, duplicate the last element to make the texture
    // behave like clamp
    if (vPaddedSize[1] > vSize[1]) {
      if (!m_bDisableBorder)
        memcpy(pPaddedData.get()+iTarget, pPaddedData.get()+iTarget-iRowSizeTarget,
               iRowSizeTarget);
      iTarget += (vPaddedSize[1]-vSize[1])*iRowSizeTarget;
    }
  }

  // if the z sizes differ, duplicate the last element to make the texture
  // behave like clamp
  if (!m_bDisableBorder && vPaddedSize[2] > vSize[2]) {
    memcpy(pPaddedData.get()+iTarget,
           pPaddedData.get()+(iTarget-vPaddedSize[1]*iRowSizeTarget),
           vPaddedSize[1]*iRowSizeTarget);
  }

  MESSAGE("Actually using new texture %u x %u x %u, bitsize=%llu, "
          "componentcount=%llu due to compatibility settings",
          vPaddedSize[0], vPaddedSize[1], vPaddedSize[2],
          iBitWidth, iCompCount);

  return std::make_pair(pPaddedData, vPaddedSize);
}

bool GLVolumeListElem::CreateTexture(std::vector<unsigned char>& vUploadHub,
                                     bool bDeleteOldTexture) {
  if (bDeleteOldTexture) FreeTexture();

  if (vData.empty()) {
    MESSAGE("Completely reloading brick");
    if (!LoadData(vUploadHub)) { return false; }
  } else {
    MESSAGE("Reusing CPU copy of brick data");
  }

  unsigned char* pRawData = (m_bUsingHub) ? &vUploadHub.at(0) : &vData.at(0);

  // Figure out how big this is going to be.
  const UINTVECTOR3 vSize = pDataset->GetBrickVoxelCounts(m_Key);

  bool bToggleEndian = !pDataset->IsSameEndianness();
  uint64_t iBitWidth  = pDataset->GetBitWidth();
  uint64_t iCompCount = pDataset->GetComponentCount();

  MESSAGE("%llu components of width %llu", iCompCount, iBitWidth);

  GLint glInternalformat;
  GLenum glFormat;
  GLenum glType;

  if (m_bIsDownsampledTo8Bits && iBitWidth != 8) {
    // here we assume that data which is not 8 bit is 16 bit
    if (iBitWidth != 16) {
      T_ERROR("Don't know how to handle %llu-bit data.", iBitWidth);
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
        uint64_t iElemCount = vSize[0] * vSize[1] * vSize[2];
        short* pShorData = (short*)pRawData;
        std::transform(pShorData, pShorData+(iCompCount*iElemCount), pShorData,
                       EndianConvert::Swap<short>);
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
      (MathTools::IsPow2(uint32_t(vSize[0])) &&
       MathTools::IsPow2(uint32_t(vSize[1])) &&
       MathTools::IsPow2(uint32_t(vSize[2])))) {
    GLenum clamp = m_bDisableBorder ? GL_CLAMP_TO_EDGE : GL_CLAMP;

    if (m_bEmulate3DWith2DStacks) {
      volume = new GLVolume2DTex(uint32_t(vSize[0]), uint32_t(vSize[1]),
                                     uint32_t(vSize[2]),
                                     glInternalformat, glFormat, glType,
                                     uint32_t(iBitWidth/8*iCompCount), pRawData,
                                     GL_LINEAR, GL_LINEAR,
                                     clamp, clamp, clamp);
    } else {
      volume = new GLVolume3DTex(uint32_t(vSize[0]), uint32_t(vSize[1]),
                                     uint32_t(vSize[2]),
                                     glInternalformat, glFormat, glType,
                                     uint32_t(iBitWidth/8*iCompCount), pRawData,
                                     GL_LINEAR, GL_LINEAR,
                                     clamp, clamp, clamp);
    }
  } else {
    std::pair<shared_ptr<unsigned char>, UINTVECTOR3> padded =
      PadData(pRawData, vSize, iBitWidth, iCompCount);
    const shared_ptr<unsigned char>& pPaddedData = padded.first;
    const UINTVECTOR3& vPaddedSize = padded.second;

    if (m_bEmulate3DWith2DStacks) {
      volume = new GLVolume2DTex(vPaddedSize[0], vPaddedSize[1], vPaddedSize[2],
                                     glInternalformat, glFormat, glType,
                                     uint32_t(iBitWidth/8*iCompCount),
                                     pPaddedData.get(),
                                     GL_LINEAR, GL_LINEAR,
                                     m_bDisableBorder ? GL_CLAMP_TO_EDGE : GL_CLAMP,
                                     m_bDisableBorder ? GL_CLAMP_TO_EDGE : GL_CLAMP,
                                     m_bDisableBorder ? GL_CLAMP_TO_EDGE : GL_CLAMP);
    } else {
      volume = new GLVolume3DTex(vPaddedSize[0], vPaddedSize[1], vPaddedSize[2],
                                     glInternalformat, glFormat, glType,
                                     uint32_t(iBitWidth/8*iCompCount),
                                     pPaddedData.get(),
                                     GL_LINEAR, GL_LINEAR,
                                     m_bDisableBorder ? GL_CLAMP_TO_EDGE : GL_CLAMP,
                                     m_bDisableBorder ? GL_CLAMP_TO_EDGE : GL_CLAMP,
                                     m_bDisableBorder ? GL_CLAMP_TO_EDGE : GL_CLAMP);
    }
  }

  // In the OpenGL case we can release the data at this point as we
  // let the OpenGL subsystem handle the CPU/GPU paging, i.e. we ignore
  // the GPU usage
  FreeData();
  return GL_NO_ERROR==glGetError();
}

void GLVolumeListElem::FreeTexture() {
  if (volume) {
    delete volume;
    volume = NULL;
  }
}
