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

#pragma once

#ifndef RENDERREGION_H
#define RENDERREGION_H

namespace tuvok {

  class RenderRegion {
  public:

    enum EWindowMode {
      WM_SAGITTAL = 0,
      WM_AXIAL    = 1,
      WM_CORONAL  = 2,
      WM_3D,
      WM_INVALID
    };

    UINTVECTOR2 minCoord, maxCoord;
    EWindowMode windowMode;
    bool redrawMask;

    // for 2D window modes:
    VECTOR2<bool> flipView;
    bool useMIP;
    UINT64 sliceIndex;

    RenderRegion():
      redrawMask(true),
      useMIP(false),
      sliceIndex(0)
    {
      flipView = VECTOR2<bool>(false, false);
    }

    bool ContainsPoint(UINTVECTOR2 pos) {
      return (minCoord[0] < pos[0] && pos[0] < maxCoord[0]) &&
        (minCoord[1] < pos[1] && pos[1] < maxCoord[1]);
    }
  };
};
#endif // RENDERREGION_H
