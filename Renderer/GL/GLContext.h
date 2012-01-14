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
  \file    GLContext.h
  \author  Tom Fogal
           SCI Institute
           University of Utah
*/
#pragma once

#ifndef TUVOK_GL_CONTEXT_ID_H
#define TUVOK_GL_CONTEXT_ID_H

#include <StdTuvokDefines.h>

#ifdef DETECTED_OS_WINDOWS
# include <GL/wglew.h>
#else
# include <GL/glxew.h>
#endif
#include "../Context.h"
#include "GLStateManager.h"

namespace tuvok {

class GLContext : public Context {
public:
#ifdef DETECTED_OS_WINDOWS
  #define GetContext wglGetCurrentContext
#else
  #define GetContext glXGetCurrentContext
#endif

    GLContext(int iShareGroupID) : Context(iShareGroupID) {
      ctx = GetContext();
      if (ctx) 
        m_pState = std::tr1::shared_ptr<StateManager>(new GLStateManager());
      else
        m_pState = std::tr1::shared_ptr<StateManager>();
    }

    /// Create an ID from the given context.
    GLContext(const GLContext& ct) : Context(ct.m_iShareGroupID) {
      ctx = ct.ctx;
      m_pState = ct.m_pState;
    }
    
    static std::tr1::shared_ptr<Context> Current(int iShareGroupID) {
       if(contextMap.find(GetContext()) == contextMap.end()) {
         std::pair<const void*, std::tr1::shared_ptr<Context> > tmp(
            GetContext(),
            std::tr1::shared_ptr<Context>(new GLContext(iShareGroupID))
         );
         return contextMap.insert(tmp).first->second; // return what we're inserting
       }
       return contextMap[GetContext()];
    }

    bool operator==(const GLContext &gl_cid) const {
      return this->ctx == gl_cid.ctx;
    }
    bool operator!=(const GLContext &gl_cid) const {
      return this->ctx != gl_cid.ctx;
    }

    GLContext& operator=(const GLContext &ct) {
      this->m_iShareGroupID = ct.m_iShareGroupID;
      this->ctx = ct.ctx;
      return *this;
    }

  private:
    GLContext(const Context&); ///< unimplemented
};

}
#endif // TUVOK_GL_CONTEXT_ID_H
