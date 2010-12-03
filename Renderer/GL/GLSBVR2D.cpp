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
  \file    GLSBVR2D.cpp
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    August 2008
*/
#include <algorithm>
#include "GLInclude.h"
#include "GLSBVR2D.h"

#include "Controller/Controller.h"
#include "Renderer/GL/GLSLProgram.h"
#include "Renderer/GL/GLTexture1D.h"
#include "Renderer/GL/GLTexture2D.h"
#include "Renderer/GL/GLVolume2DTex.h"
#include "Renderer/GPUMemMan/GPUMemMan.h"
#include "Renderer/TFScaling.h"

using namespace std;
using namespace tuvok;

GLSBVR2D::GLSBVR2D(MasterController* pMasterController, bool bUseOnlyPowerOfTwo, bool bDownSampleTo8Bits, bool bDisableBorder) :
  GLRenderer(pMasterController, bUseOnlyPowerOfTwo, bDownSampleTo8Bits, bDisableBorder),
  m_pProgramIsoNoCompose(NULL),
  m_pProgramColorNoCompose(NULL),
  m_bUse3DTexture(false)
{
  m_bSupportsMeshes = false; // not fully implemented yet
}

GLSBVR2D::~GLSBVR2D() {
}

void GLSBVR2D::CleanupShaders() {
  GLRenderer::CleanupShaders();
  CleanupShader(&m_pProgramIsoNoCompose);
  CleanupShader(&m_pProgramColorNoCompose);
}

void GLSBVR2D::SetUse3DTexture(bool bUse3DTexture) {

  if (bUse3DTexture != m_bUse3DTexture) {
    m_bUse3DTexture = bUse3DTexture;
    CleanupShaders();
    LoadShaders();
  }
}

void GLSBVR2D::BindVolumeStringsToTexUnit(GLSLProgram* program, bool bGradients) {
  if (m_bUse3DTexture) {
    program->ConnectTextureID("texVolume",0);
  } else {
    program->ConnectTextureID("texSlice0",0);
    program->ConnectTextureID("texSlice1",2);
    if (bGradients) program->ConnectTextureID("texSlicem1",3);
  }
}

bool GLSBVR2D::LoadShaders() {
  // do not call GLRenderer::LoadShaders as we want to control
  // what volume access function is linked (Volume3D or Volume2D)

  string volumeAccessFunction = m_bUse3DTexture ? "Volume3D.glsl"
                                                : "Volume2D.glsl";
  const std::string tfqn = m_pDataset
                           ? m_pDataset->GetComponentCount() == 4
                              ? "vr-no-tfqn.glsl"
                              : "vr-std.glsl"
                           : "vr-std.glsl";

  if(!LoadAndVerifyShader(&m_pProgramTrans, m_vShaderSearchDirs,
                          "Transfer-VS.glsl",
                          NULL,
                          "Transfer-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgram1DTransSlice, m_vShaderSearchDirs,
                          "Transfer-VS.glsl",
                          NULL,
                          tfqn.c_str(),
                          "lighting.glsl",
                          "1D-slice-FS.glsl", volumeAccessFunction.c_str(), NULL) ||
     !LoadAndVerifyShader(&m_pProgram2DTransSlice, m_vShaderSearchDirs,
                          "Transfer-VS.glsl",
                          NULL,
                          tfqn.c_str(),
                          "lighting.glsl",
                          "2D-slice-FS.glsl", volumeAccessFunction.c_str(), NULL) ||
     !LoadAndVerifyShader(&m_pProgramMIPSlice, m_vShaderSearchDirs,
                          "Transfer-VS.glsl",
                          NULL,
                          tfqn.c_str(),
                          "lighting.glsl",
                          "MIP-slice-FS.glsl", volumeAccessFunction.c_str(), NULL) ||
     !LoadAndVerifyShader(&m_pProgram1DTransSlice3D, m_vShaderSearchDirs,
                          "SlicesIn3D.glsl",
                          NULL,
                          tfqn.c_str(),
                          "lighting.glsl",
                          "1D-slice-FS.glsl", volumeAccessFunction.c_str(), NULL) ||
     !LoadAndVerifyShader(&m_pProgram2DTransSlice3D, m_vShaderSearchDirs,
                          "SlicesIn3D.glsl",
                          NULL,
                          tfqn.c_str(),
                          "lighting.glsl",
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
     !LoadAndVerifyShader(&m_pProgramAFStereo, m_vShaderSearchDirs,
                          "Transfer-VS.glsl",
                          NULL,
                          "Compose-AF-FS.glsl",
                          NULL)                                              ||
     !LoadAndVerifyShader(&m_pProgramSBSStereo, m_vShaderSearchDirs,
                          "Transfer-VS.glsl",
                          NULL,
                          "Compose-SBS-FS.glsl",
                          NULL)                                              ||
     !LoadAndVerifyShader(&m_pProgramComposeScanlineStereo,
                          m_vShaderSearchDirs,
                          "Transfer-VS.glsl",
                          NULL,
                          "Compose-Scanline-FS.glsl", NULL))

  {
      T_ERROR("Error loading transfer function shaders.");
      return false;
  } else {

    m_pProgramTrans->ConnectTextureID("texColor",0);
    m_pProgramTrans->ConnectTextureID("texDepth",1);

    BindVolumeStringsToTexUnit(m_pProgram1DTransSlice, false);
    m_pProgram1DTransSlice->ConnectTextureID("texTrans1D",1);

    BindVolumeStringsToTexUnit(m_pProgram2DTransSlice, false);
    m_pProgram2DTransSlice->ConnectTextureID("texTrans2D",1);

    BindVolumeStringsToTexUnit(m_pProgram1DTransSlice3D, false);
    m_pProgram1DTransSlice3D->ConnectTextureID("texTrans1D",1);

    BindVolumeStringsToTexUnit(m_pProgram2DTransSlice3D, false);
    m_pProgram2DTransSlice3D->ConnectTextureID("texTrans2D",1);

    BindVolumeStringsToTexUnit(m_pProgramMIPSlice, false);

    m_pProgramTransMIP->ConnectTextureID("texLast",0);
    m_pProgramTransMIP->ConnectTextureID("texTrans1D",1);

    FLOATVECTOR2 vParams = m_FrustumCullingLOD.GetDepthScaleParams();

    m_pProgramIsoCompose->ConnectTextureID("texRayHitPos",0);
    m_pProgramIsoCompose->ConnectTextureID("texRayHitNormal",1);
    m_pProgramIsoCompose->SetUniformVector("vProjParam",vParams.x, vParams.y);

    m_pProgramColorCompose->ConnectTextureID("texRayHitPos",0);
    m_pProgramColorCompose->ConnectTextureID("texRayHitNormal",1);
    m_pProgramColorCompose->SetUniformVector("vProjParam",vParams.x, vParams.y);

    m_pProgramCVCompose->ConnectTextureID("texRayHitPos",0);
    m_pProgramCVCompose->ConnectTextureID("texRayHitNormal",1);
    m_pProgramCVCompose->ConnectTextureID("texRayHitPos2",2);
    m_pProgramCVCompose->ConnectTextureID("texRayHitNormal2",3);
    m_pProgramCVCompose->SetUniformVector("vProjParam",vParams.x, vParams.y);

    m_pProgramComposeAnaglyphs->ConnectTextureID("texLeftEye",0);
    m_pProgramComposeAnaglyphs->ConnectTextureID("texRightEye",1);

    m_pProgramComposeScanlineStereo->ConnectTextureID("texLeftEye",0);
    m_pProgramComposeScanlineStereo->ConnectTextureID("texRightEye",1);

    m_pProgramSBSStereo->ConnectTextureID("texLeftEye",0);
    m_pProgramSBSStereo->ConnectTextureID("texRightEye",1);

    m_pProgramAFStereo->ConnectTextureID("texLeftEye",0);
    m_pProgramAFStereo->ConnectTextureID("texRightEye",1);
  }


  if(!LoadAndVerifyShader(&m_pProgram1DTrans[0], m_vShaderSearchDirs,
                          "GLSBVR-VS.glsl",
                          NULL,
                          tfqn.c_str(), "lighting.glsl",
                          "GLSBVR-1D-FS.glsl",
                          "FTB.glsl",
                          volumeAccessFunction.c_str(), NULL) ||
     !LoadAndVerifyShader(&m_pProgram1DTrans[1], m_vShaderSearchDirs,
                          "GLSBVR-VS.glsl",
                          NULL,
                          tfqn.c_str(),
                          "lighting.glsl", volumeAccessFunction.c_str(),
                          "FTB.glsl",
                          "GLSBVR-1D-light-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgram2DTrans[0], m_vShaderSearchDirs,
                          "GLSBVR-VS.glsl",
                          NULL,
                          tfqn.c_str(),
                          "lighting.glsl",
                          "GLSBVR-2D-FS.glsl",
                          volumeAccessFunction.c_str(), NULL) ||
     !LoadAndVerifyShader(&m_pProgram2DTrans[1], m_vShaderSearchDirs,
                          "GLSBVR-VS.glsl",
                          NULL,
                          tfqn.c_str(),
                          "lighting.glsl", volumeAccessFunction.c_str(),
                          "GLSBVR-2D-light-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramHQMIPRot, m_vShaderSearchDirs,
                          "GLSBVR-VS.glsl",
                          NULL,
                          "GLSBVR-MIP-Rot-FS.glsl",
                          volumeAccessFunction.c_str(), NULL) ||
     !LoadAndVerifyShader(&m_pProgramIso, m_vShaderSearchDirs,
                          "GLSBVR-VS.glsl",
                          NULL,
                          tfqn.c_str(),
                          "lighting.glsl", volumeAccessFunction.c_str(),
                          "GLSBVR-ISO-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramColor, m_vShaderSearchDirs,
                          "GLSBVR-VS.glsl",
                          NULL,
                          tfqn.c_str(),
                          "lighting.glsl", volumeAccessFunction.c_str(),
                          "GLSBVR-Color-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramIsoNoCompose, m_vShaderSearchDirs,
                          "GLSBVR-VS.glsl",
                          NULL,
                          tfqn.c_str(),
                          "lighting.glsl", volumeAccessFunction.c_str(),
                          "GLSBVR-ISO-NC-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramColorNoCompose, m_vShaderSearchDirs,
                          "GLSBVR-VS.glsl",
                          NULL,
                          tfqn.c_str(),
                          "lighting.glsl",
                          "GLSBVR-Color-NC-FS.glsl",
                          volumeAccessFunction.c_str(), NULL) ||
     !LoadAndVerifyShader(&m_pProgramBBox,
                          m_vShaderSearchDirs,
                          "BBox-VS.glsl",
                          NULL,
                          "BBox-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramMeshBTF,
                          m_vShaderSearchDirs,
                          "Mesh-VS.glsl",
                          NULL,
                          "BTF.glsl","Mesh-FS.glsl","lighting.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramMeshFTB,
                          m_vShaderSearchDirs, "Mesh-VS.glsl",
                          NULL,
                          "FTB.glsl","Mesh-FS.glsl","lighting.glsl", NULL))
  {
      Cleanup();

      T_ERROR("Error loading a shader.");
      return false;
  } else {
    BindVolumeStringsToTexUnit(m_pProgram1DTrans[0],false);
    m_pProgram1DTrans[0]->ConnectTextureID("texTrans1D",1);

    BindVolumeStringsToTexUnit(m_pProgram1DTrans[1]);
    m_pProgram1DTrans[1]->ConnectTextureID("texTrans1D",1);

    BindVolumeStringsToTexUnit(m_pProgram2DTrans[0]);
    m_pProgram2DTrans[0]->ConnectTextureID("texTrans2D",1);

    BindVolumeStringsToTexUnit(m_pProgram2DTrans[1]);
    m_pProgram2DTrans[1]->ConnectTextureID("texTrans2D",1);

    BindVolumeStringsToTexUnit(m_pProgramIso);

    BindVolumeStringsToTexUnit(m_pProgramColor);

    BindVolumeStringsToTexUnit(m_pProgramHQMIPRot, false);

    BindVolumeStringsToTexUnit(m_pProgramIsoNoCompose);

    BindVolumeStringsToTexUnit(m_pProgramColorNoCompose, false);

    UpdateLightParamsInShaders();
  }

  return true;
}

void GLSBVR2D::SetDataDepShaderVars() {
  GLRenderer::SetDataDepShaderVars();

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
    m_pProgram1DTrans[0]->Enable();
    m_pProgram1DTrans[0]->SetUniformVector("TFuncBias", bias_scale.first);
    m_pProgram1DTrans[0]->SetUniformVector("fTransScale", bias_scale.second);
  }
}

void GLSBVR2D::SetBrickDepShaderVars(const RenderRegion3D&,
                                     const Brick& currentBrick) {
  FLOATVECTOR3 vStep(1.0f/currentBrick.vVoxelCount.x,
                     1.0f/currentBrick.vVoxelCount.y,
                     1.0f/currentBrick.vVoxelCount.z);

  float fSampleRateModifier = m_fSampleRateModifier /
    (this->decreaseSamplingRateNow ? m_fSampleDecFactor : 1.0f);
  float fStepScale =  1.414213562f/fSampleRateModifier * //1.414213562 = sqrt(2)
    (FLOATVECTOR3(m_pDataset->GetDomainSize()) /
     FLOATVECTOR3(m_pDataset->GetDomainSize(
                              static_cast<size_t>(m_iCurrentLOD))
                              )).maxVal();


  switch (m_eRenderMode) {
    case RM_1DTRANS: {
      GLSLProgram *shader = m_pProgram1DTrans[m_bUseLighting ? 1 : 0];
      shader->SetUniformVector("fStepScale", fStepScale);
      if (m_bUseLighting) {
        m_pProgram1DTrans[1]->SetUniformVector("vVoxelStepsize",
                                               vStep.x, vStep.y, vStep.z);
      }
      break;
    }
    case RM_2DTRANS: {
      GLSLProgram *shader = m_pProgram2DTrans[m_bUseLighting ? 1 : 0];
      shader->SetUniformVector("fStepScale", fStepScale);
      shader->SetUniformVector("vVoxelStepsize", vStep.x, vStep.y, vStep.z);
      break;
    }
    case RM_ISOSURFACE: {
      GLSLProgram *shader;
      if (m_bAvoidSeparateCompositing) {
        shader = (m_pDataset->GetComponentCount() == 1) ?
                 m_pProgramIsoNoCompose : m_pProgramColorNoCompose;
      } else {
        shader = (m_pDataset->GetComponentCount() == 1) ?
                 m_pProgramIso : m_pProgramColor;
      }
      shader->SetUniformVector("vVoxelStepsize", vStep.x, vStep.y, vStep.z);
      break;
    }
    case RM_INVALID: T_ERROR("Invalid rendermode set"); break;
  }
}

void GLSBVR2D::EnableClipPlane(RenderRegion *renderRegion) {
  if(!m_bClipPlaneOn) {
    AbstrRenderer::EnableClipPlane(renderRegion);
    m_SBVRGeogen.EnableClipPlane();
    PLANE<float> plane(m_ClipPlane.Plane());
    m_SBVRGeogen.SetClipPlane(plane);
  }
}

void GLSBVR2D::DisableClipPlane(RenderRegion *renderRegion) {
  if(m_bClipPlaneOn) {
    AbstrRenderer::DisableClipPlane(renderRegion);
    m_SBVRGeogen.DisableClipPlane();
  }
}

void GLSBVR2D::Render3DPreLoop(const RenderRegion3D&) {
  m_SBVRGeogen.SetSamplingModifier(m_fSampleRateModifier / ((this->decreaseSamplingRateNow) ? m_fSampleDecFactor : 1.0f));

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

  m_SBVRGeogen.SetLODData(UINTVECTOR3(m_pDataset->GetDomainSize(static_cast<size_t>(m_iCurrentLOD))));
}

void GLSBVR2D::RenderProxyGeometry() const {
  if (!m_pGLVolume) {
    T_ERROR("Volume data invalid, unable to render.");
    return;
  }

  if (m_bUse3DTexture) RenderProxyGeometry3D(); else RenderProxyGeometry2D();
}

// keeps track of slice geometry.  Meant to be an element in a larger
// data structure; for each slice_geom, we store the texture to use, a
// set of (3D) texture coords, and a set of (3D) vertices.
struct slice_geom {
  UINT32 texid;
  std::vector<float> texcoords;
  std::vector<float> tris;
};

// Iterate through the slices: bind the current slice and adjacent
// slices, and then send the tex and vertex coords down via arrays.
static void submit_vert_arrays(const GLVolume2DTex* vol,
                               const std::vector<slice_geom>& slices,
                               size_t dimension)
{
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    for(std::vector<slice_geom>::const_iterator slice = slices.begin();
        slice != slices.end(); ++slice) {
      // skip empty arrays.
      if(slice->texcoords.empty() || slice->tris.empty()) { continue; }

      vol->Bind(3, static_cast<int>(slice->texid)-1, static_cast<int>(dimension));
      vol->Bind(0, static_cast<int>(slice->texid)+0, static_cast<int>(dimension));
      vol->Bind(2, static_cast<int>(slice->texid)+1, static_cast<int>(dimension));
      glTexCoordPointer(3, GL_FLOAT, 0, &(slice->texcoords[0]));
      glVertexPointer(3, GL_FLOAT, 0, &(slice->tris[0]));
      glDrawArrays(GL_TRIANGLES, 0,
                   static_cast<GLsizei>(slice->tris.size()/3));
    }
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  glDisableClientState(GL_VERTEX_ARRAY);
}

// assignment with move semantics; copies the data from "from" to "to", but in
// doing so clobbers the values in "from".  This can be done considerably more
// efficiently than a simple assignment, however.
void move_slice(struct slice_geom& to, struct slice_geom& from) {
  to.texid = from.texid;
  to.texcoords.swap(from.texcoords);
  to.tris.swap(from.tris);
}

void GLSBVR2D::RenderProxyGeometry2D() const {
  GLVolume2DTex* pGLVolume =  static_cast<GLVolume2DTex*>(m_pGLVolume);

  if (!m_SBVRGeogen.m_vSliceTrianglesX.empty()) {
    // set coordinate shuffle matrix
    float m[16] = {0,0,1,0,
                   0,1,0,0,
                   1,0,0,0,
                   0,0,0,1};
    glActiveTextureARB(GL_TEXTURE0);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glLoadMatrixf(m);

    std::vector<slice_geom> slices;
    slices.reserve(pGLVolume->GetSizeX());

    int iLastTexID = -1;
    size_t slc_idx = 0; // index into 'slices'.
    slice_geom geom;
    // Experimentally, I observed that the max we see is a little above
    // 1800 entries (i.e. 600 verts).  So this should make sure we do
    // all our allocations up front.
    geom.texcoords.reserve(2048); geom.tris.reserve(2048);
    for(size_t i=0; i < m_SBVRGeogen.m_vSliceTrianglesX.size(); ++i) {
      const float depth = m_SBVRGeogen.m_vSliceTrianglesX[i].m_vVertexData.x;
      const unsigned iCurrentTexID = static_cast<unsigned>(depth*(pGLVolume->GetSizeX()));
      if(static_cast<int>(iCurrentTexID) != iLastTexID) { // finished a slice
        // copy the current geom over ...
        slice_geom g;
        slices.push_back(g);
        // note that move_slice will clear geom.texcoords/tris, too!
        move_slice(slices[slc_idx], geom);
        slices[slc_idx++].texid = iCurrentTexID;
        // .. and move on to the next slice.
        iLastTexID = iCurrentTexID;
      }
      const float fraction = depth*pGLVolume->GetSizeX() - iCurrentTexID;
      geom.texcoords.push_back(m_SBVRGeogen.m_vSliceTrianglesX[i].m_vVertexData.z);
      geom.texcoords.push_back(m_SBVRGeogen.m_vSliceTrianglesX[i].m_vVertexData.y);
      geom.texcoords.push_back(fraction);
      geom.tris.push_back(m_SBVRGeogen.m_vSliceTrianglesX[i].m_vPos.x);
      geom.tris.push_back(m_SBVRGeogen.m_vSliceTrianglesX[i].m_vPos.y);
      geom.tris.push_back(m_SBVRGeogen.m_vSliceTrianglesX[i].m_vPos.z);
    }
    submit_vert_arrays(pGLVolume, slices, 0);
  }
  if (!m_SBVRGeogen.m_vSliceTrianglesY.empty()) {
    // set coordinate shuffle matrix
    float m[16] = {1,0,0,0,
                   0,0,1,0,
                   0,1,0,0,
                   0,0,0,1};
    glActiveTextureARB(GL_TEXTURE0);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glLoadMatrixf(m);

    int iLastTexID = -1;
    std::vector<slice_geom> slices;
    slices.reserve(pGLVolume->GetSizeX());

    size_t slc_idx = 0; // index into 'slices'.
    slice_geom geom;
    geom.texcoords.reserve(2048); geom.tris.reserve(2048);
    for (size_t i = 0;i<m_SBVRGeogen.m_vSliceTrianglesY.size();i++) {
      const float depth = m_SBVRGeogen.m_vSliceTrianglesY[i].m_vVertexData.y;
      const unsigned iCurrentTexID = static_cast<unsigned>(depth*(pGLVolume->GetSizeY()));
      if(static_cast<int>(iCurrentTexID) != iLastTexID) {
        // we finished a slice.  copy the current geom over ...
        slice_geom g;
        slices.push_back(g);
        // note that move_slice will clear geom.texcoords/tris, too!
        move_slice(slices[slc_idx], geom);
        slices[slc_idx++].texid = iCurrentTexID;
        // .. and move on to the next slice.
        iLastTexID = iCurrentTexID;
      }
      const float fraction = depth*pGLVolume->GetSizeY() - iCurrentTexID;

      geom.texcoords.push_back(m_SBVRGeogen.m_vSliceTrianglesY[i].m_vVertexData.x);
      geom.texcoords.push_back(m_SBVRGeogen.m_vSliceTrianglesY[i].m_vVertexData.z);
      geom.texcoords.push_back(fraction);
      geom.tris.push_back(m_SBVRGeogen.m_vSliceTrianglesY[i].m_vPos.x);
      geom.tris.push_back(m_SBVRGeogen.m_vSliceTrianglesY[i].m_vPos.y);
      geom.tris.push_back(m_SBVRGeogen.m_vSliceTrianglesY[i].m_vPos.z);
    }
    submit_vert_arrays(pGLVolume, slices, 1);
  }
  if (!m_SBVRGeogen.m_vSliceTrianglesZ.empty()) {
    // set coordinate shuffle matrix
    glActiveTextureARB(GL_TEXTURE0);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();

    int iLastTexID = -1;
    std::vector<slice_geom> slices;
    slices.reserve(pGLVolume->GetSizeX());

    size_t slc_idx = 0; // index into 'slices'.
    slice_geom geom;
    geom.texcoords.reserve(2048); geom.tris.reserve(2048);
    for (size_t i = 0;i<m_SBVRGeogen.m_vSliceTrianglesZ.size();i++) {
      const float depth = m_SBVRGeogen.m_vSliceTrianglesZ[i].m_vVertexData.z;
      const unsigned iCurrentTexID = static_cast<unsigned>(depth*(pGLVolume->GetSizeZ()));
      if(static_cast<int>(iCurrentTexID) != iLastTexID) {
        // we finished a slice.  copy the current geom over ...
        slice_geom g;
        slices.push_back(g);
        // note that move_slice will clear geom.texcoords/tris, too!
        move_slice(slices[slc_idx], geom);
        slices[slc_idx++].texid = iCurrentTexID;
        // .. and move on to the next slice.
        iLastTexID = iCurrentTexID;
      }
      const float fraction = depth*pGLVolume->GetSizeZ() - iCurrentTexID;

      geom.texcoords.push_back(m_SBVRGeogen.m_vSliceTrianglesZ[i].m_vVertexData.x);
      geom.texcoords.push_back(m_SBVRGeogen.m_vSliceTrianglesZ[i].m_vVertexData.y);
      geom.texcoords.push_back(fraction);
      geom.tris.push_back(m_SBVRGeogen.m_vSliceTrianglesZ[i].m_vPos.x);
      geom.tris.push_back(m_SBVRGeogen.m_vSliceTrianglesZ[i].m_vPos.y);
      geom.tris.push_back(m_SBVRGeogen.m_vSliceTrianglesZ[i].m_vPos.z);
    }
    submit_vert_arrays(pGLVolume, slices, 2);
  }
}

void GLSBVR2D::RenderProxyGeometry3D() const {
  if(!m_SBVRGeogen.m_vSliceTrianglesX.empty()) {
    glBegin(GL_TRIANGLES);
      for (size_t i = 0;i<m_SBVRGeogen.m_vSliceTrianglesX.size();i++) {
        glTexCoord3f(m_SBVRGeogen.m_vSliceTrianglesX[i].m_vVertexData.x,
                     m_SBVRGeogen.m_vSliceTrianglesX[i].m_vVertexData.y,
                     m_SBVRGeogen.m_vSliceTrianglesX[i].m_vVertexData.z);
        glVertex3f(m_SBVRGeogen.m_vSliceTrianglesX[i].m_vPos.x,
                   m_SBVRGeogen.m_vSliceTrianglesX[i].m_vPos.y,
                   m_SBVRGeogen.m_vSliceTrianglesX[i].m_vPos.z);
      }
    glEnd();
  }
  if(!m_SBVRGeogen.m_vSliceTrianglesY.empty()) {
    glBegin(GL_TRIANGLES);
      for (size_t i = 0;i<m_SBVRGeogen.m_vSliceTrianglesY.size();i++) {
        glTexCoord3f(m_SBVRGeogen.m_vSliceTrianglesY[i].m_vVertexData.x,
                     m_SBVRGeogen.m_vSliceTrianglesY[i].m_vVertexData.y,
                     m_SBVRGeogen.m_vSliceTrianglesY[i].m_vVertexData.z);
        glVertex3f(m_SBVRGeogen.m_vSliceTrianglesY[i].m_vPos.x,
                   m_SBVRGeogen.m_vSliceTrianglesY[i].m_vPos.y,
                   m_SBVRGeogen.m_vSliceTrianglesY[i].m_vPos.z);
      }
    glEnd();
  }
  if(!m_SBVRGeogen.m_vSliceTrianglesZ.empty()) {
    glBegin(GL_TRIANGLES);
      for (size_t i = 0;i<m_SBVRGeogen.m_vSliceTrianglesZ.size();i++) {
        glTexCoord3f(m_SBVRGeogen.m_vSliceTrianglesZ[i].m_vVertexData.x,
                     m_SBVRGeogen.m_vSliceTrianglesZ[i].m_vVertexData.y,
                     m_SBVRGeogen.m_vSliceTrianglesZ[i].m_vVertexData.z);
        glVertex3f(m_SBVRGeogen.m_vSliceTrianglesZ[i].m_vPos.x,
                   m_SBVRGeogen.m_vSliceTrianglesZ[i].m_vPos.y,
                   m_SBVRGeogen.m_vSliceTrianglesZ[i].m_vPos.z);
      }
    glEnd();
  }
}

void GLSBVR2D::Render3DInLoop(const RenderRegion3D& renderRegion,
                              size_t iCurrentBrick, int iStereoID) {
  const Brick& b = (iStereoID == 0) ? m_vCurrentBrickList[iCurrentBrick] : m_vLeftEyeBrickList[iCurrentBrick];

  if (m_iBricksRenderedInThisSubFrame == 0 && !m_bAvoidSeparateCompositing && m_eRenderMode == RM_ISOSURFACE){
    m_TargetBinder.Bind(m_pFBOIsoHit[iStereoID], 0, m_pFBOIsoHit[iStereoID], 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (m_bDoClearView) {
      m_TargetBinder.Bind(m_pFBOCVHit[iStereoID], 0, m_pFBOCVHit[iStereoID], 1);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
  }

  if (b.bIsEmpty) {
    return;
  }


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

  m_SBVRGeogen.ComputeGeometry(b.bIsEmpty);

  if (! m_bAvoidSeparateCompositing && m_eRenderMode == RM_ISOSURFACE) {
    glDisable(GL_BLEND);
    GLSLProgram* shader = (m_pDataset->GetComponentCount() == 1) ? m_pProgramIso : m_pProgramColor;

    m_TargetBinder.Bind(m_pFBOIsoHit[iStereoID], 0, m_pFBOIsoHit[iStereoID], 1);

    shader->Enable();
    SetBrickDepShaderVars(renderRegion, b);
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
    m_TargetBinder.Bind(m_pFBO3DImageCurrent[iStereoID]);

    glDepthMask(GL_FALSE);
    SetBrickDepShaderVars(renderRegion, b);
    RenderProxyGeometry();
    glDepthMask(GL_TRUE);
  }
  m_TargetBinder.Unbind();
}


void GLSBVR2D::Render3DPostLoop() {
  GLRenderer::Render3DPostLoop();
  glDisable(GL_BLEND);
}

void GLSBVR2D::RenderHQMIPPreLoop(RenderRegion2D& region) {
  GLRenderer::RenderHQMIPPreLoop(region);
  m_pProgramHQMIPRot->Enable();

  glBlendFunc(GL_ONE, GL_ONE);
  glBlendEquation(GL_MAX);
  glEnable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
}

void GLSBVR2D::RenderHQMIPInLoop(const RenderRegion2D&, const Brick& b) {
  m_SBVRGeogen.SetBrickData(b.vExtension, b.vVoxelCount, b.vTexcoordsMin, b.vTexcoordsMax);
  m_SBVRGeogen.SetBrickTrans(b.vCenter);

  if (m_bOrthoView) {
    // here we push the volume back by one to make sure
    // the viewing direction computation in the geometry generator
    // works
    FLOATMATRIX4 m;
    m.Translation(0,0,1);
    m_SBVRGeogen.SetView(m);
  } else {
    m_SBVRGeogen.SetView(m_mView[0]);
  }

  m_SBVRGeogen.SetWorld(m_maMIPRotation);
  m_SBVRGeogen.ComputeGeometry(b.bIsEmpty);

  RenderProxyGeometry();
}

bool GLSBVR2D::LoadDataset(const string& strFilename) {
  if (GLRenderer::LoadDataset(strFilename)) {
    UINTVECTOR3    vSize = UINTVECTOR3(m_pDataset->GetDomainSize());
    FLOATVECTOR3 vAspect = FLOATVECTOR3(m_pDataset->GetScale());
    vAspect /= vAspect.maxVal();

    m_SBVRGeogen.SetVolumeData(vAspect, vSize);
    return true;
  } else return false;
}

void GLSBVR2D::ComposeSurfaceImage(RenderRegion& renderRegion, int iStereoID) {
  if (!m_bAvoidSeparateCompositing)
    GLRenderer::ComposeSurfaceImage(renderRegion, iStereoID);
}


void GLSBVR2D::UpdateLightParamsInShaders() {
  GLRenderer::UpdateLightParamsInShaders();

  FLOATVECTOR3 a = m_cAmbient.xyz()*m_cAmbient.w;
  FLOATVECTOR3 d = m_cDiffuse.xyz()*m_cDiffuse.w;
  FLOATVECTOR3 s = m_cSpecular.xyz()*m_cSpecular.w;

  FLOATVECTOR3 scale = 1.0f/FLOATVECTOR3(m_pDataset->GetScale());

  m_pProgramIsoNoCompose->Enable();
  m_pProgramIsoNoCompose->SetUniformVector("vLightAmbient",a.x,a.y,a.z);
  m_pProgramIsoNoCompose->SetUniformVector("vLightDiffuse",d.x,d.y,d.z);
  m_pProgramIsoNoCompose->SetUniformVector("vLightSpecular",s.x,s.y,s.z);
  m_pProgramIsoNoCompose->SetUniformVector("vLightDir",m_vLightDir.x,
                                                       m_vLightDir.y,
                                                       m_vLightDir.z);
  m_pProgramIsoNoCompose->SetUniformVector("vDomainScale",scale.x,
                                                          scale.y,
                                                          scale.z);
  m_pProgramColorNoCompose->Enable();
  m_pProgramColorNoCompose->SetUniformVector("vLightAmbient",a.x,a.y,a.z);
  m_pProgramColorNoCompose->SetUniformVector("vLightDir",m_vLightDir.x,
                                                         m_vLightDir.y,
                                                         m_vLightDir.z);
  m_pProgramColorNoCompose->SetUniformVector("vDomainScale",scale.x,
                                                            scale.y,
                                                            scale.z);
}

bool GLSBVR2D::BindVolumeTex(const BrickKey& bkey,
                             const UINT64 iIntraFrameCounter) {

  if (m_bUse3DTexture) return GLRenderer::BindVolumeTex(bkey,iIntraFrameCounter);

  m_pGLVolume = m_pMasterController->MemMan()->GetVolume(m_pDataset, bkey,
                                            m_bUseOnlyPowerOfTwo,
                                            m_bDownSampleTo8Bits,
                                            m_bDisableBorder,
                                            true,
                                            iIntraFrameCounter,
                                            m_iFrameCounter);
  if(m_pGLVolume) {
    return true;
  } else {
    return false;
  }
}

bool GLSBVR2D::IsVolumeResident(const BrickKey& key) {
  if (m_bUse3DTexture) return GLRenderer::IsVolumeResident(key);

  return m_pMasterController->MemMan()->IsResident(m_pDataset, key,
                                                m_bUseOnlyPowerOfTwo,
                                                m_bDownSampleTo8Bits,
                                                m_bDisableBorder,
                                                true);
}

void GLSBVR2D::RenderSlice(const RenderRegion2D& region, double fSliceIndex,
                             FLOATVECTOR3 vMinCoords, FLOATVECTOR3 vMaxCoords,
                             DOUBLEVECTOR3 vAspectRatio,
                             DOUBLEVECTOR2 vWinAspectRatio) {
  GLVolume2DTex* pGLVolume =  static_cast<GLVolume2DTex*>(m_pGLVolume);
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

      int iCurrentTexID =  int(fSliceIndex*pGLVolume->GetSizeY());
      pGLVolume->Bind(0, iCurrentTexID, 1);
      pGLVolume->Bind(2, iCurrentTexID+1, 1);
      float fraction = float(fSliceIndex*pGLVolume->GetSizeY() - iCurrentTexID);

      DOUBLEVECTOR2 v2AspectRatio = vAspectRatio.xz()*DOUBLEVECTOR2(vWinAspectRatio);
      v2AspectRatio = v2AspectRatio / v2AspectRatio.maxVal();
      glBegin(GL_QUADS);
      glTexCoord3d(vMinCoords.x,vMaxCoords.z,fraction);
      glVertex3d(-1.0f*v2AspectRatio.x, +1.0f*v2AspectRatio.y, -0.5f);
      glTexCoord3d(vMaxCoords.x,vMaxCoords.z,fraction);
      glVertex3d(+1.0f*v2AspectRatio.x, +1.0f*v2AspectRatio.y, -0.5f);
      glTexCoord3d(vMaxCoords.x,vMinCoords.z,fraction);
      glVertex3d(+1.0f*v2AspectRatio.x, -1.0f*v2AspectRatio.y, -0.5f);
      glTexCoord3d(vMinCoords.x,vMinCoords.z,fraction);
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

      int iCurrentTexID =  int(fSliceIndex*pGLVolume->GetSizeZ());
      pGLVolume->Bind(0, iCurrentTexID, 2);
      pGLVolume->Bind(2, iCurrentTexID+1, 2);
      float fraction = float(fSliceIndex*pGLVolume->GetSizeZ() - iCurrentTexID);

      glBegin(GL_QUADS);
      glTexCoord3d(vMinCoords.x,vMaxCoords.y,fraction);
      glVertex3d(-1.0f*v2AspectRatio.x, +1.0f*v2AspectRatio.y, -0.5f);
      glTexCoord3d(vMaxCoords.x,vMaxCoords.y,fraction);
      glVertex3d(+1.0f*v2AspectRatio.x, +1.0f*v2AspectRatio.y, -0.5f);
      glTexCoord3d(vMaxCoords.x,vMinCoords.y,fraction);
      glVertex3d(+1.0f*v2AspectRatio.x, -1.0f*v2AspectRatio.y, -0.5f);
      glTexCoord3d(vMinCoords.x,vMinCoords.y,fraction);
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

      int iCurrentTexID =  int(fSliceIndex*pGLVolume->GetSizeX());
      pGLVolume->Bind(0, iCurrentTexID, 0);
      pGLVolume->Bind(2, iCurrentTexID+1, 0);
      float fraction = float(fSliceIndex*pGLVolume->GetSizeX() - iCurrentTexID);

      DOUBLEVECTOR2 v2AspectRatio = vAspectRatio.yz()*DOUBLEVECTOR2(vWinAspectRatio);
      v2AspectRatio = v2AspectRatio / v2AspectRatio.maxVal();
      glBegin(GL_QUADS);
      glTexCoord3d(vMaxCoords.z,vMinCoords.y,fraction);
      glVertex3d(-1.0f*v2AspectRatio.x, +1.0f*v2AspectRatio.y, -0.5f);
      glTexCoord3d(vMaxCoords.z,vMaxCoords.y,fraction);
      glVertex3d(+1.0f*v2AspectRatio.x, +1.0f*v2AspectRatio.y, -0.5f);
      glTexCoord3d(vMinCoords.z,vMaxCoords.y,fraction);
      glVertex3d(+1.0f*v2AspectRatio.x, -1.0f*v2AspectRatio.y, -0.5f);
      glTexCoord3d(vMinCoords.z,vMinCoords.y,fraction);
      glVertex3d(-1.0f*v2AspectRatio.x, -1.0f*v2AspectRatio.y, -0.5f);
      glEnd();
      break;
    }
  default :  T_ERROR("Invalid windowmode set"); break;
  }
}
