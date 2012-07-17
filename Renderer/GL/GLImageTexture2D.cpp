#include <stdexcept>
#include <GL/glew.h>
#include "GLImageTexture2D.h"
#include "GLCommon.h"
#include "Basics/nonstd.h"

namespace tuvok {

static GLenum textype(GLenum type) {
  // This is basically table X.3 from the shader_image_load_store_EXT document.
  switch(type) {
    case GL_RGBA32F_ARB:
    case GL_RG32F:
    case GL_R32F: return GL_FLOAT;
    case GL_RGBA16F_ARB:
    case GL_RG16F:
    case GL_R16F: return GL_HALF_FLOAT;
    case GL_R11F_G11F_B10F: return GL_UNSIGNED_INT_10F_11F_11F_REV;
    case GL_RGBA32UI_EXT:
    case GL_RG32UI:
    case GL_R32UI: return GL_UNSIGNED_INT;
    case GL_RGBA16UI_EXT:
    case GL_RG16UI:
    case GL_RGBA16:
    case GL_RG16:
    case GL_R16:
    case GL_R16UI: return GL_UNSIGNED_SHORT;
    case GL_RGB10_A2UI: return GL_UNSIGNED_INT_2_10_10_10_REV;
    case GL_RGBA8UI:
    case GL_RG8UI:
    case GL_RGBA8:
    case GL_RG8:
    case GL_R8:
    case GL_R8UI: return GL_UNSIGNED_BYTE;
    case GL_RGBA32I:
    case GL_RG32I: return GL_INT;
    case GL_RGBA16I_EXT:
    case GL_RG16I:
    case GL_R16I: return GL_SHORT;
    case GL_RGBA8I_EXT:
    case GL_RG8I:
    case GL_R8I: return GL_BYTE;
    case GL_RGB10_A2: return GL_UNSIGNED_INT_2_10_10_10_REV;
    case GL_RGBA16_SNORM: return GL_SHORT;
    case GL_RGBA8_SNORM: return GL_BYTE;
    case GL_RG16_SNORM: return GL_SHORT;
    case GL_RG8_SNORM: return GL_BYTE;
    case GL_R16_SNORM: return GL_SHORT;
    case GL_R8_SNORM: return GL_BYTE;
  }
  throw std::domain_error("unknown image store -> texture mapping");
}

static GLenum internaltype(GLenum type) {
  switch(type) {
    case GL_RGBA32F_ARB: return GL_RGBA;
    case GL_RG32F: return GL_RG;
    case GL_R32F: return GL_R;
    case GL_RGBA16F_ARB: return GL_RGBA16;
    case GL_RG16F: return GL_RG16;
    case GL_R16F: return GL_R16;
    case GL_RGBA32UI_EXT: return GL_RGBA;
    case GL_RG32UI: return GL_RG;
    case GL_R32UI: return GL_R;
    case GL_RGBA16UI_EXT: return GL_RGBA16;
    case GL_RG16UI: return GL_RG16;
    case GL_RGBA16: return GL_RGBA16;
    case GL_RG16: return GL_RG16;
    case GL_R16: return GL_R16;
    case GL_R16UI: return GL_R16;
    case GL_RGB10_A2UI: return GL_RGB10_A2;
    case GL_RGBA8UI: return GL_RGBA8;
    case GL_RG8UI: return GL_RG8;
    case GL_RGBA8: return GL_RGBA8;
    case GL_RG8: return GL_RG8;
    case GL_R8: return GL_R8;
    case GL_R8UI: return GL_R8;
    case GL_RGBA32I: return GL_RGBA;
    case GL_RG32I: return GL_RG;
    case GL_RGBA16I_EXT: return GL_RGBA16;
    case GL_RG16I: return GL_RG16;
    case GL_R16I: return GL_R16;
    case GL_RGBA8I_EXT: return GL_RGBA8;
    case GL_RG8I: return GL_RG8;
    case GL_R8I: return GL_R8;
    case GL_RGB10_A2: return GL_RGB10_A2;
    case GL_RGBA16_SNORM: return GL_RGBA16;
    case GL_RGBA8_SNORM: return GL_RGBA8;
    case GL_RG16_SNORM: return GL_RG16;
    case GL_RG8_SNORM: return GL_RG8;
    case GL_R16_SNORM: return GL_R16;
    case GL_R8_SNORM: return GL_R8;
  }
  throw std::domain_error("no format mapping");
}

GLImageTexture2D::GLImageTexture2D(uint32_t iSizeX, uint32_t iSizeY,
                                   GLenum format, GLenum type,
                                   uint32_t iSizePerElement,
                                   const GLvoid *pixels) :
  GLTexture2D(iSizeX, iSizeY, internaltype(type), format, textype(type), iSizePerElement, pixels)
{
}

void GLImageTexture2D::Bind(uint32_t iUnit, uint32_t texture) {
  GLTexture2D::Bind(texture);
  GL(glBindImageTextureEXT(iUnit, texture, /*level*/ 0, GL_TRUE, 0,
                           GL_READ_WRITE, m_type));
}

void GLImageTexture2D::Clear() {
  WARNING("Clearing by setting a giant texture... inefficient.");
  std::shared_ptr<uint8_t> pixels(
    new uint8_t[m_iSizeX*m_iSizeY * gl_components(m_format) *
                gl_byte_width(m_type)],
    nonstd::DeleteArray<uint8_t>()
  );
  this->SetData(pixels.get());
}

} /* namespace tuvok */

/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2012 Scientific Computing and Imaging Institute,
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
