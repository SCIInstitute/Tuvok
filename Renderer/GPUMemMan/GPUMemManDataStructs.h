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
  \file    GPUMemManDataStructs.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    August 2008
*/

#pragma once

#ifndef GPUMEMMANDATASTRUCTS_H
#define GPUMEMMANDATASTRUCTS_H

#include <deque>
#include <string>
#include <vector>
#include "GL/glew.h"
#include "boost/noncopyable.hpp"
#include "Basics/Vectors.h"
#include "IO/Brick.h"
#include "../Context.h"
#include "../../StdTuvokDefines.h"
#include "../GL/GLFBOTex.h"
#include "../GL/GLSLProgram.h"

class TransferFunction1D;
class TransferFunction2D;
class VolumeDataset;
namespace tuvok {
  class Dataset;
  class AbstrRenderer;
  class GLTexture1D;
  class GLTexture2D;
  class GLTexture3D;
  class GLVolume;

  typedef std::deque< AbstrRenderer* > AbstrRendererList;
  typedef AbstrRendererList::iterator AbstrRendererListIter;

  // volume datasets
  class VolDataListElem {
  public:
    VolDataListElem(Dataset* _pVolumeDataset, AbstrRenderer* pUser) :
      pVolumeDataset(_pVolumeDataset)
    {
      qpUser.push_back(pUser);
    }

    Dataset* pVolumeDataset;
    AbstrRendererList qpUser;
  };
  typedef std::deque<VolDataListElem> VolDataList;
  typedef VolDataList::iterator VolDataListIter;

  // simple textures
  class SimpleTextureListElem {
  public:
    SimpleTextureListElem(UINT32 _iAccessCounter, GLTexture2D* _pTexture, std::string _strFilename) :
      iAccessCounter(_iAccessCounter),
      pTexture(_pTexture),
      strFilename(_strFilename)
    {}

    UINT32        iAccessCounter;
    GLTexture2D*  pTexture;
    std::string   strFilename;
  };
  typedef std::deque<SimpleTextureListElem> SimpleTextureList;
  typedef SimpleTextureList::iterator SimpleTextureListIter;

  // 1D transfer functions
  class Trans1DListElem {
  public:
    Trans1DListElem(TransferFunction1D* _pTransferFunction1D, GLTexture1D* _pTexture, AbstrRenderer* pUser) :
      pTransferFunction1D(_pTransferFunction1D),
      pTexture(_pTexture)
    {
      qpUser.push_back(pUser);
    }

    TransferFunction1D*  pTransferFunction1D;
    GLTexture1D*    pTexture;
    AbstrRendererList  qpUser;
  };
  typedef std::deque<Trans1DListElem> Trans1DList;
  typedef Trans1DList::iterator Trans1DListIter;

  // 2D transfer functions
  class Trans2DListElem {
  public:
    Trans2DListElem(TransferFunction2D* _pTransferFunction2D, GLTexture2D* _pTexture, AbstrRenderer* pUser) :
      pTransferFunction2D(_pTransferFunction2D),
      pTexture(_pTexture)
    {
      qpUser.push_back(pUser);
    }

    TransferFunction2D*  pTransferFunction2D;
    GLTexture2D*    pTexture;
    AbstrRendererList  qpUser;
  };
  typedef std::deque<Trans2DListElem> Trans2DList;
  typedef Trans2DList::iterator Trans2DListIter;

  // 3D textures
  /// For equivalent contexts, it might actually be valid to copy a 3D texture
  /// object.  However, for one, this is untested.  Secondly, this object may
  /// hold the chunk of data for the 3D texture, so copying it in the general
  /// case would be a bad idea -- the copy might be large.
  class GLVolumeListElem : boost::noncopyable {
  public:
    GLVolumeListElem(Dataset* _pDataset, const BrickKey&,
                      bool bIsPaddedToPowerOfTwo, bool bDisableBorder,
                      bool bIsDownsampledTo8Bits, bool bEmulate3DWith2DStacks,
                      UINT64 iIntraFrameCounter,
                      UINT64 iFrameCounter, MasterController* pMasterController,
                      const CTContext &,
                      std::vector<unsigned char>& vUploadHub);
    ~GLVolumeListElem();

    bool Equals(const Dataset* _pDataset, const BrickKey&,
                bool bIsPaddedToPowerOfTwo, bool bIsDownsampledTo8Bits,
                bool bDisableBorder, bool bEmulate3DWith2DStacks,
                const CTContext &);
    bool Replace(Dataset* _pDataset, const BrickKey&,
                 bool bIsPaddedToPowerOfTwo, bool bIsDownsampledTo8Bits,
                 bool bDisableBorder, bool bEmulate3DWith2DStacks,
                 UINT64 iIntraFrameCounter, UINT64 iFrameCounter,
                 const CTContext &,
                 std::vector<unsigned char>& vUploadHub);
    bool BestMatch(const UINTVECTOR3& vDimension,
                   bool bIsPaddedToPowerOfTwo, bool bIsDownsampledTo8Bits,
                   bool bDisableBorder, bool bEmulate3DWith2DStacks,
                   UINT64& iIntraFrameCounter,
                   UINT64& iFrameCounter, const CTContext &);
    void GetCounters(UINT64& iIntraFrameCounter, UINT64& iFrameCounter) {
      iIntraFrameCounter = m_iIntraFrameCounter;
      iFrameCounter = m_iFrameCounter;
    }

    GLVolume* Access(UINT64& iIntraFrameCounter, UINT64& iFrameCounter);

    bool LoadData(std::vector<unsigned char>& vUploadHub);
    void  FreeData();
    bool CreateTexture(std::vector<unsigned char>& vUploadHub,
                       bool bDeleteOldTexture=true);
    void  FreeTexture();

    std::vector<unsigned char> vData;
    GLVolume*                  pGLVolume;
    Dataset*                   pDataset;
    UINT32                     iUserCount;

    UINT64 GetIntraFrameCounter() const {return m_iIntraFrameCounter;}
    UINT64 GetFrameCounter() const {return m_iFrameCounter;}

  private:
    bool Match(const UINTVECTOR3& vDimension);

    UINT64 m_iIntraFrameCounter;
    UINT64 m_iFrameCounter;
    MasterController* m_pMasterController;
    const CTContext m_Context;

    BrickKey m_Key;
    bool m_bIsPaddedToPowerOfTwo;
    bool m_bIsDownsampledTo8Bits;
    bool m_bDisableBorder;
    bool m_bEmulate3DWith2DStacks;
    bool m_bUsingHub;
  };

  typedef std::deque<GLVolumeListElem*> GLVolumeList;
  typedef GLVolumeList::iterator GLVolumeListIter;
  typedef GLVolumeList::const_iterator GLVolumeListConstIter;

  // framebuffer objects
  class FBOListElem {
  public:
    FBOListElem(GLFBOTex* _pFBOTex) : pFBOTex(_pFBOTex)
    {}
    FBOListElem(MasterController* pMasterController, GLenum minfilter, GLenum magfilter, GLenum wrapmode, GLsizei width, GLsizei height, GLenum intformat, UINT32 iSizePerElement, bool bHaveDepth, int iNumBuffers) :
      pFBOTex(new GLFBOTex(pMasterController, minfilter, magfilter, wrapmode, width, height, intformat, iSizePerElement, bHaveDepth, iNumBuffers))
    {}

    ~FBOListElem()
    {
      delete pFBOTex;
    }

    GLFBOTex* pFBOTex;
  };
  typedef std::deque<FBOListElem*> FBOList;
  typedef FBOList::iterator FBOListIter;


  // framebuffer objects
  class GLSLListElem {
  public:
    GLSLListElem(MasterController* mc,
                 const std::vector<std::string>& vert,
                 const std::vector<std::string>& frag) :
      vertex(vert), fragment(frag),
      iAccessCounter(1),
      pGLSLProgram(new GLSLProgram(mc))
    {
      pGLSLProgram->Load(vert, frag);
      if(!pGLSLProgram->IsValid()) {
        delete pGLSLProgram;
        pGLSLProgram = NULL;
      }
    }

    ~GLSLListElem()
    {
      delete pGLSLProgram;
    }

    const std::vector<std::string> vertex;
    const std::vector<std::string> fragment;
    UINT32        iAccessCounter;
    GLSLProgram*  pGLSLProgram;
  };
  typedef std::deque<GLSLListElem*> GLSLList;
  typedef GLSLList::iterator GLSLListIter;
  typedef GLSLList::const_iterator GLSLConstListIter;
};

#endif // GPUMEMMANDATASTRUCTS_H
