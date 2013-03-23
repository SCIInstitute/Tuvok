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
  \file    AbstrRenderer.cpp
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    August 2008
*/

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>
#include "Basics/MathTools.h"
#include "Basics/GeometryGenerator.h"
#include "IO/Tuvok_QtPlugins.h"
#include "IO/IOManager.h"
#include "IO/TransferFunction1D.h"
#include "IO/TransferFunction2D.h"
#include "AbstrRenderer.h"
#include "Controller/Controller.h"
#include "LuaScripting/LuaScripting.h"
#include "LuaScripting/LuaClassInstance.h"
#include "LuaScripting/TuvokSpecific/LuaTuvokTypes.h"
#include "LuaScripting/TuvokSpecific/LuaDatasetProxy.h"
#include "LuaScripting/TuvokSpecific/LuaTransferFun1DProxy.h"
#include "LuaScripting/TuvokSpecific/LuaTransferFun2DProxy.h"
#include "Renderer/GPUMemMan/GPUMemMan.h"
#include "RenderMesh.h"
#include "ShaderDescriptor.h"

using namespace std;
using namespace tuvok;

static const FLOATVECTOR3 s_vEye(0,0,1.6f);
static const FLOATVECTOR3 s_vAt(0,0,0);
static const FLOATVECTOR3 s_vUp(0,1,0);
static const float s_fFOV = 50.0f;
static const float s_fZNear = 0.01f;
static const float s_fZFar = 1000.0f;

AbstrRenderer::AbstrRenderer(MasterController* pMasterController,
                             bool bUseOnlyPowerOfTwo,
                             bool bDownSampleTo8Bits,
                             bool bDisableBorder,
                             enum ScalingMethod sm) :
  m_pMasterController(pMasterController),
  m_eRenderMode(RM_1DTRANS),
  m_bFirstDrawAfterModeChange(true),
  m_bFirstDrawAfterResize(true),
  m_eBlendPrecision(BP_32BIT),
  m_bUseLighting(true),
  m_pDataset(NULL),
  m_p1DTrans(NULL),
  m_p2DTrans(NULL),
  m_fSampleRateModifier(1.0f),
  m_vIsoColor(0.5,0.5,0.5),
  m_vTextColor(1,1,1,1),
  m_bRenderGlobalBBox(false),
  m_bRenderLocalBBox(false),
  m_vWinSize(0,0),
  m_iLogoPos(3),
  m_strLogoFilename(""),
  m_iDebugView(0),
  m_eInterpolant(Linear),
  m_bSupportsMeshes(false),
  msecPassedCurrentFrame(-1.0f),
  m_iLODNotOKCounter(0),
  decreaseScreenRes(false),
  decreaseScreenResNow(false),
  decreaseSamplingRate(false),
  decreaseSamplingRateNow(false),
  doAnotherRedrawDueToLowResOutput(false),
  m_fMaxMSPerFrame(10000),
  m_fScreenResDecFactor(2.0f),
  m_fSampleDecFactor(2.0f),
  m_bRenderLowResIntermediateResults(false),
  m_bOffscreenIsLowRes(false),
  m_iStartDelay(1000),
  m_iMinLODForCurrentView(0),
  m_iTimeSliceMSecs(100),
  m_iIntraFrameCounter(0),
  m_iFrameCounter(0),
  m_iCheckCounter(0),
  m_iMaxLODIndex(0),
  m_iLODLimits(0,0),
  m_iPerformanceBasedLODSkip(0),
  m_iCurrentLODOffset(0),
  m_iStartLODOffset(0),
  m_iTimestep(0),
  m_bClearFramebuffer(true),
  m_bConsiderPreviousDepthbuffer(true),
  m_iCurrentLOD(0),
  m_iBricksRenderedInThisSubFrame(0),
  m_eRendererTarget(RT_INTERACTIVE),
  m_bMIPLOD(true),
  m_fMIPRotationAngle(0.0f),
  m_bOrthoView(false),
  m_bRenderCoordArrows(false),
  m_bRenderPlanesIn3D(false),
  m_bDoClearView(false),
  m_vCVColor(1,0,0),
  m_fCVSize(5.5f),
  m_fCVContextScale(1.0f),
  m_fCVBorderScale(60.0f),
  m_vCVMousePos(0, 0),
  m_vCVPos(0,0,0.5f,1.0f),
  m_bPerformReCompose(false),
  m_bRequestStereoRendering(false),
  m_bDoStereoRendering(false),
  m_bUserMatrices(false),
  m_iAlternatingFrameID(0),
  m_fStereoEyeDist(0.02f),
  m_fStereoFocalLength(1.0f),
  m_eStereoMode(SM_RB),
  m_bStereoEyeSwap(false),
  m_bDatasetIsInvalid(false),
  m_bUseOnlyPowerOfTwo(bUseOnlyPowerOfTwo),
  m_bDownSampleTo8Bits(bDownSampleTo8Bits),
  m_bDisableBorder(bDisableBorder),
  m_TFScalingMethod(sm),
  m_bClipPlaneOn(false),
  m_bClipPlaneDisplayed(true),
  m_bClipPlaneLocked(true),
  m_vEye(s_vEye),
  m_vAt(s_vAt),
  m_vUp(s_vUp),
  m_fFOV(s_fFOV),
  m_fZNear(s_fZNear),
  m_fZFar(s_fZFar),
  m_bFirstPersonMode(false),
  simpleRenderRegion3D(this),
  m_cAmbient(1.0f,1.0f,1.0f,0.1f),
  m_cDiffuse(1.0f,1.0f,1.0f,1.0f),
  m_cSpecular(1.0f,1.0f,1.0f,1.0f),
  m_cAmbientM(1.0f,1.0f,1.0f,0.1f),
  m_cDiffuseM(1.0f,1.0f,1.0f,1.0f),
  m_cSpecularM(1.0f,1.0f,1.0f,1.0f),
  m_vLightDir(0.0f,0.0f,-1.0f),
  m_fIsovalue(0.5f),
  m_fCVIsovalue(0.8f)
{
  m_vBackgroundColors[0] = FLOATVECTOR3(0,0,0);
  m_vBackgroundColors[1] = FLOATVECTOR3(0,0,0);

  simpleRenderRegion3D.minCoord = UINTVECTOR2(0,0); // maxCoord is updated in Paint().
  renderRegions.push_back(&simpleRenderRegion3D);

  RestartTimers();

  m_vShaderSearchDirs.push_back("Shaders");
  m_vShaderSearchDirs.push_back("Tuvok/Shaders");
  m_vShaderSearchDirs.push_back("../Tuvok/Shaders");
  // trim out directories which are nonsense.  Gets rid of some warnings.
  m_vShaderSearchDirs = ShaderDescriptor::ValidPaths(m_vShaderSearchDirs);
  m_vArrowGeometry = GeometryGenerator::GenArrow(0.3f,0.8f,0.006f,0.012f,20);

  // Create our dataset proxy.
  m_pLuaDataset =
      m_pMasterController->LuaScript()->cexecRet<LuaClassInstance>(
      "tuvok.datasetProxy.new");
  m_pLuaDatasetPtr = m_pLuaDataset.getRawPointer<LuaDatasetProxy>(
      m_pMasterController->LuaScript());

  // Create our 1D transfer function proxy.
  m_pLua1DTrans =
      m_pMasterController->LuaScript()->cexecRet<LuaClassInstance>(
          "tuvok.transferFun1D.new");
  m_pLua1DTransPtr = m_pLua1DTrans.getRawPointer<LuaTransferFun1DProxy>(
      m_pMasterController->LuaScript());

  // Create our 2D transfer function proxy.
  m_pLua2DTrans =
      m_pMasterController->LuaScript()->cexecRet<LuaClassInstance>(
          "tuvok.transferFun2D.new");
  m_pLua2DTransPtr = m_pLua2DTrans.getRawPointer<LuaTransferFun2DProxy>(
      m_pMasterController->LuaScript());
}

bool AbstrRenderer::Initialize(std::shared_ptr<Context> ctx) {
  m_pContext = ctx;
  return m_pDataset != NULL;
}

bool AbstrRenderer::LoadDataset(const string& strFilename) {
  if (m_pMasterController == NULL) return false;

  if (m_pMasterController->IOMan() == NULL) {
    T_ERROR("Cannot load dataset because IOManager is NULL");
    return false;
  }

  if (m_pDataset)
  {
    m_pMasterController->MemMan()->FreeDataset(m_pDataset, this);
    m_pLuaDatasetPtr->bind(NULL, m_pMasterController->LuaScript());
  }

  m_pDataset = m_pMasterController->IOMan()->LoadDataset(strFilename,this);
  if (m_pDataset == NULL) {
    T_ERROR("IOManager call to load dataset failed.");
    return false;
  }
  m_pLuaDatasetPtr->bind(m_pDataset, m_pMasterController->LuaScript());

  MESSAGE("Load successful, initializing renderer!");

  // find the maximum LOD index
  m_iMaxLODIndex = m_pDataset->GetLargestSingleBrickLOD(0);

  // now that we know the range of the dataset, we can set the default
  // isoval to half the range.  For CV, we'll set the isovals to a bit above
  // the context isoval; with any luck, it'll make a valid image right off the
  // bat.
  std::pair<double,double> rng = m_pDataset->GetRange();
  // It can happen that we don't know the range; old UVFs, for example.  We'll
  // know this because the minimum will be g.t. the maximum.
  if(rng.first > rng.second) {
    m_fIsovalue = float(rng.second) / 2.0f;
  } else {
    m_fIsovalue = float(rng.second-rng.first) / 2.0f;
  }
  m_fCVIsovalue = m_fIsovalue + (m_fIsovalue/2.0f);

  return true;
}

bool AbstrRenderer::LoadRebricked(const std::string& filename,
                                  const UINTVECTOR3 bsize,
                                  size_t minmaxMode) {
  const IOManager& iomgr = Controller::Const().IOMan();
  Dataset* ds = iomgr.LoadRebrickedDataset(filename, bsize, minmaxMode);
  m_pLuaDatasetPtr->bind(ds, m_pMasterController->LuaScript());
  this->SetDataset(ds);
  return true;
}

AbstrRenderer::~AbstrRenderer() {
  if (m_pDataset) m_pMasterController->MemMan()->FreeDataset(m_pDataset, this);
  if (m_p1DTrans) m_pMasterController->MemMan()->Free1DTrans(m_p1DTrans, this);
  if (m_p2DTrans) m_pMasterController->MemMan()->Free2DTrans(m_p2DTrans, this);
  // Ensure the master controller has remove this abstract renderer from its
  // list.
  m_pMasterController->ReleaseVolumeRenderer(this);

  // Kill the dataset proxy.
  m_pMasterController->LuaScript()->cexecRet<LuaClassInstance>(
      "deleteClass", m_pLuaDataset);

  // Kill the transfer function 1d proxy.
  m_pMasterController->LuaScript()->cexecRet<LuaClassInstance>(
      "deleteClass", m_pLua1DTrans);

  // Kill the transfer function 2d proxy.
  m_pMasterController->LuaScript()->cexecRet<LuaClassInstance>(
      "deleteClass", m_pLua2DTrans);
}

void AbstrRenderer::SetRendermode(ERenderMode eRenderMode)
{
  if (m_eRenderMode != eRenderMode) {
    m_eRenderMode = eRenderMode;
    m_bFirstDrawAfterModeChange = true;
    ScheduleCompleteRedraw();
  }
}

void AbstrRenderer::SetUseLighting(bool bUseLighting) {
  if (m_bUseLighting != bUseLighting) {
    m_bUseLighting = bUseLighting;
    Schedule3DWindowRedraws();
  }
}

void AbstrRenderer::SetBlendPrecision(EBlendPrecision eBlendPrecision) {
  if (m_eBlendPrecision != eBlendPrecision) {
    m_eBlendPrecision = eBlendPrecision;
    ScheduleCompleteRedraw();
  }
}

/// @todo fixme: this and ::LoadDataset should coexist better: the latter
/// should probably call this method, instead of duplicating stuff.
void AbstrRenderer::SetDataset(Dataset *vds) {
  if(m_pDataset) {
    Controller::Instance().MemMan()->FreeDataset(m_pDataset, this);
  }
  m_pDataset = vds;
  m_iMaxLODIndex = m_pDataset->GetLargestSingleBrickLOD(0);
  Controller::Instance().MemMan()->AddDataset(m_pDataset, this);
  ScheduleCompleteRedraw();
}

void AbstrRenderer::Free1DTrans() {
  GPUMemMan& mm = *(Controller::Instance().MemMan());
  mm.Free1DTrans(m_p1DTrans, this);
  m_pLua1DTransPtr->bind(NULL);
}

void AbstrRenderer::LuaBindNew2DTrans() {
  m_pLua2DTransPtr->bind(m_p2DTrans);
}

void AbstrRenderer::LuaBindNew1DTrans() {
  m_pLua1DTransPtr->bind(m_p1DTrans);
}

void AbstrRenderer::Changed1DTrans() {
  AbstrDebugOut *dbg = m_pMasterController->DebugOut();
  if (m_eRenderMode != RM_1DTRANS) {
    dbg->Message(_func_,
                 "not using the 1D transferfunction at the moment, "
                 "ignoring message");
  } else {
    dbg->Message(_func_, "complete redraw scheduled");
    ScheduleCompleteRedraw();
    // No provenance; as a mechanism to filter out too many updates, we place
    // the onus on updating this in the UI which is driving us.
  }
}

void AbstrRenderer::Changed2DTrans() {
  if (m_eRenderMode != RM_2DTRANS) {
    MESSAGE("not currently using 2D tfqn; ignoring message.");
  } else {
    ScheduleCompleteRedraw();
    // No provenance; handled by application, not Tuvok lib.
  }
}

void AbstrRenderer::SetFoV(float fFoV) {
  if(m_fFOV != fFoV) {
    m_fFOV = fFoV;
    Schedule3DWindowRedraws();
  }
}

void AbstrRenderer::SetSampleRateModifier(float fSampleRateModifier) {
  if(m_fSampleRateModifier != fSampleRateModifier) {
    m_fSampleRateModifier = fSampleRateModifier;
    Schedule3DWindowRedraws();
  }
}

void AbstrRenderer::SetIsoValue(float fIsovalue) {
  if(fIsovalue != m_fIsovalue) {
    m_fIsovalue = fIsovalue;
    Schedule3DWindowRedraws();
  }
}

/// The given isoval is interpreted relative to the min/max of the range of the
/// data values: 0 is the min and 1 is the max.
void AbstrRenderer::SetIsoValueRelative(float isorel) {
  // can't assert a proper range, but we can at least prevent idiots from
  // calling 'SetIsoValueRelative(255)'.
  assert(-0.01 <= isorel && isorel <= 1.01);
  // clamp it to [0,1]
  isorel = std::max(0.0f, isorel);
  isorel = std::min(1.0f, isorel);

  // grab the range and lerp the given isoval to that range
  std::pair<double,double> minmax = m_pDataset->GetRange();
  // we might not know the range; use the bit width if so.
  if(minmax.second <= minmax.first) {
    minmax.first = 0;
    minmax.second = std::pow(2.0,
                             static_cast<double>(m_pDataset->GetBitWidth()));
  }
  float newiso = MathTools::lerp<float,float>(isorel, 0.0f, 1.0f,
                                              minmax.first, minmax.second);
  this->SetIsoValue(newiso);
}

double AbstrRenderer::GetNormalizedIsovalue() const
{
  if(m_pDataset->GetBitWidth() != 8 && m_bDownSampleTo8Bits) {
    double mx;
    if(m_pDataset->GetRange().first > m_pDataset->GetRange().second) {
      mx = double(m_p1DTrans->GetSize());
    } else {
      mx = m_pDataset->GetRange().second;
    }
    return MathTools::lerp(m_fIsovalue, 0.f,static_cast<float>(mx), 0.f,1.f);
  }
  return m_fIsovalue / (1 << m_pDataset->GetBitWidth());
}

double AbstrRenderer::GetNormalizedCVIsovalue() const
{
  if(m_pDataset->GetBitWidth() != 8 && m_bDownSampleTo8Bits) {
    double mx;
    if(m_pDataset->GetRange().first > m_pDataset->GetRange().second) {
      mx = double(m_p1DTrans->GetSize());
    } else {
      mx = m_pDataset->GetRange().second;
    }
    return MathTools::lerp(m_fCVIsovalue, 0.f,static_cast<float>(mx), 0.f,1.f);
  }
  return m_fCVIsovalue / (1 << m_pDataset->GetBitWidth());
}

bool AbstrRenderer::CheckForRedraw() {
  if (m_vWinSize.area() == 0)
    return false; // can't draw to a size zero window.

  bool decrementCounter = false;
  bool redrawRequired = false;
  redrawRequired = m_bPerformReCompose;

  for (size_t i=0; i < renderRegions.size(); ++i) {
    const RenderRegion* region = renderRegions[i];
    // need to redraw for 1 of three reasons:
    //   didn't finish last paint call; bricks remain.
    //   haven't rendered the finest LOD for the current view
    //   last draw was low res or sample rate for interactivity
    if (m_vCurrentBrickList.size() > m_iBricksRenderedInThisSubFrame ||
        m_iCurrentLODOffset > m_iMinLODForCurrentView ||
        this->doAnotherRedrawDueToLowResOutput) {
      if (m_iCheckCounter == 0 || m_eRendererTarget != RT_INTERACTIVE) {
        MESSAGE("Still drawing...");
        return true;
      } else {
        decrementCounter = true;
      }
    }
    // region is completely blank?
    redrawRequired |= region->isBlank;
  }
  /// @todo Is this logic for how/when to decrement correct?
  if (decrementCounter)
    m_iCheckCounter--;

  return redrawRequired;
}

void AbstrRenderer::Resize(const UINTVECTOR2& vWinSize) {
  m_vWinSize = vWinSize;
  m_bFirstDrawAfterResize = true;
  ScheduleCompleteRedraw();
}

RenderRegion3D* AbstrRenderer::GetFirst3DRegion() {
  for (size_t i=0; i < renderRegions.size(); ++i) {
    if (renderRegions[i]->is3D())
      return dynamic_cast<RenderRegion3D*>(renderRegions[i]);
  }
  return NULL;
}

LuaClassInstance AbstrRenderer::LuaGetFirst3DRegion() {
  return m_pMasterController->LuaScript()->getLuaClassInstance(
      dynamic_cast<RenderRegion*>(GetFirst3DRegion()));
}

void AbstrRenderer::SetRotation(RenderRegion *renderRegion,
                                const FLOATMATRIX4& rotation) {
  renderRegion->rotation = rotation;
  ScheduleWindowRedraw(renderRegion);
}

const FLOATMATRIX4&
AbstrRenderer::GetRotation(const RenderRegion* renderRegion) const {
  return renderRegion->rotation;
}

void AbstrRenderer::SetTranslation(RenderRegion *renderRegion,
                                   const FLOATMATRIX4& mTranslation) {
  renderRegion->translation = mTranslation;
  ScheduleWindowRedraw(renderRegion);
}


const FLOATMATRIX4&
AbstrRenderer::GetTranslation(const RenderRegion* renderRegion) const {
  return renderRegion->translation;
}


void AbstrRenderer::SetClipPlane(RenderRegion *renderRegion,
                                 const ExtendedPlane& plane)
{
  if(plane == m_ClipPlane) { return; }
  m_ClipPlane = plane; /// @todo: Make this per RenderRegion.
  ScheduleWindowRedraw(renderRegion);
}

bool AbstrRenderer::IsClipPlaneEnabled(RenderRegion *renderRegion) {
  if (!renderRegion)
    renderRegion = GetFirst3DRegion();
  if (renderRegion)
    return m_bClipPlaneOn; /// @todo: Make this per RenderRegion.
  else
    return false;
}

void AbstrRenderer::EnableClipPlane(RenderRegion *renderRegion) {
  if (!renderRegion)
    renderRegion = GetFirst3DRegion();
  if (renderRegion) {
    if(!m_bClipPlaneOn) {
      m_bClipPlaneOn = true; /// @todo: Make this per RenderRegion.
      ScheduleWindowRedraw(renderRegion);
    }
  }
}

void AbstrRenderer::DisableClipPlane(RenderRegion *renderRegion) {
  if (!renderRegion)
    renderRegion = GetFirst3DRegion();
  if (renderRegion) {
    if(m_bClipPlaneOn) {
      m_bClipPlaneOn = false; /// @todo: Make this per RenderRegion.
      ScheduleWindowRedraw(renderRegion);
    }
  }
}
void AbstrRenderer::ShowClipPlane(bool bShown,
                                  RenderRegion *renderRegion) {
  if (!renderRegion)
    renderRegion = GetFirst3DRegion();
  if (renderRegion) {
    m_bClipPlaneDisplayed = bShown; /// @todo: Make this per RenderRegion.
    if(m_bClipPlaneOn) {
      ScheduleWindowRedraw(renderRegion);
    }
  }
}

void AbstrRenderer::LuaShowClipPlane(bool bShown,
                                     LuaClassInstance inst)
{
  shared_ptr<LuaScripting> ss = m_pMasterController->LuaScript();
  ShowClipPlane(bShown, inst.getRawPointer<RenderRegion>(ss));
}

const ExtendedPlane& AbstrRenderer::GetClipPlane() const {
  return m_ClipPlane;
}

ExtendedPlane AbstrRenderer::LuaGetClipPlane() const {
  return GetClipPlane();
}

void AbstrRenderer::ClipPlaneRelativeLock(bool bRel) {
  m_bClipPlaneLocked = bRel;/// @todo: Make this per RenderRegion ?
}

void AbstrRenderer::SetSliceDepth(RenderRegion *renderRegion, uint64_t sliceDepth) {
  if (renderRegion->GetSliceIndex() != sliceDepth) {
    renderRegion->SetSliceIndex(sliceDepth);
    ScheduleWindowRedraw(renderRegion);
    if (m_bRenderPlanesIn3D)
      Schedule3DWindowRedraws();
  }
}

uint64_t AbstrRenderer::GetSliceDepth(const RenderRegion *renderRegion) const {
  return renderRegion->GetSliceIndex();
}


void AbstrRenderer::SetGlobalBBox(bool bRenderBBox) {
  m_bRenderGlobalBBox = bRenderBBox; /// @todo: Make this per RenderRegion.
  Schedule3DWindowRedraws();
}

void AbstrRenderer::SetLocalBBox(bool bRenderBBox) {
  m_bRenderLocalBBox = bRenderBBox; /// @todo: Make this per RenderRegion.
  Schedule3DWindowRedraws();
}

void AbstrRenderer::ScheduleCompleteRedraw() {
  m_iCheckCounter = m_iStartDelay;
  MESSAGE("complete redraw scheduled");

  for (size_t i=0; i < renderRegions.size(); ++i) {
    renderRegions[i]->redrawMask = true;
    renderRegions[i]->isBlank = true;
    renderRegions[i]->isTargetBlank = true;
  }
}

void AbstrRenderer::Schedule3DWindowRedraws() {
  m_iCheckCounter = m_iStartDelay;

  for (size_t i=0; i < renderRegions.size(); ++i) {
    if (renderRegions[i]->is3D()) {
      renderRegions[i]->redrawMask = true;
      renderRegions[i]->isBlank = true;
      renderRegions[i]->isTargetBlank = true;
    }
  }
}

void AbstrRenderer::ScheduleWindowRedraw(RenderRegion *renderRegion) {
  m_iCheckCounter = m_iStartDelay;
  renderRegion->redrawMask = true;
  renderRegion->isBlank = true;
  renderRegion->isTargetBlank = true;
}

void AbstrRenderer::ScheduleRecompose(RenderRegion *renderRegion) {
  if (!renderRegion)
    renderRegion = GetFirst3DRegion();
  if (renderRegion) {
    // ensure we've finished the current frame:
    if(m_vCurrentBrickList.size() == m_iBricksRenderedInThisSubFrame) {
      m_bPerformReCompose = true;
      renderRegion->redrawMask = true;
    } else {
      ScheduleWindowRedraw(renderRegion);
    }
  }
}


void AbstrRenderer::CompletedASubframe(RenderRegion* region) {
  bool bRenderingFirstSubFrame =
    (m_iCurrentLODOffset == m_iStartLODOffset) &&
    (!this->decreaseScreenRes || this->decreaseScreenResNow) &&
    (!this->decreaseSamplingRate || this->decreaseSamplingRateNow);
  bool bSecondSubFrame =
    !bRenderingFirstSubFrame &&
    (m_iCurrentLODOffset == m_iStartLODOffset ||
     (m_iCurrentLODOffset == m_iStartLODOffset-1 &&
      !(this->decreaseScreenRes || this->decreaseSamplingRate)));

  if (bRenderingFirstSubFrame) {
    // time for current interaction LOD -> to detect if we are too slow
    this->msecPassed[0] = this->msecPassedCurrentFrame;
  } else if(bSecondSubFrame) {
    this->msecPassed[1] = this->msecPassedCurrentFrame;
  }
  this->msecPassedCurrentFrame = 0.0f;
  region->isTargetBlank = false;
}

void AbstrRenderer::RestartTimer(const size_t iTimerIndex) {
  this->msecPassed[iTimerIndex] = -1.0f;
}

void AbstrRenderer::RestartTimers() {
  RestartTimer(0);
  RestartTimer(1);
}

void AbstrRenderer::ComputeMaxLODForCurrentView() {
  if (m_eRendererTarget != RT_CAPTURE && this->msecPassed[0]>=0.0f) {
    // if rendering is too slow use a lower resolution during interaction
    if (this->msecPassed[0] > m_fMaxMSPerFrame) {
      // wait for 3 frames before switching to lower lod (3 here is
      // chosen more or less arbitrarily, can be changed if needed)
      if (m_iLODNotOKCounter < 3) {
        MESSAGE("Would increase start LOD but will give the renderer %u "
                "more frame(s) time to become faster", 3 - m_iLODNotOKCounter);
        m_iLODNotOKCounter++;
      } else {
        // We gave it a chance but rendering was too slow. So let's drop down
        // in quality.
        m_iLODNotOKCounter = 0;

        // Easiest thing is to try rendering a lower quality LOD. So try this
        // if possible.
        uint64_t iPerformanceBasedLODSkip =
          std::max<uint64_t>(1, m_iPerformanceBasedLODSkip) - 1;
        if (m_iPerformanceBasedLODSkip != iPerformanceBasedLODSkip) {
          MESSAGE("Increasing start LOD to %llu as it took %g ms "
                  "to render the first LOD level (max is %g) ",
                  m_iPerformanceBasedLODSkip, this->msecPassed[0],
                  m_fMaxMSPerFrame);
          this->msecPassed[0] = this->msecPassed[1];
          m_iPerformanceBasedLODSkip = iPerformanceBasedLODSkip;
        } else {
          // Already at lowest quality LOD, so will need to try something else.
          MESSAGE("Would like to increase start LOD as it took %g ms "
                  "to render the first LOD level (max is %g) BUT CAN'T.",
                  this->msecPassed[0], m_fMaxMSPerFrame);
          if (m_bRenderLowResIntermediateResults) {
            if (this->decreaseSamplingRate && this->decreaseScreenRes) {
              MESSAGE("Even with UseAllMeans there is nothing that "
                      "can be done to meet the specified framerate.");
            } else {
              if (!this->decreaseScreenRes) {
                MESSAGE("UseAllMeans enabled: decreasing resolution "
                        "to meet target framerate");
                this->decreaseScreenRes = true;
              } else {
                MESSAGE("UseAllMeans enabled: decreasing sampling rate "
                        "to meet target framerate");
                this->decreaseSamplingRate = true;
              }
            }
          } else {
            MESSAGE("UseAllMeans disabled so framerate can not be met...");
          }
        }
      }
    } else {
      // if finished rendering last frame (m_iBricksRenderedInThisSubFrame is
      // from the last frame, not the new one we are about to start) and did
      // this fast enough, use a higher resolution during interaction.
      if (m_vCurrentBrickList.size() == m_iBricksRenderedInThisSubFrame &&
          this->msecPassed[1] >= 0.0f &&
          this->msecPassed[1] <= m_fMaxMSPerFrame) {
        m_iLODNotOKCounter = 0;
        // We're rendering fast, so lets step up the quality. Easiest thing to
        // try first is rendering at normal sampling rate and resolution.
        if (this->decreaseSamplingRate || this->decreaseScreenRes) {
          if (this->decreaseSamplingRate) {
            MESSAGE("Rendering at full resolution as this took only %g ms",
                    this->msecPassed[0]);
            this->decreaseSamplingRate = false;
          } else {
            if (this->decreaseScreenRes) {
              MESSAGE("Rendering to full viewport as this took only %g ms",
                      this->msecPassed[0]);
              this->decreaseScreenRes = false;
            }
          }
        } else {
          // Let's try rendering at a higher quality LOD.
          uint64_t iPerformanceBasedLODSkip =
            std::min<uint64_t>(m_iMaxLODIndex - m_iMinLODForCurrentView,
                             m_iPerformanceBasedLODSkip + 1);
          if (m_iPerformanceBasedLODSkip != iPerformanceBasedLODSkip) {
            MESSAGE("Decreasing start LOD to %llu as it took only %g ms "
                    "to render the second LOD level",
                    m_iPerformanceBasedLODSkip, this->msecPassed[1]);
            m_iPerformanceBasedLODSkip = iPerformanceBasedLODSkip;
          }
        }
      } else {
        if (m_vCurrentBrickList.size() == m_iBricksRenderedInThisSubFrame) {
          MESSAGE("Start LOD seems to be ok");
        }
      }
    }

    m_iStartLODOffset = std::max(m_iMinLODForCurrentView,
                                 m_iMaxLODIndex - m_iPerformanceBasedLODSkip);
  } else if (m_eRendererTarget != RT_INTERACTIVE){
    m_iStartLODOffset = m_iMinLODForCurrentView;
  } else {
    // This is our very first frame, let's take it easy.
    m_iStartLODOffset = m_iMaxLODIndex;
  }

  m_iStartLODOffset = std::min(m_iStartLODOffset,
                               static_cast<uint64_t>(m_iMaxLODIndex -
                                                   m_iLODLimits.x));
  m_iCurrentLODOffset = m_iStartLODOffset;
  RestartTimers();
}

void AbstrRenderer::ComputeMinLODForCurrentView() {
  // compute scale factor for domain
  UINTVECTOR3 vDomainSize = UINTVECTOR3(m_pDataset->GetDomainSize());
  FLOATVECTOR3 vScale = FLOATVECTOR3(m_pDataset->GetScale());
  FLOATVECTOR3 vExtend = FLOATVECTOR3(vDomainSize) * vScale;
  vExtend /= vExtend.maxVal();

  /// @todo consider real extent not center
  FLOATVECTOR3 vfCenter(0,0,0);
  m_iMinLODForCurrentView = static_cast<uint64_t>(
    MathTools::Clamp(m_FrustumCullingLOD.GetLODLevel(vfCenter, vExtend,
                                                     vDomainSize),
                     static_cast<int>(m_iLODLimits.y),
                     static_cast<int>(m_pDataset->GetLODLevelCount()-1)));
}

/// Calculates the distance to a given brick given the current view
/// transformation.  There is a slight offset towards the center, which helps
/// avoid ambiguous cases.
static float
brick_distance(const Brick &b, const FLOATMATRIX4 &mat_modelview)
{
  const float fEpsilon = 0.4999f;
  const FLOATVECTOR3 vEpsilonEdges[8] = {
    b.vCenter + FLOATVECTOR3(-b.vExtension.x,-b.vExtension.y,-b.vExtension.z) *
      fEpsilon,
    b.vCenter + FLOATVECTOR3(-b.vExtension.x,-b.vExtension.y,+b.vExtension.z) *
      fEpsilon,
    b.vCenter + FLOATVECTOR3(-b.vExtension.x,+b.vExtension.y,-b.vExtension.z) *
      fEpsilon,
    b.vCenter + FLOATVECTOR3(-b.vExtension.x,+b.vExtension.y,+b.vExtension.z) *
      fEpsilon,
    b.vCenter + FLOATVECTOR3(+b.vExtension.x,-b.vExtension.y,-b.vExtension.z) *
      fEpsilon,
    b.vCenter + FLOATVECTOR3(+b.vExtension.x,-b.vExtension.y,+b.vExtension.z) *
      fEpsilon,
    b.vCenter + FLOATVECTOR3(+b.vExtension.x,+b.vExtension.y,-b.vExtension.z) *
      fEpsilon,
    b.vCenter + FLOATVECTOR3(+b.vExtension.x,+b.vExtension.y,+b.vExtension.z) *
      fEpsilon
  };

  // final distance is the distance to the closest corner.
  float fDistance = std::numeric_limits<float>::max();
  for(size_t i=0; i < 8; ++i) {
    fDistance = std::min(
                  fDistance,
                  (FLOATVECTOR4(vEpsilonEdges[i],1.0f)*mat_modelview)
                    .xyz().length()
                );
  }
  return fDistance;
}

vector<Brick> AbstrRenderer::BuildLeftEyeSubFrameBrickList(
                             const FLOATMATRIX4& modelView,
                             const vector<Brick>& vRightEyeBrickList) const {
  vector<Brick> vBrickList = vRightEyeBrickList;

  for (uint32_t iBrick = 0;iBrick<vBrickList.size();iBrick++) {
    // compute minimum distance to brick corners (offset slightly to
    // the center to resolve ambiguities).
    vBrickList[iBrick].fDistance = brick_distance(vBrickList[iBrick],
                                                  modelView);
  }

  sort(vBrickList.begin(), vBrickList.end());

  return vBrickList;
}

double AbstrRenderer::MaxValue() const {
  if (m_pDataset->GetBitWidth() != 8 && m_bDownSampleTo8Bits)
    return 255;
  else
    return (m_pDataset->GetRange().first > m_pDataset->GetRange().second) ?
            m_p1DTrans->GetSize() : m_pDataset->GetRange().second;
}

bool AbstrRenderer::OnlyRecomposite(RenderRegion* region) const {
  return !region->isBlank &&
          m_bPerformReCompose &&
         !this->doAnotherRedrawDueToLowResOutput;
}

bool AbstrRenderer::RegionNeedsBrick(const RenderRegion& rr,
                                     const BrickKey& key,
                                     const BrickMD& bmd,
                                     bool& bIsEmptyButInFrustum) const
{
  if(rr.is2D()) {
    return rr.GetUseMIP() ||
           std::get<1>(key) == m_pDataset->GetLODLevelCount()-1;
  }

  FLOATVECTOR3 vScale(float(m_pDataset->GetScale().x),
                      float(m_pDataset->GetScale().y),
                      float(m_pDataset->GetScale().z));
  UINT64VECTOR3 vDomainSize = m_pDataset->GetDomainSize(size_t(m_iCurrentLOD));
  FLOATVECTOR3 vDomainSizeCorrectedScale =
    vScale * FLOATVECTOR3(vDomainSize)/float(vDomainSize.maxVal());
  vScale /= vDomainSizeCorrectedScale.maxVal();
  Brick b;
  b.vExtension = bmd.extents * vScale;
  b.vCenter = bmd.center * vScale;
  b.vVoxelCount = bmd.n_voxels;

  // skip the brick if it is outside the current view frustum
  if (!m_FrustumCullingLOD.IsVisible(b.vCenter, b.vExtension)) {
    MESSAGE("Outside view frustum, skipping <%u,%u,%u>",
            static_cast<unsigned>(std::get<0>(key)),
            static_cast<unsigned>(std::get<1>(key)),
            static_cast<unsigned>(std::get<2>(key)));
    return false;
  }

  // skip the brick if the clipping plane removes it.
  if(m_bClipPlaneOn && Clipped(rr, b)) {
    MESSAGE("clipped by clip plane: skipping <%u,%u,%u>",
            static_cast<unsigned>(std::get<0>(key)),
            static_cast<unsigned>(std::get<1>(key)),
            static_cast<unsigned>(std::get<2>(key)));
    return false;
  }

  // finally, query the data in the brick; if no data can possibly be visible,
  // don't render this brick.
  bIsEmptyButInFrustum = !ContainsData(key);
  MESSAGE("empty but visible: %d", static_cast<int>(bIsEmptyButInFrustum));

  return true;
}

/// @return true if this brick is clipped by a clipping plane.
bool AbstrRenderer::Clipped(const RenderRegion& rr, const Brick& b) const
{
  FLOATVECTOR3 vBrickVertices[8] = {
    b.vCenter + FLOATVECTOR3(-b.vExtension.x,-b.vExtension.y,-b.vExtension.z) * 0.5f,
    b.vCenter + FLOATVECTOR3(-b.vExtension.x,-b.vExtension.y,+b.vExtension.z) * 0.5f,
    b.vCenter + FLOATVECTOR3(-b.vExtension.x,+b.vExtension.y,-b.vExtension.z) * 0.5f,
    b.vCenter + FLOATVECTOR3(-b.vExtension.x,+b.vExtension.y,+b.vExtension.z) * 0.5f,
    b.vCenter + FLOATVECTOR3(+b.vExtension.x,-b.vExtension.y,-b.vExtension.z) * 0.5f,
    b.vCenter + FLOATVECTOR3(+b.vExtension.x,-b.vExtension.y,+b.vExtension.z) * 0.5f,
    b.vCenter + FLOATVECTOR3(+b.vExtension.x,+b.vExtension.y,-b.vExtension.z) * 0.5f,
    b.vCenter + FLOATVECTOR3(+b.vExtension.x,+b.vExtension.y,+b.vExtension.z) * 0.5f,
  };

  FLOATMATRIX4 matWorld = rr.rotation * rr.translation;
  // project the 8 corners of the brick.  If we find just a single corner
  // which isn't clipped by our plane, we need this brick.
  for (size_t i=0; i < 8; i++) {
    vBrickVertices[i] = (FLOATVECTOR4(vBrickVertices[i],1) * matWorld)
                        .dehomo();
    if (!m_ClipPlane.Plane().clip(vBrickVertices[i])) {
      return false;
    }
  }
  // no corner was found; must be clipped.
  return true;
}

// checks if the given brick contains useful data.  As one example, a brick
// that has data between 608-912 will not be relevant if we are rendering an
// isosurface of 42.
bool AbstrRenderer::ContainsData(const BrickKey& key) const
{
  if(this->ColorData()) {
    // We don't have good metadata for color data currently, so it can never be
    // skipped.
    return true;
  }

  double fMaxValue = (m_pDataset->GetRange().first > m_pDataset->GetRange().second) ?
                          m_p1DTrans->GetSize() : m_pDataset->GetRange().second;
  double fRescaleFactor = fMaxValue / double(m_p1DTrans->GetSize()-1);

  bool bContainsData;
  // render mode dictates how we look at data ...
  switch (m_eRenderMode) {
    case RM_1DTRANS:
      // ... in 1D we only care about the range of data in a brick
      bContainsData = m_pDataset->ContainsData(
                        key,
                        double(m_p1DTrans->GetNonZeroLimits().x) * fRescaleFactor,
                        double(m_p1DTrans->GetNonZeroLimits().y) * fRescaleFactor
                      );
      break;
    case RM_2DTRANS:
      // ... in 2D we also need to concern ourselves w/ min/max gradients
      bContainsData = m_pDataset->ContainsData(
                        key,
                        double(m_p2DTrans->GetNonZeroLimits().x) * fRescaleFactor,
                        double(m_p2DTrans->GetNonZeroLimits().y) * fRescaleFactor,
                        double(m_p2DTrans->GetNonZeroLimits().z),
                        double(m_p2DTrans->GetNonZeroLimits().w)
                      );
      break;
    case RM_ISOSURFACE:
      // ... and in isosurface mode we only care about a single value.
      bContainsData = m_pDataset->ContainsData(key, m_fIsovalue);
      break;
    default:
      T_ERROR("Unhandled rendering mode.  Skipping brick!");
      bContainsData = false;
      break;
  }

  return bContainsData;
}

vector<Brick> AbstrRenderer::BuildSubFrameBrickList(bool bUseResidencyAsDistanceCriterion) {
  vector<Brick> vBrickList;
  UINT64VECTOR3 vDomainSize = m_pDataset->GetDomainSize(0);

  FLOATVECTOR3 vScale(float(m_pDataset->GetScale().x),
                      float(m_pDataset->GetScale().y),
                      float(m_pDataset->GetScale().z));

  FLOATVECTOR3 vDomainSizeCorrectedScale = vScale *
                                           FLOATVECTOR3(vDomainSize)/
                                           float(vDomainSize.maxVal());

  vScale /= vDomainSizeCorrectedScale.maxVal();

  MESSAGE("Building active brick list from %u active bricks.",
          static_cast<unsigned>(m_pDataset->GetBrickCount(size_t(m_iCurrentLOD),
                                                          m_iTimestep)));

  BrickTable::const_iterator brick = m_pDataset->BricksBegin();
  for(; brick != m_pDataset->BricksEnd(); ++brick) {
    // skip over the brick if it's for the wrong timestep or LOD
    if(std::get<0>(brick->first) != m_iTimestep ||
       std::get<1>(brick->first) != m_iCurrentLOD) {
      continue;
    }
    const BrickMD& bmd = brick->second;
    Brick b;
    b.vExtension = bmd.extents * vScale;
    b.vCenter = bmd.center * vScale;
    b.vVoxelCount = bmd.n_voxels;
#ifndef __clang__
    b.kBrick = brick->first;
#else
    BrickKey key = brick->first;
    b.kBrick = key;
#endif

    bool needed = false;
    for(std::vector<RenderRegion*>::const_iterator reg = renderRegions.begin();
        reg != renderRegions.end(); ++reg) {
      if(RegionNeedsBrick(**reg, brick->first, brick->second, b.bIsEmpty)) {
        needed = true;
        break;
      }
    }
    if(!needed) {
      MESSAGE("Skipping brick <%u,%u,%u> because it isn't relevant.",
              static_cast<unsigned>(std::get<0>(brick->first)),
              static_cast<unsigned>(std::get<1>(brick->first)),
              static_cast<unsigned>(std::get<2>(brick->first)));
      continue;
    }

    if(b.bIsEmpty) {
      MESSAGE("Skipping further computations for brick <%u,%u,%u> "
              "because it is empty/invisible given the current vis parameters,"
              " but we'll keep it in the list in case it overlaps with other "
              "data (e.g. a mesh)",
              static_cast<unsigned>(std::get<0>(brick->first)),
              static_cast<unsigned>(std::get<1>(brick->first)),
              static_cast<unsigned>(std::get<2>(brick->first)));
    } else {
      std::pair<FLOATVECTOR3, FLOATVECTOR3> vTexcoords = m_pDataset->GetTextCoords(brick, m_bUseOnlyPowerOfTwo);
      b.vTexcoordsMin = vTexcoords.first;
      b.vTexcoordsMax = vTexcoords.second;

      // the depth order doesn't really matter for MIP rotations,
      // since we need to traverse every brick anyway.  So we do a
      // sort based on which bricks are already resident, to get a
      // good cache hit rate.
      if (bUseResidencyAsDistanceCriterion) {
        if (IsVolumeResident(brick->first)) {
          b.fDistance = 0;
        } else {
          b.fDistance = 1;
        }
      } else {
        // compute minimum distance to brick corners (offset
        // slightly to the center to resolve ambiguities)
        // "GetFirst" region: see FIXME below.
        b.fDistance = brick_distance(b, GetFirst3DRegion()->modelView[0]);
      }
    }
/*
MESSAGE("considering brick <%zu,%zu,%zu>", std::get<0>(brick->first),
            std::get<1>(brick->first), std::get<2>(brick->first));
            */

    // add the brick to the list of active bricks
    vBrickList.push_back(b);
  }

  // depth sort bricks

  /// @todo FIXME?: we need to do smarter sorting.  If we've got multiple 3D
  /// regions, they might need different orderings.  However, we want to try to
  /// traverse bricks in a similar order, because the IO will rape us
  /// otherwise.
  /// For now, IV3D doesn't support multiple 3D regions in a single renderer.
  sort(vBrickList.begin(), vBrickList.end());

  return vBrickList;
}

void AbstrRenderer::GetVolumeAABB(FLOATVECTOR3& vCenter, FLOATVECTOR3& vExtend) const {
  UINT64VECTOR3 vDomainSize = m_pDataset->GetDomainSize();
  FLOATVECTOR3 vScale = FLOATVECTOR3(m_pDataset->GetScale());

  vExtend = FLOATVECTOR3(vDomainSize) * vScale;
  vExtend /= vExtend.maxVal();
  vCenter = FLOATVECTOR3(0,0,0);
}

FLOATVECTOR3 AbstrRenderer::LuaGetVolumeAABBCenter() const {
  FLOATVECTOR3 vCenter;
  FLOATVECTOR3 vExtend;
  GetVolumeAABB(vCenter, vExtend);
  return vCenter;
}

FLOATVECTOR3 AbstrRenderer::LuaGetVolumeAABBExtents() const {
  FLOATVECTOR3 vCenter;
  FLOATVECTOR3 vExtend;
  GetVolumeAABB(vCenter, vExtend);
  return vExtend;
}

void AbstrRenderer::PlanFrame(RenderRegion3D& region) {
  m_FrustumCullingLOD.SetViewMatrix(region.modelView[0]);
  m_FrustumCullingLOD.Update();

  // let the mesh know about our current state, technically
  // SetVolumeAABB only needs to be called when the geometry of volume
  // has changed (rescale) and SetUserPos only when the view has
  // changed (matrix update) but the mesh class is smart enough to catch
  // redundant changes so we just leave the code here for now
  if (m_bSupportsMeshes) {
    FLOATVECTOR3 vCenter, vExtend;
    GetVolumeAABB(vCenter, vExtend);
    FLOATVECTOR3 vMinPoint = vCenter-vExtend/2.0,
                 vMaxPoint = vCenter+vExtend/2.0;

     for (vector<shared_ptr<RenderMesh>>::iterator mesh = m_Meshes.begin();
         mesh != m_Meshes.end(); mesh++) {
      if ((*mesh)->GetActive()) {
        (*mesh)->SetVolumeAABB(vMinPoint, vMaxPoint);
        (*mesh)->SetUserPos( (FLOATVECTOR4(0,0,0,1)*
                               region.modelView[0].inverse()).xyz() );
      }
    }
  }

  // if we found a blank region, we need to reset and do some actual
  // planning.
  if(region.isBlank) {
    // figure out how fine we need to draw the data for the current view
    // this method takes the size of a voxel in screen space into account
    ComputeMinLODForCurrentView();
    // figure out at what coarse level we need to start for the current view
    // this method takes the rendermode (capture or not) and the time it took
    // to render the last subframe into account
    ComputeMaxLODForCurrentView();
  }

  // plan if the frame is to be redrawn
  // or if we have completed the last subframe but not the entire frame
  if (region.isBlank ||
      (m_vCurrentBrickList.size() == m_iBricksRenderedInThisSubFrame)) {
    bool bBuildNewList = false;
    if (region.isBlank) {
      this->decreaseSamplingRateNow = this->decreaseSamplingRate;
      this->decreaseScreenResNow = this->decreaseScreenRes;
      bBuildNewList = true;
      if (this->decreaseSamplingRateNow || this->decreaseScreenResNow)
        this->doAnotherRedrawDueToLowResOutput = true;
    } else {
      if (this->decreaseSamplingRateNow || this->decreaseScreenResNow) {
        this->decreaseScreenResNow = false;
        this->decreaseSamplingRateNow = false;
        m_iBricksRenderedInThisSubFrame = 0;
        this->doAnotherRedrawDueToLowResOutput = false;
      } else {
        if (m_iCurrentLODOffset > m_iMinLODForCurrentView) {
          bBuildNewList = true;
          m_iCurrentLODOffset--;
        }
      }
    }

    if (bBuildNewList) {
      if(m_eRendererTarget == RT_CAPTURE) {
        m_iCurrentLOD = 0;
      } else {
        m_iCurrentLOD = std::min<uint64_t>(m_iCurrentLODOffset,
                                         m_pDataset->GetLODLevelCount()-1);
      }
      // build new brick todo-list
      MESSAGE("Building new brick list for LOD %llu...", m_iCurrentLOD);
      m_vCurrentBrickList = BuildSubFrameBrickList();
      MESSAGE("%u bricks made the cut.", uint32_t(m_vCurrentBrickList.size()));
      if (m_bDoStereoRendering) {
        m_vLeftEyeBrickList =
          BuildLeftEyeSubFrameBrickList(region.modelView[1], m_vCurrentBrickList);
      }

      m_iBricksRenderedInThisSubFrame = 0;
    }
  }

  if(region.isBlank) {
    // update frame states
    m_iIntraFrameCounter = 0;
    m_iFrameCounter = m_pMasterController->MemMan()->UpdateFrameCounter();
  }
}

void AbstrRenderer::PlanHQMIPFrame(RenderRegion& renderRegion) {
  // compute modelviewmatrix and pass it to the culling object
  renderRegion.modelView[0] = renderRegion.rotation*renderRegion.translation*m_mView[0];

  m_FrustumCullingLOD.SetPassAll(true);

  UINTVECTOR3  viVoxelCount = UINTVECTOR3(m_pDataset->GetDomainSize());

  m_iCurrentLODOffset = 0;
  m_iCurrentLOD = 0;

  if (m_bMIPLOD) {
    while (viVoxelCount.minVal() >= m_vWinSize.maxVal()) {
      viVoxelCount /= 2;
      m_iCurrentLOD++;
    }
  }

  if (m_iCurrentLOD > 0) {
    m_iCurrentLOD = min<uint64_t>(m_pDataset->GetLODLevelCount()-1,
                                m_iCurrentLOD-1);
  }

  // build new brick todo-list
  m_vCurrentBrickList = BuildSubFrameBrickList(true);

  m_iBricksRenderedInThisSubFrame = 0;

  // update frame states
  m_iIntraFrameCounter = 0;
  m_iFrameCounter = m_pMasterController->MemMan()->UpdateFrameCounter();
}

void AbstrRenderer::SetCV(bool bEnable) {
  if (!SupportsClearView()) bEnable = false;

  if (m_bDoClearView != bEnable) {
    m_bDoClearView = bEnable;
    m_bFirstDrawAfterModeChange = true;
    if (m_eRenderMode == RM_ISOSURFACE)
      Schedule3DWindowRedraws();
  }
}

void AbstrRenderer::SetIsosurfaceColor(const FLOATVECTOR3& vColor) {
  m_vIsoColor = vColor;
  if (m_eRenderMode == RM_ISOSURFACE)
    ScheduleRecompose();
}

void AbstrRenderer::SetOrthoView(bool bOrthoView) {
  if (m_bOrthoView != bOrthoView) {
    m_bOrthoView = bOrthoView;
    ScheduleCompleteRedraw();
  }
}

void AbstrRenderer::Transfer3DRotationToMIP() {
  FLOATMATRIX4 rot = GetFirst3DRegion()->rotation;
  for (size_t i=0; i < renderRegions.size(); ++i) {
    if (!renderRegions[i]->is3D())
      renderRegions[i]->rotation = rot;
  }
}

void AbstrRenderer::SetRenderCoordArrows(bool bRenderCoordArrows) {
  if (m_bRenderCoordArrows != bRenderCoordArrows) {
    m_bRenderCoordArrows = bRenderCoordArrows;
    Schedule3DWindowRedraws();
  }
}

void AbstrRenderer::Set2DPlanesIn3DView(bool bRenderPlanesIn3D,
                                        RenderRegion *renderRegion) {
  if (!renderRegion)
    renderRegion = GetFirst3DRegion();
  if (renderRegion) {
    if (m_bRenderPlanesIn3D != bRenderPlanesIn3D) {
      m_bRenderPlanesIn3D = bRenderPlanesIn3D;
      ScheduleWindowRedraw(renderRegion);
    }
  }
}

void AbstrRenderer::LuaSet2DPlanesIn3DView(bool bRenderPlanesIn3D,
                                           LuaClassInstance region)
{
  shared_ptr<LuaScripting> ss = m_pMasterController->LuaScript();
  Set2DPlanesIn3DView(bRenderPlanesIn3D,region.getRawPointer<RenderRegion>(ss));
}

void AbstrRenderer::SetCVIsoValue(float fIsovalue) {
  if (m_fCVIsovalue != fIsovalue) {
    m_fCVIsovalue = fIsovalue;

    if (m_bDoClearView && m_eRenderMode == RM_ISOSURFACE) {
      Schedule3DWindowRedraws();
    }
    std::ostringstream prov;
    prov << fIsovalue;
  }
}

void AbstrRenderer::SetCVColor(const FLOATVECTOR3& vColor) {
  if (m_vCVColor != vColor) {
    m_vCVColor = vColor;
    if (m_bDoClearView && m_eRenderMode == RM_ISOSURFACE)
      ScheduleRecompose();
  }
}

void AbstrRenderer::SetCVSize(float fSize) {
  if (m_fCVSize != fSize) {
    m_fCVSize = fSize;
    if (m_bDoClearView && m_eRenderMode == RM_ISOSURFACE)
      ScheduleRecompose();
  }
}

void AbstrRenderer::SetCVContextScale(float fScale) {
  if (m_fCVContextScale != fScale) {
    m_fCVContextScale = fScale;
    if (m_bDoClearView && m_eRenderMode == RM_ISOSURFACE)
      ScheduleRecompose();
  }
}

void AbstrRenderer::SetCVBorderScale(float fScale) {
  if (m_fCVBorderScale != fScale) {
    m_fCVBorderScale = fScale;
    if (m_bDoClearView && m_eRenderMode == RM_ISOSURFACE) {
      ScheduleRecompose();
    }
  }
}

void AbstrRenderer::SetCVFocusPosFVec(const FLOATVECTOR4& vCVPos) {
  m_vCVMousePos = INTVECTOR2(-1,-1);
  m_vCVPos = vCVPos;
}

void AbstrRenderer::SetCVFocusPos(LuaClassInstance renderRegion,
                                  const INTVECTOR2& vPos) {
  if (m_vCVMousePos!= vPos) {
    m_vCVMousePos = vPos;
    if (m_bDoClearView && m_eRenderMode == RM_ISOSURFACE)
      CVFocusHasChanged(renderRegion);
  }
}

void AbstrRenderer::SetLogoParams(string strLogoFilename, int iLogoPos) {
  m_strLogoFilename = strLogoFilename;
  m_iLogoPos        = iLogoPos;
}

void AbstrRenderer::Set2DFlipMode(RenderRegion *renderRegion,
                                  bool flipX, bool flipY) {
  renderRegion->SetFlipView(flipX, flipY);
  ScheduleWindowRedraw(renderRegion);
}

void AbstrRenderer::Get2DFlipMode(const RenderRegion *renderRegion,
                                  bool& flipX, bool& flipY) const {
  renderRegion->GetFlipView(flipX, flipY);
}

bool AbstrRenderer::GetUseMIP(const RenderRegion *renderRegion) const {
  return renderRegion->GetUseMIP();
}

void AbstrRenderer::SetUseMIP(RenderRegion *renderRegion, bool useMIP) {
  renderRegion->SetUseMIP(useMIP);
  ScheduleWindowRedraw(renderRegion);
}

void AbstrRenderer::SetStereo(bool bStereoRendering) {
  m_bRequestStereoRendering = bStereoRendering;
  Schedule3DWindowRedraws();
}

void AbstrRenderer::SetStereoEyeDist(float fStereoEyeDist) {
  m_fStereoEyeDist = fStereoEyeDist;
  if (m_bDoStereoRendering) Schedule3DWindowRedraws();
}

void AbstrRenderer::SetStereoFocalLength(float fStereoFocalLength) {
  m_fStereoFocalLength = fStereoFocalLength;
  if (m_bDoStereoRendering) Schedule3DWindowRedraws();
}

void AbstrRenderer::SetStereoMode(EStereoMode mode) {
  m_eStereoMode = mode;
  if (m_bDoStereoRendering) Schedule3DWindowRedraws();
}

void AbstrRenderer::SetStereoEyeSwap(bool bSwap) {
  m_bStereoEyeSwap = bSwap;
  if (m_bDoStereoRendering) Schedule3DWindowRedraws();
}

void AbstrRenderer::CVFocusHasChanged(LuaClassInstance) {
  ScheduleRecompose();
}

bool AbstrRenderer::ColorData() const {
  // right now we just look for 4- and 3-component data, and assume all
  // such data is RGBA... at some point we probably want to add some
  // sort of query into tuvok::Dataset, so that a file format could
  // decide whether or not it wants to consider 4-component data to be
  // RGBA data.
  return m_pDataset->GetComponentCount() == 4 ||
         m_pDataset->GetComponentCount() == 3;
}

void AbstrRenderer::SetConsiderPreviousDepthbuffer(bool bConsiderPreviousDepthbuffer) {
  if (m_bConsiderPreviousDepthbuffer != bConsiderPreviousDepthbuffer)
  {
    m_bConsiderPreviousDepthbuffer = bConsiderPreviousDepthbuffer;
    ScheduleCompleteRedraw();
  }
}

void AbstrRenderer::SetPerfMeasures(uint32_t iMinFramerate, bool bRenderLowResIntermediateResults,
                                    float fScreenResDecFactor,
                                    float fSampleDecFactor, uint32_t iStartDelay) {
  m_fMaxMSPerFrame = (iMinFramerate == 0) ? 10000 : 1000.0f / float(iMinFramerate);
  m_fScreenResDecFactor = fScreenResDecFactor;
  m_fSampleDecFactor = fSampleDecFactor;
  m_bRenderLowResIntermediateResults = bRenderLowResIntermediateResults;

  if (!m_bRenderLowResIntermediateResults) {
    this->decreaseScreenRes = false;
    this->decreaseScreenResNow = false;
    this->decreaseSamplingRate = false;
    this->decreaseSamplingRateNow = false;
    this->doAnotherRedrawDueToLowResOutput = false;
  }

  m_iStartDelay = iStartDelay;

  ScheduleCompleteRedraw();
}

void AbstrRenderer::SetLODLimits(const UINTVECTOR2 iLODLimits) {
  m_iLODLimits = iLODLimits;
  ScheduleCompleteRedraw();
}

void AbstrRenderer::SetColors(FLOATVECTOR4 ambient,
                              FLOATVECTOR4 diffuse,
                              FLOATVECTOR4 specular,
                              FLOATVECTOR3 lightDir) {
  m_cAmbient = ambient;
  m_cDiffuse = diffuse;
  m_cSpecular = specular;
  m_vLightDir = lightDir;

  UpdateLightParamsInShaders();

  if (m_eRenderMode == RM_ISOSURFACE)
    ScheduleRecompose();
  else
    if (m_bUseLighting) Schedule3DWindowRedraws();
}

FLOATVECTOR4 AbstrRenderer::GetAmbient() const {
  return m_cAmbient;
}

FLOATVECTOR4 AbstrRenderer::GetDiffuse() const {
  return m_cDiffuse;
}

FLOATVECTOR4 AbstrRenderer::GetSpecular()const {
  return m_cSpecular;
}

FLOATVECTOR3 AbstrRenderer::GetLightDir()const {
  return m_vLightDir;
}

void
AbstrRenderer::SetRenderRegions(const std::vector<RenderRegion*> &regions)
{
  this->renderRegions = regions;

  this->RestartTimers();
  this->msecPassedCurrentFrame = -1.0f;
}

void
AbstrRenderer::LuaSetRenderRegions(std::vector<LuaClassInstance> regions)
{
  shared_ptr<LuaScripting> ss = m_pMasterController->LuaScript();

  // Generate our own vector of render regions from the provided information.
  this->renderRegions.clear();
  this->renderRegions.reserve(regions.size());
  for (std::vector<LuaClassInstance>::iterator it = regions.begin();
      it != regions.end(); ++it)
  {
    this->renderRegions.push_back((*it).getRawPointer<RenderRegion>(ss));
  }

  this->RestartTimers();
  this->msecPassedCurrentFrame = -1.0f;
}

std::vector<LuaClassInstance> AbstrRenderer::LuaGetRenderRegions()
{
  shared_ptr<LuaScripting> ss = m_pMasterController->LuaScript();
  std::vector<LuaClassInstance> ret;
  ret.reserve(this->renderRegions.size());
  for (std::vector<RenderRegion*>::iterator it = this->renderRegions.begin();
      it != this->renderRegions.end(); ++it)
  {
    ret.push_back(ss->getLuaClassInstance(*it));
  }
  return ret;
}

void AbstrRenderer::RemoveMeshData(size_t index) {
  m_Meshes.erase(m_Meshes.begin()+index);
  Schedule3DWindowRedraws();
}

void AbstrRenderer::ReloadMesh(size_t index, const std::shared_ptr<Mesh> m) {
  m_Meshes[index]->Clone(m.get());
  Schedule3DWindowRedraws();
}

void AbstrRenderer::SetTimestep(size_t t) {
  if(t != m_iTimestep) {
    m_iTimestep = t;
    ScheduleCompleteRedraw();
  }
}
size_t AbstrRenderer::Timestep() const {
  return m_iTimestep;
}

void AbstrRenderer::SetUserMatrices(const FLOATMATRIX4& view, const FLOATMATRIX4& projection,
                                    const FLOATMATRIX4& viewLeft, const FLOATMATRIX4& projectionLeft,
                                    const FLOATMATRIX4& viewRight, const FLOATMATRIX4& projectionRight) {
  m_UserView = view;
  m_UserProjection = projection;
  m_UserViewLeft = viewLeft;
  m_UserProjectionLeft = projectionLeft;
  m_UserViewRight = viewRight;
  m_UserProjectionRight = projectionRight;

  m_bUserMatrices = true;
  Schedule3DWindowRedraws();
}

void AbstrRenderer::UnsetUserMatrices() {
  m_bUserMatrices = false;
  Schedule3DWindowRedraws();
}


void AbstrRenderer::InitStereoFrame() {
  m_iAlternatingFrameID = 0;
  Schedule3DWindowRedraws();
}

void AbstrRenderer::ToggleStereoFrame() {
  m_iAlternatingFrameID = 1-m_iAlternatingFrameID;
  ScheduleRecompose();
}

void AbstrRenderer::SetViewPos(const FLOATVECTOR3& vPos) {
  m_vAt += vPos-m_vEye;
  m_vEye = vPos;
  this->ScheduleCompleteRedraw();
}

FLOATVECTOR3 AbstrRenderer::GetViewPos() const {
  return m_vEye;
}

void AbstrRenderer::ResetViewPos() {
  m_vAt  = s_vAt;
  m_vEye = s_vEye;
  this->ScheduleCompleteRedraw();
}

void AbstrRenderer::SetViewDir(const FLOATVECTOR3& vDir) {
  m_vAt = vDir+m_vEye;
  this->ScheduleCompleteRedraw();
}

FLOATVECTOR3 AbstrRenderer::GetViewDir() const {
  return (m_vAt-m_vEye).normalized();
}

void AbstrRenderer::ResetViewDir() {
  m_vAt  = s_vAt;
  this->ScheduleCompleteRedraw();
}

void AbstrRenderer::SetUpDir(const FLOATVECTOR3& vDir) {
  m_vUp = vDir;
  this->ScheduleCompleteRedraw();
}

FLOATVECTOR3 AbstrRenderer::GetUpDir() const {
  return m_vUp.normalized();
}

void AbstrRenderer::ResetUpDir() {
  m_vUp  = s_vUp;
  this->ScheduleCompleteRedraw();
}

void AbstrRenderer::CycleDebugViews() {
  SetDebugView((m_iDebugView + 1) % GetDebugViewCount());
}

void AbstrRenderer::SetDebugView(uint32_t iDebugView) {
  m_iDebugView = iDebugView;
  this->ScheduleCompleteRedraw();
}

uint32_t AbstrRenderer::GetDebugView() const {
  return m_iDebugView;
}

uint32_t AbstrRenderer::GetDebugViewCount() const {
  return 1;
}

bool AbstrRenderer::Execute(const std::string& strCommand,
                            const std::vector<std::string>& strParams,
                            std::string& strMessage)
{
  strMessage = "";
  std::istringstream is;

  if(strCommand == "eye") {
    float x,y,z;
    if(strParams.size() != 3) {
      strMessage = "Invalid number of parameters.";
      return false;
    }
    is.str(strParams[0]); is >> x;
    is.str(strParams[1]); is >> y;
    is.str(strParams[2]); is >> z;
    this->SetViewParameters(m_fFOV, m_fZNear, m_fZFar, FLOATVECTOR3(x,y,z),
                            m_vAt, m_vUp);
  } else if(strCommand == "ref") {
    float x,y,z;
    if(strParams.size() != 3) {
      strMessage = "Invalid number of parameters.";
      return false;
    }
    is.str(strParams[0]); is >> x;
    is.str(strParams[1]); is >> y;
    is.str(strParams[2]); is >> z;
    this->SetViewParameters(m_fFOV, m_fZNear, m_fZFar, m_vEye,
                            FLOATVECTOR3(x,y,z), m_vUp);
  } else if(strCommand == "vup") {
    float x,y,z;
    if(strParams.size() != 3) {
      strMessage = "Invalid number of parameters.";
      return false;
    }
    is.str(strParams[0]); is >> x;
    is.str(strParams[1]); is >> y;
    is.str(strParams[2]); is >> z;
    this->SetViewParameters(m_fFOV, m_fZNear, m_fZFar, m_vEye, m_vAt,
                            FLOATVECTOR3(x,y,z));
  } else {
    strMessage = "Unhandled command.";
    return false;
  }
  return true;
}


void AbstrRenderer::SetInterpolant(Interpolant eInterpolant) {
  if (m_eInterpolant != eInterpolant) {
      m_eInterpolant = eInterpolant;
      ScheduleCompleteRedraw();
  }
}

LuaClassInstance AbstrRenderer::LuaGetDataset() {
  return m_pLuaDataset;
}

LuaClassInstance AbstrRenderer::LuaGet1DTrans() {
  return m_pLua1DTrans;
}

LuaClassInstance AbstrRenderer::LuaGet2DTrans() {
  return m_pLua2DTrans;
}

void
AbstrRenderer::SetFrustumCullingModelMatrix(const FLOATMATRIX4& modelMatrix) {
  m_FrustumCullingLOD.SetModelMatrix(modelMatrix);
}

int AbstrRenderer::GetFrustumCullingLODLevel(
    const FLOATVECTOR3& vfCenter,
    const FLOATVECTOR3& vfExtent,
    const UINTVECTOR3& viVoxelCount) const {
  return m_FrustumCullingLOD.GetLODLevel(vfCenter, vfExtent, viVoxelCount);
}

void AbstrRenderer::LuaCloneRenderMode(LuaClassInstance inst) {
  shared_ptr<LuaScripting> ss(m_pMasterController->LuaScript());
  AbstrRenderer* other = inst.getRawPointer<AbstrRenderer>(ss);

  this->SetUseLighting(other->GetUseLighting());
  this->SetSampleRateModifier(other->GetSampleRateModifier());
  this->SetGlobalBBox(other->GetGlobalBBox());
  this->SetLocalBBox(other->GetLocalBBox());

  if (other->IsClipPlaneEnabled())
    this->EnableClipPlane();
  else
    this->DisableClipPlane();

  this->SetIsosurfaceColor(other->GetIsosurfaceColor());
  this->SetIsoValue(other->GetIsoValue());
  this->SetCVIsoValue(other->GetCVIsoValue());
  this->SetCVSize(other->GetCVSize());
  this->SetCVContextScale(other->GetCVContextScale());
  this->SetCVBorderScale(other->GetCVBorderScale());
  this->SetCVColor(other->GetCVColor());
  this->SetCV(other->GetCV());
  this->SetCVFocusPosFVec(other->GetCVFocusPos());
  this->SetInterpolant(other->GetInterpolant());
}

void AbstrRenderer::ClearRendererMeshes() {
  m_Meshes.clear();
}

bool AbstrRenderer::CaptureSingleFrame(const std::string&, bool) const {
  return false;
}

/// Hacks!  These just do nothing.
void AbstrRenderer::PH_ClearWorkingSet() { }
void AbstrRenderer::PH_RecalculateVisibility() {}
bool AbstrRenderer::PH_Converged() const {
  AbstrRenderer* ren = const_cast<AbstrRenderer*>(this);
  return ren->CheckForRedraw();
}
bool AbstrRenderer::PH_OpenBrickAccessLogfile(const std::string&) { return false; }
bool AbstrRenderer::PH_CloseBrickAccessLogfile() { return false; }
bool AbstrRenderer::PH_OpenLogfile(const std::string&) { return false; }
bool AbstrRenderer::PH_CloseLogfile() { return false; }
void AbstrRenderer::PH_SetOptimalFrameAverageCount(size_t) { }
size_t AbstrRenderer::PH_GetOptimalFrameAverageCount() const { return 0; }
bool AbstrRenderer::PH_IsDebugViewAvailable() const { return false; }
bool AbstrRenderer::PH_IsWorkingSetTrackerAvailable() const { return false; }

std::vector<LuaClassInstance> AbstrRenderer::vecRegion(LuaClassInstance c) {
  std::vector<LuaClassInstance> vc(1);
  vc[0] = c;
  return vc;
}

void AbstrRenderer::RegisterLuaFunctions(
    LuaClassRegistration<AbstrRenderer>& reg,
    AbstrRenderer* me,
    LuaScripting* ss) {
  ss->vPrint("Registering abstract renderer functions.");

  std::string id;

  id = reg.function(&AbstrRenderer::GetRendererType,
                    "getRendererType",
                    "Retrieves the renderer type.", false);

  id = reg.function(&AbstrRenderer::GetRendererTarget,
                    "getRendererTarget",
                    "Renderer target specifies the interaction mode. "
                    "The two basic modes are interactive (standard ImageVis3D "
                    "mode), and high quality capture mode.", false);
  id = reg.function(&AbstrRenderer::SetRendererTarget,
                    "setRendererTarget",
                    "Specifies the renderer target. See getRendererTarget.",
                    true);
  id = reg.function(&AbstrRenderer::SetViewPos,
                    "setViewPos",
                    "Set the camera's position",
                    true);
  id = reg.function(&AbstrRenderer::GetViewPos,
                    "getViewPos",
                    "Retrieve the camera's position",
                    false);
  id = reg.function(&AbstrRenderer::ResetViewPos,
                    "resetViewPos",
                    "Reset the camera's position",
                    false);
  id = reg.function(&AbstrRenderer::SetViewDir,
                    "setViewDir",
                    "Set the camera's viewing direction",
                    true);
  id = reg.function(&AbstrRenderer::GetViewDir,
                    "getViewDir",
                    "Retrieve the camera's viewing direction",
                    false);
  id = reg.function(&AbstrRenderer::ResetViewDir,
                    "resetViewDir",
                    "Reset the camera's direction",
                    false);
  id = reg.function(&AbstrRenderer::SetUpDir,
                    "setUpDir",
                    "Set the camera's up direction",
                    true);
  id = reg.function(&AbstrRenderer::GetUpDir,
                    "getUpDir",
                    "Retrieve the camera's up direction",
                    false);
  id = reg.function(&AbstrRenderer::ResetUpDir,
                    "resetUpDir",
                    "Reset the camera's up direction",
                    false);
  id = reg.function(&AbstrRenderer::CycleDebugViews,
                    "cycleDebugViews",
                    "Cycle through available debug view modes",
                    true);
  id = reg.function(&AbstrRenderer::SetDebugView,
                    "setDebugView",
                    "Set whether debug view mode",
                    true);
  id = reg.function(&AbstrRenderer::GetDebugView,
                    "getDebugView",
                    "Retrieve current debug view mode",
                    true);
  id = reg.function(&AbstrRenderer::SetRendermode,
                    "setRenderMode",
                    "Set the render mode (1D transfer function, 2D transfer "
                    "function, etc...",
                    true);
  id = reg.function(&AbstrRenderer::GetRendermode,
                    "getRenderMode",
                    "Returns the renderer mode (1D transfer function, 2D "
                    "transfer function, etc...",
                    false);

  id = reg.function(&AbstrRenderer::LuaGetDataset,
                    "getDataset",
                    "Retrieves the renderer's current dataset.", false);

  id = reg.function(&AbstrRenderer::LuaGet1DTrans,
                    "get1DTrans", "", false);
  id = reg.function(&AbstrRenderer::LuaGet2DTrans,
                    "get2DTrans", "", false);

  id = reg.function(&AbstrRenderer::SetBackgroundColors,
                    "setBGColors",
                    "Sets the background colors.", true);
  ss->addParamInfo(id, 0, "topColor", "Top [0,1] RGB color.");
  ss->addParamInfo(id, 1, "botColor", "Bottom [0,1] RGB color");
  id = reg.function(&AbstrRenderer::GetBackgroundColor,
                    "getBackgroundColor", "Retrieves specified background "
                    "color.", false);

  id = reg.function(&AbstrRenderer::SetTextColor,
                    "setTextColor",
                    "Sets the text color.", true);
  ss->addParamInfo(id, 0, "textColor", "Text [0,1] RGBA color.");

  id = reg.function(&AbstrRenderer::SetBlendPrecision, "setBlendPrecision",
                    "Sets the blending precision to 8, 16, or 32 bit.", true);

  id = reg.function(&AbstrRenderer::SetLogoParams, "setLogoParams",
                    "Sets the filename and position of a logo.", true);
  ss->addParamInfo(id, 0, "filename", "Filename of the logo.");
  ss->addParamInfo(id, 1, "pos", "Position of the logo.");

  id = reg.function(&AbstrRenderer::LuaSetRenderRegions,
                    "setRenderRegions",
                    "Sets regions to render. See tuvok.renderRegion3D "
                    "and tuvok.renderRegion2D.",
                    true);
  ss->addParamInfo(id, 0, "regionArray", "Render region array.");

  id = reg.function(&AbstrRenderer::LuaGetRenderRegions,
                    "getRenderRegions",
                    "Retrieves the current render regions.", false);

  id = reg.function(&AbstrRenderer::LuaGetFirst3DRegion,
                    "getFirst3DRenderRegion",
                    "Retrieves the first 3D render region. A default instance "
                    "is returned if none is found.",
                    false);

  id = reg.function(&AbstrRenderer::ClipPlaneLocked,
                    "isClipPlaneLocked", "", false);
  id = reg.function(&AbstrRenderer::ClipPlaneRelativeLock,
                    "setClipPlaneLocked", "Sets relative clip plane lock.",
                    true);
  id = reg.function(&AbstrRenderer::ClipPlaneEnabled,
                    "isClipPlaneEnabled", "", false);
  id = reg.function(&AbstrRenderer::ClipPlaneShown,
                    "isClipPlaneShown", "", false);

  id = reg.function(&AbstrRenderer::SupportsClearView,
                    "supportsClearView", "Returns true if clear view is "
                    "supported by this renderer instance.", false);
  id = reg.function(&AbstrRenderer::SupportsMeshes,
                    "supportsMeshes", "Returns true if meshes are supported "
                    "by this renderer.", false);

  id = reg.function(&AbstrRenderer::SetCV, "setClearViewEnabled",
                    "Enables/Disables clear view.", true);
  id = reg.function(&AbstrRenderer::GetCV, "getClearViewEnabled",
                    "Returns clear view enabled state.", false);
  id = reg.function(&AbstrRenderer::ClearViewDisableReason,
                    "getClearViewDisabledReason", "", false);

  id = reg.function(&AbstrRenderer::SetRenderCoordArrows,
                    "setCoordinateArrowsEnabled", "Enables/Disables rendering "
                    "of coordinate axes.", true);
  id = reg.function(&AbstrRenderer::GetRenderCoordArrows,
                    "getCoordinateArrowsEnabled", "Returns coordinate axes "
                    "enabled state.", false);

  id = reg.function(&AbstrRenderer::Transfer3DRotationToMIP,
                    "transfer3DRotationToMIP", "Transfers current 3D rotation "
                    "into all 2D render regions. Has the effect of applying "
                    "3D rotation to maximal intensity projection.", true);
  id = reg.function(&AbstrRenderer::SetMIPRotationAngle,
                    "setMIPRotationAngle", "", true);
  id = reg.function(&AbstrRenderer::SetMIPLOD, "setMIPLODEnabled",
                    "Enables/Disables MIP LOD.", true);

  id = reg.function(&AbstrRenderer::LuaSet2DPlanesIn3DView,
                    "set2DPlanesIn3DView", "Adds 2D planes to the 3d view.",
                    true);
  id = reg.function(&AbstrRenderer::LuaGet2DPlanesIn3DView,
                    "get2DPlanesIn3DView", "Returns 2D planes in 3D view "
                    "enabled state.", false);

  id = reg.function(&AbstrRenderer::SetStereo,
                    "setStereoEnabled", "Enables/Disables stereo rendering.",
                    true);
  id = reg.function(&AbstrRenderer::GetStereo,
                    "getStereoEnabled", "Returns stereo enabled state.",
                    false);
  id = reg.function(&AbstrRenderer::GetStereoEyeDist,
                    "getStereoEyeDist", "", false);
  id = reg.function(&AbstrRenderer::SetStereoEyeDist,
                    "setStereoEyeDist", "", false);
  id = reg.function(&AbstrRenderer::GetStereoFocalLength,
                    "getStereoFocalLength", "", false);
  id = reg.function(&AbstrRenderer::SetStereoFocalLength,
                    "setStereoFocalLength", "", false);
  id = reg.function(&AbstrRenderer::GetStereoEyeSwap,
                    "getStereoEyeSwap", "", false);
  id = reg.function(&AbstrRenderer::SetStereoEyeSwap,
                    "setStereoEyeSwap", "", false);
  id = reg.function(&AbstrRenderer::GetStereoMode,
                    "getStereoMode", "", false);
  id = reg.function(&AbstrRenderer::SetStereoMode,
                    "setStereoMode", "", false);
  id = reg.function(&AbstrRenderer::InitStereoFrame,
                    "initStereoFrame", "", false);
  id = reg.function(&AbstrRenderer::ToggleStereoFrame,
                    "toggleStereoFrame", "", false);

  id = reg.function(&AbstrRenderer::SetTimeSlice,
                    "setTimeSlice", "", false);

  id = reg.function(&AbstrRenderer::ScheduleCompleteRedraw,
                    "scheduleCompleteRedraw", "", false);
  id = reg.function(&AbstrRenderer::CheckForRedraw,
                    "checkForRedraw", "", false);
  ss->setProvenanceExempt(id);  // This function was spamming the prov record.


  /// @todo This function is deprecated. Just delete the render window that
  /// created this abstract renderer, or delete the abstract renderer itself.
  id = reg.function(&AbstrRenderer::Cleanup, "cleanup",
                    "Deallocates GPU memory allocated during the rendering "
                    "process. Should always be called before deleting "
                    "the renderer.", false);

  /// @todo Pick was not exposed and subsequently removed from IV3D because
  ///       of a discussion I had with Tom. We will need to re-implement it
  ///       correctly.
//  id = reg.function(&AbstrRenderer::Pick,
//                    "pick", "Obtains an index for the volume element at the "
//                    "specified screen coordinates. Output is printed to "
//                    "the 'Other' debug channel.", false);

  id = reg.function(&AbstrRenderer::SetPerfMeasures, "setPerfMeasures",
                    "Deallocates GPU memory allocated during the rendering "
                    "process. Should always be called before deleting "
                    "the renderer.", false);
  ss->addParamInfo(id, 0, "minFramerate", "");
  ss->addParamInfo(id, 1, "renderLowResResults", "If true, renders low "
      "resolution intermediate results.");
  ss->addParamInfo(id, 2, "screenResDecFactor", "");
  ss->addParamInfo(id, 3, "sampleDecFactor", "");
  ss->addParamInfo(id, 4, "startDelay", "");

  id = reg.function(&AbstrRenderer::SetOrthoView, "setOrthoViewEnabled",
                    "Enables/Disables orthographic view.", true);
  id = reg.function(&AbstrRenderer::GetOrthoView, "getOrthoViewEnabled",
                    "Returns orthographic view enabled state.", false);

  id = reg.function(&AbstrRenderer::LuaCloneRenderMode, "cloneRenderMode",
                    "Clones this renderer's mode from the specified renderer.",
                    true);

  id = reg.function(&AbstrRenderer::SetUseLighting, "setLightingEnabled",
                    "Enables/Disables lighting.", true);
  id = reg.function(&AbstrRenderer::GetUseLighting, "getLightingEnabled",
                    "Returns lighting enabled state.", false);

  id = reg.function(&AbstrRenderer::SetSampleRateModifier,
                    "setSampleRateModifier", "Sets the multiplicator for the sampling rate, e.g. setting it to two will double the samples.", true);

  id = reg.function(&AbstrRenderer::SetFoV,
                    "setFoV", "Sets the angle/field of view for the virtual camera.", true);

  id = reg.function(&AbstrRenderer::GetFoV,
                    "getFoV", "Returns the angle/field of view for the virtual camera.", false);

  id = reg.function(&AbstrRenderer::SetIsoValue,
                    "setIsoValue", "changes the current isovalue", true);
  id = reg.function(&AbstrRenderer::SetIsoValueRelative,
                    "setIsoValueRelative", "changes isovalue; [0,1] range", true);
  id = reg.function(&AbstrRenderer::SetIsosurfaceColor,
                    "setIsosurfaceColor", "", true);
  id = reg.function(&AbstrRenderer::GetIsosurfaceColor,
                    "getIsosurfaceColor", "", false);

  id = reg.function(&AbstrRenderer::SetCVIsoValue,
                    "setCVIsoValue", "", true);
  id = reg.function(&AbstrRenderer::GetCVIsoValue,
                    "getCVIsoValue", "", false);
  id = reg.function(&AbstrRenderer::SetCVSize,
                    "setCVSize", "", true);
  id = reg.function(&AbstrRenderer::GetCVSize,
                    "getCVSize", "", false);
  id = reg.function(&AbstrRenderer::SetCVContextScale,
                    "setCVContextScale", "", true);
  id = reg.function(&AbstrRenderer::GetCVContextScale,
                    "getCVContextScale", "", false);
  id = reg.function(&AbstrRenderer::SetCVBorderScale,
                    "setCVBorderScale", "", true);
  id = reg.function(&AbstrRenderer::GetCVBorderScale,
                    "getCVBorderScale", "", false);
  id = reg.function(&AbstrRenderer::SetCVColor,
                    "setCVColor", "", true);
  id = reg.function(&AbstrRenderer::GetCVColor,
                    "getCVColor", "", true);
  id = reg.function(&AbstrRenderer::SetCVFocusPos,
                    "setCVFocusPos", "", true);

  id = reg.function(&AbstrRenderer::SetTimestep,
                    "setTimestep", "", true);

  id = reg.function(&AbstrRenderer::SetGlobalBBox,
                    "setGlobalBBox", "", true);
  id = reg.function(&AbstrRenderer::SetLocalBBox,
                    "setLocalBBox", "", true);

  id = reg.function(&AbstrRenderer::LuaShowClipPlane,
                    "showClipPlane", "", true);

  id = reg.function(&AbstrRenderer::GetSize,
                    "getSize", "", false);
  id = reg.function(&AbstrRenderer::Resize,
                    "resize", "", true);
  id = reg.function(&AbstrRenderer::Paint,
                    "paint", "", true);

  id = reg.function(&AbstrRenderer::GetCurrentSubFrameCount,
                    "getCurrentSubFrameCount", "", false);
  id = reg.function(&AbstrRenderer::GetWorkingSubFrame,
                    "getWorkingSubFrame", "", false);
  id = reg.function(&AbstrRenderer::GetCurrentBrickCount,
                    "getCurrentBrickCount", "", false);
  id = reg.function(&AbstrRenderer::GetWorkingBrick,
                    "getWorkingBrick", "", false);
  id = reg.function(&AbstrRenderer::GetMinLODIndex,
                    "getMinLODIndex", "", false);
  id = reg.function(&AbstrRenderer::GetMaxLODIndex,
                    "getMaxLODIndex", "", false);

  id = reg.function(&AbstrRenderer::SetConsiderPreviousDepthbuffer,
                    "setConsiderPrevDepthBuffer", "", true);
  id = reg.function(&AbstrRenderer::LoadDataset,
                    "loadDataset", "", true);
  id = reg.function(&AbstrRenderer::LoadRebricked,
                    "loadRebricked", "load a rebricked DS", true);

  id = reg.function(&AbstrRenderer::GetUseOnlyPowerOfTwo,
                    "getUseOnlyPowerOfTwo", "", false);
  id = reg.function(&AbstrRenderer::GetDownSampleTo8Bits,
                    "getDownSampleTo8Bits", "", false);
  id = reg.function(&AbstrRenderer::GetDisableBorder,
                    "getDisableBorder", "", false);

  id = reg.function(&AbstrRenderer::AddShaderPath,
                    "addShaderPath", "", true);

  id = reg.function(&AbstrRenderer::Initialize,
                    "initialize",
                    "Provenance is NOT recorded for this function. It should "
                    "be called from the renderer initialization code "
                    "(such that all provenance records are "
                    "children of the call to create the new renderer).",
                    false);
  ss->setProvenanceExempt(id);

  id = reg.function(&AbstrRenderer::SetLODLimits,
                    "setLODLimits", "", true);
  id = reg.function(&AbstrRenderer::SetColors,
                    "setLightColors", "", true);

  id = reg.function(&AbstrRenderer::SetInterpolant,
                    "setInterpolant", "", true);
  id = reg.function(&AbstrRenderer::GetInterpolant,
                    "getInterpolant", "", false);

  id = reg.function(&AbstrRenderer::GetAmbient,
                    "getAmbient", "", false);
  id = reg.function(&AbstrRenderer::GetDiffuse,
                    "getDiffuse", "", false);
  id = reg.function(&AbstrRenderer::GetSpecular,
                    "getSpecular", "", false);
  id = reg.function(&AbstrRenderer::GetLightDir,
                    "getLightDir", "", false);

  id = reg.function(&AbstrRenderer::SetDatasetIsInvalid,
                    "setDatasetIsInvalid", "", true);
  id = reg.function(&AbstrRenderer::RemoveMeshData,
                    "removeMeshData", "", true);
  id = reg.function(&AbstrRenderer::ScanForNewMeshes,
                    "scanForNewMeshes", "", true);
  id = reg.function(&AbstrRenderer::GetMeshes,
                    "getMeshes", "", false);
  ss->setProvenanceExempt(id);
  id = reg.function(&AbstrRenderer::ClearRendererMeshes,
                    "clearMeshes", "", true);

  id = reg.function(&AbstrRenderer::GetSampleRateModifier,
                    "getSampleRateModifier", "", false);
  id = reg.function(&AbstrRenderer::GetIsoValue,
                    "getIsoValue", "", false);
  id = reg.function(&AbstrRenderer::GetRescaleFactors,
                    "getRescaleFactors", "", false);
  id = reg.function(&AbstrRenderer::SetRescaleFactors,
                    "setRescaleFactors", "", false);
  id = reg.function(&AbstrRenderer::GetGlobalBBox,
                    "getGlobalBBox", "", false);
  id = reg.function(&AbstrRenderer::GetLocalBBox,
                    "getLocalBBox", "", false);

  id = reg.function(&AbstrRenderer::Timestep,
                    "getTimestep", "", false);
  id = reg.function(&AbstrRenderer::GetLODLimits,
                    "getLODLimits", "", false);

  id = reg.function(&AbstrRenderer::Schedule3DWindowRedraws,
                    "schedule3DWindowRedraws", "", false);
  id = reg.function(&AbstrRenderer::ReloadMesh,
                    "reloadMesh", "", false);
  id = reg.function(&AbstrRenderer::LuaGetVolumeAABBCenter,
                    "getVolumeAABBCenter", "", false);
  id = reg.function(&AbstrRenderer::LuaGetVolumeAABBExtents,
                    "getVolumeAABBExtents", "", false);
  id = reg.function(&AbstrRenderer::CropDataset,
                    "cropDataset", "", false);
  id = reg.function(&AbstrRenderer::LuaGetClipPlane,
                    "getClipPlane", "", false);

  id = reg.function(&AbstrRenderer::FixedFunctionality,
                    "fixedFunctionality", "", false);
  id = reg.function(&AbstrRenderer::SyncStateManager,
                    "syncStateManager", "", false);

  id = reg.function(&AbstrRenderer::PH_ClearWorkingSet,
                    "clearWorkingSet", "clears pool data", false);
  id = reg.function(&AbstrRenderer::PH_RecalculateVisibility,
                    "recalcVisibility", "synchronous!", false);
  id = reg.function(&AbstrRenderer::PH_Converged, "converged",
                    "checks if rendering converged", false);
  id = reg.function(&AbstrRenderer::PH_OpenBrickAccessLogfile, "openBALogfile",
                    "opens a new log file where brick access stats will be written to", false);
  id = reg.function(&AbstrRenderer::PH_CloseBrickAccessLogfile, "closeBALogfile",
                    "closes a open brick access log file", false);
  id = reg.function(&AbstrRenderer::PH_OpenLogfile, "openLogfile",
                    "opens a new log file where frame stats will be written to", false);
  id = reg.function(&AbstrRenderer::PH_CloseLogfile, "closeLogfile",
                    "closes a open log file", false);
  id = reg.function(&AbstrRenderer::PH_SetOptimalFrameAverageCount, "setOptimalFrameAveragingCount",
                    "re-renders the optimal frame", false);
  id = reg.function(&AbstrRenderer::PH_GetOptimalFrameAverageCount, "getOptimalFrameAveragingCount",
                    "returns the number of frames that get re-rendered", false);
  id = reg.function(&AbstrRenderer::PH_IsDebugViewAvailable, "isDebugViewAvailable",
                    "checks if debug view can be toggled", false);
  id = reg.function(&AbstrRenderer::PH_IsWorkingSetTrackerAvailable, "isWorkingSetTrackerAvailable",
                    "checks if working set is being tracked (bad performance)", false);

  id = reg.function(&AbstrRenderer::GetSubFrameProgress, "getSubFrameProgress",
                    "", false);
  id = reg.function(&AbstrRenderer::GetFrameProgress, "getFrameProgress",
                    "", false);
  id = reg.function(&AbstrRenderer::GetProjectionMatrix, "getProjectionMatrix",
                    "", false);

  id = reg.function(&AbstrRenderer::SetUserMatrices, "setUserMatrices",
                    "", true);
  id = reg.function(&AbstrRenderer::UnsetUserMatrices, "unsetUserMatrices",
                    "", true);

  id = reg.function(&AbstrRenderer::SetFrustumCullingModelMatrix,
                    "setFrustumCullingModelMatrix",
                    "", true);
  id = reg.function(&AbstrRenderer::GetFrustumCullingLODLevel,
                    "getFrustumCullingLODLevel",
                    "", true);

  id = reg.function(&AbstrRenderer::CaptureSingleFrame,
                    "captureSingleFrame", "Captures current FBO state.", true);
  reg.function(&AbstrRenderer::vecRegion, "createVecRegion", "creates a "
               "std::vector<LuaClassInstance> from a single LuaClassInstance",
               true);

  /// Register renderer specific functions.
  me->RegisterDerivedClassLuaFunctions(reg, ss);
}
