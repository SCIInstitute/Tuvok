#include "GLVBO.h"

#include <GL/glew.h>
#include "GLInclude.h"

using namespace tuvok;

GLVBO::GLVBO() :
  m_vboHandles(0),
  m_iboHandle(std::make_pair(IBDesc(0,0),0))
{
}

GLVBO::~GLVBO() {
  FreeGL();
}

GLuint GLVBO::GetNewAB() {
  GLuint vboHandle = 0;
  GL(glGenBuffers(1, &vboHandle));
  GL(glBindBuffer(GL_ARRAY_BUFFER, vboHandle));
  return vboHandle;
}

void GLVBO::ClearVertexData() {
  if(m_vboHandles.size()) {
    for (auto buffer = m_vboHandles.begin();buffer<m_vboHandles.end();buffer++) 
      GL(glDeleteBuffers(1, &(buffer->second)));
    m_vboHandles.clear();
  }
}

void GLVBO::FreeGL() {
  ClearVertexData();
  if(m_iboHandle.first.count) {
    GL(glDeleteBuffers(1, &(m_iboHandle.second)));
    m_iboHandle.first.count = 0;
    m_iboHandle.first.type = 0;
    m_iboHandle.second = 0;
  }
}

void GLVBO::Bind() const {
  GLuint i = 0;
  for (auto buffer = m_vboHandles.begin();buffer<m_vboHandles.end();buffer++) {
    GL(glBindBuffer(GL_ARRAY_BUFFER, buffer->second));
    GL(glEnableVertexAttribArray(i));
    GL(glVertexAttribPointer(i++, GLint(buffer->first.elemCount), buffer->first.type, GL_FALSE, 0, NULL));
  }
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_iboHandle.second); 
}

void GLVBO::Draw(GLenum mode) const {
  if (m_vboHandles.empty()) return;
  
  if(m_iboHandle.first.count)
    GL(glDrawElements(mode, GLsizei(m_iboHandle.first.count), m_iboHandle.first.type, 0));
  else
    GL(glDrawArrays(mode, 0, GLsizei(m_vboHandles[0].first.count)));
}

void GLVBO::Draw(GLsizei count, GLenum mode) const {
  if (m_vboHandles.empty()) return;

  if(m_iboHandle.first.count)
    GL(glDrawElements(mode, count, m_iboHandle.first.type, 0));
  else
    GL(glDrawArrays(mode, 0, count));
}

void GLVBO::UnBind() const {
  GLuint i = 0;
  for (auto buffer = m_vboHandles.begin();buffer<m_vboHandles.end();buffer++) {
    GL(glDisableVertexAttribArray(i++));
  }
  GL(glBindBuffer(GL_ARRAY_BUFFER, 0));
  GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0)); 
}

uint64_t GLVBO::GetCPUSize() const {
  return GetGPUSize();
}

uint64_t GLVBO::GetGPUSize() const {
  return 0;
}

void GLVBO::SetIndexData(const std::vector<uint32_t>& indexData) {
  if (indexData.empty()) return;

  if (m_iboHandle.first.count == 0) GL(glGenBuffers(1, &m_iboHandle.second));
  m_iboHandle.first.type = GL_UNSIGNED_INT;
  m_iboHandle.first.count = indexData.size();
  GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_iboHandle.second));
  GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t)*indexData.size(), &(indexData[0]), GL_STATIC_DRAW));
}

void GLVBO::SetIndexData(const std::vector<uint16_t>& indexData) {
  if (indexData.empty()) return;

  if (m_iboHandle.first.count == 0) GL(glGenBuffers(1, &m_iboHandle.second));
  m_iboHandle.first.type = GL_UNSIGNED_SHORT;
  m_iboHandle.first.count = indexData.size();
  GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_iboHandle.second));
  GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint16_t)*indexData.size(), &(indexData[0]), GL_STATIC_DRAW));
}

void GLVBO::AddVertexData(const std::vector<float>& vertexData) {
  if (!vertexData.empty())
    AddVertexData(1, sizeof(float), GL_FLOAT, vertexData.size(), &(vertexData[0]));
}
void GLVBO::AddVertexData(const std::vector<FLOATVECTOR2>& vertexData){
  if (!vertexData.empty())
    AddVertexData(2, sizeof(float), GL_FLOAT, vertexData.size(), &(vertexData[0]));
}
void GLVBO::AddVertexData(const std::vector<FLOATVECTOR3>& vertexData){
  if (!vertexData.empty())
    AddVertexData(3, sizeof(float), GL_FLOAT, vertexData.size(), &(vertexData[0]));
}
void GLVBO::AddVertexData(const std::vector<FLOATVECTOR4>& vertexData){
  AddVertexData(4, sizeof(float), GL_FLOAT, vertexData.size(), &(vertexData[0]));
}

void GLVBO::AddVertexData(const std::vector<int32_t>& vertexData){
  if (!vertexData.empty())
    AddVertexData(1, sizeof(int32_t), GL_INT, vertexData.size(), &(vertexData[0]));
}
void GLVBO::AddVertexData(const std::vector<INTVECTOR2>& vertexData){
  if (!vertexData.empty())
    AddVertexData(2, sizeof(int32_t), GL_INT, vertexData.size(), &(vertexData[0]));
}
void GLVBO::AddVertexData(const std::vector<INTVECTOR3>& vertexData){
  if (!vertexData.empty())
    AddVertexData(3, sizeof(int32_t), GL_INT, vertexData.size(), &(vertexData[0]));
}
void GLVBO::AddVertexData(const std::vector<INTVECTOR4>& vertexData){
  if (!vertexData.empty())
    AddVertexData(4, sizeof(int32_t), GL_INT, vertexData.size(), &(vertexData[0]));
}

void GLVBO::AddVertexData(const std::vector<uint32_t>& vertexData){
  if (!vertexData.empty())
    AddVertexData(1, sizeof(uint32_t), GL_UNSIGNED_INT, vertexData.size(), &(vertexData[0]));
}
void GLVBO::AddVertexData(const std::vector<UINTVECTOR2>& vertexData){
  if (!vertexData.empty())
    AddVertexData(2, sizeof(uint32_t), GL_UNSIGNED_INT, vertexData.size(), &(vertexData[0]));
}
void GLVBO::AddVertexData(const std::vector<UINTVECTOR3>& vertexData){
  if (!vertexData.empty())
    AddVertexData(3, sizeof(uint32_t), GL_UNSIGNED_INT, vertexData.size(), &(vertexData[0]));
}
void GLVBO::AddVertexData(const std::vector<UINTVECTOR4>& vertexData){
  if (!vertexData.empty())
    AddVertexData(4, sizeof(uint32_t), GL_UNSIGNED_INT, vertexData.size(), &(vertexData[0]));
}

void GLVBO::AddVertexData(const std::vector<int8_t>& vertexData){
  if (!vertexData.empty())
    AddVertexData(1, sizeof(int8_t), GL_BYTE, vertexData.size(), &(vertexData[0]));
}
void GLVBO::AddVertexData(const std::vector<VECTOR2<int8_t>>& vertexData){
  if (!vertexData.empty())
    AddVertexData(2, sizeof(int8_t), GL_BYTE, vertexData.size(), &(vertexData[0]));
}
void GLVBO::AddVertexData(const std::vector<VECTOR3<int8_t>>& vertexData){
  if (!vertexData.empty())
    AddVertexData(3, sizeof(int8_t), GL_BYTE, vertexData.size(), &(vertexData[0]));
}
void GLVBO::AddVertexData(const std::vector<VECTOR4<int8_t>>& vertexData){
  if (!vertexData.empty())
    AddVertexData(4, sizeof(int8_t), GL_BYTE, vertexData.size(), &(vertexData[0]));
}

void GLVBO::AddVertexData(const std::vector<uint8_t>& vertexData){
  if (!vertexData.empty())
    AddVertexData(1, sizeof(int8_t), GL_UNSIGNED_BYTE, vertexData.size(), &(vertexData[0]));
}
void GLVBO::AddVertexData(const std::vector<VECTOR2<uint8_t>>& vertexData){
  if (!vertexData.empty())
    AddVertexData(2, sizeof(int8_t), GL_UNSIGNED_BYTE, vertexData.size(), &(vertexData[0]));
}
void GLVBO::AddVertexData(const std::vector<VECTOR3<uint8_t>>& vertexData){
  if (!vertexData.empty())
    AddVertexData(3, sizeof(int8_t), GL_UNSIGNED_BYTE, vertexData.size(), &(vertexData[0]));
}
void GLVBO::AddVertexData(const std::vector<VECTOR4<uint8_t>>& vertexData){
  if (!vertexData.empty())
    AddVertexData(4, sizeof(int8_t), GL_UNSIGNED_BYTE, vertexData.size(), &(vertexData[0]));
}

void GLVBO::AddVertexData(size_t elemCount, size_t elemSize, GLenum type, size_t count, const GLvoid * pointer) {
  GLuint id = GetNewAB();
  m_vboHandles.push_back(std::make_pair(ABDesc(type, elemCount, count), id));
  GL(glBufferData(GL_ARRAY_BUFFER, count*elemCount*elemSize, pointer, GL_STATIC_DRAW));
}

