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

//!    File   : MasterController.cpp
//!    Author : Jens Krueger
//!             SCI Institute
//!             University of Utah
//!    Date   : September 2008
//
//!    Copyright (C) 2008 SCI Institute

#include <algorithm>
#include <sstream>
#include <functional>
#include "MasterController.h"
#include "../Basics/SystemInfo.h"
#include "../IO/IOManager.h"
#include "../IO/TransferFunction1D.h"
#include "../IO/Dataset.h"
#include "../IO/AbstrConverter.h"
#include "../Renderer/GPUMemMan/GPUMemMan.h"
#include "../Renderer/GL/GLRaycaster.h"
#include "../Renderer/GL/GLGridLeaper.h"
#include "../Renderer/GL/GLSBVR.h"
#include "../Renderer/GL/GLSBVR2D.h"

#include "../LuaScripting/LuaScripting.h"
#include "../LuaScripting/LuaMemberReg.h"
#include "../LuaScripting/TuvokSpecific/LuaTuvokTypes.h"
#include "../LuaScripting/TuvokSpecific/LuaDatasetProxy.h"
#include "../LuaScripting/TuvokSpecific/LuaTransferFun1DProxy.h"
#include "../LuaScripting/TuvokSpecific/LuaTransferFun2DProxy.h"
#include "../LuaScripting/TuvokSpecific/LuaIOManagerProxy.h"
#include "../LuaScripting/TuvokSpecific/MatrixMath.h"

using namespace tuvok;

MasterController::MasterController() :
  m_bDeleteDebugOutOnExit(false),
  m_bExperimentalFeatures(false),
  m_pLuaScript(new LuaScripting()),
  m_pMemReg(new LuaMemberReg(m_pLuaScript)),
  m_pActiveRenderer(NULL)
{
  m_pSystemInfo   = new SystemInfo();
  m_pIOManager    = new IOManager();
  m_pGPUMemMan    = new GPUMemMan(this);

  using namespace std::placeholders;
  std::function<Dataset*(const std::string&, AbstrRenderer*)> f =
    std::bind(&GPUMemMan::LoadDataset, m_pGPUMemMan, _1, _2);
  m_pIOManager->SetMemManLoadFunction(f);

  RegisterLuaCommands();

  // Register IO commands.
  m_pIOProxy = std::unique_ptr<LuaIOManagerProxy>(
      new LuaIOManagerProxy(m_pIOManager, m_pLuaScript));

  // TEMPORARY -- Disable the provenance system.
  LuaScript()->cexec("provenance.enable", false);

  PHState.BStrategy = PH_HackyState::BS_SkipTwoLevels;
  std::fill(m_Perf, m_Perf+PERF_END, 0.0);
}


MasterController::~MasterController() {
  Cleanup();
  m_DebugOut.clear();
}

void MasterController::Cleanup() {
  std::for_each(m_vVolumeRenderer.begin(), m_vVolumeRenderer.end(),
                [](AbstrRenderer* i) { delete i; });
  m_vVolumeRenderer.clear();
  delete m_pSystemInfo;
  m_pSystemInfo = NULL;
  delete m_pIOManager;
  m_pIOManager = NULL;
  delete m_pGPUMemMan;
  m_pGPUMemMan = NULL;
  m_pActiveRenderer = NULL;
}


void MasterController::AddDebugOut(AbstrDebugOut* debugOut) {
  if (debugOut != NULL) {
    m_DebugOut.Other(_func_, "Disconnecting from this debug out");

    m_DebugOut.AddDebugOut(debugOut);

    debugOut->Other(_func_, "Connected to this debug out");
  } else {
    m_DebugOut.Warning(_func_,
                       "New debug is a NULL pointer, ignoring it.");
  }
}


void MasterController::RemoveDebugOut(AbstrDebugOut* debugOut) {
  m_DebugOut.RemoveDebugOut(debugOut);
}

/// Access the currently-active debug stream.
AbstrDebugOut* MasterController::DebugOut()
{
  return (m_DebugOut.empty()) ? static_cast<AbstrDebugOut*>(&m_DefaultOut)
                                  : static_cast<AbstrDebugOut*>(&m_DebugOut);
}
const AbstrDebugOut *MasterController::DebugOut() const {
  return (m_DebugOut.empty())
           ? static_cast<const AbstrDebugOut*>(&m_DefaultOut)
           : static_cast<const AbstrDebugOut*>(&m_DebugOut);
}


AbstrRenderer*
MasterController::RequestNewVolumeRenderer(EVolumeRendererType eRendererType,
                                           bool bUseOnlyPowerOfTwo,
                                           bool bDownSampleTo8Bits,
                                           bool bDisableBorder,
                                           bool bBiasAndScaleTF) {
  std::string api;
  std::string method;
  AbstrRenderer *retval;

  switch (eRendererType) {
  case OPENGL_SBVR :
    api = "OpenGL";
    method = "Slice-Based Volume Renderer";
    retval = new GLSBVR(this,
                        bUseOnlyPowerOfTwo,
                        bDownSampleTo8Bits,
                        bDisableBorder);
    break;

  case OPENGL_CHOOSE: // we just need to make something up, will fix later
  case OPENGL_2DSBVR:
    api = "OpenGL";
    method = "Axis-Aligned 2D Slice-Based Volume Renderer";
    retval = new GLSBVR2D(this,
                          bUseOnlyPowerOfTwo,
                          bDownSampleTo8Bits,
                          bDisableBorder);
    break;

  case OPENGL_RAYCASTER :
    api = "OpenGL";
    method = "Raycaster";
    retval = new GLRaycaster(this,
                             bUseOnlyPowerOfTwo,
                             bDownSampleTo8Bits,
                             bDisableBorder);
    break;
  case OPENGL_GRIDLEAPER :
    api = "OpenGL";
    method = "Grid Leaper";
    retval = new GLGridLeaper(this,
                             bUseOnlyPowerOfTwo,
                             bDownSampleTo8Bits,
                             bDisableBorder);
    break;

  case DIRECTX_RAYCASTER :
  case DIRECTX_2DSBVR :
  case DIRECTX_SBVR :
  case DIRECTX_GRIDLEAPER:
    m_DebugOut.Error(_func_,"DirectX 10 renderer not yet implemented."
                            "Please select OpenGL as the render API "
                            "in the settings dialog.");
    return NULL;

  default :
    m_DebugOut.Error(_func_, "Unsupported Volume renderer requested");
    return NULL;
  };

  m_DebugOut.Message(_func_, "Starting up new renderer (API=%s, Method=%s)",
                     api.c_str(), method.c_str());
  if(bBiasAndScaleTF) {
    retval->SetScalingMethod(AbstrRenderer::SMETH_BIAS_AND_SCALE);
  }
  m_vVolumeRenderer.push_back(retval);
  return m_vVolumeRenderer[m_vVolumeRenderer.size()-1];
}

void MasterController::ReleaseVolumeRenderer(LuaClassInstance pRenderer)
{
  ReleaseVolumeRenderer(pRenderer.getRawPointer<AbstrRenderer>(LuaScript()));
}

void MasterController::ReleaseVolumeRenderer(AbstrRenderer* pVolumeRenderer) {
  // Note: Even if this function is called from deleteClass inside lua, it is
  // still safe to check whether the pointer exists inside of lua. The pointer
  // in the pointer lookup table is always removed before deletion of the
  // pointer itself.
  LuaClassInstance inst;
  try {
    inst = LuaScript()->getLuaClassInstance(pVolumeRenderer);
  }
  catch (LuaNonExistantClassInstancePointer&) {
    // Ignore it. This more than likely means we are inside of the class'
    // destructor.
  }

  bool foundRenderer = false;

  for(auto i = m_vVolumeRenderer.begin(); i != m_vVolumeRenderer.end(); ++i) {
    if (*i == pVolumeRenderer) {
      foundRenderer = true;
      m_DebugOut.Message(_func_, "Removing volume renderer");
      m_vVolumeRenderer.erase(i);
      break;
    }
  }

  // Only warn if we did find the instance in lua, but didn't find it in our
  // abstract renderer list.
  if (foundRenderer == false && inst.isValid(LuaScript()))
    m_DebugOut.Warning(_func_, "requested volume renderer not found");

  // Delete the class if it hasn't been already. This covers the case where
  // the class calls ReleaseVolumeRenderer instead of calling deleteClass.
  if (inst.isValid(LuaScript()))
    LuaScript()->cexec("deleteClass", inst);
}


RenderRegion* MasterController::LuaCreateRenderRegion3D(LuaClassInstance ren) {
  return new RenderRegion3D(ren.getRawPointer<AbstrRenderer>(LuaScript()));
}

RenderRegion* MasterController::LuaCreateRenderRegion2D(
    int mode,
    uint64_t sliceIndex,
    LuaClassInstance ren) {
  return new RenderRegion2D(static_cast<RenderRegion::EWindowMode>(mode),
                            sliceIndex,
                            ren.getRawPointer<AbstrRenderer>(LuaScript()));
}

void MasterController::AddLuaRendererType(const std::string& rendererLoc,
                                          const std::string& rendererName,
                                          int value) {
  std::ostringstream os;
  os << rendererLoc << ".types." << rendererName << "=" << value;
  LuaScript()->exec(os.str());
}

void MasterController::SetBrickStrategy(size_t strat) {
  assert(strat <= PH_HackyState::BS_SkipTwoLevels); // hack
  this->PHState.BStrategy = static_cast<PH_HackyState::BrickStrategy>(strat);
}
void MasterController::SetRehashCount(uint32_t n) {
  this->PHState.RehashCount = n;
}

double MasterController::PerfQuery(enum PerfCounter pc) {
  assert(pc < PERF_END_RENDER);
  double tmp = m_Perf[pc];
  m_Perf[pc] = 0.0;
  return tmp;
}
void MasterController::IncrementPerfCounter(enum PerfCounter pc,
                                            double amount) {
  assert(pc < PERF_END_RENDER);
  m_Perf[pc] += amount;
}

void MasterController::SetMaxGPUMem(uint64_t megs) {
  const uint64_t megabyte = 1024 * 1024;
  m_pSystemInfo->SetMaxUsableGPUMem(megabyte * megs);
  m_pGPUMemMan->MemSizesChanged();
}

void MasterController::SetMaxCPUMem(uint64_t megs) {
  const uint64_t megabyte = 1024 * 1024;
  m_pSystemInfo->SetMaxUsableCPUMem(megabyte * megs);
  m_pGPUMemMan->MemSizesChanged();
}

void register_unsigned(lua_State* lua, const char* name, unsigned value) {
  lua_pushinteger(lua, value);
  lua_setglobal(lua, name);
}

void register_perf_enum(std::shared_ptr<LuaScripting>& ss) {
  lua_State* lua = ss->getLuaState();
  register_unsigned(lua, "PERF_DISK_READ", PERF_DISK_READ);
  register_unsigned(lua, "PERF_DECOMPRESSION", PERF_DECOMPRESSION);
  register_unsigned(lua, "PERF_COMPRESSION", PERF_COMPRESSION);
  register_unsigned(lua, "PERF_BRICKS", PERF_BRICKS);
  register_unsigned(lua, "PERF_BRICK_COPY", PERF_BRICK_COPY);
  register_unsigned(lua, "PERF_MM_PRECOMPUTE", PERF_MM_PRECOMPUTE);
  register_unsigned(lua, "PERF_READ_HTABLE", PERF_READ_HTABLE);
  register_unsigned(lua, "PERF_CONDENSE_HTABLE", PERF_CONDENSE_HTABLE);
  register_unsigned(lua, "PERF_RENDER", PERF_RENDER);
  register_unsigned(lua, "PERF_UPLOAD_BRICKS", PERF_UPLOAD_BRICKS);
}

void MasterController::RegisterLuaCommands() {
  std::shared_ptr<LuaScripting> ss = LuaScript();

  // Register volume renderer class.
  std::string renderer = "tuvok.renderer";
  ss->registerClass<AbstrRenderer>(
      this,
      &MasterController::RequestNewVolumeRenderer,
      renderer,
      "Constructs a new renderer. The first parameter is one "
      "of the values in the tuvok.renderer.types table.",
      LuaClassRegCallback<AbstrRenderer>::Type(
          AbstrRenderer::RegisterLuaFunctions));

  tuvok::Registrar::dataset(ss);

  // Populate the tuvok.renderer.type table.
  ss->exec(renderer + ".types = {}");

  AddLuaRendererType(renderer, "OpenGL_SVBR", OPENGL_SBVR);
  AddLuaRendererType(renderer, "OpenGL_2DSBVR", OPENGL_2DSBVR);
  AddLuaRendererType(renderer, "OpenGL_Raycaster", OPENGL_RAYCASTER);
  AddLuaRendererType(renderer, "OpenGL_GridLeaper", OPENGL_GRIDLEAPER);

  AddLuaRendererType(renderer, "DirectX_SVBR", DIRECTX_SBVR);
  AddLuaRendererType(renderer, "DirectX_2DSBVR", DIRECTX_2DSBVR);
  AddLuaRendererType(renderer, "DirectX_Raycaster", DIRECTX_RAYCASTER);
  AddLuaRendererType(renderer, "DirectX_GridLeaper", DIRECTX_GRIDLEAPER);

  AddLuaRendererType(renderer, "RT_Interactive", AbstrRenderer::RT_INTERACTIVE);
  AddLuaRendererType(renderer, "RT_Capture", AbstrRenderer::RT_CAPTURE);
  AddLuaRendererType(renderer, "RT_Headless", AbstrRenderer::RT_HEADLESS);

  // Register RenderRegion3D.
  ss->registerClass<RenderRegion>(this,
      &MasterController::LuaCreateRenderRegion3D,
      "tuvok.renderRegion3D",
      "Constructs a 3D render region.",
      LuaClassRegCallback<RenderRegion>::Type(
          RenderRegion::defineLuaInterface));

  // Register RenderRegion2D.
  ss->registerClass<RenderRegion>(this,
      &MasterController::LuaCreateRenderRegion2D,
      "tuvok.renderRegion2D",
      "Constructs a 2D render region.",
      LuaClassRegCallback<RenderRegion>::Type(
          RenderRegion::defineLuaInterface));
  ss->addParamInfo("tuvok.renderRegion2D.new", 0, "mode",
                   "Specifies viewing axis.");
  ss->addParamInfo("tuvok.renderRegion2D.new", 1, "sliceIndex",
                   "Index of slice to view.");

  // Register dataset proxy
  ss->registerClassStatic<LuaDatasetProxy>(
      &LuaDatasetProxy::luaConstruct,
      "tuvok.datasetProxy",
      "Constructs a dataset proxy.",
      LuaClassRegCallback<LuaDatasetProxy>::Type(
          LuaDatasetProxy::defineLuaInterface));

  // Register 1D transfer function proxy
  ss->registerClassStatic<LuaTransferFun1DProxy>(
      &LuaTransferFun1DProxy::luaConstruct,
      "tuvok.transferFun1D",
      "Constructs a 1D transfer function proxy. Construction of these proxies "
      "should be left to the renderer.",
      LuaClassRegCallback<LuaTransferFun1DProxy>::Type(
          LuaTransferFun1DProxy::defineLuaInterface));

  // Register 2D transfer function proxy
  ss->registerClassStatic<LuaTransferFun2DProxy>(
      &LuaTransferFun2DProxy::luaConstruct,
      "tuvok.transferFun2D",
      "Constructs a 2D transfer function proxy. Construction of these proxies "
      "should be left to the renderer.",
      LuaClassRegCallback<LuaTransferFun2DProxy>::Type(
          LuaTransferFun2DProxy::defineLuaInterface));

  // register matrix math functions.
  tuvok::registrar::matrix_math(ss);

  m_pMemReg->registerFunction(this,
    &MasterController::SetBrickStrategy, "tuvok.state.brickStrategy", "",
    false
  );
  m_pMemReg->registerFunction(this,
    &MasterController::SetRehashCount, "tuvok.state.rehashCount", "",
    false
  );
  m_pMemReg->registerFunction(this,
    &MasterController::SetMaxGPUMem, "tuvok.state.gpuMem",
    "sets a new max amount of GPU memory.  In megabytes.", false
  );
  m_pMemReg->registerFunction(this,
    &MasterController::SetMaxCPUMem, "tuvok.state.cpuMem",
    "sets a new max amount of CPU memory.  In megabytes.", false
  );

  m_pMemReg->registerFunction(this,
    &MasterController::PerfQuery, "tuvok.perf",
    "queries performance information.  meaning is query-specific.", false
  );
  register_perf_enum(ss);

  // Register Tuvok specific math functions...
  LuaMathFunctions::registerMathFunctions(ss);
}

bool MasterController::ExperimentalFeatures() const {
  return m_bExperimentalFeatures;
}

void MasterController::ExperimentalFeatures(bool b) {
  m_bExperimentalFeatures = b;
}
