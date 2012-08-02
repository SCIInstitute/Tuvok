#include "GLInclude.h"
#include "GLTreeRaycaster.h"
#include "GLFBOTex.h"

#include <Controller/Controller.h>
#include "../GPUMemMan/GPUMemMan.h"
#include "../../Basics/Plane.h"
#include "GLSLProgram.h"
#include "GLTexture1D.h"
#include "GLTexture2D.h"
#include "Renderer/TFScaling.h"
#include "Basics/MathTools.h"

#include "GLHashTable.h"
#include "GLVolumePool.h"
#include "GLVBO.h"

using namespace std;
using namespace tuvok;

GLTreeRaycaster::GLTreeRaycaster(MasterController* pMasterController, 
                         bool bUseOnlyPowerOfTwo, 
                         bool bDownSampleTo8Bits, 
                         bool bDisableBorder, 
                         bool bNoRCClipplanes) :
  GLRenderer(pMasterController,
             bUseOnlyPowerOfTwo, 
             bDownSampleTo8Bits, 
             bDisableBorder),

  m_pglHashTable(NULL),
  m_pVolumePool(NULL),
  m_pBBoxVBO(NULL),
  m_pFBORayEntry(NULL),
  m_pProgramRenderFrontFaces(NULL),
  m_bNoRCClipplanes(bNoRCClipplanes)
{
  m_bSupportsMeshes = false;
}

GLTreeRaycaster::~GLTreeRaycaster() {
  delete m_pglHashTable; m_pglHashTable = NULL;
  delete  m_pVolumePool; m_pVolumePool = NULL;
}


void GLTreeRaycaster::CleanupShaders() {
  GLRenderer::CleanupShaders();

  CleanupShader(&m_pProgramRenderFrontFaces);
}

void GLTreeRaycaster::Cleanup() {
  GLRenderer::Cleanup();

  if (m_pFBORayEntry){
    m_pMasterController->MemMan()->FreeFBO(m_pFBORayEntry); 
    m_pFBORayEntry = NULL;
  }

  if (m_pBBoxVBO) {
    delete m_pBBoxVBO;
    m_pBBoxVBO = NULL;
  }

}

void GLTreeRaycaster::CreateOffscreenBuffers() {
  GLRenderer::CreateOffscreenBuffers();
  if (m_pFBORayEntry){m_pMasterController->MemMan()->FreeFBO(m_pFBORayEntry); m_pFBORayEntry = NULL;}
  if (m_vWinSize.area() > 0) {
    m_pFBORayEntry = m_pMasterController->MemMan()->GetFBO(GL_NEAREST, GL_NEAREST, GL_CLAMP, m_vWinSize.x, m_vWinSize.y, GL_RGBA16F_ARB, 2*4, m_pContext->GetShareGroupID(), false);
  }
}

bool GLTreeRaycaster::Initialize(std::shared_ptr<Context> ctx) {
  if (!GLRenderer::Initialize(ctx)) {
    T_ERROR("Error in parent call -> aborting");
    return false;
  }

  // init bbox vbo
  FLOATVECTOR3 vCenter, vExtend;
  GetVolumeAABB(vCenter, vExtend);

  FLOATVECTOR3 vMinPoint, vMaxPoint;
  vMinPoint = (vCenter - vExtend/2.0);
  vMaxPoint = (vCenter + vExtend/2.0);

  m_pBBoxVBO = new GLVBO();
  std::vector<FLOATVECTOR3> posData;
  // BACK
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMinPoint.y, vMinPoint.z));
  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMinPoint.y, vMinPoint.z));
  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMaxPoint.y, vMinPoint.z));
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMaxPoint.y, vMinPoint.z));
  // FRONT
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMaxPoint.y, vMaxPoint.z));
  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMaxPoint.y, vMaxPoint.z));
  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMinPoint.y, vMaxPoint.z));
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMinPoint.y, vMaxPoint.z));
  // LEFT
  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMaxPoint.y, vMinPoint.z));
  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMinPoint.y, vMinPoint.z));
  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMinPoint.y, vMaxPoint.z));
  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMaxPoint.y, vMaxPoint.z));
  // RIGHT
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMaxPoint.y, vMaxPoint.z));
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMinPoint.y, vMaxPoint.z));
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMinPoint.y, vMinPoint.z));
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMaxPoint.y, vMinPoint.z));
  // BOTTOM
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMinPoint.y, vMaxPoint.z));
  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMinPoint.y, vMaxPoint.z));
  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMinPoint.y, vMinPoint.z));
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMinPoint.y, vMinPoint.z));
  // TOP
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMaxPoint.y, vMinPoint.z));
  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMaxPoint.y, vMinPoint.z));
  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMaxPoint.y, vMaxPoint.z));
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMaxPoint.y, vMaxPoint.z));

  // write into fbo
  m_pBBoxVBO->AddVertexData(posData);

  return true;
}

bool GLTreeRaycaster::LoadShaders() {
  if (!GLRenderer::LoadShaders()) {
    T_ERROR("Error in parent call -> aborting");
    return false;
  }

  if(!LoadAndVerifyShader(&m_pProgramRenderFrontFaces, m_vShaderSearchDirs,
                          "GLTreeRaycaster-VS.glsl",
                          NULL,
                          "GLTreeRaycaster-frontfaces-FS.glsl", NULL))
  {
      Cleanup();
      T_ERROR("Error loading a shader.");
      return false;
  } else {

  }

  return true;
/*
  const char* shaderNames[7];
  if (m_bNoRCClipplanes) {
   shaderNames[0] = "GLTreeRaycaster-1D-FS-NC.glsl";
   shaderNames[1] = "GLTreeRaycaster-1D-light-FS-NC.glsl";
   shaderNames[2] = "GLTreeRaycaster-2D-FS-NC.glsl";
   shaderNames[3] = "GLTreeRaycaster-2D-light-FS-NC.glsl";
   shaderNames[4] = "GLTreeRaycaster-Color-FS-NC.glsl";
   shaderNames[5] = "GLTreeRaycaster-ISO-CV-FS-NC.glsl";
   shaderNames[6] = "GLTreeRaycaster-ISO-FS-NC.glsl";
  } else {
   shaderNames[0] = "GLTreeRaycaster-1D-FS.glsl";
   shaderNames[1] = "GLTreeRaycaster-1D-light-FS.glsl";
   shaderNames[2] = "GLTreeRaycaster-2D-FS.glsl";
   shaderNames[3] = "GLTreeRaycaster-2D-light-FS.glsl";
   shaderNames[4] = "GLTreeRaycaster-Color-FS.glsl";
   shaderNames[5] = "GLTreeRaycaster-ISO-CV-FS.glsl";
   shaderNames[6] = "GLTreeRaycaster-ISO-FS.glsl";
  }

  std::string tfqn = m_pDataset
                     ? (m_pDataset->GetComponentCount() == 3 ||
                        m_pDataset->GetComponentCount() == 4)
                        ? "VRender1D-Color"
                        : "VRender1D"
                     : "VRender1D";

  const std::string tfqnLit = m_pDataset
                           ? (m_pDataset->GetComponentCount() == 3 ||
                              m_pDataset->GetComponentCount() == 4)
                              ? "VRender1DLit-Color.glsl"
                              : "VRender1DLit.glsl"
                           : "VRender1DLit.glsl";
  const std::string bias = tfqn + "-BScale.glsl";
  tfqn += ".glsl";

  if(!LoadAndVerifyShader(&m_pProgramRenderFrontFaces, m_vShaderSearchDirs,
                          "GLTreeRaycaster-VS.glsl",
                          NULL,
                          "GLTreeRaycaster-frontfaces-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramRenderFrontFacesNT, m_vShaderSearchDirs,
                          "GLTreeRaycasterNoTransform-VS.glsl",
                          NULL,
                          "GLTreeRaycaster-frontfaces-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgram1DTrans[0], m_vShaderSearchDirs,
                          "GLTreeRaycaster-VS.glsl",
                          NULL,
                          "Compositing.glsl",   // UnderCompositing
                          "clip-plane.glsl",    // ClipByPlane
                          "Volume3D.glsl",      // SampleVolume
                          tfqn.c_str(),         // VRender1D
                          bias.c_str(),
                          "VRender1DProxy.glsl",
                          shaderNames[0],  NULL) ||
     !LoadAndVerifyShader(&m_pProgram1DTrans[1], m_vShaderSearchDirs,
                          "GLTreeRaycaster-VS.glsl",
                          NULL,
                          "Compositing.glsl",   // UnderCompositing
                          "clip-plane.glsl",    // ClipByPlane
                          "Volume3D.glsl",      // SampleVolume
                          "lighting.glsl",      // Lighting
                          tfqnLit.c_str(),      // VRender1DLit
                          shaderNames[1], NULL) ||
     !LoadAndVerifyShader(&m_pProgram2DTrans[0], m_vShaderSearchDirs,
                          "GLTreeRaycaster-VS.glsl",
                          NULL,
                          "Compositing.glsl",   // UnderCompositing
                          "clip-plane.glsl",    // ClipByPlane
                          "Volume3D.glsl",      // SampleVolume, ComputeGradient
                          shaderNames[2], NULL) ||
     !LoadAndVerifyShader(&m_pProgram2DTrans[1], m_vShaderSearchDirs,
                          "GLTreeRaycaster-VS.glsl",
                          NULL,
                          "Compositing.glsl",   // UnderCompositing
                          "clip-plane.glsl",    // ClipByPlane
                          "Volume3D.glsl",      // SampleVolume, ComputeGradient
                          "lighting.glsl",      // Lighting
                          shaderNames[3], NULL) ||
     !LoadAndVerifyShader(&m_pProgramIso, m_vShaderSearchDirs,
                          "GLTreeRaycaster-VS.glsl",
                          NULL,
                          "clip-plane.glsl",       // ClipByPlane
                          "RefineIsosurface.glsl", // RefineIsosurface
                          "Volume3D.glsl",        // SampleVolume, ComputeNormal
                          shaderNames[6], NULL) ||
     !LoadAndVerifyShader(&m_pProgramColor, m_vShaderSearchDirs,
                          "GLTreeRaycaster-VS.glsl",
                          NULL,
                          "clip-plane.glsl",       // ClipByPlane
                          "RefineIsosurface.glsl", // RefineIsosurface
                          "Volume3D.glsl",        // SampleVolume, ComputeNormal
                          shaderNames[4], NULL) ||
     !LoadAndVerifyShader(&m_pProgramIso2, m_vShaderSearchDirs,
                          "GLTreeRaycaster-VS.glsl",
                          NULL,
                          "clip-plane.glsl",       // ClipByPlane
                          "RefineIsosurface.glsl", // RefineIsosurface
                          "Volume3D.glsl",        // SampleVolume, ComputeNormal
                          shaderNames[5], NULL) ||
     !LoadAndVerifyShader(&m_pProgramHQMIPRot, m_vShaderSearchDirs,
                          "GLTreeRaycaster-VS.glsl",
                          NULL,
                          "Volume3D.glsl",      // SampleVolume
                          "GLTreeRaycaster-MIP-Rot-FS.glsl",
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
  */
}

void GLTreeRaycaster::RenderBox(const RenderRegion& renderRegion, bool bCullBack, 
                                int iStereoID, GLSLProgram* shader) const  {
  
  m_pContext->GetStateManager()->SetCullState(bCullBack ? CULL_FRONT : CULL_BACK);

  shader->Enable();
  shader->Set("mEyeToModel", ComputeEyeToModelMatrix(renderRegion, iStereoID), 4, false); 
  shader->Set("mModelView", renderRegion.modelView[iStereoID], 4, false); 
  shader->Set("mModelViewProjection", renderRegion.modelView[iStereoID]*m_mProjection[iStereoID], 4, false); 

  m_pBBoxVBO->Bind();
  m_pBBoxVBO->Draw(GL_QUADS);
  m_pBBoxVBO->UnBind();
}


void GLTreeRaycaster::StartFrame() {
  GLRenderer::StartFrame();
  return;

  // TODO
  /*
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
  */
}

void GLTreeRaycaster::SetDataDepShaderVars() {
/*  GLRenderer::SetDataDepShaderVars();
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
  */
}


FLOATMATRIX4 GLTreeRaycaster::ComputeEyeToModelMatrix(const RenderRegion &renderRegion,
                                                        int iStereoID) const {

  FLOATVECTOR3 vCenter, vExtend;
  GetVolumeAABB(vCenter, vExtend);

  FLOATMATRIX4 mTrans, mScale, mNormalize;

  mTrans.Translation(-vCenter);
  mScale.Scaling(1.0f/vExtend);
  mNormalize.Translation(0.5f, 0.5f, 0.5f);

  return renderRegion.modelView[iStereoID].inverse() * mTrans * mScale * mNormalize;
}

bool GLTreeRaycaster::Continue3DDraw() {
  // TODO
  return false;
}

bool GLTreeRaycaster::Render3DRegion(RenderRegion3D& rr) {
  // reset state
  GPUState localState = m_BaseState;
  localState.enableBlend = false;
  m_pContext->GetStateManager()->Apply(localState);
  
  // bind output render target (DEBUG)
  m_TargetBinder.Bind(m_pFBO3DImageCurrent[0]);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // render frontfaces of bbox
  RenderBox(rr, true, 0, m_pProgramRenderFrontFaces);

  // done rendering
  m_TargetBinder.Unbind();

  // nothig can go wrong here
  return true;
}

/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2011 Interactive Visualization and Data Analysis Group.


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
