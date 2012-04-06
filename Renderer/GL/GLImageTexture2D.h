#pragma once
#ifndef TUVOK_GL_IMAGE_TEXTURE_2D_H
#define TUVOK_GL_IMAGE_TEXTURE_2D_H

#include "StdTuvokDefines.h"
#include "GLTexture2D.h"

namespace tuvok {

class GLImageTexture2D : public GLTexture2D {
  public:
    GLImageTexture2D(uint32_t iSizeX, uint32_t iSizeY, GLenum format,
                     GLenum type=GL_RGBA16UI_EXT, uint32_t iSizePerElement=16,
                     const GLvoid *pixels = 0);
    virtual ~GLImageTexture2D() {}

    // binds the texture to this img and texture unit.
    virtual void Bind(uint32_t imgUnit, uint32_t texUnit);
    // sets the entire texture to all 0s.
    virtual void Clear();
};

}
#endif // TUVOK_GL_IMAGE_TEXTURE_2D_H

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
