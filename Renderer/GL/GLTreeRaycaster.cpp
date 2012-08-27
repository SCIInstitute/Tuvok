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

static BrickKey HashValueToBrickKey(const UINTVECTOR4& hash, size_t timestep, const UVFDataset* pToCDataset) {
  UINT64VECTOR3 layout = pToCDataset->GetBrickLayout(hash.w, timestep);

  return BrickKey(timestep, hash.w, hash.x+
                                    hash.y*layout.x+
                                    hash.z*layout.x*layout.y);
}

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
  m_pFBODebug(NULL),
  m_bNoRCClipplanes(bNoRCClipplanes)
{
  // a member of the parent class, henced it's initialized here
  m_bSupportsMeshes = false;

  std::fill(m_pFBOStartColor.begin(), m_pFBOStartColor.end(), (GLFBOTex*)NULL);
  std::fill(m_pFBOStartColorNext.begin(), m_pFBOStartColorNext.end(), (GLFBOTex*)NULL);
  std::fill(m_pFBORayStart.begin(), m_pFBORayStart.end(), (GLFBOTex*)NULL);
  std::fill(m_pFBORayStartNext.begin(), m_pFBORayStartNext.end(), (GLFBOTex*)NULL);
}


bool GLTreeRaycaster::CreateVolumePool() {

  // todo: make this configurable
  const UINTVECTOR3 poolSize = UINTVECTOR3(1024, 1024, 512);

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

  UINTVECTOR3 vDomainSize = UINTVECTOR3(m_pToCDataset->GetDomainSize());


  m_pVolumePool = new GLVolumePool(poolSize, UINTVECTOR3(m_pToCDataset->GetMaxUsedBrickSizes()), 
                                   m_pToCDataset->GetBrickOverlapSize(), 
                                   vDomainSize, glInternalformat, glFormat, glType);


  // upload a brick that covers the entire domain to make sure have something to render

  if (m_vUploadMem.empty()) {
  // if loading a brick for the first time, prepare upload mem
    const UINTVECTOR3 vSize = m_pToCDataset->GetMaxBrickSize();
    const uint64_t iByteWidth = m_pToCDataset->GetBitWidth()/8;
    const uint64_t iCompCount = m_pToCDataset->GetComponentCount();
    const uint64_t iBrickSize = vSize.volume() * iByteWidth * iCompCount;
    m_vUploadMem.resize(static_cast<size_t>(iBrickSize));
  }

  // find lowest LoD with only a single brick
  for (size_t iLoD = 0;iLoD<m_pToCDataset->GetLODLevelCount();++iLoD) {
    if (m_pToCDataset->GetBrickCount(iLoD, m_iTimestep) == 1) {
      const BrickKey bkey = HashValueToBrickKey(UINTVECTOR4(0,0,0,uint32_t(m_pToCDataset->GetLODLevelCount()-1)), m_iTimestep, m_pToCDataset);

      m_pToCDataset->GetBrick(bkey, m_vUploadMem);
      m_pVolumePool->UploadFirstBrick(m_pToCDataset->GetBrickVoxelCounts(bkey), &m_vUploadMem[0]);
      break;
    }
  }

  
  RecomputeBrickVisibility();

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
  std::for_each(m_pFBOStartColor.begin(),     m_pFBOStartColor.end(),     deleteFBO);
  std::for_each(m_pFBOStartColorNext.begin(), m_pFBOStartColorNext.end(), deleteFBO);

  deleteFBO(m_pFBODebug);

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
      bind(recreateFBO, _1, m_pContext ,m_vWinSize, intformat, GL_RGBA, type));
    for_each(m_pFBORayStartNext.begin(), m_pFBORayStartNext.end(), 
      bind(recreateFBO, _1, m_pContext ,m_vWinSize, intformat, GL_RGBA, type));
    for_each(m_pFBOStartColor.begin(), m_pFBOStartColor.end(), 
      bind(recreateFBO, _1, m_pContext ,m_vWinSize, intformat, GL_RGBA, type));
    for_each(m_pFBOStartColorNext.begin(), m_pFBOStartColorNext.end(), 
      bind(recreateFBO, _1, m_pContext ,m_vWinSize, intformat, GL_RGBA, type));

    recreateFBO(m_pFBODebug, m_pContext ,m_vWinSize, intformat, GL_RGBA, type);
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
  vs.push_back(FindFileInDirs("GLTreeRaycaster-entry-VS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-1D-FS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("Compositing.glsl", m_vShaderSearchDirs, false));
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
  CreateVBO();

  return true;
}

void GLTreeRaycaster::SetRescaleFactors(const DOUBLEVECTOR3& vfRescale) {
  GLRenderer::SetRescaleFactors(vfRescale);
  CreateVBO();
}

void GLTreeRaycaster::CreateVBO() {
  delete m_pBBoxVBO;
  m_pBBoxVBO = NULL;


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
  m_pBBoxVBO->AddVertexData(posData);
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

  // start a little (actually a thousandth of a voxel) into the volume 
  // to get rid of float inaccuracies with the starting point computation
  UINTVECTOR3 vDomainSize = UINTVECTOR3(m_pToCDataset->GetDomainSize());
  mScale.Scaling((1.0f-(0.001f/vDomainSize.x))/vExtend.x, 
                 (1.0f-(0.001f/vDomainSize.y))/vExtend.y, 
                 (1.0f-(0.001f/vDomainSize.z))/vExtend.z);


  return renderRegion.modelView[size_t(eStereoID)].inverse() * mTrans * mScale * mNormalize;
}

bool GLTreeRaycaster::Continue3DDraw() {
  return !m_bConverged;
}

void GLTreeRaycaster::FillRayEntryBuffer(RenderRegion3D& rr, EStereoID eStereoID) {
  m_TargetBinder.Bind(m_pFBOStartColor[size_t(eStereoID)]);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

void GLTreeRaycaster::SetIsoValue(float fIsovalue) {
  GLRenderer::SetIsoValue(fIsovalue);
  RecomputeBrickVisibility();
}

void GLTreeRaycaster::Changed2DTrans() {
  GLRenderer::Changed2DTrans();
  RecomputeBrickVisibility();
}

void GLTreeRaycaster::Changed1DTrans() {
  GLRenderer::Changed1DTrans();
  RecomputeBrickVisibility();
}

void GLTreeRaycaster::Set1DTrans(const std::vector<unsigned char>& rgba) {
  GLRenderer::Set1DTrans(rgba);
  RecomputeBrickVisibility();
}

void GLTreeRaycaster::SetRendermode(AbstrRenderer::ERenderMode eRenderMode) {
  GLRenderer::SetRendermode(eRenderMode);
  RecomputeBrickVisibility();
}

void GLTreeRaycaster::RecomputeBrickVisibility() {
  if (!m_pVolumePool) return;

  for(auto brick = m_pDataset->BricksBegin(); brick != m_pDataset->BricksEnd(); ++brick) {
    const BrickKey key = brick->first;
    const bool bContainsData = ContainsData(key);

    m_pVolumePool->BrickIsVisible(uint32_t(std::get<1>(key)), 
                                     uint32_t(std::get<2>(key)),
                                     bContainsData, bContainsData); // TODO: compute the children visibility
  }
  m_pVolumePool->UploadMetaData();
}

void GLTreeRaycaster::Raycast(RenderRegion3D& rr, EStereoID eStereoID) {
  m_TargetBinder.Bind(m_pFBO3DImageNext[size_t(eStereoID)],                      
                      m_pFBOStartColorNext[size_t(eStereoID)],
                      m_pFBORayStartNext[size_t(eStereoID)],
                      m_pFBODebug);

  m_pFBORayStart[size_t(eStereoID)]->Read(0);
  m_pFBOStartColor[size_t(eStereoID)]->Read(1);
  m_p1DTransTex->Bind(2);   // todo: use m_eRenderMode, this is only for RM_1DTRANS

  UINTVECTOR3 vDomainSize = UINTVECTOR3(m_pToCDataset->GetDomainSize());
  FLOATVECTOR3 vScale = FLOATVECTOR3(m_pToCDataset->GetScale());
  FLOATVECTOR3 vExtend = FLOATVECTOR3(vDomainSize) * vScale;
  vExtend /= vExtend.maxVal();
  vScale /= vScale.minVal();

  m_pVolumePool->Enable(m_FrustumCullingLOD.GetLoDFactor(), 
                        vExtend, vScale, m_pProgramRayCast1D); // bound to 3 and 4
  m_pglHashTable->Enable(); // bound to 5

  // set shader parameters (shader is already enabled by m_pVolumePool->Enable)
  m_pProgramRayCast1D->Enable();
  m_pProgramRayCast1D->Set("sampleRateModifier", m_fSampleRateModifier);
  m_pProgramRayCast1D->Set("mEyeToModel", ComputeEyeToModelMatrix(rr, eStereoID), 4, false); 
  m_pProgramRayCast1D->Set("mModelView", rr.modelView[size_t(eStereoID)], 4, false); 
  m_pProgramRayCast1D->Set("mModelViewProjection", rr.modelView[size_t(eStereoID)]*m_mProjection[size_t(eStereoID)], 4, false); 

  float fScale         = CalculateScaling();
#if 0
  float fGradientScale = (m_pDataset->MaxGradientMagnitude() == 0) ?
                          1.0f : 1.0f/m_pDataset->MaxGradientMagnitude();
#endif

  m_pProgramRayCast1D->Set("fTransScale",fScale);

  // clear the buffers 
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // render the back faces
  m_pContext->GetStateManager()->SetEnableCullFace(true);
  m_pContext->GetStateManager()->SetCullState(CULL_FRONT);
  m_pBBoxVBO->Bind();
  m_pBBoxVBO->Draw(GL_QUADS);
  m_pBBoxVBO->UnBind();

  // unbind input textures
  m_pFBORayStart[size_t(eStereoID)]->FinishRead();
  m_pFBOStartColor[size_t(eStereoID)]->FinishRead();

  // done rendering for now
  m_TargetBinder.Unbind();

  // swap current and next resume color
  std::swap(m_pFBOStartColorNext[size_t(eStereoID)], m_pFBOStartColor[size_t(eStereoID)]);
  std::swap(m_pFBORayStartNext[size_t(eStereoID)], m_pFBORayStart[size_t(eStereoID)]);

  if (m_bDebugView) 
    std::swap(m_pFBODebug, m_pFBO3DImageNext[size_t(eStereoID)]);
}

bool GLTreeRaycaster::CheckForRedraw() {
  // can't draw to a size zero window.
  if (m_vWinSize.area() == 0) return false; 

  // if we have not converged yet redraw immediately
  // TODO: after finished implementing and debugging
  // we should be using the m_iCheckCounter here similar
  // to AbstrRenderer::CheckForRedraw() 
  if (!m_bConverged) return true; 

  for (size_t i=0; i < renderRegions.size(); ++i) {
    const RenderRegion* region = renderRegions[i];
    if (region->isBlank) return true;
  }

  return false;
}

void GLTreeRaycaster::UpdateToVolumePool(const UINTVECTOR4& brick) {
  std::vector<UINTVECTOR4> bricks;
  bricks.push_back(brick);
  UpdateToVolumePool(bricks);
}

// TODO: this entire function should be part of the GPU mem-manager
void GLTreeRaycaster::UpdateToVolumePool(std::vector<UINTVECTOR4>& hash) {
/*
  // DEBUG Code
  uint32_t iHQLevel = std::numeric_limits<uint32_t>::max();
  uint32_t iLQLevel = 0;
  for (auto missingBrick = hash.begin();missingBrick<hash.end(); ++missingBrick) {
    iHQLevel = std::min(missingBrick->w, iHQLevel);
    iLQLevel = std::max(missingBrick->w, iLQLevel);
  }
  MESSAGE("Max Quality %i, Min Quality=%i", iHQLevel, iLQLevel);
  // DEBUG Code End
*/

  // now iterate over the missing bricks and upload them to the GPU
  // todo: consider batching this if it turns out to make a difference
  //       from submitting each brick seperately
  for (auto missingBrick = hash.begin();missingBrick<hash.end(); ++missingBrick) {
    const BrickKey bkey = HashValueToBrickKey(*missingBrick, m_iTimestep, m_pToCDataset);
    const UINTVECTOR3 vVoxelSize = m_pToCDataset->GetBrickVoxelCounts(bkey);

    m_pToCDataset->GetBrick(bkey, m_vUploadMem);
    if (!m_pVolumePool->UploadBrick(BrickElemInfo(*missingBrick, vVoxelSize), &m_vUploadMem[0])) break;
  }
  
  if (!hash.empty())
    m_pVolumePool->UploadMetaData();
}

bool GLTreeRaycaster::Render3DRegion(RenderRegion3D& rr) {
  // for DEBUG render always MONO
  EStereoID eStereoID = SI_LEFT_OR_MONO;

  // prepare a new view
  if (rr.isBlank) {
//    MESSAGE("Starting a new frame");
    // compute new ray start
    m_Timer.Start();
    FillRayEntryBuffer(rr,eStereoID);
    msecPassedCurrentFrame = 0;
  } else {
//    MESSAGE("Continuing a frame");
  }

  // reset state
  GPUState localState = m_BaseState;
  localState.enableBlend = false;
  m_pContext->GetStateManager()->Apply(localState);

  // clear hastable
  m_pglHashTable->ClearData();

  // do raycasting
  Raycast(rr, eStereoID);

  // evaluate hastable
  std::vector<UINTVECTOR4> hash = m_pglHashTable->GetData();
  //hash.push_back(UINTVECTOR4(0,0,0,0));  // DEBUG Code
  m_bConverged = hash.empty();

  // upload missing bricks
  if (!m_bConverged) {
//    MESSAGE("Last rendering pass was missing %i brick(s), paging in now...", int(hash.size()));
    UpdateToVolumePool(hash);
    float fMsecPassed = float(m_Timer.Elapsed());
    OTHER("The current subframe took %g ms to render (%g sFPS)", fMsecPassed, 1000./fMsecPassed);
  } else {
//    MESSAGE("All bricks rendered, frame complete.");
    float fMsecPassed = float(m_Timer.Elapsed());
    OTHER("The total frame took %g ms to render (%g FPS)", fMsecPassed, 1000./fMsecPassed);
  }


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
