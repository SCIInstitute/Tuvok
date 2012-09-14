#pragma once

#ifndef TUVOK_VISIBILITYSTATE_H
#define TUVOK_VISIBILITYSTATE_H

#include "Renderer/AbstrRenderer.h"

namespace tuvok
{
  /** \class VisibilityState
   * Caches last given visibility state.
   *
   * Simply stores last given render mode along with transfer function (min, max, ...)
   * parameters and tells you whether visibility needs to be recomputed or not. */
  class VisibilityState
  {
  public:
    struct RM1DTransfer {
      double fMin;
      double fMax;
    };
    struct RM2DTransfer {
      double fMin;
      double fMax;
      double fMinGradient;
      double fMaxGradient;
    };
    struct RMIsoSurface {
      double fIsoValue;
    };

    VisibilityState();

    bool NeedsUpdate(double fMin, double fMax);
    bool NeedsUpdate(double fMin, double fMax, double fMinGradient, double fMaxGradient);
    bool NeedsUpdate(double fIsoValue);

    AbstrRenderer::ERenderMode GetRenderMode() const { return m_eRenderMode; }
    RM1DTransfer const& Get1DTransfer() const { return m_rm1DTrans; }
    RM2DTransfer const& Get2DTransfer() const { return m_rm2DTrans; }
    RMIsoSurface const& GetIsoSurface() const { return m_rmIsoSurf; }

  private:
    AbstrRenderer::ERenderMode m_eRenderMode;
    union {
      RM1DTransfer m_rm1DTrans;
      RM2DTransfer m_rm2DTrans;
      RMIsoSurface m_rmIsoSurf;
    };
  };

} // namespace tuvok

#endif // TUVOK_VISIBILITYSTATE_H

/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2012 Interactive Visualization and Data Analysis Group.


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
