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
  \file    GLSBVR.cpp
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    August 2008
*/
#include "GLInclude.h"
#include "GLSBVR.h"

#include "Controller/Controller.h"
#include "Renderer/GL/GLSLProgram.h"
#include "Renderer/GL/GLTexture1D.h"
#include "Renderer/GL/GLTexture2D.h"
#include "Renderer/GPUMemMan/GPUMemMan.h"
#include "Renderer/TFScaling.h"
#include "Basics/MathTools.h"
using namespace std;
using namespace tuvok;

GLSBVR::GLSBVR(MasterController* pMasterController,              
               bool bUseOnlyPowerOfTwo, 
               bool bDownSampleTo8Bits, 
               bool bDisableBorder) :
  GLRenderer(pMasterController, 
             bUseOnlyPowerOfTwo,
             bDownSampleTo8Bits, 
             bDisableBorder)
{
  m_bSupportsMeshes = true;
  m_pProgram1DTransMesh[0] = NULL;
  m_pProgram1DTransMesh[1] = NULL;
  m_pProgram2DTransMesh[0] = NULL;
  m_pProgram2DTransMesh[1] = NULL;
}

GLSBVR::~GLSBVR() {
}


void GLSBVR::CleanupShaders() {
  GLRenderer::CleanupShaders();
  CleanupShader(&m_pProgram1DTransMesh[0]);
  CleanupShader(&m_pProgram1DTransMesh[1]);
  CleanupShader(&m_pProgram2DTransMesh[0]);
  CleanupShader(&m_pProgram2DTransMesh[1]);
}

bool GLSBVR::LoadShaders() {
  if (!GLRenderer::LoadShaders()) {
    T_ERROR("Error in parent call -> aborting");
    return false;
  }

  std::string tfqn = m_pDataset
                     ? this->ColorData()
                        ? "VRender1D-Color"
                        : "VRender1D"
                     : "VRender1D";
  const std::string tfqnLit = m_pDataset
                           ? this->ColorData()
                              ? "VRender1DLit-Color.glsl"
                              : "VRender1DLit.glsl"
                           : "VRender1DLit.glsl";
  const std::string bias = tfqn + "-BScale.glsl";
  tfqn += ".glsl";

  if(!LoadAndVerifyShader(&m_pProgram1DTrans[0], m_vShaderSearchDirs,
                          "GLSBVR-VS.glsl",
                          NULL,
                          "Volume3D.glsl",      // SampleVolume
                          tfqn.c_str(),         // VRender1D
                          bias.c_str(),
                          "VRender1DProxy.glsl",
                          "FTB.glsl",           // TraversalOrderDepColor
                          "GLSBVR-1D-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgram1DTrans[1], m_vShaderSearchDirs,
                          "GLSBVR-VS.glsl",
                          NULL,                          
                          "Volume3D.glsl",      // SampleVolume
                          tfqnLit.c_str(),      // VRender1DLit
                          "lighting.glsl",      // Lighting
                          "FTB.glsl",           // TraversalOrderDepColor
                          "GLSBVR-1D-light-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgram2DTrans[0], m_vShaderSearchDirs,
                          "GLSBVR-VS.glsl",
                          NULL,
                          "Volume3D.glsl",      // SampleVolume, ComputeGradient
                          "FTB.glsl",           // TraversalOrderDepColor
                          "GLSBVR-2D-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgram2DTrans[1], m_vShaderSearchDirs,
                          "GLSBVR-VS.glsl",
                          NULL,
                          "Volume3D.glsl",      // SampleVolume, ComputeGradient
                          "lighting.glsl",      // Lighting
                          "FTB.glsl",           // TraversalOrderDepColor
                          "GLSBVR-2D-light-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramHQMIPRot, m_vShaderSearchDirs,
                          "GLSBVR-VS.glsl",
                          NULL,
                          "Volume3D.glsl",      // SampleVolume
                          "GLSBVR-MIP-Rot-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramIso, m_vShaderSearchDirs,
                          "GLSBVR-VS.glsl",
                          NULL,
                          "Volume3D.glsl",      // SampleVolume, ComputeNormal
                          "GLSBVR-ISO-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramColor, m_vShaderSearchDirs,
                          "GLSBVR-VS.glsl",
                          NULL,
                          "Volume3D.glsl",      // SampleVolume, ComputeNormal
                          "GLSBVR-Color-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgram1DTransMesh[0], m_vShaderSearchDirs,
                          "GLSBVR-Mesh-VS.glsl",
                          NULL,
                          "Volume3D.glsl",      // SampleVolume
                          tfqn.c_str(),         // VRender1D
                          bias.c_str(),
                          "FTB.glsl",           // TraversalOrderDepColor
                          "lighting.glsl",      // Lighting (for Mesh)
                          "VRender1DProxy.glsl",
                          "GLSBVR-Mesh-1D-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgram1DTransMesh[1], m_vShaderSearchDirs,
                          "GLSBVR-Mesh-VS.glsl",
                          NULL,
                          "Volume3D.glsl",      // SampleVolume
                          tfqnLit.c_str(),      // VRender1DLit
                          "lighting.glsl",      // Lighting
                          "FTB.glsl",           // TraversalOrderDepColor
                          "GLSBVR-Mesh-1D-light-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgram2DTransMesh[0], m_vShaderSearchDirs,
                          "GLSBVR-Mesh-VS.glsl",
                          NULL,
                          "Volume3D.glsl",      // SampleVolume, ComputeNormal
                          "lighting.glsl",      // Lighting (for Mesh)
                          "FTB.glsl",           // TraversalOrderDepColor
                          "GLSBVR-Mesh-2D-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgram2DTransMesh[1], m_vShaderSearchDirs,
                          "GLSBVR-Mesh-VS.glsl",
                          NULL,
                          "Volume3D.glsl",      // SampleVolume
                          "lighting.glsl",      // Lighting
                          "FTB.glsl",           // TraversalOrderDepColor
                          "GLSBVR-Mesh-2D-light-FS.glsl", NULL)) {
      Cleanup();
      T_ERROR("Error loading a shader.");
      return false;
  } else {
    m_pProgram1DTrans[0]->ConnectTextureID("texVolume",0);
    m_pProgram1DTrans[0]->ConnectTextureID("texTrans",1);

    m_pProgram1DTrans[1]->ConnectTextureID("texVolume",0);
    m_pProgram1DTrans[1]->ConnectTextureID("texTrans",1);

    m_pProgram2DTrans[0]->ConnectTextureID("texVolume",0);
    m_pProgram2DTrans[0]->ConnectTextureID("texTrans",1);

    m_pProgram2DTrans[1]->ConnectTextureID("texVolume",0);
    m_pProgram2DTrans[1]->ConnectTextureID("texTrans",1);

    m_pProgram1DTransMesh[0]->ConnectTextureID("texVolume",0);
    m_pProgram1DTransMesh[0]->ConnectTextureID("texTrans",1);

    m_pProgram1DTransMesh[1]->ConnectTextureID("texVolume",0);
    m_pProgram1DTransMesh[1]->ConnectTextureID("texTrans",1);

    m_pProgram2DTransMesh[0]->ConnectTextureID("texVolume",0);
    m_pProgram2DTransMesh[0]->ConnectTextureID("texTrans",1);

    m_pProgram2DTransMesh[1]->ConnectTextureID("texVolume",0);
    m_pProgram2DTransMesh[1]->ConnectTextureID("texTrans",1);

    m_pProgramIso->ConnectTextureID("texVolume",0);

    m_pProgramColor->ConnectTextureID("texVolume",0);

    m_pProgramHQMIPRot->ConnectTextureID("texVolume",0);

    UpdateLightParamsInShaders();
  }

  return true;
}

void GLSBVR::SetDataDepShaderVars() {
  GLRenderer::SetDataDepShaderVars();

  if (m_SBVRGeogen.HasMesh()) {
    // if m_bDownSampleTo8Bits is enabled the full range from 0..255 -> 0..1 is used
    float fScale         = CalculateScaling();
    float fGradientScale = (m_pDataset->MaxGradientMagnitude() == 0) ?
                            1.0f : 1.0f/m_pDataset->MaxGradientMagnitude();
    switch (m_eRenderMode) {
      case RM_1DTRANS: {
        m_pProgram1DTransMesh[m_bUseLighting ? 1 : 0]->Enable();
        m_pProgram1DTransMesh[m_bUseLighting ? 1 : 0]->Set("fTransScale",fScale);
        break;
      }
      case RM_2DTRANS: {
        m_pProgram2DTransMesh[m_bUseLighting ? 1 : 0]->Enable();
        m_pProgram2DTransMesh[m_bUseLighting ? 1 : 0]->Set("fTransScale",fScale);
        m_pProgram2DTransMesh[m_bUseLighting ? 1 : 0]->Set("fGradientScale",fGradientScale);
        break;
      }

      default : break; // suppress warnings 
    }
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

void GLSBVR::SetBrickDepShaderVars(const Brick& currentBrick) {
  FLOATVECTOR3 vVoxelSizeTexSpace;
  if (m_bUseOnlyPowerOfTwo)  {
    UINTVECTOR3 vP2VoxelCount(MathTools::NextPow2(currentBrick.vVoxelCount.x),
                              MathTools::NextPow2(currentBrick.vVoxelCount.y),
                              MathTools::NextPow2(currentBrick.vVoxelCount.z));

    vVoxelSizeTexSpace = 1.0f/FLOATVECTOR3(vP2VoxelCount);
  } else {
    vVoxelSizeTexSpace = 1.0f/FLOATVECTOR3(currentBrick.vVoxelCount);
  }

  float fStepScale = m_SBVRGeogen.GetOpacityCorrection();

  GLSLProgram *shader;
  switch (m_eRenderMode) {
    case RM_1DTRANS: {
      shader = (m_SBVRGeogen.HasMesh() 
                   ? (m_pProgram1DTransMesh[m_bUseLighting ? 1 : 0])
                   : (m_pProgram1DTrans[m_bUseLighting ? 1 : 0]));
      shader->Enable();
      shader->Set("fStepScale", fStepScale);
      if (m_bUseLighting) {
        shader->Set("vVoxelStepsize", vVoxelSizeTexSpace.x, vVoxelSizeTexSpace.y, vVoxelSizeTexSpace.z);
      }
      break;
    }
    case RM_2DTRANS: {
      shader = (m_SBVRGeogen.HasMesh() 
                    ? (m_pProgram2DTransMesh[m_bUseLighting ? 1 : 0])
                    : (m_pProgram2DTrans[m_bUseLighting ? 1 : 0]));
      shader->Enable();
      shader->Set("fStepScale", fStepScale);
      shader->Set("vVoxelStepsize", vVoxelSizeTexSpace.x, vVoxelSizeTexSpace.y, vVoxelSizeTexSpace.z);
      break;
    }
    case RM_ISOSURFACE: {
      shader = this->ColorData() ? m_pProgramColor : m_pProgramIso;
      shader->Enable();
      shader->Set("vVoxelStepsize", vVoxelSizeTexSpace.x, vVoxelSizeTexSpace.y, vVoxelSizeTexSpace.z);
      break;
    }
    default: T_ERROR("Invalid rendermode set"); break;
  }
}

void GLSBVR::EnableClipPlane(RenderRegion *renderRegion) {
  if(!m_bClipPlaneOn) {
    AbstrRenderer::EnableClipPlane(renderRegion);
    m_SBVRGeogen.EnableClipPlane();
    PLANE<float> plane(m_ClipPlane.Plane());
    m_SBVRGeogen.SetClipPlane(plane);
  }
}

void GLSBVR::DisableClipPlane(RenderRegion *renderRegion) {
  if(m_bClipPlaneOn) {
    AbstrRenderer::DisableClipPlane(renderRegion);
    m_SBVRGeogen.DisableClipPlane();
  }
}

void GLSBVR::Render3DPreLoop(const RenderRegion3D&) {
  m_SBVRGeogen.SetSamplingModifier(
    m_fSampleRateModifier / (this->decreaseSamplingRateNow ?
                             m_fSampleDecFactor : 1.0f));

  if(m_bClipPlaneOn) {
    m_SBVRGeogen.EnableClipPlane();
    PLANE<float> plane(m_ClipPlane.Plane());
    m_SBVRGeogen.SetClipPlane(plane);
  } else {
    m_SBVRGeogen.DisableClipPlane();
  }

  switch (m_eRenderMode) {
    case RM_1DTRANS    :  m_p1DTransTex->Bind(1);
                          m_pProgram1DTrans[m_bUseLighting ? 1 : 0]->Enable();
                          break;
    case RM_2DTRANS    :  m_p2DTransTex->Bind(1);
                          m_pProgram2DTrans[m_bUseLighting ? 1 : 0]->Enable();
                          break;
    case RM_ISOSURFACE :  // can't set the shader here as multiple shaders
                          // are used for that renderer
                          break;
    default    :  T_ERROR("Invalid rendermode set");
                          break;
  }

  m_SBVRGeogen.SetLODData( UINTVECTOR3(m_pDataset->GetDomainSize(static_cast<size_t>(m_iCurrentLOD))));
}


#define BUFFER_OFFSET(i) ((char *)NULL + (i))
GLsizei iStructSize = GLsizei(sizeof(VERTEX_FORMAT));

void GLSBVR::RenderProxyGeometry() const {
  if (m_SBVRGeogen.m_vSliceTriangles.empty()) return;

  GL(glBindBuffer(GL_ARRAY_BUFFER, m_GeoBuffer));
  GL(glBufferData(GL_ARRAY_BUFFER, GLsizei(m_SBVRGeogen.m_vSliceTriangles.size())*iStructSize, &m_SBVRGeogen.m_vSliceTriangles[0], GL_STREAM_DRAW));
  GL(glVertexPointer(3, GL_FLOAT, iStructSize, BUFFER_OFFSET(0)));
  if (m_SBVRGeogen.HasMesh()) {
    GL(glTexCoordPointer(4 , GL_FLOAT, iStructSize, BUFFER_OFFSET(12)));
    GL(glNormalPointer(GL_FLOAT, iStructSize, BUFFER_OFFSET(28)));
    GL(glEnableClientState(GL_NORMAL_ARRAY));
  } else {
    GL(glTexCoordPointer(3, GL_FLOAT, iStructSize, BUFFER_OFFSET(12)));
  }
  
  GL(glEnableClientState(GL_VERTEX_ARRAY));
  GL(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
  GL(glDrawArrays(GL_TRIANGLES, 0, GLsizei(m_SBVRGeogen.m_vSliceTriangles.size())));
  GL(glDisableClientState(GL_VERTEX_ARRAY));
  GL(glDisableClientState(GL_TEXTURE_COORD_ARRAY));
  GL(glDisableClientState(GL_NORMAL_ARRAY));
}

void GLSBVR::Render3DInLoop(const RenderRegion3D& renderRegion,
                            size_t iCurrentBrick, EStereoID eStereoID) {

  m_pContext->GetStateManager()->Apply(m_BaseState);

  const Brick& b = (eStereoID == SI_LEFT_OR_MONO) ? m_vCurrentBrickList[iCurrentBrick] : m_vLeftEyeBrickList[iCurrentBrick];
  
  if (m_iBricksRenderedInThisSubFrame == 0 && m_eRenderMode == RM_ISOSURFACE){
    m_TargetBinder.Bind(m_pFBOIsoHit[size_t(eStereoID)], 0, m_pFBOIsoHit[size_t(eStereoID)], 1);
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    if (m_bDoClearView) {
      m_TargetBinder.Bind(m_pFBOCVHit[size_t(eStereoID)], 0, m_pFBOCVHit[size_t(eStereoID)], 1);
      GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    }
  }

  if (!m_bSupportsMeshes && b.bIsEmpty) return;

  // setup the slice generator
  m_SBVRGeogen.SetBrickData(b.vExtension, b.vVoxelCount,
                            b.vTexcoordsMin, b.vTexcoordsMax);
  FLOATMATRIX4 maBricktTrans;
  maBricktTrans.Translation(b.vCenter.x, b.vCenter.y, b.vCenter.z);
  m_mProjection[size_t(eStereoID)].setProjection();
  renderRegion.modelView[size_t(eStereoID)].setModelview();

  m_SBVRGeogen.SetBrickTrans(b.vCenter);
  m_SBVRGeogen.SetWorld(renderRegion.rotation*renderRegion.translation);
  m_SBVRGeogen.SetView(m_mView[size_t(eStereoID)]);

  if (m_bSupportsMeshes) {
    m_SBVRGeogen.ResetMesh();
    if (m_eRenderMode != RM_ISOSURFACE) {
      for (vector<shared_ptr<RenderMesh>>::iterator mesh = m_Meshes.begin();
           mesh != m_Meshes.end(); mesh++) {
        if ((*mesh)->GetActive()) {
          m_SBVRGeogen.AddMesh((*mesh)->GetInPointList(false));
        }
      }
    }
  }

  m_SBVRGeogen.ComputeGeometry(b.bIsEmpty);

  if (m_eRenderMode == RM_ISOSURFACE) {
    m_pContext->GetStateManager()->SetEnableBlend(false);

    m_TargetBinder.Bind(m_pFBOIsoHit[size_t(eStereoID)], 0, m_pFBOIsoHit[size_t(eStereoID)], 1);
    SetBrickDepShaderVars(b);
    
    GLSLProgram* shader = this->ColorData() ? m_pProgramColor : m_pProgramIso;
    shader->Set("fIsoval", static_cast<float>
                                        (this->GetNormalizedIsovalue()));    
    RenderProxyGeometry();

    if (m_bDoClearView) {
      m_TargetBinder.Bind(m_pFBOCVHit[size_t(eStereoID)], 0, m_pFBOCVHit[size_t(eStereoID)], 1);

      m_pProgramIso->Enable();
      m_pProgramIso->Set("fIsoval", static_cast<float>
                                                 (GetNormalizedCVIsovalue()));
      RenderProxyGeometry();
    }
  } else {
    m_pContext->GetStateManager()->SetDepthMask(false);

    m_TargetBinder.Bind(m_pFBO3DImageNext[size_t(eStereoID)]);
    SetBrickDepShaderVars(b);
    RenderProxyGeometry();
  }
  m_TargetBinder.Unbind();
}

void GLSBVR::RenderHQMIPPreLoop(RenderRegion2D &region) {
  GLRenderer::RenderHQMIPPreLoop(region);
  m_pProgramHQMIPRot->Enable();
}

void GLSBVR::RenderHQMIPInLoop(const RenderRegion2D &, const Brick& b) {
  m_SBVRGeogen.SetBrickData(b.vExtension, b.vVoxelCount, b.vTexcoordsMin, b.vTexcoordsMax);
  if (m_bOrthoView)
    m_SBVRGeogen.SetView(FLOATMATRIX4());
  else
    m_SBVRGeogen.SetView(m_mView[0]);

  m_SBVRGeogen.SetBrickTrans(b.vCenter);
  m_SBVRGeogen.SetWorld(m_maMIPRotation);

  m_SBVRGeogen.ComputeGeometry(false);

  GPUState localState = m_BaseState;
  localState.blendFuncSrc = BF_ONE;
  localState.blendEquation = BE_MAX;
  localState.enableDepthTest = false;
  m_pContext->GetStateManager()->Apply(localState);

  RenderProxyGeometry();
}

bool GLSBVR::LoadDataset(const string& strFilename) {
  if (GLRenderer::LoadDataset(strFilename)) {
    UINTVECTOR3    vSize = UINTVECTOR3(m_pDataset->GetDomainSize());
    FLOATVECTOR3 vAspect = FLOATVECTOR3(m_pDataset->GetScale());
    vAspect /= vAspect.maxVal();

    m_SBVRGeogen.SetVolumeData(vAspect, vSize);
    return true;
  } else return false;
}

void GLSBVR::UpdateLightParamsInShaders() {
  GLRenderer::UpdateLightParamsInShaders();

  FLOATVECTOR3 a = m_cAmbient.xyz()*m_cAmbient.w;
  FLOATVECTOR3 d = m_cDiffuse.xyz()*m_cDiffuse.w;
  FLOATVECTOR3 s = m_cSpecular.xyz()*m_cSpecular.w;

  FLOATVECTOR3 aM = m_cAmbientM.xyz()*m_cAmbientM.w;
  FLOATVECTOR3 dM = m_cDiffuseM.xyz()*m_cDiffuseM.w;
  FLOATVECTOR3 sM = m_cSpecularM.xyz()*m_cSpecularM.w;

  FLOATVECTOR3 scale = 1.0f/FLOATVECTOR3(m_pDataset->GetScale());

  m_pProgram1DTransMesh[0]->Enable();
  m_pProgram1DTransMesh[0]->Set("vLightAmbientM",aM.x,aM.y,aM.z);
  m_pProgram1DTransMesh[0]->Set("vLightDiffuseM",dM.x,dM.y,dM.z);
  m_pProgram1DTransMesh[0]->Set("vLightSpecularM",sM.x,sM.y,sM.z);
  m_pProgram1DTransMesh[0]->Set("vLightDir",m_vLightDir.x,m_vLightDir.y,m_vLightDir.z);

  m_pProgram2DTransMesh[0]->Enable();
  m_pProgram2DTransMesh[0]->Set("vLightAmbientM",aM.x,aM.y,aM.z);
  m_pProgram2DTransMesh[0]->Set("vLightDiffuseM",dM.x,dM.y,dM.z);
  m_pProgram2DTransMesh[0]->Set("vLightSpecularM",sM.x,sM.y,sM.z);
  m_pProgram2DTransMesh[0]->Set("vLightDir",m_vLightDir.x,m_vLightDir.y,m_vLightDir.z);

  m_pProgram1DTransMesh[1]->Enable();
  m_pProgram1DTransMesh[1]->Set("vLightAmbient",a.x,a.y,a.z);
  m_pProgram1DTransMesh[1]->Set("vLightDiffuse",d.x,d.y,d.z);
  m_pProgram1DTransMesh[1]->Set("vLightSpecular",s.x,s.y,s.z);
  m_pProgram1DTransMesh[1]->Set("vLightAmbientM",aM.x,aM.y,aM.z);
  m_pProgram1DTransMesh[1]->Set("vLightDiffuseM",dM.x,dM.y,dM.z);
  m_pProgram1DTransMesh[1]->Set("vLightSpecularM",sM.x,sM.y,sM.z);
  m_pProgram1DTransMesh[1]->Set("vLightDir",m_vLightDir.x,m_vLightDir.y,m_vLightDir.z);
  m_pProgram1DTransMesh[1]->Set("vDomainScale",scale.x,scale.y,scale.z);

  m_pProgram2DTransMesh[1]->Enable();
  m_pProgram2DTransMesh[1]->Set("vLightAmbient",a.x,a.y,a.z);
  m_pProgram2DTransMesh[1]->Set("vLightDiffuse",d.x,d.y,d.z);
  m_pProgram2DTransMesh[1]->Set("vLightSpecular",s.x,s.y,s.z);
  m_pProgram2DTransMesh[1]->Set("vLightAmbientM",aM.x,aM.y,aM.z);
  m_pProgram2DTransMesh[1]->Set("vLightDiffuseM",dM.x,dM.y,dM.z);
  m_pProgram2DTransMesh[1]->Set("vLightSpecularM",sM.x,sM.y,sM.z);
  m_pProgram2DTransMesh[1]->Set("vLightDir",m_vLightDir.x,m_vLightDir.y,m_vLightDir.z);
  m_pProgram2DTransMesh[1]->Set("vDomainScale",scale.x,scale.y,scale.z);
}
