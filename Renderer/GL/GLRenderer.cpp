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

#include <cstdarg>
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

using namespace std;
using namespace tuvok;

GLRenderer::GLRenderer(MasterController* pMasterController, bool bUseOnlyPowerOfTwo,
                       bool bDownSampleTo8Bits, bool bDisableBorder) :
  AbstrRenderer(pMasterController, bUseOnlyPowerOfTwo, bDownSampleTo8Bits,
                bDisableBorder),
  m_TargetBinder(pMasterController),
  m_p1DTransTex(NULL),
  m_p2DTransTex(NULL),
  m_p2DData(NULL),
  m_pFBO3DImageLast(NULL),
  m_pFBOResizeQuickBlit(NULL),
  m_bFirstDrawAfterResize(true),
  m_pLogoTex(NULL),
  m_pProgramIso(NULL),
  m_pProgramColor(NULL),
  m_pProgramHQMIPRot(NULL),
  m_pGLVolume(NULL),
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
  DeleteDepthStorage();
}

bool GLRenderer::Initialize() {
  if (!AbstrRenderer::Initialize()) {
    T_ERROR("Error in parent call -> aborting");
    return false;
  }

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

  return LoadShaders();
}

bool GLRenderer::LoadShaders() {
  if(!LoadAndVerifyShader(&m_pProgramTrans, m_vShaderSearchDirs,
                          "Transfer-VS.glsl", "Transfer-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgram1DTransSlice, m_vShaderSearchDirs,
                          "Transfer-VS.glsl", "1D-slice-FS.glsl", "Volume3D.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgram2DTransSlice, m_vShaderSearchDirs,
                          "Transfer-VS.glsl", "2D-slice-FS.glsl", "Volume3D.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramMIPSlice, m_vShaderSearchDirs,
                          "Transfer-VS.glsl", "MIP-slice-FS.glsl", "Volume3D.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgram1DTransSlice3D, m_vShaderSearchDirs,
                          "SlicesIn3D.glsl", "1D-slice-FS.glsl", "Volume3D.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgram2DTransSlice3D, m_vShaderSearchDirs,
                          "SlicesIn3D.glsl", "2D-slice-FS.glsl", "Volume3D.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramTransMIP, m_vShaderSearchDirs,
                          "Transfer-VS.glsl", "Transfer-MIP-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramIsoCompose, m_vShaderSearchDirs,
                          "Transfer-VS.glsl", "Compose-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramColorCompose, m_vShaderSearchDirs,
                          "Transfer-VS.glsl", "Compose-Color-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramCVCompose, m_vShaderSearchDirs,
                          "Transfer-VS.glsl", "Compose-CV-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramComposeAnaglyphs, m_vShaderSearchDirs,
                          "Transfer-VS.glsl", "Compose-Anaglyphs-FS.glsl",
                          NULL)                                              ||
     !LoadAndVerifyShader(&m_pProgramComposeScanlineStereo,
                          m_vShaderSearchDirs, "Transfer-VS.glsl",
                          "Compose-Scanline-FS.glsl", NULL))
  {
      T_ERROR("Error loading transfer function shaders.");
      return false;
  } else {
    m_pProgramTrans->Enable();
    m_pProgramTrans->SetUniformVector("texColor",0);
    m_pProgramTrans->SetUniformVector("texDepth",1);
    m_pProgramTrans->Disable();

    m_pProgram1DTransSlice->Enable();
    m_pProgram1DTransSlice->SetUniformVector("texVolume",0);
    m_pProgram1DTransSlice->SetUniformVector("texTrans1D",1);
    m_pProgram1DTransSlice->Disable();

    m_pProgram2DTransSlice->Enable();
    m_pProgram2DTransSlice->SetUniformVector("texVolume",0);
    m_pProgram2DTransSlice->SetUniformVector("texTrans2D",1);
    m_pProgram2DTransSlice->Disable();

    m_pProgram1DTransSlice3D->Enable();
    m_pProgram1DTransSlice3D->SetUniformVector("texVolume",0);
    m_pProgram1DTransSlice3D->SetUniformVector("texTrans1D",1);
    m_pProgram1DTransSlice3D->Disable();

    m_pProgram2DTransSlice3D->Enable();
    m_pProgram2DTransSlice3D->SetUniformVector("texVolume",0);
    m_pProgram2DTransSlice3D->SetUniformVector("texTrans2D",1);
    m_pProgram2DTransSlice3D->Disable();

    m_pProgramMIPSlice->Enable();
    m_pProgramMIPSlice->SetUniformVector("texVolume",0);
    m_pProgramMIPSlice->Disable();

    m_pProgramTransMIP->Enable();
    m_pProgramTransMIP->SetUniformVector("texLast",0);
    m_pProgramTransMIP->SetUniformVector("texTrans1D",1);
    m_pProgramTransMIP->Disable();

    FLOATVECTOR2 vParams = m_FrustumCullingLOD.GetDepthScaleParams();

    m_pProgramIsoCompose->Enable();
    m_pProgramIsoCompose->SetUniformVector("texRayHitPos",0);
    m_pProgramIsoCompose->SetUniformVector("texRayHitNormal",1);
    m_pProgramIsoCompose->SetUniformVector("vProjParam",vParams.x, vParams.y);
    m_pProgramIsoCompose->Disable();

    m_pProgramColorCompose->Enable();
    m_pProgramColorCompose->SetUniformVector("texRayHitPos",0);
    m_pProgramColorCompose->SetUniformVector("texRayHitNormal",1);
    m_pProgramColorCompose->SetUniformVector("vProjParam",vParams.x, vParams.y);
    m_pProgramColorCompose->Disable();

    m_pProgramCVCompose->Enable();
    m_pProgramCVCompose->SetUniformVector("texRayHitPos",0);
    m_pProgramCVCompose->SetUniformVector("texRayHitNormal",1);
    m_pProgramCVCompose->SetUniformVector("texRayHitPos2",2);
    m_pProgramCVCompose->SetUniformVector("texRayHitNormal2",3);
    m_pProgramCVCompose->SetUniformVector("vProjParam",vParams.x, vParams.y);
    m_pProgramCVCompose->Disable();

    m_pProgramComposeAnaglyphs->Enable();
    m_pProgramComposeAnaglyphs->SetUniformVector("texLeftEye",0);
    m_pProgramComposeAnaglyphs->SetUniformVector("texRightEye",1);
    m_pProgramComposeAnaglyphs->Disable();

    m_pProgramComposeScanlineStereo->Enable();
    m_pProgramComposeScanlineStereo->SetUniformVector("texLeftEye",0);
    m_pProgramComposeScanlineStereo->SetUniformVector("texRightEye",1);
    m_pProgramComposeScanlineStereo->Disable();    
  }
  return true;
}

void GLRenderer::CleanupShaders() {
  if (m_pProgramTrans)         {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgramTrans); m_pProgramTrans =NULL;}
  if (m_pProgram1DTransSlice)  {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgram1DTransSlice); m_pProgram1DTransSlice =NULL;}
  if (m_pProgram2DTransSlice)  {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgram2DTransSlice); m_pProgram2DTransSlice =NULL;}
  if (m_pProgram1DTransSlice3D){m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgram1DTransSlice3D); m_pProgram1DTransSlice3D =NULL;}
  if (m_pProgram2DTransSlice3D){m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgram2DTransSlice3D); m_pProgram2DTransSlice3D =NULL;}
  if (m_pProgramMIPSlice)      {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgramMIPSlice); m_pProgramMIPSlice =NULL;}
  if (m_pProgramHQMIPRot)      {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgramHQMIPRot); m_pProgramHQMIPRot =NULL;}
  if (m_pProgramTransMIP)      {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgramTransMIP); m_pProgramTransMIP =NULL;}
  if (m_pProgram1DTrans[0])    {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgram1DTrans[0]); m_pProgram1DTrans[0] =NULL;}
  if (m_pProgram1DTrans[1])    {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgram1DTrans[1]); m_pProgram1DTrans[1] =NULL;}
  if (m_pProgram2DTrans[0])    {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgram2DTrans[0]); m_pProgram2DTrans[0] =NULL;}
  if (m_pProgram2DTrans[1])    {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgram2DTrans[1]); m_pProgram2DTrans[1] =NULL;}
  if (m_pProgramIso)           {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgramIso); m_pProgramIso =NULL;}
  if (m_pProgramColor)         {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgramColor); m_pProgramColor =NULL;}
  if (m_pProgramIsoCompose)    {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgramIsoCompose); m_pProgramIsoCompose = NULL;}
  if (m_pProgramColorCompose)  {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgramColorCompose); m_pProgramColorCompose = NULL;}
  if (m_pProgramCVCompose)     {m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgramCVCompose); m_pProgramCVCompose = NULL;}
  if (m_pProgramComposeAnaglyphs){m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgramComposeAnaglyphs); m_pProgramComposeAnaglyphs = NULL;}
  if (m_pProgramComposeScanlineStereo){m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgramComposeScanlineStereo); m_pProgramComposeScanlineStereo = NULL;}
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
  m_bFirstDrawAfterResize = true;
}

void GLRenderer::ClearDepthBuffer() {
  glClear(GL_DEPTH_BUFFER_BIT);
}

void GLRenderer::ClearColorBuffer() {
  glDepthMask(GL_FALSE);
  if (m_bDoStereoRendering && m_eStereoMode == SM_RB) {
    // render anaglyphs against a black background only
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);
  } else {
    if (m_vBackgroundColors[0] == m_vBackgroundColors[1]) {
      glClearColor(m_vBackgroundColors[0].x,
                   m_vBackgroundColors[0].y,
                   m_vBackgroundColors[0].z, 0);
      glClear(GL_COLOR_BUFFER_BIT);
    } else {
      glDisable(GL_BLEND);
      DrawBackGradient();
    }
  }
  DrawLogo();
  glDepthMask(GL_TRUE);
}


void GLRenderer::StartFrame() {
  // clear the framebuffer (if requested)
  if (m_bClearFramebuffer) {
    ClearDepthBuffer();
    if (m_bConsiderPreviousDepthbuffer) SaveEmptyDepthBuffer();
  } else {
    if (m_bConsiderPreviousDepthbuffer) SaveDepthBuffer();
  }

  if (m_eRenderMode == RM_ISOSURFACE) {
    FLOATVECTOR2 vfWinSize = FLOATVECTOR2(m_vWinSize);
    if (m_bDoClearView) {
      m_pProgramCVCompose->Enable();
      m_pProgramCVCompose->SetUniformVector("vScreensize",vfWinSize.x, vfWinSize.y);
      m_pProgramCVCompose->Disable();
    } else {
      GLSLProgram* shader = (m_pDataset->GetComponentCount() == 1) ?
                            m_pProgramIsoCompose : m_pProgramColorCompose;

      shader->Enable();
      shader->SetUniformVector("vScreensize",vfWinSize.x, vfWinSize.y);
      shader->Disable();
    }
  }
}

bool GLRenderer::Paint() {
  if (!AbstrRenderer::Paint()) return false;

  vector<char> justCompletedRegions(renderRegions.size(), false);

  // if we are drawing for the first time after a resize we do want to 
  // start a full redraw loop but rather just blit the last valud image
  // onto the screen, this makes resizing more responsive
  if (m_bFirstDrawAfterResize) {
    CreateOffscreenBuffers();
    CreateDepthStorage();
    StartFrame();

    if (m_pFBOResizeQuickBlit) {
      m_pFBO3DImageLast->Write();
      glViewport(0,0,m_vWinSize.x,m_vWinSize.y);
      glDisable(GL_BLEND);

      m_pFBOResizeQuickBlit->Read(0);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      m_pFBOResizeQuickBlit->ReadDepth(1);

      glClearColor(1,0,0,1);
      glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
      glDisable(GL_DEPTH_TEST);
      glClearColor(0,0,0,0);

      m_pProgramTrans->Enable();
      FullscreenQuad();
      m_pProgramTrans->Disable();

      m_pFBOResizeQuickBlit->FinishRead();
      m_pFBOResizeQuickBlit->FinishDepthRead();
      m_pFBO3DImageLast->FinishWrite();

      m_pMasterController->MemMan()->FreeFBO(m_pFBOResizeQuickBlit);
      m_pFBOResizeQuickBlit = NULL;
    }
    m_bFirstDrawAfterResize = false;
  } else {
    StartFrame();

    for (size_t i=0; i < renderRegions.size(); ++i) {
      if (renderRegions[i]->redrawMask) {
        SetRenderTargetArea(*renderRegions[i],
                            renderRegions[i]->decreaseScreenResNow);
        if (renderRegions[i]->is3D()) {
          RenderRegion3D &region3D = *static_cast<RenderRegion3D*>
                                                 (renderRegions[i]);
          if (!region3D.isBlank && m_bPerformReCompose){
            Recompose3DView(region3D);
            justCompletedRegions[i] = true;
          } else {
            Plan3DFrame(region3D);

            // region3D.decreaseScreenResNow could have changed after calling
            // Plan3DFrame.
            SetRenderTargetArea(region3D, region3D.decreaseScreenResNow);

            // execute the frame
            float fMsecPassed = 0.0f;
            bool bJobDone = false;
            if (!Execute3DFrame(region3D, fMsecPassed, bJobDone) ) {
              T_ERROR("Could not execute the 3D frame, aborting.");
              return false;
            }
            justCompletedRegions[i] = static_cast<char>(bJobDone);
            region3D.msecPassedCurrentFrame += fMsecPassed;
          }
          // are we done rendering or do we need to render at higher quality?
          region3D.redrawMask =
            (m_vCurrentBrickList.size() > m_iBricksRenderedInThisSubFrame) ||
            (m_iCurrentLODOffset > m_iMinLODForCurrentView) ||
            region3D.decreaseScreenResNow;
        } else if (renderRegions[i]->is2D()) {  // in a 2D view mode
          RenderRegion2D& region2D = *static_cast<RenderRegion2D*>(renderRegions[i]);
          justCompletedRegions[i] = Render2DView(region2D);
          region2D.redrawMask = false;
        }
      } else {
        justCompletedRegions[i] = false;
      }
      renderRegions[i]->isBlank = false;
    }
  }
  EndFrame(justCompletedRegions);
  return true;
}

void GLRenderer::FullscreenQuad() {
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

void GLRenderer::FullscreenQuadRegions() {
  for (size_t i=0; i < renderRegions.size(); ++i) {
    FullscreenQuadRegion(renderRegions[i], renderRegions[i]->decreaseScreenResNow);
  }
}

void GLRenderer::FullscreenQuadRegion(const RenderRegion* region,
                                      bool decreaseScreenRes) {
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

  glDisable(GL_BLEND);

  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glDepthFunc(GL_LEQUAL);

  glViewport(0, 0, m_vWinSize.x, m_vWinSize.y);

  SetRenderTargetAreaScissor(*region);

  // always clear the depth buffer since we are transporting new data from the FBO
  glClear(GL_DEPTH_BUFFER_BIT);

  // Read newly completed image
  m_pFBO3DImageCurrent[0]->Read(0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                  (region->decreaseScreenResNow) ? GL_LINEAR : GL_NEAREST);
  m_pFBO3DImageCurrent[0]->ReadDepth(1);

  // Display this to the old buffer so we can reuse it in future frame.
  m_pProgramTrans->Enable();
  FullscreenQuadRegion(region, region->decreaseScreenResNow);
  m_pProgramTrans->Disable();

  m_TargetBinder.Unbind();
  m_pFBO3DImageCurrent[0]->FinishRead();
  m_pFBO3DImageCurrent[0]->FinishDepthRead();

  glDepthFunc(GL_LESS);
  glDisable(GL_SCISSOR_TEST);
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
  glDisable(GL_SCISSOR_TEST);

  // For a single region we can support stereo and we can also optimize the
  // code by swapping the buffers instead of copying data from one to the
  // other.
  if (renderRegions.size() == 1) {
    // if the image is complete
    if (justCompletedRegions[0]) {
      m_bOffscreenIsLowRes = renderRegions[0]->decreaseScreenResNow;

      // in stereo compose both images into one, in mono mode simply swap the
      // pointers
      if (m_bDoStereoRendering) {
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
          case SM_RB : m_pProgramComposeAnaglyphs->Enable(); break;
          default    : {
                        m_pProgramComposeScanlineStereo->Enable(); 
                        FLOATVECTOR2 vfWinSize = FLOATVECTOR2(m_vWinSize);
                        m_pProgramComposeScanlineStereo->SetUniformVector("vScreensize",vfWinSize.x, vfWinSize.y);
                        break;
                       }
        }

        glDisable(GL_DEPTH_TEST);
        FullscreenQuadRegions();
        glEnable(GL_DEPTH_TEST);

        switch (m_eStereoMode) {
          case SM_RB : m_pProgramComposeAnaglyphs->Disable(); break;
          default    : m_pProgramComposeScanlineStereo->Disable(); break;
        }

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
  glEnable( GL_SCISSOR_TEST );
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
  // get the 3D texture from the memory manager
  m_pGLVolume = m_pMasterController->MemMan()->GetVolume(m_pDataset, bkey,
                                                         m_bUseOnlyPowerOfTwo,
                                                         m_bDownSampleTo8Bits,
                                                         m_bDisableBorder,
                                                         false,
                                                         iIntraFrameCounter,
                                                         m_iFrameCounter);
  if(m_pGLVolume) {
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
  if (!renderRegion.GetUseMIP() || !m_bCaptureMode)  {
    if (!renderRegion.GetUseMIP()) {
      switch (m_eRenderMode) {
        case RM_2DTRANS    :  m_p2DTransTex->Bind(1);
                              m_pProgram2DTransSlice->Enable();
                              break;
        default            :  m_p1DTransTex->Bind(1);
                              m_pProgram1DTransSlice->Enable();
                              break;
      }
      glDisable(GL_BLEND);
    } else {
      m_pProgramMIPSlice->Enable();
      glBlendFunc(GL_ONE, GL_ONE);
      glBlendEquation(GL_MAX);
      glEnable(GL_BLEND);
    }

    glDisable(GL_DEPTH_TEST);

    UINT64 iCurrentLOD = 0;
    UINTVECTOR3 vVoxelCount(1,1,1); // make sure we do not dive by zero later
                                    // if no single-brick LOD exists

    // For now to make things simpler for the slice renderer we use the LOD
    // level with just one brick
    for (UINT64 i = 0;i<m_pDataset->GetLODLevelCount();i++) {
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
    SetRenderTargetAreaScissor(renderRegion);

    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);

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

    glEnable(GL_DEPTH_TEST);
    if (!renderRegion.GetUseMIP()) {
      switch (m_eRenderMode) {
        case RM_2DTRANS    :  m_pProgram2DTransSlice->Disable(); break;
        default            :  m_pProgram1DTransSlice->Disable(); break;
      }
    } else m_pProgramMIPSlice->Disable();
  } else {
    if (m_bOrthoView) {
      FLOATMATRIX4 maOrtho;
      UINT64VECTOR3 vDomainSize = m_pDataset->GetDomainSize();
      DOUBLEVECTOR2 vWinAspectRatio = 1.0 / DOUBLEVECTOR2(m_vWinSize);
      vWinAspectRatio = vWinAspectRatio / vWinAspectRatio.maxVal();
      float fRoot2Scale = (vWinAspectRatio.x < vWinAspectRatio.y) ?
                          max(1.0, 1.414213f * vWinAspectRatio.x/vWinAspectRatio.y) :
                          1.414213f;

      maOrtho.Ortho(-0.5*fRoot2Scale/vWinAspectRatio.x, +0.5*fRoot2Scale/vWinAspectRatio.x,
                    -0.5*fRoot2Scale/vWinAspectRatio.y, +0.5*fRoot2Scale/vWinAspectRatio.y,
                    -100.0, 100.0);
      maOrtho.setProjection();
    }

    PlanHQMIPFrame(renderRegion);
    glClearColor(0,0,0,0);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    RenderHQMIPPreLoop(renderRegion);

    for (size_t iBrickIndex = 0;iBrickIndex<m_vCurrentBrickList.size();iBrickIndex++) {
      MESSAGE("Brick %u of %u", static_cast<unsigned>(iBrickIndex+1),
              static_cast<unsigned>(m_vCurrentBrickList.size()));

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
    glBlendEquation(GL_FUNC_ADD);
    glDisable( GL_BLEND );

    m_TargetBinder.Bind(m_pFBO3DImageCurrent[0]);

    SetRenderTargetArea(UINTVECTOR2(0,0), m_vWinSize, false);
    SetRenderTargetAreaScissor(renderRegion);

    m_pFBO3DImageCurrent[1]->Read(0);
    m_p1DTransTex->Bind(1);
    m_pProgramTransMIP->Enable();
    glDisable(GL_DEPTH_TEST);
    FullscreenQuad();
    glDisable( GL_SCISSOR_TEST );
    m_pFBO3DImageCurrent[1]->FinishRead(0);

    m_pProgramTransMIP->Disable();
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
  m_maMIPRotation = matRotDir * matFlipX * matFlipY * m_maMIPRotation;
}

void GLRenderer::RenderBBox(const FLOATVECTOR4 vColor, bool bEpsilonOffset) {
  UINT64VECTOR3 vDomainSize = m_pDataset->GetDomainSize();
  FLOATVECTOR3 vScale = FLOATVECTOR3(m_pDataset->GetScale());
  FLOATVECTOR3 vExtend = FLOATVECTOR3(vDomainSize) * vScale;
  vExtend /= vExtend.maxVal();

  FLOATVECTOR3 vCenter(0,0,0);
  RenderBBox(vColor, bEpsilonOffset, vCenter, vExtend);
}

void GLRenderer::RenderBBox(const FLOATVECTOR4 vColor, bool bEpsilonOffset,
                            const FLOATVECTOR3& vCenter, const FLOATVECTOR3& vExtend) {
  FLOATVECTOR3 vMinPoint, vMaxPoint;

  // increase the size of the bbox by epsilon = 0.003 to avoid the data
  // z-fighting with the bbox when requested
  FLOATVECTOR3 vEExtend(vExtend+(bEpsilonOffset ? 0.003f : 0));

  vMinPoint = (vCenter - vEExtend/2.0);
  vMaxPoint = (vCenter + vEExtend/2.0);

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
  SetRenderTargetAreaScissor(renderRegion);

  glClearColor(0,0,0,0);

  m_TargetBinder.Bind(m_pFBO3DImageCurrent[0]);

  if (m_bConsiderPreviousDepthbuffer && m_aDepthStorage) {
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRasterPos2f(-1.0,-1.0);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDrawPixels(m_vWinSize.x, m_vWinSize.y, GL_DEPTH_COMPONENT, GL_FLOAT, m_aDepthStorage);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  } else {
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
  }

  if (m_bDoStereoRendering) {
    m_TargetBinder.Bind(m_pFBO3DImageCurrent[1]);

    if (m_bConsiderPreviousDepthbuffer && m_aDepthStorage) {
      glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
      glRasterPos2f(-1.0,-1.0);
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
      glDrawPixels(m_vWinSize.x, m_vWinSize.y, GL_DEPTH_COMPONENT, GL_FLOAT, m_aDepthStorage);
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    } else {
      glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    }
  }

  m_TargetBinder.Unbind();

  // since we do not clear anymore in this subframe we do not need the scissor
  // test, maybe disabling it saves performance
  glDisable( GL_SCISSOR_TEST );
}

void GLRenderer::RenderCoordArrows(const RenderRegion& renderRegion) {
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_LIGHT0);
  glEnable(GL_LIGHTING);

  glCullFace(GL_BACK);
  glEnable(GL_CULL_FACE);

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

  glDisable(GL_LIGHTING);
  glDisable(GL_COLOR_MATERIAL);
  glDisable(GL_CULL_FACE);
}

/// Actions to perform every subframe (rendering of a complete LOD level).
void GLRenderer::PreSubframe(RenderRegion& renderRegion)
{
  NewFrameClear(renderRegion);

  // Render the coordinate cross (three arrows in upper right corner)
  if (m_bRenderCoordArrows) {
    m_TargetBinder.Bind(m_pFBO3DImageCurrent[0]);
    RenderCoordArrows(renderRegion);

    if (m_bDoStereoRendering) {
      m_TargetBinder.Bind(m_pFBO3DImageCurrent[1]);
      RenderCoordArrows(renderRegion);
    }
    m_TargetBinder.Unbind();
  }

  // write the bounding boxes into the depth buffer (+ colorbuffer for
  // isosurfacing).
  m_TargetBinder.Bind(m_pFBO3DImageCurrent[0]);
  m_mProjection[0].setProjection();
  renderRegion.modelView[0].setModelview();
  BBoxPreRender();
  PlaneIn3DPreRender();
  if (m_bDoStereoRendering) {
    m_TargetBinder.Bind(m_pFBO3DImageCurrent[1]);
    m_mProjection[1].setProjection();
    renderRegion.modelView[1].setModelview();
    BBoxPreRender();
    PlaneIn3DPreRender();
  }
  m_TargetBinder.Unbind();
}

/// Actions which should be performed when we declare a subframe complete.
void GLRenderer::PostSubframe(RenderRegion& renderRegion)
{
  // render the bounding boxes and clip plane; these are essentially no
  // ops if they aren't enabled.
  m_TargetBinder.Bind(m_pFBO3DImageCurrent[0]);
  m_mProjection[0].setProjection();
  renderRegion.modelView[0].setModelview();
  BBoxPostRender();
  PlaneIn3DPostRender();
  RenderClipPlane(0);
  if (m_bDoStereoRendering) {
    m_TargetBinder.Bind(m_pFBO3DImageCurrent[1]);
    m_mProjection[1].setProjection();
    renderRegion.modelView[1].setModelview();
    BBoxPostRender();
    PlaneIn3DPostRender();
    RenderClipPlane(1);
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
            renderRegion.msecPassedCurrentFrame + fMsecPassed,
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
  glViewport(0,0,m_vWinSize.x,m_vWinSize.y);

  if (m_bClearFramebuffer)
    ClearColorBuffer();

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  m_pFBO3DImageLast->Read(0);

  // when we have more than 1 region the buffer already contains the normal
  // sized region so there's no need to resize again.
  //
  // Note: We check m_bOffscreenIsLowRes instead of the
  // RenderRegion::decreaseScreenResNow because the low res image we are trying
  // to display might have been rendered a while ago and now the render region
  // has decreaseScreenResNow set to false while it's in the midst of trying to
  // render a new image.
  bool decreaseRes = renderRegions.size() == 1 && m_bOffscreenIsLowRes;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                  decreaseRes ? GL_LINEAR : GL_NEAREST);

  m_pFBO3DImageLast->ReadDepth(1);

  // always clear the depth buffer since we are transporting new data from the FBO
  glClear(GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glDepthFunc(GL_LEQUAL);

  m_pProgramTrans->Enable();

  if (decreaseRes)
    FullscreenQuadRegion(renderRegions[0], decreaseRes);
  else
    FullscreenQuad();

  m_pProgramTrans->Disable();

  m_pFBO3DImageLast->FinishRead();
  m_pFBO3DImageLast->FinishDepthRead();

  glDepthFunc(GL_LESS);
}


void GLRenderer::DrawLogo() {
  if (m_pLogoTex == NULL) return;

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(-0.5, +0.5, -0.5, +0.5, 0.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  m_pLogoTex->Bind();
  glDisable(GL_TEXTURE_3D);
  glEnable(GL_TEXTURE_2D);

  glActiveTextureARB(GL_TEXTURE0_ARB);
  glMatrixMode(GL_TEXTURE);
  glLoadIdentity();

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

  glDisable(GL_TEXTURE_2D);

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
}

void GLRenderer::DrawBackGradient() {
  glDisable(GL_DEPTH_TEST);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(-1.0, +1.0, +1.0, -1.0, 0.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glDisable(GL_TEXTURE_3D);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_CULL_FACE);

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

  glEnable(GL_DEPTH_TEST);
}

void GLRenderer::Cleanup() {
  GPUMemMan& mm = *(m_pMasterController->MemMan());

  if (m_pFBO3DImageLast) {
    mm.FreeFBO(m_pFBO3DImageLast);
    m_pFBO3DImageLast =NULL;
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
                                                        GL_RGBA16F_ARB, 2*4,
                                                        true);
                        }
                        m_pFBO3DImageCurrent[i] = mm.GetFBO(GL_NEAREST,
                                                            GL_NEAREST,
                                                            GL_CLAMP,
                                                            m_vWinSize.x,
                                                            m_vWinSize.y,
                                                            GL_RGBA16F_ARB,
                                                            2*4, true);
                        break;

        case BP_32BIT : if (i==0) {
                          m_pFBO3DImageLast = mm.GetFBO(GL_NEAREST, GL_NEAREST,
                                                        GL_CLAMP, m_vWinSize.x,
                                                        m_vWinSize.y,
                                                        GL_RGBA32F_ARB, 4*4,
                                                        true);
                        }
                        m_pFBO3DImageCurrent[i] = mm.GetFBO(GL_NEAREST,
                                                            GL_NEAREST,
                                                            GL_CLAMP,
                                                            m_vWinSize.x,
                                                            m_vWinSize.y,
                                                            GL_RGBA32F_ARB,
                                                            4*4, true);
                        break;

        default       : MESSAGE("Invalid Blending Precision");
                        if (i==0) m_pFBO3DImageLast = NULL;
                        m_pFBO3DImageCurrent[i] = NULL;
                        break;
      }
      m_pFBOIsoHit[i]   = mm.GetFBO(GL_NEAREST, GL_NEAREST, GL_CLAMP,
                                    m_vWinSize.x, m_vWinSize.y, GL_RGBA32F_ARB,
                                    4*4, true, 2);

      m_pFBOCVHit[i]    = mm.GetFBO(GL_NEAREST, GL_NEAREST, GL_CLAMP,
                                    m_vWinSize.x, m_vWinSize.y, GL_RGBA16F_ARB,
                                    2*4, true, 2);
    }
  }
}

void GLRenderer::SetBrickDepShaderVarsSlice(const UINTVECTOR3& vVoxelCount) {
  if (m_eRenderMode == RM_2DTRANS) {
    FLOATVECTOR3 vStep = 1.0f/FLOATVECTOR3(vVoxelCount);
    m_pProgram2DTransSlice->SetUniformVector("vVoxelStepsize",
                                             vStep.x, vStep.y, vStep.z);
  }
}

// If we're downsampling the data, no scaling is needed, but otherwise we need
// to scale the TF in the same manner that we've scaled the data.
float GLRenderer::CalculateScaling()
{
  size_t iMaxValue     = size_t(MaxValue());
  UINT32 iMaxRange     = UINT32(1<<m_pDataset->GetBitWidth());
  return (m_pDataset->GetBitWidth() != 8 && m_bDownSampleTo8Bits) ?
    1.0f : float(iMaxRange)/float(iMaxValue);
}

void GLRenderer::SetDataDepShaderVars() {
  MESSAGE("Setting up vars");

  // if m_bDownSampleTo8Bits is enabled the full range from 0..255 -> 0..1 is used
  float fScale         = CalculateScaling();
  float fGradientScale = (m_pDataset->MaxGradientMagnitude() == 0) ?
                          1.0f : 1.0f/m_pDataset->MaxGradientMagnitude();

  MESSAGE("Transfer function scaling factor: %5.3f", fScale);
  MESSAGE("Gradient scaling factor: %5.3f", fGradientScale);

  m_pProgramTransMIP->Enable();
  m_pProgramTransMIP->SetUniformVector("fTransScale",fScale);
  m_pProgramTransMIP->Disable();

  switch (m_eRenderMode) {
    case RM_1DTRANS: {
      m_pProgram1DTransSlice->Enable();
      m_pProgram1DTransSlice->SetUniformVector("fTransScale",fScale);
      m_pProgram1DTransSlice->Disable();

      m_pProgram1DTransSlice3D->Enable();
      m_pProgram1DTransSlice3D->SetUniformVector("fTransScale",fScale);
      m_pProgram1DTransSlice3D->Disable();

      m_pProgram1DTrans[m_bUseLighting ? 1 : 0]->Enable();
      m_pProgram1DTrans[m_bUseLighting ? 1 : 0]->SetUniformVector("fTransScale",fScale);
      m_pProgram1DTrans[m_bUseLighting ? 1 : 0]->Disable();
      break;
    }
    case RM_2DTRANS: {
      m_pProgram2DTransSlice->Enable();
      m_pProgram2DTransSlice->SetUniformVector("fTransScale",fScale);
      m_pProgram2DTransSlice->SetUniformVector("fGradientScale",fGradientScale);
      m_pProgram2DTransSlice->Disable();

      m_pProgram2DTransSlice3D->Enable();
      m_pProgram2DTransSlice3D->SetUniformVector("fTransScale",fScale);
      m_pProgram2DTransSlice3D->SetUniformVector("fGradientScale",fGradientScale);
      m_pProgram2DTransSlice3D->Disable();

      m_pProgram2DTrans[m_bUseLighting ? 1 : 0]->Enable();
      m_pProgram2DTrans[m_bUseLighting ? 1 : 0]->SetUniformVector("fTransScale",fScale);
      m_pProgram2DTrans[m_bUseLighting ? 1 : 0]->SetUniformVector("fGradientScale",fGradientScale);
      m_pProgram2DTrans[m_bUseLighting ? 1 : 0]->Disable();
      break;
    }
    case RM_ISOSURFACE: {
      // as we are rendering the 2 slices with the 1d transferfunction
      // in iso mode update that shader also
      m_pProgram1DTransSlice->Enable();
      m_pProgram1DTransSlice->SetUniformVector("fTransScale",fScale);
      m_pProgram1DTransSlice->Disable();

      m_pProgram1DTransSlice3D->Enable();
      m_pProgram1DTransSlice3D->SetUniformVector("fTransScale",fScale);
      m_pProgram1DTransSlice3D->Disable();

      GLSLProgram* shader = (m_pDataset->GetComponentCount() == 1) ?
        m_pProgramIso : m_pProgramColor;

      shader->Enable();
      shader->SetUniformVector("fIsoval", static_cast<float>
                                          (this->GetNormalizedIsovalue()));
      shader->Disable();
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
      MESSAGE("searching %s", testfn.c_str());
      if(SysTools::FileExists(testfn)) {
        return testfn;
      }
    }
    MESSAGE("Could not find '%s'", file.c_str());
    return "";
  }

  return file;
}

namespace {
  template <typename ForwIter>
  bool all_exist(ForwIter bgn, ForwIter end) {
    do {
      if(!SysTools::FileExists(*bgn)) {
        return false;
      }
      ++bgn;
    } while(bgn != end);
    return true;
  }
}


bool GLRenderer::LoadAndVerifyShader(GLSLProgram** program,
                                     const std::vector<std::string>& strDirs,
                                     const char* vertex, ...)
{
  // first build list of fragment shaders
  std::vector<std::string> frag;

  va_list args;
  va_start(args, vertex);
  {
    const char* fp;
    do {
      fp = va_arg(args, const char*);
      if(fp != NULL) {
        std::string shader = find_shader(std::string(fp), false);
        if(shader == "") {
          T_ERROR("Could not find shader '%s'!", fp);
          return false;
        }
        frag.push_back(shader);
      }
    } while(fp != NULL);
  }
  va_end(args);

  std::vector<std::string> fullVS(1);
  fullVS[0] = find_shader(vertex, false);

  // now iterate through all directories, looking for our shaders in them.
  for (size_t i = 0;i<strDirs.size();i++) {
    MESSAGE("Searching for shaders in %s ...", strDirs[i].c_str());

    std::vector<std::string> fullFS(frag.size());

    if(!SysTools::FileExists(fullVS[0])) {
      fullVS[0] = strDirs[i] + "/" + vertex;
      if(!SysTools::FileExists(fullVS[0])) {
        MESSAGE("%s doesn't exist, skipping this directory...",
                fullVS[0].c_str());
        continue;
      }
    }
    for(size_t j=0; j < fullFS.size(); ++j) {
      fullFS[j] = strDirs[i] + "/" + frag[j];
    }
    // if any of those files don't exist, skip this directory.
    if(!all_exist(fullFS.begin(), fullFS.end())) {
      MESSAGE("Not all fragment shaders present in %s, skipping...",
              strDirs[i].c_str());
    }

    if (LoadAndVerifyShader(program, fullVS, fullFS)) {
      return true;
    }
  }

  {
    std::ostringstream shaders;
    shaders << "Shaders [" << vertex << ", ";
    std::copy(frag.begin(), frag.end(),
              std::ostream_iterator<std::string>(shaders, ", "));
    shaders << "] not found!";
    T_ERROR("%s", shaders.str().c_str());
  }
  return false;
}

bool GLRenderer::LoadAndVerifyShader(GLSLProgram** program,
                                     std::vector<std::string> vert,
                                     std::vector<std::string> frag) const
{
  vert[0] = find_shader(std::string(vert[0]), false);
  if(vert[0] == "") {
    T_ERROR("Could not find vertex shader '%s'!", vert[0].c_str());
    return false;
  }
  for(std::vector<std::string>::iterator f = frag.begin(); f != frag.end();
      ++f) {
    *f = find_shader(*f, false);
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

void GLRenderer::BBoxPreRender() {
  // for rendering modes other than isosurface render the bbox in the first
  // pass once to init the depth buffer.  for isosurface rendering we can go
  // ahead and render the bbox directly as isosurfacing writes out correct
  // depth values
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  if (m_eRenderMode != RM_ISOSURFACE || m_bDoClearView ||
      m_bAvoidSeperateCompositing) {
    glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
    if (m_bRenderGlobalBBox)
      RenderBBox();
    if (m_bRenderLocalBBox) {
      for (UINT64 iCurrentBrick = 0;
           iCurrentBrick < m_vCurrentBrickList.size();
           iCurrentBrick++) {
        RenderBBox(FLOATVECTOR4(0,1,0,1), false,
                   m_vCurrentBrickList[iCurrentBrick].vCenter,
                   m_vCurrentBrickList[iCurrentBrick].vExtension);
      }
    }
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  } else {
    glDisable(GL_BLEND);
    if (m_bRenderGlobalBBox) RenderBBox();
    if (m_bRenderLocalBBox) {
      for (UINT64 iCurrentBrick = 0;
           iCurrentBrick < m_vCurrentBrickList.size();
           iCurrentBrick++) {
        RenderBBox(FLOATVECTOR4(0,1,0,1), false,
                   m_vCurrentBrickList[iCurrentBrick].vCenter,
                   m_vCurrentBrickList[iCurrentBrick].vExtension);
      }
    }
  }
}

// For volume rendering, we render the bounding box again after rendering the
// dataset.  This is because we want the box lines which are in front of the
// dataset to appear .. well, in front of the dataset.
void GLRenderer::BBoxPostRender() {
  // Not required for isosurfacing, since we use the depth buffer for
  // occluding/showing the bbox's outline.
  if (m_eRenderMode != RM_ISOSURFACE || m_bDoClearView || m_bAvoidSeperateCompositing) {
    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    if (m_bRenderGlobalBBox) RenderBBox();
    if (m_bRenderLocalBBox) {
      for (UINT64 iCurrentBrick = 0;iCurrentBrick<m_vCurrentBrickList.size();iCurrentBrick++) {
        RenderBBox(FLOATVECTOR4(0,1,0,1), false, m_vCurrentBrickList[iCurrentBrick].vCenter,
                   m_vCurrentBrickList[iCurrentBrick].vExtension);
      }
    }
    glDepthFunc(GL_LESS);
    glDisable(GL_BLEND);
  }
}

void GLRenderer::PlaneIn3DPreRender() {
  if (!m_bRenderPlanesIn3D) return;

  // for rendering modes other than isosurface render the planes in the first
  // pass once to init the depth buffer.  for isosurface rendering we can go
  // ahead and render the planes directly as isosurfacing writes out correct
  // depth values
  if (m_eRenderMode != RM_ISOSURFACE || m_bDoClearView ||
      m_bAvoidSeperateCompositing) {
    glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
    RenderPlanesIn3D(true);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  } else {
    glDisable(GL_BLEND);
    RenderPlanesIn3D(false);
  }
}

void GLRenderer::PlaneIn3DPostRender() {
  if (!m_bRenderPlanesIn3D) return;
  // Not required for isosurfacing, since we use the depth buffer for
  // occluding/showing the planes
  if (m_eRenderMode != RM_ISOSURFACE || m_bDoClearView || m_bAvoidSeperateCompositing) {
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    RenderPlanesIn3D(false);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
  }
}

void GLRenderer::RenderPlanesIn3D(bool bDepthPassOnly) {
  UINT64VECTOR3 vDomainSize = m_pDataset->GetDomainSize();
  FLOATVECTOR3 vScale = FLOATVECTOR3(m_pDataset->GetScale());
  FLOATVECTOR3 vExtend = FLOATVECTOR3(vDomainSize) * vScale;
  vExtend /= vExtend.maxVal();

  FLOATVECTOR3 vMinPoint = -vExtend/2.0, vMaxPoint = vExtend/2.0;

  glDisable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);

  glDepthFunc(GL_LEQUAL);
  glLineWidth(2);

  if (!bDepthPassOnly) glColor4f(1,1,1,1);
  for (size_t i=0; i < renderRegions.size(); ++i) {

    int k=0;
    switch (renderRegions[i]->windowMode) {
    case RenderRegion::WM_SAGITTAL: k=0; break;
    case RenderRegion::WM_AXIAL   : k=1; break;
    case RenderRegion::WM_CORONAL : k=2; break;
    default: continue;
    };

    const float sliceIndex = static_cast<float>(renderRegions[i]->GetSliceIndex()) / vDomainSize[k];
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
    default: continue; // Should not get here.
    };
    glEnd();
  }

  glLineWidth(1);

  /*

  /// \todo fix this: this code fills the clip planes in 3D
            view with the 3D texture sice, while this works
            fine with the SBVR it does not work at present
            with the ray-caster until the raycaster takes
            the depth buffer into account for transferfunction
            rendering (iso-surfaces work)

  GLTexture3D* t = NULL;

  if (!bDepthPassOnly) {
    switch (m_eRenderMode) {
      case RM_2DTRANS    :  m_p2DTransTex->Bind(1);
                            m_pProgram2DTransSlice3D->Enable();
                            break;
      default            :  m_p1DTransTex->Bind(1);
                            m_pProgram1DTransSlice3D->Enable();
                            break;
    }

    UINT64 iCurrentLOD = 0;
    UINTVECTOR3 vVoxelCount;
    for (UINT64 i = 0;i<m_pDataset->GetLODLevelCount();i++) {
      if (m_pDataset->GetBrickCount(i).volume() == 1) {
          iCurrentLOD = i;
          vVoxelCount = UINTVECTOR3(m_pDataset->GetDomainSize(i));
      }
    }
    // convert 3D variables to the more general ND scheme used in the memory manager, i.e. convert 3-vectors to stl vectors
    vector<UINT64> vLOD; vLOD.push_back(iCurrentLOD);
    vector<UINT64> vBrick;
    vBrick.push_back(0);vBrick.push_back(0);vBrick.push_back(0);

    // get the 3D texture from the memory manager
    t = m_pMasterController->MemMan()->GetVolume(m_pDataset, vLOD, vBrick, m_bUseOnlyPowerOfTwo, m_bDownSampleTo8Bits, m_bDisableBorder, 0, m_iFrameCounter);
    if(t) t->Bind(0);
  }

  glBegin(GL_QUADS);
    glTexCoord3f(vfSliceIndex.x,0,1);
    glVertex3f(vfPlanePos.x, vMinPoint.y, vMaxPoint.z);
    glTexCoord3f(vfSliceIndex.x,0,0);
    glVertex3f(vfPlanePos.x, vMinPoint.y, vMinPoint.z);
    glTexCoord3f(vfSliceIndex.x,1,0);
    glVertex3f(vfPlanePos.x, vMaxPoint.y, vMinPoint.z);
    glTexCoord3f(vfSliceIndex.x,1,1);
    glVertex3f(vfPlanePos.x, vMaxPoint.y, vMaxPoint.z);
  glEnd();
  glBegin(GL_QUADS);
    glTexCoord3f(1,vfSliceIndex.y,0);
    glVertex3f(vMaxPoint.x, vfPlanePos.y, vMinPoint.z);
    glTexCoord3f(0,vfSliceIndex.y,0);
    glVertex3f(vMinPoint.x, vfPlanePos.y, vMinPoint.z);
    glTexCoord3f(0,vfSliceIndex.y,1);
    glVertex3f(vMinPoint.x, vfPlanePos.y, vMaxPoint.z);
    glTexCoord3f(1,vfSliceIndex.y,1);
    glVertex3f(vMaxPoint.x, vfPlanePos.y, vMaxPoint.z);
  glEnd();
  glBegin(GL_QUADS);
    glTexCoord3f(1,0,vfSliceIndex.z);
    glVertex3f(vMaxPoint.x, vMinPoint.y, vfPlanePos.z);
    glTexCoord3f(0,0,vfSliceIndex.z);
    glVertex3f(vMinPoint.x, vMinPoint.y, vfPlanePos.z);
    glTexCoord3f(0,1,vfSliceIndex.z);
    glVertex3f(vMinPoint.x, vMaxPoint.y, vfPlanePos.z);
    glTexCoord3f(1,1,vfSliceIndex.z);
    glVertex3f(vMaxPoint.x, vMaxPoint.y, vfPlanePos.z);
  glEnd();

  if (!bDepthPassOnly) {
    switch (m_eRenderMode) {
      case RM_2DTRANS    :  m_pProgram2DTransSlice3D->Disable(); break;
      default            :  m_pProgram1DTransSlice3D->Disable(); break;
    }

    m_pMasterController->MemMan()->Release3DTexture(t);
  }
*/
  glDepthFunc(GL_LESS);
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
  FLOATVECTOR3 vTransformedCenter;

  ExtendedPlane transformed(m_ClipPlane);
  m_mView[iStereoID].setModelview();

  /* transformed.Quad will give us back a list of triangle vertices; the return
   * value gives us the order we should render those so that we don't mess up
   * front/back faces. */
  typedef std::vector<FLOATVECTOR3> TriList;
  TriList quad;
  bool ccw = transformed.Quad(m_vEye, quad);
  if(m_eRenderMode != RM_ISOSURFACE) {
    if(ccw) {
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
      vColorQuad *= vColorQuad.w;
      vColorBorder *= vColorBorder.w;
      glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);
    }
  } else {
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  }

  // Now render the plane.
  glEnable(GL_BLEND);
  glBegin(GL_TRIANGLES);
    glColor4f(vColorQuad.x, vColorQuad.y, vColorQuad.z, vColorQuad.w);
    for(size_t i=0; i < 6; i+=3) { // 2 tris: 6 points.
      glVertex3f(quad[i+0].x, quad[i+0].y, quad[i+0].z);
      glVertex3f(quad[i+1].x, quad[i+1].y, quad[i+1].z);
      glVertex3f(quad[i+2].x, quad[i+2].y, quad[i+2].z);
    }
  glEnd();

  glEnable(GL_LINE_SMOOTH);
  glLineWidth(4);
  glBegin(GL_LINES);
    glColor4f(vColorBorder.x, vColorBorder.y, vColorBorder.z, vColorBorder.w);
    for(size_t i = 6; i<14 ; i += 2) {
      glVertex3f(quad[i+0].x, quad[i+0].y, quad[i+0].z);
      glVertex3f(quad[i+1].x, quad[i+1].y, quad[i+1].z);
    }
  glEnd();
  glLineWidth(1);
  glDisable(GL_LINE_SMOOTH);

  glDisable(GL_BLEND);
}

bool GLRenderer::LoadDataset(const string& strFilename) {
  if (AbstrRenderer::LoadDataset(strFilename)) {
    if (m_pProgram1DTrans[0] != NULL) SetDataDepShaderVars();
    return true;
  } else return false;
}

void GLRenderer::Recompose3DView(RenderRegion3D& renderRegion) {
  MESSAGE("Recompositing...");
  NewFrameClear(renderRegion);

  m_TargetBinder.Bind(m_pFBO3DImageCurrent[0]);
  m_mProjection[0].setProjection();
  renderRegion.modelView[0].setModelview();
  BBoxPreRender();
  PlaneIn3DPreRender();
  Render3DPreLoop(renderRegion);
  Render3DPostLoop();
  ComposeSurfaceImage(renderRegion, 0);
  BBoxPostRender();
  PlaneIn3DPostRender();
  RenderClipPlane(0);

  if (m_bDoStereoRendering) {
    m_TargetBinder.Bind(m_pFBO3DImageCurrent[1]);
    m_mProjection[1].setProjection();
    renderRegion.modelView[1].setModelview();
    BBoxPreRender();
    PlaneIn3DPreRender();
    Render3DPreLoop(renderRegion);
    Render3DPostLoop();
    ComposeSurfaceImage(renderRegion, 1);
    BBoxPostRender();
    PlaneIn3DPostRender();
    RenderClipPlane(1);
  }
  m_TargetBinder.Unbind();
}

bool GLRenderer::Render3DView(RenderRegion3D& renderRegion, float& fMsecPassed) {
  Render3DPreLoop(renderRegion);

  // loop over all bricks in the current LOD level
  m_Timer.Start();
  UINT32 bricks_this_call = 0;
  fMsecPassed = 0;

  while (m_vCurrentBrickList.size() > m_iBricksRenderedInThisSubFrame &&
         fMsecPassed < m_iTimeSliceMSecs) {
    MESSAGE("  Brick %u of %u",
            static_cast<unsigned>(m_iBricksRenderedInThisSubFrame+1),
            static_cast<unsigned>(m_vCurrentBrickList.size()));

    const BrickKey& bkey = m_vCurrentBrickList[m_iBricksRenderedInThisSubFrame].kBrick;

    MESSAGE("  Requesting texture from MemMan");

    if(BindVolumeTex(bkey, m_iIntraFrameCounter++)) {
      MESSAGE("  Binding Texture");
    } else {
      T_ERROR("Cannot bind texture, GetVolume returned invalid volume");
      return false;
    }

    Render3DInLoop(renderRegion, m_iBricksRenderedInThisSubFrame,0);
    if (m_bDoStereoRendering) {
      if (m_vLeftEyeBrickList[m_iBricksRenderedInThisSubFrame].kBrick !=
          m_vCurrentBrickList[m_iBricksRenderedInThisSubFrame].kBrick) {
        const BrickKey& left_eye_key = m_vLeftEyeBrickList[m_iBricksRenderedInThisSubFrame].kBrick;

        UnbindVolumeTex();
        if(BindVolumeTex(left_eye_key, m_iIntraFrameCounter++)) {
          MESSAGE("  Binding Texture (left eye)");
        } else {
          T_ERROR("Cannot bind texture (left eye), GetVolume returned invalid volume");
          return false;
        }
      }

      Render3DInLoop(renderRegion, m_iBricksRenderedInThisSubFrame,1);
    }

    // release the 3D texture
    if (!UnbindVolumeTex()) {
      T_ERROR("Cannot unbind volume.");
      return false;
    }

    // count the bricks rendered
    m_iBricksRenderedInThisSubFrame++;

    if (!m_bCaptureMode) {
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
    fMsecPassed = m_Timer.Elapsed();

    ++bricks_this_call;
  }
  MESSAGE("Rendered %u bricks this call.", bricks_this_call);

  Render3DPostLoop();

  if (m_eRenderMode == RM_ISOSURFACE &&
      m_vCurrentBrickList.size() == m_iBricksRenderedInThisSubFrame) {
    m_TargetBinder.Bind(m_pFBO3DImageCurrent[0]);
    ComposeSurfaceImage(renderRegion, 0);
    if (m_bDoStereoRendering) {
      m_TargetBinder.Bind(m_pFBO3DImageCurrent[1]);
      ComposeSurfaceImage(renderRegion, 1);
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

void GLRenderer::ComposeSurfaceImage(RenderRegion &renderRegion, int iStereoID) {
  glEnable(GL_DEPTH_TEST);

  m_pFBOIsoHit[iStereoID]->Read(0, 0);
  m_pFBOIsoHit[iStereoID]->Read(1, 1);

  FLOATVECTOR3 d = m_cDiffuse.xyz()*m_cDiffuse.w;

  if (m_bDoClearView) {
    m_pProgramCVCompose->Enable();
    m_pProgramCVCompose->SetUniformVector("vLightDiffuse", d.x*m_vIsoColor.x,
                                          d.y*m_vIsoColor.y, d.z*m_vIsoColor.z);
    m_pProgramCVCompose->SetUniformVector("vLightDiffuse2", d.x*m_vCVColor.x,
                                          d.y*m_vCVColor.y, d.z*m_vCVColor.z);
    m_pProgramCVCompose->SetUniformVector("vCVParam",m_fCVSize,
                                          m_fCVContextScale, m_fCVBorderScale);

    FLOATVECTOR4 transPos = m_vCVPos * renderRegion.modelView[iStereoID];
    m_pProgramCVCompose->SetUniformVector("vCVPickPos", transPos.x,
                                                        transPos.y,
                                                        transPos.z);
    m_pFBOCVHit[iStereoID]->Read(2, 0);
    m_pFBOCVHit[iStereoID]->Read(3, 1);
    glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);
  } else {
    if (m_pDataset->GetComponentCount() == 1) {
      m_pProgramIsoCompose->Enable();
      m_pProgramIsoCompose->SetUniformVector("vLightDiffuse", d.x*m_vIsoColor.x,
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
    m_pProgramCVCompose->Disable();
  } else {
    if (m_pDataset->GetComponentCount() == 1)
      m_pProgramIsoCompose->Disable();
    else
      m_pProgramColorCompose->Disable();
  }

  m_pFBOIsoHit[iStereoID]->FinishRead(1);
  m_pFBOIsoHit[iStereoID]->FinishRead(0);
}


void GLRenderer::CVFocusHasChanged(RenderRegion &renderRegion) {
  // read back the 3D position from the framebuffer
  m_pFBOIsoHit[0]->Write(0,0,false);
  glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
  float vec[4];
  glReadPixels(m_vCVMousePos.x, m_vWinSize.y-m_vCVMousePos.y, 1, 1,
               GL_RGBA, GL_FLOAT, vec);
  glReadBuffer(GL_BACK);
  m_pFBOIsoHit[0]->FinishWrite(0);

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


void GLRenderer::UpdateColorsInShaders() {

    FLOATVECTOR3 a = m_cAmbient.xyz()*m_cAmbient.w;
    FLOATVECTOR3 d = m_cDiffuse.xyz()*m_cDiffuse.w;
    FLOATVECTOR3 s = m_cSpecular.xyz()*m_cSpecular.w;

    FLOATVECTOR3 scale = 1.0f/FLOATVECTOR3(m_pDataset->GetScale());

    m_pProgram1DTrans[1]->Enable();
    m_pProgram1DTrans[1]->SetUniformVector("vLightAmbient",a.x,a.y,a.z);
    m_pProgram1DTrans[1]->SetUniformVector("vLightDiffuse",d.x,d.y,d.z);
    m_pProgram1DTrans[1]->SetUniformVector("vLightSpecular",s.x,s.y,s.z);
    m_pProgram1DTrans[1]->SetUniformVector("vLightDir",m_vLightDir.x,m_vLightDir.y,m_vLightDir.z);
    m_pProgram1DTrans[1]->SetUniformVector("vDomainScale",scale.x,scale.y,scale.z);
    m_pProgram1DTrans[1]->Disable();

    m_pProgram2DTrans[1]->Enable();
    m_pProgram2DTrans[1]->SetUniformVector("vLightAmbient",a.x,a.y,a.z);
    m_pProgram2DTrans[1]->SetUniformVector("vLightDiffuse",d.x,d.y,d.z);
    m_pProgram2DTrans[1]->SetUniformVector("vLightSpecular",s.x,s.y,s.z);
    m_pProgram2DTrans[1]->SetUniformVector("vLightDir",m_vLightDir.x,m_vLightDir.y,m_vLightDir.z);
    m_pProgram2DTrans[1]->SetUniformVector("vDomainScale",scale.x,scale.y,scale.z);
    m_pProgram2DTrans[1]->Disable();

    m_pProgramIsoCompose->Enable();
    m_pProgramIsoCompose->SetUniformVector("vLightAmbient",a.x,a.y,a.z);
    m_pProgramIsoCompose->SetUniformVector("vLightDiffuse",d.x,d.y,d.z);
    m_pProgramIsoCompose->SetUniformVector("vLightSpecular",s.x,s.y,s.z);
    m_pProgramIsoCompose->SetUniformVector("vLightDir",m_vLightDir.x,m_vLightDir.y,m_vLightDir.z);
    m_pProgramIsoCompose->Disable();

    m_pProgramColorCompose->Enable();
    m_pProgramColorCompose->SetUniformVector("vLightAmbient",a.x,a.y,a.z);
//    m_pProgramColorCompose->SetUniformVector("vLightDiffuse",d.x,d.y,d.z);
//    m_pProgramColorCompose->SetUniformVector("vLightSpecular",s.x,s.y,s.z);
    m_pProgramColorCompose->SetUniformVector("vLightDir",m_vLightDir.x,m_vLightDir.y,m_vLightDir.z);
    m_pProgramColorCompose->Disable();

    m_pProgramCVCompose->Enable();
    m_pProgramCVCompose->SetUniformVector("vLightAmbient",a.x,a.y,a.z);
    m_pProgramCVCompose->SetUniformVector("vLightDiffuse",d.x,d.y,d.z);
    m_pProgramCVCompose->SetUniformVector("vLightSpecular",s.x,s.y,s.z);
    m_pProgramCVCompose->SetUniformVector("vLightDir",m_vLightDir.x,m_vLightDir.y,m_vLightDir.z);
    m_pProgramCVCompose->Disable();

    m_pProgramIso->Enable();
    m_pProgramIso->SetUniformVector("vDomainScale",scale.x,scale.y,scale.z);
    m_pProgramIso->Disable();

    m_pProgramColor->Enable();
    m_pProgramColor->SetUniformVector("vDomainScale",scale.x,scale.y,scale.z);
    m_pProgramColor->Disable();

}
