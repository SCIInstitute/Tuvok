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
  \file    QtGLContext.h
  \author  Tom Fogal
           SCI Institute
           University of Utah
*/
#pragma once

#ifndef TUVOK_QT_GL_CONTEXT_ID_H
#define TUVOK_QT_GL_CONTEXT_ID_H

#include <GL/glew.h>
#include <QtOpenGL/QGLContext>

#include "../Context.h"
#include "GLStateManager.h"

namespace tuvok {

/// GL context information based on the `QGLContext' class.
class QtGLContext : public Context {
  public:
    /// Create an ID with the current context.
    QtGLContext() {
      ctx = QGLContext::currentContext();
      if (ctx) 
        m_pState = std::tr1::shared_ptr<StateManager>(new GLStateManager());
      else
        m_pState = std::tr1::shared_ptr<StateManager>();
    }
    /// Create an ID from the given context.
    /// NOTE: Do not create multiple QtGLContext's from the same QGLContext!
    QtGLContext(const QGLContext *ct) {
      ctx = ct;
      if (ctx) 
        m_pState = std::tr1::shared_ptr<StateManager>(new GLStateManager());
      else
        m_pState = std::tr1::shared_ptr<StateManager>();
    }
    QtGLContext(const QtGLContext& ct) {
      ctx = ct.ctx;
      m_pState = ct.m_pState;
    }

    static std::tr1::shared_ptr<Context> Current() {
       if(contextMap.find(QGLContext::currentContext()) == contextMap.end()) {
         std::pair<const void*, std::tr1::shared_ptr<Context> > tmp(
           QGLContext::currentContext(),
           std::tr1::shared_ptr<Context>(new QtGLContext())
         );
         return contextMap.insert(tmp).first->second; // return what we're inserting
       }
       return contextMap[QGLContext::currentContext()];
    }

    bool operator==(const QtGLContext &gl_cid) const {
      return this->ctx == gl_cid.ctx;
    }
    bool operator!=(const QtGLContext &gl_cid) const {
      return this->ctx != gl_cid.ctx;
    }

    QtGLContext& operator=(const QtGLContext &ct) {
      this->ctx = ct.ctx;
      return *this;
    }

  private:
    QtGLContext(const Context&); ///< unimplemented
};

}
#endif // TUVOK_GL_CONTEXT_ID_H
