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

using namespace std;
using namespace tuvok;

GLSBVR::GLSBVR(MasterController* pMasterController, bool bUseOnlyPowerOfTwo, bool bDownSampleTo8Bits, bool bDisableBorder) :
  GLRenderer(pMasterController, bUseOnlyPowerOfTwo, bDownSampleTo8Bits, bDisableBorder),
  m_pProgramIsoNoCompose(NULL),
  m_pProgramColorNoCompose(NULL)
{
  m_bSupportsMeshes = true;
  m_pProgram1DTransMesh[0] = NULL;
  m_pProgram1DTransMesh[1] = NULL;
  m_pProgram2DTransMesh[0] = NULL;
  m_pProgram2DTransMesh[1] = NULL;
}

GLSBVR::~GLSBVR() {
}

void GLSBVR::Cleanup() {
  GLRenderer::Cleanup();
}


void GLSBVR::CleanupShaders() {
  GLRenderer::CleanupShaders();
  CleanupShader(&m_pProgramIsoNoCompose);
  CleanupShader(&m_pProgramColorNoCompose);
  CleanupShader(&m_pProgram1DTransMesh[0]);
  CleanupShader(&m_pProgram1DTransMesh[1]);
  CleanupShader(&m_pProgram2DTransMesh[0]);
  CleanupShader(&m_pProgram2DTransMesh[1]);
}

bool GLSBVR::Initialize() {
  bool bParentOK = GLRenderer::Initialize();
  if (bParentOK) {
    return true;
  }
  return false;
}

bool GLSBVR::LoadShaders() {

  if (!GLRenderer::LoadShaders()) {
    T_ERROR("Error in parent call -> aborting");
    return false;
  }

  const std::string tfqn = m_pDataset
                           ? (m_pDataset->GetComponentCount() == 3 ||
                              m_pDataset->GetComponentCount() == 4)
                              ? "vr-no-tfqn.glsl"
                              : "vr-std.glsl"
                           : "vr-std.glsl";

  if(!LoadAndVerifyShader(&m_pProgram1DTrans[0], m_vShaderSearchDirs,
                          "GLSBVR-VS.glsl",
                          NULL,
                          "Volume3D.glsl",      // SampleVolume
                          tfqn.c_str(),         // VRender1D
                          "FTB.glsl",           // TraversalOrderDepColor
                          "GLSBVR-1D-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgram1DTrans[1], m_vShaderSearchDirs,
                          "GLSBVR-VS.glsl",
                          NULL,                          
                          "Volume3D.glsl",      // SampleVolume
                          tfqn.c_str(),         // VRender1DLit
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
     !LoadAndVerifyShader(&m_pProgramIsoNoCompose, m_vShaderSearchDirs,
                          "GLSBVR-VS.glsl",
                          NULL,
                          "Volume3D.glsl",      // SampleVolume, ComputeNormal
                          "lighting.glsl",      // Lighting
                          "GLSBVR-ISO-NC-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramColorNoCompose, m_vShaderSearchDirs,
                          "GLSBVR-VS.glsl",
                          NULL,
                          "Volume3D.glsl",      // SampleVolume
                          "GLSBVR-Color-NC-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgram1DTransMesh[0], m_vShaderSearchDirs,
                          "GLSBVR-Mesh-VS.glsl",
                          NULL,
                          "Volume3D.glsl",      // SampleVolume
                          tfqn.c_str(),         // VRender1D
                          "FTB.glsl",           // TraversalOrderDepColor
                          "lighting.glsl",      // Lighting (for Mesh)
                          "GLSBVR-Mesh-1D-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgram1DTransMesh[1], m_vShaderSearchDirs,
                          "GLSBVR-Mesh-VS.glsl",
                          NULL,
                          "Volume3D.glsl",      // SampleVolume
                          tfqn.c_str(),         // VRender1DLit
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

    m_pProgramIsoNoCompose->ConnectTextureID("texVolume",0);

    m_pProgramColorNoCompose->ConnectTextureID("texVolume",0);

    UpdateLightParamsInShaders();
  }

  return true;
}

void GLSBVR::SetDataDepShaderVars() {
  GLRenderer::SetDataDepShaderVars();

  // if m_bDownSampleTo8Bits is enabled the full range from 0..255 -> 0..1 is used
  float fScale         = CalculateScaling();
  float fGradientScale = (m_pDataset->MaxGradientMagnitude() == 0) ?
                          1.0f : 1.0f/m_pDataset->MaxGradientMagnitude();
  switch (m_eRenderMode) {
    case RM_1DTRANS: {
      m_pProgram1DTransMesh[m_bUseLighting ? 1 : 0]->Enable();
      m_pProgram1DTransMesh[m_bUseLighting ? 1 : 0]->SetUniformVector("fTransScale",fScale);
      break;
    }
    case RM_2DTRANS: {
      m_pProgram2DTransMesh[m_bUseLighting ? 1 : 0]->Enable();
      m_pProgram2DTransMesh[m_bUseLighting ? 1 : 0]->SetUniformVector("fTransScale",fScale);
      m_pProgram2DTransMesh[m_bUseLighting ? 1 : 0]->SetUniformVector("fGradientScale",fGradientScale);
      break;
    }

    default : break; // suppress warnings 
  }

  if (m_eRenderMode == RM_ISOSURFACE && m_bAvoidSeparateCompositing) {
    GLSLProgram* shader = (m_pDataset->GetComponentCount() == 1) ? m_pProgramIsoNoCompose : m_pProgramColorNoCompose;

    FLOATVECTOR3 d = m_cDiffuse.xyz()*m_cDiffuse.w;

    shader->Enable();
    shader->SetUniformVector("fIsoval", static_cast<float>
                                        (this->GetNormalizedIsovalue()));
    // this is not really a data dependent var but as we only need to
    // do it once per frame we may also do it here
    shader->SetUniformVector("vLightDiffuse",d.x*m_vIsoColor.x,d.y*m_vIsoColor.y,d.z*m_vIsoColor.z);
  }

  if(m_eRenderMode == RM_1DTRANS && m_TFScalingMethod == SMETH_BIAS_AND_SCALE) {
    std::pair<float,float> bias_scale = scale_bias_and_scale(*m_pDataset);
    MESSAGE("setting TF bias (%5.3f) and scale (%5.3f)", bias_scale.first,
            bias_scale.second);
    m_pProgram1DTrans[m_bUseLighting ? 1 : 0]->Enable();
    m_pProgram1DTrans[m_bUseLighting ? 1 : 0]->SetUniformVector("TFuncBias", bias_scale.first);
    m_pProgram1DTrans[m_bUseLighting ? 1 : 0]->SetUniformVector("fTransScale", bias_scale.second);
  }
}

void GLSBVR::SetBrickDepShaderVars(const Brick& currentBrick) {
  FLOATVECTOR3 vStep(1.0f/currentBrick.vVoxelCount.x, 1.0f/currentBrick.vVoxelCount.y, 1.0f/currentBrick.vVoxelCount.z);

  float fStepScale = m_SBVRGeogen.GetOpacityCorrection();

  GLSLProgram *shader;
  switch (m_eRenderMode) {
    case RM_1DTRANS: {
      shader = (m_SBVRGeogen.HasMesh() 
                   ? (m_pProgram1DTransMesh[m_bUseLighting ? 1 : 0])
                   : (m_pProgram1DTrans[m_bUseLighting ? 1 : 0]));
      shader->Enable();
      shader->SetUniformVector("fStepScale", fStepScale);
      if (m_bUseLighting) {
        shader->SetUniformVector("vVoxelStepsize", vStep.x, vStep.y, vStep.z);
      }
      break;
    }
    case RM_2DTRANS: {
      shader = (m_SBVRGeogen.HasMesh() 
                    ? (m_pProgram2DTransMesh[m_bUseLighting ? 1 : 0])
                    : (m_pProgram2DTrans[m_bUseLighting ? 1 : 0]));
      shader->Enable();
      shader->SetUniformVector("fStepScale", fStepScale);
      shader->SetUniformVector("vVoxelStepsize", vStep.x, vStep.y, vStep.z);
      break;
    }
    case RM_ISOSURFACE: {
      if (m_bAvoidSeparateCompositing) {
        shader = (m_pDataset->GetComponentCount() == 1) ?
                 m_pProgramIsoNoCompose : m_pProgramColorNoCompose;
      } else {
        shader = (m_pDataset->GetComponentCount() == 1) ?
                 m_pProgramIso : m_pProgramColor;
      }
      shader->Enable();
      shader->SetUniformVector("vVoxelStepsize", vStep.x, vStep.y, vStep.z);
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
                          glEnable(GL_BLEND);
                          glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);
                          break;
    case RM_2DTRANS    :  m_p2DTransTex->Bind(1);
                          m_pProgram2DTrans[m_bUseLighting ? 1 : 0]->Enable();
                          glEnable(GL_BLEND);
                          glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);
                          break;
    case RM_ISOSURFACE :  if (m_bAvoidSeparateCompositing) {
                            if (m_pDataset->GetComponentCount() == 1)
                              m_pProgramIsoNoCompose->Enable();
                            else
                              m_pProgramColorNoCompose->Enable();
                            glEnable(GL_BLEND);
                            glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);
                          } else {
                            glEnable(GL_DEPTH_TEST);
                          }
                          break;
    default    :  T_ERROR("Invalid rendermode set");
                          break;
  }

  m_SBVRGeogen.SetLODData( UINTVECTOR3(m_pDataset->GetDomainSize(static_cast<size_t>(m_iCurrentLOD))));
  glEnable(GL_DEPTH_TEST);
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
                            size_t iCurrentBrick, int iStereoID) {
  const Brick& b = (iStereoID == 0) ? m_vCurrentBrickList[iCurrentBrick] : m_vLeftEyeBrickList[iCurrentBrick];
  
  if (m_iBricksRenderedInThisSubFrame == 0 && !m_bAvoidSeparateCompositing && m_eRenderMode == RM_ISOSURFACE){
    m_TargetBinder.Bind(m_pFBOIsoHit[iStereoID], 0, m_pFBOIsoHit[iStereoID], 1);
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    if (m_bDoClearView) {
      m_TargetBinder.Bind(m_pFBOCVHit[iStereoID], 0, m_pFBOCVHit[iStereoID], 1);
      GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    }
  }

  if (!m_bSupportsMeshes && b.bIsEmpty) return;

  // setup the slice generator
  m_SBVRGeogen.SetBrickData(b.vExtension, b.vVoxelCount,
                            b.vTexcoordsMin, b.vTexcoordsMax);
  FLOATMATRIX4 maBricktTrans;
  maBricktTrans.Translation(b.vCenter.x, b.vCenter.y, b.vCenter.z);
  m_mProjection[iStereoID].setProjection();
  renderRegion.modelView[iStereoID].setModelview();

  m_SBVRGeogen.SetBrickTrans(b.vCenter);
  m_SBVRGeogen.SetWorld(renderRegion.rotation*renderRegion.translation);
  m_SBVRGeogen.SetView(m_mView[iStereoID]);

  if (m_bSupportsMeshes) {
    m_SBVRGeogen.ResetMesh();
    if (m_eRenderMode != RM_ISOSURFACE) {
      for (vector<RenderMesh*>::iterator mesh = m_Meshes.begin();
           mesh != m_Meshes.end(); mesh++) {
        if ((*mesh)->GetActive()) {
          m_SBVRGeogen.AddMesh((*mesh)->GetInPointList(false));
        }
      }
    }
  }

  m_SBVRGeogen.ComputeGeometry(b.bIsEmpty);

  if (!m_bAvoidSeparateCompositing && m_eRenderMode == RM_ISOSURFACE) {
    GL(glDisable(GL_BLEND));
    m_TargetBinder.Bind(m_pFBOIsoHit[iStereoID], 0, m_pFBOIsoHit[iStereoID], 1);
    SetBrickDepShaderVars(b);
    
    GLSLProgram* shader = (m_pDataset->GetComponentCount() == 1) ? m_pProgramIso : m_pProgramColor;
    shader->SetUniformVector("fIsoval", static_cast<float>
                                        (this->GetNormalizedIsovalue()));    
    RenderProxyGeometry();

    if (m_bDoClearView) {
      m_TargetBinder.Bind(m_pFBOCVHit[iStereoID], 0, m_pFBOCVHit[iStereoID], 1);

      m_pProgramIso->Enable();
      m_pProgramIso->SetUniformVector("fIsoval", static_cast<float>
                                                 (GetNormalizedCVIsovalue()));
      RenderProxyGeometry();
    }
  } else {
    GL((void)0);
    m_TargetBinder.Bind(m_pFBO3DImageCurrent[iStereoID]);
    GL(glDepthMask(GL_FALSE));
    SetBrickDepShaderVars(b);
    RenderProxyGeometry();
    GL(glDepthMask(GL_TRUE));
  }
  m_TargetBinder.Unbind();
}


void GLSBVR::Render3DPostLoop() {
  GLRenderer::Render3DPostLoop();

  // disable the shader
  switch (m_eRenderMode) {
    case RM_1DTRANS    :  glDisable(GL_BLEND);
                          break;
    case RM_2DTRANS    :  glDisable(GL_BLEND);
                          break;
    case RM_ISOSURFACE :  if (m_bAvoidSeparateCompositing) {
                             glDisable(GL_BLEND);
                          }
                          break;
    case RM_INVALID    :  T_ERROR("Invalid rendermode set"); break;
  }
}

void GLSBVR::RenderHQMIPPreLoop(RenderRegion2D &region) {
  GLRenderer::RenderHQMIPPreLoop(region);
  m_pProgramHQMIPRot->Enable();

  glBlendFunc(GL_ONE, GL_ONE);
  glBlendEquation(GL_MAX);
  glEnable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
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

void GLSBVR::ComposeSurfaceImage(RenderRegion& renderRegion, int iStereoID) {
  if (!m_bAvoidSeparateCompositing)
    GLRenderer::ComposeSurfaceImage(renderRegion, iStereoID);
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

  m_pProgramIsoNoCompose->Enable();
  m_pProgramIsoNoCompose->SetUniformVector("vLightAmbient",a.x,a.y,a.z);
  m_pProgramIsoNoCompose->SetUniformVector("vLightDiffuse",d.x,d.y,d.z);
  m_pProgramIsoNoCompose->SetUniformVector("vLightSpecular",s.x,s.y,s.z);
  m_pProgramIsoNoCompose->SetUniformVector("vLightDir",m_vLightDir.x,m_vLightDir.y,m_vLightDir.z);
  m_pProgramIsoNoCompose->SetUniformVector("vDomainScale",scale.x,scale.y,scale.z);

  m_pProgramColorNoCompose->Enable();
  m_pProgramColorNoCompose->SetUniformVector("vLightAmbient",a.x,a.y,a.z);
  //m_pProgramColorNoCompose->SetUniformVector("vLightDiffuse",d.x,d.y,d.z); // only abient color is used in color-volume mode yet
  //m_pProgramColorNoCompose->SetUniformVector("vLightSpecular",s.x,s.y,s.z); // only abient color is used in color-volume mode yet
  m_pProgramColorNoCompose->SetUniformVector("vLightDir",m_vLightDir.x,m_vLightDir.y,m_vLightDir.z);
  m_pProgramColorNoCompose->SetUniformVector("vDomainScale",scale.x,scale.y,scale.z);

  m_pProgram1DTransMesh[0]->Enable();
  m_pProgram1DTransMesh[0]->SetUniformVector("vLightAmbientM",aM.x,aM.y,aM.z);
  m_pProgram1DTransMesh[0]->SetUniformVector("vLightDiffuseM",dM.x,dM.y,dM.z);
  m_pProgram1DTransMesh[0]->SetUniformVector("vLightSpecularM",sM.x,sM.y,sM.z);
  m_pProgram1DTransMesh[0]->SetUniformVector("vLightDir",m_vLightDir.x,m_vLightDir.y,m_vLightDir.z);

  m_pProgram2DTransMesh[0]->Enable();
  m_pProgram2DTransMesh[0]->SetUniformVector("vLightAmbientM",aM.x,aM.y,aM.z);
  m_pProgram2DTransMesh[0]->SetUniformVector("vLightDiffuseM",dM.x,dM.y,dM.z);
  m_pProgram2DTransMesh[0]->SetUniformVector("vLightSpecularM",sM.x,sM.y,sM.z);
  m_pProgram2DTransMesh[0]->SetUniformVector("vLightDir",m_vLightDir.x,m_vLightDir.y,m_vLightDir.z);

  m_pProgram1DTransMesh[1]->Enable();
  m_pProgram1DTransMesh[1]->SetUniformVector("vLightAmbient",a.x,a.y,a.z);
  m_pProgram1DTransMesh[1]->SetUniformVector("vLightDiffuse",d.x,d.y,d.z);
  m_pProgram1DTransMesh[1]->SetUniformVector("vLightSpecular",s.x,s.y,s.z);
  m_pProgram1DTransMesh[1]->SetUniformVector("vLightAmbientM",aM.x,aM.y,aM.z);
  m_pProgram1DTransMesh[1]->SetUniformVector("vLightDiffuseM",dM.x,dM.y,dM.z);
  m_pProgram1DTransMesh[1]->SetUniformVector("vLightSpecularM",sM.x,sM.y,sM.z);
  m_pProgram1DTransMesh[1]->SetUniformVector("vLightDir",m_vLightDir.x,m_vLightDir.y,m_vLightDir.z);
  m_pProgram1DTransMesh[1]->SetUniformVector("vDomainScale",scale.x,scale.y,scale.z);

  m_pProgram2DTransMesh[1]->Enable();
  m_pProgram2DTransMesh[1]->SetUniformVector("vLightAmbient",a.x,a.y,a.z);
  m_pProgram2DTransMesh[1]->SetUniformVector("vLightDiffuse",d.x,d.y,d.z);
  m_pProgram2DTransMesh[1]->SetUniformVector("vLightSpecular",s.x,s.y,s.z);
  m_pProgram2DTransMesh[1]->SetUniformVector("vLightAmbientM",aM.x,aM.y,aM.z);
  m_pProgram2DTransMesh[1]->SetUniformVector("vLightDiffuseM",dM.x,dM.y,dM.z);
  m_pProgram2DTransMesh[1]->SetUniformVector("vLightSpecularM",sM.x,sM.y,sM.z);
  m_pProgram2DTransMesh[1]->SetUniformVector("vLightDir",m_vLightDir.x,m_vLightDir.y,m_vLightDir.z);
  m_pProgram2DTransMesh[1]->SetUniformVector("vDomainScale",scale.x,scale.y,scale.z);
}
