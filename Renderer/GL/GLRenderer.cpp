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
  \file    GLRenderer.cpp
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    August 2008
*/

#include <algorithm>
#include <cstdarg>
#include <iterator>
#include <stdexcept>
#include <typeinfo>
#include "GLInclude.h"
#include "GLFBOTex.h"
#include "GLRenderer.h"
#include "GLSLProgram.h"
#include "GLTexture1D.h"
#include "GLTexture2D.h"
#include "GLVolume3DTex.h"
#include <Controller/Controller.h>
#include <Basics/SystemInfo.h>
#include <Basics/SysTools.h>
#include <Basics/MathTools.h>
#include "IO/FileBackedDataset.h"
#include "IO/TransferFunction1D.h"
#include "IO/TransferFunction2D.h"
#include "../GPUMemMan/GPUMemMan.h"
#include "../../Basics/GeometryGenerator.h"
#include "../SBVRGeogen.h"
#include "../Context.h"

using namespace std;
using namespace tuvok;

GLRenderer::GLRenderer(MasterController* pMasterController, 
                       bool bUseOnlyPowerOfTwo,
                       bool bDownSampleTo8Bits, 
                       bool bDisableBorder) :
  AbstrRenderer(pMasterController, 
                bUseOnlyPowerOfTwo, 
                bDownSampleTo8Bits,
                bDisableBorder),
  m_TargetBinder(pMasterController),
  m_p1DTransTex(NULL),
  m_p2DTransTex(NULL),
  m_p2DData(NULL),
  m_pFBO3DImageLast(NULL),
  m_pFBOResizeQuickBlit(NULL),
  m_pLogoTex(NULL),
  m_pProgramIso(NULL),
  m_pProgramColor(NULL),
  m_pProgramHQMIPRot(NULL),
  m_pGLVolume(NULL),
  m_bSortMeshBTF(false),
  m_GeoBuffer(0),
  m_iNumTransMeshes(0),
  m_iNumMeshes(0),
  m_pProgramTrans(NULL),
  m_pProgram1DTransSlice(NULL),
  m_pProgram2DTransSlice(NULL),
  m_pProgram1DTransSlice3D(NULL),
  m_pProgram2DTransSlice3D(NULL),
  m_pProgramMIPSlice(NULL),
  m_pProgramTransMIP(NULL),
  m_pProgramIsoCompose(NULL),
  m_pProgramColorCompose(NULL),
  m_pProgramCVCompose(NULL),
  m_pProgramComposeAnaglyphs(NULL),
  m_pProgramComposeScanlineStereo(NULL),
  m_pProgramSBSStereo(NULL),
  m_pProgramAFStereo(NULL),
  m_pProgramBBox(NULL),
  m_pProgramMeshFTB(NULL),
  m_pProgramMeshBTF(NULL),
  m_texFormat16(GL_RGBA16),
  m_texFormat32(GL_RGBA),
  m_aDepthStorage(NULL)
{
  m_pProgram1DTrans[0]   = NULL;
  m_pProgram1DTrans[1]   = NULL;
  m_pProgram2DTrans[0]   = NULL;
  m_pProgram2DTrans[1]   = NULL;

  m_pFBO3DImageCurrent[0] = NULL;
  m_pFBOIsoHit[0] = NULL;
  m_pFBOCVHit[0] = NULL;
  m_pFBO3DImageCurrent[1] = NULL;
  m_pFBOIsoHit[1] = NULL;
  m_pFBOCVHit[1] = NULL;
}

GLRenderer::~GLRenderer() {
  delete [] m_p2DData;

  for (vector<RenderMesh*>::iterator mesh = m_Meshes.begin();
       mesh != m_Meshes.end(); mesh++) {
    delete (*mesh);
  }
  m_Meshes.clear();

  DeleteDepthStorage();
}

void GLRenderer::InitBaseState() {
  // first get the current state
  m_BaseState = GPUState(m_pContext->GetStateManager()->GetCurrentState());

  // now set gl parameters how we use them most of the time
  m_BaseState.enableDepthTest = true;
  m_BaseState.depthFunc = DF_LESS;
  m_BaseState.enableCullFace = false;
  m_BaseState.cullState = CULL_BACK;
  m_BaseState.enableBlend = true;
  m_BaseState.enableScissor = false;
  m_BaseState.enableLighting =false;
  m_BaseState.enableColorMaterial = false;
  m_BaseState.enableTex[0] = TEX_3D;
  m_BaseState.enableTex[1] = TEX_2D;
  m_BaseState.activeTexUnit = 0;
  m_BaseState.depthMask = true;
  m_BaseState.colorMask = true;
  m_BaseState.blendEquation = BE_FUNC_ADD;
  m_BaseState.blendFuncSrc = BF_ONE_MINUS_DST_ALPHA;;
  m_BaseState.blendFuncDst = BF_ONE;
  m_BaseState.lineWidth = 1.0f;
}

// Some drivers do not support floating point textures.
static GLenum driver_supports_fp_textures()
{
  return glewGetExtension("GL_ARB_texture_float");
}

bool GLRenderer::Initialize(std::tr1::shared_ptr<Context> ctx) {
  if (!AbstrRenderer::Initialize(ctx)) {
    T_ERROR("Error in parent call -> aborting");
    return false;
  }

  InitBaseState();

  // Try to guess filenames for a transfer functions.  We guess based on the
  // filename of the dataset, but it could be the case that our client gave us
  // an in-memory dataset.
  std::string strPotential1DTransName;
  std::string strPotential2DTransName;
  try {
    FileBackedDataset& ds = dynamic_cast<FileBackedDataset&>(*m_pDataset);
    strPotential1DTransName = SysTools::ChangeExt(ds.Filename(), "1dt");
    strPotential2DTransName = SysTools::ChangeExt(ds.Filename(), "2dt");
  } catch(std::bad_cast) {
    // Will happen when we don't have a file-backed dataset; just disable the
    // filename-guessing / automatic TF loading feature.
    strPotential1DTransName = "";
    strPotential2DTransName = "";
  }

  GPUMemMan &mm = *(Controller::Instance().MemMan());
  if (SysTools::FileExists(strPotential1DTransName)) {
    MESSAGE("Loading 1D TF from file.");
    mm.Get1DTransFromFile(strPotential1DTransName, this,
                          &m_p1DTrans, &m_p1DTransTex,
                          m_pDataset->Get1DHistogram().GetFilledSize());
  } else {
    MESSAGE("Creating empty 1D TF.");
    mm.GetEmpty1DTrans(m_pDataset->Get1DHistogram().GetFilledSize(), this,
                       &m_p1DTrans, &m_p1DTransTex);
  }

  if (SysTools::FileExists(strPotential2DTransName)) {
    mm.Get2DTransFromFile(strPotential2DTransName, this,
                          &m_p2DTrans, &m_p2DTransTex,
                          m_pDataset->Get2DHistogram().GetFilledSize());
    if(m_p2DTrans == NULL) {
      WARNING("Falling back to empty 2D TFqn...");
      mm.GetEmpty2DTrans(m_pDataset->Get2DHistogram().GetFilledSize(), this,
                         &m_p2DTrans, &m_p2DTransTex);
    }
  } else {
    mm.GetEmpty2DTrans(m_pDataset->Get2DHistogram().GetFilledSize(), this,
                       &m_p2DTrans, &m_p2DTransTex);

    // Setup a default polygon in the 2D TF, so it doesn't look like they're
    // broken (nothing is rendered) when the user first switches to 2D TF mode.
    TFPolygon newSwatch;
    newSwatch.pPoints.push_back(FLOATVECTOR2(0.1f,0.1f));
    newSwatch.pPoints.push_back(FLOATVECTOR2(0.1f,0.9f));
    newSwatch.pPoints.push_back(FLOATVECTOR2(0.9f,0.9f));
    newSwatch.pPoints.push_back(FLOATVECTOR2(0.9f,0.1f));

    newSwatch.pGradientCoords[0] = FLOATVECTOR2(0.1f,0.5f);
    newSwatch.pGradientCoords[1] = FLOATVECTOR2(0.9f,0.5f);

    GradientStop g1(0.0f,FLOATVECTOR4(0,0,0,0)),
                 g2(0.5f,FLOATVECTOR4(1,1,1,1)),
                 g3(1.0f,FLOATVECTOR4(0,0,0,0));
    newSwatch.pGradientStops.push_back(g1);
    newSwatch.pGradientStops.push_back(g2);
    newSwatch.pGradientStops.push_back(g3);

    m_p2DTrans->m_Swatches.push_back(newSwatch);
    m_pMasterController->MemMan()->Changed2DTrans(NULL, m_p2DTrans);
  }

  for (vector<RenderMesh*>::iterator mesh = m_Meshes.begin();
       mesh != m_Meshes.end(); mesh++) {
    (*mesh)->InitRenderer();
  }

  GL(glGenBuffers(1, &m_GeoBuffer));

  this->m_texFormat16 = GL_RGBA16;
  this->m_texFormat32 = GL_RGBA;
  if(driver_supports_fp_textures()) {
    MESSAGE("Flaoting point textures supported (yay!)");
    this->m_texFormat16 = GL_RGBA16F_ARB;
    this->m_texFormat32 = GL_RGBA32F_ARB;
  }

  return LoadShaders();
}

bool GLRenderer::LoadShaders(const string& volumeAccessFunction, bool bBindVolume) {
  const std::string tfqn = m_pDataset
                           ? m_pDataset->GetComponentCount() == 4
                              ? "vr-col-tfqn.glsl"
                              : "vr-scal-tfqn.glsl"
                           : "vr-scal-tfqn.glsl";
  const std::string tfqnLit = m_pDataset
                           ? (m_pDataset->GetComponentCount() == 3 ||
                              m_pDataset->GetComponentCount() == 4)
                              ? "vr-col-tfqn-lit.glsl"
                              : "vr-scal-tfqn-lit.glsl"
                           : "vr-scal-tfqn.glsl";

  MESSAGE("Loading '%s' volume rendering...", tfqn.c_str());

  if(!LoadAndVerifyShader(&m_pProgramTrans, m_vShaderSearchDirs,
                          "Transfer-VS.glsl",
                          NULL,
                          "Transfer-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgram1DTransSlice, m_vShaderSearchDirs,
                          "Transfer-VS.glsl",
                          NULL,
                          tfqn.c_str(), "lighting.glsl",
                          "1D-slice-FS.glsl", volumeAccessFunction.c_str(), NULL) ||
     !LoadAndVerifyShader(&m_pProgram2DTransSlice, m_vShaderSearchDirs,
                          "Transfer-VS.glsl",
                          NULL,
                           "2D-slice-FS.glsl", volumeAccessFunction.c_str(), NULL) ||
     !LoadAndVerifyShader(&m_pProgramMIPSlice, m_vShaderSearchDirs,
                          "Transfer-VS.glsl",
                          NULL,
                           "MIP-slice-FS.glsl", volumeAccessFunction.c_str(), NULL) ||
     !LoadAndVerifyShader(&m_pProgram1DTransSlice3D, m_vShaderSearchDirs,
                          "SlicesIn3D.glsl",
                          NULL,
                          tfqn.c_str(), "lighting.glsl",
                           "1D-slice-FS.glsl", volumeAccessFunction.c_str(), NULL) ||
     !LoadAndVerifyShader(&m_pProgram2DTransSlice3D, m_vShaderSearchDirs,
                          "SlicesIn3D.glsl",
                          NULL,
                           "2D-slice-FS.glsl", volumeAccessFunction.c_str(), NULL) ||
     !LoadAndVerifyShader(&m_pProgramTransMIP, m_vShaderSearchDirs,
                          "Transfer-VS.glsl",
                          NULL,
                           "Transfer-MIP-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramIsoCompose, m_vShaderSearchDirs,
                          "Transfer-VS.glsl",
                          NULL,
                           "Compose-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramColorCompose, m_vShaderSearchDirs,
                          "Transfer-VS.glsl",
                          NULL,
                          "Compose-Color-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramCVCompose, m_vShaderSearchDirs,
                          "Transfer-VS.glsl",
                          NULL,
                          "Compose-CV-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramComposeAnaglyphs, m_vShaderSearchDirs,
                          "Transfer-VS.glsl",
                          NULL,
                          "Compose-Anaglyphs-FS.glsl",
                          NULL)                                              ||
     !LoadAndVerifyShader(&m_pProgramSBSStereo, m_vShaderSearchDirs,
                          "Transfer-VS.glsl",
                          NULL,
                          "Compose-SBS-FS.glsl",
                          NULL)                                              ||
     !LoadAndVerifyShader(&m_pProgramAFStereo, m_vShaderSearchDirs,
                          "Transfer-VS.glsl",
                          NULL,
                          "Compose-AF-FS.glsl",
                          NULL)                                              ||
     !LoadAndVerifyShader(&m_pProgramComposeScanlineStereo,
                          m_vShaderSearchDirs, 
                          "Transfer-VS.glsl",
                          NULL,
                          "Compose-Scanline-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramBBox,
                          m_vShaderSearchDirs, 
                          "BBox-VS.glsl",
                          NULL,
                          "BBox-FS.glsl",
                          NULL) ||
     !LoadAndVerifyShader(&m_pProgramMeshFTB,
                          m_vShaderSearchDirs,
                          "Mesh-VS.glsl",
                          NULL,                          
                          "Mesh-FS.glsl","FTB.glsl","lighting.glsl",NULL) ||
    !LoadAndVerifyShader(&m_pProgramMeshBTF,
                          m_vShaderSearchDirs,
                          "Mesh-VS.glsl",
                          NULL,                          
                          "Mesh-FS.glsl","BTF.glsl","lighting.glsl", NULL))
  {
      T_ERROR("Error loading transfer function shaders.");
      return false;
  } else {
    m_pProgramTrans->ConnectTextureID("texColor",0);
    m_pProgramTrans->ConnectTextureID("texDepth",1);

    if (bBindVolume) m_pProgram1DTransSlice->ConnectTextureID("texVolume",0);
    m_pProgram1DTransSlice->ConnectTextureID("texTrans",1);

    if (bBindVolume) m_pProgram2DTransSlice->ConnectTextureID("texVolume",0);
    m_pProgram2DTransSlice->ConnectTextureID("texTrans",1);

    if (bBindVolume) m_pProgram1DTransSlice3D->ConnectTextureID("texVolume",0);
    m_pProgram1DTransSlice3D->ConnectTextureID("texTrans",1);

    if (bBindVolume) m_pProgram2DTransSlice3D->ConnectTextureID("texVolume",0);
    m_pProgram2DTransSlice3D->ConnectTextureID("texTrans",1);

    if (bBindVolume) m_pProgramMIPSlice->ConnectTextureID("texVolume",0);

    m_pProgramTransMIP->ConnectTextureID("texLast",0);
    m_pProgramTransMIP->ConnectTextureID("texTrans",1);

    FLOATVECTOR2 vParams = m_FrustumCullingLOD.GetDepthScaleParams();

    m_pProgramIsoCompose->ConnectTextureID("texRayHitPos",0);
    m_pProgramIsoCompose->ConnectTextureID("texRayHitNormal",1);
    m_pProgramIsoCompose->Set("vProjParam",vParams.x, vParams.y);

    m_pProgramColorCompose->ConnectTextureID("texRayHitPos",0);
    m_pProgramColorCompose->ConnectTextureID("texRayHitNormal",1);
    m_pProgramColorCompose->Set("vProjParam",vParams.x, vParams.y);

    m_pProgramCVCompose->ConnectTextureID("texRayHitPos",0);
    m_pProgramCVCompose->ConnectTextureID("texRayHitNormal",1);
    m_pProgramCVCompose->ConnectTextureID("texRayHitPos2",2);
    m_pProgramCVCompose->ConnectTextureID("texRayHitNormal2",3);
    m_pProgramCVCompose->Set("vProjParam",vParams.x, vParams.y);

    m_pProgramComposeAnaglyphs->ConnectTextureID("texLeftEye",0);
    m_pProgramComposeAnaglyphs->ConnectTextureID("texRightEye",1);

    m_pProgramComposeScanlineStereo->ConnectTextureID("texLeftEye",0);
    m_pProgramComposeScanlineStereo->ConnectTextureID("texRightEye",1);

    m_pProgramSBSStereo->ConnectTextureID("texLeftEye",0);
    m_pProgramSBSStereo->ConnectTextureID("texRightEye",1);    

    m_pProgramAFStereo->ConnectTextureID("texLeftEye",0);
    m_pProgramAFStereo->ConnectTextureID("texRightEye",1);    
  }
  return true;
}

void GLRenderer::CleanupShader(GLSLProgram** p) {
  if (*p) {
    m_pMasterController->MemMan()->FreeGLSLProgram(*p); 
    *p =NULL;
  }
}

void GLRenderer::CleanupShaders() {
  FixedFunctionality();
  CleanupShader(&m_pProgramTrans);
  CleanupShader(&m_pProgram1DTransSlice);
  CleanupShader(&m_pProgram2DTransSlice);
  CleanupShader(&m_pProgram1DTransSlice3D);
  CleanupShader(&m_pProgram2DTransSlice3D);
  CleanupShader(&m_pProgramMIPSlice);
  CleanupShader(&m_pProgramHQMIPRot);
  CleanupShader(&m_pProgramTransMIP);
  CleanupShader(&m_pProgram1DTrans[0]);
  CleanupShader(&m_pProgram1DTrans[1]);
  CleanupShader(&m_pProgram2DTrans[0]);
  CleanupShader(&m_pProgram2DTrans[1]);
  CleanupShader(&m_pProgramIso);
  CleanupShader(&m_pProgramColor);
  CleanupShader(&m_pProgramIsoCompose);
  CleanupShader(&m_pProgramColorCompose);
  CleanupShader(&m_pProgramCVCompose);
  CleanupShader(&m_pProgramComposeAnaglyphs);
  CleanupShader(&m_pProgramComposeScanlineStereo);
  CleanupShader(&m_pProgramSBSStereo);
  CleanupShader(&m_pProgramAFStereo);
  CleanupShader(&m_pProgramBBox);
  CleanupShader(&m_pProgramMeshFTB);
  CleanupShader(&m_pProgramMeshBTF);
}

void GLRenderer::Set1DTrans(const std::vector<unsigned char>& rgba)
{
  AbstrRenderer::Free1DTrans();

  GPUMemMan& mm = *(Controller::Instance().MemMan());
  std::pair<TransferFunction1D*, GLTexture1D*> tf;
  tf = mm.SetExternal1DTrans(rgba, this);

  m_p1DTrans = tf.first;
  m_p1DTransTex = tf.second;
}

void GLRenderer::Changed1DTrans() {
  assert(m_p1DTransTex->GetSize() == m_p1DTrans->GetSize());

  m_p1DTrans->GetByteArray(m_p1DData);
  m_p1DTransTex->SetData(&m_p1DData.at(0));

  AbstrRenderer::Changed1DTrans();
}

void GLRenderer::Changed2DTrans() {
  m_p2DTrans->GetByteArray(&m_p2DData);
  m_p2DTransTex->SetData(m_p2DData);

  AbstrRenderer::Changed2DTrans();
}

void GLRenderer::Resize(const UINTVECTOR2& vWinSize) {
  AbstrRenderer::Resize(vWinSize);
  MESSAGE("Resizing to %u x %u", vWinSize.x, vWinSize.y);

  glViewport(0, 0, m_vWinSize.x, m_vWinSize.y);
  ClearColorBuffer();
}

void GLRenderer::ClearColorBuffer() const {
  m_pContext->GetStateManager()->Apply(m_BaseState);

  if (m_bDoStereoRendering && m_eStereoMode == SM_RB) {
    // render anaglyphs against a black background only
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);
  } else {
    // if top and bottom colors are the same simply clear ...
    if (m_vBackgroundColors[0] == m_vBackgroundColors[1]) {
      glClearColor(m_vBackgroundColors[0].x,
                   m_vBackgroundColors[0].y,
                   m_vBackgroundColors[0].z, 0);
      glClear(GL_COLOR_BUFFER_BIT);
    } else {
      // ... draw a gradient image otherwise
      DrawBackGradient();
    }
  }
  // finally blit the logo onto the screen (if present)
  DrawLogo();
}

void GLRenderer::StartFrame() {
  // clear the depthbuffer (if requested)
  if (m_bClearFramebuffer) {
    glClear(GL_DEPTH_BUFFER_BIT);
    if (m_bConsiderPreviousDepthbuffer) SaveEmptyDepthBuffer();
  } else {
    if (m_bConsiderPreviousDepthbuffer) SaveDepthBuffer();
  }

  if (m_eRenderMode == RM_ISOSURFACE) {
    FLOATVECTOR2 vfWinSize = FLOATVECTOR2(m_vWinSize);
    if (m_bDoClearView) {
      m_pProgramCVCompose->Enable();
      m_pProgramCVCompose->Set("vScreensize",vfWinSize.x, vfWinSize.y);
    } else {
      GLSLProgram* shader = (m_pDataset->GetComponentCount() == 1) ?
                            m_pProgramIsoCompose : m_pProgramColorCompose;

      shader->Enable();
      shader->Set("vScreensize",vfWinSize.x, vfWinSize.y);
    }
  }
}

void GLRenderer::RecomposeView(const RenderRegion& rgn)
{
  MESSAGE("Recomposing region {(%u,%u), (%u,%u)}",
          static_cast<unsigned>(rgn.minCoord[0]),
          static_cast<unsigned>(rgn.minCoord[1]),
          static_cast<unsigned>(rgn.maxCoord[0]),
          static_cast<unsigned>(rgn.maxCoord[1]));
  if(rgn.is3D()) {
    Recompose3DView(dynamic_cast<const RenderRegion3D&>(rgn));
  }
}

bool GLRenderer::Paint() {
  if (!AbstrRenderer::Paint()) return false;

  m_pContext->GetStateManager()->Apply(m_BaseState);

  if (m_bDatasetIsInvalid) return true;

  // we want vector<bool>, but of course that's a bad idea.
  vector<char> justCompletedRegions(renderRegions.size(), false);

  // if we are drawing for the first time after a resize we do not want to
  // start a full redraw loop, rather we just blit the last valid image
  // onto the screen.  This makes resizing more responsive.  We'll schedule a
  // complete redraw after, no worries.
  if (m_bFirstDrawAfterResize) {
    CreateOffscreenBuffers();
    CreateDepthStorage();
  }

  if (m_bFirstDrawAfterResize || m_bFirstDrawAfterModeChange ) {
    StartFrame();
  }

  if (m_bFirstDrawAfterResize && m_eRendererTarget != RT_HEADLESS) {
    if (m_pFBOResizeQuickBlit) {
      m_pFBO3DImageLast->Write();
      glViewport(0,0,m_vWinSize.x,m_vWinSize.y);
  
      m_pContext->GetStateManager()->SetEnableBlend(false);

      m_pFBOResizeQuickBlit->Read(0);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      m_pFBOResizeQuickBlit->ReadDepth(1);

      glClearColor(1,0,0,1);
      glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

      m_pContext->GetStateManager()->SetEnableDepthTest(false);
      glClearColor(0,0,0,0);

      m_pProgramTrans->Enable();
      FullscreenQuad();

      m_pFBOResizeQuickBlit->FinishRead();
      m_pFBOResizeQuickBlit->FinishDepthRead();
      m_pFBO3DImageLast->FinishWrite();

      m_pMasterController->MemMan()->FreeFBO(m_pFBOResizeQuickBlit);
      m_pFBOResizeQuickBlit = NULL;
    }
  } else {
    for (size_t i=0; i < renderRegions.size(); ++i) {
      if (renderRegions[i]->redrawMask) {
        SetRenderTargetArea(*renderRegions[i], this->decreaseScreenResNow);
        if (renderRegions[i]->is3D()) {
          RenderRegion3D &region3D = *static_cast<RenderRegion3D*>
                                                 (renderRegions[i]);
          if (!region3D.isBlank && m_bPerformReCompose){
            Recompose3DView(region3D);
            justCompletedRegions[i] = true;
          } else {
            PlanFrame(region3D);

            // decreaseScreenResNow could have changed after calling PlanFrame.
            SetRenderTargetArea(region3D, this->decreaseScreenResNow);

            // execute the frame
            float fMsecPassed = 0.0f;
            bool bJobDone = false;
            if (!Execute3DFrame(region3D, fMsecPassed, bJobDone) ) {
              T_ERROR("Could not execute the 3D frame, aborting.");
              return false;
            }
            justCompletedRegions[i] = static_cast<char>(bJobDone);
            this->msecPassedCurrentFrame += fMsecPassed;
          }
          // are we done rendering or do we need to render at higher quality?
          region3D.redrawMask =
            (m_vCurrentBrickList.size() > m_iBricksRenderedInThisSubFrame) ||
            (m_iCurrentLODOffset > m_iMinLODForCurrentView) ||
            this->decreaseScreenResNow;
        } else if (renderRegions[i]->is2D()) {  // in a 2D view mode
          RenderRegion2D& region2D = *static_cast<RenderRegion2D*>(renderRegions[i]);
          justCompletedRegions[i] = Render2DView(region2D);
          region2D.redrawMask = false;
          if(this->decreaseScreenResNow) {
            // if we just rendered at reduced res, we've got to do another
            // render later.
            region2D.redrawMask = true;
          }
        }
      } else {
        justCompletedRegions[i] = false;
      }
      renderRegions[i]->isBlank = false;
    }
  }
  EndFrame(justCompletedRegions);

  // reset render states
  m_bFirstDrawAfterResize = false;
  m_bFirstDrawAfterModeChange = false;
  return true;
}

void GLRenderer::FullscreenQuad() const {
  glBegin(GL_QUADS);
    glTexCoord2d(0, 0);
    glVertex3d(-1.0, -1.0, -0.5);
    glTexCoord2d(1, 0);
    glVertex3d( 1.0, -1.0, -0.5);
    glTexCoord2d(1, 1);
    glVertex3d( 1.0,  1.0, -0.5);
    glTexCoord2d(0, 1);
    glVertex3d(-1.0,  1.0, -0.5);
  glEnd();
}

void GLRenderer::FullscreenQuadRegions() const {
  for (size_t i=0; i < renderRegions.size(); ++i) {
    FullscreenQuadRegion(renderRegions[i], this->decreaseScreenResNow);
  }
}

void GLRenderer::FullscreenQuadRegion(const RenderRegion* region,
                                      bool decreaseScreenRes) const {
  const float rescale = decreaseScreenRes ? 1.0f / m_fScreenResDecFactor : 1.0f;

  FLOATVECTOR2 minCoord(region->minCoord);
  FLOATVECTOR2 maxCoord(region->maxCoord);

  //normalize to 0,1.
  FLOATVECTOR2 minCoordNormalized = minCoord / FLOATVECTOR2(m_vWinSize);
  FLOATVECTOR2 maxCoordNormalized = maxCoord / FLOATVECTOR2(m_vWinSize);

  FLOATVECTOR2 minTexCoord = minCoordNormalized;
  FLOATVECTOR2 maxTexCoord = minCoordNormalized +
    (maxCoordNormalized-minCoordNormalized)*rescale;

  glBegin(GL_QUADS);
    glTexCoord2d(minTexCoord[0], minTexCoord[1]);
    glVertex3d(minCoordNormalized[0]*2-1, minCoordNormalized[1]*2-1, -0.5);
    glTexCoord2d(maxTexCoord[0], minTexCoord[1]);
    glVertex3d(maxCoordNormalized[0]*2-1, minCoordNormalized[1]*2-1, -0.5);
    glTexCoord2d(maxTexCoord[0], maxTexCoord[1]);
    glVertex3d(maxCoordNormalized[0]*2-1, maxCoordNormalized[1]*2-1, -0.5);
    glTexCoord2d(minTexCoord[0], maxTexCoord[1]);
    glVertex3d(minCoordNormalized[0]*2-1, maxCoordNormalized[1]*2-1, -0.5);
  glEnd();
}

/// copy the newly completed image into the buffer that stores completed
/// images.
void GLRenderer::CopyOverCompletedRegion(const RenderRegion* region) {
  // write to FBO that contains final images.
  m_TargetBinder.Bind(m_pFBO3DImageLast);

  GPUState localState = m_BaseState;
  localState.enableBlend = false;
  localState.depthFunc = DF_LEQUAL;
  localState.enableScissor = true;
  m_pContext->GetStateManager()->Apply(localState);

  glViewport(0, 0, m_vWinSize.x, m_vWinSize.y);

  SetRenderTargetAreaScissor(*region);

  // always clear the depth buffer since we are transporting new data from the FBO
  glClear(GL_DEPTH_BUFFER_BIT);

  // Read newly completed image
  m_pFBO3DImageCurrent[0]->Read(0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                  (this->decreaseScreenResNow) ? GL_LINEAR : GL_NEAREST);
  m_pFBO3DImageCurrent[0]->ReadDepth(1);

  // Display this to the old buffer so we can reuse it in future frame.
  m_pProgramTrans->Enable();
  FullscreenQuadRegion(region, this->decreaseScreenResNow);

  m_TargetBinder.Unbind();
  m_pFBO3DImageCurrent[0]->FinishRead();
  m_pFBO3DImageCurrent[0]->FinishDepthRead();
}

void GLRenderer::TargetIsBlankButFrameIsNotFinished(const RenderRegion* region) {
  // In stereo, we just clear; otherwise we'll see a rendering for just one of
  // the eyes.
  if(m_bDoStereoRendering) {
    ClearColorBuffer();
  } else {
    CopyOverCompletedRegion(region);
  }
}

void GLRenderer::EndFrame(const vector<char>& justCompletedRegions) {
  // For a single region we can support stereo and we can also optimize the
  // code by swapping the buffers instead of copying data from one to the
  // other.
  if (renderRegions.size() == 1) {
    // if the image is complete
    if (justCompletedRegions[0]) {
      m_bOffscreenIsLowRes = this->decreaseScreenResNow;

      // in stereo compose both images into one, in mono mode simply swap the
      // pointers
      if (m_bDoStereoRendering) {
         m_pContext->GetStateManager()->Apply(m_BaseState);

        if (m_bStereoEyeSwap) {
          m_pFBO3DImageCurrent[0]->Read(1);
          m_pFBO3DImageCurrent[1]->Read(0);
        } else {
          m_pFBO3DImageCurrent[0]->Read(0);
          m_pFBO3DImageCurrent[1]->Read(1);
        }

        m_TargetBinder.Bind(m_pFBO3DImageLast);
        glClear(GL_COLOR_BUFFER_BIT);

        switch (m_eStereoMode) {
          case SM_RB : m_pProgramComposeAnaglyphs->Enable(); 
					   break;
          case SM_SCANLINE: {
                        m_pProgramComposeScanlineStereo->Enable(); 
                        FLOATVECTOR2 vfWinSize = FLOATVECTOR2(m_vWinSize);
                        m_pProgramComposeScanlineStereo->Set("vScreensize",vfWinSize.x, vfWinSize.y);
                        break;
                       }
          case SM_SBS: {
					          m_pProgramSBSStereo->Enable(); 
                    float fSplitCoord = (m_bOffscreenIsLowRes) ? 0.5f / m_fScreenResDecFactor : 0.5f;
                    m_pProgramSBSStereo->Set("fSplitCoord",fSplitCoord);
                    break;
                       }
          default : // SM_AF
					m_pProgramAFStereo->Enable(); 
					m_pProgramAFStereo->Set("iAlternatingFrameID",m_iAlternatingFrameID);
                    break;
		    }

        m_pContext->GetStateManager()->SetEnableDepthTest(false);
        FullscreenQuadRegions();

        m_TargetBinder.Unbind();

        m_pFBO3DImageCurrent[0]->FinishRead();
        m_pFBO3DImageCurrent[1]->FinishRead();
      } else {
        swap(m_pFBO3DImageLast, m_pFBO3DImageCurrent[0]);
      }

      for (size_t i=0; i < renderRegions.size(); ++i) {
        if(!OnlyRecomposite(renderRegions[i])) {
          CompletedASubframe(renderRegions[i]);
        }
      }

    } else {
      if (!renderRegions[0]->isBlank && renderRegions[0]->isTargetBlank) {
        TargetIsBlankButFrameIsNotFinished(renderRegions[0]);
      }
    }
  } else {
    for (size_t i=0; i < renderRegions.size(); ++i) {
      if(justCompletedRegions[i]) {
        if (!OnlyRecomposite(renderRegions[i])) {
          CompletedASubframe(renderRegions[i]);
        }
        CopyOverCompletedRegion(renderRegions[i]);
      } else {
        if (!renderRegions[i]->isBlank && renderRegions[i]->isTargetBlank) {
          TargetIsBlankButFrameIsNotFinished(renderRegions[i]);
        }
      }
    }
  }

  CopyImageToDisplayBuffer();

  // we've definitely recomposed by now.
  m_bPerformReCompose = false; 
}

void GLRenderer::SetRenderTargetArea(const RenderRegion& renderRegion,
                                     bool bDecreaseScreenResNow) {
  SetRenderTargetArea(renderRegion.minCoord, renderRegion.maxCoord,
                      bDecreaseScreenResNow);
}

void GLRenderer::SetRenderTargetArea(UINTVECTOR2 minCoord, UINTVECTOR2 maxCoord,
                                     bool bDecreaseScreenResNow) {
  SetViewPort(minCoord, maxCoord, bDecreaseScreenResNow);
}

void GLRenderer::SetRenderTargetAreaScissor(const RenderRegion& renderRegion) {
  UINTVECTOR2 regionSize = renderRegion.maxCoord - renderRegion.minCoord;
  glScissor(renderRegion.minCoord.x, renderRegion.minCoord.y,
            regionSize.x, regionSize.y);
}

void GLRenderer::SetViewPort(UINTVECTOR2 viLowerLeft, UINTVECTOR2 viUpperRight,
                             bool bDecreaseScreenResNow) {
  UINTVECTOR2 viSize = viUpperRight-viLowerLeft;
  const UINT originalPixelsY = viSize.y;
  if (bDecreaseScreenResNow) {
    const float rescale = 1.0f/m_fScreenResDecFactor;

    //Note, we round to the nearest int (the + 0.5 part). This in
    //effect expands the render region in all directions to the nearest int and
    //so hides any possible gaps that could result.
    viSize = UINTVECTOR2((FLOATVECTOR2(viSize) * rescale) +
                         FLOATVECTOR2(0.5, 0.5));
  }

  // viewport
  glViewport(viLowerLeft.x,viLowerLeft.y,viSize.x,viSize.y);

  float fAspect =(float)viSize.x/(float)viSize.y;
  ComputeViewAndProjection(fAspect);

  // forward the projection matrix to the culling object
  m_FrustumCullingLOD.SetProjectionMatrix(m_mProjection[0]);
  m_FrustumCullingLOD.SetScreenParams(m_fFOV, fAspect, m_fZNear, m_fZFar,
                                      originalPixelsY);

}

void GLRenderer::ComputeViewAndProjection(float fAspect) {
  if (m_bUserMatrices)  {
    if (m_bDoStereoRendering) {
      m_mView[0] = m_UserViewLeft;
      m_mProjection[0] = m_UserProjectionLeft;
      m_mView[1] = m_UserViewRight;
      m_mProjection[1] = m_UserProjectionRight;
    } else {
      m_mView[0] = m_UserView;
      m_mProjection[0] = m_UserProjection;
      m_mProjection[0].setProjection();
    }
  } else {
    if (m_bDoStereoRendering) {
      FLOATMATRIX4::BuildStereoLookAtAndProjection(m_vEye, m_vAt, m_vUp, m_fFOV,
                                                   fAspect, m_fZNear, m_fZFar,
                                                   m_fStereoFocalLength,
                                                   m_fStereoEyeDist, m_mView[0],
                                                   m_mView[1], m_mProjection[0],
                                                   m_mProjection[1]);
    } else {
      // view matrix
      m_mView[0].BuildLookAt(m_vEye, m_vAt, m_vUp);

      // projection matrix
      m_mProjection[0].Perspective(m_fFOV, fAspect, m_fZNear, m_fZFar);
      m_mProjection[0].setProjection();
    }
  }
}

void GLRenderer::RenderSlice(const RenderRegion2D& region, double fSliceIndex,
                             FLOATVECTOR3 vMinCoords, FLOATVECTOR3 vMaxCoords,
                             DOUBLEVECTOR3 vAspectRatio,
                             DOUBLEVECTOR2 vWinAspectRatio) {
  
  switch (region.windowMode) {
    case RenderRegion::WM_AXIAL :
      {
        if (region.flipView.x) {
          float fTemp = vMinCoords.x;
          vMinCoords.x = vMaxCoords.x;
          vMaxCoords.x = fTemp;
        }

        if (region.flipView.y) {
          float fTemp = vMinCoords.z;
          vMinCoords.z = vMaxCoords.z;
          vMaxCoords.z = fTemp;
        }

        DOUBLEVECTOR2 v2AspectRatio = vAspectRatio.xz()*DOUBLEVECTOR2(vWinAspectRatio);
        v2AspectRatio = v2AspectRatio / v2AspectRatio.maxVal();
        glBegin(GL_QUADS);
          glTexCoord3d(vMinCoords.x,fSliceIndex,vMaxCoords.z);
          glVertex3d(-1.0f*v2AspectRatio.x, +1.0f*v2AspectRatio.y, -0.5f);
          glTexCoord3d(vMaxCoords.x,fSliceIndex,vMaxCoords.z);
          glVertex3d(+1.0f*v2AspectRatio.x, +1.0f*v2AspectRatio.y, -0.5f);
          glTexCoord3d(vMaxCoords.x,fSliceIndex,vMinCoords.z);
          glVertex3d(+1.0f*v2AspectRatio.x, -1.0f*v2AspectRatio.y, -0.5f);
          glTexCoord3d(vMinCoords.x,fSliceIndex,vMinCoords.z);
          glVertex3d(-1.0f*v2AspectRatio.x, -1.0f*v2AspectRatio.y, -0.5f);
        glEnd();
        break;
      }
    case RenderRegion::WM_CORONAL :
      {
        if (region.flipView.x) {
          float fTemp = vMinCoords.x;
          vMinCoords.x = vMaxCoords.x;
          vMaxCoords.x = fTemp;
        }

        if (region.flipView.y) {
          float fTemp = vMinCoords.y;
          vMinCoords.y = vMaxCoords.y;
          vMaxCoords.y = fTemp;
        }

        DOUBLEVECTOR2 v2AspectRatio = vAspectRatio.xy()*DOUBLEVECTOR2(vWinAspectRatio);
        v2AspectRatio = v2AspectRatio / v2AspectRatio.maxVal();
        glBegin(GL_QUADS);
          glTexCoord3d(vMinCoords.x,vMaxCoords.y,fSliceIndex);
          glVertex3d(-1.0f*v2AspectRatio.x, +1.0f*v2AspectRatio.y, -0.5f);
          glTexCoord3d(vMaxCoords.x,vMaxCoords.y,fSliceIndex);
          glVertex3d(+1.0f*v2AspectRatio.x, +1.0f*v2AspectRatio.y, -0.5f);
          glTexCoord3d(vMaxCoords.x,vMinCoords.y,fSliceIndex);
          glVertex3d(+1.0f*v2AspectRatio.x, -1.0f*v2AspectRatio.y, -0.5f);
          glTexCoord3d(vMinCoords.x,vMinCoords.y,fSliceIndex);
          glVertex3d(-1.0f*v2AspectRatio.x, -1.0f*v2AspectRatio.y, -0.5f);
        glEnd();
        break;
      }
    case RenderRegion::WM_SAGITTAL :
      {
        if (region.flipView.x) {
          float fTemp = vMinCoords.y;
          vMinCoords.y = vMaxCoords.y;
          vMaxCoords.y = fTemp;
        }

        if (region.flipView.y) {
          float fTemp = vMinCoords.z;
          vMinCoords.z = vMaxCoords.z;
          vMaxCoords.z = fTemp;
        }

        DOUBLEVECTOR2 v2AspectRatio = vAspectRatio.yz()*DOUBLEVECTOR2(vWinAspectRatio);
        v2AspectRatio = v2AspectRatio / v2AspectRatio.maxVal();
        glBegin(GL_QUADS);
          glTexCoord3d(fSliceIndex,vMinCoords.y,vMaxCoords.z);
          glVertex3d(-1.0f*v2AspectRatio.x, +1.0f*v2AspectRatio.y, -0.5f);
          glTexCoord3d(fSliceIndex,vMaxCoords.y,vMaxCoords.z);
          glVertex3d(+1.0f*v2AspectRatio.x, +1.0f*v2AspectRatio.y, -0.5f);
          glTexCoord3d(fSliceIndex,vMaxCoords.y,vMinCoords.z);
          glVertex3d(+1.0f*v2AspectRatio.x, -1.0f*v2AspectRatio.y, -0.5f);
          glTexCoord3d(fSliceIndex,vMinCoords.y,vMinCoords.z);
          glVertex3d(-1.0f*v2AspectRatio.x, -1.0f*v2AspectRatio.y, -0.5f);
        glEnd();
        break;
      }
    default :  T_ERROR("Invalid windowmode set"); break;
  }
}

bool GLRenderer::BindVolumeTex(const BrickKey& bkey,
                               const UINT64 iIntraFrameCounter) {
  GL_CHECK();

  // get the 3D texture from the memory manager
  m_pGLVolume = m_pMasterController->MemMan()->GetVolume(m_pDataset, bkey,
                                                         m_bUseOnlyPowerOfTwo,
                                                         m_bDownSampleTo8Bits,
                                                         m_bDisableBorder,
                                                         false,
                                                         iIntraFrameCounter,
                                                         m_iFrameCounter);
  GL_CHECK();
  if(m_pGLVolume) {
    m_pGLVolume->SetFilter(ComputeGLFilter(), ComputeGLFilter());
    static_cast<GLVolume3DTex*>(m_pGLVolume)->Bind(0);

    return true;
  } else {
    return false;
  }
}

bool GLRenderer::UnbindVolumeTex() {
  if(m_pGLVolume) {
    m_pMasterController->MemMan()->Release3DTexture(m_pGLVolume);
    m_pGLVolume = NULL;
    return true;
  } else {
    return false;
  }
}

bool GLRenderer::Render2DView(RenderRegion2D& renderRegion) {
  
  // bind offscreen buffer
  if (renderRegion.GetUseMIP()) {
    // for MIP rendering "abuse" left-eye buffer for the itermediate results
    m_TargetBinder.Bind(m_pFBO3DImageCurrent[1]);
  } else {
    m_TargetBinder.Bind(m_pFBO3DImageCurrent[0]);
  }

  SetDataDepShaderVars();

  // if we render a slice view or MIP preview
  if (!renderRegion.GetUseMIP() || m_eRendererTarget != RT_CAPTURE)  {
    GPUState localState = m_BaseState;
    if (!renderRegion.GetUseMIP()) {
      switch (m_eRenderMode) {
        case RM_2DTRANS    :  m_p2DTransTex->Bind(1);
                              m_pProgram2DTransSlice->Enable();
                              break;
        default            :  m_p1DTransTex->Bind(1);
                              m_pProgram1DTransSlice->Enable();
                              break;
      }
      localState.enableBlend = false;
    } else {
      m_pProgramMIPSlice->Enable();

      localState.blendEquation = BE_MAX;
      localState.blendFuncSrc = BF_ONE;
      localState.blendFuncDst = BF_ONE;
    }
    localState.enableDepthTest = false;
    m_pContext->GetStateManager()->Apply(localState);

    size_t iCurrentLOD = 0;
    UINTVECTOR3 vVoxelCount(1,1,1); // make sure we do not divide by zero later
                                    // if no single-brick LOD exists

    // For now to make things simpler for the slice renderer we use the LOD
    // level with just one brick
    for (size_t i = 0;i<size_t(m_pDataset->GetLODLevelCount());i++) {
      if (m_pDataset->GetBrickCount(i, m_iTimestep) == 1) {
          iCurrentLOD = i;
          vVoxelCount = UINTVECTOR3(m_pDataset->GetDomainSize(i));
          break;
      }
    }

    if (!renderRegion.GetUseMIP()) SetBrickDepShaderVarsSlice(vVoxelCount);

    // Get the brick at this LOD; note we're guaranteed this brick will cover
    // the entire domain, because the search above gives us the coarsest LOD!
    const BrickKey bkey(m_iTimestep, iCurrentLOD, 0);

    if (!BindVolumeTex(bkey,0)) {
      T_ERROR("Unable to bind volume to texture (LOD:%u, Brick:0)",
              static_cast<unsigned>(iCurrentLOD));
      return false;
    }

    // clear the target at the beginning
    m_pContext->GetStateManager()->SetEnableScissor(true);
    SetRenderTargetAreaScissor(renderRegion);

    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT);

    m_pContext->GetStateManager()->SetEnableScissor(false);

    // 'VoxelCount' is the number of voxels in the brick which contain data.
    // 'RealVoxelCount' will be the actual number of voxels in the brick, which
    // could be larger than the voxel count if we need to use PoT textures.
    UINTVECTOR3 vRealVoxelCount;
    if (m_bUseOnlyPowerOfTwo) {
      vRealVoxelCount = UINTVECTOR3(MathTools::NextPow2(vVoxelCount.x),
                                    MathTools::NextPow2(vVoxelCount.y),
                                    MathTools::NextPow2(vVoxelCount.z));
    } else {
     vRealVoxelCount = vVoxelCount;
    }
    FLOATVECTOR3 vMinCoords = FLOATVECTOR3(0.5f/FLOATVECTOR3(vRealVoxelCount));
    FLOATVECTOR3 vMaxCoords = (FLOATVECTOR3(vVoxelCount) / FLOATVECTOR3(vRealVoxelCount)) - vMinCoords;

    UINT64VECTOR3 vDomainSize = m_pDataset->GetDomainSize();
    DOUBLEVECTOR3 vAspectRatio = m_pDataset->GetScale() * DOUBLEVECTOR3(vDomainSize);

    DOUBLEVECTOR2 renderRegionSize(renderRegion.maxCoord - renderRegion.minCoord);
    DOUBLEVECTOR2 vWinAspectRatio = 1.0 / renderRegionSize;
    vWinAspectRatio = vWinAspectRatio / vWinAspectRatio.maxVal();

    const int sliceDir = static_cast<size_t>(renderRegion.windowMode);

    if (renderRegion.GetUseMIP()) {
      // Iterate; render all slices, and we'll figure out the 'M'(aximum) in
      // the shader.  Note that we iterate over all slices which have data
      // ("VoxelCount"), not over all slices ("RealVoxelCount").
      for (UINT64 i = 0;i<vVoxelCount[sliceDir];i++) {
        // First normalize to a [0..1] space
        double fSliceIndex = static_cast<double>(i) / vVoxelCount[sliceDir];
        // Now correct for PoT textures: a [0..1] space gives us the location
        // of the slice in a perfect world, but if we're using PoT textures we
        // might only access say [0..0.75] e.g. if we needed to increase the
        // 3Dtexture size by 25% to make it PoT.
        fSliceIndex *= static_cast<double> (vVoxelCount[sliceDir]) /
                       static_cast<double> (vRealVoxelCount[sliceDir]);
        RenderSlice(renderRegion, fSliceIndex, vMinCoords, vMaxCoords,
                    vAspectRatio, vWinAspectRatio);
      }
    } else {
      // same indexing fix as above.
      double fSliceIndex = static_cast<double>(renderRegion.GetSliceIndex()) /
                           vDomainSize[sliceDir];
      fSliceIndex *= static_cast<double> (vVoxelCount[sliceDir]) /
                     static_cast<double> (vRealVoxelCount[sliceDir]);
      RenderSlice(renderRegion, fSliceIndex, vMinCoords, vMaxCoords,
                  vAspectRatio, vWinAspectRatio);
    }

    if (!UnbindVolumeTex()) {
      T_ERROR("Cannot unbind volume: No volume bound");
      return false;
    }
  } else {
    if (m_bOrthoView) {
      FLOATMATRIX4 maOrtho;
      UINT64VECTOR3 vDomainSize = m_pDataset->GetDomainSize();
      DOUBLEVECTOR2 vWinAspectRatio = 1.0 / DOUBLEVECTOR2(m_vWinSize);
      vWinAspectRatio = vWinAspectRatio / vWinAspectRatio.maxVal();
      float fRoot2Scale = (vWinAspectRatio.x < vWinAspectRatio.y) ?
                          max(1.0f, 1.414213f * float(vWinAspectRatio.x/vWinAspectRatio.y)) :
                          1.414213f;

      maOrtho.Ortho(-0.5f*fRoot2Scale/float(vWinAspectRatio.x), 
                    +0.5f*fRoot2Scale/float(vWinAspectRatio.x),
                    -0.5f*fRoot2Scale/float(vWinAspectRatio.y), 
                    +0.5f*fRoot2Scale/float(vWinAspectRatio.y),
                    -100.0f, 100.0f);
      maOrtho.setProjection();
    }

    PlanHQMIPFrame(renderRegion);
    glClearColor(0,0,0,0);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    RenderHQMIPPreLoop(renderRegion);

    for (size_t iBrickIndex = 0;iBrickIndex<m_vCurrentBrickList.size();iBrickIndex++) {
      MESSAGE("Brick %u of %u in full resolution MIP mode", static_cast<unsigned>(iBrickIndex+1),
              static_cast<unsigned>(m_vCurrentBrickList.size()));

      // for MIP we do not consider empty bricks since we do not render 
      // other geometry such as meshes anyway
      if (m_vCurrentBrickList[iBrickIndex].bIsEmpty) continue;

      // convert 3D variables to the more general ND scheme used in the memory
      // manager, i.e. convert 3-vectors to stl vectors
      vector<UINT64> vLOD; vLOD.push_back(m_iCurrentLOD);
      vector<UINT64> vBrick;
      vBrick.push_back(m_vCurrentBrickList[iBrickIndex].vCoords.x);
      vBrick.push_back(m_vCurrentBrickList[iBrickIndex].vCoords.y);
      vBrick.push_back(m_vCurrentBrickList[iBrickIndex].vCoords.z);
      const BrickKey &key = m_vCurrentBrickList[iBrickIndex].kBrick;

      // get the 3D texture from the memory manager

      if (!BindVolumeTex(key,0)) {
        T_ERROR("Unable to bind volume to texture (LOD:%u, Brick:%u)",
                static_cast<unsigned>(m_iCurrentLOD),
                static_cast<unsigned>(iBrickIndex));
        return false;
      }
      RenderHQMIPInLoop(renderRegion, m_vCurrentBrickList[iBrickIndex]);
      if (!UnbindVolumeTex()) {
        T_ERROR("Cannot unbind volume: No volume bound");
        return false;
      }
    }
    RenderHQMIPPostLoop();
  }

  // apply 1D transferfunction to MIP image
  if (renderRegion.GetUseMIP()) {

    GPUState localState = m_BaseState;
    localState.enableBlend = false;
    localState.enableDepthTest = false;
    m_pContext->GetStateManager()->Apply(localState);

    m_TargetBinder.Bind(m_pFBO3DImageCurrent[0]);

    SetRenderTargetArea(UINTVECTOR2(0,0), m_vWinSize, false);
    m_pContext->GetStateManager()->SetEnableScissor(true);
    SetRenderTargetAreaScissor(renderRegion);
    m_pFBO3DImageCurrent[1]->Read(0);
    m_p1DTransTex->Bind(1);
    m_pProgramTransMIP->Enable();
    FullscreenQuad();
    m_pFBO3DImageCurrent[1]->FinishRead(0);
  }

  m_TargetBinder.Unbind();

  return true;
}

void GLRenderer::RenderHQMIPPreLoop(RenderRegion2D& region) {
  double dPI = 3.141592653589793238462643383;
  FLOATMATRIX4 matRotDir, matFlipX, matFlipY;
  switch (region.windowMode) {
    case RenderRegion::WM_SAGITTAL : {
      FLOATMATRIX4 matTemp;
      matRotDir.RotationX(-dPI/2.0);
      matTemp.RotationY(-dPI/2.0);
      matRotDir = matRotDir * matTemp;
      break;
    }
    case RenderRegion::WM_AXIAL :
      matRotDir.RotationX(-dPI/2.0);
      break;
    case RenderRegion::WM_CORONAL :
      break;
    default : T_ERROR("Invalid windowmode set"); break;
  }
  if (region.flipView.x) {
    matFlipY.Scaling(-1,1,1);
  }
  if (region.flipView.y) {
    matFlipX.Scaling(1,-1,1);
  }
  m_maMIPRotation.RotationY(dPI*double(m_fMIPRotationAngle)/180.0);
  m_maMIPRotation = matRotDir * region.rotation * matFlipX * matFlipY * m_maMIPRotation;
}

void GLRenderer::RenderBBox(const FLOATVECTOR4 vColor) {
  FLOATVECTOR3 vCenter, vExtend;
  GetVolumeAABB(vCenter,vExtend);
  RenderBBox(vColor, vCenter, vExtend);
}

void GLRenderer::RenderBBox(const FLOATVECTOR4 vColor,
                            const FLOATVECTOR3& vCenter, const FLOATVECTOR3& vExtend) {
  FLOATVECTOR3 vMinPoint, vMaxPoint;

  vMinPoint = (vCenter - vExtend/2.0);
  vMaxPoint = (vCenter + vExtend/2.0);

  m_pProgramBBox->Enable();

  glBegin(GL_LINES);
    glColor4f(vColor.x,vColor.y,vColor.z,vColor.w);
    // FRONT
    glVertex3f( vMaxPoint.x,vMinPoint.y,vMinPoint.z);
    glVertex3f(vMinPoint.x,vMinPoint.y,vMinPoint.z);
    glVertex3f( vMaxPoint.x, vMaxPoint.y,vMinPoint.z);
    glVertex3f(vMinPoint.x, vMaxPoint.y,vMinPoint.z);
    glVertex3f(vMinPoint.x,vMinPoint.y,vMinPoint.z);
    glVertex3f(vMinPoint.x, vMaxPoint.y,vMinPoint.z);
    glVertex3f( vMaxPoint.x,vMinPoint.y,vMinPoint.z);
    glVertex3f( vMaxPoint.x, vMaxPoint.y,vMinPoint.z);

    // BACK
    glVertex3f( vMaxPoint.x,vMinPoint.y, vMaxPoint.z);
    glVertex3f(vMinPoint.x,vMinPoint.y, vMaxPoint.z);
    glVertex3f( vMaxPoint.x, vMaxPoint.y, vMaxPoint.z);
    glVertex3f(vMinPoint.x, vMaxPoint.y, vMaxPoint.z);
    glVertex3f(vMinPoint.x,vMinPoint.y, vMaxPoint.z);
    glVertex3f(vMinPoint.x, vMaxPoint.y, vMaxPoint.z);
    glVertex3f( vMaxPoint.x,vMinPoint.y, vMaxPoint.z);
    glVertex3f( vMaxPoint.x, vMaxPoint.y, vMaxPoint.z);

    // CONNECTION
    glVertex3f(vMinPoint.x,vMinPoint.y, vMaxPoint.z);
    glVertex3f(vMinPoint.x,vMinPoint.y,vMinPoint.z);
    glVertex3f(vMinPoint.x, vMaxPoint.y, vMaxPoint.z);
    glVertex3f(vMinPoint.x, vMaxPoint.y,vMinPoint.z);
    glVertex3f( vMaxPoint.x,vMinPoint.y, vMaxPoint.z);
    glVertex3f( vMaxPoint.x,vMinPoint.y,vMinPoint.z);
    glVertex3f( vMaxPoint.x, vMaxPoint.y, vMaxPoint.z);
    glVertex3f( vMaxPoint.x, vMaxPoint.y,vMinPoint.z);
  glEnd();
}

void GLRenderer::NewFrameClear(const RenderRegion& renderRegion) {
  m_pContext->GetStateManager()->SetEnableScissor(true);
  SetRenderTargetAreaScissor(renderRegion);

  GL(glClearColor(0,0,0,0));

  size_t iStereoBufferCount = (m_bDoStereoRendering) ? 2 : 1;
  for (size_t i = 0;i<iStereoBufferCount;i++) {
    m_TargetBinder.Bind(m_pFBO3DImageCurrent[i]);

    if (m_bConsiderPreviousDepthbuffer && m_aDepthStorage) {
      GL(glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT));
      GL(glMatrixMode(GL_PROJECTION));
      GL(glLoadIdentity());
      GL(glMatrixMode(GL_MODELVIEW));
      GL(glLoadIdentity());
      GL(glRasterPos2f(-1.0,-1.0));
      m_pContext->GetStateManager()->SetColorMask(false);
      GL(glDrawPixels(m_vWinSize.x, m_vWinSize.y, GL_DEPTH_COMPONENT, GL_FLOAT,
                      m_aDepthStorage));
      m_pContext->GetStateManager()->SetColorMask(true);
    } else {
      GL(glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT));
    }
  }

  m_TargetBinder.Unbind();
}

void GLRenderer::RenderCoordArrows(const RenderRegion& renderRegion) const {
  GPUState localState = m_BaseState;
  localState.enableLighting = true;
  localState.enableLight[0] = true;
  localState.enableCullFace = true;
  localState.enableTex[0] = TEX_NONE;
  localState.enableTex[1] = TEX_NONE;
  m_pContext->GetStateManager()->Apply(localState);

  // TODO get rid of all the fixed function lighting and use a shader
  FixedFunctionality();
  GLfloat light_diffuse[4]  ={0.4f,0.4f,0.4f,1.0f};
  GLfloat light_specular[4] ={1.0f,1.0f,1.0f,1.0f};
  GLfloat global_ambient[4] ={0.1f,0.1f,0.1f,1.0f};
  glLightfv(GL_LIGHT0, GL_DIFFUSE,  light_diffuse);
  glLightfv(GL_LIGHT0, GL_AMBIENT,  global_ambient);
  glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
  glLightModelf(GL_LIGHT_MODEL_LOCAL_VIEWER, 1.0f);
  glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,16.0f);
  glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,light_specular);
  glEnable(GL_COLOR_MATERIAL);
  GLfloat pfLightDirection[4]={0.0f,1.0f,1.0f,0.0f};

  FLOATMATRIX4 matModelView, mTranslation, mProjection;

  matModelView = m_mView[0];
  matModelView.setModelview();
  glLightfv(GL_LIGHT0, GL_POSITION, pfLightDirection);

  mTranslation.Translation(0.8f,0.8f,-1.85f);
  mProjection = m_mProjection[0]*mTranslation;
  mProjection.setProjection();
  FLOATMATRIX4 mRotation;
  matModelView = renderRegion.rotation*m_mView[0];
  matModelView.setModelview();

  glBegin(GL_TRIANGLES);
    glColor4f(0.0f,0.0f,1.0f,1.0f);
    for (size_t i = 0;i<m_vArrowGeometry.size();i++) {
      for (size_t j = 0;j<3;j++) {
        glNormal3f(m_vArrowGeometry[i].m_vertices[j].m_vNormal.x,
                   m_vArrowGeometry[i].m_vertices[j].m_vNormal.y,
                   m_vArrowGeometry[i].m_vertices[j].m_vNormal.z);
        glVertex3f(m_vArrowGeometry[i].m_vertices[j].m_vPos.x,
                   m_vArrowGeometry[i].m_vertices[j].m_vPos.y,
                   m_vArrowGeometry[i].m_vertices[j].m_vPos.z);
      }
    }
  glEnd();

  mRotation.RotationX(-3.1415f/2.0f);
  matModelView = mRotation*renderRegion.rotation*m_mView[0];
  matModelView.setModelview();

  glBegin(GL_TRIANGLES);
    glColor4f(0.0f,1.0f,0.0f,1.0f);
    for (size_t i = 0;i<m_vArrowGeometry.size();i++) {
      for (size_t j = 0;j<3;j++) {
        glNormal3f(m_vArrowGeometry[i].m_vertices[j].m_vNormal.x,
                   m_vArrowGeometry[i].m_vertices[j].m_vNormal.y,
                   m_vArrowGeometry[i].m_vertices[j].m_vNormal.z);
        glVertex3f(m_vArrowGeometry[i].m_vertices[j].m_vPos.x,
                   m_vArrowGeometry[i].m_vertices[j].m_vPos.y,
                   m_vArrowGeometry[i].m_vertices[j].m_vPos.z);
      }
    }
  glEnd();

  mRotation.RotationY(3.1415f/2.0f);
  matModelView = mRotation*renderRegion.rotation*m_mView[0];
  matModelView.setModelview();

  glBegin(GL_TRIANGLES);
    glColor4f(1.0f,0.0f,0.0f,1.0f);
    for (size_t i = 0;i<m_vArrowGeometry.size();i++) {
      for (size_t j = 0;j<3;j++) {
        glNormal3f(m_vArrowGeometry[i].m_vertices[j].m_vNormal.x,
                   m_vArrowGeometry[i].m_vertices[j].m_vNormal.y,
                   m_vArrowGeometry[i].m_vertices[j].m_vNormal.z);
        glVertex3f(m_vArrowGeometry[i].m_vertices[j].m_vPos.x,
                   m_vArrowGeometry[i].m_vertices[j].m_vPos.y,
                   m_vArrowGeometry[i].m_vertices[j].m_vPos.z);
      }
    }
  glEnd();
}

/// Actions to perform every subframe (rendering of a complete LOD level).
void GLRenderer::PreSubframe(const RenderRegion& renderRegion)
{
  NewFrameClear(renderRegion);

  size_t iStereoBufferCount = (m_bDoStereoRendering) ? 2 : 1;
  for (size_t i = 0;i<iStereoBufferCount;i++) {
    // Render the coordinate cross (three arrows in upper right corner)
    if (m_bRenderCoordArrows) {
      m_TargetBinder.Bind(m_pFBO3DImageCurrent[i]);
      RenderCoordArrows(renderRegion);
    }

    // write the bounding boxes into the depth buffer 
    // and the colorbuffer for isosurfacing.
    m_TargetBinder.Bind(m_pFBO3DImageCurrent[i]);
    m_mProjection[i].setProjection();
    renderRegion.modelView[i].setModelview();
    GeometryPreRender();
    PlaneIn3DPreRender();
  }
  m_TargetBinder.Unbind();
}

/// Actions which should be performed when we declare a subframe complete.
void GLRenderer::PostSubframe(const RenderRegion& renderRegion)
{
  // render the bounding boxes, clip plane, and geometry behind the volume
  size_t iStereoBufferCount = (m_bDoStereoRendering) ? 2 : 1;
  for (size_t i = 0;i<iStereoBufferCount;i++) {
    m_TargetBinder.Bind(m_pFBO3DImageCurrent[i]);
    m_mProjection[i].setProjection();
    renderRegion.modelView[i].setModelview();
    GeometryPostRender();
    PlaneIn3DPostRender();
    RenderClipPlane(i);
  }
  m_TargetBinder.Unbind();
}

bool GLRenderer::Execute3DFrame(RenderRegion3D& renderRegion,
                                float& fMsecPassed, bool& completedJob) {
  // are we starting a new LOD level?
  if (m_iBricksRenderedInThisSubFrame == 0) {
    fMsecPassed = 0;
    PreSubframe(renderRegion);
  }

  // if zero bricks are to be rendered we have completed the draw job
  if (m_vCurrentBrickList.empty()) {
    MESSAGE("zero bricks are to be rendered, completed the draw job");
    PostSubframe(renderRegion);
    completedJob = true;
    return true;
  }

  // if there is something left in the TODO list
  if (m_vCurrentBrickList.size() > m_iBricksRenderedInThisSubFrame) {
    MESSAGE("%u bricks left to render",
            static_cast<unsigned>(UINT64(m_vCurrentBrickList.size()) -
                                  m_iBricksRenderedInThisSubFrame));

    // setup shaders vars
    SetDataDepShaderVars();

    // Render a few bricks and return the time it took
    float fMsecPassedInThisPass = 0;
    if (!Render3DView(renderRegion, fMsecPassedInThisPass)) {
      completedJob = false;
      return false;
    }
    fMsecPassed += fMsecPassedInThisPass;

    // if there is nothing left todo in this subframe -> present the result
    if (m_vCurrentBrickList.size() == m_iBricksRenderedInThisSubFrame) {
      // show the timings as "other", to distinguish it from all those million messages
      OTHER("The current subframe took %g ms to render (LOD Level %u)",
            this->msecPassedCurrentFrame + fMsecPassed,
            static_cast<unsigned>(m_iCurrentLODOffset));
      PostSubframe(renderRegion);
      completedJob = true;
      return true;
    }
  }
  completedJob = false;
  return true;
}

void GLRenderer::CopyImageToDisplayBuffer() {
  GL(glViewport(0,0,m_vWinSize.x,m_vWinSize.y));
  if (m_bClearFramebuffer) ClearColorBuffer();

  GPUState localState = m_BaseState;
  localState.blendFuncSrc = BF_SRC_ALPHA;
  localState.blendFuncDst = BF_ONE_MINUS_SRC_ALPHA;
  localState.depthFunc = DF_LEQUAL;
  localState.enableTex[0] = TEX_2D;
  localState.enableTex[1] = TEX_NONE;
  m_pContext->GetStateManager()->Apply(localState);

  m_pFBO3DImageLast->Read(0);

  // when we have more than 1 region the buffer already contains the normal
  // sized region so there's no need to resize again.
  //
  // Note: We check m_bOffscreenIsLowRes instead of the
  // ::decreaseScreenResNow because the low res image we are trying
  // to display might have been rendered a while ago and now the render region
  // has decreaseScreenResNow set to false while it's in the midst of trying to
  // render a new image.
  bool decreaseRes = renderRegions.size() == 1 && m_bOffscreenIsLowRes;
  GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                     decreaseRes ? GL_LINEAR : GL_NEAREST));

  m_pFBO3DImageLast->ReadDepth(1);

  // always clear the depth buffer since we are transporting new data from the FBO
  GL(glClear(GL_DEPTH_BUFFER_BIT));

  m_pProgramTrans->Enable();

  if (decreaseRes)
    FullscreenQuadRegion(renderRegions[0], decreaseRes);
  else
    FullscreenQuad();

  m_pFBO3DImageLast->FinishRead();
  m_pFBO3DImageLast->FinishDepthRead();
}

void GLRenderer::DrawLogo() const {
  if (m_pLogoTex == NULL) return;

  FixedFunctionality();

  GPUState localState = m_BaseState;
  localState.depthMask = false; 
  localState.blendFuncSrc = BF_SRC_ALPHA;
  localState.blendFuncDst = BF_ONE_MINUS_SRC_ALPHA;
  localState.enableDepthTest = false;
  localState.enableTex[0] = TEX_2D;
  localState.enableTex[1] = TEX_NONE;
  m_pContext->GetStateManager()->Apply(localState);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(-0.5, +0.5, -0.5, +0.5, 0.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glMatrixMode(GL_TEXTURE);
  glLoadIdentity();

  m_pLogoTex->Bind();

  UINTVECTOR2 vSizes(m_pLogoTex->GetSize());
  FLOATVECTOR2 vTexelSize(1.0f/FLOATVECTOR2(vSizes));
  FLOATVECTOR2 vImageAspect(FLOATVECTOR2(vSizes)/FLOATVECTOR2(m_vWinSize));
  vImageAspect /= vImageAspect.maxVal();

  FLOATVECTOR2 vExtend(vImageAspect*0.25f);

  FLOATVECTOR2 vCenter;
  switch (m_iLogoPos) {
    case 0  : vCenter = FLOATVECTOR2(-0.50f+vExtend.x,  0.50f-vExtend.y); break;
    case 1  : vCenter = FLOATVECTOR2( 0.50f-vExtend.x,  0.50f-vExtend.y); break;
    case 2  : vCenter = FLOATVECTOR2(-0.50f+vExtend.x, -0.50f+vExtend.y); break;
    default : vCenter = FLOATVECTOR2( 0.50f-vExtend.x, -0.50f+vExtend.y); break;
  }

  glBegin(GL_QUADS);
    glColor4d(1,1,1,1);
    glTexCoord2d(0+vTexelSize.x,1-vTexelSize.y);
    glVertex3f(vCenter.x-vExtend.x, vCenter.y+vExtend.y, -0.5);
    glTexCoord2d(1-vTexelSize.x,1-vTexelSize.y);
    glVertex3f(vCenter.x+vExtend.x, vCenter.y+vExtend.y, -0.5);
    glTexCoord2d(1-vTexelSize.x,0+vTexelSize.y);
    glVertex3f(vCenter.x+vExtend.x, vCenter.y-vExtend.y, -0.5);
    glTexCoord2d(0+vTexelSize.x,0+vTexelSize.y);
    glVertex3f(vCenter.x-vExtend.x, vCenter.y-vExtend.y, -0.5);
  glEnd();

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
}

void GLRenderer::DrawBackGradient() const {
  FixedFunctionality();

  GPUState localState = m_BaseState;
  localState.depthMask = false; 
  localState.enableBlend = false;
  localState.enableDepthTest = false;
  localState.enableTex[0] = TEX_NONE;
  localState.enableTex[1] = TEX_NONE;
  m_pContext->GetStateManager()->Apply(localState);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(-1.0, +1.0, +1.0, -1.0, 0.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glBegin(GL_QUADS);
    glColor4d(m_vBackgroundColors[0].x,
              m_vBackgroundColors[0].y,
              m_vBackgroundColors[0].z, 0);
    glVertex3d(-1.0, -1.0, -0.5);
    glVertex3d( 1.0, -1.0, -0.5);
    glColor4d(m_vBackgroundColors[1].x,
              m_vBackgroundColors[1].y,
              m_vBackgroundColors[1].z,0);
    glVertex3d( 1.0,  1.0, -0.5);
    glVertex3d(-1.0,  1.0, -0.5);
  glEnd();

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
}

void GLRenderer::Cleanup() {
  GPUMemMan& mm = *(m_pMasterController->MemMan());

  if (m_pFBO3DImageLast) {
    mm.FreeFBO(m_pFBO3DImageLast);
    m_pFBO3DImageLast =NULL;
  }

  if (m_pFBOResizeQuickBlit) {
    mm.FreeFBO(m_pFBOResizeQuickBlit);
    m_pFBOResizeQuickBlit =NULL;
  }

  for (UINT32 i = 0;i<2;i++) {
    if (m_pFBO3DImageCurrent[i]) {
      mm.FreeFBO(m_pFBO3DImageCurrent[i]);
      m_pFBO3DImageCurrent[i] = NULL;
    }
    if (m_pFBOIsoHit[i]) {
      mm.FreeFBO(m_pFBOIsoHit[i]);
      m_pFBOIsoHit[i] = NULL;
    }
    if (m_pFBOCVHit[i]) {
      mm.FreeFBO(m_pFBOCVHit[i]);
      m_pFBOCVHit[i] = NULL;
    }
  }

  if (m_pLogoTex) {
    mm.FreeTexture(m_pLogoTex);
    m_pLogoTex =NULL;
  }

  // opengl may not be enabed yet so be careful calling gl functions
  if (glDeleteBuffers) GL(glDeleteBuffers(1, &m_GeoBuffer));

  CleanupShaders();
}

void GLRenderer::CreateOffscreenBuffers() {
  GPUMemMan &mm = *(Controller::Instance().MemMan());

  if (m_pFBO3DImageLast) {
    if (m_pFBOResizeQuickBlit) {
      mm.FreeFBO(m_pFBOResizeQuickBlit);
      m_pFBOResizeQuickBlit = NULL;
    }
    m_pFBOResizeQuickBlit = m_pFBO3DImageLast;
    m_pFBO3DImageLast = NULL;
  }

  for (UINT32 i=0; i < 2; i++) {
    if (m_pFBO3DImageCurrent[i]) {    
      mm.FreeFBO(m_pFBO3DImageCurrent[i]);
      m_pFBO3DImageCurrent[i] = NULL;
    }
    if (m_pFBOIsoHit[i]) {
      mm.FreeFBO(m_pFBOIsoHit[i]);
      m_pFBOIsoHit[i] = NULL;
    }
    if (m_pFBOCVHit[i]) {
      mm.FreeFBO(m_pFBOCVHit[i]);
      m_pFBOCVHit[i] = NULL;
    }
  }

  if (m_vWinSize.area() > 0) {
    MESSAGE("Creating FBOs...");
    for (UINT32 i = 0;i<2;i++) {
      switch (m_eBlendPrecision) {
        case BP_8BIT  : if (i==0) {
                          m_pFBO3DImageLast = mm.GetFBO(GL_NEAREST, GL_NEAREST,
                                                        GL_CLAMP, m_vWinSize.x,
                                                        m_vWinSize.y, GL_RGBA8,
                                                        4, true);
                        }
                        m_pFBO3DImageCurrent[i] = mm.GetFBO(GL_NEAREST,
                                                            GL_NEAREST,
                                                            GL_CLAMP,
                                                            m_vWinSize.x,
                                                            m_vWinSize.y,
                                                            GL_RGBA8, 4, true);
                        break;

        case BP_16BIT : if (i==0) {
                          m_pFBO3DImageLast = mm.GetFBO(GL_NEAREST, GL_NEAREST,
                                                        GL_CLAMP, m_vWinSize.x,
                                                        m_vWinSize.y,
                                                        m_texFormat16, 2*4,
                                                        true);
                        }
                        m_pFBO3DImageCurrent[i] = mm.GetFBO(GL_NEAREST,
                                                            GL_NEAREST,
                                                            GL_CLAMP,
                                                            m_vWinSize.x,
                                                            m_vWinSize.y,
                                                            m_texFormat16,
                                                            2*4, true);
                        break;

        case BP_32BIT : if (i==0) {
                          m_pFBO3DImageLast = mm.GetFBO(GL_NEAREST, GL_NEAREST,
                                                        GL_CLAMP, m_vWinSize.x,
                                                        m_vWinSize.y,
                                                        m_texFormat32, 4*4,
                                                        true);
                        }
                        m_pFBO3DImageCurrent[i] = mm.GetFBO(GL_NEAREST,
                                                            GL_NEAREST,
                                                            GL_CLAMP,
                                                            m_vWinSize.x,
                                                            m_vWinSize.y,
                                                            m_texFormat32,
                                                            4*4, true);
                        break;

        default       : MESSAGE("Invalid Blending Precision");
                        if (i==0) m_pFBO3DImageLast = NULL;
                        m_pFBO3DImageCurrent[i] = NULL;
                        break;
      }
      m_pFBOIsoHit[i]   = mm.GetFBO(GL_NEAREST, GL_NEAREST, GL_CLAMP,
                                    m_vWinSize.x, m_vWinSize.y, m_texFormat32,
                                    4*4, true, 2);

      m_pFBOCVHit[i]    = mm.GetFBO(GL_NEAREST, GL_NEAREST, GL_CLAMP,
                                    m_vWinSize.x, m_vWinSize.y, m_texFormat16,
                                    2*4, true, 2);
    }
  }
}

void
GLRenderer::SetBrickDepShaderVarsSlice(const UINTVECTOR3& vVoxelCount) const
{
  if (m_eRenderMode == RM_2DTRANS) {
    FLOATVECTOR3 vStep = 1.0f/FLOATVECTOR3(vVoxelCount);
    m_pProgram2DTransSlice->Set("vVoxelStepsize",
                                             vStep.x, vStep.y, vStep.z);
  }
}

// If we're downsampling the data, no scaling is needed, but otherwise we need
// to scale the TF in the same manner that we've scaled the data.
float GLRenderer::CalculateScaling()
{
  double fMaxValue     = MaxValue();
  UINT32 iMaxRange     = UINT32(1<<m_pDataset->GetBitWidth());
  return (m_pDataset->GetBitWidth() != 8 && m_bDownSampleTo8Bits) ?
    1.0f : float(iMaxRange/fMaxValue);
}

void GLRenderer::SetDataDepShaderVars() {
  MESSAGE("Setting up vars");

  // if m_bDownSampleTo8Bits is enabled the full range from 0..255 -> 0..1 is used
  float fScale         = CalculateScaling();
  float fGradientScale = (m_pDataset->MaxGradientMagnitude() == 0) ?
                          1.0f : 1.0f/m_pDataset->MaxGradientMagnitude();

  MESSAGE("Transfer function scaling factor: %5.3f", fScale);
  MESSAGE("Gradient scaling factor: %5.3f", fGradientScale);

  bool bMipViewActive = false;
  bool bSliceViewActive = false;
  bool b3DViewActive = false;
  for (size_t i=0; i < renderRegions.size(); ++i) {
    if (renderRegions[i]->is2D()) {
      bSliceViewActive = true;
      if (renderRegions[i]->GetUseMIP()) {
        bMipViewActive = true;
      } 
    } else {
      b3DViewActive = true;
    }
  }



  // If we're rendering RGBA data, we don't scale the TFqn... because we
  // don't even use a TFqn.
  if(!this->RGBAData() && bMipViewActive) {
    m_pProgramTransMIP->Enable();
    m_pProgramTransMIP->Set("fTransScale",fScale);
  }


  switch (m_eRenderMode) {
    case RM_1DTRANS: {
      if(!this->RGBAData()) {
        if (bSliceViewActive) {
          m_pProgram1DTransSlice->Enable();
          m_pProgram1DTransSlice->Set("fTransScale",fScale);

          m_pProgram1DTransSlice3D->Enable();
          m_pProgram1DTransSlice3D->Set("fTransScale",fScale);
        }
        if (b3DViewActive) {
          m_pProgram1DTrans[m_bUseLighting ? 1 : 0]->Enable();
          m_pProgram1DTrans[m_bUseLighting ? 1 : 0]->Set("fTransScale",fScale);
        }
      }
      break;
    }
    case RM_2DTRANS: {
      if (bSliceViewActive) {
        m_pProgram2DTransSlice->Enable();
        m_pProgram2DTransSlice->Set("fTransScale",fScale);
        m_pProgram2DTransSlice->Set("fGradientScale",fGradientScale);

        m_pProgram2DTransSlice3D->Enable();
        m_pProgram2DTransSlice3D->Set("fTransScale",fScale);
        m_pProgram2DTransSlice3D->Set("fGradientScale",fGradientScale);
      }
      if (b3DViewActive) {
        m_pProgram2DTrans[m_bUseLighting ? 1 : 0]->Enable();
        m_pProgram2DTrans[m_bUseLighting ? 1 : 0]->Set("fTransScale",fScale);
        m_pProgram2DTrans[m_bUseLighting ? 1 : 0]->Set("fGradientScale",fGradientScale);
      }
      break;
    }
    case RM_ISOSURFACE: {
      // as we are rendering the 2 slices with the 1d transferfunction in iso 
      // mode, we need update that shader, too
      if (bSliceViewActive) {
        m_pProgram1DTransSlice->Enable();
        m_pProgram1DTransSlice->Set("fTransScale",fScale);

        m_pProgram1DTransSlice3D->Enable();
        m_pProgram1DTransSlice3D->Set("fTransScale",fScale);
      }
      if (b3DViewActive) {
        GLSLProgram* shader = (m_pDataset->GetComponentCount() == 1) ?
        m_pProgramIso : m_pProgramColor;

        shader->Enable();
        shader->Set("fIsoval", static_cast<float>
                                            (this->GetNormalizedIsovalue()));
      }
      break;
    }
    case RM_INVALID: T_ERROR("Invalid rendermode set"); break;
  }

  MESSAGE("Done");
}


void GLRenderer::SetBlendPrecision(EBlendPrecision eBlendPrecision) {
  if (eBlendPrecision != m_eBlendPrecision) {
    AbstrRenderer::SetBlendPrecision(eBlendPrecision);
    CreateOffscreenBuffers();
  }
}

static std::string find_shader(std::string file, bool subdirs)
{
#ifdef DETECTED_OS_APPLE
  if (SysTools::FileExists(SysTools::GetFromResourceOnMac(file))) {
    file = SysTools::GetFromResourceOnMac(file);
    MESSAGE("Found %s in bundle, using that.", file.c_str());
    return file;
  }
#endif

  // if it doesn't exist, try our subdirs.
  if(!SysTools::FileExists(file) && subdirs) {
    std::vector<std::string> dirs = SysTools::GetSubDirList(
      Controller::Instance().SysInfo()->GetProgramPath()
    );
    dirs.push_back(Controller::Instance().SysInfo()->GetProgramPath());

    std::string raw_fn = SysTools::GetFilename(file);
    for(std::vector<std::string>::const_iterator d = dirs.begin();
        d != dirs.end(); ++d) {
      std::string testfn = *d + "/" + raw_fn;
      if(SysTools::FileExists(testfn)) {
        return testfn;
      }
    }
    WARNING("Could not find '%s'", file.c_str());
    return "";
  }

  return file;
}

namespace {
  template <typename ForwIter>
  bool all_exist(ForwIter bgn, ForwIter end) {
    if(bgn == end) { WARNING("Odd, empty range..."); }
    while(bgn != end) {
      if(!SysTools::FileExists(*bgn)) {
        return false;
      }
      ++bgn;
    }
    return true;
  }
}

bool GLRenderer::LoadAndVerifyShader(GLSLProgram** program,
                                     const std::vector<std::string> strDirs,
                                     ...)
{
  // first build list of fragment shaders
  std::vector<std::string> vertex;
  std::vector<std::string> frag;

  va_list args;
  va_start(args, strDirs);
  {
    const char* filename;
    // We expect two NULLs; the first terminates the vertex shader list, the
    // latter terminates the fragment shader list.
    do {
      filename = va_arg(args, const char*);
      if(filename != NULL) {
        std::string shader = find_shader(std::string(filename), false);
        if(shader == "") {
          WARNING("Could not find VS shader '%s'!", filename);
        }
        vertex.push_back(shader);
      }
    } while(filename != NULL);

    do {
      filename = va_arg(args, const char*);
      if(filename != NULL) {
        std::string shader = find_shader(std::string(filename), false);
        if(shader == "") {
          WARNING("Could not find FS shader '%s'!", filename);
        }
        frag.push_back(shader);
      }
    } while(filename != NULL);
  }
  va_end(args);

  if(!vertex.empty() && !frag.empty() &&
     all_exist(vertex.begin(), vertex.end()) &&
     all_exist(frag.begin(), frag.end())) {
    return LoadAndVerifyShader(vertex, frag, program);
  }

  // now iterate through all directories, looking for our shaders in them.
  for (size_t i = 0;i<strDirs.size();i++) {
    if(!SysTools::FileExists(strDirs[i])) {
      continue;
    }

    std::vector<std::string> fullVS(vertex.size());
    std::vector<std::string> fullFS(frag.size());

    // prepend the directory name, if needed.
    for(size_t j=0; j < fullVS.size(); ++j) {
      if(SysTools::FileExists(vertex[j])) {
        fullVS[j] = vertex[j];
      } else {
        fullVS[j] = strDirs[i] + "/" + vertex[j];
      }
    }
    // if any of those files don't exist, skip this directory.
    if(fullVS.empty() || !all_exist(fullVS.begin(), fullVS.end())) {
      WARNING("Not all vertex shaders present in %s, skipping...",
              strDirs[i].c_str());
      continue;
    }

    // prepend the directory to the fragment shader path, if needed.
    for(size_t j=0; j < fullFS.size(); ++j) {
      if(SysTools::FileExists(frag[j])) {
        fullFS[j] = frag[j];
      } else {
        fullFS[j] = strDirs[i] + "/" + frag[j];
      }
    }

    // if any of those files don't exist, skip this directory.
    if(fullFS.empty() || !all_exist(fullFS.begin(), fullFS.end())) {
      WARNING("Not all fragment shaders present in %s, skipping...",
              strDirs[i].c_str());
      continue;
    }

    if (LoadAndVerifyShader(fullVS, fullFS, program)) {
      return true;
    }
  }

  {
    std::ostringstream shaders;
    shaders << "Shaders [VS: ";
    std::copy(vertex.begin(), vertex.end(),
              std::ostream_iterator<std::string>(shaders, ", "));
    shaders << " FS: ";
    std::copy(frag.begin(), frag.end(),
              std::ostream_iterator<std::string>(shaders, ", "));
    shaders << "] not found!";
    T_ERROR("%s", shaders.str().c_str());
  }
  return false;
}

bool GLRenderer::LoadAndVerifyShader(std::vector<std::string> vert,
                                     std::vector<std::string> frag,
                                     GLSLProgram** program) const
{
  for(std::vector<std::string>::iterator v = vert.begin(); v != vert.end(); ++v)
  {
    *v = find_shader(*v, false);
    if(v->empty()) {
      WARNING("We'll need to search for vertex shader '%s'...", v->c_str());
    }
  }
      
  for(std::vector<std::string>::iterator f = frag.begin(); f != frag.end();
      ++f) {
    *f = find_shader(*f, false);
    if(f->empty()) {
      WARNING("We'll need to search for fragment shader '%s'...", f->c_str());
    }
  }

  GPUMemMan& mm = *(m_pMasterController->MemMan());
  (*program) = mm.GetGLSLProgram(vert, frag);

  if((*program) == NULL || !(*program)->IsValid()) {
    /// @todo fixme report *which* shaders!
    T_ERROR("Error loading shaders.");
    mm.FreeGLSLProgram(*program);
    return false;
  }

  return true;
}

void GLRenderer::CheckMeshStatus() {
  // if we can do geometry then let's first find out information
  // about the geometry to render
  if (m_bSupportsMeshes) {
    m_iNumTransMeshes = 0;
    m_iNumMeshes = 0;
    for (vector<RenderMesh*>::iterator mesh = m_Meshes.begin();
         mesh != m_Meshes.end(); mesh++) {
      if ((*mesh)->GetActive()) {
        m_iNumMeshes++;
        if (!(*mesh)->IsCompletelyOpaque()) 
          m_iNumTransMeshes++;
      }
    }
    MESSAGE("Found %u meshes %u of which contain transparent parts.",
            static_cast<unsigned>(m_iNumMeshes),
            static_cast<unsigned>(m_iNumTransMeshes));
  }
}

void GLRenderer::GeometryPreRender() {
  CheckMeshStatus();
  // for rendering modes other than isosurface render the bbox in the first
  // pass once, to init the depth buffer.  for isosurface rendering we can go
  // ahead and render the bbox directly as isosurfacing writes out correct
  // depth values
  if (m_eRenderMode != RM_ISOSURFACE || m_bDoClearView) {

    GPUState localState = m_BaseState;
    localState.enableBlend = false;
    localState.depthMask = false;
    m_pContext->GetStateManager()->Apply(localState);

    // first render the pars of the meshes that are in front of the volume
    // remember the volume uses front to back compositing
    if (m_bSupportsMeshes && m_iNumMeshes) {
      m_pProgramMeshFTB->Enable();
      RenderTransFrontGeometry();
    }

    // now write the depth mask of the opaque geomentry into the buffer
    // as we do front to back compositing we can not write the colors 
    // into the buffer yet
    // start with the bboxes
    m_pContext->GetStateManager()->SetDepthMask(true);
    m_pContext->GetStateManager()->SetColorMask(false);

    if (m_bRenderGlobalBBox)
      RenderBBox();
    if (m_bRenderLocalBBox) {
      for (size_t iCurrentBrick = 0;
           iCurrentBrick < m_vCurrentBrickList.size();
           iCurrentBrick++) {
        if (m_vCurrentBrickList[iCurrentBrick].bIsEmpty) 
          RenderBBox(FLOATVECTOR4(1,1,0,1),
                     m_vCurrentBrickList[iCurrentBrick].vCenter,
                     m_vCurrentBrickList[iCurrentBrick].vExtension*0.99f);
        else
          RenderBBox(FLOATVECTOR4(0,1,0,1),
                     m_vCurrentBrickList[iCurrentBrick].vCenter,
                     m_vCurrentBrickList[iCurrentBrick].vExtension);
      }
    }

    // now the opaque parts of the mesh
    if (m_bSupportsMeshes && m_iNumMeshes) {
      // FTB and BTF would both be ok here, so we use BTF as it is simpler
      m_pProgramMeshBTF->Enable(); 
      RenderOpaqueGeometry();
    }
  } else {
    // if we are in isosurface mode none of the complicated stuff from 
    // above applies, as the volume is opqaue we can just use regular
    // depth testing and the order of the opaque elements does not matter
    // so we might as well now write all the opaque geoemtry into the color
    // and depth buffer

    GPUState localState = m_BaseState;
    localState.enableBlend = false;
    m_pContext->GetStateManager()->Apply(localState);

    // first the bboxes
    if (m_bRenderGlobalBBox) RenderBBox();
    if (m_bRenderLocalBBox) {
      for (size_t iCurrentBrick = 0;
           iCurrentBrick < m_vCurrentBrickList.size();
           iCurrentBrick++) {
        if (m_vCurrentBrickList[iCurrentBrick].bIsEmpty) 
          RenderBBox(FLOATVECTOR4(1,1,0,1),
                     m_vCurrentBrickList[iCurrentBrick].vCenter,
                     m_vCurrentBrickList[iCurrentBrick].vExtension*0.99f);
        else
          RenderBBox(FLOATVECTOR4(0,1,0,1),
                     m_vCurrentBrickList[iCurrentBrick].vCenter,
                     m_vCurrentBrickList[iCurrentBrick].vExtension);
      }
    }
    // the the opaque parts of the meshes
    if (m_bSupportsMeshes && m_iNumMeshes) {
      // FTB and BTF would both be ok here, so we use BTF as it is simpler
      m_pProgramMeshBTF->Enable();
      RenderOpaqueGeometry();
    }
  }
}

// For volume rendering, we render the bounding box again after rendering the
// dataset.  This is because we want the box lines which are in front of the
// dataset to appear .. well, in front of the dataset.
void GLRenderer::GeometryPostRender() {
  // Not required for isosurfacing, since we use the depth buffer for
  // occluding/showing the bbox's outline.
  if (m_eRenderMode != RM_ISOSURFACE || m_bDoClearView) {

    GPUState localState = m_BaseState;
    localState.depthFunc = DF_LEQUAL;
    m_pContext->GetStateManager()->Apply(localState);

    if (m_bRenderGlobalBBox) RenderBBox();
    if (m_bRenderLocalBBox) {
      for (size_t iCurrentBrick = 0;iCurrentBrick<m_vCurrentBrickList.size();iCurrentBrick++) {
        if (m_vCurrentBrickList[iCurrentBrick].bIsEmpty) 
          RenderBBox(FLOATVECTOR4(1,1,0,1),
                     m_vCurrentBrickList[iCurrentBrick].vCenter,
                     m_vCurrentBrickList[iCurrentBrick].vExtension*0.99f);
        else
          RenderBBox(FLOATVECTOR4(0,1,0,1),
                     m_vCurrentBrickList[iCurrentBrick].vCenter,
                     m_vCurrentBrickList[iCurrentBrick].vExtension);
      }
    }

    if (m_bSupportsMeshes && m_iNumMeshes) {
      // FTB and BTF would both be ok here, so we use BTF as it is simpler
      m_pProgramMeshBTF->Enable();
      m_pProgramMeshBTF->Set("fOffset",0.001f);
      RenderOpaqueGeometry();
      m_pProgramMeshBTF->Set("fOffset",0.0f);
    }

    m_pContext->GetStateManager()->SetEnableDepthTest(false);

    if (m_bSupportsMeshes && m_iNumMeshes) {
      m_pProgramMeshFTB->Enable();
      RenderTransBackGeometry();
    }
  } else {
    if (m_bSupportsMeshes && m_iNumMeshes) {

      GPUState localState = m_BaseState;
      localState.depthMask = false;
      m_pContext->GetStateManager()->Apply(localState);

      // "over"-compositing with proper alpha
      // we only use this once in the project so we bypass 
      // the state manager, be carfeull to reset it below
      GL(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                             GL_ONE, GL_ONE_MINUS_SRC_ALPHA));

      m_pProgramMeshBTF->Enable();
    
      SetMeshBTFSorting(true);
      RenderTransBackGeometry();
      RenderTransInGeometry();
      RenderTransFrontGeometry();
      SetMeshBTFSorting(false);

      // reset the blending in the state manager
      m_pContext->GetStateManager()->SetBlendFunction(BF_ONE,BF_ONE, true);
    }
  }
}

void GLRenderer::SetMeshBTFSorting(bool bSortBTF) {
  m_bSortMeshBTF = bSortBTF;
  for (vector<RenderMesh*>::iterator mesh = m_Meshes.begin();
       mesh != m_Meshes.end(); mesh++) {
    (*mesh)->EnableOverSorting(bSortBTF);
  }
}

void GLRenderer::RenderOpaqueGeometry() {
  for (vector<RenderMesh*>::iterator mesh = m_Meshes.begin();
       mesh != m_Meshes.end(); mesh++) {
   if ((*mesh)->GetActive()) (*mesh)->RenderOpaqueGeometry();
  }
}

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

struct MeshFormat {
  FLOATVECTOR3 m_vPos;
  FLOATVECTOR4 m_vColor;
  FLOATVECTOR3 m_vNormal;
  FLOATVECTOR2 m_vTexCoords;
};

static const GLsizei iStructSize = GLsizei(sizeof(MeshFormat));

void ListEntryToMeshFormat(std::vector<MeshFormat>& list, 
                           const RenderMesh* mesh,
                           size_t startIndex) {

  bool bHasNormal = mesh->GetNormalIndices().size() == mesh->GetVertexIndices().size();
  bool bHasTC     = mesh->GetTexCoordIndices().size() == mesh->GetVertexIndices().size();

  MeshFormat f;

  // currently we only support triangles, hence the 3
  for (size_t i = 0;i<3;i++) {
    size_t vertexIndex =  mesh->GetVertexIndices()[startIndex+i];
    f.m_vPos =  mesh->GetVertices()[vertexIndex];

    if (mesh->UseDefaultColor())
      f.m_vColor = mesh->GetDefaultColor();
    else
      f.m_vColor = mesh->GetColors()[vertexIndex];

    if (bHasNormal) 
      f.m_vNormal = mesh->GetNormals()[vertexIndex];
    else
      f.m_vNormal = FLOATVECTOR3(2,2,2);

    if (bHasNormal) 
      f.m_vNormal = mesh->GetNormals()[vertexIndex];
    else
      f.m_vNormal = FLOATVECTOR3(2,2,2);

    if (bHasTC) 
      f.m_vTexCoords = mesh->GetTexCoords()[vertexIndex];
    else
      f.m_vTexCoords = FLOATVECTOR2(0,0);

    list.push_back(f);
  }
}

void GLRenderer::RenderMergedMesh(SortIndexPVec& mergedMesh) {
  // terminate early if the mesh is empty
  if (mergedMesh.empty()) return;

  // sort the mesh
  std::sort(mergedMesh.begin(), mergedMesh.end(), 
           (m_bSortMeshBTF) ?  DistanceSortOver : DistanceSortUnder);

  // turn it into something renderable
  std::vector<MeshFormat> list;
  for (SortIndexPVec::const_iterator index = mergedMesh.begin();
       index != mergedMesh.end();
       index++) {   
    ListEntryToMeshFormat(list, (*index)->m_mesh, (*index)->m_index);
  }

  // render it
  // all of the following calls are bypassing the state manager
  GL(glBindBuffer(GL_ARRAY_BUFFER, m_GeoBuffer));
  GL(glBufferData(GL_ARRAY_BUFFER, GLsizei(list.size())*iStructSize,
                  &list[0], GL_STREAM_DRAW));
  GL(glVertexPointer(3, GL_FLOAT, iStructSize, BUFFER_OFFSET(0)));
  GL(glColorPointer(4, GL_FLOAT, iStructSize, BUFFER_OFFSET(3*sizeof(float))));
  GL(glNormalPointer(GL_FLOAT, iStructSize, BUFFER_OFFSET(7*sizeof(float))));
  GL(glTexCoordPointer(2, GL_FLOAT, iStructSize,
                       BUFFER_OFFSET(10*sizeof(float))));
  GL(glEnableClientState(GL_VERTEX_ARRAY)); 
  GL(glEnableClientState(GL_COLOR_ARRAY));
  GL(glEnableClientState(GL_NORMAL_ARRAY));
  GL(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
  GL(glDrawArrays(GL_TRIANGLES, 0, GLsizei(list.size())));
  GL(glDisableClientState(GL_VERTEX_ARRAY));
  GL(glDisableClientState(GL_COLOR_ARRAY));
  GL(glDisableClientState(GL_NORMAL_ARRAY));
  GL(glDisableClientState(GL_TEXTURE_COORD_ARRAY)); 
}

void GLRenderer::RenderTransBackGeometry() {
  // no transparent mesh -> nothing todo
  if (m_iNumTransMeshes == 0)  return;

  // only one transparent mesh -> render it
  if (m_iNumTransMeshes == 1)  {
    for (vector<RenderMesh*>::iterator mesh = m_Meshes.begin();
         mesh != m_Meshes.end(); mesh++) {
     if ((*mesh)->GetActive()) (*mesh)->RenderTransGeometryBehind();
    }
    return;
  }
  
  // more than one transparent mesh -> merge them before sorting and rendering
  SortIndexPVec mergedMesh;
  for (vector<RenderMesh*>::iterator mesh = m_Meshes.begin();
       mesh != m_Meshes.end(); mesh++) {
    if ((*mesh)->GetActive()) {
      const SortIndexPVec& m = (*mesh)->GetBehindPointList(false);
      // don't worry about empty meshes
      if (m.empty()) continue;
      // currently only triangles are supported
      if (m[0]->m_mesh->GetVerticesPerPoly() != 3) continue;
      // merge lists
      mergedMesh.insert(mergedMesh.end(), m.begin(), m.end());   
    }
  }
  RenderMergedMesh(mergedMesh);
}

void GLRenderer::RenderTransInGeometry() {
  // no transparent mesh -> nothing todo
  if (m_iNumTransMeshes == 0)  return;

  // only one transparent mesh -> render it
  if (m_iNumTransMeshes == 1)  {
    for (vector<RenderMesh*>::iterator mesh = m_Meshes.begin();
         mesh != m_Meshes.end(); mesh++) {
     if ((*mesh)->GetActive()) (*mesh)->RenderTransGeometryInside();
    }
    return;
  }
  
  // more than one transparent mesh -> merge them before sorting and rendering
  SortIndexPVec mergedMesh;
  for (vector<RenderMesh*>::iterator mesh = m_Meshes.begin();
       mesh != m_Meshes.end(); mesh++) {
    if ((*mesh)->GetActive()) {
      const SortIndexPVec& m = (*mesh)->GetInPointList(false);
      // don't worry about empty meshes
      if (m.empty()) continue;
      // currently only triangles are supported
      if (m[0]->m_mesh->GetVerticesPerPoly() != 3) continue;
      // merge lists
      mergedMesh.insert(mergedMesh.end(), m.begin(), m.end());   
    }
  }
  RenderMergedMesh(mergedMesh);
}

void GLRenderer::RenderTransFrontGeometry() {
  // no transparent mesh -> nothing todo
  if (m_iNumTransMeshes == 0)  return;

  // only one transparent mesh -> render it
  if (m_iNumTransMeshes == 1)  {
    for (vector<RenderMesh*>::iterator mesh = m_Meshes.begin();
          mesh != m_Meshes.end(); mesh++) {
      if ((*mesh)->GetActive()) (*mesh)->RenderTransGeometryFront();
    }
    return;
  }
  
  // more than one transparent mesh -> merge them before sorting and rendering
  SortIndexPVec mergedMesh;
  for (vector<RenderMesh*>::iterator mesh = m_Meshes.begin();
       mesh != m_Meshes.end(); mesh++) {
    if ((*mesh)->GetActive()) {
      const SortIndexPVec& m = (*mesh)->GetFrontPointList(false);
      // don't worry about empty meshes
      if (m.empty()) continue;
      // currently only triangles are supported
      if (m[0]->m_mesh->GetVerticesPerPoly() != 3) continue;
      // merge lists
      mergedMesh.insert(mergedMesh.end(), m.begin(), m.end());   
    }
  }
  RenderMergedMesh(mergedMesh);
}

void GLRenderer::PlaneIn3DPreRender() {
  if (!m_bRenderPlanesIn3D) return;

  FixedFunctionality();

  // for rendering modes other than isosurface render the planes in the first
  // pass once to init the depth buffer.  for isosurface rendering we can go
  // ahead and render the planes directly as isosurfacing writes out correct
  // depth values
  if (m_eRenderMode != RM_ISOSURFACE || m_bDoClearView) {  
    RenderPlanesIn3D(true);
  } else {
    RenderPlanesIn3D(false);
  }
}

void GLRenderer::PlaneIn3DPostRender() {
  if (!m_bRenderPlanesIn3D) return;

  FixedFunctionality();

  // Not required for isosurfacing, since we use the depth buffer for
  // occluding/showing the planes
  if (m_eRenderMode != RM_ISOSURFACE || m_bDoClearView) {
    GPUState localState = m_BaseState;
    localState.enableDepthTest = false;
    m_pContext->GetStateManager()->Apply(localState);
    
    RenderPlanesIn3D(false);
  }
}

void GLRenderer::RenderPlanesIn3D(bool bDepthPassOnly) {
  FLOATVECTOR3 vCenter, vExtend;
  GetVolumeAABB(vCenter, vExtend);

  FLOATVECTOR3 vMinPoint = -vExtend/2.0f, vMaxPoint = vExtend/2.0f;

  GPUState localState = m_BaseState;
  localState.depthFunc = DF_LEQUAL;
  localState.lineWidth = 2.0f;
  localState.enableTex[0] = TEX_NONE;
  localState.enableTex[1] = TEX_NONE;
  localState.colorMask = !bDepthPassOnly;
  m_pContext->GetStateManager()->Apply(localState);
  
  if (!bDepthPassOnly) glColor4f(1,1,1,1);
  for (size_t i=0; i < renderRegions.size(); ++i) {

    int k=0;
    switch (renderRegions[i]->windowMode) {
    case RenderRegion::WM_SAGITTAL: k=0; break;
    case RenderRegion::WM_AXIAL   : k=1; break;
    case RenderRegion::WM_CORONAL : k=2; break;
    default: continue;
    };

    const float sliceIndex = static_cast<float>(renderRegions[i]->GetSliceIndex()) / m_pDataset->GetDomainSize()[k];
    const float planePos = vMinPoint[k] * (1.0f-sliceIndex) + vMaxPoint[k] * sliceIndex;

    glBegin(GL_LINE_LOOP);
      switch (renderRegions[i]->windowMode) {
        case RenderRegion::WM_SAGITTAL   :
          glVertex3f(planePos, vMinPoint.y, vMaxPoint.z);
          glVertex3f(planePos, vMinPoint.y, vMinPoint.z);
          glVertex3f(planePos, vMaxPoint.y, vMinPoint.z);
          glVertex3f(planePos, vMaxPoint.y, vMaxPoint.z);
          break;
        case RenderRegion::WM_AXIAL   :
          glVertex3f(vMaxPoint.x, planePos, vMinPoint.z);
          glVertex3f(vMinPoint.x, planePos, vMinPoint.z);
          glVertex3f(vMinPoint.x, planePos, vMaxPoint.z);
          glVertex3f(vMaxPoint.x, planePos, vMaxPoint.z);
          break;
        case RenderRegion::WM_CORONAL :
          glVertex3f(vMaxPoint.x, vMinPoint.y, planePos);
          glVertex3f(vMinPoint.x, vMinPoint.y, planePos);
          glVertex3f(vMinPoint.x, vMaxPoint.y, planePos);
          glVertex3f(vMaxPoint.x, vMaxPoint.y, planePos);
          break;
        default: break; // Should not get here.
      };
    glEnd();
  }
}

/** Renders the currently configured clip plane.
 * The plane logic is mostly handled by ExtendedPlane::Quad: though we only
 * need the plane's normal to clip things, we store an orthogonal vector for
 * the plane's surface specifically to make rendering the plane easy. */
void GLRenderer::RenderClipPlane(size_t iStereoID)
{
  /* Bail if the user doesn't want to use or see the plane. */
  if(!m_bClipPlaneOn || !m_bClipPlaneDisplayed) { return ; }

  FLOATVECTOR4 vColorQuad(0.0f,0.0f,0.8f,0.4f);
  FLOATVECTOR4 vColorBorder(1.0f,1.0f,0.0f,1.0f);

  ExtendedPlane transformed(m_ClipPlane);
  m_mView[iStereoID].setModelview();

  FixedFunctionality();
  GPUState localState = m_BaseState;	
  localState.enableTex[0] = TEX_NONE;
  localState.enableTex[1] = TEX_NONE;

  typedef std::vector<FLOATVECTOR3> TriList;
  TriList quad;
  /* transformed.Quad will give us back a list of triangle vertices; the return
    * value gives us the order we should render those so that we don't mess up
    * front/back faces. */ 
  bool ccw = transformed.Quad(m_vEye, quad);

  if (m_iNumMeshes == 0) {
    if((m_eRenderMode != RM_ISOSURFACE || m_bDoClearView) && !ccw) {
      vColorQuad *= vColorQuad.w;
      vColorBorder *= vColorBorder.w;
      localState.blendFuncSrc = BF_ONE_MINUS_DST_ALPHA;
      localState.blendFuncDst = BF_ONE;
    } else {
      localState.blendFuncSrc = BF_SRC_ALPHA;
      localState.blendFuncDst = BF_ONE_MINUS_SRC_ALPHA;
    }

    // Now render the plane.
    m_pContext->GetStateManager()->Apply(localState);

    glBegin(GL_TRIANGLES);
      glColor4f(vColorQuad.x, vColorQuad.y, vColorQuad.z, vColorQuad.w);
      for(size_t i=0; i < 6; i+=3) { // 2 tris: 6 points.
        glVertex3f(quad[i+0].x, quad[i+0].y, quad[i+0].z);
        glVertex3f(quad[i+1].x, quad[i+1].y, quad[i+1].z);
        glVertex3f(quad[i+2].x, quad[i+2].y, quad[i+2].z);
      }
    glEnd();
    glEnable(GL_LINE_SMOOTH); // bypassing the state manager here
  } else {
    localState.enableBlend = false;
    m_pContext->GetStateManager()->Apply(localState);
  }

  m_pContext->GetStateManager()->SetLineWidth(4.0f);
  glBegin(GL_LINES);
    glColor4f(vColorBorder.x, vColorBorder.y, vColorBorder.z, vColorBorder.w);
    for(size_t i = 6; i<14 ; i += 2) {
      glVertex3f(quad[i+0].x, quad[i+0].y, quad[i+0].z);
      glVertex3f(quad[i+1].x, quad[i+1].y, quad[i+1].z);
    }
  glEnd();
  glDisable(GL_LINE_SMOOTH); // bypassing the state manager here
}

void GLRenderer::ScanForNewMeshes() {
  const vector<Mesh*>& meshVec = m_pDataset->GetMeshes();
  for (size_t i = m_Meshes.size(); i<meshVec.size();i++) {
    m_Meshes.push_back(new RenderMeshGL(*meshVec[i]));
    m_Meshes[m_Meshes.size()-1]->InitRenderer();
  }
  Schedule3DWindowRedraws();
}

void GLRenderer::FixedFunctionality() const {
  GLSLProgram::Disable();
}

void GLRenderer::SyncStateManager() {
  m_pContext->GetStateManager()->Apply(m_BaseState, true);  
}

bool GLRenderer::LoadDataset(const string& strFilename) {
  if(!AbstrRenderer::LoadDataset(strFilename)) {
    return false;
  }

  if (m_pProgram1DTrans[0] != NULL) SetDataDepShaderVars();

  // convert meshes in dataset to RenderMeshes
  const vector<Mesh*>& meshVec = m_pDataset->GetMeshes();
  for (vector<Mesh*>::const_iterator mesh = meshVec.begin();
       mesh != meshVec.end(); mesh++) {
    m_Meshes.push_back(new RenderMeshGL(**mesh));
  }

  return true;
}

void GLRenderer::Recompose3DView(const RenderRegion3D& renderRegion) {
  MESSAGE("Recompositing...");
  NewFrameClear(renderRegion);

  size_t iStereoBufferCount = (m_bDoStereoRendering) ? 2 : 1;
  for (size_t i = 0;i<iStereoBufferCount;i++) {
    m_TargetBinder.Bind(m_pFBO3DImageCurrent[i]);
    m_mProjection[i].setProjection();
    renderRegion.modelView[i].setModelview();
    GeometryPreRender();
    PlaneIn3DPreRender();
    ComposeSurfaceImage(renderRegion, int(i));
    GeometryPostRender();
    PlaneIn3DPostRender();
    RenderClipPlane(i);
  }
  m_TargetBinder.Unbind();
}

bool GLRenderer::Render3DView(const RenderRegion3D& renderRegion,
                              float& fMsecPassed) {
  Render3DPreLoop(renderRegion);
  size_t iStereoBufferCount = (m_bDoStereoRendering) ? 2 : 1;

  // loop over all bricks in the current LOD level
  m_Timer.Start();
  UINT32 bricks_this_call = 0;
  fMsecPassed = 0;

  while (m_vCurrentBrickList.size() > m_iBricksRenderedInThisSubFrame &&
         (m_eRendererTarget == RT_HEADLESS || fMsecPassed < m_iTimeSliceMSecs)) {
    MESSAGE("  Brick %u of %u",
            static_cast<unsigned>(m_iBricksRenderedInThisSubFrame+1),
            static_cast<unsigned>(m_vCurrentBrickList.size()));

    const BrickKey& bkey = m_vCurrentBrickList[size_t(m_iBricksRenderedInThisSubFrame)].kBrick;

    MESSAGE("  Requesting texture from MemMan");

    if(BindVolumeTex(bkey, m_iIntraFrameCounter++)) {
      MESSAGE("  Binding Texture");
    } else {
      T_ERROR("Cannot bind texture, GetVolume returned invalid volume");
      return false;
    }

    Render3DInLoop(renderRegion, size_t(m_iBricksRenderedInThisSubFrame),0);
    if (m_bDoStereoRendering) {
      if (m_vLeftEyeBrickList[size_t(m_iBricksRenderedInThisSubFrame)].kBrick !=
          m_vCurrentBrickList[size_t(m_iBricksRenderedInThisSubFrame)].kBrick) {
        const BrickKey& left_eye_key = m_vLeftEyeBrickList[size_t(m_iBricksRenderedInThisSubFrame)].kBrick;

        UnbindVolumeTex();
        if(BindVolumeTex(left_eye_key, m_iIntraFrameCounter++)) {
          MESSAGE("  Binding Texture (left eye)");
        } else {
          T_ERROR("Cannot bind texture (left eye), GetVolume returned invalid volume");
          return false;
        }
      }

      Render3DInLoop(renderRegion, size_t(m_iBricksRenderedInThisSubFrame),1);
    }

    // release the 3D texture
    if (!UnbindVolumeTex()) {
      T_ERROR("Cannot unbind volume.");
      return false;
    }

    // count the bricks rendered
    m_iBricksRenderedInThisSubFrame++;

    if (m_eRendererTarget != RT_CAPTURE) {
#ifdef DETECTED_OS_APPLE
      // really (hopefully) force a pipleine flush
      unsigned char dummy[4];
      glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,dummy);
#else
      // let's pretend this actually does what it should
      glFinish();
#endif
    }
    // time this loop
    fMsecPassed = float(m_Timer.Elapsed());

    ++bricks_this_call;
  }
  MESSAGE("Rendered %u bricks this call.", bricks_this_call);

  Render3DPostLoop();

  if (m_eRenderMode == RM_ISOSURFACE &&
      m_vCurrentBrickList.size() == m_iBricksRenderedInThisSubFrame) {
     
      for (size_t i = 0;i<iStereoBufferCount;i++) {
        m_TargetBinder.Bind(m_pFBO3DImageCurrent[i]);
        ComposeSurfaceImage(renderRegion, int(i));
      } 

      m_TargetBinder.Unbind();
  }

  return true;
}

void GLRenderer::SetLogoParams(std::string strLogoFilename, int iLogoPos) {
  AbstrRenderer::SetLogoParams(strLogoFilename, iLogoPos);

  GPUMemMan &mm = *(m_pMasterController->MemMan());
  if (m_pLogoTex) {
    mm.FreeTexture(m_pLogoTex);
    m_pLogoTex =NULL;
  }
  if (m_strLogoFilename != "")
    m_pLogoTex = mm.Load2DTextureFromFile(m_strLogoFilename);
  ScheduleCompleteRedraw();
}

void GLRenderer::ComposeSurfaceImage(const RenderRegion &renderRegion, int iStereoID) {
  GPUState localState = m_BaseState;
  localState.enableTex[0] = TEX_2D;
  localState.enableTex[1] = TEX_2D;
  localState.enableBlend = false;
  m_pContext->GetStateManager()->Apply(localState);

  m_pFBOIsoHit[iStereoID]->Read(0, 0);
  m_pFBOIsoHit[iStereoID]->Read(1, 1);

  FLOATVECTOR3 d = m_cDiffuse.xyz()*m_cDiffuse.w;

  if (m_bDoClearView) {
    m_pProgramCVCompose->Enable();
    m_pProgramCVCompose->Set("vLightDiffuse", d.x*m_vIsoColor.x,
                                          d.y*m_vIsoColor.y, d.z*m_vIsoColor.z);
    m_pProgramCVCompose->Set("vLightDiffuse2", d.x*m_vCVColor.x,
                                          d.y*m_vCVColor.y, d.z*m_vCVColor.z);
    m_pProgramCVCompose->Set("vCVParam",m_fCVSize,
                                          m_fCVContextScale, m_fCVBorderScale);

    FLOATVECTOR4 transPos = m_vCVPos * renderRegion.modelView[iStereoID];
    m_pProgramCVCompose->Set("vCVPickPos", transPos.x,
                                                        transPos.y,
                                                        transPos.z);
    m_pFBOCVHit[iStereoID]->Read(2, 0);
    m_pFBOCVHit[iStereoID]->Read(3, 1);
  } else {
    if (m_pDataset->GetComponentCount() == 1) {
      m_pProgramIsoCompose->Enable();
      m_pProgramIsoCompose->Set("vLightDiffuse", d.x*m_vIsoColor.x,
                                             d.y*m_vIsoColor.y, d.z*m_vIsoColor.z);
    } else {
      m_pProgramColorCompose->Enable();
    }
  }

  glBegin(GL_QUADS);
    glTexCoord2d(0,1);
    glVertex3d(-1.0,  1.0, -0.5);
    glTexCoord2d(1,1);
    glVertex3d( 1.0,  1.0, -0.5);
    glTexCoord2d(1,0);
    glVertex3d( 1.0, -1.0, -0.5);
    glTexCoord2d(0,0);
    glVertex3d(-1.0, -1.0, -0.5);
  glEnd();

  if (m_bDoClearView) {
    m_pFBOCVHit[iStereoID]->FinishRead(0);
    m_pFBOCVHit[iStereoID]->FinishRead(1);
  } 

  m_pFBOIsoHit[iStereoID]->FinishRead(1);
  m_pFBOIsoHit[iStereoID]->FinishRead(0);
}


void GLRenderer::CVFocusHasChanged(const RenderRegion &renderRegion) {
  // read back the 3D position from the framebuffer
  float vec[4];
  m_pFBOIsoHit[0]->ReadBackPixels(m_vCVMousePos.x, 
                                  m_vWinSize.y-m_vCVMousePos.y,
                                  1, 1, vec);

  // update m_vCVPos
  if (vec[3] != 0.0f) {
    m_vCVPos = FLOATVECTOR4(vec[0],vec[1],vec[2],1.0f) *
               renderRegion.modelView[0].inverse();
  } else {
    // if we do not pick a valid point move CV pos to "nirvana"
    m_vCVPos = FLOATVECTOR4(10000000.0f, 10000000.0f, 10000000.0f, 0.0f);
  }

  // now let the parent do its part
  AbstrRenderer::CVFocusHasChanged(renderRegion);
}

FLOATVECTOR3 GLRenderer::Pick(const UINTVECTOR2& mousePos) const
{
  if(m_eRenderMode != RM_ISOSURFACE) {
    throw std::runtime_error("Can only determine pick locations in "
                             "isosurface rendering mode.");
  }

  // readback the pos from the FB
  float vec[4];
  m_pFBOIsoHit[0]->ReadBackPixels(mousePos.x, m_vWinSize.y-mousePos.y,
                                  1, 1, vec);

  if(vec[3] == 0.0f) {
    throw std::range_error("No intersection.");
  }
  return FLOATVECTOR3(vec[0], vec[1], vec[2]);
}

void GLRenderer::SaveEmptyDepthBuffer() {
  if (m_aDepthStorage == NULL) return;
  for (size_t i = 0;i<m_vWinSize.area();i++) m_aDepthStorage[i] = 1.0f;
}

void GLRenderer::SaveDepthBuffer() {
  if (m_aDepthStorage == NULL) return;
  glReadPixels(0, 0, m_vWinSize.x, m_vWinSize.y, GL_DEPTH_COMPONENT, GL_FLOAT,
               m_aDepthStorage);
}

void GLRenderer::CreateDepthStorage() {
  DeleteDepthStorage();
  m_aDepthStorage = new float[m_vWinSize.area()];
}

void GLRenderer::UpdateLightParamsInShaders() {
  FLOATVECTOR3 a = m_cAmbient.xyz()*m_cAmbient.w;
  FLOATVECTOR3 d = m_cDiffuse.xyz()*m_cDiffuse.w;
  FLOATVECTOR3 s = m_cSpecular.xyz()*m_cSpecular.w;

  FLOATVECTOR3 aM = m_cAmbientM.xyz()*m_cAmbientM.w;
  FLOATVECTOR3 dM = m_cDiffuseM.xyz()*m_cDiffuseM.w;
  FLOATVECTOR3 sM = m_cSpecularM.xyz()*m_cSpecularM.w;

  FLOATVECTOR3 scale = 1.0f/FLOATVECTOR3(m_pDataset->GetScale());

  m_pProgram1DTrans[1]->Enable();
  m_pProgram1DTrans[1]->Set("vLightAmbient",a.x,a.y,a.z);
  m_pProgram1DTrans[1]->Set("vLightDiffuse",d.x,d.y,d.z);
  m_pProgram1DTrans[1]->Set("vLightSpecular",s.x,s.y,s.z);
  m_pProgram1DTrans[1]->Set("vLightDir",m_vLightDir.x,m_vLightDir.y,m_vLightDir.z);
  m_pProgram1DTrans[1]->Set("vDomainScale",scale.x,scale.y,scale.z);

  m_pProgram2DTrans[1]->Enable();
  m_pProgram2DTrans[1]->Set("vLightAmbient",a.x,a.y,a.z);
  m_pProgram2DTrans[1]->Set("vLightDiffuse",d.x,d.y,d.z);
  m_pProgram2DTrans[1]->Set("vLightSpecular",s.x,s.y,s.z);
  m_pProgram2DTrans[1]->Set("vLightDir",m_vLightDir.x,m_vLightDir.y,m_vLightDir.z);
  m_pProgram2DTrans[1]->Set("vDomainScale",scale.x,scale.y,scale.z);

  m_pProgramIsoCompose->Enable();
  m_pProgramIsoCompose->Set("vLightAmbient",a.x,a.y,a.z);
  m_pProgramIsoCompose->Set("vLightDiffuse",d.x,d.y,d.z);
  m_pProgramIsoCompose->Set("vLightSpecular",s.x,s.y,s.z);
  m_pProgramIsoCompose->Set("vLightDir",m_vLightDir.x,m_vLightDir.y,m_vLightDir.z);

  m_pProgramColorCompose->Enable();
  m_pProgramColorCompose->Set("vLightAmbient",a.x,a.y,a.z);
  m_pProgramColorCompose->Set("vLightDir",m_vLightDir.x,m_vLightDir.y,m_vLightDir.z);

  m_pProgramCVCompose->Enable();
  m_pProgramCVCompose->Set("vLightAmbient",a.x,a.y,a.z);
  m_pProgramCVCompose->Set("vLightDiffuse",d.x,d.y,d.z);
  m_pProgramCVCompose->Set("vLightSpecular",s.x,s.y,s.z);
  m_pProgramCVCompose->Set("vLightDir",m_vLightDir.x,m_vLightDir.y,m_vLightDir.z);

  m_pProgramMeshBTF->Enable();
  m_pProgramMeshBTF->Set("vLightAmbientM",aM.x,aM.y,aM.z);
  m_pProgramMeshBTF->Set("vLightDiffuseM",dM.x,dM.y,dM.z);
  m_pProgramMeshBTF->Set("vLightSpecularM",sM.x,sM.y,sM.z);
  m_pProgramMeshBTF->Set("vLightDir",m_vLightDir.x,m_vLightDir.y,m_vLightDir.z);

  m_pProgramMeshFTB->Enable();
  m_pProgramMeshFTB->Set("vLightAmbientM",aM.x,aM.y,aM.z);
  m_pProgramMeshFTB->Set("vLightDiffuseM",dM.x,dM.y,dM.z);
  m_pProgramMeshFTB->Set("vLightSpecularM",sM.x,sM.y,sM.z);
  m_pProgramMeshFTB->Set("vLightDir",m_vLightDir.x,m_vLightDir.y,m_vLightDir.z);

  m_pProgramIso->Enable();
  m_pProgramIso->Set("vDomainScale",scale.x,scale.y,scale.z);

  m_pProgramColor->Enable();
  m_pProgramColor->Set("vDomainScale",scale.x,scale.y,scale.z);
}

bool GLRenderer::IsVolumeResident(const BrickKey& key) const {
  // normally we use "real" 3D textures so implement this method
  // for 3D textures, it is overridden by 2D texture children
  return m_pMasterController->MemMan()->IsResident(m_pDataset, key,
    m_bUseOnlyPowerOfTwo, m_bDownSampleTo8Bits, m_bDisableBorder, false
  );
}

GLint GLRenderer::ComputeGLFilter() const {
  GLint iFilter = GL_LINEAR;
  switch (m_eInterpolant) {
    case Linear :          iFilter = GL_LINEAR; break;
    case NearestNeighbor : iFilter = GL_NEAREST; break;
  }    
  return iFilter;
}


bool GLRenderer::CropDataset(const std::string& strTempDir, bool bKeepOldData) {
  ExtendedPlane p = GetClipPlane();
  FLOATMATRIX4 trans = GetFirst3DRegion()->rotation * GetFirst3DRegion()->translation;

  // get rid of the viewing transformation in the plane
  p.Transform(trans.inverse(),false);

  if (!m_pDataset->Crop(p.Plane(),strTempDir,bKeepOldData)) return false;

  FileBackedDataset* fbd = dynamic_cast<FileBackedDataset*>(m_pDataset);
  if (NULL != fbd)
  {
    LoadDataset(fbd->Filename());	
  }
  
  return true;
}
