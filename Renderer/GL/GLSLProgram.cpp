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
  \file    GLSLProgram.cpp
  \author  Jens Schneider, Jens Krueger
           SCI Institute, University of Utah
  \date    October 2008
*/

#ifdef _WIN32
  #pragma warning( disable : 4996 ) // disable deprecated warning
#endif

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include "GLSLProgram.h"
#include "Controller/Controller.h"
#include "Renderer/ShaderDescriptor.h"
#include "Renderer/GL/GLError.h"
#include "Renderer/GL/GLTexture.h"

using namespace tuvok;
using namespace std;

bool GLSLProgram::m_bGLChecked=false;       ///< use pre GL 2.0 syntax
bool GLSLProgram::m_bGLUseARB=false;       ///< use pre GL 2.0 syntax

// Abstract out the basic ARB/OGL 2.0 shader API differences.  Does not attempt
// to unify the APIs; some calls actually do differ, and error checking changed
// when standardizing,  Clients should check gl::arb and handle these
// differences manually.
namespace gl {
  static bool arb = false;
  GLuint CreateProgram();
  GLuint CreateShader(GLenum type);
  void ShaderSource(GLuint, GLsizei, const GLchar**, const GLint*);
  void CompileShader(GLuint);
  void AttachShader(GLuint, GLuint);
  void DetachShader(GLuint, GLuint);
  GLboolean IsShader(GLuint);
  void UseProgram(GLuint);
  void DeleteShader(GLuint);
  void DeleteProgram(GLuint);
  void GetAttachedShaders(GLuint, GLsizei, GLsizei*, GLuint*);

  GLint GetUniformLocation(GLuint, const GLchar*);

  GLuint CreateProgram() {
    if(arb) { return glCreateProgramObjectARB(); }
    else    { return glCreateProgram(); }
  }

  GLuint CreateShader(GLenum type) {
    assert(type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER);
    if(arb) { return glCreateShaderObjectARB(type); }
    else    { return glCreateShader(type); }
  }

  void ShaderSource(GLuint shader, GLsizei count, const GLchar** str,
                    const GLint*length) {
    if(arb) { glShaderSourceARB(shader, count, str, length); }
    else    { glShaderSource(shader, count, str, length); }
  }

  void CompileShader(GLuint shader) {
    if(arb) { glCompileShaderARB(shader); }
    else    { glCompileShader(shader); }
  }
  void AttachShader(GLuint program, GLuint shader) {
    if(arb) { glAttachObjectARB(program, shader); }
    else    { glAttachShader(program, shader); }
  }
  void DetachShader(GLuint program, GLuint shader) {
    if(arb) { glDetachObjectARB(program, shader); }
    else    { glDetachShader(program, shader); }
  }

  // There's no function of this type in the ARB whatsoever.  So we just hack
  // it for the ARB case.
  GLboolean IsShader(GLuint shader) {
    if(arb) { return shader != 0; }
    else    { return glIsShader(shader); }
  }

  void LinkProgram(GLuint program) {
    if(arb) { glLinkProgramARB(program); }
    else    { glLinkProgram(program); }
  }

  void UseProgram(GLuint shader) {
    if(arb) { glUseProgramObjectARB(shader); }
    else    { glUseProgram(shader); }
  }

  void DeleteShader(GLuint shader) {
    if(arb) { glDeleteObjectARB(shader); }
    else    { glDeleteShader(shader); }
  }

  void DeleteProgram(GLuint p) {
    if(arb) { glDeleteObjectARB(p); }
    else    { glDeleteProgram(p); }
  }

  void GetAttachedShaders(GLuint program, GLsizei mx, GLsizei* count,
                          GLuint* objs) {
    
    // workaround for broken GL implementations
    // some implementations crash if count is a NULL pointer
    // althougth this is perfectly valid according to the spec
    GLsizei dummyCount;   
    if (count == NULL) count = &dummyCount;

    if(arb) {
      glGetAttachedObjectsARB(static_cast<GLhandleARB>(program), mx, count,
                              reinterpret_cast<GLhandleARB*>(objs));
    } else {
      glGetAttachedShaders(program, mx, count, objs);
    }
  }

  GLint GetUniformLocation(GLuint program, const GLchar* name) {
    if(arb) { return glGetUniformLocationARB(program, name); }
    else    { return glGetUniformLocation(program, name); }
  }
}

/**
 * Default Constructor.
 * Initializes glew on first instantiation.
 * \param void
 * \return void
 * \author <a href="mailto:jens.schneider@in.tum.de">Jens Schneider</a>
 * \date Aug.2004
 * \see Initialize()
 */
GLSLProgram::GLSLProgram(MasterController* pMasterController) :
  m_pMasterController(pMasterController),
  m_bInitialized(false),
  m_bEnabled(false),
  m_hProgram(0)
{
  if(!Initialize()) {
    T_ERROR("GL initialization failed!");
  }
}

/// detaches all shaders attached to the given program ID
static void detach_shaders(GLuint program)
{
  GLenum err;

  // clear all other errors in the stack
  do {
    err = glGetError();
    if(err != GL_NO_ERROR) {
      WARNING("Previous GL error: %#x", static_cast<unsigned>(err));
    }
  } while(err != GL_NO_ERROR);

  // how many shaders are attached?
  GLint num_shaders=0;
  if(gl::arb) {
    glGetObjectParameterivARB(program, GL_OBJECT_ATTACHED_OBJECTS_ARB,
                              &num_shaders);
  } else {
    glGetProgramiv(program, GL_ATTACHED_SHADERS, &num_shaders);
  }

  if((err = glGetError()) != GL_NO_ERROR) {
    WARNING("Error obtaining the number of shaders attached to program %u: %x",
            static_cast<unsigned>(program), static_cast<unsigned>(err));
    num_shaders=0;
  }

  if(num_shaders > 0) {
    // get the shader IDs
    std::vector<GLuint> shaders(num_shaders);    
    gl::GetAttachedShaders(program, num_shaders, NULL, &shaders[0]);
    if((err = glGetError()) != GL_NO_ERROR) {
      WARNING("Error obtaining the shader IDs attached to program %u: %#x",
              static_cast<unsigned>(program), static_cast<unsigned>(err));
    }

//    MESSAGE("%d shaders attached to %u", static_cast<int>(num_shaders),
//          static_cast<unsigned>(program));

           
    // detach each shader
    for(std::vector<GLuint>::const_iterator sh = shaders.begin();
        sh != shaders.end(); ++sh) {
      if(gl::IsShader(*sh)) {
        gl::DetachShader(program, *sh);
        if((err = glGetError()) != GL_NO_ERROR) {
          WARNING("Error detaching shader %u from %u: %#x",
                  static_cast<unsigned>(*sh),
                  static_cast<unsigned>(program), static_cast<unsigned>(err));
        }
      }
    }
  }
}

GLSLProgram::~GLSLProgram() {
  if (IsValid() && m_hProgram != 0) {
    detach_shaders(m_hProgram);
    gl::DeleteProgram(m_hProgram);
  }
  m_hProgram=0;
}

/**
 * Returns the handle to this shader.
 * \return a const handle. If this is an invalid shader, the handle will be 0.
 * \author <a href="mailto:jens.schneider@in.tum.de">Jens Schneider</a>
 * \date Feb.2005
 */
GLSLProgram::operator GLuint(void) const {
  return m_hProgram;
}

/**
 * Initializes the class.
 * If GLSLProgram is initialized for the first time, initialize GLEW
 * \param void
 * \return bool
 * \author <a href="mailto:jens.schneider@in.tum.de">Jens Schneider</a>
 * \date Aug.2004
 * \see m_bGlewInitialized
 */
bool GLSLProgram::Initialize(void) {

  if (!m_bGLChecked) {
    MESSAGE("Initializing OpenGL on a: %s",
            (const char*)glGetString(GL_VENDOR));
    if (atof((const char*)glGetString(GL_VERSION)) >= 2.0) {
      MESSAGE("OpenGL 2.0 supported (actual version: \"%s\")", glGetString(GL_VERSION));
      gl::arb = m_bGLUseARB = false;
    } else { // check for ARB extensions
      if (glewGetExtension("GL_ARB_shader_objects"))
        MESSAGE("ARB_shader_objects supported.");
      else {
        T_ERROR("Neither OpenGL 2.0 nor ARB_shader_objects not supported!");
        return false;
      }
      if (glewGetExtension("GL_ARB_shading_language_100"))
        MESSAGE("ARB_shading_language_100 supported.");
      else {
        MESSAGE("Neither OpenGL 2.0 nor ARB_shading_language_100 not supported!");
        return false;
      }

      glUniform1i  = glUniform1iARB;    glUniform2i  = glUniform2iARB;
      glUniform1iv = glUniform1ivARB;   glUniform2iv = glUniform2ivARB;
      glUniform3i  = glUniform3iARB;    glUniform4i  = glUniform4iARB;
      glUniform3iv = glUniform3ivARB;   glUniform4iv = glUniform4ivARB;

      glUniform1f  = glUniform1fARB;    glUniform2f  = glUniform2fARB;
      glUniform1fv = glUniform1fvARB;   glUniform2fv = glUniform2fvARB;
      glUniform3f  = glUniform3fARB;    glUniform4f  = glUniform4fARB;
      glUniform3fv = glUniform3fvARB;   glUniform4fv = glUniform4fvARB;

      glUniformMatrix2fv = glUniformMatrix2fvARB;
      glUniformMatrix3fv = glUniformMatrix3fvARB;
      glUniformMatrix4fv = glUniformMatrix4fvARB;

      MESSAGE("Using ARB functions instead of builtin GL 2.0.");
      gl::arb = m_bGLUseARB = true;
    }
    m_bGLChecked = true;
  }
  return true;
}

static bool attachshader(GLuint program, const std::string& source,
                         const std::string& filename,
                         GLenum shtype)
{
  if(source.empty()) {
    T_ERROR("Empty shader (type %d) '%s'!", static_cast<int>(shtype),
            filename.c_str());
    return false;
  }

  GLuint sh = gl::CreateShader(shtype);
  if(sh == 0) {
    T_ERROR("Error (%d) creating shader (type %d) from '%s'",
            static_cast<int>(glGetError()), static_cast<int>(shtype),
            filename.c_str());
    return false;
  }

  const GLchar* src[1] = { source.c_str() };
  const GLint lengths[1] = { static_cast<GLint>(source.length()) };
  gl::ShaderSource(sh, 1, src, lengths);

  if(glGetError() != GL_NO_ERROR) {
    T_ERROR("Error uploading shader (type %d) source from '%s'",
            static_cast<int>(shtype), filename.c_str());
    gl::DeleteShader(sh);
    return false;
  }

  gl::CompileShader(sh);

  // did it compile successfully?
  {
    GLint success[1] = { GL_TRUE };
    if(!gl::arb) {
      glGetShaderiv(sh, GL_COMPILE_STATUS, success);
    } else {
      glGetObjectParameterivARB(sh, GL_OBJECT_COMPILE_STATUS_ARB, success);
    }
    {
      GLenum glerr = glGetError();
      if(glerr != GL_NO_ERROR) {
        WARNING("GL error looking up compilation status: %#x",
                static_cast<unsigned>(glerr));
        success[0] = GL_FALSE;
      }
    }
    if(success[0] == GL_FALSE) {
      std::ostringstream errmsg;
      errmsg << "Compilation error in '" << filename << "': ";

      // retrieve the error message
      if(!gl::arb) { // ARB calls are different, not used much, we don't care.
        GLint log_length=0;
        glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &log_length);
        // cap err length, for crazy drivers.
        log_length = std::min(static_cast<GLint>(4096), log_length);
        std::vector<GLchar> log(log_length);
        glGetShaderInfoLog(sh, static_cast<GLsizei>(log.size()), NULL, &log[0]);
        errmsg << &log[0];
      }

      gl::DeleteShader(sh);
      T_ERROR("%s", errmsg.str().c_str());
      return false;
    }
  }

//  MESSAGE("Adding shader '%s' to program %u", filename.c_str(),
//          static_cast<unsigned>(program));

  gl::AttachShader(program, sh);
  if(glGetError() != GL_NO_ERROR) {
    T_ERROR("Error attaching shader %u to program %u",
            static_cast<unsigned>(sh), static_cast<unsigned>(program));
    gl::DeleteShader(sh);
    return false;
  }

  // shaders are refcounted; it won't actually be deleted until it is
  // detached from the program object.
  gl::DeleteShader(sh);

  return true;
}

void GLSLProgram::Load(const ShaderDescriptor& sd)
{
  CheckGLError(); // clear previous error status.

  // create the shader program
  this->m_hProgram = gl::CreateProgram();
  if(this->m_hProgram == 0) {
    T_ERROR("Error creating shader program.");
    CheckGLError(_func_);
    gl::DeleteProgram(this->m_hProgram);
    this->m_hProgram = 0;
    return;
  }

  // create a shader for each vertex shader, and attach it to the main program.
  typedef ShaderDescriptor::SIterator si;
  for(si vsh = sd.begin_vertex(); vsh != sd.end_vertex(); ++vsh) {
    if(!attachshader(this->m_hProgram, (*vsh).first, (*vsh).second,
                     GL_VERTEX_SHADER)) {
      T_ERROR("Attaching vertex shader '%s' failed.", (*vsh).second.c_str());
      detach_shaders(this->m_hProgram);
      gl::DeleteProgram(this->m_hProgram);
      this->m_hProgram = 0;
      return;
    }
  }

  // create a shader for each fragment shader, and attach it to the main
  // program.
  for(si fsh = sd.begin_fragment(); fsh != sd.end_fragment(); ++fsh) {
    if(!attachshader(this->m_hProgram, (*fsh).first, (*fsh).second,
                     GL_FRAGMENT_SHADER)) {
      T_ERROR("Attaching fragment shader '%s' failed.", (*fsh).second.c_str());
      detach_shaders(this->m_hProgram);
      gl::DeleteProgram(this->m_hProgram);
      return;
    }
  }

  if(glBindFragDataLocation) {
    for (auto binding = sd.fragmentDataBindings.begin();
         binding < sd.fragmentDataBindings.end(); binding++) {
      GL(glBindFragDataLocation(this->m_hProgram, binding->first, binding->second.c_str()));
    }
  } else {
    T_ERROR("glBindFragDataLocation not supported on this GL version");
  }


  gl::LinkProgram(this->m_hProgram);

  { // figure out if the linking was successful.
    GLint linked = GL_TRUE;
    if(gl::arb) {
      GL(glGetObjectParameterivARB(this->m_hProgram, GL_OBJECT_LINK_STATUS_ARB,
                                &linked));
    } else {
      GL(glGetProgramiv(this->m_hProgram, GL_LINK_STATUS, &linked));
    }

    if(linked != GL_TRUE) {
      GLchar linkerr[2048];
      linkerr[2047]=0;
      GLsizei len=256;
      glGetProgramInfoLog(this->m_hProgram, 2047, &len, linkerr);
      linkerr[len] = 0;
      T_ERROR("Program could not link (%d): '%s'", len, linkerr);
      detach_shaders(this->m_hProgram);
      gl::DeleteProgram(this->m_hProgram);
      this->m_hProgram = 0;
      return;
    }
  }

  m_bInitialized = true;
}

/**
 * Gets the InfoLogARB of hObject and messages it.
 * \param hObject - a handle to the object.
 * \param bProgram - if true, hObject is a program object, otherwise it is a shader object.
 * \return true: InfoLogARB non-empty and GLSLPROGRAM_STRICT defined OR only warning, false otherwise
 * \author <a href="mailto:jens.schneider@in.tum.de">Jens Schneider</a>
 * \date Aug.2004
 */
bool GLSLProgram::WriteInfoLog(const char* shaderdesc, GLuint hObject,
                               bool bProgram) {
  // Check for errors
  GLint iLength=0;
  if (bProgram)
    glGetProgramiv(hObject,GL_INFO_LOG_LENGTH,&iLength);
  else
    glGetShaderiv(hObject,GL_INFO_LOG_LENGTH,&iLength);

#ifdef _DEBUG
  GLenum err = glGetError();
  assert(GL_NO_ERROR == err);
#endif

  GLboolean bAtMostWarnings = GL_TRUE;
  if (iLength > 1) {
    char *pcLogInfo=new char[iLength];
    if (bProgram) {
      glGetProgramInfoLog(hObject,iLength,&iLength,pcLogInfo);
      bAtMostWarnings = glIsProgram(hObject);
    } else {
      glGetShaderInfoLog(hObject,iLength,&iLength,pcLogInfo);
      bAtMostWarnings = glIsShader(hObject);
    }
    if (bAtMostWarnings) {
      WARNING("%s", shaderdesc);
      WARNING("%s", pcLogInfo);
      delete[] pcLogInfo;
      return false;
    } else {
      T_ERROR("%s", shaderdesc);
      T_ERROR("%s", pcLogInfo);
      delete[] pcLogInfo;
#ifdef GLSLPROGRAM_STRICT
      return true;
#endif
    }
#ifdef _DEBUG
  } else {
    MESSAGE("No info log available.");
#endif
  }
  return !bool(bAtMostWarnings==GL_TRUE); // error occured?
}



/**
 * Loads a vertex or fragment shader.
 * Loads either a vertex or fragment shader and tries to compile it.
 * \param ShaderDesc - name of the file containing the shader
 * \param Type - either GL_VERTEX_SHADER or GL_FRAGMENT_SHADER
 * \param src - defines the source of the shader. Can be either GLSLPROGRAM_DISK or GLSLPROGRAM_STRING.
 * \return a handle to the compiled shader if successful, 0 otherwise
 * \warning uses glGetError()
 * \author <a href="mailto:jens.schneider@in.tum.de">Jens Schneider</a>
 * \date Mar.2005
 * \see GLSLPROGRAM_SOURCE
 */
GLuint GLSLProgram::LoadShader(const char *ShaderDesc, GLenum Type, GLSLPROGRAM_SOURCE src) {
  // assert right type
  assert(Type==GL_VERTEX_SHADER || Type==GL_FRAGMENT_SHADER);

  CheckGLError();

  unsigned long lFileSize;
  char *pcShader;
  FILE *fptr;

  // Load and compile vertex shader
  switch(src) {
    case GLSLPROGRAM_DISK:
      fptr=fopen(ShaderDesc,"rb");
      if (!fptr) {
        T_ERROR("File %s not found!",ShaderDesc);
        return 0;
      }
      if (fseek(fptr,0,SEEK_END)) {
        fclose(fptr);
        T_ERROR("Error reading file %s.",ShaderDesc);
        return 0;
      }
      lFileSize=ftell(fptr)/sizeof(char);
      fseek(fptr,0,SEEK_SET);
      pcShader=new char[lFileSize+1];
      pcShader[lFileSize]='\0';
      if (lFileSize!=fread(pcShader,sizeof(char),lFileSize,fptr)) {
        fclose(fptr);
        delete[] pcShader;
        T_ERROR("Error reading file %s.",ShaderDesc);
        return 0;
      }
      fclose(fptr);
      break;
    case GLSLPROGRAM_STRING:
      pcShader=(char*)ShaderDesc;
      lFileSize=long(strlen(pcShader));
      break;
    default:
      T_ERROR("Unknown source");
      return 0;
      break;
  }

  GLuint hShader = 0;
  bool bError=false;
  if (m_bGLUseARB) {
    hShader = glCreateShaderObjectARB(Type);
    glShaderSourceARB(hShader,1,(const GLchar**)&pcShader,NULL); // upload null-terminated shader
    glCompileShaderARB(hShader);

    // Check for errors
    if (CheckGLError("LoadProgram()")) {
      glDeleteObjectARB(hShader);
      bError =true;
    }
  } else {
    hShader = glCreateShader(Type);
    // upload null-terminated shader
    glShaderSource(hShader,1,(const char**)&pcShader,NULL);
    glCompileShader(hShader);

    // Check for compile status
    GLint iCompiled=42, srclen=42;
    glGetShaderiv(hShader,GL_COMPILE_STATUS,&iCompiled);
    glGetShaderiv(hShader, GL_SHADER_SOURCE_LENGTH, &srclen);

    // Check for errors
    if (WriteInfoLog(ShaderDesc, hShader, false)) {
      T_ERROR("WIL failed, deleting shader..");
      glDeleteShader(hShader);
      bError=true;
    }

    if (CheckGLError("LoadProgram()") || iCompiled!=GLint(GL_TRUE)) {
      T_ERROR("Shader compilation failed.");
      glDeleteShader(hShader);
      bError=true;
    }
  }

  if (pcShader!=ShaderDesc) delete[] pcShader;

  if (bError) return 0;
  return hShader;
}

/**
 * Enables the program for rendering.
 * Generates error messages if something went wrong (i.e. program not initialized etc.)
 * \param void
 * \return void
 * \warning uses glGetError()
 * \author <a href="mailto:jens.schneider@in.tum.de">Jens Schneider</a>
 * \date Aug.2004
 */
void GLSLProgram::Enable(void) {
  if(glIsProgram(m_hProgram) != GL_TRUE) {
    T_ERROR("not a program!");
  }
  if (m_bInitialized) {
    do { } while(CheckGLError());
    gl::UseProgram(m_hProgram);
    if (!CheckGLError("Enable()")) m_bEnabled=true;
  }
  else T_ERROR("No program loaded!");
}

/**
 * Disables the program for rendering.
 * Generates error messages if something went wrong (i.e. program not initialized etc.)
 * \param void
 * \return void
 * \warning uses glGetError()
 * \author <a href="mailto:jens.schneider@in.tum.de">Jens Schneider</a>
 * \date Aug.2004
 */
void GLSLProgram::Disable(void) {
  // opengl may not be enabed yet so be careful calling gl functions
 if (glUseProgramObjectARB != NULL || glUseProgram != NULL) gl::UseProgram(0);
}


/**
 * Checks and handles glErrors.
 * This routine is verbose when run with GLSL_DEBUG defined, only.
 * If called with NULL as parameter, queries glGetError() and returns true if glGetError() did not return GL_NO_ERROR.
 * If called with a non-NULL parameter, queries glGetError() and concatenates pcError and the verbosed glError.
 * If in debug mode, the error is output to stderr, otherwise it is silently ignored.
 * \param pcError first part of an error message. May be NULL.
 * \param pcAdditional additional part of error message. May be NULL.
 * \return bool specifying if an error occured (true) or not (false)
 * \warning uses glGetError()
 * \author <a href="mailto:jens.schneider@in.tum.de">Jens Schneider</a>
 * \date Aug.2004
 */
#ifndef GLSL_DEBUG
bool GLSLProgram::CheckGLError(const char*, const char*) const {
  return (glGetError()!=GL_NO_ERROR);
}
#else
bool GLSLProgram::CheckGLError(const char *pcError,
                               const char *pcAdditional) const {
  if (pcError==NULL) {  // Simply check for error, true if an error occured.

    // is there and error in the stack
    bool bError = glGetError()!=GL_NO_ERROR ;

    // clear all other errors in the stack
    while (glGetError() != GL_NO_ERROR ) {}

    return bError;
  } else {  // print out error
    GLenum iError=glGetError();
  
    if (iError==GL_NO_ERROR ) return false;

    while (iError !=GL_NO_ERROR ) {
      char *pcMessage;
      if (pcAdditional) {
        size_t len=16+strlen(pcError)+(pcAdditional ? strlen(pcAdditional) : 0);
        pcMessage=new char[len];
        sprintf(pcMessage,pcError,pcAdditional);
      }
      else pcMessage=(char*)pcError;

      std::ostringstream msg;
      msg << pcMessage << " - ";
      switch (iError) {
        case GL_NO_ERROR:
          if (pcMessage!=pcError) delete[] pcMessage;
          return false;
          break;
        case GL_INVALID_ENUM:
          msg << "GL_INVALID_ENUM";
          break;
        case GL_INVALID_VALUE:
          msg << "GL_INVALID_VALUE";
          break;
        case GL_INVALID_OPERATION:
          msg << "GL_INVALID_OPERATION";
          break;
        case GL_STACK_OVERFLOW:
          msg << "GL_STACK_OVERFLOW";
          break;
        case GL_STACK_UNDERFLOW:
          msg << "GL_STACK_UNDERFLOW";
          break;
        case GL_OUT_OF_MEMORY:
          msg << "GL_OUT_OF_MEMORY";
          break;
        default:
          msg << "unknown GL_ERROR " << iError;
          break;
      }
      if (pcMessage!=pcError) delete[] pcMessage;

      // display the error.
      T_ERROR("%s", msg.str().c_str());

      // fetch the next error from the stack
      iError=glGetError();
    }

    return true;
  }
}
#endif


/***************************************************************************

(c) 2004-05 by Jens Schneider, TUM.3D
    mailto:jens.schneider@in.tum.de
    Computer Graphics and Visualization Group
      Institute for Computer Science I15
    Technical University of Munich

****************************************************************************/

/**
 * Returns true if this program is valid.
 * \param void
 * \return true if this program was initialized properly
 * \author <a href="mailto:jens.schneider@in.tum.de">Jens Schneider</a>
 * \date Jun.2005
 */
bool GLSLProgram::IsValid(void) const {
  return m_bInitialized;
}

GLint GLSLProgram::get_location(const char *name) const {
  while(glGetError() != GL_NO_ERROR) {;}  // flush current error state.

  GLint location;

  // Get the position for the uniform var.
  location = gl::GetUniformLocation(m_hProgram, name);
  GLenum gl_err = glGetError();
  if(gl_err != GL_NO_ERROR) {
    throw GL_ERROR(gl_err);
  }

  if(location == -1) {
    throw GL_ERROR(0);
  }

  return location;
}


GLenum GLSLProgram::get_type(const char *name) const
{
  GLint numUniforms = 0;
  glGetProgramiv( m_hProgram, GL_ACTIVE_UNIFORMS, &numUniforms );
  GLint uniformMaxLength = 0;
  glGetProgramiv( m_hProgram, GL_ACTIVE_UNIFORM_MAX_LENGTH, &uniformMaxLength );
  
  GLint size = -1;
  GLenum type = 0;
  GLchar* uniformName = new GLchar[uniformMaxLength];
  for ( GLint i = 0; i < numUniforms; ++i )
  {
    GLsizei length;
    if (GLSLProgram::m_bGLUseARB)
      glGetActiveUniformARB( m_hProgram, i, uniformMaxLength, &length, &size, &type, uniformName);
    else
      glGetActiveUniform( m_hProgram, i, uniformMaxLength, &length, &size, &type, uniformName);
    if ( strcmp(uniformName, name) == 0 ) 
      break;
    else
      type = 0;
  }
  delete [] uniformName;

  GLenum gl_err = glGetError();
  if(gl_err != GL_NO_ERROR) {
    T_ERROR("Error getting uniform parameter type.");
    throw GL_ERROR(gl_err);
  }

  return type;
}

#ifdef GLSL_DEBUG
void GLSLProgram::CheckType(const char *name, GLenum type) const {
  GLenum eTypeInShader = get_type(name);
  if (eTypeInShader != type) {
    WARNING("Requested uniform variable type (%i) does not "
            "match shader definition (%i).",
            type, eTypeInShader);
  }
}
#else
void GLSLProgram::CheckType(const char *, GLenum) const { }
#endif

#ifdef GLSL_DEBUG
void GLSLProgram::CheckSamplerType(const char *name) const {
  GLenum eTypeInShader = get_type(name);

  if (eTypeInShader != GL_SAMPLER_1D &&
      eTypeInShader != GL_SAMPLER_2D &&
      eTypeInShader != GL_SAMPLER_3D &&
      eTypeInShader != GL_SAMPLER_CUBE &&
      eTypeInShader != GL_SAMPLER_1D_SHADOW &&
      eTypeInShader != GL_SAMPLER_2D_SHADOW &&
      eTypeInShader != GL_SAMPLER_2D_RECT_ARB &&
      eTypeInShader != GL_SAMPLER_2D_RECT_SHADOW_ARB) {
    WARNING("Shader definition (%i) does not match any "
            "sampler type.", eTypeInShader);
  }
}
#else
void GLSLProgram::CheckSamplerType(const char *) const { }
#endif

void GLSLProgram::ConnectTextureID(const string& name,
                                   const int iUnit) {
  Enable();
  m_mBindings[name] = iUnit;
  
  try {
    GLint location = get_location(name.c_str());
    CheckSamplerType(name.c_str());
    GL(glUniform1i(location,iUnit));
  } catch(GLError gl) {
    T_ERROR("Error (%d) obtaining uniform %s.", gl.error(), name.c_str());
    return;
  }
}

void GLSLProgram::SetTexture(const string& name,
                             const GLTexture& pTexture) {

  if (m_mBindings.find(name) == m_mBindings.end ()) {
    // find a free texture unit
    int iUnusedTexUnit = 0;
    for (texMap::iterator i = m_mBindings.begin();i != m_mBindings.end();++i){
      if (i->second <= iUnusedTexUnit) 
        iUnusedTexUnit = i->second+1;
    }
    ConnectTextureID(name, iUnusedTexUnit);
    pTexture.Bind(iUnusedTexUnit);
  } else {
    pTexture.Bind(m_mBindings[name]);
  }
}


void GLSLProgram::Set(const char *name, float x) const {
  try {
    GLint location = get_location(name);    
    CheckType(name, GL_FLOAT);
    GL(glUniform1f(location,x));    
    // MESSAGE("Set uniform %s to %g", name, x);
  } catch(GLError gl) {
    T_ERROR("Error (%d) obtaining uniform %s.", gl.error(), name);
    return;
  }
}

void GLSLProgram::Set(const char *name, float x, float y) const {
  try {
    GLint location = get_location(name);
    CheckType(name, GL_FLOAT_VEC2);
    GL(glUniform2f(location,x,y));    
    // MESSAGE("Set uniform %s to (%g,%g)", name, x, y);
  } catch(GLError gl) {
    T_ERROR("Error (%d) obtaining uniform %s.", gl.error(), name);
    return;
  }
}

void GLSLProgram::Set(const char *name, float x, float y, float z) const {
  try {
    GLint location = get_location(name);
    CheckType(name, GL_FLOAT_VEC3);
    GL(glUniform3f(location,x,y,z));
    // MESSAGE("Set uniform %s to (%g,%g,%g)", name, x, y, z);
  } catch(GLError gl) {
    T_ERROR("Error (%d) obtaining uniform %s.", gl.error(), name);
    return;
  }
}

void GLSLProgram::Set(const char *name, float x, float y, float z, float w) const {
  try {
    GLint location = get_location(name);
    CheckType(name, GL_FLOAT_VEC4);
    GL(glUniform4f(location,x,y,z,w));
    // MESSAGE("Set uniform %s to (%g,%g,%g,%g)", name, x, y, z, w);
  } catch(GLError gl) {
    T_ERROR("Error (%d) obtaining uniform %s.", gl.error(), name);
    return;
  }
}

void GLSLProgram::Set(const char *name, const float *m, size_t size, bool bTranspose) const {
  try {
    GLint location = get_location(name);
    switch (size) {
      case 2 : CheckType(name, GL_FLOAT_MAT2);
               GL(glUniformMatrix2fv(location,1,bTranspose,m)); 
               break;
      case 3 : CheckType(name, GL_FLOAT_MAT3);
               GL(glUniformMatrix3fv(location,1,bTranspose,m));
               break;
      case 4 : CheckType(name, GL_FLOAT_MAT4);
               GL(glUniformMatrix4fv(location,1,bTranspose,m));
               break;
      default: T_ERROR("Invalid size (%i) when setting matrix %s.", (int)size, name); return;
    }
  } catch(GLError gl) {
    T_ERROR("Error (%d) obtaining uniform %s.", gl.error(), name);
    return;
  }  
}

void GLSLProgram::Set(const char *name, int x) const {
  try {
    GLint location = get_location(name);
    CheckType(name, GL_INT);
    GL(glUniform1i(location,x));
    // MESSAGE("Set uniform %s to %d", name, x);
  } catch(GLError gl) {
    T_ERROR("Error (%d) obtaining uniform %s.", gl.error(), name);
    return;
  }
}

void GLSLProgram::Set(const char *name, int x, int y) const {
  try {
    GLint location = get_location(name);
    CheckType(name, GL_INT_VEC2);
    GL(glUniform2i(location,x,y));
    // MESSAGE("Set uniform %s to (%d,%d)", name, x, y);
  } catch(GLError gl) {
    T_ERROR("Error (%d) obtaining uniform %s.", gl.error(), name);
    return;
  }
}

void GLSLProgram::Set(const char *name, int x, int y, int z) const {
  try {
    GLint location = get_location(name);
    CheckType(name, GL_INT_VEC3);
    GL(glUniform3i(location,x,y,z));
    // MESSAGE("Set uniform %s to (%d,%d,%d)", name, x, y, z);
  } catch(GLError gl) {
    T_ERROR("Error (%d) obtaining uniform %s.", gl.error(), name);
    return;
  }
}

void GLSLProgram::Set(const char *name, int x, int y, int z, int w) const {
  try {
    GLint location = get_location(name);
    CheckType(name, GL_INT_VEC4);
    GL(glUniform4i(location,x,y,z,w));    
    // MESSAGE("Set uniform %s to (%d,%d,%d,%d)", name, x, y, z, w);
  } catch(GLError gl) {
    T_ERROR("Error (%d) obtaining uniform %s.", gl.error(), name);
    return;
  }
}

void GLSLProgram::Set(const char *name, const int *m, size_t size, bool bTranspose) const {
  if (size < 2 || size > 4) {
     T_ERROR("Invalid size (%i) when setting matrix %s.", (int)size, name);
     return;
  }
  float mf[4];
  for (size_t i = 0;i<size;++i) mf[i] = float(m[i]);
  Set(name, mf, size, bTranspose);
}

void GLSLProgram::Set(const char *name, bool x) const {
  try {
    GLint location = get_location(name);
    CheckType(name, GL_BOOL);
    GL(glUniform1i(location,x?1:0));
    // MESSAGE("Set uniform %s to %s", name, x ? "true" : "false");
  } catch(GLError gl) {
    T_ERROR("Error (%d) obtaining uniform %s.", gl.error(), name);
    return;
  }
}

void GLSLProgram::Set(const char *name, bool x, bool y) const {
  try {
    GLint location = get_location(name);
    CheckType(name, GL_BOOL_VEC2);
    GL(glUniform2i(location,x?1:0,y?1:0));    
    // MESSAGE("Set uniform %s to (%s,%s)", name, x ? "true" : "false",
    //                                           y ? "true" : "false");
  } catch(GLError gl) {
    T_ERROR("Error (%d) obtaining uniform %s.", gl.error(), name);
    return;
  }
}

void GLSLProgram::Set(const char *name, bool x, bool y, bool z) const {
  try {
    GLint location = get_location(name);
    CheckType(name, GL_BOOL_VEC3);
    GL(glUniform3i(location,x?1:0,y?1:0,z?1:0));    
    // MESSAGE("Set uniform %s to (%s,%s,%s)", name, x ? "true" : "false",
    //                                              y ? "true" : "false",
    //                                              z ? "true" : "false");
  } catch(GLError gl) {
    T_ERROR("Error (%d) obtaining uniform %s.", gl.error(), name);
    return;
  }
}

void GLSLProgram::Set(const char *name, bool x, bool y, bool z, bool w) const {
  try {
    GLint location = get_location(name);
    CheckType(name, GL_BOOL_VEC4);
    GL(glUniform4i(location,x?1:0,y?1:0,z?1:0,w?1:0));    
//    MESSAGE("Set uniform %s to (%s,%s,%s,%s)", name, x ? "true" : "false",
//                                                     y ? "true" : "false",
//                                                     z ? "true" : "false",
//                                                     w ? "true" : "false");
  } catch(GLError gl) {
    T_ERROR("Error (%d) obtaining uniform %s.", gl.error(), name);
    return;
  }
}

void GLSLProgram::Set(const char *name, const bool *m, size_t size, bool bTranspose) const {
  if (size < 2 || size > 4) {
     T_ERROR("Invalid size (%i) when setting matrix %s.", (int)size, name);
     return;
  }
  float mf[4];
  for (size_t i = 0;i<size;++i) mf[i] = m[i] ? 1.0f : 0.0f;
  Set(name, mf, size, bTranspose);
}
