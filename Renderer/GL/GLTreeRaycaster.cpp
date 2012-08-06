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

#include "IO/uvfDataset.h"

#include "GLHashTable.h"
#include "GLVolumePool.h"
#include "GLVBO.h"

using std::bind;
using namespace std::placeholders;
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
  m_pNearPlaneQuad(NULL),
  m_pBBoxVBO(NULL),
  m_pProgramRenderFrontFaces(NULL),
  m_pProgramRenderFrontFacesNearPlane(NULL),
  m_pProgramRayCast1D(NULL),
  m_pToCDataset(NULL),
  m_bConverged(true),
  m_bNoRCClipplanes(bNoRCClipplanes)
{
  // a member of the parent class, henced it's initialized here
  m_bSupportsMeshes = false;

  
  std::fill(m_pFBOResumeColor.begin(), m_pFBOResumeColor.end(), (GLFBOTex*)NULL);
  std::fill(m_pFBOResumeColorNext.begin(), m_pFBOResumeColorNext.end(), (GLFBOTex*)NULL);
  std::fill(m_pFBORayStart.begin(), m_pFBORayStart.end(), (GLFBOTex*)NULL);
  std::fill(m_pFBORayStartNext.begin(), m_pFBORayStartNext.end(), (GLFBOTex*)NULL);
}


bool GLTreeRaycaster::CreateVolumePool() {

  // todo: make this configurable
  const UINTVECTOR3 poolSize = UINTVECTOR3(512, 512, 512);

  GLenum glInternalformat=0;
  GLenum glFormat=0;
  GLenum glType=0;

  uint64_t iBitWidth  = m_pToCDataset->GetBitWidth();
  uint64_t iCompCount = m_pToCDataset->GetComponentCount();


  switch (iCompCount) {
    case 1 : glFormat = GL_LUMINANCE; break;
    case 3 : glFormat = GL_RGB; break;
    case 4 : glFormat = GL_RGBA; break;
    default : T_ERROR("Invalid Component Count"); return false;
  }

  switch (iBitWidth) {
    case 8 :  {
        glType = GL_UNSIGNED_BYTE;
        switch (iCompCount) {
          case 1 : glInternalformat = GL_LUMINANCE8; break;
          case 3 : glInternalformat = GL_RGB8; break;
          case 4 : glInternalformat = GL_RGBA8; break;
          default : T_ERROR("Invalid Component Count"); return false;
        }
      } 
      break;
    case 16 :  {
        glType = GL_UNSIGNED_SHORT;
        switch (iCompCount) {
          case 1 : glInternalformat = GL_LUMINANCE16; break;
          case 3 : glInternalformat = GL_RGB16; break;
          case 4 : glInternalformat = GL_RGBA16; break;
          default : T_ERROR("Invalid Component Count"); return false;
        }
      } 
      break;
    case 32 :  {
        glType = GL_FLOAT;
        switch (iCompCount) {
          case 1 : glInternalformat = GL_LUMINANCE32F_ARB; break;
          case 3 : glInternalformat = GL_RGB32F; break;
          case 4 : glInternalformat = GL_RGBA32F; break;
          default : T_ERROR("Invalid Component Count"); return false;
        }
      } 
      break;
    default : T_ERROR("Invalid bit width"); return false;
  }


  m_pVolumePool = new GLVolumePool(poolSize, UINTVECTOR3(m_pToCDataset->GetMaxUsedBrickSizes()), 
                                   m_pToCDataset->GetBrickOverlapSize(), 
                                   UINTVECTOR3(m_pToCDataset->GetDomainSize()), glInternalformat,
                                   glFormat, glType);

  return true;
}

bool GLTreeRaycaster::LoadDataset(const string& strFilename) {
  if(!AbstrRenderer::LoadDataset(strFilename)) {
    return false;
  }

  UVFDataset* pUVFDataset = dynamic_cast<UVFDataset*>(m_pDataset);
  if (!m_pDataset) {
    T_ERROR("'Currently, this renderer works only with UVF datasets.");
    return false;
  }

  if (!pUVFDataset->IsTOCBlock()) {
    T_ERROR("'Currently, this renderer works only with UVF datasets in ExtendedOctree format.");
    return false;
  }

  m_pToCDataset = pUVFDataset;

  return true;
}
GLTreeRaycaster::~GLTreeRaycaster() {
  delete m_pglHashTable; m_pglHashTable = NULL;
  delete  m_pVolumePool; m_pVolumePool = NULL;
}


void GLTreeRaycaster::CleanupShaders() {
  GLRenderer::CleanupShaders();

  CleanupShader(&m_pProgramRenderFrontFaces);
  CleanupShader(&m_pProgramRenderFrontFacesNearPlane);
  CleanupShader(&m_pProgramRayCast1D);
}

struct {
  void operator() (GLFBOTex*& fbo) {
    if (fbo){
      Controller::Instance().MemMan()->FreeFBO(fbo); 
      fbo = NULL;
    }
  }
} deleteFBO;

void GLTreeRaycaster::Cleanup() {
  GLRenderer::Cleanup();

  std::for_each(m_pFBORayStart.begin(),        m_pFBORayStart.end(),        deleteFBO);
  std::for_each(m_pFBORayStartNext.begin(),    m_pFBORayStartNext.end(),    deleteFBO);
  std::for_each(m_pFBOResumeColor.begin(),     m_pFBOResumeColor.end(),     deleteFBO);
  std::for_each(m_pFBOResumeColorNext.begin(), m_pFBOResumeColorNext.end(), deleteFBO);

  delete m_pBBoxVBO;
  m_pBBoxVBO = NULL;
  delete m_pNearPlaneQuad;
  m_pNearPlaneQuad = NULL;

  if (m_pglHashTable) {
    m_pglHashTable->FreeGL();
    delete m_pglHashTable;
    m_pglHashTable = NULL;
  }
}


void recreateFBO(GLFBOTex*& fbo, std::shared_ptr<Context> pContext,
                 const UINTVECTOR2& ws, GLenum intformat, 
                 GLenum format, GLenum type) {
    if (fbo)
      Controller::Instance().MemMan()->FreeFBO(fbo); 

    fbo = Controller::Instance().MemMan()->GetFBO(GL_NEAREST, GL_NEAREST, GL_CLAMP, ws.x,
      ws.y, intformat, format, type, pContext->GetShareGroupID(), false);
}

void GLTreeRaycaster::CreateOffscreenBuffers() {
  GLRenderer::CreateOffscreenBuffers();

  GLenum intformat, type;
  switch (m_eBlendPrecision) {
      case BP_8BIT  : intformat = GL_RGBA8;
                      type = GL_UNSIGNED_BYTE;
                      break;
      case BP_16BIT : intformat = m_texFormat16;
                      type = GL_HALF_FLOAT;
                      break;
      case BP_32BIT : intformat = m_texFormat32;
                      type = GL_FLOAT;
                      break;
      default       : MESSAGE("Invalid Blending Precision");
                      return;
    }

  if (m_vWinSize.area() > 0) {
    for_each(m_pFBORayStart.begin(), m_pFBORayStart.end(), 
      bind(recreateFBO, _1, m_pContext ,m_vWinSize, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT));
    for_each(m_pFBORayStartNext.begin(), m_pFBORayStartNext.end(), 
      bind(recreateFBO, _1, m_pContext ,m_vWinSize, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT));
    for_each(m_pFBOResumeColor.begin(), m_pFBOResumeColor.end(), 
      bind(recreateFBO, _1, m_pContext ,m_vWinSize, intformat, GL_RGBA, type));
    for_each(m_pFBOResumeColorNext.begin(), m_pFBOResumeColorNext.end(), 
      bind(recreateFBO, _1, m_pContext ,m_vWinSize, intformat, GL_RGBA, type));
  }
}

bool GLTreeRaycaster::Initialize(std::shared_ptr<Context> ctx) {
  if (!GLRenderer::Initialize(ctx)) {
    T_ERROR("Error in parent call -> aborting");
    return false;
  }

  m_pglHashTable = new GLHashTable(UINTVECTOR3( m_pToCDataset->GetBrickLayout(0, 0)) );
  m_pglHashTable->InitGL();

  if (!CreateVolumePool()) return false;

  // now that we've created the hastable and the volume pool
  // we can load the rest of the shader that depend on those
  std::vector<std::string> vs, fs;
  vs.push_back(FindFileInDirs("GLTreeRaycaster-VS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-1D-FS.glsl", m_vShaderSearchDirs, false));
  ShaderDescriptor sd(vs, fs);
  sd.AddFragmentShaderString(m_pVolumePool->GetShaderFragment(3,4));
  sd.AddFragmentShaderString(m_pglHashTable->GetShaderFragment(5));
  m_pProgramRayCast1D = m_pMasterController->MemMan()->GetGLSLProgram(sd, m_pContext->GetShareGroupID());


  if (!m_pProgramRayCast1D || !m_pProgramRayCast1D->IsValid())
  {
      Cleanup();
      T_ERROR("Error loading a shader.");
      return false;
  } 

  // init near plane vbo
  m_pNearPlaneQuad = new GLVBO();
  std::vector<FLOATVECTOR3> posData;
  posData.push_back(FLOATVECTOR3(-1.0f,  1.0f, -0.5f));
  posData.push_back(FLOATVECTOR3( 1.0f,  1.0f, -0.5f));
  posData.push_back(FLOATVECTOR3( 1.0f, -1.0f, -0.5f));
  posData.push_back(FLOATVECTOR3(-1.0f, -1.0f, -0.5f));
  m_pNearPlaneQuad->AddVertexData(posData);

  // init bbox vbo
  FLOATVECTOR3 vCenter, vExtend;
  GetVolumeAABB(vCenter, vExtend);

  FLOATVECTOR3 vMinPoint, vMaxPoint;
  vMinPoint = (vCenter - vExtend/2.0);
  vMaxPoint = (vCenter + vExtend/2.0);

  m_pBBoxVBO = new GLVBO();
  posData.clear();
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
  m_pBBoxVBO->AddVertexData(posData);

  return true;
}

bool GLTreeRaycaster::LoadShaders() {
  if (!GLRenderer::LoadShaders()) {
    T_ERROR("Error in parent call -> aborting");
    return false;
  }


  if(!LoadAndVerifyShader(&m_pProgramRenderFrontFaces, m_vShaderSearchDirs,
                          "GLTreeRaycaster-entry-VS.glsl",
                          NULL,
                          "GLTreeRaycaster-frontfaces-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramRenderFrontFacesNearPlane, m_vShaderSearchDirs,
                          "GLTreeRaycaster-NearPlane-VS.glsl",
                          NULL,
                          "GLTreeRaycaster-frontfaces-FS.glsl", NULL) )
  {
      Cleanup();
      T_ERROR("Error loading a shader.");
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

void GLTreeRaycaster::RenderBox(const RenderRegion& renderRegion, STATE_CULL cull, 
                                EStereoID eStereoID, GLSLProgram* shader) const  {
  
  m_pContext->GetStateManager()->SetCullState(cull);

  shader->Enable();
  shader->Set("mModelViewProjection", renderRegion.modelView[size_t(eStereoID)]*m_mProjection[size_t(eStereoID)], 4, false); 

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
                                                      EStereoID eStereoID) const {

  FLOATVECTOR3 vCenter, vExtend;
  GetVolumeAABB(vCenter, vExtend);

  FLOATMATRIX4 mTrans, mScale, mNormalize;

  mTrans.Translation(-vCenter);
  mScale.Scaling(1.0f/vExtend);
  mNormalize.Translation(0.5f, 0.5f, 0.5f);

  return renderRegion.modelView[size_t(eStereoID)].inverse() * mTrans * mScale * mNormalize;
}

bool GLTreeRaycaster::Continue3DDraw() {
  return !m_bConverged;
}

void GLTreeRaycaster::FillRayEntryBuffer(RenderRegion3D& rr, EStereoID eStereoID) {
  // bind output render target (DEBUG, should be front face texture)
  m_TargetBinder.Bind(m_pFBORayStart[size_t(eStereoID)]);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // render nearplane into buffer
  GPUState localState = m_BaseState;
  localState.enableBlend = false;
  localState.depthMask = false;
  localState.enableDepthTest  = false;
  m_pContext->GetStateManager()->Apply(localState);  

  m_pProgramRenderFrontFacesNearPlane->Enable();
  m_pProgramRenderFrontFacesNearPlane->Set("mEyeToModel", ComputeEyeToModelMatrix(rr, eStereoID), 4, false); 
  m_pProgramRenderFrontFacesNearPlane->Set("mInvProjection", m_mProjection[size_t(eStereoID)].inverse(), 4, false); 
  
  m_pNearPlaneQuad->Bind();
  m_pNearPlaneQuad->Draw(GL_QUADS);
  m_pNearPlaneQuad->UnBind();
  
  // render bbox's front faces into buffer
  m_pContext->GetStateManager()->SetEnableCullFace(true);
  m_pContext->GetStateManager()->SetCullState(CULL_BACK);

  m_pProgramRenderFrontFaces->Enable();
  m_pProgramRenderFrontFaces->Set("mEyeToModel", ComputeEyeToModelMatrix(rr, eStereoID), 4, false); 
  m_pProgramRenderFrontFaces->Set("mModelView", rr.modelView[size_t(eStereoID)], 4, false); 
  m_pProgramRenderFrontFaces->Set("mModelViewProjection", rr.modelView[size_t(eStereoID)]*m_mProjection[size_t(eStereoID)], 4, false); 

  m_pBBoxVBO->Bind();
  m_pBBoxVBO->Draw(GL_QUADS);
  m_pBBoxVBO->UnBind();
}

void GLTreeRaycaster::Raycast(RenderRegion3D& rr, EStereoID eStereoID) {
  m_TargetBinder.Bind(m_pFBO3DImageCurrent[size_t(eStereoID)],
                      m_pFBOResumeColorNext[size_t(eStereoID)],
                      m_pFBORayStartNext[size_t(eStereoID)]);

  m_pFBORayStart[size_t(eStereoID)]->Read(0);
  m_pFBOResumeColor[size_t(eStereoID)]->Read(1);
  m_pVolumePool->BindTexures(); // bound to 2 and 3

  // todo: use m_eRenderMode, code below is only for RM_1DTRANS
  m_p1DTransTex->Bind(4);

  m_pglHashTable->Enable(); // bound to 5

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  RenderBox(rr, CULL_FRONT, eStereoID, m_pProgramRayCast1D);

  m_pFBORayStart[size_t(eStereoID)]->FinishRead();
  m_pFBOResumeColor[size_t(eStereoID)]->FinishRead();

  // swap current and next resume color
  std::swap(m_pFBOResumeColorNext[size_t(eStereoID)], m_pFBOResumeColor[size_t(eStereoID)]);
  std::swap(m_pFBORayStart[size_t(eStereoID)], m_pFBORayStartNext[size_t(eStereoID)]);
}

bool GLTreeRaycaster::CheckForRedraw() {
  // can't draw to a size zero window.
  if (m_vWinSize.area() == 0) return false; 

  // if we have not converged yet redraw immediately
  // TODO: after finished implementing and debugging
  // using the m_iCheckCounter here similar to 
  // AbstrRenderer::CheckForRedraw() 
  if (!m_bConverged) return true; 

  for (size_t i=0; i < renderRegions.size(); ++i) {
    const RenderRegion* region = renderRegions[i];
    if (region->isBlank) return true;
  }

  return false;
}

static BrickKey HashValueToBrickKey(const UINTVECTOR4& hash, size_t timestep, const UVFDataset* pToCDataset) {
  UINT64VECTOR3 layout = pToCDataset->GetBrickLayout(hash.w, timestep);

  return BrickKey(timestep, hash.w, hash.x*layout.x*layout.y+
                                    hash.y*layout.x+
                                    hash.z);
}

void GLTreeRaycaster::UpdateVolumePool(std::vector<UINTVECTOR4>& hash) {
  // TODO: this entire function should be part of the GPU mem-manager

  // if loading a brick for the first time, prepare upload mem
  if (m_vUploadMem.empty()) {
    const UINTVECTOR3 vSize = m_pToCDataset->GetMaxBrickSize();
    const uint64_t iByteWidth = m_pToCDataset->GetBitWidth()/8;
    const uint64_t iCompCount = m_pToCDataset->GetComponentCount();
    const uint64_t iBrickSize = vSize.volume() * iByteWidth * iCompCount;
    m_vUploadMem.resize(iBrickSize);
  }

  // now iterate over the missing bricks and upload them to the GPU
  // todo: consider batching this if it turns out to make a difference
  //       from submitting each brick seperately
  for (auto missingBrick = hash.begin();missingBrick<hash.end(); ++missingBrick) {
    const BrickKey bkey = HashValueToBrickKey(*missingBrick, m_iTimestep, m_pToCDataset);
    const UINTVECTOR3 vVoxelSize = m_pToCDataset->GetBrickVoxelCounts(bkey);

    m_pToCDataset->GetBrick(bkey, m_vUploadMem);
    m_pVolumePool->UploadBrick(BrickElemInfo(*missingBrick, vVoxelSize), &m_vUploadMem[0]);
  }
  
  if (!hash.empty())
    m_pVolumePool->UploadMetaData();
}

bool GLTreeRaycaster::Render3DRegion(RenderRegion3D& rr) {
  // for DEBUG render always MONO
  EStereoID eStereoID = SI_LEFT_OR_MONO;

  // reset state
  GPUState localState = m_BaseState;
  localState.enableBlend = false;
  m_pContext->GetStateManager()->Apply(localState);
  
  // prepare a new view
  if (rr.isBlank) {
    // compute new ray start
    FillRayEntryBuffer(rr,eStereoID);
  }

  // clear hastable
  m_pglHashTable->ClearData();

  // do raycasting
  Raycast(rr, eStereoID);

  // done rendering for now
  m_TargetBinder.Unbind();

  // evaluate hastable
  std::vector<UINTVECTOR4> hash = m_pglHashTable->GetData();
  m_bConverged = hash.empty();

  // upload missing bricks
  if (!m_bConverged) UpdateVolumePool(hash);

  // always display intermediate results
  return true;
}

/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2012 Interactive Visualization and Data Analysis Group.


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
