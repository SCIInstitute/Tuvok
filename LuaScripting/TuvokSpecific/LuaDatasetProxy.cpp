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
 \brief A Lua class proxy for IO's dataset class.
 */

#include <vector>

#include "3rdParty/LUA/lua.hpp"
#include "Controller/Controller.h"
#include "IO/DynamicBrickingDS.h"
#include "IO/FileBackedDataset.h"
#include "IO/IOManager.h"
#include "IO/uvfDataset.h"
#include "../LuaClassRegistration.h"
#include "../LuaScripting.h"
#include "LuaDatasetProxy.h"
#include "LuaTuvokTypes.h"

using namespace std;

namespace tuvok
{

namespace Registrar {
void addIOInterface(LuaClassRegistration<Dataset>& reg, Dataset*,
                    LuaScripting*) {
  reg.function(&Dataset::GetLODLevelCount, "LODs", "help?", false);
}
// exposes "IOManager::ExportDataset".
bool exportDS(LuaClassInstance lua_ds, uint64_t LOD,
              const std::string& filename) {
  const LuaDatasetProxy* datasetProxy =
    lua_ds.getRawPointer<LuaDatasetProxy>(Controller::Instance().LuaScript());
  const tuvok::Dataset* ds = datasetProxy->getDataset();
  const IOManager& ioman = Controller::Const().IOMan();
  return ioman.ExportDataset(dynamic_cast<const tuvok::UVFDataset*>(ds),
                             LOD, filename, ".");
}

void dataset(std::shared_ptr<LuaScripting>& ss) {
  LuaDatasetProxy* proxy = new LuaDatasetProxy;
  ss->registerClass<Dataset>(
    proxy, &LuaDatasetProxy::CreateDS, "tuvok.dataset", "creates a new dataset",
    LuaClassRegCallback<Dataset>::Type(addIOInterface)
  );
  delete proxy;

  ss->registerFunction(&exportDS, "tuvok.dataset.export", "exports a DS", true);
}
}

LuaDatasetProxy::LuaDatasetProxy()
    : mReg(NULL)
    , mDS(NULL)
    , mDatasetType(Unknown) { }

LuaDatasetProxy::~LuaDatasetProxy()
{
  delete mReg; mReg = NULL;
  mDS = NULL;
}

Dataset* LuaDatasetProxy::CreateDS(const std::string& uvf, unsigned bricksize) {
  return Controller::Const().IOMan().CreateDataset(uvf,
    uint64_t(bricksize), false
  );
}

void LuaDatasetProxy::bind(Dataset* ds, shared_ptr<LuaScripting> ss)
{
  if (mReg == NULL)
    throw LuaError("Unable to bind dataset, no class registration available.");

  mReg->clearProxyFunctions();

  mDS = ds;
  if (ds != NULL)
  {
    // Register dataset functions using ds.
    string id;

    id = mReg->functionProxy(ds, &Dataset::GetDomainSize,
                             "getDomainSize", "", false);
    id = mReg->functionProxy(ds, &Dataset::GetRange,
                             "getRange", "", false);
    id = mReg->functionProxy(ds, &Dataset::GetLODLevelCount,
                             "getLODLevelCount", "", false);
    id = mReg->functionProxy(ds, &Dataset::GetNumberOfTimesteps,
                             "getNumberOfTimesteps", "", false);
    id = mReg->functionProxy(ds, &Dataset::GetMeshes,
                             "getMeshes", "", false);
    // We do NOT want the return values from GetMeshes stuck in the provenance
    // system (Okay, so the provenance system doesn't store return values, just
    // function parameters. But it's best to be safe).
    ss->setProvenanceExempt(id);
    id = mReg->functionProxy(ds, &Dataset::GetBitWidth,
                             "getBitWidth", "", false);
    id = mReg->functionProxy(ds, &Dataset::Get1DHistogram,
                             "get1DHistogram", "", false);
    id = mReg->functionProxy(ds, &Dataset::Get2DHistogram,
                             "get2DHistogram", "", false);
    id = mReg->functionProxy(ds, &Dataset::SaveRescaleFactors,
                             "saveRescaleFactors", "", false);
    id = mReg->functionProxy(ds, &Dataset::GetRescaleFactors,
                             "getRescaleFactors", "", false);
    id = mReg->functionProxy(ds, &Dataset::Clear,
                             "clear", "clears cache data", false);

    // Attempt to cast the dataset to a file backed dataset.
    FileBackedDataset* fileDataset = dynamic_cast<FileBackedDataset*>(ds);
    if (fileDataset != NULL)
    {
      MESSAGE("Binding extra FileBackedDS functions.");
      id = mReg->functionProxy(fileDataset, &FileBackedDataset::Filename,
                               "fullpath", "Full path to the dataset.", false);
      id = mReg->functionProxy(ds, &FileBackedDataset::Name,
                               "name", "Dataset descriptive name.", false);
    }

    try {
      BrickedDataset& bds = dynamic_cast<BrickedDataset&>(*ds);
      id = mReg->functionProxy(&bds, &BrickedDataset::GetMaxUsedBrickSizes,
                               "maxUsedBrickSize",
                               "the size of the largest brick", false);
    } catch(const std::bad_cast&) {
      WARNING("Not binding BrickedDataset functions.");
    }

    try {
      UVFDataset& uvfDataset = dynamic_cast<UVFDataset&>(*ds);
      MESSAGE("Binding extra UVF functions.");
      mDatasetType = UVF;
      mReg->functionProxy(&uvfDataset, &UVFDataset::RemoveMesh, "removeMesh",
                          "", true);
      mReg->functionProxy(&uvfDataset, &UVFDataset::AppendMesh,
                          "appendMesh", "", false);
      id = mReg->functionProxy(&uvfDataset,
        &UVFDataset::GeometryTransformToFile, "geomTransformToFile", "",
        false
      );
      ss->setProvenanceExempt(id);
    } catch(const std::bad_cast&) {
      WARNING("Not a uvf; not binding mesh functions.");
    }

    try {
      DynamicBrickingDS& dynDS = dynamic_cast<DynamicBrickingDS&>(*ds);
      MESSAGE("Binding dynamic bricking cache control functions");
      id = mReg->functionProxy(&dynDS, &DynamicBrickingDS::SetCacheSize,
                               "setCacheSize",
                               "sets the size of the cache, in megabytes.",
                               false);
      ss->addParamInfo(id, 0, "cacheMB", "cache size (megabytes)");
    } catch(const std::bad_cast&) {
      MESSAGE("Not dynamically bricked; not adding cache control functions.");
    }

    /// @todo Expose 1D/2D histogram? Currently, it is being transfered
    ///       via shared_ptr. If lua wants to interpret this, the histogram
    ///       will need to be placed in terms that lua can understand.
    ///       Two approaches:
    ///       1) Add Grid1D to the LuaStrictStack.
    ///       2) Create a Histogram1D and Histogram2D proxy.
    ///       
    ///       The second solution would be more efficient, since there wouldn't
    ///       be any time spent converting datatypes to and from Lua (and with
    ///       histograms, that time wouldn't be negligible).
  }

}

void LuaDatasetProxy::defineLuaInterface(
    LuaClassRegistration<LuaDatasetProxy>& reg,
    LuaDatasetProxy* me,
    LuaScripting* ss)
{
  me->mReg = new LuaClassRegistration<LuaDatasetProxy>(reg);

  string id;

  // Register our functions
  id = reg.function(&LuaDatasetProxy::getDatasetType, "getDSType", "", false);
  id = reg.function(&LuaDatasetProxy::proxyGetMetadata, "getMetadata", "",
                    false);

  lua_State* L = ss->getLuaState();
  lua_pushinteger(L, DynamicBrickingDS::MM_SOURCE);
  lua_setglobal(L, "MM_SOURCE");
  lua_pushinteger(L, DynamicBrickingDS::MM_PRECOMPUTE);
  lua_setglobal(L, "MM_PRECOMPUTE");
  lua_pushinteger(L, DynamicBrickingDS::MM_DYNAMIC);
  lua_setglobal(L, "MM_DYNAMIC");
}


std::vector<std::pair<std::string, std::string>>
LuaDatasetProxy::proxyGetMetadata()
{
  return mDS->GetMetadata();
}

} /* namespace tuvok */
