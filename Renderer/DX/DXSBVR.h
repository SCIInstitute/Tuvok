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
  \file    DXSBVR.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \version 1.0
  \date    November 2008
*/


#pragma once

#if defined(_WIN32) && defined(USE_DIRECTX)

#ifndef DXSBVR_H
#define DXSBVR_H

#include "../../StdTuvokDefines.h"
#include "DXRenderer.h"

namespace tuvok {

/** \class DXSBVR
 * DirectX 10 GPU SBVR.
 *
 * DXSBVR is a GPU slice based renderer for volumetric scalar data which uses HLSL. */
class DXSBVR : public DXRenderer {
  public:
    /** Constructs a VRer with immediate redraw, and
     * wireframe mode off.
     * \param pMasterController message routing object
     * \param bUseOnlyPowerOfTwo force power of two textures (compatibility)
     * \param bDownSampleTo8Bits force 8bit textures (compatibility) */
    DXSBVR(MasterController* pMasterController, bool bUseOnlyPowerOfTwo, bool bDownSampleTo8Bits, bool bDisableBorder);
    virtual ~DXSBVR();

    virtual ERendererType GetRendererType() {return RT_SBVR;}

  protected:
    virtual void Render3DInLoop(size_t iCurrentBrick, EStereoID eStereoID);
    virtual void RenderHQMIPInLoop(const Brick& b);
    virtual void UpdateLightParamsInShaders();
};

}; //namespace tuvok

#endif // DXSBVR_H

#endif // _WIN32 && USE_DIRECTX
