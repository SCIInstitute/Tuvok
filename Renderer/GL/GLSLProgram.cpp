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
#include "Renderer/GL/GLError.h"
#include "Renderer/GL/GLTexture.h"

using namespace tuvok;
using namespace std;

/// GL Extension Wrangler (glew) is initialized on first instantiation
bool GLSLProgram::m_bGlewInitialized=true;
bool GLSLProgram::m_bGLChecked=false;      ///< GL extension check
bool GLSLProgram::m_bGLUseARB=false;       ///< use pre GL 2.0 syntax

/* Hack: the ATI/AMD driver has a bug which requires the 3rd parameter of
 * glGetActiveUniform to be nonzero.  We use these two dummy vars in the
 * glGetActiveUniform calls to cope with it. */
GLsizei AtiHackLen;
GLchar AtiHackChar;

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

    MESSAGE("%d shaders attached to %u", static_cast<int>(num_shaders),
          static_cast<unsigned>(program));

           
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
  if (!m_bGlewInitialized) {
    GLenum err = glewInit();
    if(err != GLEW_OK) {
      T_ERROR("GLEW initialization failed: %s", glewGetErrorString(err));
    } else {
      m_bGlewInitialized=true;
    }
  }
  // just in case someone wants to handle GLEW himself (by setting the
  // static var to true) but failed to do so properly
#ifdef GLSL_DEBUG
  else {
    if (glMultiTexCoord2f==NULL) {
      T_ERROR("GLEW must be initialized.  "
              "Set GLSLProgram::m_bGlewInitialized = false "
              "in GLSLProgram.cpp if you want this class to do it for you");
    }
  }
#endif

  if (!m_bGLChecked) {
    MESSAGE("Initializing OpenGL on a: %s",
            (const char*)glGetString(GL_VENDOR));
    if (atof((const char*)glGetString(GL_VERSION)) >= 2.0) {
      MESSAGE("OpenGL 2.0 supported");
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

      gl::arb = m_bGLUseARB = true;
    }
  }
  return true;
}

// reads an entire file into a string.
static std::string readshader(const std::string& filename)
{
  // open in append mode so the file pointer will be at EOF and we can
  // therefore easily/quickly figure out the file size.
  std::ifstream ifs(filename.c_str(), std::ios::in | std::ios::ate);
  if(!ifs.is_open()) {
    T_ERROR("Could not open shader '%s'", filename.c_str());
    return "";
  }
  std::ifstream::pos_type len = ifs.tellg();
  ifs.seekg(0, std::ios::beg);

  std::vector<char> shader(len+std::ifstream::pos_type(1), 0);
  size_t offset=0;
  do {
    std::streamsize length = std::streamsize(len) - std::streamsize(offset);
    ifs.read(&shader[offset], length);
    offset += ifs.gcount();
  } while(!ifs.eof() && std::ifstream::pos_type(offset) < len);
  ifs.close();

  return std::string(&shader[0]);
}

static bool addshader(GLuint program, const std::string& filename,
                      GLenum shtype)
{
  std::string shader = readshader(filename);
  if(shader.empty()) {
    T_ERROR("Empty shader (type %d) '%s'!", static_cast<int>(shtype),
            filename.c_str());
    return false;
  }

  GLuint sh = gl::CreateShader(shtype);
  if(sh == 0) {
    T_ERROR("Error (%d) creating shader (type %d) '%s'",
            static_cast<int>(glGetError()), static_cast<int>(shtype),
            filename.c_str());
    return false;
  }

  const GLchar* src[1] = { shader.c_str() };
  const GLint lengths[1] = { static_cast<GLint>(shader.length()) };
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

  MESSAGE("Adding shader %s to program %u", filename.c_str(),
          static_cast<unsigned>(program));

  gl::AttachShader(program, sh);
  if(glGetError() != GL_NO_ERROR) {
    T_ERROR("Error attaching shader '%s'", filename.c_str());
    gl::DeleteShader(sh);
    return false;
  }

  // shaders are refcounted; it won't actually be deleted until it is
  // detached from the program object.
  gl::DeleteShader(sh);

  return true;
}

void GLSLProgram::Load(const std::vector<std::string>& vert,
                       const std::vector<std::string>& frag)
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
  for(std::vector<std::string>::const_iterator vsh = vert.begin();
      vsh != vert.end(); ++vsh) {
    if(false == addshader(this->m_hProgram, *vsh, GL_VERTEX_SHADER)) {
      T_ERROR("Attaching shader '%s' failed.", vsh->c_str());
      detach_shaders(this->m_hProgram);
      gl::DeleteProgram(this->m_hProgram);
      this->m_hProgram = 0;
      return;
    }
  }

  // create a shader for each fragment shader, and attach it to the main
  // program.
  for(std::vector<std::string>::const_iterator fsh = frag.begin();
      fsh != frag.end(); ++fsh) {
    if(false == addshader(this->m_hProgram, *fsh, GL_FRAGMENT_SHADER)) {
      T_ERROR("Attaching shader '%s' failed.", fsh->c_str());
      detach_shaders(this->m_hProgram);
      gl::DeleteProgram(this->m_hProgram);
      return;
    }
  }

  gl::LinkProgram(this->m_hProgram);

  { // figure out if the linking was successful.
    GLint linked = GL_TRUE;
    if(gl::arb) {
      glGetObjectParameterivARB(this->m_hProgram, GL_OBJECT_LINK_STATUS_ARB,
                                &linked);
    } else {
      glGetProgramiv(this->m_hProgram, GL_LINK_STATUS, &linked);
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
 * Writes errors/information messages to stdout.
 * Gets the InfoLogARB and writes it to stdout.
 * Parameter is necessary for temporary objects (i.e. vertex / fragment shader)
 * \param hObject - a handle to the object.
 * \return true if InfoLogARB non-empty (i.e. error/info message), false otherwise
 * \author <a href="mailto:jens.schneider@in.tum.de">Jens Schneider</a>
 * \date Aug.2004
 */
bool GLSLProgram::WriteError(GLhandleARB hObject) {
  // Check for errors
  GLint iLength;
  glGetObjectParameterivARB(hObject,GL_OBJECT_INFO_LOG_LENGTH_ARB,&iLength);
  if (iLength>1) {
    GLcharARB *pcLogInfo=new GLcharARB[iLength];
    glGetInfoLogARB(hObject,iLength,&iLength,pcLogInfo);
    MESSAGE(pcLogInfo);
    delete[] pcLogInfo;
    return true;  // an error had occured.
  }
  return false;
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

static GLint get_uniform_vector(const char *name, GLuint program, GLenum *type)
{
  while(glGetError() != GL_NO_ERROR) {;}  // flush current error state.

  GLint size;
  GLint location;

  // Get the position for the uniform var.
  location = gl::GetUniformLocation(program, name);
  GLenum gl_err = glGetError();
  if(gl_err != GL_NO_ERROR || location == -1) {
    throw GL_ERROR(gl_err);
  }

  if (GLSLProgram::m_bGLUseARB) {
    glGetActiveUniformARB(program, location, 0, NULL, &size, type, NULL);
  } else {
    glGetActiveUniform(program, location, 1, &AtiHackLen, &size, type,
                       &AtiHackChar);
  }

  gl_err = glGetError();
  if(gl_err != GL_NO_ERROR) {
    T_ERROR("Error getting type.");
    throw GL_ERROR(gl_err);
  }

  return location;
}

  /**
   * Sets an uniform vector parameter.
   * \warning uses glGetError();
   * \param name - name of the parameter
   * \param x,y,z,w - up to four float components of the vector to set.
   * \return void
   * \author <a href="mailto:jens.schneider@in.tum.de">Jens Schneider</a>
   * \date Aug.2004
   */
  void GLSLProgram::SetUniformVector(const char *name,
                                     float x, float y, float z, float w) const {
    assert(m_bEnabled);
    CheckGLError();

    GLenum iType;
    GLint iLocation;

  try {
    iLocation = get_uniform_vector(name, m_hProgram, &iType);
  } catch(GLError gl) {
    T_ERROR("Error (%d) obtaining uniform %s.", gl.error(), name);
    return;
  }

  switch (iType) {
    case GL_FLOAT:      glUniform1f(iLocation,x); break;
    case GL_FLOAT_VEC2: glUniform2f(iLocation,x,y); break;
    case GL_FLOAT_VEC3: glUniform3f(iLocation,x,y,z); break;
    case GL_FLOAT_VEC4: glUniform4f(iLocation,x,y,z,w); break;

#ifdef GLSL_ALLOW_IMPLICIT_CASTS
    case GL_INT:
    case GL_SAMPLER_1D:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_3D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_1D_SHADOW:
    case GL_SAMPLER_2D_SHADOW:
    case GL_SAMPLER_2D_RECT_ARB:
    case GL_SAMPLER_2D_RECT_SHADOW_ARB:  glUniform1i(iLocation,int(x)); break;

    case GL_INT_VEC2:   glUniform2i(iLocation,int(x),int(y)); break;
    case GL_INT_VEC3:   glUniform3i(iLocation,int(x),int(y),int(z)); break;
    case GL_INT_VEC4:   glUniform4i(iLocation,int(x),int(y),int(z),int(w)); break;
    case GL_BOOL:       glUniform1f(iLocation,x); break;
    case GL_BOOL_VEC2:  glUniform2f(iLocation,x,y); break;
    case GL_BOOL_VEC3:  glUniform3f(iLocation,x,y,z); break;
    case GL_BOOL_VEC4:  glUniform4f(iLocation,x,y,z,w); break;

    default:
      T_ERROR("(const char*, float, float, float, float)"
              " Unknown type (%d) for %s.", iType, name);
      break;
#else
    default:
      T_ERROR("(const char*, float, float, float, float)"
              " Unknown type (%d) for %s."
              " (expecting %d, %d, %d, or %d)", iType, name, 
              GL_FLOAT, GL_FLOAT_VEC2, GL_FLOAT_VEC3, GL_FLOAT_VEC4);
      break;
#endif
  }
#ifdef GLSL_DEBUG
  CheckGLError("SetUniformVector(%s,float,...)",name);
#endif
}

/**
 * Sets an uniform vector parameter.
 * \warning uses glGetError();
 * \param name - name of the parameter
 * \param x,y,z,w - up to four bool components of the vector to set.
 * \return void
 * \author <a href="mailto:jens.schneider@in.tum.de">Jens Schneider</a>
 * \date Mar.2005
 */
void GLSLProgram::SetUniformVector(const char *name,bool x, bool y, bool z, bool w) const {
  assert(m_bEnabled);
  CheckGLError();

  GLenum iType;
  GLint iLocation;

  try {
    iLocation = get_uniform_vector(name, m_hProgram, &iType);
  } catch(GLError gl) {
    T_ERROR("Error (%d) obtaining uniform %s.", gl.error(), name);
    return;
  }

  switch (iType) {
    case GL_BOOL:            glUniform1i(iLocation,(x ? 1 : 0)); break;
    case GL_BOOL_VEC2:          glUniform2i(iLocation,(x ? 1 : 0),(y ? 1 : 0)); break;
    case GL_BOOL_VEC3:          glUniform3i(iLocation,(x ? 1 : 0),(y ? 1 : 0),(z ? 1 : 0)); break;
    case GL_BOOL_VEC4:          glUniform4i(iLocation,(x ? 1 : 0),(y ? 1 : 0),(z ? 1 : 0),(w ? 1 : 0)); break;

#ifdef GLSL_ALLOW_IMPLICIT_CASTS
    case GL_FLOAT:            glUniform1f(iLocation,(x ? 1.0f : 0.0f)); break;
    case GL_FLOAT_VEC2:          glUniform2f(iLocation,(x ? 1.0f : 0.0f),(y ? 1.0f : 0.0f)); break;
    case GL_FLOAT_VEC3:          glUniform3f(iLocation,(x ? 1.0f : 0.0f),(y ? 1.0f : 0.0f),(z ? 1.0f : 0.0f)); break;
    case GL_FLOAT_VEC4:          glUniform4f(iLocation,(x ? 1.0f : 0.0f),(y ? 1.0f : 0.0f),(z ? 1.0f : 0.0f),(w ? 1.0f : 0.0f)); break;
    case GL_INT:            glUniform1i(iLocation,(x ? 1 : 0)); break;
    case GL_INT_VEC2:          glUniform2i(iLocation,(x ? 1 : 0),(y ? 1 : 0)); break;
    case GL_INT_VEC3:          glUniform3i(iLocation,(x ? 1 : 0),(y ? 1 : 0),(z ? 1 : 0)); break;
    case GL_INT_VEC4:          glUniform4i(iLocation,(x ? 1 : 0),(y ? 1 : 0),(z ? 1 : 0),(w ? 1 : 0)); break;
    default:
      T_ERROR("(const char*, bool, bool, bool, bool)"
              " Unknown type (%d) for %s.", iType, name);
      break;
#else
    default:
      T_ERROR("(const char*, bool, bool, bool, bool)"
              " Unknown type (%d) for %s."
              " (expecting %d, %d, %d, or %d)", iType, name, 
              GL_BOOL, GL_BOOL_VEC2, GL_BOOL_VEC3, GL_BOOL_VEC4);
      break;
#endif

  }
#ifdef GLSL_DEBUG
  CheckGLError("SetUniformVector(%s,bool,...)",name);
#endif
}

/**
 * Sets an uniform vector parameter.
 * \warning uses glGetError();
 * \param name - name of the parameter
 * \param x,y,z,w - four int components of the vector to set.
 * \return void
 * \author <a href="mailto:jens.schneider@in.tum.de">Jens Schneider</a>
 * \date Aug.2004
 */
void GLSLProgram::SetUniformVector(const char *name,int x,int y,int z,int w) const {
  assert(m_bEnabled);
  CheckGLError();

  GLenum eType;
  GLint iLocation;

  try {
    iLocation = get_uniform_vector(name, m_hProgram, &eType);
  } catch(GLError gl) {
    T_ERROR("Error (%d) obtaining uniform %s.", gl.error(), name);
    return;
  }

  int iType = eType;

  T_ERROR("Trying to match type %x to %x for variable %s", eType, GL_SAMPLER_2D,name);
  if (eType == GL_SAMPLER_2D)  T_ERROR("OK 1");
  if (iType == GL_SAMPLER_2D)  T_ERROR("OK 2");
  if (eType == 0x8B5E)  T_ERROR("OK 3");
  if (iType == 0x8B5E)  T_ERROR("OK 4");

  switch (iType) {
    case GL_INT:
    case GL_SAMPLER_1D:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_3D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_1D_SHADOW:
    case GL_SAMPLER_2D_SHADOW:
    case GL_SAMPLER_2D_RECT_ARB:
    case GL_SAMPLER_2D_RECT_SHADOW_ARB:  glUniform1i(iLocation,x); break;

    case GL_INT_VEC2:          glUniform2i(iLocation,x,y); break;
    case GL_INT_VEC3:          glUniform3i(iLocation,x,y,z); break;
    case GL_INT_VEC4:          glUniform4i(iLocation,x,y,z,w); break;

#ifdef GLSL_ALLOW_IMPLICIT_CASTS
    case GL_BOOL:            glUniform1i(iLocation,x); break;
    case GL_BOOL_VEC2:          glUniform2i(iLocation,x,y); break;
    case GL_BOOL_VEC3:          glUniform3i(iLocation,x,y,z); break;
    case GL_BOOL_VEC4:          glUniform4i(iLocation,x,y,z,w); break;
    case GL_FLOAT:            glUniform1f(iLocation,float(x)); break;
    case GL_FLOAT_VEC2:          glUniform2f(iLocation,float(x),float(y)); break;
    case GL_FLOAT_VEC3:          glUniform3f(iLocation,float(x),float(y),float(z)); break;
    case GL_FLOAT_VEC4:          glUniform4f(iLocation,float(x),float(y),float(z),float(w)); break;
    default:
      T_ERROR("(const char*, int, int, int, int)"
              " Unknown type (%d) for %s.", iType, name);
      break;
#else
    default:
      T_ERROR("(const char*, int, int, int, int)"
              " Unknown type (%d) for %s."
              " (expecting %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, or %d)", 
              iType, name, 
              GL_INT, GL_SAMPLER_1D, GL_SAMPLER_2D, GL_SAMPLER_3D,
              GL_SAMPLER_CUBE, GL_SAMPLER_1D_SHADOW, GL_SAMPLER_2D_SHADOW,
              GL_SAMPLER_2D_RECT_ARB, GL_SAMPLER_2D_RECT_SHADOW_ARB,
              GL_INT_VEC2, GL_INT_VEC3, GL_INT_VEC4);
      break;
#endif
  }
#ifdef GLSL_DEBUG
  CheckGLError("SetUniformVector(%s,int,...)",name);
#endif
}

/**
 * Sets an uniform vector parameter.
 * \warning uses glGetError();
 * \param name - name of the parameter
 * \param v - a float vector containing up to four elements.
 * \return void
 * \author <a href="mailto:jens.schneider@in.tum.de">Jens Schneider</a>
 * \date Aug.2004
 */
void GLSLProgram::SetUniformVector(const char *name,const float *v) const {
  assert(m_bEnabled);
  CheckGLError();

  GLenum iType;
  GLint iLocation;

  try {
    iLocation = get_uniform_vector(name, m_hProgram, &iType);
  } catch(GLError gl) {
    T_ERROR("Error (%d) obtaining uniform %s.", gl.error(), name);
    return;
  }

  switch (iType) {
    case GL_FLOAT:            glUniform1fv(iLocation,1,v); break;
    case GL_FLOAT_VEC2:          glUniform2fv(iLocation,1,v); break;
    case GL_FLOAT_VEC3:          glUniform3fv(iLocation,1,v); break;
    case GL_FLOAT_VEC4:          glUniform4fv(iLocation,1,v); break;

#ifdef GLSL_ALLOW_IMPLICIT_CASTS
    case GL_INT:
    case GL_SAMPLER_1D:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_3D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_1D_SHADOW:
    case GL_SAMPLER_2D_SHADOW:
    case GL_SAMPLER_2D_RECT_ARB:
    case GL_SAMPLER_2D_RECT_SHADOW_ARB:  glUniform1i(iLocation,int(v[0])); break;

    case GL_INT_VEC2:          glUniform2i(iLocation,int(v[0]),int(v[1])); break;
    case GL_INT_VEC3:          glUniform3i(iLocation,int(v[0]),int(v[1]),int(v[2])); break;
    case GL_INT_VEC4:          glUniform4i(iLocation,int(v[0]),int(v[1]),int(v[2]),int(v[3])); break;
    case GL_BOOL:            glUniform1fv(iLocation,1,v); break;
    case GL_BOOL_VEC2:          glUniform2fv(iLocation,1,v); break;
    case GL_BOOL_VEC3:          glUniform3fv(iLocation,1,v); break;
    case GL_BOOL_VEC4:          glUniform4fv(iLocation,1,v); break;
#endif

    default:
      T_ERROR("(const char*, const float*)"
              " Unknown type (%d) for %s.", iType, name);
      break;
  }
#ifdef GLSL_DEBUG
  CheckGLError("SetUniformVector(%s,float*)",name);
#endif
}

/**
 * Sets an uniform vector parameter.
 * \warning uses glGetError();
 * \param name - name of the parameter
 * \param i - an int vector containing up to 4 elements.
 * \return void
 * \author <a href="mailto:jens.schneider@in.tum.de">Jens Schneider</a>
 * \date Aug.2004
 */
void GLSLProgram::SetUniformVector(const char *name,const int *i) const {
  assert(m_bEnabled);
  CheckGLError();

  GLenum iType;
  GLint iLocation;

  try {
    iLocation = get_uniform_vector(name, m_hProgram, &iType);
  } catch(GLError gl) {
    T_ERROR("Error (%d) obtaining uniform %s.", gl.error(), name);
    return;
  }

  switch (iType) {
    case GL_INT:
    case GL_SAMPLER_1D:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_3D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_1D_SHADOW:
    case GL_SAMPLER_2D_SHADOW:
    case GL_SAMPLER_2D_RECT_ARB:
    case GL_SAMPLER_2D_RECT_SHADOW_ARB:  glUniform1i(iLocation,i[0]); break;

    case GL_INT_VEC2:          glUniform2iv(iLocation,1,(const GLint*)i); break;
    case GL_INT_VEC3:          glUniform3iv(iLocation,1,(const GLint*)i); break;
    case GL_INT_VEC4:          glUniform4iv(iLocation,1,(const GLint*)i); break;
#ifdef GLSL_ALLOW_IMPLICIT_CASTS
    case GL_BOOL:            glUniform1iv(iLocation,1,(const GLint*)i); break;
    case GL_BOOL_VEC2:          glUniform2iv(iLocation,1,(const GLint*)i); break;
    case GL_BOOL_VEC3:          glUniform3iv(iLocation,1,(const GLint*)i); break;
    case GL_BOOL_VEC4:          glUniform4iv(iLocation,1,(const GLint*)i); break;
    case GL_FLOAT:            glUniform1f(iLocation,float(i[0])); break;
    case GL_FLOAT_VEC2:          glUniform2f(iLocation,float(i[0]),float(i[1])); break;
    case GL_FLOAT_VEC3:          glUniform3f(iLocation,float(i[0]),float(i[1]),float(i[2])); break;
    case GL_FLOAT_VEC4:          glUniform4f(iLocation,float(i[0]),float(i[1]),float(i[2]),float(i[3])); break;
#endif
    default:
      T_ERROR("(const char*, const int*)"
              " Unknown type (%d) for %s.", iType, name);
      break;
  }
#ifdef GLSL_DEBUG
  CheckGLError("SetUniformVector(%s,int*)",name);
#endif
}

/**
 * Sets an uniform vector parameter.
 * \warning uses glGetError();
 * \param name - name of the parameter
 * \param b - a bool vector containing up to 4 elements.
 * \return void
 * \author <a href="mailto:jens.schneider@in.tum.de">Jens Schneider</a>
 * \date Mar.2005
 */
void GLSLProgram::SetUniformVector(const char *name,const bool *b) const {
  assert(m_bEnabled);
  CheckGLError();

  GLenum iType;
  GLint iLocation;

  try {
    iLocation = get_uniform_vector(name, m_hProgram, &iType);
  } catch(GLError gl) {
    T_ERROR("Error (%d) obtaining uniform %s.", gl.error(), name);
    return;
  }

  switch (iType) {
    case GL_BOOL:            glUniform1i(iLocation,(b[0] ? 1 : 0)); break;
    case GL_BOOL_VEC2:          glUniform2i(iLocation,(b[0] ? 1 : 0),(b[1] ? 1 : 0)); break;
    case GL_BOOL_VEC3:          glUniform3i(iLocation,(b[0] ? 1 : 0),(b[1] ? 1 : 0),(b[2] ? 1 : 0)); break;
    case GL_BOOL_VEC4:          glUniform4i(iLocation,(b[0] ? 1 : 0),(b[1] ? 1 : 0),(b[2] ? 1 : 0),(b[3] ? 1 : 0)); break;
#ifdef GLSL_ALLOW_IMPLICIT_CASTS
    case GL_INT:            glUniform1i(iLocation,(b[0] ? 1 : 0)); break;
    case GL_INT_VEC2:          glUniform2i(iLocation,(b[0] ? 1 : 0),(b[1] ? 1 : 0)); break;
    case GL_INT_VEC3:          glUniform3i(iLocation,(b[0] ? 1 : 0),(b[1] ? 1 : 0),(b[2] ? 1 : 0)); break;
    case GL_INT_VEC4:          glUniform4i(iLocation,(b[0] ? 1 : 0),(b[1] ? 1 : 0),(b[2] ? 1 : 0),(b[3] ? 1 : 0)); break;
    case GL_FLOAT:            glUniform1f(iLocation,(b[0] ? 1.0f : 0.0f)); break;
    case GL_FLOAT_VEC2:          glUniform2f(iLocation,(b[0] ? 1.0f : 0.0f),(b[1] ? 1.0f : 0.0f)); break;
    case GL_FLOAT_VEC3:          glUniform3f(iLocation,(b[0] ? 1.0f : 0.0f),(b[1] ? 1.0f : 0.0f),(b[2] ? 1.0f : 0.0f)); break;
    case GL_FLOAT_VEC4:          glUniform4f(iLocation,(b[0] ? 1.0f : 0.0f),(b[1] ? 1.0f : 0.0f),(b[2] ? 1.0f : 0.0f),(b[3] ? 1.0f : 0.0f)); break;
#endif
    default:
      T_ERROR("(const char*, const bool*)"
              " Unknown type (%d) for %s.", iType, name);
      break;
  }
#ifdef GLSL_DEBUG
  CheckGLError("SetUniformVector(%s,bool*)",name);
#endif
}

/**
 * Sets an uniform matrix.
 * Matrices are always of type float.
 * \warning uses glGetError();
 * \param name - name of the parameter
 * \param m - a float array containing up to 16 floats for the matrix.
 * \param bTranspose - if true, the matrix will be transposed before uploading.
 * \return void
 * \author <a href="mailto:jens.schneider@in.tum.de">Jens Schneider</a>
 * \date Mar.2005
 */
void GLSLProgram::SetUniformMatrix(const char *name,const float *m,bool bTranspose) const {
  assert(m_bEnabled);
  CheckGLError();

  GLenum iType;
  GLint iLocation;

  try {
    iLocation = get_uniform_vector(name, m_hProgram, &iType);
  } catch(GLError gl) {
    T_ERROR("Error (%d) obtaining uniform %s.", gl.error(), name);
    return;
  }

  switch (iType) {
    case GL_FLOAT_MAT2:          glUniformMatrix2fv(iLocation,1,bTranspose,m); break;
    case GL_FLOAT_MAT3:          glUniformMatrix3fv(iLocation,1,bTranspose,m); break;
    case GL_FLOAT_MAT4:          glUniformMatrix4fv(iLocation,1,bTranspose,m); break;
    default:
      T_ERROR("(const char*, const float*, bool)"
              " Unknown type (%d) for %s.", iType, name);
      break;
  }
#ifdef GLSL_DEBUG
  CheckGLError("SetUniformMatrix(%s,float*,bool)",name);
#endif
}

#ifdef GLSL_ALLOW_IMPLICIT_CASTS

/**
 * Sets an uniform matrix.
 * Matrices are always of type float.
 * \warning uses glGetError();
 * \remark only available if GLSL_ALLOW_IMPLICIT_CASTS is defined.
 * \param name - name of the parameter
 * \param m - an int array containing up to 16 ints for the matrix. Ints are converted to float before uploading.
 * \param bTranspose - if true, the matrix will be transposed before uploading.
 * \return void
 * \author <a href="mailto:jens.schneider@in.tum.de">Jens Schneider</a>
 * \date Mar.2005
 */
void GLSLProgram::SetUniformMatrix(const char *name,const int *m, bool bTranspose) const {
  assert(m_bEnabled);
  CheckGLError();

  GLenum iType;
  GLint iLocation;

  try {
    iLocation = get_uniform_vector(name, m_hProgram, &iType);
  } catch(GLError gl) {
    T_ERROR("Error (%d) obtaining uniform %s.", gl.error(), name);
    return;
  }

  float M[16];
  switch (iType) {
    case GL_FLOAT_MAT2:
      for (unsigned int ui=0; ui<4; ui++) M[ui]=float(m[ui]);
      glUniformMatrix2fv(iLocation,1,bTranspose,M);
      break;
    case GL_FLOAT_MAT3:
      for (unsigned int ui=0; ui<9; ui++) M[ui]=float(m[ui]);
      glUniformMatrix3fv(iLocation,1,bTranspose,M);
      break;
    case GL_FLOAT_MAT4:
      for (unsigned int ui=0; ui<16; ui++) M[ui]=float(m[ui]);
      glUniformMatrix4fv(iLocation,1,bTranspose,M);
      break;
    default:
      T_ERROR("(const char*, const int*, bool)"
              " Unknown type (%d) for %s.", iType, name);
      break;
  }
#ifdef GLSL_DEBUG
  CheckGLError("SetUniformMatrix(%s,int*,bool)",name);
#endif
}



/**
 * Sets an uniform matrix.
 * Matrices are always of type float.
 * \warning uses glGetError();
 * \remark only available if GLSL_ALLOW_IMPLICIT_CASTS is defined.
 * \param name - name of the parameter
 * \param m - an int array containing up to 16 ints for the matrix. Ints are converted to float before uploading.
 * \param bTranspose - if true, the matrix will be transposed before uploading.
 * \return void
 * \author <a href="mailto:jens.schneider@in.tum.de">Jens Schneider</a>
 * \date Mar.2005
 */
void GLSLProgram::SetUniformMatrix(const char *name,const bool *m, bool bTranspose) const {
  assert(m_bEnabled);
  CheckGLError();

  GLenum iType;
  GLint iLocation;

  try {
    iLocation = get_uniform_vector(name, m_hProgram, &iType);
  } catch(GLError gl) {
    T_ERROR("Error (%d) obtaining uniform %s.", gl.error(), name);
    return;
  }

  float M[16];
  switch (iType) {
    case GL_FLOAT_MAT2:
      for (unsigned int ui=0; ui<4; ui++) M[ui]=(m[ui] ? 1.0f : 0.0f);
      glUniformMatrix2fv(iLocation,1,bTranspose,M);
      break;
    case GL_FLOAT_MAT3:
      for (unsigned int ui=0; ui<9; ui++) M[ui]=(m[ui] ? 1.0f : 0.0f);
      glUniformMatrix3fv(iLocation,1,bTranspose,M);
      break;
    case GL_FLOAT_MAT4:
      for (unsigned int ui=0; ui<16; ui++) M[ui]=(m[ui] ? 1.0f : 0.0f);
      glUniformMatrix4fv(iLocation,1,bTranspose,M);
      break;
    default:
      T_ERROR("(const char*, const bool*, bool)"
              " Unknown type (%d) for %s.", iType, name);
      break;
  }
#ifdef GLSL_DEBUG
  CheckGLError("SetUniformMatrix(%s,int*,bool)",name);
#endif
}

#endif // GLSL_ALLOW_IMPLICIT_CASTS

/**
 * Sets an uniform array.
 * Sets the entire array at once. Single positions can still be set using the other SetUniform*() methods.
 * \warning uses glGetError();
 * \param name - name of the parameter
 * \param a - a float array containing enough floats to fill the entire uniform array.
 * \return void
 * \author <a href="mailto:jens.schneider@in.tum.de">Jens Schneider</a>
 * \date Mar.2005
 */
void GLSLProgram::SetUniformArray(const char *name,const float *a) const {
  assert(m_bEnabled);
  CheckGLError();

  GLint iSize;
  GLenum iType;
  GLint iLocation;

  iLocation = gl::GetUniformLocation(m_hProgram, name);

  if (CheckGLError("SetUniformVector(%s,float,...) [getting adress]",name)) {
    return;
  }

  if(iLocation==-1) {
    T_ERROR("Error getting address for %s.", name);
    return;
  }

  if (m_bGLUseARB) {
    glGetActiveUniformARB(m_hProgram,iLocation,0,NULL,&iSize,&iType,NULL);
  } else {
    glGetActiveUniform(m_hProgram,iLocation,1,&AtiHackLen,&iSize,&iType,
                       &AtiHackChar);
  }

  if (CheckGLError("SetUniformVector(%s,float,...) [getting type]",name)) {
    return;
  }

#ifdef GLSL_ALLOW_IMPLICIT_CASTS
  GLint *iArray;
#endif

  switch (iType) {
    case GL_FLOAT:            glUniform1fv(iLocation,iSize,a); break;
    case GL_FLOAT_VEC2:          glUniform2fv(iLocation,iSize,a); break;
    case GL_FLOAT_VEC3:          glUniform3fv(iLocation,iSize,a); break;
    case GL_FLOAT_VEC4:          glUniform4fv(iLocation,iSize,a); break;

#ifdef GLSL_ALLOW_IMPLICIT_CASTS
    case GL_BOOL:            glUniform1fv(iLocation,iSize,a); break;
    case GL_BOOL_VEC2:          glUniform2fv(iLocation,iSize,a); break;
    case GL_BOOL_VEC3:          glUniform3fv(iLocation,iSize,a); break;
    case GL_BOOL_VEC4:          glUniform4fv(iLocation,iSize,a); break;

    case GL_INT:
    case GL_SAMPLER_1D:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_3D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_1D_SHADOW:
    case GL_SAMPLER_2D_SHADOW:
    case GL_SAMPLER_2D_RECT_ARB:
    case GL_SAMPLER_2D_RECT_SHADOW_ARB:
      iArray=new GLint[iSize];
      for (int i=0; i<iSize; i++) iArray[i]=int(a[i]);
      glUniform1iv(iLocation,iSize,iArray);
      delete[] iArray;
      break;

    case GL_INT_VEC2:
      iArray=new GLint[2*iSize];
      for (int i=0; i<2*iSize; i++) iArray[i]=int(a[i]);
      glUniform2iv(iLocation,iSize,iArray);
      delete[] iArray;
      break;
    case GL_INT_VEC3:
      iArray=new GLint[3*iSize];
      for (int i=0; i<3*iSize; i++) iArray[i]=int(a[i]);
      glUniform3iv(iLocation,iSize,iArray);
      delete[] iArray;
      break;
    case GL_INT_VEC4:
      iArray=new GLint[4*iSize];
      for (int i=0; i<4*iSize; i++) iArray[i]=int(a[i]);
      glUniform4iv(iLocation,iSize,iArray);
      delete[] iArray;
      break;
#endif

    default:
      T_ERROR("(const char*, const float*)"
              " Unknown type (%d) for %s.", iType, name);

      break;
  }
#ifdef GLSL_DEBUG
  CheckGLError("SetUniformArray(%s,float*)",name);
#endif
}

/**
 * Sets an uniform array.
 * Sets the entire array at once. Single positions can still be set using the other SetUniform*() methods.
 * \warning uses glGetError();
 * \param name - name of the parameter
 * \param a - an int array containing enough floats to fill the entire uniform array.
 * \return void
 * \author <a href="mailto:jens.schneider@in.tum.de">Jens Schneider</a>
 * \date Mar.2005
 */
void GLSLProgram::SetUniformArray(const char *name,const int *a) const {
  assert(m_bEnabled);
  CheckGLError();

  GLint iSize;
  GLenum iType;
  GLint iLocation;

  iLocation = gl::GetUniformLocation(m_hProgram, name);

  if (CheckGLError("SetUniformVector(%s,float,...) [getting adress]",name)) {
    return;
  }

  if(iLocation==-1) {
    T_ERROR("Error getting address for %s.", name);
    return;
  }

  if (m_bGLUseARB) {
    glGetActiveUniformARB(m_hProgram,iLocation,0,NULL,&iSize,&iType,NULL);
  } else {
    glGetActiveUniform(m_hProgram,iLocation,1,&AtiHackLen,&iSize,&iType,
                       &AtiHackChar);
  }

  if (CheckGLError("SetUniformVector(%s,float,...) [getting type]",name)) {
    return;
  }

#ifdef GLSL_ALLOW_IMPLICIT_CASTS
  float *fArray;
#endif

  switch (iType) {
    case GL_INT:
    case GL_SAMPLER_1D:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_3D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_1D_SHADOW:
    case GL_SAMPLER_2D_SHADOW:
    case GL_SAMPLER_2D_RECT_ARB:
    case GL_SAMPLER_2D_RECT_SHADOW_ARB:  glUniform1iv(iLocation,iSize,(const GLint*)a); break;
    case GL_INT_VEC2:          glUniform2iv(iLocation,iSize,(const GLint*)a); break;
    case GL_INT_VEC3:          glUniform3iv(iLocation,iSize,(const GLint*)a); break;
    case GL_INT_VEC4:          glUniform4iv(iLocation,iSize,(const GLint*)a); break;

#ifdef GLSL_ALLOW_IMPLICIT_CASTS
    case GL_BOOL:            glUniform1iv(iLocation,iSize,(const GLint*)a); break;
    case GL_BOOL_VEC2:          glUniform2iv(iLocation,iSize,(const GLint*)a); break;
    case GL_BOOL_VEC3:          glUniform3iv(iLocation,iSize,(const GLint*)a); break;
    case GL_BOOL_VEC4:          glUniform4iv(iLocation,iSize,(const GLint*)a); break;

    case GL_FLOAT:
      fArray=new float[iSize];
      for (int i=0; i<iSize; i++) fArray[i]=float(a[i]);
      glUniform1fv(iLocation,iSize,fArray);
      delete[] fArray;
      break;
    case GL_FLOAT_VEC2:
      fArray=new float[2*iSize];
      for (int i=0; i<2*iSize; i++) fArray[i]=float(a[i]);
      glUniform2fv(iLocation,iSize,fArray);
      delete[] fArray;
      break;
    case GL_FLOAT_VEC3:
      fArray=new float[3*iSize];
      for (int i=0; i<3*iSize; i++) fArray[i]=float(a[i]);
      glUniform3fv(iLocation,iSize,fArray);
      delete[] fArray;
      break;
    case GL_FLOAT_VEC4:
      fArray=new float[4*iSize];
      for (int i=0; i<4*iSize; i++) fArray[i]=float(a[i]);
      glUniform4fv(iLocation,iSize,fArray);
      delete[] fArray;
      break;
#endif

    default:
      T_ERROR("(const char*, const int*)"
              " Unknown type (%d) for %s.", iType, name);

      break;
  }
#ifdef GLSL_DEBUG
  CheckGLError("SetUniformArray(%s,int*)",name);
#endif
}


/**
 * Sets an uniform array.
 * Sets the entire array at once. Single positions can still be set using the other SetUniform*() methods.
 * \warning uses glGetError();
 * \param name - name of the parameter
 * \param a - a bool array containing enough floats to fill the entire uniform array.
 * \return void
 * \author <a href="mailto:jens.schneider@in.tum.de">Jens Schneider</a>
 * \date Mar.2005
 */
void GLSLProgram::SetUniformArray(const char *name,const bool  *a) const {
  assert(m_bEnabled);
  CheckGLError();

  GLint iSize;
  GLenum iType;
  GLint iLocation;

  iLocation = gl::GetUniformLocation(m_hProgram, name);

  if (CheckGLError("SetUniformVector(%s,float,...) [getting adress]",name)) {
    return;
  }

  if(iLocation==-1) {
    T_ERROR("Error getting address for %s.", name);
    return;
  }

  if (m_bGLUseARB) {
    glGetActiveUniformARB(m_hProgram,iLocation,0,NULL,&iSize,&iType,NULL);
  } else {
    glGetActiveUniform(m_hProgram,iLocation,1,&AtiHackLen,&iSize,&iType,
                       &AtiHackChar);
  }

  if (CheckGLError("SetUniformVector(%s,float,...) [getting type]",name)) {
    return;
  }

#ifdef GLSL_ALLOW_IMPLICIT_CASTS
  float *fArray;
#endif
  GLint *iArray;
  switch (iType) {
    case GL_BOOL:
      iArray=new GLint[iSize];
      for (int i=0; i<iSize; i++) iArray[i]=(a[i] ? 1 : 0);
      glUniform1iv(iLocation,iSize,iArray);
      delete[] iArray;
      break;
    case GL_BOOL_VEC2:
      iArray=new GLint[2*iSize];
      for (int i=0; i<2*iSize; i++) iArray[i]=(a[i] ? 1 : 0);
      glUniform2iv(iLocation,iSize,iArray);
      delete[] iArray;
      break;
    case GL_BOOL_VEC3:
      iArray=new GLint[3*iSize];
      for (int i=0; i<3*iSize; i++) iArray[i]=(a[i] ? 1 : 0);
      glUniform3iv(iLocation,iSize,iArray);
      delete[] iArray;
      break;
    case GL_BOOL_VEC4:
      iArray=new GLint[4*iSize];
      for (int i=0; i<4*iSize; i++) iArray[i]=(a[i] ? 1 : 0);
      glUniform4iv(iLocation,iSize,iArray);
      delete[] iArray;
      break;

#ifdef GLSL_ALLOW_IMPLICIT_CASTS
    case GL_INT:
      iArray=new GLint[iSize];
      for (int i=0; i<iSize; i++) iArray[i]=(a[i] ? 1 : 0);
      glUniform1iv(iLocation,iSize,iArray);
      delete[] iArray;
      break;
    case GL_INT_VEC2:
      iArray=new GLint[2*iSize];
      for (int i=0; i<2*iSize; i++) iArray[i]=(a[i] ? 1 : 0);
      glUniform2iv(iLocation,iSize,iArray);
      delete[] iArray;
      break;
    case GL_INT_VEC3:
      iArray=new GLint[3*iSize];
      for (int i=0; i<3*iSize; i++) iArray[i]=(a[i] ? 1 : 0);
      glUniform3iv(iLocation,iSize,iArray);
      delete[] iArray;
      break;
    case GL_INT_VEC4:
      iArray=new GLint[4*iSize];
      for (int i=0; i<4*iSize; i++) iArray[i]=(a[i] ? 1 : 0);
      glUniform4iv(iLocation,iSize,iArray);
      delete[] iArray;
      break;
    case GL_FLOAT:
      fArray=new float[iSize];
      for (int i=0; i<iSize; i++) fArray[i]=(a[i] ? 1.0f : 0.0f);
      glUniform1fv(iLocation,iSize,fArray);
      delete[] fArray;
      break;
    case GL_FLOAT_VEC2:
      fArray=new float[2*iSize];
      for (int i=0; i<2*iSize; i++) fArray[i]=(a[i] ? 1.0f : 0.0f);
      glUniform2fv(iLocation,iSize,fArray);
      delete[] fArray;
      break;
    case GL_FLOAT_VEC3:
      fArray=new float[3*iSize];
      for (int i=0; i<3*iSize; i++) fArray[i]=(a[i] ? 1.0f : 0.0f);
      glUniform3fv(iLocation,iSize,fArray);
      delete[] fArray;
      break;
    case GL_FLOAT_VEC4:
      fArray=new float[4*iSize];
      for (int i=0; i<4*iSize; i++) fArray[i]=(a[i] ? 1.0f : 0.0f);
      glUniform4fv(iLocation,iSize,fArray);
      delete[] fArray;
      break;
#endif

    default:
      T_ERROR("(const char*, const bool*)"
              " Unknown type (%d) for %s.", iType, name);

      break;
  }
#ifdef GLSL_DEBUG
  CheckGLError("SetUniformArray(%s,bool*)",name);
#endif
}


void GLSLProgram::ConnectTextureID(const string& name,
                                   const int iUnit) {
  Enable();
  m_mBindings[name] = iUnit;
  SetUniformVector(name.c_str(),iUnit);
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
 
    Enable();
    SetUniformVector(name.c_str(),iUnusedTexUnit);
    m_mBindings[name] = iUnusedTexUnit;
    pTexture.Bind(iUnusedTexUnit);
  } else {
    pTexture.Bind(m_mBindings[name]);
  }
}
