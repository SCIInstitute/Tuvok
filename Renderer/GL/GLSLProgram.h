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
  \file    GLSLProgram.h
  \author  Jens Schneider, Jens Krueger
           SCI Institute, University of Utah
  \date    October 2008
*/
#pragma once

#ifndef GLSLPROGRAM_H
#define GLSLPROGRAM_H

#include "StdTuvokDefines.h"
#include <vector>
#include <string>

/// if enabled, GLSL-compiler warnings are treated as errors
#define GLSLPROGRAM_STRICT

#ifdef _DEBUG
  #define GLSL_DEBUG 1
#endif

/// on windows warn if GLSL_DEBUG differs from _DEBUG (i.e.. if the
/// lines above are not used)
#ifdef _WIN32
  #ifndef _DEBUG
    #ifdef GLSL_DEBUG
      #pragma message("    [GLSLProgram.h] DEBUG ON.\n")
    #endif
  #else
    #ifndef GLSL_DEBUG
      #pragma message("    [GLSLProgram.h] DEBUG OFF.\n")
    #endif
  #endif
#endif

//! Used to specify sources for shaders.
/*! \author Jens Schneider
    \date March 2004 */
typedef enum {
  GLSLPROGRAM_DISK=0, //!< the shader is a C-string containing the file name.
  GLSLPROGRAM_STRING  /*!< the shader sourcecode is specified directly in the
                           given C-string. */
} GLSLPROGRAM_SOURCE;

#include "GLObject.h"
#include <string>
#include <map>

namespace tuvok {

class GLTexture;
class MasterController;
class ShaderDescriptor;

typedef std::map<std::string, int> texMap;

/**
 * Wrapper for handling OpenGL 2.0 conformant program objects.
 * \class GLSLProgram
 * \warning include _before_ you include anything like gl.h, glut.h etc.
 * \warning requires the GL Extension Wrangler (GLEW) library.
 * \author <a href="mailto:jens.schneider@in.tum.de">Jens Schneider</a>
 * \date November 2005
 */
class GLSLProgram : public GLObject {
public:
  GLSLProgram(MasterController* pMasterController);
  virtual ~GLSLProgram();

  /// Loads a series of shaders.
  void Load(const ShaderDescriptor& sd);

  /// Enables this shader for rendering.
  void Enable(void);
  /// Disables all shaders for rendering (use fixed function pipeline again)
  static void Disable(void);
  
  /// Returns the handle of this shader.
  operator GLuint(void) const;
  /// returns true if this program is valid
  bool IsValid(void) const;

  void Set(const char *name, float x) const;
  void Set(const char *name, float x, float y) const;
  void Set(const char *name, float x, float y, float z) const;
  void Set(const char *name, float x, float y, float z, float w) const;
  void Set(const char *name, const float *m, size_t size,
           bool bTranspose=false) const;

  void Set(const char *name, int x) const;
  void Set(const char *name, int x, int y) const;
  void Set(const char *name, int x, int y, int z) const;
  void Set(const char *name, int x, int y, int z, int w) const;
  void Set(const char *name, const int *m, size_t size,
           bool bTranspose=false) const;

  void Set(const char *name, bool x) const;
  void Set(const char *name, bool x, bool y) const;
  void Set(const char *name, bool x, bool y, bool z) const;
  void Set(const char *name, bool x, bool y, bool z, bool w) const;
  void Set(const char *name, const bool *m, size_t size,
           bool bTranspose=false) const;

  /// Sets a texture parameter.
  void SetTexture(const std::string& name, const GLTexture& pTexture);
  /// Force a specific name/texID binding
  void ConnectTextureID(const std::string& name, const int iUnit);

  /// assume near zero CPU memory cost for shaders to avoid any memory
  /// manager from paging out shaders, the 1 is basically only to
  /// detect memory leaks
  virtual uint64_t GetCPUSize() const {return 1;}
  /// assume near zero GPU memory cost for shaders to avoid any memory
  /// manager from paging out shaders, the 1 is basically only to
  /// detect memory leaks
  virtual uint64_t GetGPUSize() const {return 1;}
  static bool m_bGLUseARB;

private:
  bool    Initialize(void);
  GLuint  LoadShader(const char*, GLenum, GLSLPROGRAM_SOURCE src);
  bool    WriteInfoLog(const char*, GLuint, bool);
  bool    CheckGLError(const char *pcError=nullptr,
                       const char *pcAdditional=nullptr) const;
  GLenum get_type(const char *name) const;
  GLint get_location(const char *name) const;
  void CheckType(const char *name, GLenum type) const;
  void CheckSamplerType(const char *name) const;

  MasterController*   m_pMasterController;
  bool                m_bInitialized;
  bool                m_bEnabled;
  GLuint              m_hProgram;
  static bool         m_bGlewInitialized;
  static bool         m_bGLChecked;
  texMap              m_mBindings;
};


//#ifdef GLSL_ALLOW_IMPLICIT_CASTS
//#undef GLSL_ALLOW_IMPLICIT_CASTS
//#endif

};

#endif // GLSLPROGRAM_H
