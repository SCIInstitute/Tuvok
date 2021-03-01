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
  \file    GLFBOTex.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
           Jens Schneider
           tum.3D, Muenchen
  \date    August 2008
*/
#ifndef TUVOK_GLFBOTEX_H_
#define TUVOK_GLFBOTEX_H_

#include "../../StdTuvokDefines.h"
#include "GLObject.h"
#include "GLTexture.h"

namespace tuvok {

class MasterController;

class GLFBOTex : public GLObject {
public:
  GLFBOTex(MasterController* pMasterController, GLenum minfilter,
           GLenum magfilter, GLenum wrapmode, GLsizei width, GLsizei height,
           GLenum intformat, GLenum format, GLenum type,
           bool bHaveDepth=false, int iNumBuffers=1);
  virtual ~GLFBOTex(void);
  virtual void SetViewport();
  virtual void Write(unsigned int iTargetBuffer=0, int iBuffer=0,
                     bool bCheckBuffer=true);
  virtual void Read(unsigned int iTargetUnit,int iBuffer=0);
  virtual void FinishWrite(int iBuffer=0);
  virtual void FinishRead(int iBuffer=0);
  virtual void ReadDepth(unsigned int iTargetUnit);
  virtual void FinishDepthRead();
  virtual void CopyToFramebuffer(unsigned int iBuffer=0);
  virtual void CopyToFramebuffer(unsigned int x, unsigned int w,
                                 unsigned int y, unsigned int h,
                                 unsigned int tx, unsigned int tw,
                                 unsigned int ty, unsigned int th,
                                 unsigned int iBuffer=0,
                                 GLenum eFilter=GL_NEAREST);
  virtual operator GLuint(void) { return m_hTexture[0]; }
  virtual operator GLuint*(void) { return m_hTexture; }

  /// \todo check how much mem an FBO really occupies
  virtual uint64_t GetCPUSize() const;
  virtual uint64_t GetGPUSize() const;

  static uint64_t EstimateCPUSize(GLsizei width, GLsizei height,
                                  size_t iSizePerElement,
                                  bool bHaveDepth=false, int iNumBuffers=1);
  static uint64_t EstimateGPUSize(GLsizei width, GLsizei height,
                                  size_t iSizePerElement,
                                  bool bHaveDepth=false, int iNumBuffers=1);

  bool Valid() const { return m_hFBO != 0; }

  static void NoDrawBuffer();
  static void OneDrawBuffer();
  static void TwoDrawBuffers();
  static void ThreeDrawBuffers();
  static void FourDrawBuffers();

  void ReadBackPixels(int x, int y, int sX, int sY, void* pData);
  GLuint Width() const {return m_iSizeX;}
  GLuint Height() const {return m_iSizeY;}

  void SetData(const void *pixels, int iBuffer=0, bool bRestoreBinding=true);
  void SetData(const UINTVECTOR2& offset, const UINTVECTOR2& size,
               const void *pixels, int iBuffer=0, bool bRestoreBinding=true);

private:
  MasterController*   m_pMasterController;
  GLuint              m_iSizeX;
  GLuint              m_iSizeY;
  GLuint*             m_hTexture;
  GLuint              m_hDepthBuffer;
  static GLuint       m_hFBO;
  static bool         m_bInitialized;
  static int          m_iCount;
  GLenum*             m_LastTexUnit;
  GLenum              m_LastDepthTextUnit;
  int                 m_iNumBuffers;
  GLenum*             m_LastAttachment;
  GLenum              m_intformat;
  GLenum              m_format;
  GLenum              m_type;

  bool CheckFBO(const char* method);
  void initFBO(void);
  bool initTextures(GLenum minfilter, GLenum magfilter, GLenum wrapmode,
                    GLsizei width, GLsizei height, GLenum intformat,
                    GLenum format, GLenum type);
};
} // tuvok namespace
#endif  // TUVOK_GLFBOTEX_H_
