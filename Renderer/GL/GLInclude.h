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
  \file    GLInclude.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    August 2008
*/

#pragma once

#ifndef TUVOK_GLINCLUDE_H
#define TUVOK_GLINCLUDE_H

#include "../../StdTuvokDefines.h"
#include <GL/glew.h>

#ifdef WIN32
  #define NOMINMAX
  #include <GL/wglew.h>
  #include <windows.h>
  // undef stupid windows defines to max and min
  #ifdef max
  #undef max
  #endif

  #ifdef min
  #undef min
  #endif
#endif

#include "Controller/Controller.h"

# define GL_RET(stmt)                                                  \
  do {                                                                 \
    GLenum glerr;                                                      \
    while((glerr = glGetError()) != GL_NO_ERROR) {                     \
      T_ERROR("GL error before line %u (%s): %s (%#x)",                \
              __LINE__, __FILE__,                                      \
              gluErrorString(glerr),                                   \
              static_cast<unsigned>(glerr));                           \
    }                                                                  \
    stmt;                                                              \
    while((glerr = glGetError()) != GL_NO_ERROR) {                     \
      T_ERROR("'%s' on line %u (%s) caused GL error: %s (%#x)", #stmt, \
              __LINE__, __FILE__, static_cast<unsigned>(glerr));       \
      return false;                                                    \
    }                                                                  \
  } while(0)

#ifdef _DEBUG
# define GL(stmt)                                                      \
  do {                                                                 \
    GLenum glerr;                                                      \
    while((glerr = glGetError()) != GL_NO_ERROR) {                     \
      T_ERROR("GL error before line %u (%s): %s (%#x)",                \
              __LINE__, __FILE__,                                      \
              gluErrorString(glerr),                                   \
              static_cast<unsigned>(glerr));                           \
    }                                                                  \
    stmt;                                                              \
    while((glerr = glGetError()) != GL_NO_ERROR) {                     \
      T_ERROR("'%s' on line %u (%s) caused GL error: %s (%#x)", #stmt, \
              __LINE__, __FILE__,                                      \
              gluErrorString(glerr),                                   \
              static_cast<unsigned>(glerr));                           \
    }                                                                  \
  } while(0)
#else
# define GL(stmt) do { stmt; } while(0)
#endif

/// macros for tracking glbegin and glend

#ifdef _DEBUG
  #define GLBEGIN(mode)                                                  \
    do {                                                                 \
        MESSAGE("glBegin(%s) on line %u (%s) called ", #mode,            \
                __LINE__,  __FILE__  );                                  \
        glBegin(mode);                                                   \
    } while(0)                                                           

  #define GLEND()                                                        \
    do {                                                                 \
        MESSAGE("glEnd() on line %u (%s) called ",                       \
                __LINE__,  __FILE__  );                                  \
        glEnd();                                                         \
    } while(0)                                                           
#else
  #define glBegin(mode) glBegin(mode)
  #define glEnd() glEnd()                                                
#endif

#endif // TUVOK_GLINCLUDE_H
