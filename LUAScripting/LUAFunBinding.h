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

// This is a complete implementation of static function binding in LUA.
#ifndef LUAFUNBINDING_H_
#define LUAFUNBINDING_H_

namespace tuvok
{

//==============================================================
// LUA PARAM GETTER/PUSHER (we do NOT pop off of the LUA stack)
//==============================================================

// LUA strict type stack
// This template enforces strict type compliance while converting types on the
// LUA stack.

template<typename T>
class LUAStrictStack
{
public:
  // Intentionally left unimplemented to generate compiler errors if this
  // template is chosen.
  static T get(lua_State* L, int pos);
  static void push(lua_State* L, T data);
  static std::string getTypesStr();
  static T getDefault();
};

// TODO: 	Shared pointers. Need to decide if we even want to support these.
//			If we do want to support these, what do these types mean inside of LUA?

// Specializations (supported parameter/return types)

template<>
class LUAStrictStack<void>
{
public:
  // All functions but getTypeStr don't do anything since it doesn't make
  // sense in the context of void. Compiler errors will result if there is
  // an attempted access. Generally, getTypeStr from this specialization
  // is only called when building the return type of function signatures.
  static int get(lua_State* L, int pos);
  static void push(lua_State* L, int in);

  static std::string getTypeStr() { return "void"; }

  static int         getDefault();
};

template<>
class LUAStrictStack<int>
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

  static std::string getTypeStr() { return "int"; }
  static int         getDefault() { return 0; }
};

template<>
class LUAStrictStack<bool>
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

  static std::string getTypeStr() { return "bool"; }
  static bool        getDefault() { return false; }
};

template<>
class LUAStrictStack<float>
{
public:
  static float get(lua_State* L, int pos)
  {
    return (float) luaL_checknumber(L, pos);
  }

  static void push(lua_State* L, float in)
  {
    lua_pushnumber(L, (lua_Number) in);
  }

  static std::string getTypeStr() { return "float"; }
  static float       getDefault() { return 0.0f; }
};

template<>
class LUAStrictStack<double>
{
public:
  static double get(lua_State* L, int pos)
  {
    return (double) luaL_checknumber(L, pos);
  }

  static void push(lua_State* L, double in)
  {
    lua_pushnumber(L, (lua_Number) in);
  }

  static std::string getTypeStr() { return "double"; }
  static double      getDefault() { return 0.0; }
};

template<>
class LUAStrictStack<const char *>
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

  static std::string getTypeStr() { return "string"; }
  static std::string getDefault() { return ""; }
};

template<>
class LUAStrictStack<std::string>
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

  static std::string getTypeStr() { return "string"; }
  static std::string getDefault() { return ""; }
};

template<>
class LUAStrictStack<std::string&>
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

  static std::string getTypeStr() { return "string"; }
  static std::string getDefault() { return ""; }
};

template<>
class LUAStrictStack<const std::string&>
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

  static std::string getTypeStr() { return "string"; }
  static std::string getDefault() { return ""; }
};

// TODO:	Add support for std::vector and std::map, both to be implemented as
//        tables in LUA. std::vector is efficiently implemented. It is stored
//        internally as an array in a LUA table.
//			  See http://www.lua.org/doc/hopl.pdf -- page 2, para 2. See ref 31.
//			  Consider support for 3D and 4D graphics vectors.

//==========================
// LUA C FUNCTION EXECUTION
//==========================

// Check prior definitions.
#ifdef EP_INIT
    #error LUAFunBinding.h redefines EP_INIT
#endif

#ifdef NM
    #error LUAFunBinding.h redefines NM
#endif

#ifdef EP
    #error LUAFunBinding.h redefines EP
#endif

#ifdef SG
    #error LUAFunBinding.h redefines SG
#endif

#ifdef M_NM
    #error LUAFunBinding.h redefines M_NM
#endif

#ifdef MVAR
    #error LUAFunBinding.h redefines MVAR
#endif

#ifdef PLP_INIT
    #error LUAFunBinding.h redefines PLP_INIT
#endif

#ifdef PLP
    #error LUAFunBinding.h redefines PLP
#endif

#ifdef PHP_INIT
    #error LUAFunBinding.h redefines PHP_INIT
#endif

#ifdef PHP
    #error LUAFunBinding.h redefines PHP
#endif

#ifdef MVIT
    #error LUAFunBinding.h redefines MVIT
#endif

// These definitions deal with extracting parameters off of the
// LUA stack and storing them in local/member variables to be consumed later.

// The classes below could easily be written without these preprocessor macros,
// but they make it easier to replicate the classes when more parameters are
// needed.
#define EP_INIT   int pos = 1;

// Variable Name
#define NM(x)     x##_v

// Extract Parameter
#define EP(x)     x NM(x) = LUAStrictStack<x>::get(L, pos); ++pos;

// Function signature extraction
#define SG(x)     LUAStrictStack<x>::getTypeStr()

// Member variable name and definition.
#define M_NM(x)   x##_mv
#define MVAR(x)   x M_NM(x)

// Pull parameter from stack into member variable.
#define PLP_INIT  int pos = 1;
#define PLP(x)    M_NM(x) = LUAStrictStack<x>::get(L, pos); ++pos;

// Push parameter from member variable onto the top of the stack.
#define PHP_INIT  int pos = 1;
#define PHP(x)    LUAStrictStack<x>::push(L, M_NM(x)); ++pos;

// Member variable initialization (initialize with the default for that type).
#define MVIT(x)   M_NM(x)(LUAStrictStack<x>::getDefault())

// LUA C function execution template.
template<typename LUAFunExec>
class LUACFunExec
{
  // We want to keep the run and getSignature functions static so we don't
  // have to initialize member variables for these common operations
  // (we don't know their types).
  static std::string getSignature(const std::string& funcName);

  // Pushing and pulling parameters from the stack are used to store parameters
  // on the undo / redo stacks within the program.
  void pushParamsToStack(lua_State* L) const;
  void pullParamsFromStack(lua_State* L);
};

//--------------
// 0 PARAMETERS
//--------------
template<typename Ret>
class LUACFunExec<Ret (*)()>
{
public:
  typedef Ret returnType;
  typedef Ret (*fpType)();
  static Ret run(fpType fp, lua_State* L)
  {
    return fp();
  }
  static std::string getSignature(const std::string& funcName)
  {
    return SG(Ret) + " " + funcName + "()";
  }

  void pushParamsToStack(lua_State* L) const  {}
  void pullParamsFromStack(lua_State* L)      {}
};

//-------------
// 1 PARAMETER
//-------------
template<typename Ret, typename P1>
class LUACFunExec<Ret (*)(P1)>
{
public:
  typedef Ret returnType;
  typedef Ret (*fpType)(P1);
  static Ret run(fpType fp, lua_State* L)
  {
    EP_INIT;
              EP(P1);
    return fp(NM(P1));
  }
  static std::string getSignature(const std::string& funcName)
  {
    return SG(Ret) + " " + funcName + "(" + SG(P1) + ")";
  }

  LUACFunExec()
  : MVIT(P1) {}

  void pushParamsToStack(lua_State* L) const
  { PHP_INIT; PHP(P1); }
  void pullParamsFromStack(lua_State* L)
  { PLP_INIT; PLP(P1); }

  // Member variables.
  MVAR(P1);
};

//--------------
// 2 PARAMETERS
//--------------
template<typename Ret, typename P1, typename P2>
class LUACFunExec<Ret (*)(P1, P2)>
{
public:
  typedef Ret returnType;
  typedef Ret (*fpType)(P1, P2);
  static Ret run(fpType fp, lua_State* L)
  {
    EP_INIT;
              EP(P1); EP(P2);
    return fp(NM(P1), NM(P2));
  }
  static std::string getSignature(const std::string& funcName)
  {
    return SG(Ret) + " " + funcName + "(" + SG(P1) + ", " + SG(P2) + ")";
  }

  LUACFunExec()
    : MVIT(P1), MVIT(P2) {}

  void pushParamsToStack(lua_State* L) const
  { PHP_INIT; PHP(P1); PHP(P2); }
  void pullParamsFromStack(lua_State* L)
  { PLP_INIT; PLP(P1); PLP(P2); }

  // Member variables.
  MVAR(P1); MVAR(P2);
};

//--------------
// 3 PARAMETERS
//--------------
template<typename Ret, typename P1, typename P2, typename P3>
class LUACFunExec<Ret (*)(P1, P2, P3)>
{
public:
  typedef Ret returnType;
  typedef Ret (*fpType)(P1, P2, P3);
  static Ret run(fpType fp, lua_State* L)
  {
    EP_INIT;
              EP(P1); EP(P2); EP(P3);
    return fp(NM(P1), NM(P2), NM(P3)); // nom, nom, nom ...
  }
  static std::string getSignature(const std::string& funcName)
  {
    return SG(Ret) + " " + funcName + "(" + SG(P1) + ", " + SG(P2) + ", " +
           SG(P3) + ")";
  }

  LUACFunExec()
    : MVIT(P1), MVIT(P2), MVIT(P3) {}

  void pushParamsToStack(lua_State* L) const
  { PHP_INIT; PHP(P1); PHP(P2); PHP(P3); }
  void pullParamsFromStack(lua_State* L)
  { PLP_INIT; PLP(P1); PLP(P2); PLP(P3); }

  // Member variables.
  MVAR(P1); MVAR(P2); MVAR(P3);
};

//--------------
// 4 PARAMETERS
//--------------
template<typename Ret, typename P1, typename P2, typename P3, typename P4>
class LUACFunExec<Ret (*)(P1, P2, P3, P4)>
{
public:
  typedef Ret returnType;
  typedef Ret (*fpType)(P1, P2, P3, P4);
  static Ret run(fpType fp, lua_State* L)
  {
    EP_INIT;
              EP(P1); EP(P2); EP(P3); EP(P4);
    return fp(NM(P1), NM(P2), NM(P3), NM(P4));
  }
  static std::string getSignature(const std::string& funcName)
  {
    return SG(Ret) + " " + funcName + "(" + SG(P1) + ", " + SG(P2) + ", " +
            SG(P3) + ", " + SG(P4) + ")";
  }

  LUACFunExec()
    : MVIT(P1), MVIT(P2), MVIT(P3), MVIT(P4) {}

  void pushParamsToStack(lua_State* L) const
  { PHP_INIT; PHP(P1); PHP(P2); PHP(P3); PHP(P4); }
  void pullParamsFromStack(lua_State* L)
  { PLP_INIT; PLP(P1); PLP(P2); PLP(P3); PLP(P4); }

  // Member variables.
  MVAR(P1); MVAR(P2); MVAR(P3); MVAR(P4);
};

//--------------
// 5 PARAMETERS
//--------------
template<typename Ret, typename P1, typename P2, typename P3, typename P4,
         typename P5>
class LUACFunExec<Ret (*)(P1, P2, P3, P4, P5)>
{
public:
  typedef Ret returnType;
  typedef Ret (*fpType)(P1, P2, P3, P4, P5);
  static Ret run(fpType fp, lua_State* L)
  {
    EP_INIT;
              EP(P1); EP(P2); EP(P3); EP(P4); EP(P5);
    return fp(NM(P1), NM(P2), NM(P3), NM(P4), NM(P5));
  }
  static std::string getSignature(const std::string& funcName)
  {
    return SG(Ret) + " " + funcName + "(" + SG(P1) + ", " + SG(P2) + ", " +
            SG(P3) + ", " + SG(P4) + ", " + SG(P5) + ")";
  }

  LUACFunExec()
    : MVIT(P1), MVIT(P2), MVIT(P3), MVIT(P4), MVIT(P5) {}

  void pushParamsToStack(lua_State* L) const
  { PHP_INIT; PHP(P1); PHP(P2); PHP(P3); PHP(P4); PHP(P5); }
  void pullParamsFromStack(lua_State* L)
  { PLP_INIT; PLP(P1); PLP(P2); PLP(P3); PLP(P4); PLP(P5); }

  // Member variables.
  MVAR(P1); MVAR(P2); MVAR(P3); MVAR(P4); MVAR(P5);
};

//--------------
// 6 PARAMETERS
//--------------
template<typename Ret, typename P1, typename P2, typename P3, typename P4,
         typename P5, typename P6>
class LUACFunExec<Ret (*)(P1, P2, P3, P4, P5, P6)>
{
public:
  typedef Ret returnType;
  typedef Ret (*fpType)(P1, P2, P3, P4, P5, P6);
  static Ret run(fpType fp, lua_State* L)
  {
    EP_INIT;
              EP(P1); EP(P2); EP(P3); EP(P4); EP(P5); EP(P6);
    return fp(NM(P1), NM(P2), NM(P3), NM(P4), NM(P5), NM(P6));
  }
  static std::string getSignature(const std::string& funcName)
  {
    return SG(Ret) + " " + funcName + "(" + SG(P1) + ", " + SG(P2) + ", " +
            SG(P3) + ", " + SG(P4) + ", " + SG(P5) + ", " + SG(P6) + ")";
  }

  LUACFunExec()
    : MVIT(P1), MVIT(P2), MVIT(P3), MVIT(P4), MVIT(P5), MVIT(P6) {}

  void pushParamsToStack(lua_State* L) const
  { PHP_INIT; PHP(P1); PHP(P2); PHP(P3); PHP(P4); PHP(P5); PHP(P6); }
  void pullParamsFromStack(lua_State* L)
  { PLP_INIT; PLP(P1); PLP(P2); PLP(P3); PLP(P4); PLP(P5); PLP(P6); }

  // Member variables.
  MVAR(P1); MVAR(P2); MVAR(P3); MVAR(P4); MVAR(P5); MVAR(P6);
};


// ... Add as many parameters as you want here ...


#undef EP_INIT
#undef NM
#undef EP
#undef SG
#undef M_NM
#undef MVAR
#undef PLP_INIT
#undef PLP
#undef PHP_INIT
#undef PHP
#undef MVIT

} /* namespace tuvok */

#endif /* LUAFUNBINDING_H_ */
