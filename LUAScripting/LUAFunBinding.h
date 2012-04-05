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
/// make changes. They are listed in these instructions.
/// 1) Change the member function and static function templates to reflect the
///    new number of parameters.
/// 2) Update LUAC_MAX_NUM_PARAMS below. This is only use to ensure we don't
///    exceed our stack space in Lua.
/// 3) Add additional execution function to LuaScripting.


#ifndef TUVOK_LUAFUNBINDING_H_
#define TUVOK_LUAFUNBINDING_H_

namespace tuvok
{

//==============================================================
//
// LUA PARAM GETTER/PUSHER (we do NOT pop off of the LUA stack)
//
//==============================================================

#define VOID_TYPE_STRING ("void")

class LuaEmptyType {};

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
  static std::string getTypesStr();
  static T getDefault();
};

// TODO: 	Shared pointers. Need to decide if we even want to support these.
//			If we do want to support these, what do these types mean inside of LUA?

// Specializations (supported parameter/return types)

template<>
class LuaStrictStack<LuaEmptyType>
{
public:
  static LuaEmptyType get(lua_State* L, int pos)   {return getDefault();}
  static void push(lua_State* L, int in)  {}

  static std::string  getTypeStr() { return "EMPTY TYPE"; }
  static LuaEmptyType getDefault() { return LuaEmptyType(); }
};

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

  static std::string getTypeStr() { return VOID_TYPE_STRING; }

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

  static std::string getTypeStr() { return "string"; }
  static std::string getDefault() { return ""; }
};

// TODO:	Add support for std::vector and std::map, both to be implemented as
//        tables in LUA. std::vector would be efficiently implemented in LUA.
//        In LUA, it would be stored internally as an array instead of
//        key/value pairs in a hash table.
//			  See http://www.lua.org/doc/hopl.pdf -- page 2, para 2. See ref 31.
//			  Consider support for 3D and 4D graphics vectors.

//==========================================================
//
// PUSH CLASS ALLOWS PUSHING A VARIABLE NUMBER OF ARGUMENTS
//
//==========================================================

//template <typename RET = LuaEmptyType, typename T1 = LuaEmptyType,
//          typename T2 = LuaEmptyType, typename T3 = LuaEmptyType,
//          typename T4 = LuaEmptyType, typename T5 = LuaEmptyType,
//          typename T6 = LuaEmptyType>
//class LuaPushClass
//{
//
//};

//==========================
//
// LUA C FUNCTION EXECUTION
//
//==========================

// Please update the following definition if you add more parameters.
#define LUAC_MAX_NUM_PARAMS (6)

// Check prior definitions.
#ifdef EP_INIT
    #error __FILE__ redefines EP_INIT
#endif

#ifdef NM
    #error __FILE__ redefines NM
#endif

#ifdef EP
    #error __FILE__ redefines EP
#endif

#ifdef SG
    #error __FILE__ redefines SG
#endif

#ifdef M_NM
    #error __FILE__ redefines M_NM
#endif

#ifdef MVAR
    #error __FILE__ redefines MVAR
#endif

#ifdef PLP_INIT
    #error __FILE__ redefines PLP_INIT
#endif

#ifdef PLP
    #error __FILE__ redefines PLP
#endif

#ifdef PHP
    #error __FILE__ redefines PHP
#endif

#ifdef MVIT
    #error __FILE__ redefines MVIT
#endif

// These definitions deal with extracting parameters off of the
// LUA stack and storing them in local/member variables to be consumed later.

// The classes below could easily be written without these preprocessor macros,
// but the macros make it easier to replicate the classes when more parameters
// are needed.
#define EP_INIT(idx) int pos = idx; // We are using the __call metamethod, so
                                    // the table associated with the metamethod
                                    // will take the first stack position.

// Variable Name
#define NM(x)       x##_v

// Extract Parameter
#define EP(x)       x NM(x) = LuaStrictStack<x>::get(L, pos++);

// Function signature extraction
#define SG(x)       LuaStrictStack<x>::getTypeStr()

// Member variable name and definition.
#define M_NM(x)     x##_mv
#define MVAR(x)     x M_NM(x)

// Pull parameter from stack into member variable (starts at x -- start index)
#define PLP_INIT(x) int pos = x;
#define PLP(x)      M_NM(x) = LuaStrictStack<x>::get(L, pos++);

// Push parameter from member variable onto the top of the stack.
#define PHP(x)      LuaStrictStack<x>::push(L, M_NM(x));

// Member variable initialization (initialize with the default for that type).
#define MVIT(x)     M_NM(x)(LuaStrictStack<x>::getDefault())

// Abstract base classed used to push and pull parameters off of internal
// undo/redo stacks.
class LuaCFunAbstract
{
public:
  virtual ~LuaCFunAbstract() {}

  virtual void pushParamsToStack(lua_State* L) const      = 0;

  /// Pulls parameters from the stack, starting at the non-psuedo index si.
  /// Does NOT pop the parameters off the stack.
  virtual void pullParamsFromStack(lua_State* L, int si)  = 0;
};


// LUA C function execution base unspecialized template.
template<typename LuaFunExec>
class LuaCFunExec : public LuaCFunAbstract
{
public:
  // We want to keep the run and getSignature functions static so we don't
  // have to initialize member variables for these common operations
  // (we don't know their types).
  static std::string getSigNoReturn(const std::string& funcName);
  static std::string getSignature(const std::string& funcName);

  // Pushing and pulling parameters from the stack are used to store parameters
  // on the undo / redo stacks within the program.
  virtual void pushParamsToStack(lua_State* L) const;
  virtual void pullParamsFromStack(lua_State* L, int si); // si = starting
                                                          // stack index
};


//------------------
//
// STATIC FUNCTIONS
//
//------------------

// Are return values useful to store alongside the parameters?

//--------------
// 0 PARAMETERS
//--------------
template<typename Ret>
class LuaCFunExec<Ret (*)()> : public LuaCFunAbstract
{
public:
  typedef Ret returnType;
  typedef Ret (*fpType)();
  static Ret run(lua_State* L, int paramStackIndex, fpType fp)
  {
    return fp();
  }
  static std::string getSigNoReturn(const std::string& funcName)
  {
    return funcName + "()";
  }
  static std::string getSignature(const std::string& funcName)
  { return SG(Ret) + " " + getSigNoReturn(funcName); }

  virtual void pushParamsToStack(lua_State* L) const      {}
  virtual void pullParamsFromStack(lua_State* L, int si)  {}
};

//-------------
// 1 PARAMETER
//-------------
template<typename Ret, typename P1>
class LuaCFunExec<Ret (*)(P1)> : public LuaCFunAbstract
{
public:
  typedef Ret returnType;
  typedef Ret (*fpType)(P1);
  static Ret run(lua_State* L, int paramStackIndex, fpType fp)
  {
    EP_INIT(paramStackIndex);
              EP(P1);
    return fp(NM(P1));
  }

  static std::string getSigNoReturn(const std::string& funcName)
  {
    return funcName + "(" + SG(P1) + ")";
  }
  static std::string getSignature(const std::string& funcName)
  { return SG(Ret) + " " + getSigNoReturn(funcName); }

  LuaCFunExec()
  : MVIT(P1) {}

  virtual void pushParamsToStack(lua_State* L) const
  { PHP(P1); }
  virtual void pullParamsFromStack(lua_State* L, int si)
  { PLP_INIT(si); PLP(P1); }

  // Member variables.
  MVAR(P1);
};

//--------------
// 2 PARAMETERS
//--------------
template<typename Ret, typename P1, typename P2>
class LuaCFunExec<Ret (*)(P1, P2)> : public LuaCFunAbstract
{
public:
  typedef Ret returnType;
  typedef Ret (*fpType)(P1, P2);
  static Ret run(lua_State* L, int paramStackIndex, fpType fp)
  {
    EP_INIT(paramStackIndex);
              EP(P1); EP(P2);
    return fp(NM(P1), NM(P2));
  }
  static std::string getSigNoReturn(const std::string& funcName)
  {
    return funcName + "(" + SG(P1) + ", " + SG(P2) + ")";
  }
  static std::string getSignature(const std::string& funcName)
  { return SG(Ret) + " " + getSigNoReturn(funcName); }

  LuaCFunExec()
    : MVIT(P1), MVIT(P2) {}

  virtual void pushParamsToStack(lua_State* L) const
  { PHP(P1); PHP(P2); }
  virtual void pullParamsFromStack(lua_State* L, int si)
  { PLP_INIT(si); PLP(P1); PLP(P2); }

  // Member variables.
  MVAR(P1); MVAR(P2);
};

//--------------
// 3 PARAMETERS
//--------------
template<typename Ret, typename P1, typename P2, typename P3>
class LuaCFunExec<Ret (*)(P1, P2, P3)> : public LuaCFunAbstract
{
public:
  typedef Ret returnType;
  typedef Ret (*fpType)(P1, P2, P3);
  static Ret run(lua_State* L, int paramStackIndex, fpType fp)
  {
    EP_INIT(paramStackIndex);
              EP(P1); EP(P2); EP(P3);
    return fp(NM(P1), NM(P2), NM(P3)); // nom, nom, nom ...
  }
  static std::string getSigNoReturn(const std::string& funcName)
  {
    return funcName + "(" + SG(P1) + ", " + SG(P2) + ", " + SG(P3) + ")";
  }
  static std::string getSignature(const std::string& funcName)
  { return SG(Ret) + " " + getSigNoReturn(funcName); }

  LuaCFunExec()
    : MVIT(P1), MVIT(P2), MVIT(P3) {}

  virtual void pushParamsToStack(lua_State* L) const
  { PHP(P1); PHP(P2); PHP(P3); }
  virtual void pullParamsFromStack(lua_State* L, int si)
  { PLP_INIT(si); PLP(P1); PLP(P2); PLP(P3); }

  // Member variables.
  MVAR(P1); MVAR(P2); MVAR(P3);
};

//--------------
// 4 PARAMETERS
//--------------
template<typename Ret, typename P1, typename P2, typename P3, typename P4>
class LuaCFunExec<Ret (*)(P1, P2, P3, P4)> : public LuaCFunAbstract
{
public:
  typedef Ret returnType;
  typedef Ret (*fpType)(P1, P2, P3, P4);
  static Ret run(lua_State* L, int paramStackIndex, fpType fp)
  {
    EP_INIT(paramStackIndex);
              EP(P1); EP(P2); EP(P3); EP(P4);
    return fp(NM(P1), NM(P2), NM(P3), NM(P4));
  }
  static std::string getSigNoReturn(const std::string& funcName)
  {
    return funcName + "(" + SG(P1) + ", " + SG(P2) + ", " + SG(P3) + ", " +
           SG(P4) + ")";
  }
  static std::string getSignature(const std::string& funcName)
  { return SG(Ret) + " " + getSigNoReturn(funcName); }

  LuaCFunExec()
    : MVIT(P1), MVIT(P2), MVIT(P3), MVIT(P4) {}

  void pushParamsToStack(lua_State* L) const
  { PHP(P1); PHP(P2); PHP(P3); PHP(P4); }
  void pullParamsFromStack(lua_State* L, int si)
  { PLP_INIT(si); PLP(P1); PLP(P2); PLP(P3); PLP(P4); }

  // Member variables.
  MVAR(P1); MVAR(P2); MVAR(P3); MVAR(P4);
};

//--------------
// 5 PARAMETERS
//--------------
template<typename Ret, typename P1, typename P2, typename P3, typename P4,
         typename P5>
class LuaCFunExec<Ret (*)(P1, P2, P3, P4, P5)> : public LuaCFunAbstract
{
public:
  typedef Ret returnType;
  typedef Ret (*fpType)(P1, P2, P3, P4, P5);
  static Ret run(lua_State* L, int paramStackIndex, fpType fp)
  {
    EP_INIT(paramStackIndex);
              EP(P1); EP(P2); EP(P3); EP(P4); EP(P5);
    return fp(NM(P1), NM(P2), NM(P3), NM(P4), NM(P5));
  }
  static std::string getSigNoReturn(const std::string& funcName)
  {
    return funcName + "(" + SG(P1) + ", " + SG(P2) + ", " + SG(P3) + ", " +
            SG(P4) + ", " + SG(P5) + ")";
  }
  static std::string getSignature(const std::string& funcName)
  { return SG(Ret) + " " + getSigNoReturn(funcName); }

  LuaCFunExec()
    : MVIT(P1), MVIT(P2), MVIT(P3), MVIT(P4), MVIT(P5) {}

  virtual void pushParamsToStack(lua_State* L) const
  { PHP(P1); PHP(P2); PHP(P3); PHP(P4); PHP(P5); }
  virtual void pullParamsFromStack(lua_State* L, int si)
  { PLP_INIT(si); PLP(P1); PLP(P2); PLP(P3); PLP(P4); PLP(P5); }

  // Member variables.
  MVAR(P1); MVAR(P2); MVAR(P3); MVAR(P4); MVAR(P5);
};

//--------------
// 6 PARAMETERS
//--------------
template<typename Ret, typename P1, typename P2, typename P3, typename P4,
         typename P5, typename P6>
class LuaCFunExec<Ret (*)(P1, P2, P3, P4, P5, P6)> : public LuaCFunAbstract
{
public:
  typedef Ret returnType;
  typedef Ret (*fpType)(P1, P2, P3, P4, P5, P6);
  static Ret run(lua_State* L, int paramStackIndex, fpType fp)
  {
    EP_INIT(paramStackIndex);
              EP(P1); EP(P2); EP(P3); EP(P4); EP(P5); EP(P6);
    return fp(NM(P1), NM(P2), NM(P3), NM(P4), NM(P5), NM(P6));
  }
  static std::string getSigNoReturn(const std::string& funcName)
  {
    return funcName + "(" + SG(P1) + ", " + SG(P2) + ", " + SG(P3) + ", " +
            SG(P4) + ", " + SG(P5) + ", " + SG(P6) + ")";
  }
  static std::string getSignature(const std::string& funcName)
  { return SG(Ret) + " " + getSigNoReturn(funcName); }

  LuaCFunExec()
    : MVIT(P1), MVIT(P2), MVIT(P3), MVIT(P4), MVIT(P5), MVIT(P6) {}

  virtual void pushParamsToStack(lua_State* L) const
  { PHP(P1); PHP(P2); PHP(P3); PHP(P4); PHP(P5); PHP(P6); }
  virtual void pullParamsFromStack(lua_State* L, int si)
  { PLP_INIT(si); PLP(P1); PLP(P2); PLP(P3); PLP(P4); PLP(P5); PLP(P6); }

  // Member variables.
  MVAR(P1); MVAR(P2); MVAR(P3); MVAR(P4); MVAR(P5); MVAR(P6);
};


// ... Add as many parameters as you want here ...

//------------------
//
// MEMBER FUNCTIONS
//
//------------------

// XXX: Add const member functions?

//--------------
// 0 PARAMETERS
//--------------
template<typename T, typename Ret>
class LuaCFunExec<Ret (T::*)()> : public LuaCFunAbstract
{
public:
  typedef T classType;
  typedef Ret returnType;
  typedef Ret (T::*fpType)();
  static Ret run(lua_State* L, int paramStackIndex, T* c, fpType fp)
  {
    return (c->*fp)();
  }
  static std::string getSigNoReturn(const std::string& funcName)
  {
    return funcName + "()";
  }
  static std::string getSignature(const std::string& funcName)
  { return SG(Ret) + " " + getSigNoReturn(funcName); }

  virtual void pushParamsToStack(lua_State* L) const      {}
  virtual void pullParamsFromStack(lua_State* L, int si)  {}
};

//-------------
// 1 PARAMETER
//-------------
template<typename T, typename Ret, typename P1>
class LuaCFunExec<Ret (T::*)(P1)> : public LuaCFunAbstract
{
public:
  typedef T classType;
  typedef Ret returnType;
  typedef Ret (T::*fpType)(P1);
  static Ret run(lua_State* L, int paramStackIndex, T* c, fpType fp)
  {
    EP_INIT(paramStackIndex);
                    EP(P1);
    return (c->*fp)(NM(P1));
  }
  static std::string getSigNoReturn(const std::string& funcName)
  {
    return funcName + "(" + SG(P1) + ")";
  }
  static std::string getSignature(const std::string& funcName)
  { return SG(Ret) + " " + getSigNoReturn(funcName); }

  LuaCFunExec()
  : MVIT(P1) {}

  virtual void pushParamsToStack(lua_State* L) const
  { PHP(P1); }
  virtual void pullParamsFromStack(lua_State* L, int si)
  { PLP_INIT(si); PLP(P1); }

  // Member variables.
  MVAR(P1);
};

//--------------
// 2 PARAMETERS
//--------------
template<typename T, typename Ret, typename P1, typename P2>
class LuaCFunExec<Ret (T::*)(P1, P2)> : public LuaCFunAbstract
{
public:
  typedef T classType;
  typedef Ret returnType;
  typedef Ret (T::*fpType)(P1, P2);
  static Ret run(lua_State* L, int paramStackIndex, T* c, fpType fp)
  {
    EP_INIT(paramStackIndex);
                    EP(P1); EP(P2);
    return (c->*fp)(NM(P1), NM(P2));
  }
  static std::string getSigNoReturn(const std::string& funcName)
  {
    return funcName + "(" + SG(P1) + ", " + SG(P2) + ")";
  }
  static std::string getSignature(const std::string& funcName)
  { return SG(Ret) + " " + getSigNoReturn(funcName); }

  LuaCFunExec()
    : MVIT(P1), MVIT(P2) {}

  virtual void pushParamsToStack(lua_State* L) const
  { PHP(P1); PHP(P2); }
  virtual void pullParamsFromStack(lua_State* L, int si)
  { PLP_INIT(si); PLP(P1); PLP(P2); }

  // Member variables.
  MVAR(P1); MVAR(P2);
};

//--------------
// 3 PARAMETERS
//--------------
template<typename T, typename Ret, typename P1, typename P2, typename P3>
class LuaCFunExec<Ret (T::*)(P1, P2, P3)> : public LuaCFunAbstract
{
public:
  typedef T classType;
  typedef Ret returnType;
  typedef Ret (T::*fpType)(P1, P2, P3);
  static Ret run(lua_State* L, int paramStackIndex, T* c, fpType fp)
  {
    EP_INIT(paramStackIndex);
                    EP(P1); EP(P2); EP(P3);
    return (c->*fp)(NM(P1), NM(P2), NM(P3)); // nom, nom, nom ...
  }
  static std::string getSigNoReturn(const std::string& funcName)
  {
    return funcName + "(" + SG(P1) + ", " + SG(P2) + ", " +
           SG(P3) + ")";
  }
  static std::string getSignature(const std::string& funcName)
  { return SG(Ret) + " " + getSigNoReturn(funcName); }

  LuaCFunExec()
    : MVIT(P1), MVIT(P2), MVIT(P3) {}

  virtual void pushParamsToStack(lua_State* L) const
  { PHP(P1); PHP(P2); PHP(P3); }
  virtual void pullParamsFromStack(lua_State* L, int si)
  { PLP_INIT(si); PLP(P1); PLP(P2); PLP(P3); }

  // Member variables.
  MVAR(P1); MVAR(P2); MVAR(P3);
};

//--------------
// 4 PARAMETERS
//--------------
template<typename T, typename Ret, typename P1, typename P2, typename P3,
         typename P4>
class LuaCFunExec<Ret (T::*)(P1, P2, P3, P4)> : public LuaCFunAbstract
{
public:
  typedef T classType;
  typedef Ret returnType;
  typedef Ret (T::*fpType)(P1, P2, P3, P4);
  static Ret run(lua_State* L, int paramStackIndex, T* c, fpType fp)
  {
    EP_INIT(paramStackIndex);
                    EP(P1); EP(P2); EP(P3); EP(P4);
    return (c->*fp)(NM(P1), NM(P2), NM(P3), NM(P4));
  }
  static std::string getSigNoReturn(const std::string& funcName)
  {
    return funcName + "(" + SG(P1) + ", " + SG(P2) + ", " +
            SG(P3) + ", " + SG(P4) + ")";
  }
  static std::string getSignature(const std::string& funcName)
  { return SG(Ret) + " " + getSigNoReturn(funcName); }

  LuaCFunExec()
    : MVIT(P1), MVIT(P2), MVIT(P3), MVIT(P4) {}

  virtual void pushParamsToStack(lua_State* L) const
  { PHP(P1); PHP(P2); PHP(P3); PHP(P4); }
  virtual void pullParamsFromStack(lua_State* L, int si)
  { PLP_INIT(si); PLP(P1); PLP(P2); PLP(P3); PLP(P4); }

  // Member variables.
  MVAR(P1); MVAR(P2); MVAR(P3); MVAR(P4);
};

//--------------
// 5 PARAMETERS
//--------------
template<typename T, typename Ret, typename P1, typename P2, typename P3,
         typename P4, typename P5>
class LuaCFunExec<Ret (T::*)(P1, P2, P3, P4, P5)> : public LuaCFunAbstract
{
public:
  typedef T classType;
  typedef Ret returnType;
  typedef Ret (T::*fpType)(P1, P2, P3, P4, P5);
  static Ret run(lua_State* L, int paramStackIndex, T* c, fpType fp)
  {
    EP_INIT(paramStackIndex);
                    EP(P1); EP(P2); EP(P3); EP(P4); EP(P5);
    return (c->*fp)(NM(P1), NM(P2), NM(P3), NM(P4), NM(P5));
  }
  static std::string getSigNoReturn(const std::string& funcName)
  {
    return funcName + "(" + SG(P1) + ", " + SG(P2) + ", " +
            SG(P3) + ", " + SG(P4) + ", " + SG(P5) + ")";
  }
  static std::string getSignature(const std::string& funcName)
  { return SG(Ret) + " " + getSigNoReturn(funcName); }

  LuaCFunExec()
    : MVIT(P1), MVIT(P2), MVIT(P3), MVIT(P4), MVIT(P5) {}

  virtual void pushParamsToStack(lua_State* L) const
  { PHP(P1); PHP(P2); PHP(P3); PHP(P4); PHP(P5); }
  virtual void pullParamsFromStack(lua_State* L, int si)
  { PLP_INIT(si); PLP(P1); PLP(P2); PLP(P3); PLP(P4); PLP(P5); }

  // Member variables.
  MVAR(P1); MVAR(P2); MVAR(P3); MVAR(P4); MVAR(P5);
};

//--------------
// 6 PARAMETERS
//--------------
template<typename T, typename Ret, typename P1, typename P2, typename P3,
         typename P4, typename P5, typename P6>
class LuaCFunExec<Ret (T::*)(P1, P2, P3, P4, P5, P6)> : public LuaCFunAbstract
{
public:
  static const int memberFunc = 1;
  typedef T classType;
  typedef Ret returnType;
  typedef Ret (T::*fpType)(P1, P2, P3, P4, P5, P6);
  static Ret run(lua_State* L, int paramStackIndex, T* c, fpType fp)
  {
    EP_INIT(paramStackIndex);
                    EP(P1); EP(P2); EP(P3); EP(P4); EP(P5); EP(P6);
    return (c->*fp)(NM(P1), NM(P2), NM(P3), NM(P4), NM(P5), NM(P6));
  }
  static std::string getSigNoReturn(const std::string& funcName)
  {
    return funcName + "(" + SG(P1) + ", " + SG(P2) + ", " +
            SG(P3) + ", " + SG(P4) + ", " + SG(P5) + ", " + SG(P6) + ")";
  }
  static std::string getSignature(const std::string& funcName)
  { return SG(Ret) + " " + getSigNoReturn(funcName); }

  LuaCFunExec()
    : MVIT(P1), MVIT(P2), MVIT(P3), MVIT(P4), MVIT(P5), MVIT(P6) {}

  virtual void pushParamsToStack(lua_State* L) const
  { PHP(P1); PHP(P2); PHP(P3); PHP(P4); PHP(P5); PHP(P6); }
  virtual void pullParamsFromStack(lua_State* L, int si)
  { PLP_INIT(si); PLP(P1); PLP(P2); PLP(P3); PLP(P4); PLP(P5); PLP(P6); }

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
#undef PHP
#undef MVIT

} /* namespace tuvok */

#endif /* LUAFUNBINDING_H_ */
