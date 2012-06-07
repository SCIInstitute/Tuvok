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

#include "../Basics/Plane.h"

namespace tuvok {

class AbstrRenderer;
class GLRenderer;
class GLSBVR2D;

class LuaScripting;
template <typename T> class LuaClassRegistration;


  // NOTE: client code should never directly modify a RenderRegion. Instead,
  // modifications should be done through the tuvok API so that tuvok is aware
  // of these changes.

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

    // does drawing (possibly at higher quality settings) still need to be done?
    bool redrawMask;

    // has this never been drawn (are we starting from scratch for this view)?
    bool isBlank;

    // no LOD has been completed (this is different from "isBlank" above as 
    // it considers what is visible to the user not what is in the backbuffer)
    bool isTargetBlank;

    RenderRegion(EWindowMode mode, AbstrRenderer* ren):
      windowMode(mode),
      redrawMask(true),
      isBlank(true),
      isTargetBlank(true),
      mRen(ren)
    {
    }
    virtual ~RenderRegion() { /* nothing to destruct */ }

    virtual bool is2D() const { return false; }
    virtual bool is3D() const { return false; }

    bool ContainsPoint(UINTVECTOR2 pos) {
      return (minCoord[0] < pos[0] && pos[0] < maxCoord[0]) &&
        (minCoord[1] < pos[1] && pos[1] < maxCoord[1]);
    }

    static void defineLuaInterface(LuaClassRegistration<RenderRegion>& reg,
                                   RenderRegion* me,
                                   LuaScripting* ss);

    // These really should just be for 3D and for 3D MIP. But because 3D MIP is
    // considered 2D, we have to put this here for now...
    FLOATMATRIX4 modelView[2]; // one for each eye (if in stereo mode).
    FLOATMATRIX4 rotation;
    FLOATMATRIX4 translation;

  protected:

    // These methods should be accessed through AbstrRenderer
    friend class AbstrRenderer;
    friend class GLRenderer;

    // 2D methods
    virtual bool GetUseMIP() const = 0;
    virtual void SetUseMIP(bool) = 0;
    virtual uint64_t GetSliceIndex() const = 0;
    virtual void SetSliceIndex(uint64_t index) = 0;
    virtual void GetFlipView(bool &flipX, bool &flipY) const = 0;
    virtual void SetFlipView(bool flipX, bool flipY) = 0;

    AbstrRenderer* mRen;

  private:

    // Lua functions that expose functionality. The guts of most of these
    // functions exist in the abstract renderer.
    void luaSetRotation4x4(FLOATMATRIX4 mat);
    FLOATMATRIX4 luaGetRotation4x4();
    void luaSet2DFlipMode(bool bFlipX, bool bFlipY);
    bool luaGet2DFlipModeX();
    bool luaGet2DFlipModeY();
    bool luaGetUseMIP() const;
    void luaSetUseMIP(bool bUseMIP);
    void luaSetTranslation4x4(FLOATMATRIX4 translation);
    FLOATMATRIX4 luaGetTranslation4x4();
    void luaSetSliceDepth(uint64_t sliceDepth);
    uint64_t luaGetSliceDepth() const;
    void luaSetClipPlane(ExtendedPlane plane);
    ExtendedPlane luaGetClipPlane();
    void luaEnableClipPlane(bool enable);
    bool luaIsClipPlaneEnabled();
    void luaShowClipPlane(bool enable);
  };

  class RenderRegion2D : public RenderRegion {
  public:
    RenderRegion2D(EWindowMode mode, uint64_t sliceIndex, AbstrRenderer* ren) :
      RenderRegion(mode, ren),
      useMIP(false),
      sliceIndex(sliceIndex)
    {
      flipView = VECTOR2<bool>(false, false);
    }
    virtual ~RenderRegion2D() { /* nothing to destruct */ }

    virtual bool is2D() const { return true; }

  protected:
    // These methods should be accessed through AbstrRenderer
    friend class AbstrRenderer;
    friend class GLRenderer;
    friend class GLSBVR2D;
    virtual bool GetUseMIP() const { return useMIP; }
    virtual void SetUseMIP(bool useMIP_) { useMIP = useMIP_; }
    virtual uint64_t GetSliceIndex() const { return sliceIndex; }
    virtual void SetSliceIndex(uint64_t index) { sliceIndex = index; }
    virtual void GetFlipView(bool &flipX, bool &flipY) const {
      flipX = flipView.x;
      flipY = flipView.y;
    }
    virtual void SetFlipView(bool flipX, bool flipY) {
      flipView.x = flipX;
      flipView.y = flipY;
    }

    VECTOR2<bool> flipView;
    bool useMIP;
    uint64_t sliceIndex;
  };

  class RenderRegion3D : public RenderRegion {
  public:
    RenderRegion3D(AbstrRenderer* ren) : RenderRegion(WM_3D, ren) {}
    virtual ~RenderRegion3D() { /* nothing to destruct */ }

    virtual bool is3D() const { return true; }

  protected:
    // These methods should be accessed through AbstrRenderer
    friend class AbstrRenderer;
    friend class GLRenderer;

    // 3D regions don't do the following things:
    virtual bool GetUseMIP() const { assert(false); return false; }
    virtual void SetUseMIP(bool) { assert(false); }
    virtual uint64_t GetSliceIndex() const { assert(false); return 0; }
    virtual void SetSliceIndex(uint64_t) { assert(false); }
    virtual void SetFlipView(bool, bool) { assert(false); }
    virtual void GetFlipView(bool &, bool &) const {assert(false); }
  };
};

#endif // RENDERREGION_H
