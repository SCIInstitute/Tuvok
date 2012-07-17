#include "StdTuvokDefines.h"
#include "GLHashTable.h"

#include <GL/glew.h>
#include "GLInclude.h"
#include "GLSLProgram.h"
#include "GLFBOTex.h"
#include "GLTexture1D.h"
#include "../ShaderDescriptor.h"
#include "Basics/nonstd.h"

#include <sstream>

using namespace tuvok;

GLHashTable::GLHashTable(const UINTVECTOR3& maxBrickCount, uint32_t iTableSize, uint32_t iRehashCount) :
  m_maxBrickCount(maxBrickCount),
  m_iTableSize(iTableSize),
  m_iRehashCount(iRehashCount),
  m_pHashTableTex(NULL)
{
  m_rawData = std::tr1::shared_ptr<uint32_t>(
    new uint32_t[m_iTableSize], 
    nonstd::DeleteArray<uint32_t>()
  );
}

GLHashTable::~GLHashTable() {
  FreeGL();
}

void GLHashTable::InitGL() {
  m_pHashTableTex = new GLTexture1D(m_iTableSize, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, 4, NULL);
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

void GLHashTable::Enable(uint32_t iMountPoint) { 
  GL(glBindImageTexture(iMountPoint, m_pHashTableTex->GetGLID(), 0, false, 0, GL_READ_WRITE, GL_R32UI));
}

std::vector<UINTVECTOR4> GLHashTable::GetData() {
// GL(glMemoryBarrier(GL_ALL_BARRIER_BITS));

  m_pHashTableTex->GetData(m_rawData);
  std::vector<UINTVECTOR4> requests;
  for (size_t i = 0;i<m_iTableSize;++i) {
    uint32_t elem = m_rawData.get()[i];
    if (elem != 0) requests.push_back(Int2Vector(elem-1));
  }
  return requests;
}

void GLHashTable::ClearData() {
  std::fill(m_rawData.get(), m_rawData.get()+m_iTableSize, 0);
  m_pHashTableTex->SetData(m_rawData.get());
}

std::string GLHashTable::GetShaderFragment(uint32_t iMountPoint) {
  std::stringstream ss;

  ss << "#version 420 compatibility" << std::endl
     << "" << std::endl
     << "layout(binding = " << iMountPoint << ", size1x32) coherent uniform uimage1D hashTable;" << std::endl
     << "" << std::endl
     << "uint Serialize(uvec4 bd) {" << std::endl
     << "  return 1 + bd.x + bd.y * " << m_maxBrickCount.x << " + bd.z * " << m_maxBrickCount.x * m_maxBrickCount.y << " + bd.w * " << m_maxBrickCount.volume() << ";" << std::endl
     << "}" << std::endl
     << "" << std::endl
     << "int HashValue(uint serializedValue) {" << std::endl
     << "  return int(serializedValue % " << m_iTableSize << ");" << std::endl
     << "}" << std::endl
     << "" << std::endl
     << "uint Hash(uvec4 bd) {" << std::endl
     << "  uint rehashCount = 0;" << std::endl
     << "  uint serializedValue = Serialize(bd);" << std::endl
     << "" << std::endl
     << "  do {" << std::endl
     << "    int hash = HashValue(serializedValue+rehashCount);" << std::endl
     << "    uint valueInImage = imageAtomicCompSwap(hashTable, hash, uint(0), serializedValue);" << std::endl
     << "    if (valueInImage == 0 || valueInImage == serializedValue) return rehashCount;" << std::endl
     << "  } while (++rehashCount < " << m_iRehashCount << ") ;" << std::endl
     << "" << std::endl
     << "  return uint(" << m_iRehashCount << ");" << std::endl
     << "}" << std::endl;
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
