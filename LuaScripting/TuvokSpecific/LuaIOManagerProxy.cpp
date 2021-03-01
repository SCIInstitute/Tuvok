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

/**
  \brief Lua class proxy for IO's IOManager.
  */

#include "Controller/Controller.h"
#include "3rdParty/LUA/lua.hpp"
#include "IO/IOManager.h"
#include "IO/FileBackedDataset.h"
#include "IO/uvfDataset.h"

#include <vector>

#include "../LuaScripting.h"
#include "../LuaClassRegistration.h"

#include "LuaTuvokTypes.h"

#include "LuaIOManagerProxy.h"
#include "LuaTransferFun1DProxy.h"

using namespace std;

namespace tuvok
{

LuaIOManagerProxy::LuaIOManagerProxy(IOManager* ioman,
                                     std::shared_ptr<LuaScripting> ss)
  : mIO(ioman),
    mReg(ss),
    mSS(ss)
{
  bind();
}

LuaIOManagerProxy::~LuaIOManagerProxy()
{
  mIO = NULL;
}

void LuaIOManagerProxy::bind()
{
  if (mIO != NULL)
  {
    std::string id;
    const std::string nm = "tuvok.io."; // namespace

    id = mReg.registerFunction(this, &LuaIOManagerProxy::ExportDataset,
                               nm + "exportDataset", "", false);
    id = mReg.registerFunction(this, &LuaIOManagerProxy::ExtractIsosurface,
                               nm + "extractIsosurface", "", false);
    id = mReg.registerFunction(this, &LuaIOManagerProxy::ExtractImageStack,
                               nm + "extractImageStack", "", false);
    id = mReg.registerFunction(this, &LuaIOManagerProxy::ExportMesh,
                               nm + "exportMesh", "", false);
    id = mReg.registerFunction(this, &LuaIOManagerProxy::ReBrickDataset,
                               nm + "rebrickDataset", "", false);
    id = mReg.registerFunction(this, &LuaIOManagerProxy::ConvertDataset,
                               nm + "convertDataset", "", false);
    mSS->addParamInfo(id, 0, "file list", "list of files to convert");
    mSS->addParamInfo(id, 1, "dest", "destination file");
    mSS->addParamInfo(id, 2, "temp", "directory to use as tmp");
    mSS->addParamInfo(id, 3, "interaction", "interaction allowed?");
    mSS->addParamInfo(id, 4, "quantize", "quantize to 8bit?");
    id = mReg.registerFunction(this, 
                               &LuaIOManagerProxy::ConvertDatasetWithStack,
                               nm + "convertDatasetWithStack", "", false);
    id = mReg.registerFunction(this, &LuaIOManagerProxy::AnalyzeDataset,
                               nm + "analyzeDataset", "", false);
    mSS->addParamInfo(id, 0, "MultRet", "Returns a tuple consisting of a "
                      "(1) boolean value representing whether or not the "
                      "function failed, and (2) the RangeInfo structure.");
    id = mReg.registerFunction(this, &LuaIOManagerProxy::evaluateExpression,
                               nm + "evaluateExpression", "", false);


    /// Functions that are not overloaded and can be registered directly.
    id = mReg.registerFunction(mIO, &IOManager::GetMaxBrickSize,
                               nm + "getMaxBrickSize", "", false);
    id = mReg.registerFunction(mIO, &IOManager::SetMaxBrickSize,
                               nm + "setMaxBrickSize", "", true);
    id = mReg.registerFunction(mIO, &IOManager::GetBuilderBrickSize,
                               nm + "getBuilderBrickSize", "", false);
    id = mReg.registerFunction(mIO, &IOManager::GetLoadDialogString,
                               nm + "getLoadDialogString", "", false);
    id = mReg.registerFunction(mIO, &IOManager::GetGeoExportDialogString,
                               nm + "getGeoExportDialogString", "", false);
    id = mReg.registerFunction(mIO, &IOManager::HasConverterForExt,
                               nm + "hasConverterForExt", "", false);
    id = mReg.registerFunction(mIO, &IOManager::HasGeoConverterForExt,
                               nm + "hasGeoConverterForExt", "", false);
    id = mReg.registerFunction(mIO, &IOManager::LoadMesh,
                               nm + "loadMesh", "", false);
    id = mReg.registerFunction(mIO, &IOManager::GetLoadGeoDialogString,
                               nm + "getLoadGeoDialogString", "", false);
    id = mReg.registerFunction(mIO, &IOManager::NeedsConversion,
                               nm + "needsConversion", "", false);
    id = mReg.registerFunction(mIO, &IOManager::Verify,
                               nm + "verify", "", false);
    id = mReg.registerFunction(mIO, &IOManager::GetExportDialogString,
                               nm + "getExportDialogString", "", false);
    id = mReg.registerFunction(mIO, &IOManager::ExportDialogFilterToExt,
                               nm + "exportDialogFilterToExt", "", false);
    id = mReg.registerFunction(mIO, &IOManager::GetImageExportDialogString,
                               nm + "getImageExportDialogString", "", false);
    id = mReg.registerFunction(mIO, &IOManager::ImageExportDialogFilterToExt,
                               nm + "imageExportDialogFilterToExt", "", false);
    id = mReg.registerFunction(mIO, &IOManager::MergeDatasets,
                               nm + "mergeDatasets", "", false);
    id = mReg.registerFunction(mIO, &IOManager::GetFormatList,
                               nm + "getFormatList", "", false);
    id = mReg.registerFunction(mIO, &IOManager::GetGeoFormatList,
                               nm + "getGeoFormatList", "", false);
    id = mReg.registerFunction(mIO, &IOManager::SetUseMedianFilter,
                               nm + "setUseMedianFilter", "", false);
    id = mReg.registerFunction(mIO, &IOManager::SetClampToEdge,
                               nm + "setClampToEdge", "", false);
    id = mReg.registerFunction(mIO, &IOManager::SetCompression,
                               nm + "setUVFCompression", "", false);
    id = mReg.registerFunction(mIO, &IOManager::SetCompressionLevel,
                               nm + "setUVFCompressionLevel", "", false);
    id = mReg.registerFunction(mIO, &IOManager::SetLayout,
                               nm + "setUVFLayout", "Select brick ordering"
                               " on disk", false);
    id = mReg.registerFunction(mIO, &IOManager::ScanDirectory,
                               nm + "scanDirectory", "", false);
    id = mReg.registerFunction(mIO, &IOManager::RegisterFinalConverter,
                               nm + "registerFinalConverter", "", false);
    id = mReg.registerFunction(mIO, &IOManager::RegisterExternalConverter,
                               nm + "registerExternalConverter", "", false);
  }

}

bool LuaIOManagerProxy::ExtractIsosurface(
    LuaClassInstance ds,
    uint64_t iLODlevel, double fIsovalue,
    const FLOATVECTOR4& vfColor,
    const std::wstring& strTargetFilename,
    const std::wstring& strTempDir) const {
  if (mSS->cexecRet<LuaDatasetProxy::DatasetType>(
          ds.fqName() + ".getDSType") != LuaDatasetProxy::UVF) {
    T_ERROR("tuvok.io.exportDataset only accepts UVF.");
    return false;
  }

  // Convert LuaClassInstance -> LuaDatasetProxy -> UVFdataset
  LuaDatasetProxy* dsProxy = ds.getRawPointer<LuaDatasetProxy>(mSS);
  UVFDataset* uvf = dynamic_cast<UVFDataset*>(dsProxy->getDataset());
  assert(uvf != NULL);

  return mIO->ExtractIsosurface(uvf, iLODlevel, fIsovalue,
                                vfColor, strTargetFilename,
                                strTempDir);
}

bool LuaIOManagerProxy::ExtractImageStack(
    LuaClassInstance ds,
    LuaClassInstance tf1d,
    uint64_t iLODlevel, 
    const std::wstring& strTargetFilename,
    const std::wstring& strTempDir,
    bool bAllDirs) const {
  if (mSS->cexecRet<LuaDatasetProxy::DatasetType>(
          ds.fqName() + ".getDSType") != LuaDatasetProxy::UVF) {
    T_ERROR("tuvok.io.exportDataset only accepts UVF.");
    return false;
  }

  // Convert LuaClassInstance -> LuaDatasetProxy -> UVFdataset
  LuaDatasetProxy* dsProxy = ds.getRawPointer<LuaDatasetProxy>(mSS);
  UVFDataset* uvf = dynamic_cast<UVFDataset*>(dsProxy->getDataset());
  assert(uvf != NULL);

  // Now we need to extract the transfer function...
  LuaTransferFun1DProxy* tfProxy = tf1d.getRawPointer<LuaTransferFun1DProxy>(
      mSS);
  TransferFunction1D* pTrans = tfProxy->get1DTransferFunction();
  assert(pTrans != NULL);

  return mIO->ExtractImageStack(
      uvf, pTrans, iLODlevel,
      strTargetFilename,
      strTempDir,
      bAllDirs);
}

bool LuaIOManagerProxy::ExportDataset(LuaClassInstance ds,
                                      uint64_t iLODlevel,
                                      const wstring& strTargetFilename,
                                      const wstring& strTempDir) const
{
  if (mSS->cexecRet<LuaDatasetProxy::DatasetType>(
          ds.fqName() + ".getDSType") != LuaDatasetProxy::UVF) {
    T_ERROR("tuvok.io.exportDataset only accepts UVF.");
    return false;
  }

  // Convert LuaClassInstance -> LuaDatasetProxy -> UVFdataset
  LuaDatasetProxy* dsProxy = ds.getRawPointer<LuaDatasetProxy>(mSS);
  UVFDataset* uvf = dynamic_cast<UVFDataset*>(dsProxy->getDataset());
  assert(uvf != NULL);

  return mIO->ExportDataset(uvf, iLODlevel, strTargetFilename, 
                            strTempDir);
}

bool LuaIOManagerProxy::ExportMesh(shared_ptr<Mesh> mesh,
                                   const wstring& strTargetFilename) const
{
  return mIO->ExportMesh(mesh, strTargetFilename);
}

bool LuaIOManagerProxy::ReBrickDataset(const wstring& strSourceFilename,
                                       const wstring& strTargetFilename,
                                       const wstring& strTempDir) const {
  return mIO->ReBrickDataset(strSourceFilename, strTargetFilename, strTempDir,
                             mIO->GetBuilderBrickSize(),
                             mIO->GetBrickOverlap(), false);
}

bool LuaIOManagerProxy::ConvertDataset(const list<std::wstring>& files,
                                       const wstring& strTargetFilename,
                                       const wstring& strTempDir,
                                       bool bNoUserInteraction,
                                       bool bQuantizeTo8Bit)
{
  return mIO->ConvertDataset(files, strTargetFilename, strTempDir,
                             bNoUserInteraction, bQuantizeTo8Bit);
}

bool LuaIOManagerProxy::ConvertDatasetWithStack(
    shared_ptr<FileStackInfo> stack,
    const wstring& strTargetFilename,
    const wstring& strTempDir,
    bool bQuantizeTo8Bit)
{
  /// @todo Convert 'ConvertDataset' to use shared_ptr instead of raw ptr.
  return mIO->ConvertDataset(stack.get(), strTargetFilename, strTempDir,
                             bQuantizeTo8Bit);
}

std::tuple<bool, RangeInfo> LuaIOManagerProxy::AnalyzeDataset(
    const wstring& strFilename, const wstring& strTempDir)
{
  RangeInfo info;
  bool res = mIO->AnalyzeDataset(strFilename, info, strTempDir);
  return make_tuple(res, info);
}

void LuaIOManagerProxy::evaluateExpression(
    const std::string& expr,
    const std::vector<std::wstring>& volumes,
    const std::wstring& out_fn) const
{
  mIO->EvaluateExpression(expr, volumes, out_fn);
}


} /* namespace tuvok */
