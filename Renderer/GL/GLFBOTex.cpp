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
  \file    GLFBOTex.cpp
  \author  Jens Krueger
           SCI Institute
           University of Utah
           Jens Schneider
           tum.3D, Muenchen
  \date    August 2008
*/
#include "GLFBOTex.h"
#include "GLCommon.h"
#include <Controller/Controller.h>

#ifdef WIN32
  #ifndef DEBUG
    #pragma warning( disable : 4189 ) // disable unused var warning
  #endif
#endif

using namespace tuvok;

GLuint  GLFBOTex::m_hFBO = 0;
int    GLFBOTex::m_iCount = 0;
bool  GLFBOTex::m_bInitialized = true;

/**
 * Constructor: on first instantiation, generate an FBO.
 * In any case a new dummy texture according to the parameters is generated.
 */

GLFBOTex::GLFBOTex(MasterController* pMasterController, GLenum minfilter,
                   GLenum magfilter, GLenum wrapmode, GLsizei width,
                   GLsizei height, GLenum intformat, GLenum format, GLenum type, 
                   bool bHaveDepth, int iNumBuffers) :
  m_pMasterController(pMasterController),
  m_iSizeX(width),
  m_iSizeY(height),
  m_hTexture(new GLuint[iNumBuffers]),
  m_LastTexUnit(NULL),
  m_LastDepthTextUnit(0),
  m_iNumBuffers(iNumBuffers),
  m_LastAttachment(NULL),
  m_intformat(intformat),
  m_format(format),
  m_type(type)
{
  if (width<1) width=1;
  if (height<1) height=1;
  if (!m_bInitialized) {
    if (GLEW_OK!=glewInit()) {
      T_ERROR("failed to initialize GLEW!");
      return;
    }
    if (!glewGetExtension("GL_EXT_framebuffer_object")) {
      T_ERROR("GL_EXT_framebuffer_object not supported!");
      return;
    }
    m_bInitialized=true;
  }
  assert(iNumBuffers>0);
  assert(iNumBuffers<5);
  m_LastTexUnit=new GLenum[iNumBuffers];
  m_LastAttachment=new GLenum[iNumBuffers];
  for (int i=0; i<m_iNumBuffers; i++) {
    m_LastTexUnit[i]=0;
    m_LastAttachment[i]=GL_COLOR_ATTACHMENT0_EXT+i;
    m_hTexture[i]=0;
  }
  while(glGetError() != GL_NO_ERROR) { ; } // clear error state.
  if (m_hFBO==0)
    initFBO();

  {
    GLenum glerr = glGetError();
    if(GL_NO_ERROR != glerr) {
      T_ERROR("Error '%d' during FBO creation!", static_cast<int>(glerr));
      GL(glDeleteFramebuffersEXT(1,&m_hFBO));
      m_hFBO=0;
      return;
    }
  }

  while(glGetError() != GL_NO_ERROR) { ; } // clear error state.
  if (!initTextures(minfilter,magfilter,wrapmode,width,height,intformat, format, type)) 
  {
      T_ERROR("GL Error during texture creation!");
      GL(glDeleteTextures(m_iNumBuffers,m_hTexture));
      delete[] m_hTexture;
      m_hTexture=NULL;
      return;
  }
  if (bHaveDepth) {
#ifdef GLFBOTEX_DEPTH_RENDERBUFFER
    GL(glGenRenderbuffersEXT(1,&m_hDepthBuffer));
    GL(glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24,
                                width,height));
#else
    GL(glGenTextures(1,&m_hDepthBuffer));
    GL(glBindTexture(GL_TEXTURE_2D,m_hDepthBuffer));
    GL(glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE));
    GL(glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE));
    GL(glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST));
    GL(glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST));
    GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0,
                    GL_DEPTH_COMPONENT, GL_FLOAT, NULL));
#endif
  }
  else m_hDepthBuffer=0;

  ++m_iCount;
}


/**
 * Destructor: Delete texture object. If no more instances of
 * GLFBOTex are around, the FBO is deleted as well.
 */
GLFBOTex::~GLFBOTex(void) {
  if (m_hTexture) {
    GL(glDeleteTextures(m_iNumBuffers,m_hTexture));
    delete[] m_hTexture;
    m_hTexture=NULL;
  }
  delete[] m_LastTexUnit;
  m_LastTexUnit=NULL;

  delete[] m_LastAttachment;
  m_LastAttachment=NULL;

#ifdef GLFBOTEX_DEPTH_RENDERBUFFER
  if (m_hDepthBuffer) GL(glDeleteRenderbuffersEXT(1,&m_hDepthBuffer));
#else
  if (m_hDepthBuffer) GL(glDeleteTextures(1,&m_hDepthBuffer));
#endif
  m_hDepthBuffer=0;
  --m_iCount;
  if (m_iCount==0) {
    m_pMasterController->DebugOut()->Message(_func_, "FBO released via "
                                                     "destructor call.");
    GL(glDeleteFramebuffersEXT(1,&m_hFBO));
    m_hFBO=0;
  }
}



/**
 * Build a dummy texture according to the parameters.
 */
bool GLFBOTex::initTextures(GLenum minfilter, GLenum magfilter,
                            GLenum wrapmode, GLsizei width, GLsizei height,
                            GLenum intformat, GLenum format, GLenum type) {
  MESSAGE("Initializing %u 2D texture(s) of size %ux%u (MinFilter=%#x "
          "MinFilter=%#x WrapMode=%#x, IntFormat=%#x)",
          static_cast<unsigned>(m_iNumBuffers),
          static_cast<unsigned>(width), static_cast<unsigned>(height),
          static_cast<unsigned>(minfilter), static_cast<unsigned>(magfilter),
          static_cast<unsigned>(wrapmode), static_cast<unsigned>(intformat));
  //glDeleteTextures(m_iNumBufers,m_hTexture);
  GL(glGenTextures(m_iNumBuffers,m_hTexture));
  for (int i=0; i<m_iNumBuffers; i++) {
    GL_RET(glBindTexture(GL_TEXTURE_2D, m_hTexture[i]));
    GL_RET(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minfilter));
    GL_RET(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magfilter));
    GL_RET(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapmode));
    GL_RET(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapmode));
    int level=0;
    switch (minfilter) {
      case GL_NEAREST_MIPMAP_NEAREST:
      case GL_LINEAR_MIPMAP_NEAREST:
      case GL_NEAREST_MIPMAP_LINEAR:
      case GL_LINEAR_MIPMAP_LINEAR:
        do {
          GL_RET(glTexImage2D(GL_TEXTURE_2D, level, intformat, width, height, 0,
                       format, type, NULL));
          width/=2;
          height/=2;
          if (width==0 && height>0) width=1;
          if (width>0 && height==0) height=1;
          ++level;
        } while (width>=1 && height>=1);
        break;
      default:
        GL_RET(glTexImage2D(GL_TEXTURE_2D, 0, intformat, width, height, 0,
                            format, type, NULL));
        break;
    }
  }
  return true;
}


/**
 * Build a new FBO.
 */
void GLFBOTex::initFBO(void) {
  MESSAGE("Initializing FBO...");
  // Don't wrap this in a `GL()'!  The caller is expected to query the GL
  // error state to see if this worked.
  glGenFramebuffersEXT(1, &m_hFBO);
}

/**
 * Check the FBO for consistency.
 */
bool GLFBOTex::CheckFBO(const char* method) {
  GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
  switch(status) {
    case GL_FRAMEBUFFER_COMPLETE_EXT:
      return true;
    case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
      T_ERROR("%s() - Unsupported Format!",method); return false;
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
      T_ERROR("%s() - Incomplete attachment",method); return false;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
      T_ERROR("%s() - Incomplete missing attachment",method); return false;
    case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
      T_ERROR("%s() - Incomplete dimensions",method); return false;
    case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
      T_ERROR("%s() - Incomplete formats",method); return false;
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
      T_ERROR("%s() - Incomplete draw buffer",method); return false;
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
      T_ERROR("%s() - Incomplete read buffer",method); return false;
    default:  return false;
  }
}

void GLFBOTex::SetViewport() {
    glViewport(0, 0, m_iSizeX, m_iSizeY);
}

/**
 * Lock texture for writing. Texture may not be bound any more!
 */
void GLFBOTex::Write(unsigned int iTargetBuffer, int iBuffer, bool bCheckBuffer) {
  GLenum target = GL_COLOR_ATTACHMENT0_EXT + iTargetBuffer;

  if (!m_hFBO) {
    T_ERROR("FBO not initialized!");
    return;
  }

  GL(glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,m_hFBO));
  assert(iBuffer>=0);
  assert(iBuffer<m_iNumBuffers);
  m_LastAttachment[iBuffer]=target;
  GL(glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, target, GL_TEXTURE_2D,
                               m_hTexture[iBuffer], 0));
  if (m_hDepthBuffer)
    GL(glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
                                 GL_TEXTURE_2D, m_hDepthBuffer, 0));
  if (bCheckBuffer) {
#ifdef _DEBUG
  if (!CheckFBO("Write")) return;
#endif
  }
}

void GLFBOTex::FinishWrite(int iBuffer) {
  assert(iBuffer>=0);
  assert(iBuffer<m_iNumBuffers);
  GL(glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,m_hFBO));
  GL(glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, m_LastAttachment[iBuffer],
                               GL_TEXTURE_2D, 0, 0));
  if (m_hDepthBuffer)
    GL(glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
                                 GL_TEXTURE_2D, 0, 0));
  GL(glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0));
}

void GLFBOTex::Read(unsigned int iTargetUnit, int iBuffer) {
  GLenum texunit = GL_TEXTURE0 + iTargetUnit;
#ifdef _DEBUG
  if (m_LastTexUnit[iBuffer]!=0) {
    T_ERROR("Missing FinishRead()!");
  }
#endif
  assert(iBuffer>=0);
  assert(iBuffer<m_iNumBuffers);
  m_LastTexUnit[iBuffer]=texunit;
  GL(glActiveTextureARB(texunit));
  GL(glBindTexture(GL_TEXTURE_2D,m_hTexture[iBuffer]));
}

void GLFBOTex::ReadDepth(unsigned int iTargetUnit) {
  GLenum texunit = GL_TEXTURE0 + iTargetUnit;
#ifdef _DEBUG
  if (m_LastDepthTextUnit!=0) {
    T_ERROR("Missing FinishDepthRead()!");
  }
#endif
  m_LastDepthTextUnit=texunit;
  GL(glActiveTextureARB(texunit));
  GL(glBindTexture(GL_TEXTURE_2D,m_hDepthBuffer));
}

// Finish reading from the depth texture
void GLFBOTex::FinishDepthRead() {
  GL(glActiveTextureARB(m_LastDepthTextUnit));
  GL(glBindTexture(GL_TEXTURE_2D,0));
  m_LastDepthTextUnit=0;
}

void GLFBOTex::NoDrawBuffer() {
  GL(glDrawBuffer(GL_NONE));
}

void GLFBOTex::OneDrawBuffer() {
  GL(glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT));
}

void GLFBOTex::TwoDrawBuffers() {
  GLenum twobuffers[]  = { GL_COLOR_ATTACHMENT0_EXT,
                           GL_COLOR_ATTACHMENT1_EXT };
  GL(glDrawBuffers(2, twobuffers));
}

void GLFBOTex::ThreeDrawBuffers() {
  GLenum threebuffers[]  = { GL_COLOR_ATTACHMENT0_EXT,
                            GL_COLOR_ATTACHMENT1_EXT,
                            GL_COLOR_ATTACHMENT2_EXT};
  GL(glDrawBuffers(3, threebuffers));
}

void GLFBOTex::FourDrawBuffers() {
  GLenum fourbuffers[]  = { GL_COLOR_ATTACHMENT0_EXT,
                            GL_COLOR_ATTACHMENT1_EXT,
                            GL_COLOR_ATTACHMENT2_EXT,
                            GL_COLOR_ATTACHMENT3_EXT};
  GL(glDrawBuffers(4, fourbuffers));
}

// Finish reading from this texture
void GLFBOTex::FinishRead(int iBuffer) {
  assert(iBuffer>=0);
  assert(iBuffer<m_iNumBuffers);
  GL(glActiveTextureARB(m_LastTexUnit[iBuffer]));
  GL(glBindTexture(GL_TEXTURE_2D,0));
  m_LastTexUnit[iBuffer]=0;
}


void GLFBOTex::ReadBackPixels(int x, int y, int sX, int sY, void* pData) {
  // read back the 3D position from the framebuffer
  Write(0,0,false);
  GL(glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE));
  GL(glReadBuffer(GL_COLOR_ATTACHMENT0_EXT));
  GL(glReadPixels(x, y, sX, sY, GL_RGBA, GL_FLOAT, pData));
  FinishWrite(0);
}

uint64_t GLFBOTex::GetCPUSize() const {
    return EstimateCPUSize(m_iSizeX, m_iSizeY, GLCommon::gl_byte_width(m_format) * GLCommon::gl_components(m_format),
                           m_hDepthBuffer!=0, m_iNumBuffers);
}

uint64_t GLFBOTex::GetGPUSize() const {
    return EstimateGPUSize(m_iSizeX, m_iSizeY, GLCommon::gl_byte_width(m_format) * GLCommon::gl_components(m_format),
                           m_hDepthBuffer!=0, m_iNumBuffers);
}

uint64_t GLFBOTex::EstimateCPUSize(GLsizei width, GLsizei height,
                                  size_t iSizePerElement,
                                  bool bHaveDepth, int iNumBuffers) {
    return iNumBuffers*width*height*iSizePerElement +
           ((bHaveDepth) ? width*height*4 : 0);
}
uint64_t GLFBOTex::EstimateGPUSize(GLsizei width, GLsizei height,
                              size_t iSizePerElement,
                              bool bHaveDepth, int iNumBuffers) {
  return EstimateCPUSize(width, height, iSizePerElement, bHaveDepth,
                          iNumBuffers);
}