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

//       Another possibility is just storing a light user data pointer to the
//       appropriate 'getValStr', and 'get' functions. Then call:
//       getValStr(get(L, pos)).

/// Utility lua math registration class.
class LuaMathFunctions
{
public:
  // Called from MasterController to register misc mathematical functions.
  static void registerMathFunctions(std::shared_ptr<LuaScripting> ss); 

  //
  // Utility functions. 
  //

  // Returns true if the metatable of the object at stack position 'object'
  // matches the metatable given by the stack position 'mt'.
  static bool isOfType(lua_State* L, int object, int mt);

  // Called from getMT in LuaStrictStack<Matrix4<T>>
  static void buildMatrix4Metatable(lua_State* L, int mtPos);
};

// All numeric types are converted to doubles inside Lua. Therefore there is
// no need to specialize on the type of vector.
template<typename T>
class LuaStrictStack<VECTOR4<T>>
{
public:
  typedef VECTOR4<T> Type;

  static Type get(lua_State* L, int pos)
  {
    Type ret;

    // There should be a table at 'pos', containing four numerical elements.
    luaL_checktype(L, pos, LUA_TTABLE);

    // Check the metatable.
    if (isOurType(L, pos) == false)
      throw LuaError("Attempting to convert a vector type that is missing its "
                     "metatable.");

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

    lua_pushinteger(L, 3);
    lua_pushnumber(L, static_cast<lua_Number>(in.z));
    lua_settable(L, tbl);

    lua_pushinteger(L, 4);
    lua_pushnumber(L, static_cast<lua_Number>(in.w));
    lua_settable(L, tbl);

    // Associate metatable. lua_setmetatable will pop off the metatable.
    getMT(L);
    lua_setmetatable(L, tbl);
  }

  static std::string getValStr(const Type& in)
  {
    std::ostringstream os;
    os << "{" << in.x << ", " << in.y << ", " << in.z << ", " << in.w << "}";
    return os.str();
  }
  static std::string getTypeStr() { return "Vector4"; }
  static Type        getDefault() { return Type(); }

  // Used to determine if the value at stack location 'index' is of 'our type'.
  static bool isOurType(lua_State* L, int index)
  {
    LuaStackRAII _a(L, 0, 0);

    getMT(L);
    bool ret = LuaMathFunctions::isOfType(L, index, lua_gettop(L));
    lua_pop(L, 1);
    return ret;
  }

  /// C function called from Lua to obtain a textual description of the data
  /// in this object.
  static int getLuaValStr(lua_State* L)
  {
    LuaStackRAII _a(L, 0, 1);

    // The user should have handed us a Lua value of this type.
    // Check the metatable of the type the user handed us to ensure we are
    // dealing with the same types.
    if (isOurType(L, lua_gettop(L)))
    {
      LuaStrictStack<VECTOR4<lua_Number>>::Type val = 
          LuaStrictStack<VECTOR4<lua_Number>>::get(L, lua_gettop(L));
      lua_pushstring(L, LuaStrictStack<VECTOR4<lua_Number>>::
                     getValStr(val).c_str());
    }
    else
    {
      lua_pushstring(L, "Cannot describe type; invalid type passed into "
                     "getLuaValStr.");
    }

    return 1; // Returning 1 result, the textual description of this object.
  }

  enum MUL_SEMANTIC
  {
    SCALAR_PRODUCT,
    VECTOR_SCALE_PRODUCT, // I'm not sure what this product should be named.
                          // It is essentially taking the first vector and 
                          // constructing a 4x4 matrix whose diagonal entries
                          // are the entries from the vector, then multiplying
                          // the second vector by this matrix. So the first
                          // vector could be seen as scaling the second (or
                          // vice versa).
  };

  static int multiplyMetamethod(lua_State* L)
  {
    LuaStackRAII _a(L, 0, 1);
    // Should only have two values on the stack, one at stack position 1 and
    // the other at stack position 2.
    MUL_SEMANTIC semantic = SCALAR_PRODUCT;

    lua_Number scalar = 0.0;
    LuaStrictStack<VECTOR4<lua_Number>>::Type v1;
    LuaStrictStack<VECTOR4<lua_Number>>::Type v2;

    // The first thing we need to do is determine the types of the parameters.
    if (isOurType(L, 1))
    {
      v1 = LuaStrictStack<VECTOR4<lua_Number>>::get(L, 1);
      if (lua_isnumber(L, 2))   // Is the second parameter a scalar?
      {
        // Perform a scalar multiplication.
        scalar = lua_tonumber(L, 2);
      }
      else if (isOurType(L, 2)) // Is the second parameter another vector?
      {
        semantic = VECTOR_SCALE_PRODUCT;
        v2 = LuaStrictStack<VECTOR4<lua_Number>>::get(L, 2);
      }
      else
      {
        throw LuaError("Unable to perform multiplication. Incompatible "
                       "arguments (vector handler 1)");
      }
    }
    else
    {
      // Must be scalar * vector multiplication. 
      // However, Matrix * vector multiplication would have gotten caught by 
      // the Matrix's metatable.

      // Check to make sure the value is a scalar...
      if (lua_isnumber(L, 1))   // Is the first parameter a scalar?
      {
        scalar = lua_tonumber(L, 1);
        if (isOurType(L, 2))
        {
          v1 = LuaStrictStack<VECTOR4<lua_Number>>::get(L, 2);
        }
        else
        {
          throw LuaError("Unable to perform multiplication. Incompatible "
                         "arguments (vector handler 2).");
        }
      }
      else
      {
        throw LuaError("Unable to perform multiplication. Incompatible "
                       "arguments (vector handler 3).");
      }
    }

    // We don't cover matrix * vector multiplication, since that will be covered
    // by the matrice's meta methods.
    switch (semantic)
    {
      case SCALAR_PRODUCT:
        LuaStrictStack<VECTOR4<lua_Number>>::push(
            L, static_cast<lua_Number>(scalar) * v1);
        break;

      case VECTOR_SCALE_PRODUCT:
        LuaStrictStack<VECTOR4<lua_Number>>::push(L, v1 * v2);
        break;
    }

    return 1; // Return the result of the multiplication.
  }

  static int additionMetamethod(lua_State* L)
  {
    // There is only one possible way to implement addition.
    if (isOurType(L, 1) == false) 
      throw LuaError("Unable to perform addition. Incompatible "
                     "arguments (expecting two vectors 1).");

    if (isOurType(L, 2) == false)
      throw LuaError("Unable to perform addition. Incompatible "
                     "arguments (expecting two vectors 2).");
    
    LuaStrictStack<VECTOR4<lua_Number>>::Type v1 = 
        LuaStrictStack<VECTOR4<lua_Number>>::get(L, 1);
    LuaStrictStack<VECTOR4<lua_Number>>::Type v2 = 
        LuaStrictStack<VECTOR4<lua_Number>>::get(L, 2);

    LuaStrictStack<VECTOR4<lua_Number>>::push(L, v1 + v2);

    return 1;
  }

  static int subtractionMetamethod(lua_State* L)
  {
    // This really is just addition with a unary negation...
    if (isOurType(L, 1) == false)
      throw LuaError("Unable to perform subtraction. Incompatible "
                     "arguments (expecting two vectors 1).");

    if (isOurType(L, 2) == false)
      throw LuaError("Unable to perform subtraction. Incompatible "
                     "arguments (expecting two vectors 1).");

    LuaStrictStack<VECTOR4<lua_Number>>::Type v1 = 
        LuaStrictStack<VECTOR4<lua_Number>>::get(L, 1);
    LuaStrictStack<VECTOR4<lua_Number>>::Type v2 = 
        LuaStrictStack<VECTOR4<lua_Number>>::get(L, 2);

    LuaStrictStack<VECTOR4<lua_Number>>::push(L, v1 - v2);

    return 1;
  }

  static int unaryNegationMetamethod(lua_State* L)
  {
    if (isOurType(L, 1) == false)
      throw LuaError("Unable to perform unary negation. Incompatible "
                     "argument (expecting a single vector).");

    LuaStrictStack<VECTOR4<lua_Number>>::Type v1 = 
        LuaStrictStack<VECTOR4<lua_Number>>::get(L, 1);

    LuaStrictStack<VECTOR4<lua_Number>>::push(L, -v1);

    return 1;
  }

  // Retrieves the metatable for this type.
  // The metatable is stored in the Lua registry and only one metatable is
  // generated for this type, reducing the overhead of the type.
  static void getMT(lua_State* L)
  {
    LuaStackRAII _a(L, 0, 1);

    if (luaL_newmetatable(L, getTypeStr().c_str()) == 1)
    {
      // Metatable does not already exist in the registry -- populate it.
      int mt = lua_gettop(L);
      
      // Push the getLuaValStr luaCFunction.
      lua_pushcfunction(L, &LuaStrictStack<VECTOR4<lua_Number>>::getLuaValStr);
      lua_setfield(L, mt, TUVOK_LUA_MT_TYPE_TO_STR_FUN);

      lua_pushcfunction(L, &LuaStrictStack<VECTOR4<lua_Number>>::multiplyMetamethod);
      lua_setfield(L, mt, "__mul");

      lua_pushcfunction(L, &LuaStrictStack<VECTOR4<lua_Number>>::additionMetamethod);
      lua_setfield(L, mt, "__add");

      lua_pushcfunction(L, &LuaStrictStack<VECTOR4<lua_Number>>::subtractionMetamethod);
      lua_setfield(L, mt, "__sub");
      
      lua_pushcfunction(L, &LuaStrictStack<VECTOR4<lua_Number>>::unaryNegationMetamethod);
      lua_setfield(L, mt, "__unm");

      /// @todo Move to cpp file and add (%) as cross product, and (^) as dot
      ///       product.
      /// Multiply operator should not perform a dot product, but a component
      /// wise product, to keep in-line with current vector4 operation.

      // Add __index metamethod to add various vector functions.
    }
    // If luaL_newmetatable returns 0, then there already exists a MT with this
    // type name: http://www.lua.org/manual/5.2/manual.html#luaL_newmetatable.
    // Leave it on the top of the stack.
  }
};

template<typename T>
class LuaStrictStack<const VECTOR4<T>& >
{
public:
  typedef VECTOR4<T> Type;

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

template<typename T>
class LuaStrictStack<VECTOR3<T>>
{
public:
  typedef VECTOR3<T> Type;

  static Type get(lua_State* L, int pos)
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

    lua_pushinteger(L, 3);
    lua_pushnumber(L, static_cast<lua_Number>(in.z));
    lua_settable(L, tbl);
  }

  static std::string getValStr(const Type& in)
  {
    std::ostringstream os;
    os << "{" << in.x << ", " << in.y << ", " << in.z << "}";
    return os.str();
  }
  static std::string getTypeStr() { return "Vector3"; }
  static Type        getDefault() { return Type(); }
};

template<typename T>
class LuaStrictStack<const VECTOR3<T>& >
{
public:
  typedef VECTOR3<T> Type;

  static Type get(lua_State* L, int pos)
  { return LuaStrictStack<Type>::get(L, pos); }

  static void push(lua_State* L, const Type& in)
  { LuaStrictStack<Type>::push(L, in); }

  static std::string getValStr(const Type& in)
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
  static Type        getDefault() { return Type(); }
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

  static Type get(lua_State* L, int pos)
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

  static void push(lua_State* L, const Type& in)
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

  static std::string getValStr(const Type& in)
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
  static Type        getDefault() { return Type(); }
};

template<typename T>
class LuaStrictStack<MATRIX3<T>>
{
public:
  typedef MATRIX3<T> Type;

  static Type get(lua_State* L, int pos)
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

  static void push(lua_State* L, const Type& in)
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

  static std::string getValStr(const Type& in)
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
  static Type        getDefault() { return Type(); }
};


template<typename T>
class LuaStrictStack<MATRIX4<T>>
{
public:
  typedef MATRIX4<T> Type;

  static Type get(lua_State* L, int pos)
  {
    VECTOR4<T> rows[4];

    luaL_checktype(L, pos, LUA_TTABLE);

    // Check the metatable.
    if (isOurType(L, pos) == false)
      throw LuaError("Attempting to convert a vector type that is missing its "
                     "metatable.");

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

  static void push(lua_State* L, const Type& in)
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

    // Associate metatable. lua_setmetatable will pop off the metatable.
    getMT(L);
    lua_setmetatable(L, tbl);
  }

  static std::string getValStr(const Type& in)
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
  static Type        getDefault() { return Type(); }

  // Used to determine if the value at stack location 'index' is of 'our type'.
  static bool isOurType(lua_State* L, int index)
  {
    LuaStackRAII _a(L, 0, 0);

    getMT(L);
    bool ret = LuaMathFunctions::isOfType(L, index, lua_gettop(L));
    lua_pop(L, 1);
    return ret;
  }

  /// C function called from Lua to obtain a textual description of the data
  /// in this object.
  static int getLuaValStr(lua_State* L)
  {
    LuaStackRAII _a(L, 0, 1);

    // The user should have handed us a Lua value of this type.
    // Check the metatable of the type the user handed us to ensure we are
    // dealing with the same types.
    if (isOurType(L, lua_gettop(L)))
    {
      LuaStrictStack<MATRIX4<lua_Number>>::Type val = 
          LuaStrictStack<MATRIX4<lua_Number>>::get(L, lua_gettop(L));
      lua_pushstring(L, LuaStrictStack<MATRIX4<lua_Number>>::
                     getValStr(val).c_str());
    }
    else
    {
      lua_pushstring(L, "Cannot describe type; invalid type passed into "
                     "getLuaValStr.");
    }

    return 1; // Returning 1 result, the textual description of this object.
  }

  enum MUL_SEMANTIC
  {
    SCALAR_PRODUCT,
    VECTOR_PRODUCT,
    MATRIX_PRODUCT,
  };

  static int multiplyMetamethod(lua_State* L)
  {
    LuaStackRAII _a(L, 0, 1);
    // Should only have two values on the stack, one at stack position 1 and
    // the other at stack position 2.
    MUL_SEMANTIC semantic = SCALAR_PRODUCT;

    lua_Number scalar = 0.0;
    LuaStrictStack<VECTOR4<lua_Number>>::Type v;
    LuaStrictStack<MATRIX4<lua_Number>>::Type m1;
    LuaStrictStack<MATRIX4<lua_Number>>::Type m2;

    // The first thing we need to do is determine the types of the parameters.
    if (isOurType(L, 1))
    {
      m1 = LuaStrictStack<MATRIX4<lua_Number>>::get(L, 1);
      if (lua_isnumber(L, 2))   // Is the second parameter a scalar?
      {
        // Perform a scalar multiplication.
        scalar = lua_tonumber(L, 2);
      }
      else if (LuaStrictStack<VECTOR4<lua_Number>>::isOurType(L, 2)) // Is the second parameter a vector?
      {
        semantic = VECTOR_PRODUCT;
        v = LuaStrictStack<VECTOR4<lua_Number>>::get(L, 2);
      }
      else if (isOurType(L, 2)) // Is the third parameter a matrix?
      {
        semantic = MATRIX_PRODUCT;
        m2 = LuaStrictStack<MATRIX4<lua_Number>>::get(L, 2);
      }
      else
      {
        throw LuaError("Unable to perform matrix multiplication. Incompatible "
                       "arguments (1)");
      }
    }
    else
    {
      // MUST be a scalar. The first parameter cannot be a valid matrix, which
      // is the only other valid multiplication type.
      if (lua_isnumber(L, 1))   // Is the first parameter a scalar?
      {
        scalar = lua_tonumber(L, 1);
        if (isOurType(L, 2))
        {
          m1 = LuaStrictStack<MATRIX4<lua_Number>>::get(L, 2);
        }
        else if (LuaStrictStack<VECTOR4<lua_Number>>::isOurType(L, 2)) // Is the second parameter a vector?
        {
          throw LuaError("Attempting to multiply 4x1 * 4x4. Multiplication not "
                         "defined.");
        }
        else
        {
          throw LuaError("Unable to perform multiplication. Incompatible "
                         "arguments (2).");
        }
      }
      else
      {
        throw LuaError("Unable to perform multiplication. Incompatible "
                       "arguments (3).");
      }
    }

    // We don't cover matrix * vector multiplication, since that will be covered
    // by the matrice's meta methods.
    switch (semantic)
    {
      case SCALAR_PRODUCT:
        LuaStrictStack<MATRIX4<lua_Number>>::push(L, m1 * scalar);
        break;

      case VECTOR_PRODUCT:
        LuaStrictStack<VECTOR4<lua_Number>>::push(L, m1 * v);
        break;

      case MATRIX_PRODUCT:
        LuaStrictStack<MATRIX4<lua_Number>>::push(L, m1 * m2);
        break;
    }

    return 1; // Return the result of the multiplication.
  }

  // Retrieves the metatable for this type.
  // The metatable is stored in the Lua registry and only one metatable is
  // generated for this type, reducing the overhead of the type.
  static void getMT(lua_State* L)
  {
    LuaStackRAII _a(L, 0, 1);

    if (luaL_newmetatable(
            L, LuaStrictStack<MATRIX4<lua_Number>>::getTypeStr().c_str()) == 1)
    {
      // Build the metatable
      LuaMathFunctions::buildMatrix4Metatable(L, lua_gettop(L));
    }
  }
};

template<typename T>
class LuaStrictStack<const MATRIX4<T>& >
{
public:
  typedef MATRIX4<T> Type;

  static Type get(lua_State* L, int pos)
  { return LuaStrictStack<Type>::get(L, pos); }
  static void push(lua_State* L, const Type& in)
  { LuaStrictStack<Type>::push(L, in); }

  static std::string getValStr(const Type& in)
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

  static Type get(lua_State* L, int pos)
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

  static void push(lua_State* L, const Type& in)
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

  static std::string getValStr(const Type& in)
  {
    std::ostringstream os;
    os << "{ ";
    os << LuaStrictStack<VECTOR4<float>>::getValStr(
        VECTOR4<float>(in.Plane()));
    os << " }";
    return os.str();
  }
  static std::string getTypeStr() { return "ExtendedPlane"; }
  static Type        getDefault() { return Type(); }
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
