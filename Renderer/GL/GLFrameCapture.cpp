/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2008 Scientific Computing and Imaging Institute,
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

/**
  \file    GLFrameCapture.cpp
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    August 2008
*/
#include "GLFrameCapture.h"

#include "Basics/Vectors.h"
#include "Controller/Controller.h"
#include "GLInclude.h"
#include "GLFBOTex.h"
#include "GLTargetBinder.h"

using namespace tuvok;


bool GLFrameCapture::CaptureSingleFrame(const std::wstring& strFilename, bool bPreserveTransparency) const {
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);

  // for TIFF capture in 16 bit 
  std::wstring extension = SysTools::ToLowerCase(SysTools::GetExt(strFilename));  
  if (extension == L"tif" || extension == L"tiff") {
    // testing new here to avoid a crash when 
    // trying to capture 4k images on a 32 bit build
    std::vector<uint16_t> image;
    try {
      image.resize(viewport[2]*viewport[3]*4);
    } catch (...) {
      image.clear();
    }
    if ( image.empty() ) return false;
 
    GL(glPixelStorei(GL_PACK_ALIGNMENT, 1));
    GL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    GL(glReadBuffer(GL_COLOR_ATTACHMENT0));
    GL(glReadPixels(0,0,viewport[2],viewport[3],GL_RGBA,GL_UNSIGNED_SHORT,&image[0]));

    return SaveImage(strFilename, UINTVECTOR2(viewport[2], viewport[3]), image, bPreserveTransparency);
  } else {
    // testing new here to avoid a crash when 
    // trying to capture 4k images on a 32 bit build
    std::vector<uint8_t> image;
    try {
      image.resize(viewport[2]*viewport[3]*4);
    } catch (...) {
      image.clear();
    }
    if ( image.empty() ) return false;
 
    GL(glPixelStorei(GL_PACK_ALIGNMENT, 1));
    GL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    GL(glReadBuffer(GL_COLOR_ATTACHMENT0));
    GL(glReadPixels(0,0,viewport[2],viewport[3],GL_RGBA,GL_UNSIGNED_BYTE,&image[0]));

    return SaveImage(strFilename, UINTVECTOR2(viewport[2], viewport[3]), image, bPreserveTransparency);

  }
}

bool GLFrameCapture::CaptureSingleFrame(const std::wstring& filename,
                                        GLFBOTex* from,
                                        bool transparency) const
{
  GLTargetBinder bind(&Controller::Instance());
  bind.Bind(from);
  bool rv = this->CaptureSingleFrame(filename, transparency);
  bind.Unbind();
  return rv;
}

