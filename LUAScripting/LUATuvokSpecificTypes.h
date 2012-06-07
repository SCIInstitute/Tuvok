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
  \brief  Exposes specific types found in Tuvok (such as the vector types) to
          the LuaScripting system. Do NOT include this header file if you are
          compiling the LuaScripting system outside of Tuvok.
*/

#ifndef TUVOK_LUATUVOKSPECIFICTYPES_H_
#define TUVOK_LUATUVOKSPECIFICTYPES_H_

#include "3rdParty/LUA/lua.hpp"

// LuaFunBinding must always come before our implementation, because we depend
// on the templates it has built.
#include "LuaFunBinding.h"

// Standard
#include "../Basics/Vectors.h"
#include "../StdTuvokDefines.h"
#include "../Renderer/AbstrRenderer.h"

namespace tuvok
{

// TODO: Add metatable to vector types in order to get basic vector operations
//       to function inside of Lua (add, subtract, and multiply metamethods).

// All numeric types are converted to doubles inside Lua. Therefore there is
// no need to specialize on the type of vector.
template<typename T>
class LuaStrictStack<VECTOR4<T> >
{
public:
  static VECTOR4<T> get(lua_State* L, int pos)
  {
    LuaStackRAII _a(L, 0);  ///< TODO: Remove once thoroughly tested.

    VECTOR4<T> ret;

    // There should be a table at 'pos', containing four numerical elements.
    luaL_checktype(L, pos, LUA_TTABLE);

    lua_pushinteger(L, 1);
    lua_gettable(L, pos);
    ret.x = static_cast<T>(luaL_checknumber(L, -1));
    lua_pop(L, 1);

    lua_pushinteger(L, 2);
    lua_gettable(L, pos);
    ret.y = static_cast<T>(luaL_checknumber(L, -1));
    lua_pop(L, 1);

    lua_pushinteger(L, 3);
    lua_gettable(L, pos);
    ret.z = static_cast<T>(luaL_checknumber(L, -1));
    lua_pop(L, 1);

    lua_pushinteger(L, 4);
    lua_gettable(L, pos);
    ret.w = static_cast<T>(luaL_checknumber(L, -1));
    lua_pop(L, 1);

    return ret;
  }

  static void push(lua_State* L, VECTOR4<T> in)
  {
    LuaStackRAII _a(L, 1);  ///< TODO: Remove once thoroughly tested.

    lua_newtable(L);
    int tbl = lua_gettop(L);

    lua_pushinteger(L, 1);
    lua_pushnumber(L, static_cast<lua_Number>(in.x));
    lua_settable(L, tbl);

    lua_pushinteger(L, 2);
    lua_pushnumber(L, static_cast<lua_Number>(in.y));
    lua_settable(L, tbl);

    lua_pushinteger(L, 3);
    lua_pushnumber(L, static_cast<lua_Number>(in.z));
    lua_settable(L, tbl);

    lua_pushinteger(L, 4);
    lua_pushnumber(L, static_cast<lua_Number>(in.w));
    lua_settable(L, tbl);
  }

  static std::string getValStr(VECTOR4<T> in)
  {
    std::ostringstream os;
    os << "<" << in.x << ", " << in.y << ", " << in.z << ", " << in.w << ">";
    return os.str();
  }
  static std::string getTypeStr() { return "Vector4"; }
  static VECTOR4<T>  getDefault() { return VECTOR4<T>(); }
};

template<typename T>
class LuaStrictStack<VECTOR3<T> >
{
public:
  static VECTOR3<T> get(lua_State* L, int pos)
  {
    LuaStackRAII _a(L, 0);  ///< TODO: Remove once thoroughly tested.

    VECTOR3<T> ret;

    // There should be a table at 'pos', containing four numerical elements.
    luaL_checktype(L, pos, LUA_TTABLE);

    lua_pushinteger(L, 1);
    lua_gettable(L, pos);
    ret.x = static_cast<T>(luaL_checknumber(L, -1));
    lua_pop(L, 1);

    lua_pushinteger(L, 2);
    lua_gettable(L, pos);
    ret.y = static_cast<T>(luaL_checknumber(L, -1));
    lua_pop(L, 1);

    lua_pushinteger(L, 3);
    lua_gettable(L, pos);
    ret.z = static_cast<T>(luaL_checknumber(L, -1));
    lua_pop(L, 1);

    return ret;
  }

  static void push(lua_State* L, VECTOR3<T> in)
  {
    LuaStackRAII _a(L, 1);  ///< TODO: Remove once thoroughly tested.

    lua_newtable(L);
    int tbl = lua_gettop(L);

    lua_pushinteger(L, 1);
    lua_pushnumber(L, static_cast<lua_Number>(in.x));
    lua_settable(L, tbl);

    lua_pushinteger(L, 2);
    lua_pushnumber(L, static_cast<lua_Number>(in.y));
    lua_settable(L, tbl);

    lua_pushinteger(L, 3);
    lua_pushnumber(L, static_cast<lua_Number>(in.z));
    lua_settable(L, tbl);
  }

  static std::string getValStr(VECTOR3<T> in)
  {
    std::ostringstream os;
    os << "<" << in.x << ", " << in.y << ", " << in.z << ">";
    return os.str();
  }
  static std::string getTypeStr() { return "Vector3"; }
  static VECTOR3<T>  getDefault() { return VECTOR3<T>(); }
};

template<typename T>
class LuaStrictStack<VECTOR2<T> >
{
public:
  static VECTOR2<T> get(lua_State* L, int pos)
  {
    LuaStackRAII _a(L, 0);  ///< TODO: Remove once thoroughly tested.

    VECTOR2<T> ret;

    // There should be a table at 'pos', containing four numerical elements.
    luaL_checktype(L, pos, LUA_TTABLE);

    lua_pushinteger(L, 1);
    lua_gettable(L, pos);
    ret.x = static_cast<T>(luaL_checknumber(L, -1));
    lua_pop(L, 1);

    lua_pushinteger(L, 2);
    lua_gettable(L, pos);
    ret.y = static_cast<T>(luaL_checknumber(L, -1));
    lua_pop(L, 1);

    return ret;
  }

  static void push(lua_State* L, VECTOR2<T> in)
  {
    LuaStackRAII _a(L, 1);  ///< TODO: Remove once thoroughly tested.

    lua_newtable(L);
    int tbl = lua_gettop(L);

    lua_pushinteger(L, 1);
    lua_pushnumber(L, static_cast<lua_Number>(in.x));
    lua_settable(L, tbl);

    lua_pushinteger(L, 2);
    lua_pushnumber(L, static_cast<lua_Number>(in.y));
    lua_settable(L, tbl);
  }

  static std::string getValStr(VECTOR2<T> in)
  {
    std::ostringstream os;
    os << "<" << in.x << ", " << in.y << ">";
    return os.str();
  }
  static std::string getTypeStr() { return "Vector2"; }
  static VECTOR2<T>  getDefault() { return VECTOR2<T>(); }
};

} // namespace tuvok

// Register standard Tuvok enumerations. These enumerations declare their own
// namespace, so they must be outside of the tuvok namespace.
// TODO: Embelish on this standard enumeration. Should be able to indicate
//       what values are present in the renderer.
TUVOK_LUA_REGISTER_ENUM_TYPE(AbstrRenderer::ERendererType)
TUVOK_LUA_REGISTER_ENUM_TYPE(AbstrRenderer::ERendererTarget)
TUVOK_LUA_REGISTER_ENUM_TYPE(AbstrRenderer::EStereoMode)
TUVOK_LUA_REGISTER_ENUM_TYPE(AbstrRenderer::EBlendPrecision)
TUVOK_LUA_REGISTER_ENUM_TYPE(AbstrRenderer::ScalingMethod)

#endif /* LUATUVOKSPECIFICTYPES_H_ */
