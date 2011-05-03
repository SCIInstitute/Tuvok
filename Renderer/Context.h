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
  \file    Context.h
  \author  Tom Fogal
           SCI Institute
           University of Utah
  \brief   Base class for holding comparative context information.
*/
#pragma once

#ifndef TUVOK_CONTEXT_ID_H
#define TUVOK_CONTEXT_ID_H

#include "StateManager.h"
#include <map>

namespace tuvok {

class Context {
public:
  virtual ~Context() {}

protected:
  static std::map<const void*, std::tr1::shared_ptr<Context> > contextMap;
  static std::tr1::shared_ptr<StateManager> m_pState;
  const void* ctx; /// will be GLXContext, HGLRC, or Device
};

}

#endif // TUVOK_CONTEXT_ID_H