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
#include "../LuaFunBinding.h"

// Standard
#include "../../Basics/Vectors.h"
#include "../../Basics/Plane.h"
#include "../../StdTuvokDefines.h"
#include "../../Renderer/AbstrRenderer.h"
#include "../../Renderer/RenderRegion.h"
#include "../../Controller/MasterController.h"
#include "LuaDatasetProxy.h"

namespace tuvok
{

// TODO: Add metatable to vector types in order to get basic vector operations
//       to function inside of Lua (add, subtract, and multiply metamethods).


// All numeric types are converted to doubles inside Lua. Therefore there is
// no need to specialize on the type of vector.
template<typename T>
class LuaStrictStack<VECTOR4<T>>
{
public:
  typedef VECTOR4<T> Type;

  static VECTOR4<T> get(lua_State* L, int pos)
  {
    Type ret;

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
    os << "{" << in.x << ", " << in.y << ", " << in.z << ", " << in.w << "}";
    return os.str();
  }
  static std::string getTypeStr() { return "Vector4"; }
  static VECTOR4<T>  getDefault() { return VECTOR4<T>(); }
};

template<typename T>
class LuaStrictStack<const VECTOR4<T>& >
{
public:
  typedef VECTOR4<T> Type;

  static Type get(lua_State* L, int pos)
  { return LuaStrictStack<Type>::get(L, pos); }
  static void push(lua_State* L, Type in)
  { LuaStrictStack<Type>::push(L, in); }

  static std::string getValStr(Type in)
  { return LuaStrictStack<Type>::getValStr(in); }
  static std::string getTypeStr()
  { return LuaStrictStack<Type>::getTypeStr(); }
  static Type getDefault()
  { return LuaStrictStack<Type>::getDefault(); }
};

template<typename T>
class LuaStrictStack<VECTOR3<T>>
{
public:
  typedef VECTOR3<T> Type;

  static VECTOR3<T> get(lua_State* L, int pos)
  {
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
    os << "{" << in.x << ", " << in.y << ", " << in.z << "}";
    return os.str();
  }
  static std::string getTypeStr() { return "Vector3"; }
  static VECTOR3<T>  getDefault() { return VECTOR3<T>(); }
};

template<typename T>
class LuaStrictStack<const VECTOR3<T>& >
{
public:
  typedef VECTOR3<T> Type;

  static Type get(lua_State* L, int pos)
  { return LuaStrictStack<Type>::get(L, pos); }

  static void push(lua_State* L, Type in)
  { LuaStrictStack<Type>::push(L, in); }

  static std::string getValStr(Type in)
  { return LuaStrictStack<Type>::getValStr(in); }
  static std::string getTypeStr()
  { return LuaStrictStack<Type>::getTypeStr(); }
  static VECTOR3<T>  getDefault()
  { return LuaStrictStack<Type>::getDefault(); }
};

template<typename T>
class LuaStrictStack<VECTOR2<T>>
{
public:
  typedef VECTOR2<T> Type;

  static Type get(lua_State* L, int pos)
  {
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

  static void push(lua_State* L, const Type& in)
  {
    lua_newtable(L);
    int tbl = lua_gettop(L);

    lua_pushinteger(L, 1);
    lua_pushnumber(L, static_cast<lua_Number>(in.x));
    lua_settable(L, tbl);

    lua_pushinteger(L, 2);
    lua_pushnumber(L, static_cast<lua_Number>(in.y));
    lua_settable(L, tbl);
  }

  static std::string getValStr(const Type& in)
  {
    std::ostringstream os;
    os << "{" << in.x << ", " << in.y << "}";
    return os.str();
  }
  static std::string getTypeStr() { return "Vector2"; }
  static VECTOR2<T>  getDefault() { return VECTOR2<T>(); }
};

template<typename T>
class LuaStrictStack<const VECTOR2<T>& >
{
public:
  typedef VECTOR2<T> Type;

  static Type get(lua_State* L, int pos)
  { return LuaStrictStack<Type>::get(L, pos); }

  static void push(lua_State* L, const Type& in)
  { LuaStrictStack<Type>::push(L, in); }

  static std::string getValStr(const Type& in)
  { return LuaStrictStack<Type>::getValStr(in); }
  static std::string getTypeStr() 
  { return LuaStrictStack<Type>::getTypeStr(); }
  static Type getDefault() 
  { return LuaStrictStack<Type>::getDefault(); }
};


/// NOTE: We are choosing to store matrices in row-major order because
///       initialization of these matrices looks prettier in Lua:
///       M = { {0, 1, 0},
///             {1, 2, 1},
///             {0, 1, 0} }
///
/// Plus, M[1][2] is a natural analog of the mathematical notation M_12.

/// @todo Add metamethods so we can perform linear algebra with the normal
/// matrix/matrix and matrix/vector products inside of Lua.

template<typename T>
class LuaStrictStack<MATRIX2<T>>
{
public:
  typedef MATRIX2<T> Type;

  static MATRIX2<T> get(lua_State* L, int pos)
  {
    VECTOR2<T> rows[2];

    luaL_checktype(L, pos, LUA_TTABLE);

    lua_pushinteger(L, 1);
    lua_gettable(L, pos);
    rows[0] = LuaStrictStack<VECTOR2<T>>::get(L, lua_gettop(L));
    lua_pop(L, 1);

    lua_pushinteger(L, 2);
    lua_gettable(L, pos);
    rows[1] = LuaStrictStack<VECTOR2<T>>::get(L, lua_gettop(L));
    lua_pop(L, 1);

    return MATRIX2<T>(rows);
  }

  static void push(lua_State* L, MATRIX2<T> in)
  {
    lua_newtable(L);
    int tbl = lua_gettop(L);

    // Push rows of the matrix.
    lua_pushinteger(L, 1);
    LuaStrictStack<VECTOR2<T>>::push(L, VECTOR2<T>(in.m11, in.m12));
    lua_settable(L, tbl);

    lua_pushinteger(L, 2);
    LuaStrictStack<VECTOR2<T>>::push(L, VECTOR2<T>(in.m21, in.m22));
    lua_settable(L, tbl);
  }

  static std::string getValStr(MATRIX2<T> in)
  {
    std::ostringstream os;
    os << "{ \n  ";
    os << LuaStrictStack<VECTOR2<T>>::getValStr(VECTOR2<T>(in.m11, in.m12));
    os << ",\n";
    os << "  ";
    os << LuaStrictStack<VECTOR2<T>>::getValStr(VECTOR2<T>(in.m21, in.m22));
    os << " }";
    return os.str();
  }
  static std::string getTypeStr() { return "Matrix22"; }
  static MATRIX2<T>  getDefault() { return MATRIX2<T>(); }
};

template<typename T>
class LuaStrictStack<MATRIX3<T>>
{
public:
  typedef MATRIX3<T> Type;

  static MATRIX3<T> get(lua_State* L, int pos)
  {
    VECTOR3<T> rows[3];

    luaL_checktype(L, pos, LUA_TTABLE);

    lua_pushinteger(L, 1);
    lua_gettable(L, pos);
    rows[0] = LuaStrictStack<VECTOR3<T>>::get(L, lua_gettop(L));
    lua_pop(L, 1);

    lua_pushinteger(L, 2);
    lua_gettable(L, pos);
    rows[1] = LuaStrictStack<VECTOR3<T>>::get(L, lua_gettop(L));
    lua_pop(L, 1);

    lua_pushinteger(L, 3);
    lua_gettable(L, pos);
    rows[2] = LuaStrictStack<VECTOR3<T>>::get(L, lua_gettop(L));
    lua_pop(L, 1);

    return MATRIX3<T>(rows);
  }

  static void push(lua_State* L, MATRIX3<T> in)
  {
    lua_newtable(L);
    int tbl = lua_gettop(L);

    // Push rows of the matrix.
    lua_pushinteger(L, 1);
    LuaStrictStack<VECTOR3<T>>::push(L, VECTOR3<T>(in.m11, in.m12, in.m13));
    lua_settable(L, tbl);

    lua_pushinteger(L, 2);
    LuaStrictStack<VECTOR3<T>>::push(L, VECTOR3<T>(in.m21, in.m22, in.m23));
    lua_settable(L, tbl);

    lua_pushinteger(L, 3);
    LuaStrictStack<VECTOR3<T>>::push(L, VECTOR3<T>(in.m31, in.m32, in.m33));
    lua_settable(L, tbl);
  }

  static std::string getValStr(MATRIX3<T> in)
  {
    std::ostringstream os;
    os << "{ \n  ";
    os << LuaStrictStack<VECTOR3<T>>::getValStr(VECTOR3<T>(in.m11, in.m12, in.m13));
    os << ",\n";
    os << "  ";
    os << LuaStrictStack<VECTOR3<T>>::getValStr(VECTOR3<T>(in.m21, in.m22, in.m23));
    os << ",\n";
    os << "  ";
    os << LuaStrictStack<VECTOR3<T>>::getValStr(VECTOR3<T>(in.m31, in.m32, in.m33));
    os << " }";
    return os.str();
  }
  static std::string getTypeStr() { return "Matrix33"; }
  static MATRIX3<T>  getDefault() { return MATRIX3<T>(); }
};


template<typename T>
class LuaStrictStack<MATRIX4<T>>
{
public:
  typedef MATRIX4<T> Type;

  static MATRIX4<T> get(lua_State* L, int pos)
  {
    VECTOR4<T> rows[4];

    luaL_checktype(L, pos, LUA_TTABLE);

    lua_pushinteger(L, 1);
    lua_gettable(L, pos);
    rows[0] = LuaStrictStack<VECTOR4<T>>::get(L, lua_gettop(L));
    lua_pop(L, 1);

    lua_pushinteger(L, 2);
    lua_gettable(L, pos);
    rows[1] = LuaStrictStack<VECTOR4<T>>::get(L, lua_gettop(L));
    lua_pop(L, 1);

    lua_pushinteger(L, 3);
    lua_gettable(L, pos);
    rows[2] = LuaStrictStack<VECTOR4<T>>::get(L, lua_gettop(L));
    lua_pop(L, 1);

    lua_pushinteger(L, 4);
    lua_gettable(L, pos);
    rows[3] = LuaStrictStack<VECTOR4<T>>::get(L, lua_gettop(L));
    lua_pop(L, 1);

    return MATRIX4<T>(rows);
  }

  static void push(lua_State* L, MATRIX4<T> in)
  {
    lua_newtable(L);
    int tbl = lua_gettop(L);

    // Push rows of the matrix.
    lua_pushinteger(L, 1);
    LuaStrictStack<VECTOR4<T>>::push(L, VECTOR4<T>(in.m11, in.m12, in.m13, in.m14));
    lua_settable(L, tbl);

    lua_pushinteger(L, 2);
    LuaStrictStack<VECTOR4<T>>::push(L, VECTOR4<T>(in.m21, in.m22, in.m23, in.m24));
    lua_settable(L, tbl);

    lua_pushinteger(L, 3);
    LuaStrictStack<VECTOR4<T>>::push(L, VECTOR4<T>(in.m31, in.m32, in.m33, in.m34));
    lua_settable(L, tbl);

    lua_pushinteger(L, 4);
    LuaStrictStack<VECTOR4<T>>::push(L, VECTOR4<T>(in.m41, in.m42, in.m43, in.m44));
    lua_settable(L, tbl);
  }

  static std::string getValStr(MATRIX4<T> in)
  {
    std::ostringstream os;
    os << "{ \n  ";
    os << LuaStrictStack<VECTOR4<T>>::getValStr(VECTOR4<T>(in.m11, in.m12, in.m13, in.m14));
    os << ",\n";
    os << "  ";
    os << LuaStrictStack<VECTOR4<T>>::getValStr(VECTOR4<T>(in.m21, in.m22, in.m23, in.m24));
    os << ",\n";
    os << "  ";
    os << LuaStrictStack<VECTOR4<T>>::getValStr(VECTOR4<T>(in.m31, in.m32, in.m33, in.m34));
    os << ",\n";
    os << "  ";
    os << LuaStrictStack<VECTOR4<T>>::getValStr(VECTOR4<T>(in.m41, in.m42, in.m43, in.m44));
    os << " }";
    return os.str();
  }
  static std::string getTypeStr() { return "Matrix44"; }
  static MATRIX4<T>  getDefault() { return MATRIX4<T>(); }
};

template<typename T>
class LuaStrictStack<const MATRIX4<T>& >
{
public:
  typedef MATRIX4<T> Type;

  static Type get(lua_State* L, int pos)
  { return LuaStrictStack<Type>::get(L, pos); }
  static void push(lua_State* L, Type in)
  { LuaStrictStack<Type>::push(L, in); }

  static std::string getValStr(Type in)
  { return LuaStrictStack<Type>::getValStr(in); }
  static std::string getTypeStr()
  { return LuaStrictStack<Type>::getTypeStr(); }
  static MATRIX4<T>  getDefault()
  { return LuaStrictStack<Type>::getDefault(); }
};



/// @todo Write extended plane type and update RenderWindow.cpp to use this
///       instead of bypassing the LuaClassInstance.
template<>
class LuaStrictStack<ExtendedPlane>
{
public:
  typedef ExtendedPlane Type;

  static ExtendedPlane get(lua_State* L, int pos)
  {
    luaL_checktype(L, pos, LUA_TTABLE);

    lua_pushinteger(L, 1);
    lua_gettable(L, pos);
    VECTOR4<float> plane=LuaStrictStack<VECTOR4<float>>::get(L, lua_gettop(L));
    lua_pop(L, 1);

    lua_pushinteger(L, 2);
    lua_gettable(L, pos);
    FLOATMATRIX4 m1 =
        LuaStrictStack<FLOATMATRIX4>::get(L, lua_gettop(L));
    lua_pop(L, 1);

    lua_pushinteger(L, 3);
    lua_gettable(L, pos);
    FLOATMATRIX4 m2 =
        LuaStrictStack<FLOATMATRIX4>::get(L, lua_gettop(L));
    lua_pop(L, 1);

    return ExtendedPlane(m1, m2, plane);
  }

  static void push(lua_State* L, ExtendedPlane in)
  {
    lua_newtable(L);
    int tbl = lua_gettop(L);

    // Push rows of the matrix.
    lua_pushinteger(L, 1);
    LuaStrictStack<VECTOR4<float>>::push(L, VECTOR4<float>(in.Plane().x,
                                                           in.Plane().y,
                                                           in.Plane().z,
                                                           in.Plane().w));
    lua_settable(L, tbl);

    lua_pushinteger(L, 2);
    LuaStrictStack<FLOATMATRIX4>::push(L, FLOATMATRIX4(in.Mat1()));
    lua_settable(L, tbl);

    lua_pushinteger(L, 3);
    LuaStrictStack<FLOATMATRIX4>::push(L, FLOATMATRIX4(in.Mat2()));
    lua_settable(L, tbl);
  }

  static std::string getValStr(ExtendedPlane in)
  {
    std::ostringstream os;
    os << "{ ";
    os << LuaStrictStack<VECTOR4<float>>::getValStr(
        VECTOR4<float>(in.Plane()));
    os << " }";
    return os.str();
  }
  static std::string getTypeStr() { return "ExtendedPlane"; }
  static ExtendedPlane  getDefault() { return ExtendedPlane(); }
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
TUVOK_LUA_REGISTER_ENUM_TYPE(AbstrRenderer::ERenderMode)

TUVOK_LUA_REGISTER_ENUM_TYPE(Interpolant)

TUVOK_LUA_REGISTER_ENUM_TYPE(LuaDatasetProxy::DatasetType)

TUVOK_LUA_REGISTER_ENUM_TYPE(MasterController::EVolumeRendererType)

TUVOK_LUA_REGISTER_ENUM_TYPE(RenderRegion::EWindowMode)

#endif /* LUATUVOKSPECIFICTYPES_H_ */
