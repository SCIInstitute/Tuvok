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

#include <array>
#include <deque>
#include <memory>
#include <string>
#include <vector>
#include "3rdParty/GLEW/GL/glew.h"
#include "boost/noncopyable.hpp"
#include "Basics/Vectors.h"
#include "IO/Brick.h"
#include "../Context.h"
#include "../../StdTuvokDefines.h"
#include "../GL/GLFBOTex.h"
#include "../GL/GLSLProgram.h"
#include "Renderer/AbstrRenderer.h"
#include "Renderer/ShaderDescriptor.h"


class TransferFunction1D;
class TransferFunction2D;
class VolumeDataset;

namespace tuvok {
  class Dataset;
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

    Dataset*          pVolumeDataset;
    AbstrRendererList qpUser;
  };
  typedef std::deque<VolDataListElem> VolDataList;
  typedef VolDataList::iterator VolDataListIter;

  // simple textures
  class SimpleTextureListElem {
  public:
    SimpleTextureListElem(uint32_t _iAccessCounter, GLTexture2D* _pTexture, std::string _strFilename, int iShareGroupID) :
      iAccessCounter(_iAccessCounter),
      pTexture(_pTexture),
      strFilename(_strFilename),
      m_iShareGroupID(iShareGroupID)
    {}

    uint32_t      iAccessCounter;
    GLTexture2D*  pTexture;
    std::string   strFilename;
    int           m_iShareGroupID;
  };
  typedef std::deque<SimpleTextureListElem> SimpleTextureList;
  typedef SimpleTextureList::iterator SimpleTextureListIter;

  // 1D transfer functions
  class Trans1DListElem {
  public:
    Trans1DListElem(TransferFunction1D* _pTransferFunction1D, GLTexture1D* _pTexture, AbstrRenderer* pUser) :
      pTransferFunction1D(_pTransferFunction1D),
        pTexture(_pTexture),
        m_iShareGroupID(pUser->GetContext()->GetShareGroupID())
    {
      qpUser.push_back(pUser);
    }

    TransferFunction1D*  pTransferFunction1D;
    GLTexture1D*    pTexture;
    AbstrRendererList  qpUser;
    int m_iShareGroupID;
  };
  typedef std::deque<Trans1DListElem> Trans1DList;
  typedef Trans1DList::iterator Trans1DListIter;

  // 2D transfer functions
  class Trans2DListElem {
  public:
    Trans2DListElem(TransferFunction2D* _pTransferFunction2D, GLTexture2D* _pTexture, AbstrRenderer* pUser) :
      pTransferFunction2D(_pTransferFunction2D),
      pTexture(_pTexture),
      m_iShareGroupID(pUser->GetContext()->GetShareGroupID())
    {
      qpUser.push_back(pUser);
    }

    TransferFunction2D*  pTransferFunction2D;
    GLTexture2D*    pTexture;
    AbstrRendererList  qpUser;
    int m_iShareGroupID;
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
                      uint64_t iIntraFrameCounter,
                      uint64_t iFrameCounter, MasterController* pMasterController,
                      std::vector<unsigned char>& vUploadHub, int iShareGroupID);
    ~GLVolumeListElem();

    bool Equals(const Dataset* _pDataset, const BrickKey&,
                bool bIsPaddedToPowerOfTwo, bool bIsDownsampledTo8Bits,
                bool bDisableBorder, bool bEmulate3DWith2DStacks, int iShareGroupID);
    bool Replace(Dataset* _pDataset, const BrickKey&,
                 bool bIsPaddedToPowerOfTwo, bool bIsDownsampledTo8Bits,
                 bool bDisableBorder, bool bEmulate3DWith2DStacks,
                 uint64_t iIntraFrameCounter, uint64_t iFrameCounter,
                 std::vector<unsigned char>& vUploadHub, int iShareGroupID);
    bool BestMatch(const UINTVECTOR3& vDimension,
                   bool bIsPaddedToPowerOfTwo, bool bIsDownsampledTo8Bits,
                   bool bDisableBorder, bool bEmulate3DWith2DStacks,
                   uint64_t& iIntraFrameCounter,
                   uint64_t& iFrameCounter, int iShareGroupID);
    void GetCounters(uint64_t& iIntraFrameCounter, uint64_t& iFrameCounter) {
      iIntraFrameCounter = m_iIntraFrameCounter;
      iFrameCounter = m_iFrameCounter;
    }

    /// Calculates the sizes for all GLVolume's we've currently got loaded.
    ///@{
    size_t GetGPUSize() const;
    size_t GetCPUSize() const;
    ///@}

    GLVolume* Access(uint64_t& iIntraFrameCounter, uint64_t& iFrameCounter);

    bool LoadData(std::vector<unsigned char>& vUploadHub);
    void FreeData();
    std::pair<std::shared_ptr<unsigned char>, UINTVECTOR3> PadData(
      unsigned char* pRawData,
      UINTVECTOR3 vSize,
      uint64_t iBitWidth,
      uint64_t iCompCount
    );
    bool CreateTexture(std::vector<unsigned char>& vUploadHub,
                       bool bDeleteOldTexture=true);
    void FreeTexture();

    std::vector<unsigned char>    vData;
    GLVolume*                     volume;
    Dataset*                      pDataset;
    uint32_t                      iUserCount;

    uint64_t GetIntraFrameCounter() const {return m_iIntraFrameCounter;}
    uint64_t GetFrameCounter() const {return m_iFrameCounter;}

    int GetShareGroupID() const {return m_iShareGroupID;}
  
  private:
    bool Match(const UINTVECTOR3& vDimension);

    uint64_t m_iIntraFrameCounter;
    uint64_t m_iFrameCounter;
    MasterController* m_pMasterController;

    BrickKey m_Key;
    bool m_bIsPaddedToPowerOfTwo;
    bool m_bIsDownsampledTo8Bits;
    bool m_bDisableBorder;
    bool m_bEmulate3DWith2DStacks;
    bool m_bUsingHub;
    int m_iShareGroupID;
  };

  typedef std::deque<GLVolumeListElem*> GLVolumeList;
  typedef GLVolumeList::iterator GLVolumeListIter;
  typedef GLVolumeList::const_iterator GLVolumeListConstIter;

  // framebuffer objects
  class FBOListElem {
  public:
    FBOListElem(GLFBOTex* _pFBOTex, int iShareGroupID) : 
        pFBOTex(_pFBOTex),
        m_iShareGroupID(iShareGroupID)
    {}
    FBOListElem(MasterController* pMasterController, GLenum minfilter,
                GLenum magfilter, GLenum wrapmode,
                GLsizei width, GLsizei height, GLenum intformat,
                uint32_t iSizePerElement, bool bHaveDepth, 
                int iNumBuffers, int iShareGroupID) :
      pFBOTex(new GLFBOTex(pMasterController, minfilter, magfilter,
                           wrapmode, width, height, intformat, 
                           iSizePerElement, bHaveDepth, iNumBuffers)),
      m_iShareGroupID(iShareGroupID)
    {}

    ~FBOListElem()
    {
      delete pFBOTex;
    }

    GLFBOTex* pFBOTex;
    int m_iShareGroupID;
  };
  typedef std::deque<FBOListElem*> FBOList;
  typedef FBOList::iterator FBOListIter;


  // shader objects
  class GLSLListElem {
  public:
    GLSLListElem(MasterController* mc,
                 const ShaderDescriptor& sd,
                 int iShareGroupID,
                 bool load=true) :
      sdesc(sd),
      iAccessCounter(1),
      pGLSLProgram(new GLSLProgram(mc)),
      m_iShareGroupID(iShareGroupID)
    {
      if(load) {
        pGLSLProgram->Load(sdesc);
        if(!pGLSLProgram->IsValid()) {
          delete pGLSLProgram;
          pGLSLProgram = nullptr;
        }
      }
    }

    ~GLSLListElem() { delete pGLSLProgram; }

    bool operator ==(const GLSLListElem& glsl) const {
      return m_iShareGroupID == glsl.m_iShareGroupID &&
             sdesc == glsl.sdesc;
    }

    ShaderDescriptor sdesc;
    uint32_t iAccessCounter;
    GLSLProgram* pGLSLProgram;
    int m_iShareGroupID;
  };
  typedef std::deque<GLSLListElem*> GLSLList;
  typedef GLSLList::iterator GLSLListIter;
  typedef GLSLList::const_iterator GLSLConstListIter;
};

#endif // GPUMEMMANDATASTRUCTS_H
