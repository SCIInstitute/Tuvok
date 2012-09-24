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

#ifdef GLTREERAYCASTER_WRITE_LOG
#include "Basics/SysTools.h"
#endif

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
  m_pProgramRayCast1DColor(NULL),
  m_pProgramRayCast1DLightingColor(NULL),
  m_pProgramRayCast2DColor(NULL),
  m_pProgramRayCast2DLightingColor(NULL),
  m_pToCDataset(NULL),
  m_bConverged(true),
  m_VisibilityState()
#ifdef GLTREERAYCASTER_DEBUGVIEW
  , m_pFBODebug(NULL)
  , m_pFBODebugNext(NULL)
#endif
  , m_iSubframes(0)
  , m_iPagedBricks(0)
#ifdef GLTREERAYCASTER_PROFILE
  , m_FrameTimes(100)
#ifdef GLTREERAYCASTER_AVG_FPS
  , m_bAveraging(false)
#endif
#endif
  , m_pWorkingSetTable(NULL)
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
  // upload a brick that covers the entire domain to make sure have
  // something to render

  if (m_vUploadMem.empty()) {
  // if loading a brick for the first time, prepare upload mem
    const UINTVECTOR3 vSize = m_pToCDataset->GetMaxBrickSize();
    const uint64_t iByteWidth = m_pToCDataset->GetBitWidth()/8;
    const uint64_t iCompCount = m_pToCDataset->GetComponentCount();
    const uint64_t iBrickSize = vSize.volume() * iByteWidth * iCompCount;
    m_vUploadMem.resize(static_cast<size_t>(iBrickSize));
  }

  // find lowest LoD with only a single brick
  const BrickKey bkey = m_pToCDataset->TOCVectorToKey(
    UINTVECTOR4(0,0,0,
        uint32_t(m_pToCDataset->GetLargestSingleBrickLod(m_iTimestep))),
    m_iTimestep
  );

  m_pToCDataset->GetBrick(bkey, m_vUploadMem);
  m_pVolumePool->UploadFirstBrick(m_pToCDataset->GetBrickVoxelCounts(bkey),
                                  &m_vUploadMem[0]);

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

#ifdef GLTREERAYCASTER_WRITE_LOG
  std::string const strLogFilename = SysTools::FindNextSequenceName(SysTools::RemoveExt(m_pToCDataset->Filename()) + "_log.txt");
  m_LogFile.open(strLogFilename);
#ifdef GLTREERAYCASTER_PROFILE
#ifdef GLTREERAYCASTER_AVG_FPS
  m_LogFile << "avg frame time (ms); "
            << "avg FPS; "
            << "avg sample count; "
            << "min frame time (ms); "
            << "max frame time (ms); ";
#else
  m_LogFile << "frame time (ms); "
            << "FPS; "
            << "subframe count; "
            << "paged in brick count; "
            << "paged in memory (MB); ";
#endif
#endif
#ifdef GLTREERAYCASTER_WORKINGSET
  m_LogFile << "working set brick count; "
            << "working set memory (MB); ";
#endif
  m_LogFile << std::endl;
#endif // GLTREERAYCASTER_WRITE_LOG

  return true;
}
GLTreeRaycaster::~GLTreeRaycaster() {
  delete m_pglHashTable; m_pglHashTable = NULL;
  delete m_pWorkingSetTable; m_pWorkingSetTable = NULL;
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

#ifdef GLTREERAYCASTER_DEBUGVIEW
  deleteFBO(m_pFBODebug);
  deleteFBO(m_pFBODebugNext);
#endif

  delete m_pBBoxVBO;
  m_pBBoxVBO = NULL;
  delete m_pNearPlaneQuad;
  m_pNearPlaneQuad = NULL;

  if (m_pglHashTable) {
    m_pglHashTable->FreeGL();
    delete m_pglHashTable;
    m_pglHashTable = NULL;
  }

#ifdef GLTREERAYCASTER_WORKINGSET
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

#ifdef GLTREERAYCASTER_DEBUGVIEW
    recreateFBO(m_pFBODebug, m_pContext ,m_vWinSize, intformat, GL_RGBA, type);
    recreateFBO(m_pFBODebugNext, m_pContext ,m_vWinSize, intformat, GL_RGBA, type);
#endif
  }
}

bool GLTreeRaycaster::Initialize(std::shared_ptr<Context> ctx) {
  if (!GLRenderer::Initialize(ctx)) {
    T_ERROR("Error in parent call -> aborting");
    return false;
  }

  UINTVECTOR3 const finestBrickLayout(m_pToCDataset->GetBrickLayout(0, 0));
  
  m_pglHashTable = new GLHashTable(finestBrickLayout, 511,
    Controller::ConstInstance().PHState.RehashCount
  );
  m_pglHashTable->InitGL();

#ifdef GLTREERAYCASTER_WORKINGSET
  m_pWorkingSetTable = new GLHashTable(
    finestBrickLayout, finestBrickLayout.volume() *
                       uint32_t(m_pToCDataset->GetLargestSingleBrickLod(0)),
    Controller::ConstInstance().PHState.RehashCount, true, "workingSet"
  );
  m_pWorkingSetTable->InitGL();
#endif

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


bool GLTreeRaycaster::LoadTraversalShaders() {

#ifdef GLTREERAYCASTER_WORKINGSET
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
  vs.push_back(FindFileInDirs("GLTreeRaycaster-entry-VS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-blend.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-Method-1D.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("Compositing.glsl", m_vShaderSearchDirs, false));
  ShaderDescriptor sd(vs, fs);
#ifdef GLTREERAYCASTER_DEBUGVIEW
  sd.AddDefine("#define DEBUG");
#endif
  sd.AddFragmentShaderString(poolFragment);
  sd.AddFragmentShaderString(hashFragment);
#ifdef GLTREERAYCASTER_WORKINGSET
  sd.AddFragmentShaderString(infoFragment);
#endif
  if (!LoadCheckShader(&m_pProgramRayCast1D, sd, "1D TF")) return false;

  vs.clear(); fs.clear();
  vs.push_back(FindFileInDirs("GLTreeRaycaster-entry-VS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-blend.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-Method-1D-color.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("Compositing.glsl", m_vShaderSearchDirs, false));
  sd = ShaderDescriptor(vs, fs);
#ifdef GLTREERAYCASTER_DEBUGVIEW
  sd.AddDefine("#define DEBUG");
#endif
  sd.AddFragmentShaderString(poolFragment);
  sd.AddFragmentShaderString(hashFragment);
#ifdef GLTREERAYCASTER_WORKINGSET
  sd.AddFragmentShaderString(infoFragment);
#endif
  if (!LoadCheckShader(&m_pProgramRayCast1DColor, sd, "Color 1D TF")) return false;

  vs.clear(); fs.clear();
  vs.push_back(FindFileInDirs("GLTreeRaycaster-entry-VS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-blend.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-Method-1D-L.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-GradientTools.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("lighting.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("Compositing.glsl", m_vShaderSearchDirs, false));
  sd = ShaderDescriptor(vs, fs);
#ifdef GLTREERAYCASTER_DEBUGVIEW
  sd.AddDefine("#define DEBUG");
#endif
  sd.AddFragmentShaderString(poolFragment);
  sd.AddFragmentShaderString(hashFragment);
#ifdef GLTREERAYCASTER_WORKINGSET
  sd.AddFragmentShaderString(infoFragment);
#endif
  if (!LoadCheckShader(&m_pProgramRayCast1DLighting, sd, "1D TF lighting")) return false;

  vs.clear(); fs.clear();
  vs.push_back(FindFileInDirs("GLTreeRaycaster-entry-VS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-blend.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-Method-1D-L-color.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-GradientTools.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("lighting.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("Compositing.glsl", m_vShaderSearchDirs, false));
  sd = ShaderDescriptor(vs, fs);
#ifdef GLTREERAYCASTER_DEBUGVIEW
  sd.AddDefine("#define DEBUG");
#endif
  sd.AddFragmentShaderString(poolFragment);
  sd.AddFragmentShaderString(hashFragment);
#ifdef GLTREERAYCASTER_WORKINGSET
  sd.AddFragmentShaderString(infoFragment);
#endif
  if (!LoadCheckShader(&m_pProgramRayCast1DLightingColor, sd, "Color 1D TF lighting")) return false;

  vs.clear(); fs.clear();
  vs.push_back(FindFileInDirs("GLTreeRaycaster-entry-VS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-blend.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-Method-2D.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-GradientTools.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("Compositing.glsl", m_vShaderSearchDirs, false));
  sd = ShaderDescriptor(vs, fs);
#ifdef GLTREERAYCASTER_DEBUGVIEW
  sd.AddDefine("#define DEBUG");
#endif
  sd.AddFragmentShaderString(poolFragment);
  sd.AddFragmentShaderString(hashFragment);
#ifdef GLTREERAYCASTER_WORKINGSET
  sd.AddFragmentShaderString(infoFragment);
#endif
  if (!LoadCheckShader(&m_pProgramRayCast2D, sd, "2D TF")) return false;

  vs.clear(); fs.clear();
  vs.push_back(FindFileInDirs("GLTreeRaycaster-entry-VS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-blend.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-Method-2D-color.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-GradientTools.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("Compositing.glsl", m_vShaderSearchDirs, false));
  sd = ShaderDescriptor(vs, fs);
#ifdef GLTREERAYCASTER_DEBUGVIEW
  sd.AddDefine("#define DEBUG");
#endif
  sd.AddFragmentShaderString(poolFragment);
  sd.AddFragmentShaderString(hashFragment);
#ifdef GLTREERAYCASTER_WORKINGSET
  sd.AddFragmentShaderString(infoFragment);
#endif
  if (!LoadCheckShader(&m_pProgramRayCast2DColor, sd, "Color 2D TF")) return false;

  vs.clear(); fs.clear();
  vs.push_back(FindFileInDirs("GLTreeRaycaster-entry-VS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-blend.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-Method-2D-L.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-GradientTools.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("lighting.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("Compositing.glsl", m_vShaderSearchDirs, false));
  sd = ShaderDescriptor(vs, fs);
#ifdef GLTREERAYCASTER_DEBUGVIEW
  sd.AddDefine("#define DEBUG");
#endif
  sd.AddFragmentShaderString(poolFragment);
  sd.AddFragmentShaderString(hashFragment);
#ifdef GLTREERAYCASTER_WORKINGSET
  sd.AddFragmentShaderString(infoFragment);
#endif
  if (!LoadCheckShader(&m_pProgramRayCast2DLighting, sd, "2D TF lighting")) return false;

  vs.clear(); fs.clear();
  vs.push_back(FindFileInDirs("GLTreeRaycaster-entry-VS.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-blend.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-Method-2D-L-color.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("GLTreeRaycaster-GradientTools.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("lighting.glsl", m_vShaderSearchDirs, false));
  fs.push_back(FindFileInDirs("Compositing.glsl", m_vShaderSearchDirs, false));
  sd = ShaderDescriptor(vs, fs);
#ifdef GLTREERAYCASTER_DEBUGVIEW
  sd.AddDefine("#define DEBUG");
#endif
  sd.AddFragmentShaderString(poolFragment);
  sd.AddFragmentShaderString(hashFragment);
#ifdef GLTREERAYCASTER_WORKINGSET
  sd.AddFragmentShaderString(infoFragment);
#endif
  if (!LoadCheckShader(&m_pProgramRayCast2DLightingColor, sd, "Color 2D TF lighting")) return false;

  return true;
}


void GLTreeRaycaster::CleanupTraversalShaders() {
  CleanupShader(&m_pProgramRayCast1D);
  CleanupShader(&m_pProgramRayCast1DLighting);
  CleanupShader(&m_pProgramRayCast2D);
  CleanupShader(&m_pProgramRayCast2DLighting);
  CleanupShader(&m_pProgramRayCast1DColor);
  CleanupShader(&m_pProgramRayCast1DLightingColor);
  CleanupShader(&m_pProgramRayCast2DColor);
  CleanupShader(&m_pProgramRayCast2DLightingColor);
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

  m_TargetBinder.Bind(
#ifdef GLTREERAYCASTER_DEBUGVIEW
    m_pFBODebug, m_pFBODebugNext,
#endif
    m_pFBOStartColor[size_t(eStereoID)], m_pFBORayStart[size_t(eStereoID)]);
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

void GLTreeRaycaster::RecomputeBrickVisibility(bool bForceSynchronousUpdate) {
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


void GLTreeRaycaster::SetupRaycastShader(GLSLProgram* shaderProgram, RenderRegion3D& rr, EStereoID eStereoID) {
  UINTVECTOR3 vDomainSize = UINTVECTOR3(m_pToCDataset->GetDomainSize());
  FLOATVECTOR3 vScale = FLOATVECTOR3(m_pToCDataset->GetScale());
  FLOATVECTOR3 vExtend = FLOATVECTOR3(vDomainSize) * vScale;
  vExtend /= vExtend.maxVal();
  vScale /= vScale.minVal();

  FLOATMATRIX4 emm = ComputeEyeToModelMatrix(rr, eStereoID);

  m_pVolumePool->Enable(m_FrustumCullingLOD.GetLoDFactor(), 
                        vExtend, vScale, shaderProgram); // bound to 3 and 4
  m_pglHashTable->Enable(); // bound to 5
#ifdef GLTREERAYCASTER_DEBUGVIEW
  m_pFBODebug->Read(6);
#endif
#ifdef GLTREERAYCASTER_WORKINGSET
  m_pWorkingSetTable->Enable(); // bound to 7
#endif

  // set shader parameters
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

#if 0
    FLOATVECTOR3 aM = m_cAmbientM.xyz()*m_cAmbientM.w;
    FLOATVECTOR3 dM = m_cDiffuseM.xyz()*m_cDiffuseM.w;
    FLOATVECTOR3 sM = m_cSpecularM.xyz()*m_cSpecularM.w;
#endif
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

void GLTreeRaycaster::Raycast(RenderRegion3D& rr, EStereoID eStereoID) {
  GLSLProgram* shaderProgram = NULL;
  switch (m_eRenderMode) {
    default: 
      if (m_bDoClearView) {
        // TODO: RM_CLEARVIEW
      } else {
        // TODO: RM_ISOSURFACE
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

  m_TargetBinder.Bind(m_pFBO3DImageNext[size_t(eStereoID)],
                      m_pFBOStartColorNext[size_t(eStereoID)],
                      m_pFBORayStartNext[size_t(eStereoID)]
#ifdef GLTREERAYCASTER_DEBUGVIEW
                      , m_pFBODebugNext
#endif
                      );
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
#ifdef GLTREERAYCASTER_DEBUGVIEW
  m_pFBODebug->FinishRead();
#endif

  // done rendering for now
  m_TargetBinder.Unbind();

  // swap current and next resume color
  std::swap(m_pFBOStartColorNext[size_t(eStereoID)], m_pFBOStartColor[size_t(eStereoID)]);
  std::swap(m_pFBORayStartNext[size_t(eStereoID)], m_pFBORayStart[size_t(eStereoID)]);
#ifdef GLTREERAYCASTER_DEBUGVIEW
  std::swap(m_pFBODebugNext, m_pFBODebug);
#endif
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

uint32_t GLTreeRaycaster::UpdateToVolumePool(const UINTVECTOR4& brick) {
  std::vector<UINTVECTOR4> bricks;
  bricks.push_back(brick);
  return UpdateToVolumePool(bricks);
}

uint32_t GLTreeRaycaster::UpdateToVolumePool(std::vector<UINTVECTOR4>& hash) {
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
  
  return m_pVolumePool->UploadBricks(hash, m_vUploadMem);
}

bool GLTreeRaycaster::Render3DRegion(RenderRegion3D& rr) {
  glClearColor(0,0,0,0);

  size_t iStereoBufferCount = (m_bDoStereoRendering) ? 2 : 1;

  // prepare a new view
#ifdef GLTREERAYCASTER_AVG_FPS
  if (rr.isBlank || m_bAveraging) {
    if (rr.isBlank)
      m_bAveraging = false;
#else
  if (rr.isBlank) {
#endif
    for (size_t i = 0;i<iStereoBufferCount;i++) {
#ifdef GLTREERAYCASTER_PROFILE
      //OTHER("Preparing new view");
      m_iSubframes = 0;
      m_iPagedBricks = 0;
#endif
#ifdef GLTREERAYCASTER_WORKINGSET
      // clear the info hash table at the beginning of every pass
      m_pWorkingSetTable->ClearData();
#endif
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
#ifdef GLTREERAYCASTER_PROFILE
    float const t = float(m_Timer.Elapsed());
    OTHER("subframe %d took %.2f ms and %d bricks were paged in",
      m_iSubframes, t, m_iPagedBricks);
    m_iSubframes++;
#endif
  } else {
#if defined(GLTREERAYCASTER_PROFILE) || defined(GLTREERAYCASTER_WORKINGSET)
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);
#ifdef GLTREERAYCASTER_PROFILE
    float const fFrameTime = float(m_Timer.Elapsed());
#ifdef GLTREERAYCASTER_AVG_FPS
    if (m_iPagedBricks || !m_bAveraging || !m_pVolumePool->IsVisibilityUpdated()) {
      m_bConverged = false;
      m_bAveraging = true;
      m_FrameTimes = AvgMinMaxTracker<float>(m_FrameTimes.GetMaxHistoryLength()); // clear
      return true; // quick exit to start averaging
    }
#endif
    m_FrameTimes.Push(fFrameTime);
    ss << "Total frame (with " << m_iSubframes << " subframes) took " << fFrameTime
      << " ms to render (" << 1000.f / fFrameTime << " FPS)   "
      << " Average of the last " << m_FrameTimes.GetHistroryLength()
      << " frame times: " << m_FrameTimes.GetAvgMinMax() << "   "
      << " Total paged bricks: " << m_iPagedBricks << " ("
      << m_vUploadMem.size() * m_iPagedBricks / 1024.f / 1024.f << " MB)   ";
#if defined(GLTREERAYCASTER_WRITE_LOG) && !defined(GLTREERAYCASTER_AVG_FPS)
    m_LogFile << fFrameTime << "; " // frame time
      << 1000.f/fFrameTime << "; " // FPS
      << m_iSubframes << "; " // subframe count
      << m_iPagedBricks << "; " // paged in brick count
      << m_vUploadMem.size() * m_iPagedBricks / 1024.f / 1024.f << "; "; // paged in memory
#endif
#endif // GLTREERAYCASTER_PROFILE
#ifdef GLTREERAYCASTER_WORKINGSET
    std::vector<UINTVECTOR4> vUsedBricks = m_pWorkingSetTable->GetData();
    ss << "Working set bricks for optimal frame: " << vUsedBricks.size() << " ("
      << m_vUploadMem.size() * vUsedBricks.size() / 1024.f / 1024.f << " MB)";
#if defined(GLTREERAYCASTER_WRITE_LOG) && !defined(GLTREERAYCASTER_AVG_FPS)
    m_LogFile << vUsedBricks.size() << "; " // working set brick count
      << m_vUploadMem.size() * vUsedBricks.size() / 1024.f / 1024.f << "; "; // working set memory
#endif
#endif // GLTREERAYCASTER_WORKINGSET
    OTHER("%s", ss.str().c_str());
#endif // defined(GLTREERAYCASTER_PROFILE) || defined(GLTREERAYCASTER_WORKINGSET)
#if defined(GLTREERAYCASTER_WRITE_LOG) && !defined(GLTREERAYCASTER_AVG_FPS)
    m_LogFile << std::endl;
#endif
#ifdef GLTREERAYCASTER_DEBUGVIEW
    if (m_bDebugView)
      std::swap(m_pFBODebug, m_pFBO3DImageNext[0]); // always use first eye
#endif
  }

#ifndef GLTREERAYCASTER_AVG_FPS
  m_bConverged = hash.empty();
#else
  // we want absolute frame times without paging that's why we
  // re-render a couple of times after we converged
  if (m_FrameTimes.GetHistroryLength() >= GLTREERAYCASTER_AVG_FPS) {
#ifdef GLTREERAYCASTER_WRITE_LOG
#ifdef GLTREERAYCASTER_PROFILE
    m_LogFile << m_FrameTimes.GetAvg() << "; " // avg frame time
              << 1000.f/m_FrameTimes.GetAvg() << "; " // avg FPS
              << m_FrameTimes.GetHistroryLength() << "; " // avg sample count
              << m_FrameTimes.GetMin() << "; " // min frame time
              << m_FrameTimes.GetMax() << "; "; // max frame time
#endif
#ifdef GLTREERAYCASTER_WORKINGSET
    std::vector<UINTVECTOR4> vUsedBricks = m_pWorkingSetTable->GetData();
    m_LogFile << vUsedBricks.size() << "; " // working set brick count
              << m_vUploadMem.size() * vUsedBricks.size() / 1024.f / 1024.f << "; "; // working set memory
#endif
    m_LogFile << std::endl;
#endif
    m_bConverged = true;
  } else {
    m_bConverged = false;
  }
#endif

  // always display intermediate results
  return true;
}

void GLTreeRaycaster::SetInterpolant(Interpolant eInterpolant) {  
  GLRenderer::SetInterpolant(eInterpolant);
  m_pVolumePool->SetFilterMode(ComputeGLFilter());
}

void GLTreeRaycaster::PH_ClearWorkingSet() { m_pWorkingSetTable->ClearData(); }
void GLTreeRaycaster::PH_SetPagedBricks(size_t bricks) {
  m_iPagedBricks = bricks;
}
size_t GLTreeRaycaster::PH_FramePagedBricks() const {
  return static_cast<size_t>(m_iPagedBricks);
}
size_t GLTreeRaycaster::PH_SubframePagedBricks() const {
  return 0; /// @todo fixme this info isn't stored now.
}
void GLTreeRaycaster::PH_RecalculateVisibility() {
  RecomputeBrickVisibility(true);
}
bool GLTreeRaycaster::PH_Converged() const { return m_bConverged; }

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
