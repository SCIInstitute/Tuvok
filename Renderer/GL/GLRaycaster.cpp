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
  \file    GLRaycaster.cpp
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    August 2008
*/

#include "GLInclude.h"
#include "GLRaycaster.h"
#include "GLFBOTex.h"

#include <Controller/Controller.h>
#include "../GPUMemMan/GPUMemMan.h"
#include "../../Basics/Plane.h"
#include "GLSLProgram.h"
#include "GLTexture1D.h"
#include "GLTexture2D.h"
#include "Renderer/TFScaling.h"
#include "Basics/MathTools.h"

using namespace std;
using namespace tuvok;

GLRaycaster::GLRaycaster(MasterController* pMasterController, 
                         bool bUseOnlyPowerOfTwo, 
                         bool bDownSampleTo8Bits, 
                         bool bDisableBorder, 
                         bool bNoRCClipplanes) :
  GLRenderer(pMasterController,
             bUseOnlyPowerOfTwo, 
             bDownSampleTo8Bits, 
             bDisableBorder),
  m_pFBORayEntry(NULL),
  m_pProgramRenderFrontFaces(NULL),
  m_pProgramRenderFrontFacesNT(NULL),
  m_pProgramIso2(NULL),
  m_bNoRCClipplanes(bNoRCClipplanes)
{
  m_bSupportsMeshes = false; // for now we require full support 
                             // otherweise we rather say no
}

GLRaycaster::~GLRaycaster() {
}


void GLRaycaster::CleanupShaders() {
  GLRenderer::CleanupShaders();

  CleanupShader(&m_pProgramRenderFrontFaces);
  CleanupShader(&m_pProgramRenderFrontFacesNT);
  CleanupShader(&m_pProgramIso2);
}

void GLRaycaster::Cleanup() {
  GLRenderer::Cleanup();

  if (m_pFBORayEntry){
    m_pMasterController->MemMan()->FreeFBO(m_pFBORayEntry); 
    m_pFBORayEntry = NULL;
  }
}

void GLRaycaster::CreateOffscreenBuffers() {
  GLRenderer::CreateOffscreenBuffers();
  if (m_pFBORayEntry){m_pMasterController->MemMan()->FreeFBO(m_pFBORayEntry); m_pFBORayEntry = NULL;}
  if (m_vWinSize.area() > 0) {
    m_pFBORayEntry = m_pMasterController->MemMan()->GetFBO(GL_NEAREST, GL_NEAREST, GL_CLAMP, m_vWinSize.x, m_vWinSize.y, GL_RGBA16F_ARB, 2*4, m_pContext->GetShareGroupID(), false);
  }
}

bool GLRaycaster::LoadShaders() {
  if (!GLRenderer::LoadShaders()) {
    T_ERROR("Error in parent call -> aborting");
    return false;
  }

  const char* shaderNames[7];
  if (m_bNoRCClipplanes) {
   shaderNames[0] = "GLRaycaster-1D-FS-NC.glsl";
   shaderNames[1] = "GLRaycaster-1D-light-FS-NC.glsl";
   shaderNames[2] = "GLRaycaster-2D-FS-NC.glsl";
   shaderNames[3] = "GLRaycaster-2D-light-FS-NC.glsl";
   shaderNames[4] = "GLRaycaster-Color-FS-NC.glsl";
   shaderNames[5] = "GLRaycaster-ISO-CV-FS-NC.glsl";
   shaderNames[6] = "GLRaycaster-ISO-FS-NC.glsl";
  } else {
   shaderNames[0] = "GLRaycaster-1D-FS.glsl";
   shaderNames[1] = "GLRaycaster-1D-light-FS.glsl";
   shaderNames[2] = "GLRaycaster-2D-FS.glsl";
   shaderNames[3] = "GLRaycaster-2D-light-FS.glsl";
   shaderNames[4] = "GLRaycaster-Color-FS.glsl";
   shaderNames[5] = "GLRaycaster-ISO-CV-FS.glsl";
   shaderNames[6] = "GLRaycaster-ISO-FS.glsl";
  }

  const std::string tfqn = m_pDataset
                           ? (m_pDataset->GetComponentCount() == 3 ||
                              m_pDataset->GetComponentCount() == 4)
                              ? "VRender1D-Color.glsl"
                              : "VRender1D.glsl"
                           : "VRender1D.glsl";

  const std::string tfqnLit = m_pDataset
                           ? (m_pDataset->GetComponentCount() == 3 ||
                              m_pDataset->GetComponentCount() == 4)
                              ? "vr-col-tfqn-lit.glsl"
                              : "VRender1DLit.glsl"
                           : "VRender1DLit.glsl";
  
  if(!LoadAndVerifyShader(&m_pProgramRenderFrontFaces, m_vShaderSearchDirs,
                          "GLRaycaster-VS.glsl",
                          NULL,
                          "GLRaycaster-frontfaces-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramRenderFrontFacesNT, m_vShaderSearchDirs,
                          "GLRaycasterNoTransform-VS.glsl",
                          NULL,
                          "GLRaycaster-frontfaces-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgram1DTrans[0], m_vShaderSearchDirs,
                          "GLRaycaster-VS.glsl",
                          NULL,
                          "Compositing.glsl",   // UnderCompositing
                          "clip-plane.glsl",    // ClipByPlane
                          "Volume3D.glsl",      // SampleVolume
                          tfqn.c_str(),         // VRender1D
                          shaderNames[0],  NULL) ||
     !LoadAndVerifyShader(&m_pProgram1DTrans[1], m_vShaderSearchDirs,
                          "GLRaycaster-VS.glsl",
                          NULL,
                          "Compositing.glsl",   // UnderCompositing
                          "clip-plane.glsl",    // ClipByPlane
                          "Volume3D.glsl",      // SampleVolume
                          "lighting.glsl",      // Lighting
                          tfqnLit.c_str(),      // VRender1DLit
                          shaderNames[1], NULL) ||
     !LoadAndVerifyShader(&m_pProgram2DTrans[0], m_vShaderSearchDirs,
                          "GLRaycaster-VS.glsl",
                          NULL,
                          "Compositing.glsl",   // UnderCompositing
                          "clip-plane.glsl",    // ClipByPlane
                          "Volume3D.glsl",      // SampleVolume, ComputeGradient
                          shaderNames[2], NULL) ||
     !LoadAndVerifyShader(&m_pProgram2DTrans[1], m_vShaderSearchDirs,
                          "GLRaycaster-VS.glsl",
                          NULL,
                          "Compositing.glsl",   // UnderCompositing
                          "clip-plane.glsl",    // ClipByPlane
                          "Volume3D.glsl",      // SampleVolume, ComputeGradient
                          "lighting.glsl",      // Lighting
                          shaderNames[3], NULL) ||
     !LoadAndVerifyShader(&m_pProgramIso, m_vShaderSearchDirs,
                          "GLRaycaster-VS.glsl",
                          NULL,
                          "clip-plane.glsl",       // ClipByPlane
                          "RefineIsosurface.glsl", // RefineIsosurface
                          "Volume3D.glsl",        // SampleVolume, ComputeNormal
                          shaderNames[6], NULL) ||
     !LoadAndVerifyShader(&m_pProgramColor, m_vShaderSearchDirs,
                          "GLRaycaster-VS.glsl",
                          NULL,
                          "clip-plane.glsl",       // ClipByPlane
                          "RefineIsosurface.glsl", // RefineIsosurface
                          "Volume3D.glsl",        // SampleVolume, ComputeNormal
                          shaderNames[4], NULL) ||
     !LoadAndVerifyShader(&m_pProgramIso2, m_vShaderSearchDirs,
                          "GLRaycaster-VS.glsl",
                          NULL,
                          "clip-plane.glsl",       // ClipByPlane
                          "RefineIsosurface.glsl", // RefineIsosurface
                          "Volume3D.glsl",        // SampleVolume, ComputeNormal
                          shaderNames[5], NULL) ||
     !LoadAndVerifyShader(&m_pProgramHQMIPRot, m_vShaderSearchDirs,
                          "GLRaycaster-VS.glsl",
                          NULL,
                          "Volume3D.glsl",      // SampleVolume
                          "GLRaycaster-MIP-Rot-FS.glsl",
                          NULL))
  {
      Cleanup();
      T_ERROR("Error loading a shader.");
      return false;
  } else {
    m_pProgram1DTrans[0]->ConnectTextureID("texVolume",0);
    m_pProgram1DTrans[0]->ConnectTextureID("texTrans",1);
    m_pProgram1DTrans[0]->ConnectTextureID("texRayExitPos",2);

    m_pProgram1DTrans[1]->ConnectTextureID("texVolume",0);
    m_pProgram1DTrans[1]->ConnectTextureID("texTrans",1);
    m_pProgram1DTrans[1]->ConnectTextureID("texRayExitPos",2);

    m_pProgram2DTrans[0]->ConnectTextureID("texVolume",0);
    m_pProgram2DTrans[0]->ConnectTextureID("texTrans",1);
    m_pProgram2DTrans[0]->ConnectTextureID("texRayExitPos",2);

    m_pProgram2DTrans[1]->ConnectTextureID("texVolume",0);
    m_pProgram2DTrans[1]->ConnectTextureID("texTrans",1);
    m_pProgram2DTrans[1]->ConnectTextureID("texRayExitPos",2);

    FLOATVECTOR2 vParams = m_FrustumCullingLOD.GetDepthScaleParams();

    m_pProgramIso->ConnectTextureID("texVolume",0);
    m_pProgramIso->ConnectTextureID("texRayExitPos",2);
    m_pProgramIso->Set("vProjParam",vParams.x, vParams.y);
    m_pProgramIso->Set("iTileID", 1); // just to make sure it is never 0

    m_pProgramColor->ConnectTextureID("texVolume",0);
    m_pProgramColor->ConnectTextureID("texRayExitPos",2);
    m_pProgramColor->Set("vProjParam",vParams.x, vParams.y);

    m_pProgramHQMIPRot->ConnectTextureID("texVolume",0);
    m_pProgramHQMIPRot->ConnectTextureID("texRayExitPos",2);

    m_pProgramIso2->ConnectTextureID("texVolume",0);
    m_pProgramIso2->ConnectTextureID("texRayExitPos",2);
    m_pProgramIso2->ConnectTextureID("texLastHit",4);
    m_pProgramIso2->ConnectTextureID("texLastHitPos",5);

    UpdateLightParamsInShaders();

    /// We always clip against the plane in the shader, so initialize the plane
    /// to be way out in left field, ensuring nothing will be clipped.
    ClipPlaneToShader(ExtendedPlane::FarawayPlane(),0,true);
    return true;
  }

}

void GLRaycaster::SetBrickDepShaderVars(const RenderRegion3D&,
                                        const Brick& currentBrick,
                                        size_t iCurrentBrick) {
  FLOATVECTOR3 vVoxelSizeTexSpace;
  if (m_bUseOnlyPowerOfTwo)  {
    UINTVECTOR3 vP2VoxelCount(MathTools::NextPow2(currentBrick.vVoxelCount.x),
      MathTools::NextPow2(currentBrick.vVoxelCount.y),
      MathTools::NextPow2(currentBrick.vVoxelCount.z));

    vVoxelSizeTexSpace = 1.0f/FLOATVECTOR3(vP2VoxelCount);
  } else {
    vVoxelSizeTexSpace = 1.0f/FLOATVECTOR3(currentBrick.vVoxelCount);
  }

  float fSampleRateModifier = m_fSampleRateModifier /
    (this->decreaseSamplingRateNow ? m_fSampleDecFactor : 1.0f);

  float fRayStep = (currentBrick.vExtension*vVoxelSizeTexSpace * 0.5f * 1.0f/fSampleRateModifier).minVal();
  float fStepScale = 1.0f/fSampleRateModifier * (FLOATVECTOR3(m_pDataset->GetDomainSize())/FLOATVECTOR3(m_pDataset->GetDomainSize(static_cast<size_t>(m_iCurrentLOD)))).maxVal();

  switch (m_eRenderMode) {
    case RM_1DTRANS    :  {
      m_pProgram1DTrans[m_bUseLighting ? 1 : 0]->Set("fStepScale", fStepScale);
      m_pProgram1DTrans[m_bUseLighting ? 1 : 0]->Set("fRayStepsize", fRayStep);
      if (m_bUseLighting)
        m_pProgram1DTrans[1]->Set("vVoxelStepsize",
          vVoxelSizeTexSpace.x, vVoxelSizeTexSpace.y, vVoxelSizeTexSpace.z
        );
      break;
    }
    case RM_2DTRANS    :  {
      m_pProgram2DTrans[m_bUseLighting ? 1 : 0]->Set("fStepScale", fStepScale);
      m_pProgram2DTrans[m_bUseLighting ? 1 : 0]->Set("vVoxelStepsize",
        vVoxelSizeTexSpace.x, vVoxelSizeTexSpace.y, vVoxelSizeTexSpace.z
      );
      m_pProgram2DTrans[m_bUseLighting ? 1 : 0]->Set("fRayStepsize", fRayStep);
      break;
    }
    case RM_ISOSURFACE : {
      GLSLProgram* shader = (m_pDataset->GetComponentCount() == 1)
                              ? m_pProgramIso
                              : m_pProgramColor;
      if (m_bDoClearView) {
        m_pProgramIso2->Enable();
        m_pProgramIso2->Set("vVoxelStepsize",
          vVoxelSizeTexSpace.x, vVoxelSizeTexSpace.y, vVoxelSizeTexSpace.z
        );
        m_pProgramIso2->Set("fRayStepsize", fRayStep);
        m_pProgramIso2->Set("iTileID", int(iCurrentBrick));
        shader->Enable();
        shader->Set("iTileID", int(iCurrentBrick));
      }
      shader->Set("vVoxelStepsize",
        vVoxelSizeTexSpace.x, vVoxelSizeTexSpace.y, vVoxelSizeTexSpace.z
      );
      shader->Set("fRayStepsize", fRayStep);
      break;
    }
    case RM_INVALID:
      T_ERROR("Invalid rendermode set");
      break;
  }
}

void GLRaycaster::RenderBox(const RenderRegion& renderRegion,
                            const FLOATVECTOR3& vCenter,
                            const FLOATVECTOR3& vExtend,
                            const FLOATVECTOR3& vMinCoords,
                            const FLOATVECTOR3& vMaxCoords, bool bCullBack,
                            int iStereoID) const  {
  
  m_pContext->GetStateManager()->SetCullState(bCullBack ? CULL_FRONT : CULL_BACK);

  FLOATVECTOR3 vMinPoint, vMaxPoint;
  vMinPoint = (vCenter - vExtend/2.0);
  vMaxPoint = (vCenter + vExtend/2.0);

  // \todo compute this only once per brick
  FLOATMATRIX4 m = ComputeEyeToTextureMatrix(renderRegion,
                                             FLOATVECTOR3(vMaxPoint.x, vMaxPoint.y, vMaxPoint.z),
                                             FLOATVECTOR3(vMaxCoords.x, vMaxCoords.y, vMaxCoords.z),
                                             FLOATVECTOR3(vMinPoint.x, vMinPoint.y, vMinPoint.z),
                                             FLOATVECTOR3(vMinCoords.x, vMinCoords.y, vMinCoords.z),
                                             iStereoID);

  m.setTextureMatrix();

  glBegin(GL_QUADS);
    // BACK
    glVertex3f(vMaxPoint.x, vMinPoint.y, vMinPoint.z);
    glVertex3f(vMinPoint.x, vMinPoint.y, vMinPoint.z);
    glVertex3f(vMinPoint.x, vMaxPoint.y, vMinPoint.z);
    glVertex3f(vMaxPoint.x, vMaxPoint.y, vMinPoint.z);
    // FRONT
    glVertex3f(vMaxPoint.x, vMaxPoint.y, vMaxPoint.z);
    glVertex3f(vMinPoint.x, vMaxPoint.y, vMaxPoint.z);
    glVertex3f(vMinPoint.x, vMinPoint.y, vMaxPoint.z);
    glVertex3f(vMaxPoint.x, vMinPoint.y, vMaxPoint.z);
    // LEFT
    glVertex3f(vMinPoint.x, vMaxPoint.y, vMinPoint.z);
    glVertex3f(vMinPoint.x, vMinPoint.y, vMinPoint.z);
    glVertex3f(vMinPoint.x, vMinPoint.y, vMaxPoint.z);
    glVertex3f(vMinPoint.x, vMaxPoint.y, vMaxPoint.z);
    // RIGHT
    glVertex3f(vMaxPoint.x, vMaxPoint.y, vMaxPoint.z);
    glVertex3f(vMaxPoint.x, vMinPoint.y, vMaxPoint.z);
    glVertex3f(vMaxPoint.x, vMinPoint.y, vMinPoint.z);
    glVertex3f(vMaxPoint.x, vMaxPoint.y, vMinPoint.z);
    // BOTTOM
    glVertex3f(vMaxPoint.x, vMinPoint.y, vMaxPoint.z);
    glVertex3f(vMinPoint.x, vMinPoint.y, vMaxPoint.z);
    glVertex3f(vMinPoint.x, vMinPoint.y, vMinPoint.z);
    glVertex3f(vMaxPoint.x, vMinPoint.y, vMinPoint.z);
    // TOP
    glVertex3f(vMaxPoint.x, vMaxPoint.y, vMinPoint.z);
    glVertex3f(vMinPoint.x, vMaxPoint.y, vMinPoint.z);
    glVertex3f(vMinPoint.x, vMaxPoint.y, vMaxPoint.z);
    glVertex3f(vMaxPoint.x, vMaxPoint.y, vMaxPoint.z);
  glEnd();
}


/// Set the clip plane input variable in the shader.
void GLRaycaster::ClipPlaneToShader(const ExtendedPlane& clipPlane, int iStereoID, bool bForce) {
  if (m_bNoRCClipplanes) return;

  vector<GLSLProgram*> vCurrentShader;

  if (bForce) {
    vCurrentShader.push_back(m_pProgram1DTrans[0]);
    vCurrentShader.push_back(m_pProgram1DTrans[1]);
    vCurrentShader.push_back(m_pProgram2DTrans[0]);
    vCurrentShader.push_back(m_pProgram2DTrans[1]);
    vCurrentShader.push_back(m_pProgramIso);
    vCurrentShader.push_back(m_pProgramColor);
    vCurrentShader.push_back(m_pProgramIso2);
  } else {
    switch (m_eRenderMode) {
      case RM_1DTRANS    :  vCurrentShader.push_back(m_pProgram1DTrans[m_bUseLighting ? 1 : 0]);
                            break;
      case RM_2DTRANS    :  vCurrentShader.push_back(m_pProgram2DTrans[m_bUseLighting ? 1 : 0]);
                            break;
      case RM_ISOSURFACE :  if (m_pDataset->GetComponentCount() == 1)
                              vCurrentShader.push_back(m_pProgramIso);
                            else
                              vCurrentShader.push_back(m_pProgramColor);
                            if (m_bDoClearView) vCurrentShader.push_back(m_pProgramIso2);
                            break;
      default    :          T_ERROR("Invalid rendermode set");
                            break;
    }
  }

  if (bForce || m_bClipPlaneOn) {
    ExtendedPlane plane(clipPlane);

    plane.Transform(m_mView[iStereoID], false);
    for (size_t i = 0;i<vCurrentShader.size();i++) {
      vCurrentShader[i]->Enable();
      vCurrentShader[i]->Set("vClipPlane", plane.x(), plane.y(),
                                                        plane.z(), plane.d());
    }
  }
}

void GLRaycaster::Render3DPreLoop(const RenderRegion3D &) {

  // render nearplane into buffer
  if (m_iBricksRenderedInThisSubFrame == 0) {
    GPUState localState = m_BaseState;
    localState.enableBlend = false;
    localState.depthMask = false;
    localState.enableDepthTest  = false;
    m_pContext->GetStateManager()->Apply(localState);

    m_TargetBinder.Bind(m_pFBORayEntry);
  
    FLOATMATRIX4 mInvProj = m_mProjection[0].inverse();
    mInvProj.setProjection();
    m_pProgramRenderFrontFacesNT->Enable();

    glBegin(GL_QUADS);
      glVertex3d(-1.0,  1.0, -0.5);
      glVertex3d( 1.0,  1.0, -0.5);
      glVertex3d( 1.0, -1.0, -0.5);
      glVertex3d(-1.0, -1.0, -0.5);
    glEnd();

    m_TargetBinder.Unbind();
  }

  switch (m_eRenderMode) {
    case RM_1DTRANS    :  m_p1DTransTex->Bind(1);
                          break;
    case RM_2DTRANS    :  m_p2DTransTex->Bind(1);
                          break;
    case RM_ISOSURFACE :  break;
    default    :          T_ERROR("Invalid rendermode set");
                          break;
  }

}

void GLRaycaster::Render3DInLoop(const RenderRegion3D& renderRegion,
                                 size_t iCurrentBrick, int iStereoID) {


  m_pContext->GetStateManager()->Apply(m_BaseState);
                                                                    
  const Brick& b = (iStereoID == 0) ? m_vCurrentBrickList[iCurrentBrick] : m_vLeftEyeBrickList[iCurrentBrick];

  if (m_iBricksRenderedInThisSubFrame == 0 && m_eRenderMode == RM_ISOSURFACE){
    m_TargetBinder.Bind(m_pFBOIsoHit[iStereoID], 0, m_pFBOIsoHit[iStereoID], 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (m_bDoClearView) {
      m_TargetBinder.Bind(m_pFBOCVHit[iStereoID], 0, m_pFBOCVHit[iStereoID], 1);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
  }

  if (b.bIsEmpty) return;

  GPUState localState = m_BaseState;
  localState.enableBlend = false;
  localState.depthMask = false;
  localState.enableCullFace = true;
  m_pContext->GetStateManager()->Apply(localState);
  

  renderRegion.modelView[iStereoID].setModelview();
  m_mProjection[iStereoID].setProjection();

  if (m_bClipPlaneOn) ClipPlaneToShader(m_ClipPlane, iStereoID);

  // write frontfaces (ray entry points)
  m_TargetBinder.Bind(m_pFBORayEntry);
  m_pProgramRenderFrontFaces->Enable();
  RenderBox(renderRegion, b.vCenter, b.vExtension,
            b.vTexcoordsMin, b.vTexcoordsMax,
            false, iStereoID);

/*
  float* vec = new float[m_pFBORayEntry->Width()*m_pFBORayEntry->Height()*4];
  m_pFBORayEntry->ReadBackPixels(0,0,m_pFBORayEntry->Width(),m_pFBORayEntry->Height(), vec);
  delete [] vec;
*/

  if (m_eRenderMode == RM_ISOSURFACE) {
    m_pContext->GetStateManager()->SetDepthMask(true);

    m_TargetBinder.Bind(m_pFBOIsoHit[iStereoID], 0, m_pFBOIsoHit[iStereoID], 1);

    GLSLProgram* shader = (m_pDataset->GetComponentCount() == 1) ? m_pProgramIso : m_pProgramColor;
    shader->Enable();
    SetBrickDepShaderVars(renderRegion, b, iCurrentBrick);
    m_pFBORayEntry->Read(2);
    RenderBox(renderRegion, b.vCenter, b.vExtension,
              b.vTexcoordsMin, b.vTexcoordsMax,
              true, iStereoID);
    m_pFBORayEntry->FinishRead();

    if (m_bDoClearView) {
      m_TargetBinder.Bind(m_pFBOCVHit[iStereoID], 0, m_pFBOCVHit[iStereoID], 1);

      m_pProgramIso2->Enable();
      m_pFBORayEntry->Read(2);
      m_pFBOIsoHit[iStereoID]->Read(4, 0);
      m_pFBOIsoHit[iStereoID]->Read(5, 1);
      RenderBox(renderRegion, b.vCenter, b.vExtension,
                b.vTexcoordsMin, b.vTexcoordsMax,
                true, iStereoID);
      m_pFBOIsoHit[iStereoID]->FinishRead(1);
      m_pFBOIsoHit[iStereoID]->FinishRead(0);
      m_pFBORayEntry->FinishRead();
    }

  } else {
    m_TargetBinder.Bind(m_pFBO3DImageCurrent[iStereoID]);

    // do the raycasting
    switch (m_eRenderMode) {
      case RM_1DTRANS    :  m_pProgram1DTrans[m_bUseLighting ? 1 : 0]->Enable();
                            break;
      case RM_2DTRANS    :  m_pProgram2DTrans[m_bUseLighting ? 1 : 0]->Enable();
                            break;
      default            :  T_ERROR("Invalid rendermode set");
                            break;
    }

    m_pContext->GetStateManager()->SetEnableBlend(true);

    SetBrickDepShaderVars(renderRegion, b, iCurrentBrick);

    m_pFBORayEntry->Read(2);
    RenderBox(renderRegion, b.vCenter, b.vExtension,
              b.vTexcoordsMin, b.vTexcoordsMax,
              true, iStereoID);
    m_pFBORayEntry->FinishRead();
  }
  m_TargetBinder.Unbind();
}


void GLRaycaster::RenderHQMIPPreLoop(RenderRegion2D &region) {
  GLRenderer::RenderHQMIPPreLoop(region);

  m_pProgramHQMIPRot->Enable();
  m_pProgramHQMIPRot->Set("vScreensize",float(m_vWinSize.x), float(m_vWinSize.y));
  if (m_bOrthoView)
    region.modelView[0] = m_maMIPRotation;
  else
    region.modelView[0] = m_maMIPRotation * m_mView[0];

  region.modelView[0].setModelview();
}

void GLRaycaster::RenderHQMIPInLoop(const RenderRegion2D &renderRegion,
                                    const Brick& b) {

  GPUState localState = m_BaseState;
  localState.enableDepthTest = false;
  localState.depthMask = false;
  localState.enableCullFace = false;
  localState.enableBlend = false;
  m_pContext->GetStateManager()->Apply(localState);


  // write frontfaces (ray entry points)
  m_TargetBinder.Bind(m_pFBORayEntry);

  m_pProgramRenderFrontFaces->Enable();
  RenderBox(renderRegion, b.vCenter, b.vExtension, b.vTexcoordsMin,
            b.vTexcoordsMax, false, 0);

  m_TargetBinder.Bind(m_pFBO3DImageCurrent[1]);  // for MIP rendering "abuse" left-eye buffer for the itermediate results


  localState.enableBlend = true;
  localState.blendFuncSrc = BF_ONE;
  localState.blendEquation = BE_MAX;
  m_pContext->GetStateManager()->Apply(localState);

  m_pProgramHQMIPRot->Enable();

  FLOATVECTOR3 vVoxelSizeTexSpace = 1.0f/FLOATVECTOR3(b.vVoxelCount);
  float fRayStep = (b.vExtension*vVoxelSizeTexSpace * 0.5f * 1.0f/m_fSampleRateModifier).minVal();
  m_pProgramHQMIPRot->Set("fRayStepsize", fRayStep);

  m_pFBORayEntry->Read(2);
  RenderBox(renderRegion, b.vCenter, b.vExtension,b.vTexcoordsMin,
            b.vTexcoordsMax, true, 0);
  m_pFBORayEntry->FinishRead();
}

void GLRaycaster::StartFrame() {
  GLRenderer::StartFrame();

  FLOATVECTOR2 vfWinSize = FLOATVECTOR2(m_vWinSize);
  switch (m_eRenderMode) {
    case RM_1DTRANS    :  m_pProgram1DTrans[0]->Enable();
                          m_pProgram1DTrans[0]->Set("vScreensize",vfWinSize.x, vfWinSize.y);

                          m_pProgram1DTrans[1]->Enable();
                          m_pProgram1DTrans[1]->Set("vScreensize",vfWinSize.x, vfWinSize.y);
                          break;
    case RM_2DTRANS    :  m_pProgram2DTrans[0]->Enable();
                          m_pProgram2DTrans[0]->Set("vScreensize",vfWinSize.x, vfWinSize.y);

                          m_pProgram2DTrans[1]->Enable();
                          m_pProgram2DTrans[1]->Set("vScreensize",vfWinSize.x, vfWinSize.y);
                          break;
    case RM_ISOSURFACE :  {
                           FLOATVECTOR3 scale = 1.0f/FLOATVECTOR3(m_pDataset->GetScale());
                            if (m_bDoClearView) {
                              m_pProgramIso2->Enable();
                              m_pProgramIso2->Set("vScreensize",vfWinSize.x, vfWinSize.y);
                              m_pProgramIso2->Set("vDomainScale",scale.x,scale.y,scale.z);
                            }
                            GLSLProgram* shader = (m_pDataset->GetComponentCount() == 1) ? m_pProgramIso : m_pProgramColor;
                            shader->Enable();
                            shader->Set("vScreensize",vfWinSize.x, vfWinSize.y);
                            shader->Set("vDomainScale",scale.x,scale.y,scale.z);
                          }
                          break;
    default    :          T_ERROR("Invalid rendermode set");
                          break;
  }
}

void GLRaycaster::SetDataDepShaderVars() {
  GLRenderer::SetDataDepShaderVars();
  if (m_eRenderMode == RM_ISOSURFACE && m_bDoClearView) {
    m_pProgramIso2->Enable();
    m_pProgramIso2->Set("fIsoval", static_cast<float>
                                                (GetNormalizedCVIsovalue()));
  }

  if(m_eRenderMode == RM_1DTRANS && m_TFScalingMethod == SMETH_BIAS_AND_SCALE) {
    std::pair<float,float> bias_scale = scale_bias_and_scale(*m_pDataset);
    MESSAGE("setting TF bias (%5.3f) and scale (%5.3f)", bias_scale.first,
            bias_scale.second);
    m_pProgram1DTrans[m_bUseLighting ? 1 : 0]->Enable();
    m_pProgram1DTrans[m_bUseLighting ? 1 : 0]->Set("TFuncBias", bias_scale.first);
    m_pProgram1DTrans[m_bUseLighting ? 1 : 0]->Set("fTransScale", bias_scale.second);
  }

}


FLOATMATRIX4 GLRaycaster::ComputeEyeToTextureMatrix(const RenderRegion &renderRegion,
                                                    FLOATVECTOR3 p1, FLOATVECTOR3 t1,
                                                    FLOATVECTOR3 p2, FLOATVECTOR3 t2,
                                                    int iStereoID) const {
  FLOATMATRIX4 m;

  FLOATMATRIX4 mInvModelView = renderRegion.modelView[iStereoID].inverse();

  FLOATVECTOR3 vTrans1 = -p1;
  FLOATVECTOR3 vScale  = (t2-t1) / (p2-p1);
  FLOATVECTOR3 vTrans2 =  t1;

  FLOATMATRIX4 mTrans1;
  FLOATMATRIX4 mScale;
  FLOATMATRIX4 mTrans2;

  mTrans1.Translation(vTrans1.x,vTrans1.y,vTrans1.z);
  mScale.Scaling(vScale.x,vScale.y,vScale.z);
  mTrans2.Translation(vTrans2.x,vTrans2.y,vTrans2.z);

  m = mInvModelView * mTrans1 * mScale * mTrans2;

  return m;
}


void GLRaycaster::DisableClipPlane(RenderRegion* renderRegion) {
  AbstrRenderer::DisableClipPlane(renderRegion);
  /// We always clip against the plane in the shader, so initialize the plane
  /// to be way out in left field, ensuring nothing will be clipped.
  ClipPlaneToShader(ExtendedPlane::FarawayPlane(),0,true);
}
