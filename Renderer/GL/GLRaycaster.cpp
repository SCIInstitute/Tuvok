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
#include "GLVBO.h"

#include <Controller/Controller.h>
#include "../GPUMemMan/GPUMemMan.h"
#include "../../Basics/Plane.h"
#include "GLSLProgram.h"
#include "GLTexture1D.h"
#include "GLTexture2D.h"
#include "Renderer/TFScaling.h"
#include "Basics/MathTools.h"
#include "Basics/Clipper.h"

using namespace std;
using namespace tuvok;

GLRaycaster::GLRaycaster(MasterController* pMasterController, 
                         bool bUseOnlyPowerOfTwo, 
                         bool bDownSampleTo8Bits, 
                         bool bDisableBorder) :
  GLGPURayTraverser(pMasterController,
             bUseOnlyPowerOfTwo, 
             bDownSampleTo8Bits, 
             bDisableBorder),
  m_pFBORayEntry(NULL),
  m_pProgramRenderFrontFaces(NULL),
  m_pProgramRenderFrontFacesNT(NULL),
  m_pProgramIso2(NULL)
{
  m_bSupportsMeshes = false; // for now we require full support 
                             // otherweise we rather say no
}

GLRaycaster::~GLRaycaster() {
}


void GLRaycaster::CleanupShaders() {
  GLGPURayTraverser::CleanupShaders();

  CleanupShader(&m_pProgramRenderFrontFaces);
  CleanupShader(&m_pProgramRenderFrontFacesNT);
  CleanupShader(&m_pProgramIso2);
}

void GLRaycaster::Cleanup() {
  GLGPURayTraverser::Cleanup();

  if (m_pFBORayEntry){
    m_pMasterController->MemMan()->FreeFBO(m_pFBORayEntry); 
    m_pFBORayEntry = NULL;
  }
}

void GLRaycaster::CreateOffscreenBuffers() {
  GLGPURayTraverser::CreateOffscreenBuffers();
  if (m_pFBORayEntry){m_pMasterController->MemMan()->FreeFBO(m_pFBORayEntry); m_pFBORayEntry = NULL;}
  if (m_vWinSize.area() > 0) {
    m_pFBORayEntry = m_pMasterController->MemMan()->GetFBO(GL_NEAREST, GL_NEAREST, GL_CLAMP, m_vWinSize.x, m_vWinSize.y, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, m_pContext->GetShareGroupID(), false);
  }
}

bool GLRaycaster::LoadShaders() {
  if (!GLGPURayTraverser::LoadShaders()) {
    T_ERROR("Error in parent call -> aborting");
    return false;
  }

  const wchar_t* shaderNames[7];
  shaderNames[0] = L"GLRaycaster-1D-FS.glsl";
  shaderNames[1] = L"GLRaycaster-1D-light-FS.glsl";
  shaderNames[2] = L"GLRaycaster-2D-FS.glsl";
  shaderNames[3] = L"GLRaycaster-2D-light-FS.glsl";
  shaderNames[4] = L"GLRaycaster-Color-FS.glsl";
  shaderNames[5] = L"GLRaycaster-ISO-CV-FS.glsl";
  shaderNames[6] = L"GLRaycaster-ISO-FS.glsl";

  std::wstring tfqn = m_pDataset
                     ? this->ColorData()
                        ? L"VRender1D-Color"
                        : L"VRender1D"
                     : L"VRender1D";

  const std::wstring tfqnLit = m_pDataset
                           ? this->ColorData()
                              ? L"VRender1DLit-Color.glsl"
                              : L"VRender1DLit.glsl"
                           : L"VRender1DLit.glsl";
  const std::wstring bias = tfqn + L"-BScale.glsl";
  tfqn += L".glsl";
  
  if(!LoadAndVerifyShader(&m_pProgramRenderFrontFaces, m_vShaderSearchDirs,
                          L"GLRaycaster-VS.glsl",
                          NULL,
                          L"GLRaycaster-frontfaces-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramRenderFrontFacesNT, m_vShaderSearchDirs,
                          L"GLRaycasterNoTransform-VS.glsl",
                          NULL,
                          L"GLRaycaster-frontfaces-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgram1DTrans[0], m_vShaderSearchDirs,
                          L"GLRaycaster-VS.glsl",
                          NULL,
                          L"Compositing.glsl",   // UnderCompositing
                          L"Volume3D.glsl",      // SampleVolume
                          tfqn.c_str(),         // VRender1D
                          bias.c_str(),
                          L"VRender1DProxy.glsl",
                          shaderNames[0],  NULL) ||
     !LoadAndVerifyShader(&m_pProgram1DTrans[1], m_vShaderSearchDirs,
                          L"GLRaycaster-VS.glsl",
                          NULL,
                          L"Compositing.glsl",   // UnderCompositing
                          L"Volume3D.glsl",      // SampleVolume
                          L"lighting.glsl",      // Lighting
                          tfqnLit.c_str(),      // VRender1DLit
                          shaderNames[1], NULL) ||
     !LoadAndVerifyShader(&m_pProgram2DTrans[0], m_vShaderSearchDirs,
                          L"GLRaycaster-VS.glsl",
                          NULL,
                          L"Compositing.glsl",   // UnderCompositing
                          L"Volume3D.glsl",      // SampleVolume, ComputeGradient
                          shaderNames[2], NULL) ||
     !LoadAndVerifyShader(&m_pProgram2DTrans[1], m_vShaderSearchDirs,
                          L"GLRaycaster-VS.glsl",
                          NULL,
                          L"Compositing.glsl",   // UnderCompositing
                          L"Volume3D.glsl",      // SampleVolume, ComputeGradient
                          L"lighting.glsl",      // Lighting
                          shaderNames[3], NULL) ||
     !LoadAndVerifyShader(&m_pProgramIso, m_vShaderSearchDirs,
                          L"GLRaycaster-VS.glsl",
                          NULL,
                          L"RefineIsosurface.glsl", // RefineIsosurface
                          L"Volume3D.glsl",        // SampleVolume, ComputeNormal
                          shaderNames[6], NULL) ||
     !LoadAndVerifyShader(&m_pProgramColor, m_vShaderSearchDirs,
                          L"GLRaycaster-VS.glsl",
                          NULL,
                          L"RefineIsosurface.glsl", // RefineIsosurface
                          L"Volume3D.glsl",        // SampleVolume, ComputeNormal
                          shaderNames[4], NULL) ||
     !LoadAndVerifyShader(&m_pProgramIso2, m_vShaderSearchDirs,
                          L"GLRaycaster-VS.glsl",
                          NULL,
                          L"RefineIsosurface.glsl", // RefineIsosurface
                          L"Volume3D.glsl",        // SampleVolume, ComputeNormal
                          shaderNames[5], NULL) ||
     !LoadAndVerifyShader(&m_pProgramHQMIPRot, m_vShaderSearchDirs,
                          L"GLRaycaster-VS.glsl",
                          NULL,
                          L"Volume3D.glsl",      // SampleVolume
                          L"GLRaycaster-MIP-Rot-FS.glsl",
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
      GLSLProgram* shader = this->ColorData() ? m_pProgramColor
                                              : m_pProgramIso;
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
                            EStereoID eStereoID) const {
  
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
                                             eStereoID);

  m.setTextureMatrix();

  std::vector<FLOATVECTOR3> posData;
  MaxMinBoxToVector(vMinPoint, vMaxPoint, posData);

  if ( m_bClipPlaneOn ) {
    // clip plane is normaly defined in world space, transform back to model space
    FLOATMATRIX4 inv = (renderRegion.rotation * renderRegion.translation).inverse();
    PLANE<float> transformed = m_ClipPlane.Plane() * inv;

    const FLOATVECTOR3 normal(transformed.xyz().normalized());
    const float d = transformed.d();

    Clipper::BoxPlane(posData, normal, d);
  }

  m_pBBoxVBO->ClearVertexData();
  m_pBBoxVBO->AddVertexData(posData);

  m_pBBoxVBO->Bind();
  m_pBBoxVBO->Draw(GL_TRIANGLES);
  m_pBBoxVBO->UnBind();
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
    
    m_pNearPlaneQuad->Bind();
    m_pNearPlaneQuad->Draw(GL_QUADS);
    m_pNearPlaneQuad->UnBind();

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
                                 size_t iCurrentBrick, EStereoID eStereoID) {


  m_pContext->GetStateManager()->Apply(m_BaseState);
                                                                    
  const Brick& b = (eStereoID == SI_LEFT_OR_MONO) ? m_vCurrentBrickList[iCurrentBrick] : m_vLeftEyeBrickList[iCurrentBrick];

  if (m_iBricksRenderedInThisSubFrame == 0 && m_eRenderMode == RM_ISOSURFACE){
    m_TargetBinder.Bind(m_pFBOIsoHit[size_t(eStereoID)], 0, m_pFBOIsoHit[size_t(eStereoID)], 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (m_bDoClearView) {
      m_TargetBinder.Bind(m_pFBOCVHit[size_t(eStereoID)], 0, m_pFBOCVHit[size_t(eStereoID)], 1);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
  }

  if (b.bIsEmpty) return;

  GPUState localState = m_BaseState;
  localState.enableBlend = false;
  localState.depthMask = false;
  localState.enableCullFace = true;
  m_pContext->GetStateManager()->Apply(localState);
  

  renderRegion.modelView[size_t(eStereoID)].setModelview();
  m_mProjection[size_t(eStereoID)].setProjection();

  // write frontfaces (ray entry points)
  m_TargetBinder.Bind(m_pFBORayEntry);
  m_pProgramRenderFrontFaces->Enable();
  RenderBox(renderRegion, b.vCenter, b.vExtension,
            b.vTexcoordsMin, b.vTexcoordsMax,
            false, eStereoID);

/*
  float* vec = new float[m_pFBORayEntry->Width()*m_pFBORayEntry->Height()*4];
  m_pFBORayEntry->ReadBackPixels(0,0,m_pFBORayEntry->Width(),m_pFBORayEntry->Height(), vec);
  delete [] vec;
*/

  if (m_eRenderMode == RM_ISOSURFACE) {
    m_pContext->GetStateManager()->SetDepthMask(true);

    m_TargetBinder.Bind(m_pFBOIsoHit[size_t(eStereoID)], 0, m_pFBOIsoHit[size_t(eStereoID)], 1);

    GLSLProgram* shader = this->ColorData() ? m_pProgramColor : m_pProgramIso;
    shader->Enable();
    SetBrickDepShaderVars(renderRegion, b, iCurrentBrick);
    m_pFBORayEntry->Read(2);
    RenderBox(renderRegion, b.vCenter, b.vExtension,
              b.vTexcoordsMin, b.vTexcoordsMax,
              true, eStereoID);
    m_pFBORayEntry->FinishRead();

    if (m_bDoClearView) {
      m_TargetBinder.Bind(m_pFBOCVHit[size_t(eStereoID)], 0, m_pFBOCVHit[size_t(eStereoID)], 1);

      m_pProgramIso2->Enable();
      m_pFBORayEntry->Read(2);
      m_pFBOIsoHit[size_t(eStereoID)]->Read(4, 0);
      m_pFBOIsoHit[size_t(eStereoID)]->Read(5, 1);
      RenderBox(renderRegion, b.vCenter, b.vExtension,
                b.vTexcoordsMin, b.vTexcoordsMax,
                true, eStereoID);
      m_pFBOIsoHit[size_t(eStereoID)]->FinishRead(1);
      m_pFBOIsoHit[size_t(eStereoID)]->FinishRead(0);
      m_pFBORayEntry->FinishRead();
    }

  } else {
    m_TargetBinder.Bind(m_pFBO3DImageNext[size_t(eStereoID)]);

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
              true, eStereoID);
    m_pFBORayEntry->FinishRead();
  }
  m_TargetBinder.Unbind();
}


void GLRaycaster::RenderHQMIPPreLoop(RenderRegion2D &region) {
  GLGPURayTraverser::RenderHQMIPPreLoop(region);

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
            b.vTexcoordsMax, false, SI_LEFT_OR_MONO);

  m_TargetBinder.Bind(m_pFBO3DImageNext[1]);  // for MIP rendering "abuse" left-eye buffer for the itermediate results


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
            b.vTexcoordsMax, true, SI_LEFT_OR_MONO);
  m_pFBORayEntry->FinishRead();
}

void GLRaycaster::StartFrame() {
  GLGPURayTraverser::StartFrame();

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
                            GLSLProgram* shader = this->ColorData()
                                                    ? m_pProgramColor
                                                    : m_pProgramIso;
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
  GLGPURayTraverser::SetDataDepShaderVars();
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
                                                    EStereoID eStereoID) const {
  FLOATMATRIX4 m;

  FLOATMATRIX4 mInvModelView = renderRegion.modelView[size_t(eStereoID)].inverse();

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
