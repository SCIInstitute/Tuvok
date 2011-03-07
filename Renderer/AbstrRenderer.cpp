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
#include "AbstrRenderer.h"
#include <Controller/Controller.h>
#include <IO/Tuvok_QtPlugins.h>
#include <IO/IOManager.h>
#include <Renderer/GPUMemMan/GPUMemMan.h>
#include "Basics/MathTools.h"
#include "Basics/GeometryGenerator.h"
#include "IO/ExternalDataset.h"
#include "IO/TransferFunction1D.h"
#include "IO/TransferFunction2D.h"
#include "RenderMesh.h"
#include "Scripting/Scripting.h"

using namespace std;
using namespace tuvok;

static bool unregistered = true;

AbstrRenderer::AbstrRenderer(MasterController* pMasterController,
                             bool bUseOnlyPowerOfTwo, bool bDownSampleTo8Bits,
                             bool bDisableBorder, enum ScalingMethod sm) :
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
  m_bSupportsMeshes(false),
  msecPassedCurrentFrame(-1.0f),
  m_iLODNotOKCounter(0),
  decreaseScreenRes(false),
  decreaseScreenResNow(false),
  decreaseSamplingRate(false),
  decreaseSamplingRateNow(false),
  m_fMaxMSPerFrame(10000),
  m_fScreenResDecFactor(2.0f),
  m_fSampleDecFactor(2.0f),
  m_bUseAllMeans(false),
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
  m_iAlternatingFrameID(0),
  m_fStereoEyeDist(0.02f),
  m_fStereoFocalLength(1.0f),
  m_eStereoMode(SM_RB),
  m_bStereoEyeSwap(false),
  m_bDatasetIsInvalid(false),
  m_bUseOnlyPowerOfTwo(bUseOnlyPowerOfTwo),
  m_bDownSampleTo8Bits(bDownSampleTo8Bits),
  m_bDisableBorder(bDisableBorder),
  m_bAvoidSeparateCompositing(true),
  m_TFScalingMethod(sm),
  m_bClipPlaneOn(false),
  m_bClipPlaneDisplayed(true),
  m_bClipPlaneLocked(true),
  m_vEye(0,0,1.6f),
  m_vAt(0,0,0),
  m_vUp(0,1,0),
  m_fFOV(50.0f),
  m_fZNear(0.1f),
  m_fZFar(100.0f),
  m_cAmbient(1.0f,1.0f,1.0f,0.2f),
  m_cDiffuse(1.0f,1.0f,1.0f,0.8f),
  m_cSpecular(1.0f,1.0f,1.0f,1.0f),
  m_cAmbientM(1.0f,1.0f,1.0f,0.2f),
  m_cDiffuseM(1.0f,1.0f,1.0f,0.8f),
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
  m_vArrowGeometry = GeometryGenerator::GenArrow(0.3f,0.8f,0.006f,0.012f,20);

  if(unregistered) {
    RegisterCalls(m_pMasterController->ScriptEngine());
  }
}

bool AbstrRenderer::Initialize() {
  return m_pDataset != NULL;
}

bool AbstrRenderer::LoadDataset(const string& strFilename) {
  if (m_pMasterController == NULL) return false;

  if (m_pMasterController->IOMan() == NULL) {
    T_ERROR("Cannot load dataset because IOManager is NULL");
    return false;
  }

  m_pDataset = m_pMasterController->IOMan()->LoadDataset(strFilename,this);

  if (m_pDataset == NULL) {
    T_ERROR("IOManager call to load dataset failed.");
    return false;
  }

  MESSAGE("Load successful, initializing renderer!");

  Controller::Instance().Provenance("file", "open", strFilename);

  // find the maximum LOD index
  m_iMaxLODIndex = m_pDataset->GetLODLevelCount()-1;

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

AbstrRenderer::~AbstrRenderer() {
  if (m_pDataset) m_pMasterController->MemMan()->FreeDataset(m_pDataset, this);
  if (m_p1DTrans) m_pMasterController->MemMan()->Free1DTrans(m_p1DTrans, this);
  if (m_p2DTrans) m_pMasterController->MemMan()->Free2DTrans(m_p2DTrans, this);
}

static std::string render_mode(AbstrRenderer::ERenderMode mode) {
  switch(mode) {
    case AbstrRenderer::RM_1DTRANS: return "mode1d";
    case AbstrRenderer::RM_2DTRANS: return "mode2d";
    case AbstrRenderer::RM_ISOSURFACE: return "modeiso";
    default: return "invalid";
  };
}

void AbstrRenderer::SetRendermode(ERenderMode eRenderMode)
{
  if (m_eRenderMode != eRenderMode) {
    m_eRenderMode = eRenderMode;
    m_bFirstDrawAfterModeChange = true;
    ScheduleCompleteRedraw();
    Controller::Instance().Provenance("mode", render_mode(eRenderMode));
  }
}

void AbstrRenderer::SetUseLighting(bool bUseLighting) {
  if (m_bUseLighting != bUseLighting) {
    m_bUseLighting = bUseLighting;
    Schedule3DWindowRedraws();
    Controller::Instance().Provenance("light", "lighting");
  }
}

void AbstrRenderer::SetBlendPrecision(EBlendPrecision eBlendPrecision) {
  if (m_eBlendPrecision != eBlendPrecision) {
    m_eBlendPrecision = eBlendPrecision;
    ScheduleCompleteRedraw();
  }
}

void AbstrRenderer::SetDataset(Dataset *vds)
{
  if(m_pDataset) {
    Controller::Instance().MemMan()->FreeDataset(vds, this);
    delete m_pDataset;
  }
  m_pDataset = vds;
  Controller::Instance().MemMan()->AddDataset(m_pDataset, this);
  ScheduleCompleteRedraw();
  Controller::Instance().Provenance("file", "open", "<in_memory_buffer>");
}

/*
void AbstrRenderer::UpdateData(const BrickKey& bk,
                               std::tr1::shared_ptr<float> fp, size_t len)
{
  MESSAGE("Updating data with %u element array", static_cast<UINT32>(len));
  // free old data; we know we'll never need it, at this point.
  Controller::Instance().MemMan()->FreeAssociatedTextures(m_pDataset);
  dynamic_cast<ExternalDataset*>(m_pDataset)->UpdateData(bk, fp, len);
}
*/

void AbstrRenderer::Free1DTrans()
{
  GPUMemMan& mm = *(Controller::Instance().MemMan());
  mm.Free1DTrans(m_p1DTrans, this);
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
  AbstrDebugOut *dbg = m_pMasterController->DebugOut();
  if (m_eRenderMode != RM_2DTRANS) {
    dbg->Message(_func_,
                 "not using the 2D transferfunction at the moment, "
                 "ignoring message");
  } else {
    dbg->Message(_func_, "complete redraw scheduled");
    ScheduleCompleteRedraw();
    // No provenance; handled by application, not Tuvok lib.
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
        this->doAnotherRedrawDueToAllMeans) {
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


void AbstrRenderer::EnableClipPlane(RenderRegion *renderRegion) {
  if (!renderRegion)
    renderRegion = GetFirst3DRegion();
  if (renderRegion) {
    if(!m_bClipPlaneOn) {
      m_bClipPlaneOn = true; /// @todo: Make this per RenderRegion.
      ScheduleWindowRedraw(renderRegion);
      Controller::Instance().Provenance("clip", "clip", "0.0");
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
      Controller::Instance().Provenance("clip", "clip", "disable");
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
      Controller::Instance().Provenance("clip", "showclip", "enable");
    }
  }
}

const ExtendedPlane& AbstrRenderer::GetClipPlane() const {
  return m_ClipPlane;
}

void AbstrRenderer::ClipPlaneRelativeLock(bool bRel) {
  m_bClipPlaneLocked = bRel;/// @todo: Make this per RenderRegion ?
}

void AbstrRenderer::SetSliceDepth(RenderRegion *renderRegion, UINT64 sliceDepth) {
  if (renderRegion->GetSliceIndex() != sliceDepth) {
    renderRegion->SetSliceIndex(sliceDepth);
    ScheduleWindowRedraw(renderRegion);
    if (m_bRenderPlanesIn3D)
      Schedule3DWindowRedraws();
  }
}

UINT64 AbstrRenderer::GetSliceDepth(const RenderRegion *renderRegion) const {
  return renderRegion->GetSliceIndex();
}

void AbstrRenderer::SetGlobalBBox(bool bRenderBBox) {
  m_bRenderGlobalBBox = bRenderBBox; /// @todo: Make this per RenderRegion.
  Schedule3DWindowRedraws();
  Controller::Instance().Provenance("boundingbox", "global_bbox");
}

void AbstrRenderer::SetLocalBBox(bool bRenderBBox) {
  m_bRenderLocalBBox = bRenderBBox; /// @todo: Make this per RenderRegion.
  Schedule3DWindowRedraws();
  Controller::Instance().Provenance("boundingbox", "local_bbox");
}

void AbstrRenderer::ScheduleCompleteRedraw() {
  m_iCheckCounter = m_iStartDelay;

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
    if(!m_bAvoidSeparateCompositing && // ensure we finished the current frame:
       m_vCurrentBrickList.size() == m_iBricksRenderedInThisSubFrame) {
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
      // chosen more or less arbitrary, can be changed if needed)
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
        UINT64 iPerformanceBasedLODSkip =
          std::max<UINT64>(1, m_iPerformanceBasedLODSkip) - 1;
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
          if (m_bUseAllMeans) {
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
          UINT64 iPerformanceBasedLODSkip =
            std::min<UINT64>(m_iMaxLODIndex - m_iMinLODForCurrentView,
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
                               static_cast<UINT64>(m_iMaxLODIndex -
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
  m_iMinLODForCurrentView = static_cast<UINT64>(
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

  for (UINT32 iBrick = 0;iBrick<vBrickList.size();iBrick++) {
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
         !this->doAnotherRedrawDueToAllMeans;
}

bool AbstrRenderer::RegionNeedsBrick(const RenderRegion& rr,
                                     const BrickKey& key,
                                     const BrickMD& bmd,
                                     bool& bIsEmptyButInFrustum) const
{
  if(rr.is2D()) {
    return rr.GetUseMIP() ||
           std::tr1::get<1>(key) == m_pDataset->GetLODLevelCount()-1;
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
    return false;
  }

  // skip the brick if the clipping plane removes it.
  if(m_bClipPlaneOn && Clipped(rr, b)) {
    return false;
  }

  // finally, query the data in the brick; if no data can possibly be visible,
  // don't render this brick.
  bIsEmptyButInFrustum = !ContainsData(key);

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
  double fMaxValue = MaxValue();
  double fRescaleFactor = fMaxValue / double(m_p1DTrans->GetSize());

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

  UINTVECTOR3 vOverlap = m_pDataset->GetBrickOverlapSize();
  UINT64VECTOR3 vDomainSize = m_pDataset->GetDomainSize(size_t(m_iCurrentLOD));
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
    if(std::tr1::get<0>(brick->first) != m_iTimestep ||
       std::tr1::get<1>(brick->first) != m_iCurrentLOD) {
      continue;
    }
    const BrickMD& bmd = brick->second;
    Brick b;
    b.vExtension = bmd.extents * vScale;
    b.vCenter = bmd.center * vScale;
    b.vVoxelCount = bmd.n_voxels;
    b.kBrick = brick->first;

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
              static_cast<unsigned>(std::tr1::get<0>(brick->first)),
              static_cast<unsigned>(std::tr1::get<1>(brick->first)),
              static_cast<unsigned>(std::tr1::get<2>(brick->first)));
      continue;
    } 
    
    if(b.bIsEmpty) {
      MESSAGE("Skipping further computations for brick <%u,%u,%u> "
              "because it is empty/invisible given the current vis parameters "
              "but we keep it in the list in case it overlaps with other data"
              "e.g. a mesh",
              static_cast<unsigned>(std::tr1::get<0>(brick->first)),
              static_cast<unsigned>(std::tr1::get<1>(brick->first)),
              static_cast<unsigned>(std::tr1::get<2>(brick->first)));
    } else {
      bool first_x = m_pDataset->BrickIsFirstInDimension(0, brick->first);
      bool first_y = m_pDataset->BrickIsFirstInDimension(1, brick->first);
      bool first_z = m_pDataset->BrickIsFirstInDimension(2, brick->first);
      bool last_x = m_pDataset->BrickIsLastInDimension(0, brick->first);
      bool last_y = m_pDataset->BrickIsLastInDimension(1, brick->first);
      bool last_z = m_pDataset->BrickIsLastInDimension(2, brick->first);
      // compute texture coordinates
      if (m_bUseOnlyPowerOfTwo) {
        UINTVECTOR3 vRealVoxelCount(MathTools::NextPow2(b.vVoxelCount.x),
                                    MathTools::NextPow2(b.vVoxelCount.y),
                                    MathTools::NextPow2(b.vVoxelCount.z));
        b.vTexcoordsMin = FLOATVECTOR3(
          (first_x) ? 0.5f/vRealVoxelCount.x : vOverlap.x*0.5f/vRealVoxelCount.x,
          (first_y) ? 0.5f/vRealVoxelCount.y : vOverlap.y*0.5f/vRealVoxelCount.y,
          (first_z) ? 0.5f/vRealVoxelCount.z : vOverlap.z*0.5f/vRealVoxelCount.z
        );
        b.vTexcoordsMax = FLOATVECTOR3(
          (last_x) ? 1.0f-0.5f/vRealVoxelCount.x : 1.0f-vOverlap.x*0.5f/vRealVoxelCount.x,
          (last_y) ? 1.0f-0.5f/vRealVoxelCount.y : 1.0f-vOverlap.y*0.5f/vRealVoxelCount.y,
          (last_z) ? 1.0f-0.5f/vRealVoxelCount.z : 1.0f-vOverlap.z*0.5f/vRealVoxelCount.z
        );

        b.vTexcoordsMax -= FLOATVECTOR3(vRealVoxelCount - b.vVoxelCount) /
                           FLOATVECTOR3(vRealVoxelCount);
      } else {
        // compute texture coordinates
        b.vTexcoordsMin = FLOATVECTOR3(
          (first_x) ? 0.5f/b.vVoxelCount.x : vOverlap.x*0.5f/b.vVoxelCount.x,
          (first_y) ? 0.5f/b.vVoxelCount.y : vOverlap.y*0.5f/b.vVoxelCount.y,
          (first_z) ? 0.5f/b.vVoxelCount.z : vOverlap.z*0.5f/b.vVoxelCount.z
        );
        // for padded volume adjust texcoords
        b.vTexcoordsMax = FLOATVECTOR3(
          (last_x) ? 1.0f-0.5f/b.vVoxelCount.x : 1.0f-vOverlap.x*0.5f/b.vVoxelCount.x,
          (last_y) ? 1.0f-0.5f/b.vVoxelCount.y : 1.0f-vOverlap.y*0.5f/b.vVoxelCount.y,
          (last_z) ? 1.0f-0.5f/b.vVoxelCount.z : 1.0f-vOverlap.z*0.5f/b.vVoxelCount.z
        );
      }

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

bool AbstrRenderer::IsVolumeResident(const BrickKey& key) {
  // normally we use "real" 3D textures so implement this method
  // for 3D textures, it is overriden by 2D texture children
  return m_pMasterController->MemMan()->IsResident(m_pDataset, key,
                                                   m_bUseOnlyPowerOfTwo,
                                                   m_bDownSampleTo8Bits,
                                                   m_bDisableBorder,
                                                   false);
}

void AbstrRenderer::GetVolumeAABB(FLOATVECTOR3& vCenter, FLOATVECTOR3& vExtend) {
  UINT64VECTOR3 vDomainSize = m_pDataset->GetDomainSize();
  FLOATVECTOR3 vScale = FLOATVECTOR3(m_pDataset->GetScale());
  
  vExtend = FLOATVECTOR3(vDomainSize) * vScale;
  vExtend /= vExtend.maxVal();
  vCenter = FLOATVECTOR3(0,0,0);
}

struct Blank: public std::unary_function<RenderRegion, bool> {
  bool operator()(const RenderRegion* rr) const { return rr->isBlank; }
};


void AbstrRenderer::PlanFrame(RenderRegion3D& region) {
  typedef std::vector<RenderRegion*>::iterator r_iter;
  for(r_iter r = renderRegions.begin(); r != renderRegions.end(); ++r) {
    RenderRegion& rr = **r;
    if(rr.isBlank) {
      rr.modelView[0] = rr.rotation * rr.translation * m_mView[0];
      if(m_bDoStereoRendering) {
        rr.modelView[1] = rr.rotation * rr.translation * m_mView[1];
      }
    }
    // HACK.  We know there will only be one 3D region.  However, this logic is
    // really too simple; we can't just throw away a brick if it is outside a
    // region's view frustum: it must be outside *all* regions' view frustums.
    if(rr.is3D()) {
      m_FrustumCullingLOD.SetViewMatrix(rr.modelView[0]);
      m_FrustumCullingLOD.Update();
    }
  }

  // let the mesh know about our current state, technically
  // SetVolumeAABB only needs to be called when the geometry of volume
  // has changed (rescale) and SetUserPos only when the view has 
  // changed (matrix update) but the mesh class is smart enougth to catch 
  // redundant changes so we just leave the code here for now
  if (m_bSupportsMeshes) {
    FLOATVECTOR3 vCenter, vExtend;
    GetVolumeAABB(vCenter, vExtend);
    FLOATVECTOR3 vMinPoint = vCenter-vExtend/2.0, 
                 vMaxPoint = vCenter+vExtend/2.0;

     for (vector<RenderMesh*>::iterator mesh = m_Meshes.begin();
         mesh != m_Meshes.end(); mesh++) {
      if ((*mesh)->GetActive()) {
        (*mesh)->SetVolumeAABB(vMinPoint, vMaxPoint);
        (*mesh)->SetUserPos( (FLOATVECTOR4(0,0,0,1)*
                               region.modelView[0].inverse()).xyz() );
      }
    }
  }

  // are any regions blank?
  bool blank = std::find_if(this->renderRegions.begin(),
                            this->renderRegions.end(),
                            Blank()) != this->renderRegions.end();

  // if we found a blank region, we need to reset and do some actual
  // planning.
  if(blank) {
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
  if (blank ||
      (m_vCurrentBrickList.size() == m_iBricksRenderedInThisSubFrame)) {
    bool bBuildNewList = false;
    if (blank) {
      this->decreaseSamplingRateNow = this->decreaseSamplingRate;
      this->decreaseScreenResNow = this->decreaseScreenRes;
      bBuildNewList = true;
      if (this->decreaseSamplingRateNow || this->decreaseScreenResNow)
        this->doAnotherRedrawDueToAllMeans = true;
    } else {
      if (this->decreaseSamplingRateNow || this->decreaseScreenResNow) {
        this->decreaseScreenResNow = false;
        this->decreaseSamplingRateNow = false;
        m_iBricksRenderedInThisSubFrame = 0;
        this->doAnotherRedrawDueToAllMeans = false;
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
        m_iCurrentLOD = std::min<UINT64>(m_iCurrentLODOffset,
                                         m_pDataset->GetLODLevelCount()-1);
      }
      // build new brick todo-list
      MESSAGE("Building new brick list for LOD %llu...", m_iCurrentLOD);
      m_vCurrentBrickList = BuildSubFrameBrickList();
      MESSAGE("%u bricks made the cut.", UINT32(m_vCurrentBrickList.size()));
      if (m_bDoStereoRendering) {
        m_vLeftEyeBrickList =
          BuildLeftEyeSubFrameBrickList(region.modelView[1], m_vCurrentBrickList);
      }

      m_iBricksRenderedInThisSubFrame = 0;
    }
  }

  if(blank) {
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
    m_iCurrentLOD = min<UINT64>(m_pDataset->GetLODLevelCount()-1,
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
    if (m_eRenderMode == RM_ISOSURFACE)
      Schedule3DWindowRedraws();
  }
}

void AbstrRenderer::SetIsosufaceColor(const FLOATVECTOR3& vColor) {
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

void AbstrRenderer::SetCVIsoValue(float fIsovalue) {
  if (m_fCVIsovalue != fIsovalue) {
    m_fCVIsovalue = fIsovalue;

    if (m_bDoClearView && m_eRenderMode == RM_ISOSURFACE) {
      Schedule3DWindowRedraws();
    }
    std::ostringstream prov;
    prov << fIsovalue;
    Controller::Instance().Provenance("cv", "setcviso", prov.str());
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

void AbstrRenderer::SetCVFocusPos(const FLOATVECTOR4& vCVPos) {
  m_vCVMousePos = INTVECTOR2(-1,-1);
  m_vCVPos = vCVPos;
}

void AbstrRenderer::SetCVFocusPos(const RenderRegion& renderRegion,
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

void AbstrRenderer::CVFocusHasChanged(const RenderRegion &) {
  ScheduleRecompose();
}

bool AbstrRenderer::RGBAData() const {
  // right now we just look for 4-component data, and assume all such data is
  // RGBA... at some point we probably want to add some sort of query into the
  // tuvok::Dataset, so that a file format could decide whether or not it wants
  // to consider 4-component data to be RGBA data.
  return m_pDataset->GetComponentCount() == 4;
}

void AbstrRenderer::SetConsiderPreviousDepthbuffer(bool bConsiderPreviousDepthbuffer) {
  if (m_bConsiderPreviousDepthbuffer != bConsiderPreviousDepthbuffer)
  {
    m_bConsiderPreviousDepthbuffer = bConsiderPreviousDepthbuffer;
    ScheduleCompleteRedraw();
  }
}

void AbstrRenderer::SetPerfMeasures(UINT32 iMinFramerate, bool bUseAllMeans,
                                    float fScreenResDecFactor,
                                    float fSampleDecFactor, UINT32 iStartDelay) {
  m_fMaxMSPerFrame = (iMinFramerate == 0) ? 10000 : 1000.0f / float(iMinFramerate);
  m_fScreenResDecFactor = fScreenResDecFactor;
  m_fSampleDecFactor = fSampleDecFactor;
  m_bUseAllMeans = bUseAllMeans;

  if (!m_bUseAllMeans) {
    this->decreaseScreenRes = false;
    this->decreaseScreenResNow = false;
    this->decreaseSamplingRate = false;
    this->decreaseSamplingRateNow = false;
    this->doAnotherRedrawDueToAllMeans = false;
  }

  m_iStartDelay = iStartDelay;

  ScheduleCompleteRedraw();
}

void AbstrRenderer::SetLODLimits(const UINTVECTOR2 iLODLimits) {
  m_iLODLimits = iLODLimits;
  ScheduleCompleteRedraw();
}

void AbstrRenderer::SetColors(const FLOATVECTOR4& ambient,
                              const FLOATVECTOR4& diffuse,
                              const FLOATVECTOR4& specular,
                              const FLOATVECTOR3& lightDir) {
  m_cAmbient = ambient;
  m_cDiffuse = diffuse;
  m_cSpecular = specular;
  m_vLightDir = lightDir;

  UpdateLightParamsInShaders();
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

void AbstrRenderer::RemoveMeshData(size_t index) {
  delete m_Meshes[index];
  m_Meshes[index] = NULL;
  m_Meshes.erase(m_Meshes.begin()+index);
  Schedule3DWindowRedraws();
}

void AbstrRenderer::ReloadMesh(size_t index, const Mesh* m) {
  m_Meshes[index]->Clone(m);
  Schedule3DWindowRedraws();
}

void AbstrRenderer::Timestep(size_t t) {
  if(t != m_iTimestep) {
    m_iTimestep = t;
    ScheduleCompleteRedraw();
  }
}
size_t AbstrRenderer::Timestep() const { 
  return m_iTimestep; 
}

void AbstrRenderer::InitStereoFrame() {
  m_iAlternatingFrameID = 0; 
  Schedule3DWindowRedraws();
}

void AbstrRenderer::ToggleStereoFrame() {
  m_iAlternatingFrameID = 1-m_iAlternatingFrameID;
  ScheduleRecompose();
}

void AbstrRenderer::ResetRenderStates() {
  m_bFirstDrawAfterResize = false;
  m_bFirstDrawAfterModeChange = false;
}

void AbstrRenderer::RegisterCalls(Scripting* eng) {
  eng->RegisterCommand(this, "eye", "float float float", "set eye position");
  eng->RegisterCommand(this, "ref", "float float float",
                       "set camera focus position (what the camera points at)");
  eng->RegisterCommand(this, "vup", "float float float", "set view up vector");
  unregistered = false;
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
