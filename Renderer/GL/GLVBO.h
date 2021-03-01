#pragma once

#ifndef GLVBO_H
#define GLVBO_H

#include "StdTuvokDefines.h"
#include "GLObject.h"
#include "Basics/Vectors.h"
#include <memory>

namespace tuvok {

  class ABDesc {
  public:
    ABDesc() {}
    ABDesc(GLenum _type, size_t _elemCount, size_t _count) :
      type(_type),
      elemCount(_elemCount),
      count(_count) {}

    GLenum type;
    size_t elemCount;
    size_t count;
  };

  class IBDesc {
  public:
    IBDesc() {}
    IBDesc(GLenum _type, size_t _count) :
      type(_type),
      count(_count) {}

    GLenum type;
    size_t count;
  };  

  class GLVBO : public GLObject {
    public:
      GLVBO();
      virtual ~GLVBO();

      void AddVertexData(const std::vector<float>& vertexData);
      void AddVertexData(const std::vector<FLOATVECTOR2>& vertexData);
      void AddVertexData(const std::vector<FLOATVECTOR3>& vertexData);
      void AddVertexData(const std::vector<FLOATVECTOR4>& vertexData);

      void AddVertexData(const std::vector<int32_t>& vertexData);
      void AddVertexData(const std::vector<INTVECTOR2>& vertexData);
      void AddVertexData(const std::vector<INTVECTOR3>& vertexData);
      void AddVertexData(const std::vector<INTVECTOR4>& vertexData);

      void AddVertexData(const std::vector<uint32_t>& vertexData);
      void AddVertexData(const std::vector<UINTVECTOR2>& vertexData);
      void AddVertexData(const std::vector<UINTVECTOR3>& vertexData);
      void AddVertexData(const std::vector<UINTVECTOR4>& vertexData);

      void AddVertexData(const std::vector<int8_t>& vertexData);
      void AddVertexData(const std::vector<VECTOR2<int8_t>>& vertexData);
      void AddVertexData(const std::vector<VECTOR3<int8_t>>& vertexData);
      void AddVertexData(const std::vector<VECTOR4<int8_t>>& vertexData);

      void AddVertexData(const std::vector<uint8_t>& vertexData);
      void AddVertexData(const std::vector<VECTOR2<uint8_t>>& vertexData);
      void AddVertexData(const std::vector<VECTOR3<uint8_t>>& vertexData);
      void AddVertexData(const std::vector<VECTOR4<uint8_t>>& vertexData);

      void ClearVertexData();

      void SetIndexData(const std::vector<uint16_t>& indexData);
      void SetIndexData(const std::vector<uint32_t>& indexData);

      void FreeGL();

      void Bind() const;
      void Draw(GLsizei count, GLenum mode) const;
      void Draw(GLenum mode) const;
      void UnBind() const;

      virtual uint64_t GetCPUSize() const;
      virtual uint64_t GetGPUSize() const;

    private:
      std::vector<std::pair<ABDesc, GLuint>> m_vboHandles;
      std::pair<IBDesc, GLuint> m_iboHandle;

      GLuint GetNewAB();
      void AddVertexData(size_t elemCount, size_t elemSize, GLenum type, size_t count, const GLvoid * pointer);
  };

}

#endif // GLVBO_H

/*
   The MIT License

   Copyright (c) 2012 Interactive Visualization and Data Analysis Group,
   Saarland University


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
