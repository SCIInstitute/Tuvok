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

  m_pFBORayEntry(NULL),
  m_pProgramRenderFrontFaces(NULL),
  m_pProgramRenderFrontFacesNT(NULL),
  m_pProgramIso2(NULL),
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
  CleanupShader(&m_pProgramRenderFrontFacesNT);
  CleanupShader(&m_pProgramIso2);
}

void GLTreeRaycaster::Cleanup() {
  GLRenderer::Cleanup();

  if (m_pFBORayEntry){
    m_pMasterController->MemMan()->FreeFBO(m_pFBORayEntry); 
    m_pFBORayEntry = NULL;
  }
}

void GLTreeRaycaster::CreateOffscreenBuffers() {
  GLRenderer::CreateOffscreenBuffers();
  if (m_pFBORayEntry){m_pMasterController->MemMan()->FreeFBO(m_pFBORayEntry); m_pFBORayEntry = NULL;}
  if (m_vWinSize.area() > 0) {
    m_pFBORayEntry = m_pMasterController->MemMan()->GetFBO(GL_NEAREST, GL_NEAREST, GL_CLAMP, m_vWinSize.x, m_vWinSize.y, GL_RGBA16F_ARB, 2*4, m_pContext->GetShareGroupID(), false);
  }
}

bool GLTreeRaycaster::LoadShaders() {
  if (!GLRenderer::LoadShaders()) {
    T_ERROR("Error in parent call -> aborting");
    return false;
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

void GLTreeRaycaster::RenderBox(const RenderRegion& renderRegion,
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
void GLTreeRaycaster::ClipPlaneToShader(const ExtendedPlane& clipPlane, int iStereoID, bool bForce) {
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

void GLTreeRaycaster::Render3DPreLoop(const RenderRegion3D &) {

  // render near-plane into buffer
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

void GLTreeRaycaster::Render3DInLoop(const RenderRegion3D& ,
                                 size_t , int ) {
  // TODO
}


void GLTreeRaycaster::RenderHQMIPPreLoop(RenderRegion2D &region) {
  GLRenderer::RenderHQMIPPreLoop(region);
  // TODO
}

void GLTreeRaycaster::RenderHQMIPInLoop(const RenderRegion2D &,
                                    const Brick& ) {
  // TODO
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


FLOATMATRIX4 GLTreeRaycaster::ComputeEyeToTextureMatrix(const RenderRegion &renderRegion,
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

bool GLTreeRaycaster::Continue3DDraw() {
  // TODO
  return false;
}

bool GLTreeRaycaster::Render3DRegion(RenderRegion3D& ) {
  // TODO

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
