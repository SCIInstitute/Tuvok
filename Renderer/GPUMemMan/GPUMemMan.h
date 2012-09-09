/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2012 Scientific Computing and Imaging Institute,
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
#pragma once

#ifndef TUVOK_GPUMEMMAN_H
#define TUVOK_GPUMEMMAN_H

#include <deque>
#include <utility>
#include "../../StdTuvokDefines.h"
#include "3rdParty/GLEW/GL/glew.h"
#include "Basics/Vectors.h"
#include "GPUMemManDataStructs.h"

class SystemInfo;
class TransferFunction1D;
class TransferFunction2D;

namespace tuvok {

class AbstrRenderer;
class GLFBOTex;
class GLSLProgram;
class GLTexture1D;
class GLTexture2D;
class MasterController;
class GLVolumePool; 

class Dataset;  
class UVFDataset;

class GPUMemMan {
  public:
    GPUMemMan(MasterController* masterController);
    virtual ~GPUMemMan();

    Dataset* LoadDataset(const std::string& strFilename,
                         AbstrRenderer* requester);
    void AddDataset(Dataset* ds, AbstrRenderer *requester);
    void FreeAssociatedTextures(Dataset* pDataset, int iShareGroupID);
    void FreeDataset(Dataset* pVolumeDataset, AbstrRenderer* requester);

    void Changed1DTrans(LuaClassInstance abstrRenderer,
                        LuaClassInstance transferFun1D);
    void GetEmpty1DTrans(size_t iSize, AbstrRenderer* requester,
                         TransferFunction1D** ppTransferFunction1D,
                         GLTexture1D** tex);
    void Get1DTransFromFile(const std::string& strFilename,
                            AbstrRenderer* requester,
                            TransferFunction1D** ppTransferFunction1D,
                            GLTexture1D** tex,
                            size_t iSize=0);
    std::pair<TransferFunction1D*, GLTexture1D*>
    SetExternal1DTrans(const std::vector<unsigned char>& rgba,
                       AbstrRenderer* requester);
    GLTexture1D* Access1DTrans(TransferFunction1D* pTransferFunction1D,
                               AbstrRenderer* requester);
    void Free1DTrans(TransferFunction1D* pTransferFunction1D,
                     const AbstrRenderer* requester);

    void Changed2DTrans(LuaClassInstance luaAbstrRen,
                        LuaClassInstance tf2d);
    void GetEmpty2DTrans(const VECTOR2<size_t>& vSize,
                         AbstrRenderer* requester,
                         TransferFunction2D** ppTransferFunction2D,
                         GLTexture2D** tex);
    void Get2DTransFromFile(const std::string& strFilename,
                            AbstrRenderer* requester,
                            TransferFunction2D** ppTransferFunction2D,
                            GLTexture2D** tex,
                            const VECTOR2<size_t>& iSize=VECTOR2<size_t>(0,0));
    GLTexture2D* Access2DTrans(TransferFunction2D* pTransferFunction2D,
                               AbstrRenderer* requester);
    void Free2DTrans(TransferFunction2D* pTransferFunction2D,
                     const AbstrRenderer* requester);

    GLTexture2D* Load2DTextureFromFile(const std::string& strFilename, int iShareGroupID);
    void FreeTexture(GLTexture2D* pTexture);

    GLVolume* GetVolume(Dataset* pDataset, const BrickKey& key,
                        bool bUseOnlyPowerOfTwo, bool bDownSampleTo8Bits,
                        bool bDisableBorder, bool bEmulate3DWith2DStacks,
                        uint64_t iIntraFrameCounter, uint64_t iFrameCounter,
                        int iShareGroupID);
    bool IsResident(const Dataset* pDataset,
                    const BrickKey& key, bool bUseOnlyPowerOfTwo,
                    bool bDownSampleTo8Bits, bool bDisableBorder,
                    bool bEmulate3DWith2DStacks,
                    int iShareGroupID) const;

    void Release3DTexture(GLVolume* pGLVolume);

    GLFBOTex* GetFBO(GLenum minfilter, GLenum magfilter, GLenum wrapmode,
                     GLsizei width, GLsizei height, GLenum intformat,
                     GLenum format, GLenum type,
                     int iShareGroupID, 
                     bool bHaveDepth=false, int iNumBuffers=1);
    void FreeFBO(GLFBOTex* pFBO);

    GLSLProgram* GetGLSLProgram(const ShaderDescriptor& sdesc,
                                int iShareGroupID);
    void FreeGLSLProgram(GLSLProgram* pGLSLProgram);

    GLVolumePool* GetVolumePool(UVFDataset* dataSet, GLenum filter, int iShareGroupID);
    void DeleteVolumePool(GLVolumePool** pool);

    void MemSizesChanged();

    // ok, i know this uint64_t could theoretically overflow but lets assume the
    // universe collapses before that happens
    // Seems likely. -- TJF
    uint64_t UpdateFrameCounter() {return ++m_iFrameCounter;}

    /// system statistics
    ///@{
    uint64_t GetCPUMem() const;
    uint64_t GetGPUMem() const;
    uint64_t GetAllocatedCPUMem() const {return m_iAllocatedCPUMemory;}
    uint64_t GetAllocatedGPUMem() const {return m_iAllocatedGPUMemory;}
    uint32_t GetBitWidthMem() const;
    uint32_t GetNumCPUs() const;
    ///@}

  private:
    VolDataList                 m_vpVolumeDatasets;
    SimpleTextureList           m_vpSimpleTextures;
    Trans1DList                 m_vpTrans1DList;
    Trans2DList                 m_vpTrans2DList;
    GLVolumeList                m_vpTex3DList;
    FBOList                     m_vpFBOList;
    GLSLList                    m_vpGLSLList;
    MasterController*           m_MasterController;
    SystemInfo*                 m_SystemInfo;

    uint64_t                    m_iAllocatedGPUMemory;
    uint64_t                    m_iAllocatedCPUMemory;
    uint64_t                    m_iFrameCounter;

    uint64_t                    m_iInCoreSize;

    std::vector<unsigned char>  m_vUploadHub;

    std::unique_ptr<LuaMemberReg> m_pMemReg;

    GLVolume* AllocOrGetVolume(Dataset* pDataset,
                               const BrickKey& key,
                               bool bUseOnlyPowerOfTwo,
                               bool bDownSampleTo8Bits,
                               bool bDisableBorder,
                               bool bEmulate3DWith2DStacks,
                               uint64_t iIntraFrameCounter,
                               uint64_t iFrameCounter,
                               int iShareGroupID);
    size_t DeleteUnusedBricks(int iShareGroupID);
    void DeleteArbitraryBrick(int iShareGroupID);
    void Delete3DTexture(size_t iIndex);
    void Delete3DTexture(const GLVolumeListIter &tex);
    void RegisterLuaCommands();
};
}
#endif // TUVOK_GPUMEMMAN_H
