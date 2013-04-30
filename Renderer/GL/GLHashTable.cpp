#include "StdTuvokDefines.h"
#ifdef WRITE_SHADERS
# include <fstream>
# include <iterator>
#endif
#include <sstream>
#include <stdexcept>
#include <GL/glew.h>
#include "Basics/nonstd.h"
#include "IO/UVF/ExtendedOctree/VolumeTools.h"
#include "Controller/StackTimer.h"
#include "GLHashTable.h"
#include "GLInclude.h"
#include "GLSLProgram.h"
#include "GLFBOTex.h"
#include "GLTexture1D.h"
#include "GLTexture2D.h"
#include "Renderer/ShaderDescriptor.h"

using namespace tuvok;

GLHashTable::GLHashTable(const UINTVECTOR3& maxBrickCount, uint32_t iTableSize, uint32_t iRehashCount, bool bUseGLCore, std::string const& strPrefixName) :
  m_strPrefixName(strPrefixName),
  m_maxBrickCount(maxBrickCount),
  m_iTableSize(iTableSize),
  m_iRehashCount(iRehashCount),
  m_pHashTableTex(NULL),
  m_bUseGLCore(bUseGLCore),
  m_iMountPoint(0)
{

}

GLHashTable::~GLHashTable() {
  FreeGL();
}

void GLHashTable::InitGL() {

  int gpumax; 
  GL(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gpumax));

  try {
    m_texSize = VolumeTools::Fit1DIndexTo2DArray(m_iTableSize, gpumax);
  } catch (std::runtime_error const& e) {
    // this is very unlikely but not impossible
    T_ERROR(e.what());
    throw;
  }

  // try to use 1D texture if possible because it appears to be slightly faster than a 2D texture
  if (Is2DTexture()) {
    m_pHashTableTex = new GLTexture2D(m_texSize.x, m_texSize.y, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
  } else {
    assert(m_texSize.x == m_iTableSize);
    m_pHashTableTex = new GLTexture1D(m_texSize.x, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
  }

  m_pRawData = std::shared_ptr<uint32_t>(
    new uint32_t[m_texSize.area()], 
    nonstd::DeleteArray<uint32_t>()
    );
}

UINTVECTOR4 GLHashTable::Int2Vector(uint32_t index) const {
  UINTVECTOR4 coords;

  coords.w = index / m_maxBrickCount.volume(); 
  index -= coords.w*m_maxBrickCount.volume();
  coords.z = index / (m_maxBrickCount.x * m_maxBrickCount.y); 
  index -= coords.z*(m_maxBrickCount.x * m_maxBrickCount.y);
  coords.y = index / m_maxBrickCount.x; 
  index -= coords.y*m_maxBrickCount.x;
  coords.x = index;

  return coords;
}


bool GLHashTable::Is2DTexture() const {
  return m_texSize.y > 1;
}


void GLHashTable::Enable() { 
  GL(glBindImageTexture(m_iMountPoint, m_pHashTableTex->GetGLID(), 0, false, 0, GL_READ_WRITE, GL_R32UI));
}


std::vector<UINTVECTOR4> GLHashTable::GetData() {
  GL(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));
#ifdef GLHASHTABLE_PROFILE
  GL(glFinish());
#endif
  TimedStatement(PERF_READ_HTABLE,
    m_pRawData = std::static_pointer_cast<uint32_t>(m_pHashTableTex->GetData());
  );
  std::vector<UINTVECTOR4> requests;
  StackTimer condense(PERF_CONDENSE_HTABLE);
  for (size_t i = 0;i<m_iTableSize;++i) {
    uint32_t elem = m_pRawData.get()[i];
    if (elem != 0) requests.push_back(Int2Vector(elem-1));
  }
  return requests;
}

void GLHashTable::ClearData() {
  std::fill(m_pRawData.get(), m_pRawData.get()+m_iTableSize, 0);
  m_pHashTableTex->SetData(m_pRawData.get());
}

std::string GLHashTable::GetShaderFragment(uint32_t iMountPoint) {
  m_iMountPoint = iMountPoint;
  std::stringstream ss;

#ifdef WRITE_SHADERS
  const char* shname = "hashtable.glsl";
  std::ifstream shader(shname);
  if(shader) {
    MESSAGE("Reusing hashtable.glsl shader on disk.");
    shader >> std::noskipws;
    std::string sh(
      (std::istream_iterator<char>(shader)),
      (std::istream_iterator<char>())
    );
    shader.close();
    return sh;
  }
#endif

  if (m_bUseGLCore)
    ss << "#version 420 core\n";  
  else
    ss << "#version 420 compatibility\n";

  ss << "\n"
     << "layout(binding = " << iMountPoint << ", size1x32) coherent uniform "
     << (Is2DTexture() ? "uimage2D " : "uimage1D ") << m_strPrefixName << "hashTable;\n"
     << "\n"
     << "uint " << m_strPrefixName << "Serialize(uvec4 bd) {\n"
     << "  return 1 + bd.x + bd.y * " << m_maxBrickCount.x << " + bd.z * "
     <<           m_maxBrickCount.x * m_maxBrickCount.y << " + bd.w * "
     <<           m_maxBrickCount.volume() << ";\n"
     << "}\n"
     << "\n"
     << "uint " << m_strPrefixName << "HashValue(uint serializedValue) {\n"
     << "  return int(serializedValue % " << m_iTableSize << ");\n"
     << "}\n"
     << "\n";

  if (Is2DTexture()) {
    // we are using a 2D texture
    ss << "uint " << m_strPrefixName << "AccessHashTable(uint hashValue, uint serializedValue) {\n"
       << "  ivec2 hashPosition = ivec2(hashValue % " << m_texSize.x << ", "
                                       "hashValue / " << m_texSize.x << ");\n"
       << "  return imageAtomicCompSwap(" << m_strPrefixName << "hashTable,"
                                        "hashPosition, uint(0), serializedValue);\n"
       << "}\n"
       << "\n";
  } else {
    // we are using a 1D texture
    ss << "uint " << m_strPrefixName << "AccessHashTable(uint hashValue, uint serializedValue) {\n"
       << "  int hashPosition = int(hashValue);\n"
       << "  return imageAtomicCompSwap(" << m_strPrefixName << "hashTable,"
                                        "hashPosition, uint(0), serializedValue);\n"
       << "}\n"
       << "\n";
  }

  ss << "uint " << m_strPrefixName << "Hash(uvec4 bd) {\n"
     << "  uint rehashCount = 0;\n"
     << "  uint serializedValue =  " << m_strPrefixName << "Serialize(bd);\n"
     << "\n"
     << "  do {\n"
     << "    uint hash = " << m_strPrefixName << "HashValue(serializedValue+rehashCount);\n"
     << "    uint valueInImage = " << m_strPrefixName << "AccessHashTable(hash, serializedValue);\n"
     << "    if (valueInImage == 0 || valueInImage == serializedValue)\n"
     << "      return rehashCount;\n"
     << "  } while (++rehashCount < " << m_iRehashCount << ");\n"
     << "\n"
     << "  return uint(" << m_iRehashCount << ");\n"
     << "}\n";

#ifdef WRITE_SHADERS
  std::ofstream hashtab(shname, std::ios::trunc);
  if(hashtab) {
    MESSAGE("Writing new hashtable shader.");
    const std::string& s = ss.str();
    std::copy(s.begin(), s.end(), std::ostream_iterator<char>(hashtab, ""));
  }
  hashtab.close();
#endif // WRITE_SHADERS

  return ss.str();
}

void GLHashTable::FreeGL() {
  if (m_pHashTableTex) {
    m_pHashTableTex->Delete();
    delete m_pHashTableTex;
    m_pHashTableTex = NULL;
  }
}

uint64_t GLHashTable::GetCPUSize() const {
  return m_pHashTableTex->GetCPUSize() + m_iTableSize*4;
}
uint64_t GLHashTable::GetGPUSize() const {
  return m_pHashTableTex->GetGPUSize();
}
