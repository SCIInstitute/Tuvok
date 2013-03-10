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
  \file    GPUMemMan.h
  \author    Jens Krueger
        SCI Institute
        University of Utah
  \date    August 2008
*/

#include <cassert>
#include <cstddef>
#include <algorithm>
#include <functional>
#include <numeric>
#include <typeinfo>
// normally we'd include Qt headers first, but we want to make sure to get GLEW
// before GL in this special case.
#include "GPUMemMan.h"
#ifndef TUVOK_NO_QT
# include <QtGui/QImage>
# include <QtOpenGL/QGLWidget>
#endif

#include "Basics/SystemInfo.h"
#include "Basics/SysTools.h"
#include "Controller/Controller.h"
#include "GPUMemManDataStructs.h"
#include "IO/FileBackedDataset.h"
#include "IO/IOManager.h"
#include "IO/TransferFunction1D.h"
#include "IO/TransferFunction2D.h"
#include "IO/TuvokSizes.h"
#include "Renderer/AbstrRenderer.h"
#include "Renderer/ShaderDescriptor.h"
#include "Renderer/GL/GLError.h"
#include "Renderer/GL/GLTexture1D.h"
#include "Renderer/GL/GLTexture2D.h"
#include "Renderer/GL/GLVolume.h"

#include "IO/uvfDataset.h"
#include "Renderer/GL/GLVolumePool.h"

#include "../LuaScripting/LuaScripting.h"
#include "../LuaScripting/LuaMemberReg.h"
#include "../LuaScripting/TuvokSpecific/LuaTuvokTypes.h"
#include "../LuaScripting/TuvokSpecific/LuaTransferFun1DProxy.h"
#include "../LuaScripting/TuvokSpecific/LuaTransferFun2DProxy.h"

using namespace std;
using namespace std::placeholders;
using namespace tuvok;

GPUMemMan::GPUMemMan(MasterController* masterController) :
  m_MasterController(masterController),
  m_SystemInfo(masterController->SysInfo()),
  m_iAllocatedGPUMemory(0),
  m_iAllocatedCPUMemory(0),
  m_iFrameCounter(0),
  m_iInCoreSize(DEFAULT_INCORESIZE),
  m_pMemReg(new LuaMemberReg(masterController->LuaScript()))
{
  if (masterController && masterController->IOMan()) {
    m_iInCoreSize = masterController->IOMan()->GetIncoresize();
  }

  m_vUploadHub.resize(size_t(m_iInCoreSize*4));
  m_iAllocatedCPUMemory = size_t(m_iInCoreSize*4);

  RegisterLuaCommands();
}

GPUMemMan::~GPUMemMan() {
  // Can't access the controller through the singleton; this destructor is
  // called during MC's destructor!  Since the MC is dying, we shouldn't rely
  // on anything within it being valid, but as a bit of a hack we'll grab the
  // active debug output anyway.  This works because we know that the debug
  // outputs will be deleted last -- after the memory manager.
  AbstrDebugOut &dbg = *(m_MasterController->DebugOut());
  for (VolDataListIter i = m_vpVolumeDatasets.begin();
       i < m_vpVolumeDatasets.end(); ++i) {
    try {
      dbg.Warning(_func_, "Detected unfreed dataset %s.",
                  dynamic_cast<FileBackedDataset&>
                              (*(i->pVolumeDataset)).Filename().c_str());
    } catch(std::bad_cast) {
      dbg.Warning(_func_, "Detected unfreed dataset %p.", i->pVolumeDataset);
    }
    delete i->pVolumeDataset;
  }

  for (SimpleTextureListIter i = m_vpSimpleTextures.begin();
       i < m_vpSimpleTextures.end(); ++i) {
    dbg.Warning(_func_, "Detected unfreed SimpleTexture %s.",
                i->strFilename.c_str());

    m_iAllocatedGPUMemory -= i->pTexture->GetGPUSize();
    m_iAllocatedCPUMemory -= i->pTexture->GetCPUSize();

    delete i->pTexture;
  }

  for (Trans1DListIter i = m_vpTrans1DList.begin();
       i < m_vpTrans1DList.end(); ++i) {
    dbg.Warning(_func_, "Detected unfreed 1D Transferfunction.");

    m_iAllocatedGPUMemory -= i->pTexture->GetGPUSize();
    m_iAllocatedCPUMemory -= i->pTexture->GetCPUSize();

    delete i->pTexture;
    delete i->pTransferFunction1D;
  }

  for (Trans2DListIter i = m_vpTrans2DList.begin();
       i < m_vpTrans2DList.end(); ++i) {
    dbg.Warning(_func_, "Detected unfreed 2D Transferfunction.");

    m_iAllocatedGPUMemory -= i->pTexture->GetGPUSize();
    m_iAllocatedCPUMemory -= i->pTexture->GetCPUSize();

    delete i->pTexture;
    delete i->pTransferFunction2D;
  }

  for (GLVolumeListIter i = m_vpTex3DList.begin();
       i < m_vpTex3DList.end(); ++i) {
    dbg.Warning(_func_, "Detected unfreed 3D texture.");

    m_iAllocatedGPUMemory -= (*i)->GetGPUSize();
    m_iAllocatedCPUMemory -= (*i)->GetCPUSize();

    delete (*i);
  }

  for (FBOListIter i = m_vpFBOList.begin();
       i < m_vpFBOList.end(); ++i) {
    dbg.Warning(_func_, "Detected unfreed FBO.");

    m_iAllocatedGPUMemory -= (*i)->pFBOTex->GetGPUSize();
    m_iAllocatedCPUMemory -= (*i)->pFBOTex->GetCPUSize();

    delete (*i);
  }

  for (GLSLListIter i = m_vpGLSLList.begin();
       i < m_vpGLSLList.end(); ++i) {
    dbg.Warning(_func_, "Detected unfreed GLSL program.");

    m_iAllocatedGPUMemory -= (*i)->pGLSLProgram->GetGPUSize();
    m_iAllocatedCPUMemory -= (*i)->pGLSLProgram->GetCPUSize();

    delete (*i);
  }

  m_vUploadHub.clear();
  m_iAllocatedCPUMemory -= size_t(m_iInCoreSize*4);

  assert(m_iAllocatedGPUMemory == 0);
  assert(m_iAllocatedCPUMemory == 0);
}

// ******************** Datasets

Dataset* GPUMemMan::LoadDataset(const string& strFilename,
                                AbstrRenderer* requester) {
  // We want to reuse datasets which have already been loaded.  Yet
  // we have a list of `Dataset's, not `FileBackedDataset's, and so
  // therefore we can't rely on each element of the list having a file
  // backing it up.
  //
  // Yet they all will; this method is never going to get called for datasets
  // which are given from clients via an in-memory transfer.  Thus nothing is
  // ever going to get added to the list which isn't a FileBackedDataset.
  //
  // We could make the list be composed only of FileBackedDatasets.
  // Eventually we'd like to do dataset caching for any client though,
  // not just ImageVis3D.
  for (VolDataListIter i = m_vpVolumeDatasets.begin();
       i < m_vpVolumeDatasets.end(); ++i) {
    // Given the above explanation, this cast is guaranteed to succeed.
    FileBackedDataset *dataset = dynamic_cast<FileBackedDataset*>
                                             (i->pVolumeDataset);
    assert(dataset != NULL);

    if (dataset->Filename() == strFilename) {
      MESSAGE("Reusing %s", strFilename.c_str());
      i->qpUser.push_back(requester);
      return i->pVolumeDataset;
    }
  }

  MESSAGE("Loading %s", strFilename.c_str());
  const IOManager& mgr = *(m_MasterController->IOMan());
  /// @todo fixme: just use `Dataset's here; instead of explicitly doing the
  /// IsOpen check, below, just rely on an exception being thrown.
  Dataset* dataset =
    // false: assume the file has already been verified
    mgr.CreateDataset(strFilename, mgr.GetMaxBrickSize(), false);

  m_vpVolumeDatasets.push_back(VolDataListElem(dataset, requester));
  return dataset;
}

void GPUMemMan::AddDataset(Dataset* ds, AbstrRenderer *requester)
{
  m_vpVolumeDatasets.push_back(VolDataListElem(ds, requester));
}

// Functor to find the VolDataListElem which holds the Dataset given in
// the constructor.
struct find_ds : public std::unary_function<VolDataListElem, bool> {
  find_ds(const Dataset* vds) : _ds(vds) { }
  bool operator()(const VolDataListElem &cmp) const {
    return cmp.pVolumeDataset == _ds;
  }
  private: const Dataset* _ds;
};

void GPUMemMan::FreeDataset(Dataset* pVolumeDataset,
                            AbstrRenderer* requester) {
  // store a name conditional for later logging
  std::string ds_name;
  try {
    const FileBackedDataset& ds = dynamic_cast<FileBackedDataset&>
                                              (*pVolumeDataset);
    ds_name = ds.Filename();
  } catch(std::bad_cast) {
    ds_name = "(unnamed dataset)";
  }

  // find the dataset this refers to in our internal list
  VolDataListIter vol_ds = std::find_if(m_vpVolumeDatasets.begin(),
                                        m_vpVolumeDatasets.end(),
                                        find_ds(pVolumeDataset));

  // Don't access the singleton; see comment in the destructor.
  AbstrDebugOut &dbg = *(m_MasterController->DebugOut());
  if(vol_ds == m_vpVolumeDatasets.end()) {
    dbg.Warning(_func_,"Dataset '%s' not found or not being used by requester",
                ds_name.c_str());
    return;
  }

  // search for a renderer that the dataset is using
  AbstrRendererListIter renderer = std::find(vol_ds->qpUser.begin(),
                                             vol_ds->qpUser.end(), requester);
  // bail out if there doesn't appear to be a link between the DS and a
  // renderer.
  if(renderer == vol_ds->qpUser.end()) {
    dbg.Warning(_func_, "Dataset %s does not seem to be associated "
                        "with a renderer.", ds_name.c_str());
    return;
  }

  // remove it from the list of renderers which use this DS; if this brings the
  // reference count of the DS to 0, delete it.
  vol_ds->qpUser.erase(renderer);
  if(vol_ds->qpUser.empty()) {
    dbg.Message(_func_, "Cleaning up all 3D textures associated w/ dataset %s",
                ds_name.c_str());
    if (requester->GetContext()) // if we never created a context then we never created any textures
      FreeAssociatedTextures(pVolumeDataset, requester->GetContext()->GetShareGroupID());
    dbg.Message(_func_, "Released Dataset %s", ds_name.c_str());
    delete pVolumeDataset;
    m_vpVolumeDatasets.erase(vol_ds);
  } else {
    dbg.Message(_func_,"Decreased access count but dataset %s is still "
                " in use by another subsystem", ds_name.c_str());
  }
}

// ******************** Simple Textures

GLTexture2D* GPUMemMan::Load2DTextureFromFile(const string& strFilename, int iShareGroupID) {
  for (SimpleTextureListIter i = m_vpSimpleTextures.begin();
       i < m_vpSimpleTextures.end(); ++i) {
    if (i->strFilename == strFilename && i->m_iShareGroupID == iShareGroupID) {
      MESSAGE("Reusing %s", strFilename.c_str());
      i->iAccessCounter++;
      return i->pTexture;
    }
  }

#ifndef TUVOK_NO_QT
  QImage image;
  if (!image.load(strFilename.c_str())) {
    T_ERROR("Unable to load file %s", strFilename.c_str());
    return NULL;
  }
  MESSAGE("Loaded %s, now creating OpenGL resources ..", strFilename.c_str());

  QImage glimage = QGLWidget::convertToGLFormat(image);

  GLTexture2D* tex = new GLTexture2D(glimage.width(),glimage.height(),
                                     GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE,
                                     glimage.bits());

  m_iAllocatedGPUMemory += tex->GetGPUSize();
  m_iAllocatedCPUMemory += tex->GetCPUSize();

  m_vpSimpleTextures.push_back(SimpleTextureListElem(1,tex,strFilename,iShareGroupID));
  return tex;
#else
  T_ERROR("No Qt support!");
  return NULL;
#endif
}


void GPUMemMan::FreeTexture(GLTexture2D* pTexture) {
  for (SimpleTextureListIter i = m_vpSimpleTextures.begin();
       i < m_vpSimpleTextures.end(); ++i) {
    if (i->pTexture == pTexture) {
      i->iAccessCounter--;
      if (i->iAccessCounter == 0) {
        MESSAGE("Deleted texture %s", i->strFilename.c_str());

        m_iAllocatedGPUMemory -= i->pTexture->GetGPUSize();
        m_iAllocatedCPUMemory -= i->pTexture->GetCPUSize();

        i->pTexture->Delete();
        m_vpSimpleTextures.erase(i);
      } else {
        MESSAGE("Decreased access count, "
                "but the texture %s is still in use by another subsystem",
                i->strFilename.c_str());
      }
      return;
    }
  }
  WARNING("Texture not found");
}

// ******************** 1D Trans

void GPUMemMan::Changed1DTrans(LuaClassInstance luaAbstrRen,
                               LuaClassInstance tf1d) {
  MESSAGE("Sending change notification for 1D transfer function");

  shared_ptr<LuaScripting> ss = m_MasterController->LuaScript();

  // Convert LuaClassInstance -> LuaTransferFun1DProxy -> TransferFunction1D
  LuaTransferFun1DProxy* tfProxy = 
      tf1d.getRawPointer<LuaTransferFun1DProxy>(ss);
  TransferFunction1D* pTransferFunction1D = tfProxy->get1DTransferFunction();

  pTransferFunction1D->ComputeNonZeroLimits();

  // Retrieve raw pointer for the Abstract renderer.
  AbstrRenderer* requester = NULL;
  if (luaAbstrRen.isValid(ss)) {
    requester = luaAbstrRen.getRawPointer<AbstrRenderer>(ss);
  }

  for (Trans1DListIter i = m_vpTrans1DList.begin();i<m_vpTrans1DList.end();i++) {
    if (i->pTransferFunction1D == pTransferFunction1D) {
      for (AbstrRendererListIter j = i->qpUser.begin();j<i->qpUser.end();j++) {
        if (*j != requester) (*j)->Changed1DTrans();
      }
    }
  }
}

void GPUMemMan::GetEmpty1DTrans(size_t iSize, AbstrRenderer* requester,
                                TransferFunction1D** ppTransferFunction1D,
                                GLTexture1D** tex) {
  MESSAGE("Creating new empty 1D transfer function");
  assert(iSize > 0); // if not, our TF would be *really* empty :)
  *ppTransferFunction1D = new TransferFunction1D(iSize);
  (*ppTransferFunction1D)->SetStdFunction();

  std::vector<unsigned char> vTFData;
  (*ppTransferFunction1D)->GetByteArray(vTFData);
  *tex = new GLTexture1D(uint32_t((*ppTransferFunction1D)->GetSize()), GL_RGBA8,
                         GL_RGBA, GL_UNSIGNED_BYTE, &vTFData.at(0));

  m_iAllocatedGPUMemory += (*tex)->GetGPUSize();
  m_iAllocatedCPUMemory += (*tex)->GetCPUSize();

  m_vpTrans1DList.push_back(Trans1DListElem(*ppTransferFunction1D, *tex,
                                            requester));
}

void GPUMemMan::Get1DTransFromFile(const string& strFilename,
                                   AbstrRenderer* requester,
                                   TransferFunction1D** ppTransferFunction1D,
                                   GLTexture1D** tex, size_t iSize) {
  MESSAGE("Loading 1D transfer function from file");
  *ppTransferFunction1D = new TransferFunction1D(strFilename);

  if (iSize != 0 && (*ppTransferFunction1D)->GetSize() != iSize) {
    (*ppTransferFunction1D)->Resample(iSize);
  }

  std::vector<unsigned char> vTFData;
  (*ppTransferFunction1D)->GetByteArray(vTFData);
  *tex = new GLTexture1D(uint32_t((*ppTransferFunction1D)->GetSize()), GL_RGBA8,
                         GL_RGBA, GL_UNSIGNED_BYTE, &vTFData.at(0));

  m_iAllocatedGPUMemory += (*tex)->GetGPUSize();
  m_iAllocatedCPUMemory += (*tex)->GetCPUSize();

  m_vpTrans1DList.push_back(Trans1DListElem(*ppTransferFunction1D, *tex,
                                            requester));
}

std::pair<TransferFunction1D*, GLTexture1D*>
GPUMemMan::SetExternal1DTrans(const std::vector<unsigned char>& rgba,
                              AbstrRenderer* requester)
{
  const size_t sz = rgba.size() / 4; // RGBA, i.e. 4 components.
  MESSAGE("Setting %u element 1D TF from external source.",
          static_cast<uint32_t>(sz));
  assert(!rgba.empty());

  TransferFunction1D *tf1d = new TransferFunction1D(sz);
  tf1d->Set(rgba);

  GLTexture1D *tex = new GLTexture1D(static_cast<uint32_t>(tf1d->GetSize()),
                                     GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE,
                                     &rgba.at(0));
  m_iAllocatedGPUMemory += tex->GetGPUSize();
  m_iAllocatedCPUMemory += tex->GetCPUSize();

  m_vpTrans1DList.push_back(Trans1DListElem(tf1d, tex, requester));

  return std::make_pair(tf1d, tex);
}

GLTexture1D* GPUMemMan::Access1DTrans(TransferFunction1D* pTransferFunction1D,
                                      AbstrRenderer* requester) {
  for (Trans1DListIter i = m_vpTrans1DList.begin();i<m_vpTrans1DList.end();i++) {
    if (i->pTransferFunction1D == pTransferFunction1D) {
      MESSAGE("Accessing 1D transferfunction");
      i->qpUser.push_back(requester);
      return i->pTexture;
    }
  }

  T_ERROR("Unable to find 1D transferfunction");
  return NULL;
}

void GPUMemMan::Free1DTrans(TransferFunction1D* pTransferFunction1D,
                            const AbstrRenderer* requester) {
  AbstrDebugOut &dbg = *(m_MasterController->DebugOut());
  for (Trans1DListIter i = m_vpTrans1DList.begin();
       i < m_vpTrans1DList.end(); ++i) {
    if (i->pTransferFunction1D == pTransferFunction1D) {
      for (AbstrRendererListIter j = i->qpUser.begin();j<i->qpUser.end();j++) {
        if (*j == requester) {
          i->qpUser.erase(j);
          if (i->qpUser.empty()) {
            dbg.Message(_func_, "Released 1D TF");

            m_iAllocatedGPUMemory -= i->pTexture->GetGPUSize();
            m_iAllocatedCPUMemory -= i->pTexture->GetCPUSize();

            delete i->pTransferFunction1D;
            i->pTexture->Delete();
            delete i->pTexture;
            m_vpTrans1DList.erase(i);
          } else {
            dbg.Message(_func_, "Decreased access count, but 1D TF is still "
                                "in use by another subsystem.");
          }
          return;
        }
      }
    }
  }
  dbg.Warning(_func_, "1D TF not found not in use by requester.");
}

// ******************** 2D Trans

void GPUMemMan::Changed2DTrans(LuaClassInstance luaAbstrRen,
                               LuaClassInstance tf2d) {
  MESSAGE("Sending change notification for 2D transfer function");

  shared_ptr<LuaScripting> ss = m_MasterController->LuaScript();

  // Convert LuaClassInstance -> LuaTransferFun2DProxy -> TransferFunction2D
  LuaTransferFun2DProxy* tfProxy = 
      tf2d.getRawPointer<LuaTransferFun2DProxy>(ss);
  TransferFunction2D* pTransferFunction2D = tfProxy->get2DTransferFunction();

  pTransferFunction2D->InvalidateCache();
  pTransferFunction2D->ComputeNonZeroLimits();

  // Retrieve raw pointer for the Abstract renderer.
  AbstrRenderer* requester = NULL;
  if (luaAbstrRen.isValid(ss)) {
    requester = luaAbstrRen.getRawPointer<AbstrRenderer>(ss);
  }

  for (Trans2DListIter i = m_vpTrans2DList.begin();
       i < m_vpTrans2DList.end(); ++i) {
    if (i->pTransferFunction2D == pTransferFunction2D) {
      for (AbstrRendererListIter j = i->qpUser.begin();j<i->qpUser.end();j++) {
        if (*j != requester) (*j)->Changed2DTrans();
      }
    }
  }

}

void GPUMemMan::GetEmpty2DTrans(const VECTOR2<size_t>& iSize,
                                AbstrRenderer* requester,
                                TransferFunction2D** ppTransferFunction2D,
                                GLTexture2D** tex) {
  MESSAGE("Creating new empty 2D transfer function");
  *ppTransferFunction2D = new TransferFunction2D(iSize);

  unsigned char* pcData = NULL;
  (*ppTransferFunction2D)->GetByteArray(&pcData);
  *tex = new GLTexture2D(uint32_t(iSize.x), uint32_t(iSize.y), GL_RGBA8, GL_RGBA,
                         GL_UNSIGNED_BYTE, pcData);
  delete [] pcData;

  m_iAllocatedGPUMemory += (*tex)->GetGPUSize();
  m_iAllocatedCPUMemory += (*tex)->GetCPUSize();

  m_vpTrans2DList.push_back(Trans2DListElem(*ppTransferFunction2D, *tex,
                                            requester));
}

void GPUMemMan::Get2DTransFromFile(const string& strFilename,
                                   AbstrRenderer* requester,
                                   TransferFunction2D** ppTransferFunction2D,
                                   GLTexture2D** tex,
                                   const VECTOR2<size_t>& vSize) {
  MESSAGE("Loading 2D transfer function from file");
  *ppTransferFunction2D = new TransferFunction2D();

  if(!(*ppTransferFunction2D)->Load(strFilename)) {
    T_ERROR("Loading failed.");
    delete *ppTransferFunction2D;
    *ppTransferFunction2D = NULL;
    return;
  }

  if ((vSize.x != 0 || vSize.y != 0) &&
      (*ppTransferFunction2D)->GetSize() != vSize) {
    MESSAGE("2D transfer function needs resampling...");
    (*ppTransferFunction2D)->Resample(vSize);
  }

  unsigned char* pcData = NULL;
  (*ppTransferFunction2D)->GetByteArray(&pcData);
  *tex = new GLTexture2D(uint32_t((*ppTransferFunction2D)->GetSize().x),
                         uint32_t((*ppTransferFunction2D)->GetSize().y),
                         GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE,pcData);
  delete [] pcData;

  m_iAllocatedGPUMemory += (*tex)->GetGPUSize();
  m_iAllocatedCPUMemory += (*tex)->GetCPUSize();

  m_vpTrans2DList.push_back(Trans2DListElem(*ppTransferFunction2D, *tex,
                                            requester));
}

GLTexture2D* GPUMemMan::Access2DTrans(TransferFunction2D* pTransferFunction2D,
                                      AbstrRenderer* requester) {
  for (Trans2DListIter i = m_vpTrans2DList.begin();i<m_vpTrans2DList.end();i++) {
    if (i->pTransferFunction2D == pTransferFunction2D) {
      MESSAGE("Accessing 2D transferfunction");
      i->qpUser.push_back(requester);
      return i->pTexture;
    }
  }

  T_ERROR("Unable to find 2D transferfunction");
  return NULL;
}

void GPUMemMan::Free2DTrans(TransferFunction2D* pTransferFunction2D,
                            const AbstrRenderer* requester) {
  AbstrDebugOut &dbg = *(m_MasterController->DebugOut());
  for (Trans2DListIter i = m_vpTrans2DList.begin();
       i < m_vpTrans2DList.end(); ++i) {
    if (i->pTransferFunction2D == pTransferFunction2D) {
      for (AbstrRendererListIter j = i->qpUser.begin();j<i->qpUser.end();j++) {
        if (*j == requester) {
          i->qpUser.erase(j);
          if (i->qpUser.empty()) {
            dbg.Message(_func_, "Released 2D TF");

            m_iAllocatedGPUMemory -= i->pTexture->GetGPUSize();
            m_iAllocatedCPUMemory -= i->pTexture->GetCPUSize();

            delete i->pTransferFunction2D;
            i->pTexture->Delete();
            delete i->pTexture;

            m_vpTrans2DList.erase(i);
          } else {
            dbg.Message(_func_, "Decreased access count, "
                        "but 2D TF is still in use by another subsystem.");
          }
          return;
        }
      }
    }
  }
  dbg.Warning(_func_, "2D TF not found or not in use by requester.");
}

// ******************** Volumes

bool GPUMemMan::IsResident(const Dataset* pDataset,
                           const BrickKey& key,
                           bool bUseOnlyPowerOfTwo, bool bDownSampleTo8Bits,
                           bool bDisableBorder,
                           bool bEmulate3DWith2DStacks,
                           int iShareGroupID) const {
  for(GLVolumeListConstIter i = m_vpTex3DList.begin();
      i < m_vpTex3DList.end(); ++i) {
    if((*i)->Equals(pDataset, key, bUseOnlyPowerOfTwo,
                    bDownSampleTo8Bits, bDisableBorder,
                    bEmulate3DWith2DStacks, iShareGroupID)) {
      return true;
    }
  }
  return false;
}

/// Calculates the amount of memory the given brick will take up.
/// Slightly complicated because we might have an N-dimensional brick.
static uint64_t
required_cpu_memory(const Dataset& ds, const BrickKey& key)
{
  uint64_t mem = 1;
  const UINTVECTOR3 size = ds.GetBrickVoxelCounts(key);
  mem = size[0] * size[1] * size[2];
  mem *= ds.GetBitWidth()/8;
  mem *= ds.GetComponentCount();
  return mem;
}

/// Searches the texture list for a texture which matches the given criterion.
/// @return matching texture, or lst.end() if no texture matches.
static GLVolumeListIter
find_closest_texture(GLVolumeList &lst, const UINTVECTOR3& vSize,
                     bool use_pot, bool downsample, bool disable_border,
                     bool bEmulate3DWith2DStacks,
                     int iShareGroupID)
{
  uint64_t iTargetFrameCounter = UINT64_INVALID;
  uint64_t iTargetIntraFrameCounter = UINT64_INVALID;

  GLVolumeListIter iBestMatch = lst.end();
  for (GLVolumeListIter i=lst.begin(); i < lst.end(); ++i) {
    if ((*i)->BestMatch(vSize, use_pot, downsample, disable_border,
                        bEmulate3DWith2DStacks,
                        iTargetIntraFrameCounter, iTargetFrameCounter,
                        iShareGroupID)) {
      iBestMatch = i;
    }
  }
  if(iBestMatch != lst.end()) {
    MESSAGE("  Found suitable target brick from frame %llu with intraframe "
            "counter %llu.", iTargetFrameCounter,
            iTargetIntraFrameCounter);
  }
  return iBestMatch;
}


// We use our own function instead of a functor because we're searching through
// a list of 3D textures, which are noncopyable.  A copy operation on our 3d
// texes would be expensive.
template <typename ForwIter> static ForwIter
find_brick_with_usercount(ForwIter first, const ForwIter last,
                          int iShareGroupID, uint32_t user_count)
{
  while(first != last && !((*first)->iUserCount == user_count && iShareGroupID == (*first)->GetShareGroupID())) {
    ++first;
  }
  return first;
}

// Gets rid of *all* unused bricks.  Returns the number of bricks it deleted.
size_t GPUMemMan::DeleteUnusedBricks(int iShareGroupID) {
  size_t removed = 0;
  // This is a bit harsh.  erase() in the middle of a deque invalidates *all*
  // iterators.  So we repeatedly search for unused bricks, until our search
  // comes up empty.
  // That said, erase() at the beginning or end of a deque only invalidates the
  // pointed-to element.  You might consider an optimization where we delete
  // all unused bricks from the beginning and end of the structure, before
  // moving on to the exhaustive+expensive search.
  bool found;
  do {
    found = false;
    const GLVolumeListIter& iter = find_brick_with_usercount(
                                      m_vpTex3DList.begin(),
                                      m_vpTex3DList.end(), 0, iShareGroupID
                                    );
    if(iter != m_vpTex3DList.end()) {
      ++removed;
      found = true;
      Delete3DTexture(iter);
    }
  } while(!m_vpTex3DList.empty() && found);

  MESSAGE("Got rid of %u unused bricks.", static_cast<unsigned int>(removed));
  return removed;
}

// We don't have enough CPU memory to load something.  Get rid of a brick.
void GPUMemMan::DeleteArbitraryBrick(int iShareGroupID) {
  assert(!m_vpTex3DList.empty());

  // Identify the least used brick.  The 128 is an arbitrary choice.  We want
  // it to be high enough to hit every conceivable number of users for a brick.
  // We don't want to use 2^32 though, because then the application would feel
  // like it hung if we had some other bug.
  for(uint32_t in_use_by=0; in_use_by < 128; ++in_use_by) {
    const GLVolumeListIter& iter = find_brick_with_usercount(
                                      m_vpTex3DList.begin(),
                                      m_vpTex3DList.end(), iShareGroupID, in_use_by
                                    );
    if(iter != m_vpTex3DList.end()) {
      MESSAGE("  Deleting texture %d",
              std::distance(iter, m_vpTex3DList.begin()));
      Delete3DTexture(iter);
      return;
    }
  }
  WARNING("All bricks are (heavily) in use: "
          "cannot make space for a new brick.");
}

void GPUMemMan::DeleteVolumePool(GLVolumePool** pool) {
  delete  *pool;
  *pool = NULL;
}

GLVolumePool* GPUMemMan::GetVolumePool(LinearIndexDataset* dataSet,
                                       GLenum filter,
                                       int /* iShareGroupID */) {
  const uint64_t iBitWidth  = dataSet->GetBitWidth();
  const uint64_t iCompCount = dataSet->GetComponentCount();
  const UINTVECTOR3 vMaxBS = UINTVECTOR3(dataSet->GetMaxUsedBrickSizes());

  // Compute the pool size as a (almost) cubed texture that fits 
  // into the user specified GPU mem, is a multiple of the bricksize
  // and is no bigger than what OpenGL tells us is possible
  GLint iMaxVolumeDims;
  GL(glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE_EXT, &iMaxVolumeDims));
  const uint64_t iMaxGPUMem = Controller::Instance().SysInfo()->GetMaxUsableGPUMem()-m_iAllocatedGPUMemory;

  // the max brick layout that fits into the available GPU memory
  const uint64_t iMaxVoxelCount = iMaxGPUMem/(iCompCount*iBitWidth/8);
  const uint64_t r3Voxels = uint64_t(pow(iMaxVoxelCount, 1.0f/3.0f));
  UINTVECTOR3 maxBricksForGPU;

  // round the starting input size (r3Voxels) to the closest multiple brick size
  // to guarantee as cubic as possible volume pools and to better fill the
  // available amount of memory
  // (e.g. it creates a 1024x512x1536 pool instead of a 512x2048x512 pool for
  // a brick size of 512^3 and 3.4 GB available memory)
  uint64_t multipleOfBrickSizeThatFitsInX = uint64_t(((float)r3Voxels/vMaxBS.x) + 0.5f) *vMaxBS.x;
  if (multipleOfBrickSizeThatFitsInX > uint64_t(iMaxVolumeDims))
    multipleOfBrickSizeThatFitsInX = (iMaxVolumeDims/vMaxBS.x)*vMaxBS.x;
  maxBricksForGPU.x = (uint32_t)multipleOfBrickSizeThatFitsInX;

  uint64_t multipleOfBrickSizeThatFitsInY = ((iMaxVoxelCount/(maxBricksForGPU.x*maxBricksForGPU.x))/vMaxBS.y)*vMaxBS.y;
  if (multipleOfBrickSizeThatFitsInY > uint64_t(iMaxVolumeDims))
    multipleOfBrickSizeThatFitsInY = (iMaxVolumeDims/vMaxBS.y)*vMaxBS.y;
  maxBricksForGPU.y = (uint32_t)multipleOfBrickSizeThatFitsInY;

  uint64_t multipleOfBrickSizeThatFitsInZ = ((iMaxVoxelCount/(maxBricksForGPU.x*maxBricksForGPU.y))/vMaxBS.z)*vMaxBS.z;
  if (multipleOfBrickSizeThatFitsInZ > uint64_t(iMaxVolumeDims))
    multipleOfBrickSizeThatFitsInZ = (iMaxVolumeDims/vMaxBS.z)*vMaxBS.z;
  maxBricksForGPU.z = (uint32_t)multipleOfBrickSizeThatFitsInZ;

  // the max brick layout required by the dataset
  const uint64_t iMaxBrickCount = dataSet->GetTotalBrickCount();
  const uint64_t r3Bricks = uint64_t(pow(iMaxBrickCount, 1.0f/3.0f));
  UINT64VECTOR3 maxBricksForDataset;

  multipleOfBrickSizeThatFitsInX = vMaxBS.x*r3Bricks;
  if (multipleOfBrickSizeThatFitsInX > uint64_t(iMaxVolumeDims))
    multipleOfBrickSizeThatFitsInX = (iMaxVolumeDims/vMaxBS.x)*vMaxBS.x;
  maxBricksForDataset.x = (uint32_t)multipleOfBrickSizeThatFitsInX;

  multipleOfBrickSizeThatFitsInY = vMaxBS.y*uint64_t(ceil(float(iMaxBrickCount) / ((maxBricksForDataset.x/vMaxBS.x) * (maxBricksForDataset.x/vMaxBS.x))));
  if (multipleOfBrickSizeThatFitsInY > uint64_t(iMaxVolumeDims))
    multipleOfBrickSizeThatFitsInY = (iMaxVolumeDims/vMaxBS.y)*vMaxBS.y;
  maxBricksForDataset.y = (uint32_t)multipleOfBrickSizeThatFitsInY;

  multipleOfBrickSizeThatFitsInZ = vMaxBS.z*uint64_t(ceil(float(iMaxBrickCount) / ((maxBricksForDataset.x/vMaxBS.x) * (maxBricksForDataset.y/vMaxBS.y))));
  if (multipleOfBrickSizeThatFitsInZ > uint64_t(iMaxVolumeDims))
    multipleOfBrickSizeThatFitsInZ = (iMaxVolumeDims/vMaxBS.z)*vMaxBS.z;
  maxBricksForDataset.z = (uint32_t)multipleOfBrickSizeThatFitsInZ;

  // now use the smaller of the two layouts, normally that
  // would be the maxBricksForGPU but for small datasets that
  // can be rendered entirely in-core we may need less space
  const UINTVECTOR3 poolSize = (maxBricksForDataset.volume() < UINT64VECTOR3(maxBricksForGPU).volume())
                                    ? UINTVECTOR3(maxBricksForDataset)
                                    : maxBricksForGPU;
  GLVolumePool* pPool = NULL;
  try {
    pPool = new GLVolumePool(poolSize, dataSet, filter);
  } catch (tuvok::Exception const& e) {
    pPool = NULL;
    T_ERROR(e.what());
  }
  return pPool;
}

GLVolume* GPUMemMan::GetVolume(Dataset* pDataset, const BrickKey& key,
                               bool bUseOnlyPowerOfTwo,
                               bool bDownSampleTo8Bits,
                               bool bDisableBorder,
                               bool bEmulate3DWith2DStacks,
                               uint64_t iIntraFrameCounter,
                               uint64_t iFrameCounter,
                               int iShareGroupID) {
  // It can occur that we can create the brick in CPU memory but OpenGL must
  // perform a texture copy to obtain the texture.  If that happens, we'll
  // delete any brick and then try again.
  do {
    try {
      return this->AllocOrGetVolume(pDataset, key,
                                    bUseOnlyPowerOfTwo, bDownSampleTo8Bits,
                                    bDisableBorder, bEmulate3DWith2DStacks,
                                    iIntraFrameCounter, iFrameCounter,
                                    iShareGroupID);
    } catch(OutOfMemory&) { // Texture allocation failed.
      // If texture allocation failed and we had no bricks loaded, then the
      // system must be extremely memory limited.  Make a note and then bail.
      if(m_vpTex3DList.empty()) {
        T_ERROR("This system does not have enough memory to render a brick.");
        return NULL;
      }
      // Delete all bricks that aren't used.  If that ends up being nothing,
      // then we're pretty screwed.  Stupidly choose a brick in that case.
      if(0 == DeleteUnusedBricks(iShareGroupID)) {
        WARNING("No bricks unused.  Falling back to "
                "deleting bricks that ARE in use!");
        // Delete up to 4 bricks.  We want to delete multiple bricks here
        // because we'll temporarily need copies of the bricks in memory.
        for(size_t i=0; i < 4 && !m_vpTex3DList.empty(); ++i) {
          DeleteArbitraryBrick(iShareGroupID);
        }
      }
    }
  } while(!m_vpTex3DList.empty());
  // Can't happen, but to quiet compilers:
  return NULL;
}

GLVolume* GPUMemMan::AllocOrGetVolume(Dataset* pDataset,
                                      const BrickKey& key,
                                      bool bUseOnlyPowerOfTwo,
                                      bool bDownSampleTo8Bits,
                                      bool bDisableBorder,
                                      bool bEmulate3DWith2DStacks,
                                      uint64_t iIntraFrameCounter,
                                      uint64_t iFrameCounter,
                                      int iShareGroupID) {
  for (GLVolumeListIter i = m_vpTex3DList.begin();
       i < m_vpTex3DList.end(); i++) {
    if ((*i)->Equals(pDataset, key, bUseOnlyPowerOfTwo,
                     bDownSampleTo8Bits, bDisableBorder,
                     bEmulate3DWith2DStacks, iShareGroupID)) {
      GL_CHECK();
      MESSAGE("Reusing 3D texture");
      return (*i)->Access(iIntraFrameCounter, iFrameCounter);
    }
  }

  uint64_t iNeededCPUMemory = required_cpu_memory(*pDataset, key);

  /// @todo FIXME these keys are all wrong; we shouldn't be using N-dimensional
  /// data structures for the keys here.
  const UINTVECTOR3 sz = pDataset->GetBrickVoxelCounts(key);
  const uint64_t iBitWidth = pDataset->GetBitWidth();
  const uint64_t iCompCount = pDataset->GetComponentCount();

  // for OpenGL we ignore the GPU memory load and let GL do the paging
  if (m_iAllocatedCPUMemory + iNeededCPUMemory >
      m_SystemInfo->GetMaxUsableCPUMem()) {
    MESSAGE("Not enough memory for texture %u x %u x %u (%llubit * %llu), "
            "paging ...", sz[0], sz[1], sz[2],
            iBitWidth, iCompCount);

    // search for best brick to replace with this brick
    UINTVECTOR3 vSize = pDataset->GetBrickVoxelCounts(key);
    GLVolumeListIter iBestMatch = find_closest_texture(m_vpTex3DList, vSize,
                                                       bUseOnlyPowerOfTwo,
                                                       bDownSampleTo8Bits,
                                                       bDisableBorder,
                                                       bEmulate3DWith2DStacks,
                                                       iShareGroupID);
    if (iBestMatch != m_vpTex3DList.end()) {
      // found a suitable brick that can be replaced
      (*iBestMatch)->Replace(pDataset, key, bUseOnlyPowerOfTwo,
                             bDownSampleTo8Bits, bDisableBorder,
                             bEmulate3DWith2DStacks,
                             iIntraFrameCounter, iFrameCounter,
                             m_vUploadHub,
                             iShareGroupID);
      (*iBestMatch)->iUserCount++;
      return (*iBestMatch)->volume;
    } else {
      // We know the brick doesn't fit in memory, and we know there's no
      // existing texture which matches enough that we could overwrite it with
      // this one.  There's little we can do at this point ...
      WARNING("  No suitable brick found. Randomly deleting bricks until this"
              " brick fits into memory");

      while (m_iAllocatedCPUMemory + iNeededCPUMemory >
             m_SystemInfo->GetMaxUsableCPUMem()) {
        if (m_vpTex3DList.empty()) {
          // we do not have enough memory to page in even a single block...
          T_ERROR("Not enough memory to page a single brick into memory, "
                  "aborting (MaxMem=%llukb, NeededMem=%llukb).",
                  m_SystemInfo->GetMaxUsableCPUMem()/1024,
                  iNeededCPUMemory/1024);
          return NULL;
        }

        DeleteArbitraryBrick(iShareGroupID);
      }
    }
  }

  {
    std::ostringstream newvol;
    newvol << "Creating new GL volume " << sz[0] << " x " << sz[1] << " x "
           << sz[2] << ", bitsize=" << iBitWidth << ", componentcount="
           << iCompCount;
    MESSAGE("%s", newvol.str().c_str());
  }

  GLVolumeListElem* pNew3DTex = new GLVolumeListElem(pDataset, key,
                                                     bUseOnlyPowerOfTwo,
                                                     bDownSampleTo8Bits,
                                                     bDisableBorder,
                                                     bEmulate3DWith2DStacks,
                                                     iIntraFrameCounter,
                                                     iFrameCounter,
                                                     m_MasterController,
                                                     m_vUploadHub,
                                                     iShareGroupID);

  if (pNew3DTex->volume == NULL) {
    T_ERROR("Failed to create OpenGL resource for volume.");
    delete pNew3DTex;
    return NULL;
  }
  MESSAGE("texture(s) created.");
  pNew3DTex->iUserCount = 1;

  m_iAllocatedGPUMemory += pNew3DTex->GetGPUSize();
  m_iAllocatedCPUMemory += pNew3DTex->GetCPUSize();

  m_vpTex3DList.push_back(pNew3DTex);
  return (*(m_vpTex3DList.end()-1))->volume;
}

void GPUMemMan::Release3DTexture(GLVolume* pGLVolume) {
  for (size_t i = 0;i<m_vpTex3DList.size();i++) {
    if (m_vpTex3DList[i]->volume == pGLVolume) {
      if (m_vpTex3DList[i]->iUserCount > 0) {
        m_vpTex3DList[i]->iUserCount--;
        MESSAGE("Decreased 3D texture use count to %u",
                m_vpTex3DList[i]->iUserCount);
      } else {
        WARNING("Attempting to release a 3D volume that is not in use.");
      }
    }
  }
}


void GPUMemMan::Delete3DTexture(const GLVolumeListIter& tex) {
  m_iAllocatedGPUMemory -= (*tex)->GetGPUSize();
  m_iAllocatedCPUMemory -= (*tex)->GetCPUSize();

  if((*tex)->iUserCount != 0) {
    WARNING("Freeing used GL volume!");
  }
  MESSAGE("Deleting GL texture with use count %u",
          static_cast<unsigned>((*tex)->iUserCount));
  delete *tex;
  m_vpTex3DList.erase(tex);
}

void GPUMemMan::Delete3DTexture(size_t iIndex) {
  this->Delete3DTexture(m_vpTex3DList.begin() + iIndex);
}

// Functor to identify a texture that belongs to a particular dataset.
struct DatasetTexture: std::binary_function<Dataset, GLVolumeListElem, bool> {
  bool operator()(const GLVolumeListElem* tex, std::pair<const Dataset*, int> dsinfo) const {
    return tex->pDataset == dsinfo.first && tex->GetShareGroupID() == dsinfo.second;
  }
};

void GPUMemMan::FreeAssociatedTextures(Dataset* pDataset, int iShareGroupID) {
  // Don't use singleton; see destructor comments.
  AbstrDebugOut& dbg = *(m_MasterController->DebugOut());

  try {
    const FileBackedDataset& fbd = dynamic_cast<const
      FileBackedDataset&>(*pDataset);
    dbg.Message(_func_, "Deleting textures associated with '%s' dataset.",
                fbd.Filename().c_str());
  } catch(const std::bad_cast&) {}
  while(1) { // exit condition is checked for and `break'd in the loop.
    const GLVolumeListIter& iter =
      std::find_if(m_vpTex3DList.begin(), m_vpTex3DList.end(),
                   std::bind(DatasetTexture(), _1, make_pair(pDataset,iShareGroupID)) );
    if(iter == m_vpTex3DList.end()) {
      break;
    }
    Delete3DTexture(iter);
  }
}

void GPUMemMan::MemSizesChanged() {
  if (m_iAllocatedCPUMemory > m_SystemInfo->GetMaxUsableCPUMem()) {
      /// \todo CPU free resources to match max mem requirements
  }

  if (m_iAllocatedGPUMemory > m_SystemInfo->GetMaxUsableGPUMem()) {
      /// \todo GPU free resources to match max mem requirements
  }
}


GLFBOTex* GPUMemMan::GetFBO(GLenum minfilter, GLenum magfilter,
                            GLenum wrapmode, GLsizei width, GLsizei height,
                            GLenum intformat, GLenum format, GLenum type,
                            int iShareGroupID,
                            bool bHaveDepth, int iNumBuffers) {
  MESSAGE("Creating new FBO of size %i x %i", int(width), int(height));

  uint64_t m_iCPUMemEstimate = GLFBOTex::EstimateCPUSize(width, height,
                                                         bHaveDepth, iNumBuffers);

  // if we are running out of mem, kick out bricks to create room for the FBO
  while (m_iAllocatedCPUMemory + m_iCPUMemEstimate >
         m_SystemInfo->GetMaxUsableCPUMem() && !m_vpTex3DList.empty()) {
    MESSAGE("Not enough memory for FBO %i x %i x %i, "
            "paging out bricks ...", int(width), int(height), iNumBuffers);

    // search for best brick to replace with this brick
    uint64_t iMinTargetFrameCounter;
    uint64_t iMaxTargetIntraFrameCounter;
    (*m_vpTex3DList.begin())->GetCounters(iMaxTargetIntraFrameCounter,
                                          iMinTargetFrameCounter);
    size_t iIndex = 0;
    size_t iBestIndex = 0;

    for (GLVolumeListIter i = m_vpTex3DList.begin()+1;
         i < m_vpTex3DList.end(); ++i) {
      uint64_t iTargetFrameCounter = UINT64_INVALID;
      uint64_t iTargetIntraFrameCounter = UINT64_INVALID;
      (*i)->GetCounters(iTargetIntraFrameCounter, iTargetFrameCounter);
      iIndex++;

      if (iTargetFrameCounter < iMinTargetFrameCounter) {
        iMinTargetFrameCounter = iTargetFrameCounter;
        iMaxTargetIntraFrameCounter = iTargetIntraFrameCounter;
        iBestIndex = iIndex;
      } else {
        if (iTargetFrameCounter == iMinTargetFrameCounter &&
            iTargetIntraFrameCounter > iMaxTargetIntraFrameCounter) {
          iMaxTargetIntraFrameCounter = iTargetIntraFrameCounter;
          iBestIndex = iIndex;
        }
      }
    }
    MESSAGE("   Deleting texture %u", static_cast<unsigned>(iBestIndex));
    Delete3DTexture(iBestIndex);
  }


  FBOListElem* e = new FBOListElem(m_MasterController, minfilter, magfilter,
                                   wrapmode, width, height, intformat,
                                   format, type,
                                   bHaveDepth, iNumBuffers, iShareGroupID);

  if(!e->pFBOTex->Valid()) {
    T_ERROR("FBO creation failed!");
    return NULL;
  }

  // clear the buffer, on some GPUs new FBOs are not zeroed out
  e->pFBOTex->Write();
  glClearColor(0,0,0,0);
  glClear(GL_COLOR_BUFFER_BIT);
  e->pFBOTex->FinishWrite();


  m_vpFBOList.push_back(e);

  m_iAllocatedGPUMemory += e->pFBOTex->GetGPUSize();
  m_iAllocatedCPUMemory += e->pFBOTex->GetCPUSize();

  return e->pFBOTex;
}

void GPUMemMan::FreeFBO(GLFBOTex* pFBO) {
  for (size_t i = 0;i<m_vpFBOList.size();i++) {
    if (m_vpFBOList[i]->pFBOTex == pFBO) {
      MESSAGE("Freeing FBO ");
      m_iAllocatedGPUMemory -= m_vpFBOList[i]->pFBOTex->GetGPUSize();
      m_iAllocatedCPUMemory -= m_vpFBOList[i]->pFBOTex->GetCPUSize();

      delete m_vpFBOList[i];

      m_vpFBOList.erase(m_vpFBOList.begin()+i);
      return;
    }
  }
  WARNING("FBO to free not found.");
}

struct deref_glsl : public std::binary_function<GLSLListElem*, GLSLListElem*,
                                                bool> {
  bool operator ()(const GLSLListElem* a, const GLSLListElem* b) const {
    return *a == *b;
  }
};

GLSLProgram* GPUMemMan::GetGLSLProgram(const ShaderDescriptor& sdesc,
                                       int iShareGroupID)
{
  GLSLListElem elem(m_MasterController, sdesc, iShareGroupID, false);
  using namespace std::placeholders;
  GLSLListIter i = std::find_if(m_vpGLSLList.begin(), m_vpGLSLList.end(),
                                std::bind(deref_glsl(), _1, &elem));
  if(i != m_vpGLSLList.end()) {
    MESSAGE("Reusing GLSL program.");
    (*i)->iAccessCounter++;
    return (*i)->pGLSLProgram;
  }

  MESSAGE("Creating new GLSL program from %u-element VS and %u-element FS",
          std::distance(sdesc.begin_vertex(), sdesc.end_vertex()),
          std::distance(sdesc.begin_fragment(), sdesc.end_fragment()));

  GLSLListElem* e = new GLSLListElem(m_MasterController, sdesc,
                                     iShareGroupID);

  if(e->pGLSLProgram == NULL) {
    T_ERROR("Failed to create program!");
    return NULL;
  }

  m_vpGLSLList.push_back(e);
  m_iAllocatedGPUMemory += e->pGLSLProgram->GetGPUSize();
  m_iAllocatedCPUMemory += e->pGLSLProgram->GetCPUSize();

  return e->pGLSLProgram;
}

void GPUMemMan::FreeGLSLProgram(GLSLProgram* pGLSLProgram) {
  if (pGLSLProgram == NULL) return;

  for (size_t i = 0;i<m_vpGLSLList.size();i++) {
    if (m_vpGLSLList[i]->pGLSLProgram == pGLSLProgram) {
      m_vpGLSLList[i]->iAccessCounter--;
      if (m_vpGLSLList[i]->iAccessCounter == 0) {
        MESSAGE("Freeing GLSL program %u",
                static_cast<unsigned>(GLuint(*pGLSLProgram)));
        m_iAllocatedGPUMemory -= m_vpGLSLList[i]->pGLSLProgram->GetGPUSize();
        m_iAllocatedCPUMemory -= m_vpGLSLList[i]->pGLSLProgram->GetCPUSize();

        delete m_vpGLSLList[i];

        m_vpGLSLList.erase(m_vpGLSLList.begin()+i);
      } else {
        MESSAGE("Decreased access counter but kept GLSL program in memory.");
      }
      return;
    }
  }
  WARNING("GLSL program to free not found.");
}

uint64_t GPUMemMan::GetCPUMem() const {return m_SystemInfo->GetCPUMemSize();}
uint64_t GPUMemMan::GetGPUMem() const {return m_SystemInfo->GetGPUMemSize();}
uint32_t GPUMemMan::GetBitWidthMem() const {
  return m_SystemInfo->GetProgramBitWidth();
}
uint32_t GPUMemMan::GetNumCPUs() const {return m_SystemInfo->GetNumberOfCPUs();}


void GPUMemMan::RegisterLuaCommands() {
  std::string id;
  const std::string nm = "tuvok.gpu."; // namespace

  id = m_pMemReg->registerFunction(this,&GPUMemMan::Changed1DTrans,
                                   nm + "changed1DTrans", "", false);
  id = m_pMemReg->registerFunction(this,&GPUMemMan::Changed2DTrans,
                                   nm + "changed2DTrans", "", false);
}

