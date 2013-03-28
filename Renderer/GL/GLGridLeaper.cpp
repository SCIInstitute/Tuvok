#include "Basics/Clipper.h"
#include "Basics/Plane.h"
#include "Basics/SysTools.h" // for Paper Hack file log 
#include "Controller/Controller.h"
#include "Controller/StackTimer.h"
#include "IO/TransferFunction1D.h"
#include "IO/TransferFunction2D.h"
#include "IO/FileBackedDataset.h"
#include "IO/LinearIndexDataset.h"
#include "Renderer/GPUMemMan/GPUMemMan.h"

#include "GLFBOTex.h"
#include "GLGridLeaper.h"
#include "GLHashTable.h"
#include "GLInclude.h"
#include "GLSLProgram.h"
#include "GLTexture1D.h"
#include "GLVolumePool.h"
#include "GLVBO.h"

using std::bind;
using namespace std::placeholders;
using namespace std;
using namespace tuvok;

GLGridLeaper::GLGridLeaper(MasterController* pMasterController, 
                         bool bUseOnlyPowerOfTwo, 
                         bool bDownSampleTo8Bits, 
                         bool bDisableBorder) :
  GLGPURayTraverser(pMasterController,
             bUseOnlyPowerOfTwo, 
             bDownSampleTo8Bits, 
             bDisableBorder),

  m_pglHashTable(NULL),
  m_pVolumePool(NULL),
  m_pProgramRenderFrontFaces(NULL),
  m_pProgramRenderFrontFacesNearPlane(NULL),
  m_pProgramRayCast1D(NULL),
  m_pProgramRayCast1DLighting(NULL),
  m_pProgramRayCast2D(NULL),
  m_pProgramRayCast2DLighting(NULL),
  m_pProgramRayCastISO(NULL),
  m_pProgramRayCast1DColor(NULL),
  m_pProgramRayCast1DLightingColor(NULL),
  m_pProgramRayCast2DColor(NULL),
  m_pProgramRayCast2DLightingColor(NULL),
  m_pProgramRayCastISOColor(NULL),
  m_pToCDataset(NULL),
  m_bConverged(true),
  m_VisibilityState()
  , m_iSubframes(0)
  , m_iPagedBricks(0)
  , m_FrameTimes(100)
  , m_iAveragingFrameCount(0)
  , m_bAveragingFrameTimes(false)
  , m_pLogFile(NULL)
  , m_pBrickAccess(NULL)
  , m_iFrameCount(0)
#ifdef GLGRIDLEAPER_DEBUGVIEW
  , m_pFBODebug(NULL)
  , m_pFBODebugNext(NULL)
#endif
#ifdef GLGRIDLEAPER_WORKINGSET
  , m_pWorkingSetTable(NULL)
#endif
  , m_RenderingTime(0)
{
  // a member of the parent class, hence it's initialized here
  m_bSupportsMeshes = false;

  std::fill(m_pFBOStartColor.begin(), m_pFBOStartColor.end(), (GLFBOTex*)NULL);
  std::fill(m_pFBOStartColorNext.begin(), m_pFBOStartColorNext.end(), (GLFBOTex*)NULL);
  std::fill(m_pFBORayStart.begin(), m_pFBORayStart.end(), (GLFBOTex*)NULL);
  std::fill(m_pFBORayStartNext.begin(), m_pFBORayStartNext.end(), (GLFBOTex*)NULL);
}


bool GLGridLeaper::CreateVolumePool() {
  m_pVolumePool = Controller::Instance().MemMan()->GetVolumePool(m_pToCDataset,
    ComputeGLFilter(),  m_pContext->GetShareGroupID()
  );

  if (!m_pVolumePool) return false;
  // upload a brick that covers the entire domain to make sure have
  // something to render

  // find lowest LoD with only a single brick
  const BrickKey bkey = m_pToCDataset->IndexFrom4D(
    UINTVECTOR4(0,0,0,
        uint32_t(m_pToCDataset->GetLargestSingleBrickLOD(m_iTimestep))),
    m_iTimestep
  );
  m_pVolumePool->UploadFirstBrick(bkey);

  RecomputeBrickVisibility();

  return true;
}

bool GLGridLeaper::RegisterDataset(Dataset* ds) {
  if(!AbstrRenderer::RegisterDataset(ds)) {
    return false;
  }

  LinearIndexDataset* pLinDataset = dynamic_cast<LinearIndexDataset*>
                                                (m_pDataset);
  if (!pLinDataset) {
    T_ERROR("Currently, this renderer works only with linear datasets.");
    return false;
  }

  bool const bReinit = (m_pVolumePool != NULL);
  if (bReinit) {
    CleanupTraversalShaders();
    Controller::Instance().MemMan()->DeleteVolumePool(&m_pVolumePool);
    CleanupHashTable();
  }
  m_pToCDataset = pLinDataset;
  if (bReinit) {
    m_VisibilityState = VisibilityState(); // reset visibility state to force update
    InitHashTable();
    FillBBoxVBO();
    CreateVolumePool();
    LoadTraversalShaders();
  }
  return true;
}

GLGridLeaper::~GLGridLeaper() {
  PH_CloseLogfile();
  PH_CloseBrickAccessLogfile();
}

void GLGridLeaper::CleanupShaders() {
  GLGPURayTraverser::CleanupShaders();

  CleanupShader(&m_pProgramRenderFrontFaces);
  CleanupShader(&m_pProgramRenderFrontFacesNearPlane);
  CleanupTraversalShaders();
}

struct {
  void operator() (GLFBOTex*& fbo) const {
    if (fbo){
      Controller::Instance().MemMan()->FreeFBO(fbo); 
      fbo = NULL;
    }
  }
} deleteFBO;

void GLGridLeaper::Cleanup() {
  GLGPURayTraverser::Cleanup();

  std::for_each(m_pFBORayStart.begin(), m_pFBORayStart.end(), deleteFBO);
  std::for_each(m_pFBORayStartNext.begin(), m_pFBORayStartNext.end(),
                deleteFBO);
  std::for_each(m_pFBOStartColor.begin(), m_pFBOStartColor.end(), deleteFBO);
  std::for_each(m_pFBOStartColorNext.begin(), m_pFBOStartColorNext.end(),
                deleteFBO);

#ifdef GLGRIDLEAPER_DEBUGVIEW
  deleteFBO(m_pFBODebug);
  deleteFBO(m_pFBODebugNext);
#endif

  CleanupHashTable();
  Controller::Instance().MemMan()->DeleteVolumePool(&m_pVolumePool);
}

void GLGridLeaper::CleanupHashTable() {
  if (m_pglHashTable) {
    m_pglHashTable->FreeGL();
    delete m_pglHashTable;
    m_pglHashTable = NULL;
  }

#ifdef GLGRIDLEAPER_WORKINGSET
  if (m_pWorkingSetTable) {
    m_pWorkingSetTable->FreeGL();
    delete m_pWorkingSetTable;
    m_pWorkingSetTable = NULL;
  }
#endif
}

void recreateFBO(GLFBOTex*& fbo, std::shared_ptr<Context> pContext,
                 const UINTVECTOR2& ws, GLenum intformat, 
                 GLenum format, GLenum type) {
  if (fbo)
    Controller::Instance().MemMan()->FreeFBO(fbo); 

  fbo = Controller::Instance().MemMan()->GetFBO(GL_NEAREST, GL_NEAREST, GL_CLAMP, ws.x,
    ws.y, intformat, format, type, pContext->GetShareGroupID(), false);
}

void GLGridLeaper::CreateOffscreenBuffers() {
  GLGPURayTraverser::CreateOffscreenBuffers();

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

#ifdef GLGRIDLEAPER_DEBUGVIEW
    recreateFBO(m_pFBODebug, m_pContext ,m_vWinSize, intformat, GL_RGBA, type);
    recreateFBO(m_pFBODebugNext, m_pContext ,m_vWinSize, intformat, GL_RGBA, type);
#endif
  }
}

bool GLGridLeaper::Initialize(std::shared_ptr<Context> ctx) {
  if (!GLGPURayTraverser::Initialize(ctx)) {
    T_ERROR("Error in parent call -> aborting");
    return false;
  }

  InitHashTable();

  FillBBoxVBO();

  if (!CreateVolumePool()) return false;

  // now that we've created the hashtable and the volume pool
  // we can load the rest of the shader that depend on those
  if (!LoadTraversalShaders()) return false;

  return true;
}

void GLGridLeaper::InitHashTable() {
  UINTVECTOR3 const finestBrickLayout(m_pToCDataset->GetBrickLayout(0, 0));

  m_pglHashTable = new GLHashTable(finestBrickLayout,
    std::max<uint32_t>(15,
    uint32_t(511 / (m_pToCDataset->GetMaxBrickSize().volume() / 32768.))),

    Controller::ConstInstance().PHState.RehashCount
    );
  m_pglHashTable->InitGL();

#ifdef GLGRIDLEAPER_WORKINGSET
  // the HT needs to have the full 4D volume size here in order to guarantee
  // a 1:1 mapping with the hash function
  m_pWorkingSetTable = new GLHashTable(
    finestBrickLayout, finestBrickLayout.volume() *
    uint32_t(m_pToCDataset->GetLargestSingleBrickLOD(0)),
    Controller::ConstInstance().PHState.RehashCount, true, "workingSet"
    );
  m_pWorkingSetTable->InitGL();
#endif
}

bool GLGridLeaper::LoadCheckShader(GLSLProgram** shader, ShaderDescriptor& sd, std::string name)  {
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

static GLVolumePool::MissingBrickStrategy MCStrategyToVPoolStrategy(
  PH_HackyState::BrickStrategy bs
) {
  switch(bs) {
    case PH_HackyState::BS_OnlyNeeded: return GLVolumePool::OnlyNeeded;
    case PH_HackyState::BS_RequestAll: return GLVolumePool::RequestAll;
    case PH_HackyState::BS_SkipOneLevel: return GLVolumePool::SkipOneLevel;
    case PH_HackyState::BS_SkipTwoLevels: return GLVolumePool::SkipTwoLevels;
  }
  return GLVolumePool::MissingBrickStrategy(0);
}


bool GLGridLeaper::LoadTraversalShaders(const std::vector<std::string>& vDefines) {

#ifdef GLGRIDLEAPER_WORKINGSET
  const std::string infoFragment = m_pWorkingSetTable->GetShaderFragment(7);
  const std::string poolFragment = m_pVolumePool->GetShaderFragment(
    3, 4,
    MCStrategyToVPoolStrategy(Controller::ConstInstance().PHState.BStrategy),
    m_pWorkingSetTable->GetPrefixName());
#else
  const std::string poolFragment = m_pVolumePool->GetShaderFragment(
    3, 4,
    MCStrategyToVPoolStrategy(Controller::ConstInstance().PHState.BStrategy)
  );
#endif
  const std::string hashFragment = m_pglHashTable->GetShaderFragment(5);

  std::vector<std::string> vs, fs;
  vs.push_back(FindFileInDirs("GLGridLeaper-entry-VS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-blend.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-Method-1D.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("Compositing.glsl", m_vShaderSearchDirs, false));
  ShaderDescriptor sd(vs, fs);
  sd.AddDefines(vDefines);
  sd.AddFragmentShaderString(poolFragment);
  sd.AddFragmentShaderString(hashFragment);
#ifdef GLGRIDLEAPER_WORKINGSET
  sd.AddFragmentShaderString(infoFragment);
#endif
  if (!LoadCheckShader(&m_pProgramRayCast1D, sd, "1D TF")) return false;

  vs.clear(); fs.clear();
  vs.push_back(FindFileInDirs("GLGridLeaper-entry-VS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-blend.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-Method-1D-color.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("Compositing.glsl", m_vShaderSearchDirs, false));
  sd = ShaderDescriptor(vs, fs);
  sd.AddDefines(vDefines);
  sd.AddFragmentShaderString(poolFragment);
  sd.AddFragmentShaderString(hashFragment);
#ifdef GLGRIDLEAPER_WORKINGSET
  sd.AddFragmentShaderString(infoFragment);
#endif
  if (!LoadCheckShader(&m_pProgramRayCast1DColor, sd, "Color 1D TF")) return false;

  vs.clear(); fs.clear();
  vs.push_back(FindFileInDirs("GLGridLeaper-entry-VS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-blend.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-Method-1D-L.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-GradientTools.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("lighting.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("Compositing.glsl", m_vShaderSearchDirs, false));
  sd = ShaderDescriptor(vs, fs);
  sd.AddDefines(vDefines);
  sd.AddFragmentShaderString(poolFragment);
  sd.AddFragmentShaderString(hashFragment);
#ifdef GLGRIDLEAPER_WORKINGSET
  sd.AddFragmentShaderString(infoFragment);
#endif
  if (!LoadCheckShader(&m_pProgramRayCast1DLighting, sd, "1D TF lighting")) return false;

  vs.clear(); fs.clear();
  vs.push_back(FindFileInDirs("GLGridLeaper-entry-VS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-blend.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-Method-1D-L-color.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-GradientTools.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("lighting.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("Compositing.glsl", m_vShaderSearchDirs, false));
  sd = ShaderDescriptor(vs, fs);
  sd.AddDefines(vDefines);
  sd.AddFragmentShaderString(poolFragment);
  sd.AddFragmentShaderString(hashFragment);
#ifdef GLGRIDLEAPER_WORKINGSET
  sd.AddFragmentShaderString(infoFragment);
#endif
  if (!LoadCheckShader(&m_pProgramRayCast1DLightingColor, sd, "Color 1D TF lighting")) return false;

  vs.clear(); fs.clear();
  vs.push_back(FindFileInDirs("GLGridLeaper-entry-VS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-blend.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-Method-2D.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-GradientTools.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("Compositing.glsl", m_vShaderSearchDirs, false));
  sd = ShaderDescriptor(vs, fs);
  sd.AddDefines(vDefines);
  sd.AddFragmentShaderString(poolFragment);
  sd.AddFragmentShaderString(hashFragment);
#ifdef GLGRIDLEAPER_WORKINGSET
  sd.AddFragmentShaderString(infoFragment);
#endif
  if (!LoadCheckShader(&m_pProgramRayCast2D, sd, "2D TF")) return false;

  vs.clear(); fs.clear();
  vs.push_back(FindFileInDirs("GLGridLeaper-entry-VS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-blend.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-Method-2D-color.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-GradientTools.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("Compositing.glsl", m_vShaderSearchDirs, false));
  sd = ShaderDescriptor(vs, fs);
  sd.AddDefines(vDefines);
  sd.AddFragmentShaderString(poolFragment);
  sd.AddFragmentShaderString(hashFragment);
#ifdef GLGRIDLEAPER_WORKINGSET
  sd.AddFragmentShaderString(infoFragment);
#endif
  if (!LoadCheckShader(&m_pProgramRayCast2DColor, sd, "Color 2D TF")) return false;

  vs.clear(); fs.clear();
  vs.push_back(FindFileInDirs("GLGridLeaper-entry-VS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-blend.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-Method-2D-L.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-GradientTools.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("lighting.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("Compositing.glsl", m_vShaderSearchDirs, false));
  sd = ShaderDescriptor(vs, fs);
  sd.AddDefines(vDefines);
  sd.AddFragmentShaderString(poolFragment);
  sd.AddFragmentShaderString(hashFragment);
#ifdef GLGRIDLEAPER_WORKINGSET
  sd.AddFragmentShaderString(infoFragment);
#endif
  if (!LoadCheckShader(&m_pProgramRayCast2DLighting, sd, "2D TF lighting")) return false;

  vs.clear(); fs.clear();
  vs.push_back(FindFileInDirs("GLGridLeaper-entry-VS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-blend.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-Method-2D-L-color.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-GradientTools.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("lighting.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("Compositing.glsl", m_vShaderSearchDirs, false));
  sd = ShaderDescriptor(vs, fs);
  sd.AddDefines(vDefines);
  sd.AddFragmentShaderString(poolFragment);
  sd.AddFragmentShaderString(hashFragment);
#ifdef GLGRIDLEAPER_WORKINGSET
  sd.AddFragmentShaderString(infoFragment);
#endif
  if (!LoadCheckShader(&m_pProgramRayCast2DLightingColor, sd, "Color 2D TF lighting")) return false;

  vs.clear(); fs.clear();
  vs.push_back(FindFileInDirs("GLGridLeaper-entry-VS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-iso.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-Method-iso.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-GradientTools.glsl", m_vShaderSearchDirs, false));
  sd = ShaderDescriptor(vs, fs);
  sd.AddDefines(vDefines);
  sd.AddFragmentShaderString(poolFragment);
  sd.AddFragmentShaderString(hashFragment);
#ifdef GLGRIDLEAPER_WORKINGSET
  sd.AddFragmentShaderString(infoFragment);
#endif
  if (!LoadCheckShader(&m_pProgramRayCastISO, sd, "Isosurface")) return false;

  vs.clear(); fs.clear();
  vs.push_back(FindFileInDirs("GLGridLeaper-entry-VS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-iso.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-Method-iso-color.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLGridLeaper-GradientTools.glsl", m_vShaderSearchDirs, false));
  sd = ShaderDescriptor(vs, fs);
  sd.AddDefines(vDefines);
  sd.AddFragmentShaderString(poolFragment);
  sd.AddFragmentShaderString(hashFragment);
#ifdef GLGRIDLEAPER_WORKINGSET
  sd.AddFragmentShaderString(infoFragment);
#endif
  if (!LoadCheckShader(&m_pProgramRayCastISOColor, sd, "Color Isosurface")) return false;

  return true;
}


void GLGridLeaper::CleanupTraversalShaders() {
  CleanupShader(&m_pProgramRayCast1D);
  CleanupShader(&m_pProgramRayCast1DLighting);
  CleanupShader(&m_pProgramRayCast2D);
  CleanupShader(&m_pProgramRayCast2DLighting);
  CleanupShader(&m_pProgramRayCastISO);
  CleanupShader(&m_pProgramRayCast1DColor);
  CleanupShader(&m_pProgramRayCast1DLightingColor);
  CleanupShader(&m_pProgramRayCast2DColor);
  CleanupShader(&m_pProgramRayCast2DLightingColor);
  CleanupShader(&m_pProgramRayCastISOColor);
}

void GLGridLeaper::SetRescaleFactors(const DOUBLEVECTOR3& vfRescale) {
  GLGPURayTraverser::SetRescaleFactors(vfRescale);
  FillBBoxVBO();
}

void GLGridLeaper::FillBBoxVBO() {
  FLOATVECTOR3 vCenter, vExtend;
  GetVolumeAABB(vCenter, vExtend);

  FLOATVECTOR3 vMinPoint, vMaxPoint;
  vMinPoint = (vCenter - vExtend/2.0);
  vMaxPoint = (vCenter + vExtend/2.0);

  m_pBBoxVBO = new GLVBO();
  std::vector<FLOATVECTOR3> posData;
  MaxMinBoxToVector(vMinPoint, vMaxPoint, posData);

  if ( m_bClipPlaneOn ) {
    // clip plane is normaly defined in world space, transform back to model space
    FLOATMATRIX4 inv = (GetFirst3DRegion()->rotation*GetFirst3DRegion()->translation).inverse();
    PLANE<float> transformed = m_ClipPlane.Plane() * inv;

    const FLOATVECTOR3 normal(transformed.xyz().normalized());
    const float d = transformed.d();

    Clipper::BoxPlane(posData, normal, d);
  }

  m_pBBoxVBO->ClearVertexData();
  m_pBBoxVBO->AddVertexData(posData);
}


bool GLGridLeaper::LoadShaders() {
  if (!GLGPURayTraverser::LoadShaders()) {
    T_ERROR("Error in parent call -> aborting");
    return false;
  }


  if(!LoadAndVerifyShader(&m_pProgramRenderFrontFaces, m_vShaderSearchDirs,
                          "GLGridLeaper-entry-VS.glsl",
                          NULL,
                          "GLGridLeaper-frontfaces-FS.glsl", NULL) ||
     !LoadAndVerifyShader(&m_pProgramRenderFrontFacesNearPlane, m_vShaderSearchDirs,
                          "GLGridLeaper-NearPlane-VS.glsl",
                          NULL,
                          "GLGridLeaper-frontfaces-FS.glsl", NULL) )
  {
      Cleanup();
      T_ERROR("Error loading a shader.");
      return false;
  } 


  return true;
}

FLOATMATRIX4 GLGridLeaper::ComputeEyeToModelMatrix(const RenderRegion &renderRegion,
                                                      EStereoID eStereoID) const {

  FLOATVECTOR3 vCenter, vExtend;
  GetVolumeAABB(vCenter, vExtend);

  FLOATMATRIX4 mTrans, mScale, mNormalize;

  mTrans.Translation(-vCenter);
  mScale.Scaling(1.0f/vExtend);
  mNormalize.Translation(0.5f, 0.5f, 0.5f);

  return renderRegion.modelView[size_t(eStereoID)].inverse() * mTrans * mScale * mNormalize;
}

bool GLGridLeaper::Continue3DDraw() {
  return !m_bConverged;
}

void GLGridLeaper::FillRayEntryBuffer(RenderRegion3D& rr, EStereoID eStereoID) {

#ifdef GLGRIDLEAPER_DEBUGVIEW
  if (m_iDebugView == 2)
    m_TargetBinder.Bind(m_pFBODebug, m_pFBODebugNext, m_pFBOStartColor[size_t(eStereoID)], m_pFBORayStart[size_t(eStereoID)]);
  else
    m_TargetBinder.Bind(m_pFBOStartColor[size_t(eStereoID)], m_pFBORayStart[size_t(eStereoID)]);
#else
  m_TargetBinder.Bind(m_pFBOStartColor[size_t(eStereoID)], m_pFBORayStart[size_t(eStereoID)]);
#endif
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

void GLGridLeaper::SetIsoValue(float fIsovalue) {
  GLGPURayTraverser::SetIsoValue(fIsovalue);
  RecomputeBrickVisibility();
}

void GLGridLeaper::Changed2DTrans() {
  GLGPURayTraverser::Changed2DTrans();
  RecomputeBrickVisibility();
}

void GLGridLeaper::Changed1DTrans() {
  GLGPURayTraverser::Changed1DTrans();
  RecomputeBrickVisibility();
}

void GLGridLeaper::Set1DTrans(const std::vector<unsigned char>& rgba) {
  GLGPURayTraverser::Set1DTrans(rgba);
  RecomputeBrickVisibility();
}

void GLGridLeaper::SetRendermode(AbstrRenderer::ERenderMode eRenderMode) {
  GLGPURayTraverser::SetRendermode(eRenderMode);
  RecomputeBrickVisibility();
}

void GLGridLeaper::RecomputeBrickVisibility(bool bForceSynchronousUpdate) {
  if (!m_pVolumePool) return;

  double const fMaxValue = (m_pDataset->GetRange().first > m_pDataset->GetRange().second) ? m_p1DTrans->GetSize() : m_pDataset->GetRange().second;
  double const fRescaleFactor = fMaxValue / double(m_p1DTrans->GetSize()-1);
  
  // render mode dictates how we look at data ...
  switch (m_eRenderMode) {
  case RM_1DTRANS: {
    double const fMin = double(m_p1DTrans->GetNonZeroLimits().x) * fRescaleFactor;
    double const fMax = double(m_p1DTrans->GetNonZeroLimits().y) * fRescaleFactor;
    if (m_VisibilityState.NeedsUpdate(fMin, fMax) ||
        (bForceSynchronousUpdate && !m_pVolumePool->IsVisibilityUpdated())) {
      m_pVolumePool->RecomputeVisibility(m_VisibilityState, m_iTimestep, bForceSynchronousUpdate);
    } else
      return;
    break; }
  case RM_2DTRANS: {
    double const fMin = double(m_p2DTrans->GetNonZeroLimits().x) * fRescaleFactor;
    double const fMax = double(m_p2DTrans->GetNonZeroLimits().y) * fRescaleFactor;
    double const fMinGradient = double(m_p2DTrans->GetNonZeroLimits().z);
    double const fMaxGradient = double(m_p2DTrans->GetNonZeroLimits().w);
    if (m_VisibilityState.NeedsUpdate(fMin, fMax, fMinGradient, fMaxGradient) ||
        (bForceSynchronousUpdate && !m_pVolumePool->IsVisibilityUpdated())) {
      m_pVolumePool->RecomputeVisibility(m_VisibilityState, m_iTimestep, bForceSynchronousUpdate);
    } else
      return;
    break; }
  case RM_ISOSURFACE: {
    double const fIsoValue = GetIsoValue();
    if (m_VisibilityState.NeedsUpdate(fIsoValue) ||
        (bForceSynchronousUpdate && !m_pVolumePool->IsVisibilityUpdated())) {
      m_pVolumePool->RecomputeVisibility(m_VisibilityState, m_iTimestep, bForceSynchronousUpdate);
    } else
      return;
    break; }
  default:
    T_ERROR("Unhandled rendering mode.");
    return;
  }
}


void GLGridLeaper::SetupRaycastShader(GLSLProgram* shaderProgram, RenderRegion3D& rr, EStereoID eStereoID) {
  UINTVECTOR3 vDomainSize = UINTVECTOR3(m_pToCDataset->GetDomainSize());
  FLOATVECTOR3 vScale = FLOATVECTOR3(m_pToCDataset->GetScale());
  FLOATVECTOR3 vExtend = FLOATVECTOR3(vDomainSize) * vScale;
  vExtend /= vExtend.maxVal();
  vScale /= vScale.minVal();

  FLOATMATRIX4 emm = ComputeEyeToModelMatrix(rr, eStereoID);

  m_pVolumePool->Enable(m_FrustumCullingLOD.GetLoDFactor(), 
                        vExtend, vScale, shaderProgram); // bound to 3 and 4
  m_pglHashTable->Enable(); // bound to 5
#ifdef GLGRIDLEAPER_DEBUGVIEW
  if (m_iDebugView == 2)
    m_pFBODebug->Read(6);
#endif
#ifdef GLGRIDLEAPER_WORKINGSET
  m_pWorkingSetTable->Enable(); // bound to 7
#endif

  // set shader parameters
  shaderProgram->Enable();
  shaderProgram->Set("sampleRateModifier", m_fSampleRateModifier);
  shaderProgram->Set("mEyeToModel", emm, 4, false); 
  shaderProgram->Set("mModelView", rr.modelView[size_t(eStereoID)], 4, false); 
  shaderProgram->Set("mModelViewProjection", rr.modelView[size_t(eStereoID)]*m_mProjection[size_t(eStereoID)], 4, false); 

  if (m_eRenderMode == RM_ISOSURFACE) {
    shaderProgram->Set("fIsoval", static_cast<float>
                                        (this->GetNormalizedIsovalue()));
    FLOATVECTOR3 scale = 1.0f/vScale;
    shaderProgram->Set("vDomainScale",scale.x,scale.y,scale.z);
    shaderProgram->Set("mModelToEye",emm.inverse(), 4, false);
    shaderProgram->Set("mModelViewIT", rr.modelView[size_t(eStereoID)].inverse(), 4, true);
  } else {
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
  }
}

void GLGridLeaper::Raycast(RenderRegion3D& rr, EStereoID eStereoID) {
  StackTimer rendering(PERF_RENDER);
  GLSLProgram* shaderProgram = NULL;
  switch (m_eRenderMode) {
    default: 
      if (m_bDoClearView) {
        // TODO: RM_CLEARVIEW
      } else {
        // RM_ISOSURFACE
        if (this->ColorData()) 
          shaderProgram = m_pProgramRayCastISOColor;
        else
          shaderProgram = m_pProgramRayCastISO;
      }
      break;
    case RM_1DTRANS : 
       m_p1DTransTex->Bind(2);
      if (m_bUseLighting) {
        if (this->ColorData()) 
          shaderProgram = m_pProgramRayCast1DLightingColor;
        else
          shaderProgram = m_pProgramRayCast1DLighting;
      } else {
        if (this->ColorData()) 
          shaderProgram = m_pProgramRayCast1DColor;
        else
          shaderProgram = m_pProgramRayCast1D;
      }
      break;
    case RM_2DTRANS : 
      m_p2DTransTex->Bind(2);
      if (m_bUseLighting) {
        if (this->ColorData()) 
          shaderProgram = m_pProgramRayCast2DLightingColor;
        else
          shaderProgram = m_pProgramRayCast2DLighting;
      } else {
        if (this->ColorData()) 
          shaderProgram = m_pProgramRayCast2DColor;
        else
          shaderProgram = m_pProgramRayCast2D;
      }
      break;
  }

  SetupRaycastShader(shaderProgram, rr, eStereoID);

#ifdef GLGRIDLEAPER_DEBUGVIEW
  if (m_eRenderMode == RM_ISOSURFACE) {
    m_TargetBinder.Bind(m_pFBOIsoHit[size_t(eStereoID)], 0, 
                        m_pFBOIsoHit[size_t(eStereoID)], 1,
                        m_pFBORayStartNext[size_t(eStereoID)],0,
                        m_pFBOStartColorNext[size_t(eStereoID)],0);
  } else {
    if (m_iDebugView == 2)
      m_TargetBinder.Bind(m_pFBO3DImageNext[size_t(eStereoID)],
                          m_pFBOStartColorNext[size_t(eStereoID)],
                          m_pFBORayStartNext[size_t(eStereoID)],
                          m_pFBODebugNext);
    else
      m_TargetBinder.Bind(m_pFBO3DImageNext[size_t(eStereoID)],
                          m_pFBOStartColorNext[size_t(eStereoID)],
                          m_pFBORayStartNext[size_t(eStereoID)]);
  }
#else
  if (m_eRenderMode == RM_ISOSURFACE) {
    m_TargetBinder.Bind(m_pFBOIsoHit[size_t(eStereoID)], 0, 
                        m_pFBOIsoHit[size_t(eStereoID)], 1,
                        m_pFBORayStartNext[size_t(eStereoID)],0,
                        m_pFBOStartColorNext[size_t(eStereoID)],0);
  } else {
    m_TargetBinder.Bind(m_pFBO3DImageNext[size_t(eStereoID)],
                        m_pFBOStartColorNext[size_t(eStereoID)],
                        m_pFBORayStartNext[size_t(eStereoID)]);
  }
#endif

  m_pFBORayStart[size_t(eStereoID)]->Read(0);
  m_pFBOStartColor[size_t(eStereoID)]->Read(1);

  // clear the buffers 
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // render the back faces (in this pass we do all the work)
  m_pContext->GetStateManager()->SetEnableCullFace(true);
  m_pContext->GetStateManager()->SetCullState(CULL_FRONT);
  m_pBBoxVBO->Bind();
  m_pBBoxVBO->Draw(GL_TRIANGLES);
  m_pBBoxVBO->UnBind();

  m_pVolumePool->Disable();

  // unbind input textures
  m_pFBORayStart[size_t(eStereoID)]->FinishRead();
  m_pFBOStartColor[size_t(eStereoID)]->FinishRead();
#ifdef GLGRIDLEAPER_DEBUGVIEW
  if (m_iDebugView == 2)
    m_pFBODebug->FinishRead();
#endif

  // done rendering for now
  m_TargetBinder.Unbind();

  // swap current and next resume color
  std::swap(m_pFBOStartColorNext[size_t(eStereoID)], m_pFBOStartColor[size_t(eStereoID)]);
  std::swap(m_pFBORayStartNext[size_t(eStereoID)], m_pFBORayStart[size_t(eStereoID)]);
#ifdef GLGRIDLEAPER_DEBUGVIEW
  if (m_iDebugView == 2)
    std::swap(m_pFBODebugNext, m_pFBODebug);
#endif
}

bool GLGridLeaper::CheckForRedraw() {
  // can't draw to a size zero window.
  if (m_vWinSize.area() == 0) return false; 

  if (m_bPerformReCompose) return true;

  // if we have not converged yet redraw immediately
  // TODO: after finished implementing and debugging
  // we should be using the m_iCheckCounter here similar
  // to AbstrRenderer::CheckForRedraw() 
  if (!m_bConverged) return true; 

  for (size_t i=0; i < renderRegions.size(); ++i) {
    const std::shared_ptr<RenderRegion> region = renderRegions[i];
    if (region->isBlank) return true;
  }

  return false;
}

uint32_t GLGridLeaper::UpdateToVolumePool(const UINTVECTOR4& brick) {
  std::vector<UINTVECTOR4> bricks;
  bricks.push_back(brick);
  return UpdateToVolumePool(bricks);
}

uint32_t GLGridLeaper::UpdateToVolumePool(std::vector<UINTVECTOR4>& hash) {
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
  
  return m_pVolumePool->UploadBricks(hash, m_bDebugBricks);
}

bool GLGridLeaper::Render3DRegion(RenderRegion3D& rr) {
  Timer renTime; renTime.Start();
  glClearColor(0,0,0,0);

  size_t iStereoBufferCount = (m_bDoStereoRendering) ? 2 : 1;

  // prepare a new view
  if (rr.isBlank || m_bAveragingFrameTimes) {
    if (rr.isBlank)
      m_bAveragingFrameTimes = false;

    //OTHER("Preparing new view");
    m_iSubframes = 0;
    m_iPagedBricks = 0;

#ifdef GLGRIDLEAPER_WORKINGSET
    // clear the info hash table at the beginning of every frame
    m_pWorkingSetTable->ClearData();
#endif

    for (size_t i = 0;i<iStereoBufferCount;i++) {
      // compute new ray start
      m_Timer.Start();
      FillRayEntryBuffer(rr,EStereoID(i));
      msecPassedCurrentFrame = 0;
    }
  }

  // clear hashtable
  m_pglHashTable->ClearData();

  for (size_t i = 0;i<iStereoBufferCount;i++) {
    // reset state
    GPUState localState = m_BaseState;
    localState.enableBlend = false;
    m_pContext->GetStateManager()->Apply(localState);

    // do raycasting
    Raycast(rr, EStereoID(i));
  }

  // evaluate hashtable
  std::vector<UINTVECTOR4> hash = m_pglHashTable->GetData();

  // upload missing bricks
  if (!m_pVolumePool->IsVisibilityUpdated() || !hash.empty())
    m_iPagedBricks += UpdateToVolumePool(hash);

  // conditional measurements
  if (!hash.empty()) {
    float const t = float(m_Timer.Elapsed());
    if (m_pBrickAccess) {
      // report used bricks
      *m_pBrickAccess << " Subframe=" << m_iSubframes;
      *m_pBrickAccess << " PagedBrickCount=" << hash.size() << std::endl;
      for (size_t i=0; i<hash.size(); ++i) {
        *m_pBrickAccess << hash[i] << " ";
      }
      *m_pBrickAccess << std::endl;
    }
    OTHER("subframe %d took %.2f ms and %d bricks were paged in",
      m_iSubframes, t, m_iPagedBricks);
    m_iSubframes++;
  } else {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);
    float const fFrameTime = float(m_Timer.Elapsed()); // final frame completed

    if (m_iAveragingFrameCount) {
      if (m_iPagedBricks || !m_bAveragingFrameTimes || !m_pVolumePool->IsVisibilityUpdated()) {
        m_bConverged = false;
        m_bAveragingFrameTimes = true;
        m_FrameTimes = AvgMinMaxTracker<float>(m_FrameTimes.GetMaxHistoryLength()); // clear
        return true; // quick exit to start averaging
      }
    }

    // debug output
    m_FrameTimes.Push(fFrameTime);
    ss << "Total frame (with " << m_iSubframes << " subframes) took " << fFrameTime
      << " ms to render (" << 1000.f / fFrameTime << " FPS)   "
      << " Average of the last " << m_FrameTimes.GetHistroryLength()
      << " frame times: " << m_FrameTimes.GetAvgMinMax() << "   "
      << " Total paged bricks: " << m_iPagedBricks << " ("
      << m_pVolumePool->GetMaxUsedBrickBytes() * m_iPagedBricks / 1024.f / 1024.f << " MB)   ";

    bool const bWriteToLogFileForEveryFrame = m_pLogFile && !m_iAveragingFrameCount;
    if (bWriteToLogFileForEveryFrame) {
      *m_pLogFile << 1000.f/fFrameTime << ";\t" // avg FPS
                  << "1;\t" // avg sample count
                  << fFrameTime << ";\t" // avg frame time (ms)
                  << fFrameTime << ";\t" // min frame time (ms)
                  << fFrameTime << ";\t" // max frame time (ms)
                  << m_iSubframes << ";\t" // subframe count
                  << m_iPagedBricks << ";\t" // paged in brick count
                  << m_pVolumePool->GetMaxUsedBrickBytes() * m_iPagedBricks / 1024.f / 1024.f << ";\t"; // paged in memory (MB)
    }
    if (m_pBrickAccess) {
      *m_pBrickAccess << " Frame=" << ++m_iFrameCount;
      *m_pBrickAccess << " TotalPagedBrickCount=" << m_iPagedBricks;
      *m_pBrickAccess << " TotalSubframeCount=" << m_iSubframes << std::endl;
    }

#ifdef GLGRIDLEAPER_WORKINGSET
    std::vector<UINTVECTOR4> vUsedBricks = m_pWorkingSetTable->GetData();
    // compute accurate working set size
    const uint64_t iBytesPerVoxel = (m_pDataset->GetBitWidth() / 8) * m_pDataset->GetComponentCount();
    uint64_t iAccurateGpuWorkingSetBytes = 0;
    for (auto usedBrick=vUsedBricks.cbegin(); usedBrick!=vUsedBricks.cend(); usedBrick++) {
      BrickKey const key = m_pToCDataset->IndexFrom4D(*usedBrick, 0);
      UINTVECTOR3 const vVoxels = m_pToCDataset->GetBrickVoxelCounts(key);
      iAccurateGpuWorkingSetBytes += vVoxels.volume() * iBytesPerVoxel;
    }
    // debug output
    ss << "Working set bricks for optimal frame: " << vUsedBricks.size() << " ("
       << iAccurateGpuWorkingSetBytes / 1024.f / 1024.f << " MB)";

    if (bWriteToLogFileForEveryFrame) {
      *m_pLogFile << vUsedBricks.size() << ";\t" // working set brick count
                  << iAccurateGpuWorkingSetBytes / 1024.f / 1024.f << ";\t"; // working set memory (MB)
    }
#endif

    // debug output
    OTHER("%s", ss.str().c_str());

    if (bWriteToLogFileForEveryFrame) {
      *m_pLogFile << std::endl;
    }

#ifdef GLGRIDLEAPER_DEBUGVIEW
    if (m_iDebugView == 2)
      std::swap(m_pFBODebug, m_pFBO3DImageNext[0]); // always use first eye
#endif
  }

  if (!m_iAveragingFrameCount) {
    m_bConverged = hash.empty();
    m_bAveragingFrameTimes = false;
  } else {
    // we want absolute frame times without paging that's why we
    // re-render a couple of times after we converged and write averaged stats to log file
    if (m_FrameTimes.GetHistroryLength() >= m_iAveragingFrameCount) {
      if (m_pLogFile) {
        *m_pLogFile << 1000.f/m_FrameTimes.GetAvg() << ";\t" // avg FPS
                    << m_FrameTimes.GetHistroryLength() << ";\t" // avg sample count
                    << m_FrameTimes.GetAvg() << ";\t" // avg frame time (ms)
                    << m_FrameTimes.GetMin() << ";\t" // min frame time (ms)
                    << m_FrameTimes.GetMax() << ";\t" // max frame time (ms)
                    << m_iSubframes << ";\t" // subframe count
                    << m_iPagedBricks << ";\t" // paged in brick count
                    << m_pVolumePool->GetMaxUsedBrickBytes() * m_iPagedBricks / 1024.f / 1024.f << ";\t"; // paged in memory (MB)
#ifdef GLGRIDLEAPER_WORKINGSET
        std::vector<UINTVECTOR4> vUsedBricks = m_pWorkingSetTable->GetData();
        // compute accurate working set size
        const uint64_t iBytesPerVoxel = (m_pDataset->GetBitWidth() / 8) * m_pDataset->GetComponentCount();
        uint64_t iAccurateGpuWorkingSetBytes = 0;
        for (auto usedBrick=vUsedBricks.cbegin(); usedBrick!=vUsedBricks.cend(); usedBrick++) {
          BrickKey const key = m_pToCDataset->IndexFrom4D(*usedBrick, 0);
          UINTVECTOR3 const vVoxels = m_pToCDataset->GetBrickVoxelCounts(key);
          iAccurateGpuWorkingSetBytes += vVoxels.volume() * iBytesPerVoxel;
        }
        *m_pLogFile << vUsedBricks.size() << ";\t" // working set brick count
                    << iAccurateGpuWorkingSetBytes / 1024.f / 1024.f << ";\t"; // working set memory (MB)
#endif
        *m_pLogFile << std::endl;
      }
      if (m_pBrickAccess) {
        *m_pBrickAccess << " Frame=" << ++m_iFrameCount;
        *m_pBrickAccess << " TotalPagedBrickCount=" << m_iPagedBricks;
        *m_pBrickAccess << " TotalSubframeCount=" << m_iSubframes << std::endl;
      }
      m_bConverged = true;
      m_bAveragingFrameTimes = false;
    } else {
      m_bConverged = false;
    }
  }
  m_RenderingTime = renTime.Elapsed();

    if (m_eRenderMode == RM_ISOSURFACE) {
        for (size_t i = 0;i<iStereoBufferCount;i++) {
          m_TargetBinder.Bind(m_pFBO3DImageNext[i]);
          glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
          ComposeSurfaceImage(rr, EStereoID(i));
        } 
        m_TargetBinder.Unbind();
    }

  // always display intermediate results
  return true;
}

void GLGridLeaper::SetInterpolant(Interpolant eInterpolant) {  
  GLGPURayTraverser::SetInterpolant(eInterpolant);
  m_pVolumePool->SetFilterMode(ComputeGLFilter());
}

void GLGridLeaper::PH_ClearWorkingSet() {
  m_pVolumePool->PH_Reset(m_VisibilityState, m_iTimestep);
  ScheduleWindowRedraw(GetFirst3DRegion().get());
}
void GLGridLeaper::PH_RecalculateVisibility() {
  RecomputeBrickVisibility(true);
}
bool GLGridLeaper::PH_Converged() const {
  return m_bConverged;
}

bool GLGridLeaper::PH_OpenBrickAccessLogfile(const std::string& filename) {
  if (m_pBrickAccess)
    return false; // we already have a file do close and open
  std::string logFilename = filename;
  std::string dsName;

  if (filename.empty()) {
    try {
      const FileBackedDataset& ds = dynamic_cast<const FileBackedDataset&>
                                                (*m_pToCDataset);
      dsName = SysTools::RemoveExt(ds.Filename());
      logFilename = dsName + "_log.ba";
    } catch(const std::bad_cast&) {
      return false; // we do not know which file to open
    }
  }

  logFilename = SysTools::FindNextSequenceName(logFilename);
  m_pBrickAccess = new std::ofstream(logFilename);

  // write header
  *m_pBrickAccess << std::fixed << std::setprecision(5);
  if (m_pToCDataset) {
    *m_pBrickAccess << "Filename=" << dsName << std::endl;
    *m_pBrickAccess << "MaxBrickSize=" << m_pToCDataset->GetMaxBrickSize() << std::endl;
    *m_pBrickAccess << "BrickOverlap=" << m_pToCDataset->GetBrickOverlapSize() << std::endl;
    *m_pBrickAccess << "LoDCount=" << m_pToCDataset->GetLODLevelCount() << std::endl;
    for (uint64_t lod=0; lod < m_pToCDataset->GetLODLevelCount(); ++lod) {
      *m_pBrickAccess << " LoD=" << lod;
      *m_pBrickAccess << " DomainSize=" << m_pToCDataset->GetDomainSize(lod);
      *m_pBrickAccess << " BrickCount=" << m_pToCDataset->GetBrickLayout(lod, 0);
      *m_pBrickAccess << std::endl;
    }
  }
  *m_pBrickAccess << std::endl;
  m_iFrameCount = 0;

  // single largest brick is always chached, just print it here for completeness
  *m_pBrickAccess << " Subframe=" << 0;
  *m_pBrickAccess << " PagedBrickCount=" << 1 << std::endl;
  *m_pBrickAccess << UINTVECTOR4(0,0,0, uint32_t(m_pToCDataset->GetLargestSingleBrickLOD(m_iTimestep))) << " ";
  *m_pBrickAccess << std::endl;
  *m_pBrickAccess << " Frame=" << 0;
  *m_pBrickAccess << " TotalPagedBrickCount=" << 1;
  *m_pBrickAccess << " TotalSubframeCount=" << 1 << std::endl;

  return true;
}

bool GLGridLeaper::PH_CloseBrickAccessLogfile() {
  if (m_pBrickAccess) {
    m_pBrickAccess->close();
    delete m_pBrickAccess;
    m_pBrickAccess = NULL;
    return true;
  } else {
    return false;
  }
}

bool GLGridLeaper::PH_OpenLogfile(const std::string& filename) {
  if (m_pLogFile)
    return false; // we already have a file do close and open
  std::string logFilename = filename;

  if (filename.empty()) {
    try {
      const FileBackedDataset& ds = dynamic_cast<const FileBackedDataset&>
                                                (*m_pToCDataset);
      logFilename = SysTools::RemoveExt(ds.Filename()) + "_log.csv";
    } catch(const std::bad_cast&) {
      return false; // we do not know which file to open
    }
  }

  logFilename = SysTools::FindNextSequenceName(logFilename);
  m_pLogFile = new std::ofstream(logFilename);

  // write header
  *m_pLogFile << std::fixed << std::setprecision(5);
  *m_pLogFile << "avg FPS;\t"
              << "avg sample count;\t"
              << "avg frame time (ms);\t"
              << "min frame time (ms);\t"
              << "max frame time (ms);\t"
              << "subframe count;\t"
              << "paged in brick count;\t"
              << "paged in memory (MB);\t";
#ifdef GLGRIDLEAPER_WORKINGSET
  *m_pLogFile << "working set brick count;\t"
              << "working set memory (MB);\t";
#endif
  *m_pLogFile << std::endl;

  return true;
}

bool GLGridLeaper::PH_CloseLogfile() {
  if (m_pLogFile) {
    m_pLogFile->close();
    delete m_pLogFile;
    m_pLogFile = NULL;
    return true;
  } else {
    return false;
  }
}

void GLGridLeaper::PH_SetOptimalFrameAverageCount(size_t iValue) {
  m_iAveragingFrameCount = iValue;
}

size_t GLGridLeaper::PH_GetOptimalFrameAverageCount() const {
  return m_iAveragingFrameCount;
}

bool GLGridLeaper::PH_IsDebugViewAvailable() const {
#ifdef GLGRIDLEAPER_DEBUGVIEW
  return true;
#else
  return false;
#endif
}

bool GLGridLeaper::PH_IsWorkingSetTrackerAvailable() const {
#ifdef GLGRIDLEAPER_WORKINGSET
  return true;
#else
  return false;
#endif
}

uint32_t GLGridLeaper::GetDebugViewCount() const {
#ifdef GLGRIDLEAPER_DEBUGVIEW
  return 3;
#else
  return 2;
#endif
}

void GLGridLeaper::SetDebugView(uint32_t iDebugView) {

  // recompile shaders
  CleanupTraversalShaders();
  std::vector<std::string> vDefines;

  switch (iDebugView) {
  case 0:
    break; // no debug mode
  case 1:
    vDefines.push_back("#define COLOR_LODS");
    break;
  default:
    // should only happen if GLGRIDLEAPER_DEBUGVIEW is defined
    vDefines.push_back("#define DEBUG");
    break;
  }

  if (!LoadTraversalShaders(vDefines)) {
    T_ERROR("could not reload traversal shaders");
  }

  AbstrRenderer::SetDebugView(iDebugView);
}

void GLGridLeaper::SetClipPlane(RenderRegion *renderRegion,
                                   const ExtendedPlane& plane) {
  GLGPURayTraverser::SetClipPlane(renderRegion, plane);
  FillBBoxVBO();
}

void GLGridLeaper::EnableClipPlane(RenderRegion *renderRegion) {
  GLGPURayTraverser::EnableClipPlane(renderRegion);
  FillBBoxVBO();
}

void GLGridLeaper::DisableClipPlane(RenderRegion *renderRegion) {
  GLGPURayTraverser::DisableClipPlane(renderRegion);
  FillBBoxVBO();
}

void GLGridLeaper::ClipPlaneRelativeLock(bool bRelative) {
  GLGPURayTraverser::ClipPlaneRelativeLock(bRelative);
  FillBBoxVBO();
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
