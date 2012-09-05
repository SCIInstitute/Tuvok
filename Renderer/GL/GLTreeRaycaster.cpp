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
#include "Basics/SystemInfo.h"
#include "Basics/Clipper.h"

#include "IO/uvfDataset.h"
#include "IO/TransferFunction1D.h"
#include "IO/TransferFunction2D.h"

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
                         bool) :
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
  m_pProgramRayCast1DLighting(NULL),
  m_pProgramRayCast2D(NULL),
  m_pProgramRayCast2DLighting(NULL),
  m_pToCDataset(NULL),
  m_bConverged(true),
  m_pFBODebug(NULL),
  m_pFBODebugNext(NULL),
  m_FrameTimes(100)
{
  // a member of the parent class, hence it's initialized here
  m_bSupportsMeshes = false;

  std::fill(m_pFBOStartColor.begin(), m_pFBOStartColor.end(), (GLFBOTex*)NULL);
  std::fill(m_pFBOStartColorNext.begin(), m_pFBOStartColorNext.end(), (GLFBOTex*)NULL);
  std::fill(m_pFBORayStart.begin(), m_pFBORayStart.end(), (GLFBOTex*)NULL);
  std::fill(m_pFBORayStartNext.begin(), m_pFBORayStartNext.end(), (GLFBOTex*)NULL);
}


bool GLTreeRaycaster::CreateVolumePool() {
  m_pVolumePool = Controller::Instance().MemMan()->GetVolumePool(m_pToCDataset, ComputeGLFilter(),  m_pContext->GetShareGroupID());

  if (!m_pVolumePool) return false;
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
  Controller::Instance().MemMan()->DeleteVolumePool(&m_pVolumePool);
}


void GLTreeRaycaster::CleanupShaders() {
  GLRenderer::CleanupShaders();

  CleanupShader(&m_pProgramRenderFrontFaces);
  CleanupShader(&m_pProgramRenderFrontFacesNearPlane);
  CleanupTraversalShaders();
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
  deleteFBO(m_pFBODebugNext);

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
    recreateFBO(m_pFBODebugNext, m_pContext ,m_vWinSize, intformat, GL_RGBA, type);
  }
}

bool GLTreeRaycaster::Initialize(std::shared_ptr<Context> ctx) {
  if (!GLRenderer::Initialize(ctx)) {
    T_ERROR("Error in parent call -> aborting");
    return false;
  }

  m_pglHashTable = new GLHashTable(UINTVECTOR3( m_pToCDataset->GetBrickLayout(0, 0)) );
  m_pglHashTable->InitGL();

  CreateVBO();

  if (!CreateVolumePool()) return false;

  // now that we've created the hashtable and the volume pool
  // we can load the rest of the shader that depend on those
  if (!LoadTraversalShaders()) return false;

  // init near plane vbo
  m_pNearPlaneQuad = new GLVBO();
  std::vector<FLOATVECTOR3> posData;
  posData.push_back(FLOATVECTOR3(-1.0f,  1.0f, -0.5f));
  posData.push_back(FLOATVECTOR3( 1.0f,  1.0f, -0.5f));
  posData.push_back(FLOATVECTOR3( 1.0f, -1.0f, -0.5f));
  posData.push_back(FLOATVECTOR3(-1.0f, -1.0f, -0.5f));
  m_pNearPlaneQuad->AddVertexData(posData);


  return true;
}

bool GLTreeRaycaster::LoadCheckShader(GLSLProgram** shader, ShaderDescriptor& sd, std::string name)  {
  MESSAGE("Loading %s shader.", name.c_str());
  *shader = m_pMasterController->MemMan()->GetGLSLProgram(sd, m_pContext->GetShareGroupID());
  if (!(*shader) || !(*shader)->IsValid())
  {
      Cleanup();
      T_ERROR("Error loading %s shader.", name.c_str());
      return false;
  } 
  return true;
}

bool GLTreeRaycaster::LoadTraversalShaders() {

  const std::string poolFragment = m_pVolumePool->GetShaderFragment(3,4);
  const std::string hashFragment = m_pglHashTable->GetShaderFragment(5);

  std::vector<std::string> vs, fs;
  vs.push_back(FindFileInDirs("GLTreeRaycaster-entry-VS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-blend.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-Method-1D.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("Compositing.glsl", m_vShaderSearchDirs, false));
  ShaderDescriptor sd(vs, fs);
  sd.AddFragmentShaderString(poolFragment);
  sd.AddFragmentShaderString(hashFragment);
  if (!LoadCheckShader(&m_pProgramRayCast1D, sd, "1D TF")) return false;

  vs.clear(); fs.clear();
  vs.push_back(FindFileInDirs("GLTreeRaycaster-entry-VS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-blend.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-Method-1D-L.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-GradientTools.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("lighting.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("Compositing.glsl", m_vShaderSearchDirs, false));
  sd = ShaderDescriptor(vs, fs);
  sd.AddFragmentShaderString(poolFragment);
  sd.AddFragmentShaderString(hashFragment);
  if (!LoadCheckShader(&m_pProgramRayCast1DLighting, sd, "1D TF lighting")) return false;

  vs.clear(); fs.clear();
  vs.push_back(FindFileInDirs("GLTreeRaycaster-entry-VS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-blend.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-Method-2D.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-GradientTools.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("Compositing.glsl", m_vShaderSearchDirs, false));
  sd = ShaderDescriptor(vs, fs);
  sd.AddFragmentShaderString(poolFragment);
  sd.AddFragmentShaderString(hashFragment);
  if (!LoadCheckShader(&m_pProgramRayCast2D, sd, "2D TF")) return false;

  vs.clear(); fs.clear();
  vs.push_back(FindFileInDirs("GLTreeRaycaster-entry-VS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-blend.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-Method-2D-L.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-GradientTools.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("lighting.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("Compositing.glsl", m_vShaderSearchDirs, false));
  sd = ShaderDescriptor(vs, fs);
  sd.AddFragmentShaderString(poolFragment);
  sd.AddFragmentShaderString(hashFragment);
  if (!LoadCheckShader(&m_pProgramRayCast2DLighting, sd, "2D TF lighting")) return false;

  return true;
}


void GLTreeRaycaster::CleanupTraversalShaders() {
  CleanupShader(&m_pProgramRayCast1D);
  CleanupShader(&m_pProgramRayCast1DLighting);
  CleanupShader(&m_pProgramRayCast2D);
  CleanupShader(&m_pProgramRayCast2DLighting);
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

  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMaxPoint.y, vMinPoint.z));
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMaxPoint.y, vMinPoint.z));
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMinPoint.y, vMinPoint.z));
  
  // FRONT
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMaxPoint.y, vMaxPoint.z));
  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMaxPoint.y, vMaxPoint.z));
  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMinPoint.y, vMaxPoint.z));

  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMinPoint.y, vMaxPoint.z));
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMinPoint.y, vMaxPoint.z));
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMaxPoint.y, vMaxPoint.z));

  // LEFT
  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMaxPoint.y, vMinPoint.z));
  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMinPoint.y, vMinPoint.z));
  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMinPoint.y, vMaxPoint.z));

  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMinPoint.y, vMaxPoint.z));
  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMaxPoint.y, vMaxPoint.z));
  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMaxPoint.y, vMinPoint.z));

  // RIGHT
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMaxPoint.y, vMaxPoint.z));
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMinPoint.y, vMaxPoint.z));
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMinPoint.y, vMinPoint.z));

  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMinPoint.y, vMinPoint.z));
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMaxPoint.y, vMinPoint.z));
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMaxPoint.y, vMaxPoint.z));

  // BOTTOM
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMinPoint.y, vMaxPoint.z));
  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMinPoint.y, vMaxPoint.z));
  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMinPoint.y, vMinPoint.z));

  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMinPoint.y, vMinPoint.z));
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMinPoint.y, vMinPoint.z));
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMinPoint.y, vMaxPoint.z));

  // TOP
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMaxPoint.y, vMinPoint.z));
  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMaxPoint.y, vMinPoint.z));
  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMaxPoint.y, vMaxPoint.z));

  posData.push_back(FLOATVECTOR3(vMinPoint.x, vMaxPoint.y, vMaxPoint.z));
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMaxPoint.y, vMaxPoint.z));
  posData.push_back(FLOATVECTOR3(vMaxPoint.x, vMaxPoint.y, vMinPoint.z));

  if ( m_bClipPlaneOn ) {
    // clip plane is normaly defined in world space, transform back to model space
    FLOATMATRIX4 inv = (GetFirst3DRegion()->rotation*GetFirst3DRegion()->translation).inverse();
    PLANE<float> transformed = m_ClipPlane.Plane() * inv;

    const FLOATVECTOR3 normal(transformed.xyz().normalized());
    const float d = transformed.d();

    Clipper::BoxPlane(posData, normal, d);
  }
  

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

  m_TargetBinder.Bind(m_pFBODebug, m_pFBODebugNext, m_pFBOStartColor[size_t(eStereoID)], m_pFBORayStart[size_t(eStereoID)]);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  m_TargetBinder.Bind(m_pFBORayStart[size_t(eStereoID)]);

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
  m_pBBoxVBO->Draw(GL_TRIANGLES);
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

  if(this->ColorData()) {
    // We don't have good metadata for color data currently, so it can never be
    // skipped.
    return;
  }

  //Timer timer; timer.Start();

  double const fMaxValue = (m_pDataset->GetRange().first > m_pDataset->GetRange().second) ? m_p1DTrans->GetSize() : m_pDataset->GetRange().second;
  double const fRescaleFactor = fMaxValue / double(m_p1DTrans->GetSize()-1);
  
  // render mode dictates how we look at data ...
  switch (m_eRenderMode) {
  case RM_1DTRANS: {
    double const fMin = double(m_p1DTrans->GetNonZeroLimits().x) * fRescaleFactor;
    double const fMax = double(m_p1DTrans->GetNonZeroLimits().y) * fRescaleFactor;
    // ... in 1D we only care about the range of data in a brick
    for(auto brick = m_pDataset->BricksBegin(); brick != m_pDataset->BricksEnd(); ++brick) {
      const BrickKey key = brick->first;
      const bool bContainsData = m_pDataset->ContainsData(key, fMin, fMax);
      m_pVolumePool->BrickIsVisible(uint32_t(std::get<1>(key)), uint32_t(std::get<2>(key)), bContainsData);
    }
    break; }
  case RM_2DTRANS: {
    double const fMin = double(m_p2DTrans->GetNonZeroLimits().x) * fRescaleFactor;
    double const fMax = double(m_p2DTrans->GetNonZeroLimits().y) * fRescaleFactor;
    double const fMinGradient = double(m_p2DTrans->GetNonZeroLimits().z);
    double const fMaxGradient = double(m_p2DTrans->GetNonZeroLimits().w);
    // ... in 2D we also need to concern ourselves w/ min/max gradients
    for(auto brick = m_pDataset->BricksBegin(); brick != m_pDataset->BricksEnd(); ++brick) {
      const BrickKey key = brick->first;
      const bool bContainsData = m_pDataset->ContainsData(key, fMin, fMax, fMinGradient, fMaxGradient);
      m_pVolumePool->BrickIsVisible(uint32_t(std::get<1>(key)), uint32_t(std::get<2>(key)), bContainsData);
    }
    break; }
  case RM_ISOSURFACE:
    // ... and in isosurface mode we only care about a single value.
    for(auto brick = m_pDataset->BricksBegin(); brick != m_pDataset->BricksEnd(); ++brick) {
      const BrickKey key = brick->first;
      const bool bContainsData = m_pDataset->ContainsData(key, GetIsoValue());
      m_pVolumePool->BrickIsVisible(uint32_t(std::get<1>(key)), uint32_t(std::get<2>(key)), bContainsData);
    }
    break;
  default:
    T_ERROR("Unhandled rendering mode.");
    return;
  }

  m_pVolumePool->EvaluateChildEmptiness();
  m_pVolumePool->UploadMetaData();

  //OTHER("%.5f msec", timer.Elapsed());
}

void GLTreeRaycaster::Raycast(RenderRegion3D& rr, EStereoID eStereoID) {
  m_TargetBinder.Bind(m_pFBO3DImageNext[size_t(eStereoID)],
                      m_pFBOStartColorNext[size_t(eStereoID)],
                      m_pFBORayStartNext[size_t(eStereoID)],
                      m_pFBODebugNext);

  m_pFBORayStart[size_t(eStereoID)]->Read(0);
  m_pFBOStartColor[size_t(eStereoID)]->Read(1);

  UINTVECTOR3 vDomainSize = UINTVECTOR3(m_pToCDataset->GetDomainSize());
  FLOATVECTOR3 vScale = FLOATVECTOR3(m_pToCDataset->GetScale());
  FLOATVECTOR3 vExtend = FLOATVECTOR3(vDomainSize) * vScale;
  vExtend /= vExtend.maxVal();
  vScale /= vScale.minVal();

  GLSLProgram* shaderProgram = NULL;
  
  switch (m_eRenderMode) {
    default: // TODO
    case RM_1DTRANS : 
       m_p1DTransTex->Bind(2);
      if (m_bUseLighting) 
        shaderProgram = m_pProgramRayCast1DLighting;
      else
        shaderProgram = m_pProgramRayCast1D;
      break;
    case RM_2DTRANS : 
      m_p2DTransTex->Bind(2);
      if (m_bUseLighting) 
        shaderProgram = m_pProgramRayCast2DLighting;
      else
        shaderProgram = m_pProgramRayCast2D;
      break;
  }
  FLOATMATRIX4 emm = ComputeEyeToModelMatrix(rr, eStereoID);

  m_pVolumePool->Enable(m_FrustumCullingLOD.GetLoDFactor(), 
                        vExtend, vScale, shaderProgram); // bound to 3 and 4
  m_pglHashTable->Enable(); // bound to 5
  m_pFBODebug->Read(6);

  // set shader parameters (shader is already enabled by m_pVolumePool->Enable)
  shaderProgram->Enable();
  shaderProgram->Set("sampleRateModifier", m_fSampleRateModifier);
  shaderProgram->Set("mEyeToModel", emm, 4, false); 
  shaderProgram->Set("mModelView", rr.modelView[size_t(eStereoID)], 4, false); 
  shaderProgram->Set("mModelViewProjection", rr.modelView[size_t(eStereoID)]*m_mProjection[size_t(eStereoID)], 4, false); 

  float fScale         = CalculateScaling();
  shaderProgram->Set("fTransScale",fScale);

  if (m_eRenderMode == RM_2DTRANS) {
    float fGradientScale = (m_pDataset->MaxGradientMagnitude() == 0) ?
                            1.0f : 1.0f/m_pDataset->MaxGradientMagnitude();
    shaderProgram->Set("fGradientScale",fGradientScale);
  }

  if (m_bUseLighting) {
    FLOATVECTOR3 a = m_cAmbient.xyz()*m_cAmbient.w;
    FLOATVECTOR3 d = m_cDiffuse.xyz()*m_cDiffuse.w;
    FLOATVECTOR3 s = m_cSpecular.xyz()*m_cSpecular.w;

    FLOATVECTOR3 aM = m_cAmbientM.xyz()*m_cAmbientM.w;
    FLOATVECTOR3 dM = m_cDiffuseM.xyz()*m_cDiffuseM.w;
    FLOATVECTOR3 sM = m_cSpecularM.xyz()*m_cSpecularM.w;

    FLOATVECTOR3 scale = 1.0f/vScale;


    FLOATVECTOR3 vModelSpaceLightDir = ( FLOATVECTOR4(m_vLightDir,0.0f) * emm ).xyz().normalized();
    FLOATVECTOR3 vModelSpaceEyePos   = (FLOATVECTOR4(0,0,0,1) * emm).xyz();

    shaderProgram->Set("vLightAmbient",a.x,a.y,a.z);
    shaderProgram->Set("vLightDiffuse",d.x,d.y,d.z);
    shaderProgram->Set("vLightSpecular",s.x,s.y,s.z);
    shaderProgram->Set("vModelSpaceLightDir",vModelSpaceLightDir.x,vModelSpaceLightDir.y,vModelSpaceLightDir.z);
    shaderProgram->Set("vModelSpaceEyePos",vModelSpaceEyePos.x,vModelSpaceEyePos.y,vModelSpaceEyePos.z);
    shaderProgram->Set("vDomainScale",scale.x,scale.y,scale.z);

  }


  // clear the buffers 
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // render the back faces
  m_pContext->GetStateManager()->SetEnableCullFace(true);
  m_pContext->GetStateManager()->SetCullState(CULL_FRONT);
  m_pBBoxVBO->Bind();
  m_pBBoxVBO->Draw(GL_TRIANGLES);
  m_pBBoxVBO->UnBind();

  // unbind input textures
  m_pFBORayStart[size_t(eStereoID)]->FinishRead();
  m_pFBOStartColor[size_t(eStereoID)]->FinishRead();
  m_pFBODebug->FinishRead();

  // done rendering for now
  m_TargetBinder.Unbind();

  // swap current and next resume color
  std::swap(m_pFBOStartColorNext[size_t(eStereoID)], m_pFBOStartColor[size_t(eStereoID)]);
  std::swap(m_pFBORayStartNext[size_t(eStereoID)], m_pFBORayStart[size_t(eStereoID)]);
  std::swap(m_pFBODebugNext, m_pFBODebug);
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
  glClearColor(0,0,0,0);

  size_t iStereoBufferCount = (m_bDoStereoRendering) ? 2 : 1;

  // prepare a new view
  if (rr.isBlank) {
    for (size_t i = 0;i<iStereoBufferCount;i++) {
      // compute new ray start
      m_Timer.Start();
      FillRayEntryBuffer(rr,EStereoID(i));
      msecPassedCurrentFrame = 0;
    }
  }

  size_t iPagedBricks = 0;
  for (size_t i = 0;i<iStereoBufferCount;i++) {
    // reset state
    GPUState localState = m_BaseState;
    localState.enableBlend = false;
    m_pContext->GetStateManager()->Apply(localState);

    // clear hastable
    m_pglHashTable->ClearData();

    // do raycasting
    Raycast(rr, EStereoID(i));

    // evaluate hastable
    std::vector<UINTVECTOR4> hash = m_pglHashTable->GetData();

    // upload missing bricks
    if (!hash.empty()) {
      //    MESSAGE("Last rendering pass was missing %i brick(s), paging in now...", int(hash.size()));
      UpdateToVolumePool(hash);
  #if 0
      float fMsecPassed = float(m_Timer.Elapsed());
      OTHER("The current subframe took %g ms to render (%g sFPS)", fMsecPassed, 1000./fMsecPassed);
  #endif
      m_iSubFrames++;
    } else {
  //    MESSAGE("All bricks rendered, frame complete.");
      float fMsecPassed = float(m_Timer.Elapsed());
      m_FrameTimes.Push(fMsecPassed);
      OTHER("The total frame (with %d subframes) took %.2f ms to render (%.2f FPS)\t[avg: %.2f, min: %.2f, max: %.2f, samples: %d]", m_iSubFrames, fMsecPassed, 1000./fMsecPassed, m_FrameTimes.GetAvg(), m_FrameTimes.GetMin(), m_FrameTimes.GetMax(), m_FrameTimes.GetHistroryLength());
      m_iSubFrames = 0;

      if (m_bDebugView)
        std::swap(m_pFBODebug, m_pFBO3DImageNext[i]);
    }

    iPagedBricks += hash.size();
  }

   m_bConverged = iPagedBricks == 0;


  if (m_eRenderMode == RM_ISOSURFACE &&
      m_vCurrentBrickList.size() == m_iBricksRenderedInThisSubFrame) {
     
      for (size_t i = 0;i<iStereoBufferCount;i++) {
        m_TargetBinder.Bind(m_pFBO3DImageNext[i]);
        ComposeSurfaceImage(rr, EStereoID(i));
      } 

      m_TargetBinder.Unbind();
  }

  // always display intermediate results
  return true;
}

void GLTreeRaycaster::SetInterpolant(Interpolant eInterpolant) {  
  GLRenderer::SetInterpolant(eInterpolant);
  m_pVolumePool->SetFilterMode(ComputeGLFilter());
}

void GLTreeRaycaster::SetClipPlane(RenderRegion *renderRegion,
                                   const ExtendedPlane& plane) {
  GLRenderer::SetClipPlane(renderRegion, plane);
  CreateVBO();
}

void GLTreeRaycaster::EnableClipPlane(RenderRegion *renderRegion) {
  GLRenderer::EnableClipPlane(renderRegion);
  CreateVBO();  
}

void GLTreeRaycaster::DisableClipPlane(RenderRegion *renderRegion) {
  GLRenderer::DisableClipPlane(renderRegion);
  CreateVBO();
}

void GLTreeRaycaster::ClipPlaneRelativeLock(bool bRelative) {
  GLRenderer::ClipPlaneRelativeLock(bRelative);
  CreateVBO();
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
