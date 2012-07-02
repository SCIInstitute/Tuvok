/**
  \brief   Empty program which does OGL init; for simple testing.
*/
#include "StdTuvokDefines.h"
#include <cstdlib>
#include <iostream>
#include <memory>

#include <GL/glew.h>
#include <Renderer/GL/GLInclude.h>

#include "Controller/Controller.h"
#include "context.h"

using namespace tuvok;

int windowWidth = 4, windowHeight = 4;

int main(int, const char *[])
{
  try {
    std::auto_ptr<TvkContext> ctx(TvkContext::Create(windowWidth,windowHeight, 32, 24, 8, true));
    Controller::Instance().DebugOut()->SetOutput(true,true,true,true);

    GL(glClearColor(0.1f, 0.2f, 0.3f, 1.0f));
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    GLubyte* pixels = new GLubyte[windowWidth*windowHeight*4];
    GL(glReadPixels(0, 0, windowWidth, windowHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels));
  
    for (size_t v = 0;v<windowHeight;++v) {
      for (size_t u = 0;u<windowWidth;++u) {
        std::cout << "("  << int(pixels[0+4*(u+v*windowWidth)])
                  << ", " << int(pixels[1+4*(u+v*windowWidth)])
                  << ", " << int(pixels[2+4*(u+v*windowWidth)])
                  << ", " << int(pixels[3+4*(u+v*windowWidth)]) << ") ";
      }
      std::cout << std::endl;
    }


  } catch(const std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

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
