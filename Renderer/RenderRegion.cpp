/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2012 Scientific Computing and Imaging Institute,
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


#include "3rdParty/LUA/lua.hpp"

// Standard
#include "../Basics/Vectors.h"
#include "../StdTuvokDefines.h"

#include "RenderRegion.h"

#include "../LuaScripting/LuaScripting.h"
#include "../LuaScripting/LuaClassRegistration.h"
#include "../LuaScripting/TuvokSpecific/LuaTuvokTypes.h"

namespace tuvok {

//-----------------------------------------------------------------------------
void RenderRegion::defineLuaInterface(LuaClassRegistration<RenderRegion>& reg,
                                      RenderRegion*,
                                      LuaScripting*)
{
  std::string id;

  id = reg.function(&RenderRegion::is2D, "is2D", "True if render region is 2D.",
                    false);
  id = reg.function(&RenderRegion::is3D, "is3D", "True if render region is 2D.",
                    false);

  id = reg.function(&RenderRegion::ContainsPoint, "containsPoint", "True if the "
                    "pixel coordinates given are within our rectangular region.",
                    false);

  id = reg.function(&RenderRegion::luaSetRotation4x4,
                    "setRotation4x4",
                    "Sets the render region's world space rotation as a 4x4"
                    "matrix.",
                    true);
  id = reg.function(&RenderRegion::luaGetRotation4x4,
                    "getRotation4x4",
                    "Retrieves render region's rotation as a 4x4 matrix.",
                    false);

  id = reg.function(&RenderRegion::luaSetTranslation4x4,
                    "setTranslation4x4",
                    "Sets the render region's translation as a 4x4 matrix.",
                    true);
  id = reg.function(&RenderRegion::luaGetTranslation4x4,
                    "getTranslation4x4",
                    "Retrieves the render region's translation as a 4x4 matrix."
                    , false);

  id = reg.function(&RenderRegion::luaSet2DFlipMode,
                    "set2DFlipMode",
                    "Sets horizontal and vertical flip flags.", true);
  id = reg.function(&RenderRegion::luaGet2DFlipModeX,
                    "get2DFlipModeX",
                    "Returns current horizontal flip flag value.", false);
  id = reg.function(&RenderRegion::luaGet2DFlipModeY,
                    "get2DFlipModeY",
                    "Returns current vertical flip flag value.", false);

  id = reg.function(&RenderRegion::luaSetUseMIP,
                    "setUseMIP",
                    "Toggle Maximum Intensity Projection on/off.", true);
  id = reg.function(&RenderRegion::luaGetUseMIP,
                    "getUseMIP",
                    "Retrieve the state of Maximum Intensity Projection.",
                    false);

  id = reg.function(&RenderRegion::luaSetSliceDepth,
                    "setSliceDepth",
                    "Sets the slice depth.",
                    true);
  id = reg.function(&RenderRegion::luaGetSliceDepth,
                    "getSliceDepth",
                    "Retrieves the slice depth.",
                    false);
  id = reg.function(&RenderRegion::luaSetClipPlane,
                    "setClipPlane",
                    "Sets the arbitrary clipping plane against which to clip "
                    "the volume.",
                    true);
  id = reg.function(&RenderRegion::luaGetClipPlane,
                    "getClipPlane",
                    "Retrieves arbitrary clipping plane.",
                    false);
  id = reg.function(&RenderRegion::luaEnableClipPlane,
                    "enableClipPlane",
                    "Enables/Disables clipping plane.",
                    true);
  id = reg.function(&RenderRegion::luaIsClipPlaneEnabled,
                    "isClipPlaneEnabled",
                    "Returns enabled status of clipping plane.",
                    true);
  id = reg.function(&RenderRegion::luaShowClipPlane,
                    "showClipPlane",
                    "Enable/Disables clip plane visibility.",
                    true);
  id = reg.function(&RenderRegion::luaGetModelView,
                    "getModelView",
                    "Retrieves model view matrix.",
                    false);
}

//-----------------------------------------------------------------------------
void RenderRegion::luaSetRotation4x4(FLOATMATRIX4 mat)
{
  mRen->SetRotationRR(this, mat);
}

//-----------------------------------------------------------------------------
FLOATMATRIX4 RenderRegion::luaGetRotation4x4()
{
  return mRen->GetRotation(this);
}

//-----------------------------------------------------------------------------
void RenderRegion::luaSet2DFlipMode(bool bFlipX, bool bFlipY)
{
  mRen->Set2DFlipMode(this, bFlipX, bFlipY);
}

//-----------------------------------------------------------------------------
bool RenderRegion::luaGet2DFlipModeX()
{
  bool x = false, y = false;
  mRen->Get2DFlipMode(this, x, y);
  return x;
}

//-----------------------------------------------------------------------------
bool RenderRegion::luaGet2DFlipModeY()
{
  bool x = false, y = false;
  mRen->Get2DFlipMode(this, x, y);
  return y;
}

//-----------------------------------------------------------------------------
bool RenderRegion::luaGetUseMIP() const
{
  return mRen->GetUseMIP(this);
}

//-----------------------------------------------------------------------------
void RenderRegion::luaSetUseMIP(bool bUseMIP)
{
  mRen->SetUseMIP(this, bUseMIP);
}

//-----------------------------------------------------------------------------
void RenderRegion::luaSetTranslation4x4(FLOATMATRIX4 translation)
{
  mRen->SetTranslation(this, translation);
}

//-----------------------------------------------------------------------------
FLOATMATRIX4 RenderRegion::luaGetTranslation4x4()
{
  return mRen->GetTranslation(this);
}

//-----------------------------------------------------------------------------
void RenderRegion::luaSetSliceDepth(uint64_t sliceDepth)
{
  mRen->SetSliceDepth(this, sliceDepth);
}

//-----------------------------------------------------------------------------
uint64_t RenderRegion::luaGetSliceDepth() const
{
  return mRen->GetSliceDepth(this);
}

//-----------------------------------------------------------------------------
void RenderRegion::luaSetClipPlane(ExtendedPlane plane)
{
  mRen->SetClipPlane(this, plane);
}

//-----------------------------------------------------------------------------
ExtendedPlane RenderRegion::luaGetClipPlane()
{
  return mRen->GetClipPlane();
}

//-----------------------------------------------------------------------------
void RenderRegion::luaEnableClipPlane(bool enable)
{
  if (enable)
    mRen->EnableClipPlane(this);
  else
    mRen->DisableClipPlane(this);
}

//-----------------------------------------------------------------------------
bool RenderRegion::luaIsClipPlaneEnabled()
{
  return mRen->IsClipPlaneEnabled(this);
}

//-----------------------------------------------------------------------------
void RenderRegion::luaShowClipPlane(bool enable)
{
  mRen->ShowClipPlane(enable, this);
}

//-----------------------------------------------------------------------------
FLOATMATRIX4 RenderRegion::luaGetModelView(int stereoIndex)
{
  return modelView[stereoIndex];
}

}


