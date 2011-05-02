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
  \file    DXContext.h
  \author  Tom Fogal
           SCI Institute
           University of Utah
*/
#pragma once

#ifndef TUVOK_DX_CONTEXT_ID_H
#define TUVOK_DX_CONTEXT_ID_H

#include "../ContextID.h"

#include "../../Basics/DynamicDX.h"

namespace tuvok {

/// Abstraction for current DirectX context.  See base class for details.
///
/// Currently unimplemented, just holds null; this means that all DirectX
/// contexts will be considered equivalent.
class DXContextID : public ContextID<DXContextID> {
  public:
    DXContextID() : ContextID<DXContextID>(),
                    ctx(NULL) {}
    /// Create an ID from the given context.
    DXContextID(ID3D10Device *c) : ContextID<DXContextID>(), ctx(c) {}
    DXContextID(const DXContextID &dx) : ContextID<DXContextID>(),
                                         ctx(dx.ctx) { }

    static DXContextID Current(ID3D10Device *c) { return DXContextID(c); }

    bool operator==(const DXContextID &dx_cid) const {
      return this->ctx == dx_cid.ctx;
    }
    bool operator!=(const DXContextID &dx_cid) const {
      return this->ctx != dx_cid.ctx;
    }

    DXContextID& operator=(const DXContextID &ct) {
      this->ctx = ct.ctx;
      return *this;
    }

  private:
    const ID3D10Device* ctx;
};

};

#endif // TUVOK_DX_CONTEXT_ID_H
