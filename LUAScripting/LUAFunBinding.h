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
 \file    LUAFunBinding.h
 \author  James Hughes
          SCI Institute
          University of Utah
 \date    Mar 21, 2012
 \brief   Auxiliary templates used to implement 1-1 function binding in LUA.
 */


/// TODO: Look into rewriting using variadic templates when we support C++11.

/// **** INSTRUCTIONS ****
/// When adding new parameters below, there are multiple places you need to
/// make changes.
/// 1) Change the member function and static function templates to reflect the
///    new number of parameters.
/// 2) Update LUAC_MAX_NUM_PARAMS below. This is only used to ensure we don't
///    exceed our stack space in Lua.
/// 3) Add additional execution functions to LuaScripting (exec, cexec and their
///    associated ret functions).


#ifndef TUVOK_LUAFUNBINDING_H_
#define TUVOK_LUAFUNBINDING_H_

#include <iomanip>
#include <typeinfo>
#include <sstream>

#include "LUAClassInstance.h"
#include "LUAStackRAII.h"

// Uncomment TUVOK_DEBUG_LUA_USE_RTTI_CHECKS to check types of function calls
// made through lua at run time.
// (will be especially useful to debug shared_ptr type issues)
//
// I haven't used RTTI up to this point, and have relied on templates instead,
// so I don't want to make it a requirement now.
#define TUVOK_DEBUG_LUA_USE_RTTI_CHECKS


#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
#include <typeinfo>
#endif


namespace tuvok
{

//==============================================================
//
// LUA PARAM GETTER/PUSHER (we do NOT pop off of the LUA stack)
//
//==============================================================

// LUA strict type stack
// This template enforces strict type compliance while converting types on the
// LUA stack.

template<typename T>
class LuaStrictStack
{
public:
  // Intentionally left unimplemented to generate compiler errors if this
  // template is chosen.
  static T get(lua_State* L, int pos);
  static void push(lua_State* L, T data);
  static std::string getValStr(T in);
  static std::string getTypesStr();
  static T getDefault();
};

// TODO: 	Shared pointers. Need to decide if we even want to support these.
//			If we do want to support these, what do these types mean inside of LUA?

// Specializations (supported parameter/return types)

template<>
class LuaStrictStack<void>
{
public:
  // All functions except getTypeStr and push don't do anything since none of
  // these functions make sense in the context of 'void'.
  // getTypeStr is only called when building the return type of function
  // signatures.
  static int get(lua_State* L, int pos);
  static void push(lua_State* L, int in);

  static std::string getValStr(int in);
  static std::string getTypeStr() { return "void"; }

  static int         getDefault();
};

template<>
class LuaStrictStack<int>
{
public:
  static int get(lua_State* L, int pos)
  {
    return luaL_checkint(L, pos);
  }

  static void push(lua_State* L, int in)
  {
    lua_pushinteger(L, in);
  }

  static std::string getValStr(int in)
  {
    std::ostringstream os;
    os << in;
    return os.str();
  }
  static std::string getTypeStr() { return "int"; }
  static int         getDefault() { return 0; }
};

template<>
class LuaStrictStack<bool>
{
public:
  static bool get(lua_State* L, int pos)
  {
    luaL_checktype(L, pos, LUA_TBOOLEAN);
    int retVal = lua_toboolean(L, pos);
    return (retVal != 0) ? true : false;
  }

  static void push(lua_State* L, bool in)
  {
    lua_pushboolean(L, in ? 1 : 0);
  }

  static std::string getValStr(bool in)
  {
    std::ostringstream os;
    os << std::boolalpha << in;
    return os.str();
  }
  static std::string getTypeStr() { return "bool"; }
  static bool        getDefault() { return false; }
};

template<>
class LuaStrictStack<float>
{
public:
  static float get(lua_State* L, int pos)
  {
    return static_cast<float>(luaL_checknumber(L, pos));
  }

  static void push(lua_State* L, float in)
  {
    lua_pushnumber(L, static_cast<lua_Number>(in));
  }

  static std::string getValStr(float in)
  {
    std::ostringstream os;
    os << std::setprecision(2) << in;
    return os.str();
  }
  static std::string getTypeStr() { return "float"; }
  static float       getDefault() { return 0.0f; }
};

template<>
class LuaStrictStack<double>
{
public:
  static double get(lua_State* L, int pos)
  {
    return static_cast<double>(luaL_checknumber(L, pos));
  }

  static void push(lua_State* L, double in)
  {
    lua_pushnumber(L, static_cast<lua_Number>(in));
  }

  static std::string getValStr(double in)
  {
    std::ostringstream os;
    os << std::setprecision(4) << in;
    return os.str();
  }
  static std::string getTypeStr() { return "double"; }
  static double      getDefault() { return 0.0; }
};

template<>
class LuaStrictStack<const char *>
{
public:
  static const char* get(lua_State* L, int pos)
  {
    return luaL_checkstring(L, pos);
  }

  static void push(lua_State* L, const char* in)
  {
    lua_pushstring(L, in);
  }

  static std::string getValStr(const char* in)
  {
    std::ostringstream os;
    os << "'" << in << "'";
    return os.str();
  }
  static std::string getTypeStr() { return "string"; }
  static std::string getDefault() { return ""; }
};

template<>
class LuaStrictStack<std::string>
{
public:
  static std::string get(lua_State* L, int pos)
  {
    return luaL_checkstring(L, pos);
  }

  static void push(lua_State* L, std::string in)
  {
    lua_pushstring(L, in.c_str());
  }

  static std::string getValStr(std::string in)
  {
    std::ostringstream os;
    os << "'" << in << "'";
    return os.str();
  }
  static std::string getTypeStr() { return "string"; }
  static std::string getDefault() { return ""; }
};

template<>
class LuaStrictStack<std::string&>
{
public:
  static std::string get(lua_State* L, int pos)
  {
    return luaL_checkstring(L, pos);
  }

  static void push(lua_State* L, std::string& in)
  {
    lua_pushstring(L, in.c_str());
  }

  static std::string getValStr(std::string& in)
  {
    std::ostringstream os;
    os << "'" << in << "'";
    return os.str();
  }
  static std::string getTypeStr() { return "string"; }
  static std::string getDefault() { return ""; }
};

template<>
class LuaStrictStack<const std::string&>
{
public:
  static std::string get(lua_State* L, int pos)
  {
    return luaL_checkstring(L, pos);
  }

  static void push(lua_State* L, const std::string& in)
  {
    lua_pushstring(L, in.c_str());
  }

  static std::string getValStr(const std::string& in)
  {
    std::ostringstream os;
    os << "'" << in << "'";
    return os.str();
  }
  static std::string getTypeStr() { return "string"; }
  static std::string getDefault() { return ""; }
};

template<>
class LuaStrictStack<LuaClassInstance>
{
public:
  static LuaClassInstance get(lua_State* L, int pos)
  {
    LuaStackRAII _a(L, 0);

    // If the class that was passed to us didn't exist (nil) then we will
    // ignore the attempted retrieval, and return the default instance ID.
    // This allows out of order deletion (ignoring deletions of already
    // deleted classes).
    if (lua_isnil(L, pos))
    {
      return LuaClassInstance(LuaClassInstance::DEFAULT_INSTANCE_ID);
    }

    lua_getfield(L, pos, "_DefaultInstance_");
    if (lua_isnil(L, -1))
    {
      lua_pop(L, 1);

      // Grab the metatable of the table at pos and extract global ID.
      lua_getmetatable(L, pos);
      lua_getfield(L, -1, LuaClassInstance::MD_GLOBAL_INSTANCE_ID);
      int globalID = luaL_checkinteger(L, -1);
      lua_pop(L, 2);
      return LuaClassInstance(globalID);
    }
    else
    {
      lua_pop(L, 1);
      return LuaClassInstance(LuaClassInstance::DEFAULT_INSTANCE_ID);
    }
  }

  static void push(lua_State* L, LuaClassInstance in)
  {
    LuaStackRAII _a(L, 1);

    // Lookup the instance table in the global instance table based on the
    // instance ID.
    // TODO: This can be done more efficiently by parsing and walking the
    // tables ourselves.
    if (in.getGlobalInstID() != LuaClassInstance::DEFAULT_INSTANCE_ID)
    {
      std::ostringstream os;
      os << "return " << LuaClassInstance::CLASS_INSTANCE_TABLE << "."
         << LuaClassInstance::CLASS_INSTANCE_PREFIX << in.getGlobalInstID();
      luaL_dostring(L, os.str().c_str()); // Return the class instance.

      // Interesting corner case: If the class instance has already been
      // deleted, luaL_dostring will return nil, and result in us deleting
      // elements from our last exec table.
      // Since deleteClass has a null undo function, we are safe doing this.
      // deleteClass will be the only function that runs into this corner case.
      if (lua_isnil(L, -1))
      {
        lua_pop(L, 1);

        // Empty table.
        lua_newtable(L);
        lua_pushboolean(L, 1);
        lua_setfield(L, -2, "_DefaultInstance_");
      }
    }
    else
    {
      // Empty table.
      lua_newtable(L);
      lua_pushboolean(L, 1);
      lua_setfield(L, -2, "_DefaultInstance_");
    }
  }

  static std::string getValStr(LuaClassInstance in)
  {
    std::ostringstream os;
    os << LuaClassInstance::CLASS_INSTANCE_TABLE << "."
       << LuaClassInstance::CLASS_INSTANCE_PREFIX << in.getGlobalInstID();
    return os.str();
  }
  static std::string getTypeStr() { return "LuaClass"; }
  static LuaClassInstance getDefault() { return LuaClassInstance(-1); }
};

// Shared pointer type to allow arbitrary pointers to be passed into the system.
// Be careful when using shared pointers with the LuaScripting class.
// The LuaScripting class will not be destroyed until the class' provenance
// record is cleared in this particular case. This is because a shared pointer
// reference to LuaScripting will be stored inside of the provenance system.
template <typename T>
class LuaStrictStack<std::tr1::shared_ptr<T> >
{
public:
  static std::tr1::shared_ptr<T> get(lua_State* L, int pos)
  {
    std::tr1::shared_ptr<T>* ptr =
        reinterpret_cast<std::tr1::shared_ptr<T>* >(lua_touserdata(L, pos));
    return *ptr;
  }

  static int gc(lua_State* L)
  {
    // Explicitly call the shared pointer's destructor.
    std::tr1::shared_ptr<T>* ptr =
        reinterpret_cast<std::tr1::shared_ptr<T>* >(lua_touserdata(L, 1));
    (*ptr).~shared_ptr();
    return 0;
  }

  static void push(lua_State* L, std::tr1::shared_ptr<T> in)
  {
    // Allocate space for a shared pointer.
    void* spData = lua_newuserdata(L, sizeof(std::tr1::shared_ptr<T>));
    new(spData) std::tr1::shared_ptr<T>(in);

    // Setup metatable for the shared pointer to ensure it is dereferenced
    // when the lua instance is destroyed. We need to explicitly call the
    // destructor of the user data.
    lua_newtable(L);
    lua_pushcfunction(L, gc);
    lua_setfield(L, -2, "__gc");
    lua_setmetatable(L, -2);
  }

  static std::string getValStr(std::tr1::shared_ptr<T>)
  {
    std::ostringstream os;
    os << "SharedPointer";
    return os.str();
  }
  static std::string getTypeStr() { return "shared_ptr"; }
  static std::tr1::shared_ptr<T> getDefault()
  {
    return std::tr1::shared_ptr<T>();
  }
};

// TODO:  If boost detected, add boost shared_ptr.

// TODO:	Add support for std::vector and std::map, both to be implemented as
//        tables in LUA. std::vector would be efficiently implemented in LUA.
//        In LUA, it would be stored internally as an array instead of
//        key/value pairs in a hash table.
//			  See http://www.lua.org/doc/hopl.pdf -- page 2, para 2. See ref 31.
//			  Consider support for 3D and 4D graphics vectors.

//========================
//
// RUN TIME TYPE CHECKING
//
//========================

#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS

//
// From the C++ Standard [ISO/IEC 14882:1998(E)]
// to ISO N3337 (post-C++11 standard with minor corrections)
//  ¤5.2.8, first point. I paraphrase below (standard is copyrighted).
//
//   typeid returns an object of static type std::type_info. The object
//   referred to by the return value (lvalue) of typeid is guaranteed to exist
//   for the lifetime of the program.
//
// From this, we can safely deduce that the address of any type_info class will
// remain valid for the life of the program.
//
// As such, we can store a pointer to a type_info object inside lua as a void*
// and recast the object to compare types at a later time.
// This functionality is needed for the setDefaults function and to ensure
// that the objects pointed to by shared_ptrs are indeed the same type.
//

typedef const std::type_info* LSSTypeID;

template <typename T>
LSSTypeID LSS_getTypeInfo() { return &typeid(T); }

template <typename T>
bool LSS_compareToTypeOnStack(lua_State* L, int stackIndex)
{
  LSSTypeID a = static_cast<LSSTypeID>(lua_touserdata(L, stackIndex));
  return (*a) == (*LSS_getTypeInfo<T>());
}

template <typename T1, typename T2>
bool LSS_compareTypes()
{
  return (*LSS_getTypeInfo<T1>()) == (*LSS_getTypeInfo<T2>());
}

template <typename T>
void LSS_pushTypeInfo(lua_State* L)
{
  lua_pushlightuserdata(L,const_cast<void*>(reinterpret_cast<const void*>(
                                LSS_getTypeInfo<T>())));
}

#endif


} /* namespace tuvok */

//============================
//
// FUNCTION BINDING TEMPLATES
//
//============================

#include "LUAFunBindingCore.h"

#endif /* LUAFUNBINDING_H_ */
