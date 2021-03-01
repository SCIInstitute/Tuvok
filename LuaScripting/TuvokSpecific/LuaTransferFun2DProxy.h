
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

/**
 \brief Lua proxy for IO's 2D transfer function.
 */

#ifndef TUVOK_LUATRANSFERFUN2DPROXY_H
#define TUVOK_LUATRANSFERFUN2DPROXY_H

// Necessary for setting up LuaStrictStack for TFPolygon.
#include "IO/TransferFunction2D.h"

namespace tuvok {

/// @brief classDescription
class LuaTransferFun2DProxy
{
public:
  LuaTransferFun2DProxy();
  virtual ~LuaTransferFun2DProxy();

  void bind(TransferFunction2D* tf);

  static LuaTransferFun2DProxy* luaConstruct() 
  {
    return new LuaTransferFun2DProxy();
  }
  static void defineLuaInterface(LuaClassRegistration<LuaTransferFun2DProxy>& reg,
                                 LuaTransferFun2DProxy* me,
                                 LuaScripting* ss);

  /// Transfer function pointer retrieval. 
  TransferFunction2D* get2DTransferFunction() {return m2DTrans;}

private:

  /// Proxies to split apart overloaded functions.
  /// @{
  bool proxyLoadWithSize(const std::wstring& file, const VECTOR2<size_t>& size);
  bool proxySave(const std::wstring& file);
  /// @}

  /// Update1DTrans proxy exists because IO does not understand Lua types
  /// (in this case, LuaClassInstance).
  void proxyUpdate1DTrans(LuaClassInstance tf1d);

  /// The following proxies exist because it isn't necessary to create a new
  /// LuaStrictStack template specialization because const we prepended onto
  /// VECTOR2<size_t>.
  VECTOR2<size_t> proxyGetSize();
  VECTOR2<size_t> proxyGetRenderSize();

  /// Class registration we received from defineLuaInterface.
  /// @todo Change to unique pointer.
  LuaClassRegistration<LuaTransferFun2DProxy>*  mReg;

  /// The 2D transfer function that we represent.
  TransferFunction2D*                           m2DTrans;

  LuaScripting*                                 mSS;
};

/// This template specialization converts what it means to be a swatch in C++
/// to what it means to be a swatch in Lua and vice versa.
template<>
class LuaStrictStack<const TFPolygon&>
{
public:

  typedef TFPolygon Type;

  static TFPolygon get(lua_State* L, int pos)
  {
    LuaStackRAII _a(L, 0, 0);

    TFPolygon ret;

    // There should be a table at 'pos', containing four numerical elements.
    luaL_checktype(L, pos, LUA_TTABLE);

    lua_pushstring(L, "radial");
    lua_gettable(L, pos);
    ret.bRadial = LuaStrictStack<bool>::get(L, lua_gettop(L));
    lua_pop(L, 1);

    lua_pushstring(L, "points");
    lua_gettable(L, pos);
    ret.pPoints = 
        LuaStrictStack<std::vector<FLOATVECTOR2>>::get(L, lua_gettop(L));
    lua_pop(L, 1);

    lua_pushstring(L, "gradCoords0");
    lua_gettable(L, pos);
    ret.pGradientCoords[0] = LuaStrictStack<FLOATVECTOR2>::get(L,lua_gettop(L));
    lua_pop(L, 1);

    lua_pushstring(L, "gradCoords1");
    lua_gettable(L, pos);
    ret.pGradientCoords[1] = LuaStrictStack<FLOATVECTOR2>::get(L,lua_gettop(L));
    lua_pop(L, 1);

    lua_pushstring(L, "gradStops");
    lua_gettable(L, pos);
    ret.pGradientStops =
        LuaStrictStack<std::vector<GradientStop>>::get(L,lua_gettop(L));

    return ret;
  }

  static void push(lua_State* L, TFPolygon in)
  {
    LuaStackRAII _a(L, 0, 1);

    lua_newtable(L);
    int tbl = lua_gettop(L);

    lua_pushstring(L, "radial");
    LuaStrictStack<bool>::push(L, in.bRadial);
    lua_settable(L, tbl);

    lua_pushstring(L, "points");
    LuaStrictStack<std::vector<FLOATVECTOR2>>::push(L, in.pPoints);
    lua_settable(L, tbl);

    lua_pushstring(L, "gradCoords0");
    LuaStrictStack<FLOATVECTOR2>::push(L, in.pGradientCoords[0]);
    lua_settable(L, tbl);

    lua_pushstring(L, "gradCoords1");
    LuaStrictStack<FLOATVECTOR2>::push(L, in.pGradientCoords[1]);
    lua_settable(L, tbl);

    lua_pushstring(L, "gradStops");
    LuaStrictStack<std::vector<GradientStop>>::push(L, in.pGradientStops);
    lua_settable(L, tbl);
  }

  static std::string getValStr(TFPolygon in)
  {
    std::ostringstream os;
    os << "{" << LuaStrictStack<bool>::getValStr(in.bRadial)
       << "," << LuaStrictStack<std::vector<FLOATVECTOR2>>::getValStr(
           in.pPoints)
       << "," << LuaStrictStack<FLOATVECTOR2>::getValStr(in.pGradientCoords[0])
       << "," << LuaStrictStack<FLOATVECTOR2>::getValStr(in.pGradientCoords[1])
       << "," << LuaStrictStack<std::vector<GradientStop>>::getValStr(
           in.pGradientStops)
       << "}";
    return os.str();
  }
  static std::string getTypeStr() { return "TFPolygon"; }
  static TFPolygon   getDefault() { return TFPolygon(); }
};

// Duplicate of the above code, with only const TFPolygon& changed to TFPolygon.
template<>
class LuaStrictStack<TFPolygon>
{
public:

  typedef TFPolygon Type;

  static TFPolygon get(lua_State* L, int pos)
  {
    LuaStackRAII _a(L, 0, 0);

    TFPolygon ret;

    // There should be a table at 'pos', containing four numerical elements.
    luaL_checktype(L, pos, LUA_TTABLE);

    lua_pushstring(L, "radial");
    lua_gettable(L, pos);
    ret.bRadial = LuaStrictStack<bool>::get(L, lua_gettop(L));
    lua_pop(L, 1);

    lua_pushstring(L, "points");
    lua_gettable(L, pos);
    ret.pPoints = 
        LuaStrictStack<std::vector<FLOATVECTOR2>>::get(L, lua_gettop(L));
    lua_pop(L, 1);

    lua_pushstring(L, "gradCoords0");
    lua_gettable(L, pos);
    ret.pGradientCoords[0] = LuaStrictStack<FLOATVECTOR2>::get(L,lua_gettop(L));
    lua_pop(L, 1);

    lua_pushstring(L, "gradCoords1");
    lua_gettable(L, pos);
    ret.pGradientCoords[1] = LuaStrictStack<FLOATVECTOR2>::get(L,lua_gettop(L));
    lua_pop(L, 1);

    lua_pushstring(L, "gradStops");
    lua_gettable(L, pos);
    ret.pGradientStops =
        LuaStrictStack<std::vector<GradientStop>>::get(L,lua_gettop(L));

    return ret;
  }

  static void push(lua_State* L, TFPolygon in)
  {
    LuaStackRAII _a(L, 0, 1);

    lua_newtable(L);
    int tbl = lua_gettop(L);

    lua_pushstring(L, "radial");
    LuaStrictStack<bool>::push(L, in.bRadial);
    lua_settable(L, tbl);

    lua_pushstring(L, "points");
    LuaStrictStack<std::vector<FLOATVECTOR2>>::push(L, in.pPoints);
    lua_settable(L, tbl);

    lua_pushstring(L, "gradCoords0");
    LuaStrictStack<FLOATVECTOR2>::push(L, in.pGradientCoords[0]);
    lua_settable(L, tbl);

    lua_pushstring(L, "gradCoords1");
    LuaStrictStack<FLOATVECTOR2>::push(L, in.pGradientCoords[1]);
    lua_settable(L, tbl);

    lua_pushstring(L, "gradStops");
    LuaStrictStack<std::vector<GradientStop>>::push(L, in.pGradientStops);
    lua_settable(L, tbl);
  }

  static std::string getValStr(TFPolygon in)
  {
    std::ostringstream os;
    os << "{" << LuaStrictStack<bool>::getValStr(in.bRadial)
       << "," << LuaStrictStack<std::vector<FLOATVECTOR2>>::getValStr(
           in.pPoints)
       << "," << LuaStrictStack<FLOATVECTOR2>::getValStr(in.pGradientCoords[0])
       << "," << LuaStrictStack<FLOATVECTOR2>::getValStr(in.pGradientCoords[1])
       << "," << LuaStrictStack<std::vector<GradientStop>>::getValStr(
           in.pGradientStops)
       << "}";
    return os.str();
  }
  static std::string getTypeStr() { return "TFPolygon"; }
  static TFPolygon   getDefault() { return TFPolygon(); }
};



} // namespace tuvok

#endif // TUVOK_LUATRANSFERFUN2DPROXY_H
