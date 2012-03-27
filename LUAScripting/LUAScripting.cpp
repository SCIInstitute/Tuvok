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
 \file    LUAScripting.cpp
 \author  James Hughes
          SCI Institute
          University of Utah
 \date    Mar 21, 2012
 \brief   Interface to the LUA scripting system built for Tuvok.
          Made to be unit tested externally.
 */

#ifndef EXTERNAL_UNIT_TESTING

#include "Controller/Controller.h"
#include "3rdParty/LUA/lua.hpp"

#else

#include "assert.h"
#include "utestCommon.h"

#endif

#include <vector>

#include "LUAError.h"
#include "LUAFunBinding.h"
#include "LUAScripting.h"

using namespace std;

#define QUALIFIED_NAME_DELIMITER  "."

namespace tuvok
{


const char* LUAScripting::TBL_MD_DESC           = "desc";
const char* LUAScripting::TBL_MD_SIG            = "signature";
const char* LUAScripting::TBL_MD_SIG_NAME       = "sigName";
const char* LUAScripting::TBL_MD_NUM_EXEC       = "numExec";
const char* LUAScripting::TBL_MD_QNAME          = "fqName";
const char* LUAScripting::TBL_MD_FUN_PDEFS      = "pDefaults";
const char* LUAScripting::TBL_MD_FUN_LAST_EXEC  = "pLastExec";


//-----------------------------------------------------------------------------
LUAScripting::LUAScripting()
{
  mL = lua_newstate(luaInternalAlloc, NULL);

  if (mL == NULL) throw LUAError("Failed to initialize LUA.");

  // Setup lua panic function and open default libraries.
  lua_atpanic(mL, &luaPanic);
  luaL_openlibs(mL);
}

//-----------------------------------------------------------------------------
LUAScripting::~LUAScripting()
{

}

//-----------------------------------------------------------------------------
int LUAScripting::luaPanic(lua_State* L)
{
  // Note: We compile LUA as c plus plus. So we won't have problems throwing
  // exceptions from functions called by LUA. When not compiling as CPP, LUA
  // uses set_jmp and long_jmp. This can cause issues when exceptions are
  // thrown, see:
  // http://developers.sun.com/solaris/articles/mixing.html#except .

  throw LUAError("Unrecoverable LUA error");

  // Returning from this function would mean that abort() gets called by LUA.
  // We don't want this.
  return 0;
}

//-----------------------------------------------------------------------------
void* LUAScripting::luaInternalAlloc(void* ud, void* ptr, size_t osize,
                                            size_t nsize)
{
  // TODO: Convert to use new (mind the realloc).
  (void) ud;
  (void) osize;
  if (nsize == 0)
  {
    free(ptr);
    return NULL;
  }
  else
  {
    return realloc(ptr, nsize);
  }
}

//-----------------------------------------------------------------------------
string LUAScripting::getUnqualifiedName(const string& fqName) const
{
  const string delims(QUALIFIED_NAME_DELIMITER);
  string::size_type beg = fqName.find_last_of(delims, 0);
  if (beg == string::npos)
  {
    return fqName;
  }
  else
  {
    return fqName.substr(beg, string::npos);
  }
}

//-----------------------------------------------------------------------------
void LUAScripting::bindClosureWithFQName(const string& fqName,
                                         int tableIndex)
{
  int baseStackIndex = lua_gettop(mL);

  // Tokenize the fully qualified name.
  const string delims(QUALIFIED_NAME_DELIMITER);
  vector<string> tokens;

  string::size_type beg = 0;
  string::size_type end = 0;

  while (end != string::npos)
  {
    end = fqName.find_first_of(delims, beg);

    if (end != string::npos)
    {
      tokens.push_back(fqName.substr(beg, end - beg));
      beg = end + 1;
      if (beg == fqName.size())
        throw LUAFunBindError("Invalid function name. No function "
                              "name after trailing period.");
    }
    else
    {
      tokens.push_back(fqName.substr(beg, string::npos));
    }
  }

  if (tokens.size() == 0) throw LUAFunBindError("No function name specified.");

  // Build name hierarchy in LUA, handle base case specially due to globals.
  vector<string>::iterator it = tokens.begin();
  string token = (*it);
  ++it;

  lua_getglobal(mL, token.c_str());
  int type = lua_type(mL, -1);
  if (it != tokens.end())
  {
    // Create a new table (module) at the global level.
    if (type == LUA_TNIL)
    {
      lua_pop(mL, 1); // Pop nil off the stack
      lua_newtable(mL);
      lua_pushvalue(mL, -1);    // Push table to keep it on the stack.
      lua_setglobal(mL, token.c_str());
    }
    else if (type != LUA_TTABLE)  // Other
    {
      throw LUAFunBindError("A module in the fully qualified name"
                            "not of type table.");
    }
    // keep the table on the stack
  }
  else
  {
    if (type == LUA_TNIL)
    {
      lua_pop(mL, 1); // Pop nil off the stack
      lua_pushvalue(mL, tableIndex);
      lua_setglobal(mL, token.c_str());
    }
    else
    {
      throw LUAFunBindError("Unable to bind function closure."
                            "Duplicate name already exists in globals.");
    }
  }


  for (; it != tokens.end(); ++it)
  {
    token = *it;

    // The table we are working with is on the top of the stack.
    // Retrieve the key and test its type.
    lua_pushstring(mL, token.c_str());
    lua_gettable(mL, -2);

    type = lua_type(mL, -1);

    // Check to see if we are at the end.
    if ((it != tokens.end()) && (it + 1 == tokens.end()))
    {
      // This should be where the function closure is bound, no exceptions are
      // made for tables.
      if (type == LUA_TNIL)
      {
        lua_pop(mL, 1);   // Pop nil off the stack.
        lua_pushstring(mL, token.c_str());
        lua_pushvalue(mL, tableIndex);
        lua_settable(mL, -3);
        lua_pop(mL, 1);   // Pop last table off of the stack.
      }
      else
      {
        throw LUAFunBindError("Unable to bind function closure."
                              "Duplicate name already exists at last"
                              "descendant.");
      }
    }
    else
    {
      // Create a new table (module) at the global level.
      if (type == LUA_TNIL)
      {
        lua_pop(mL, 1);           // Pop nil off the stack
        lua_newtable(mL);
        lua_pushstring(mL, token.c_str());
        lua_pushvalue(mL, -2);    // Push table to keep it on the stack.
        lua_settable(mL, -4);     // Assign new table to prior table.
        lua_remove(mL, -2);       // Remove prior table from the stack.
      }
      else if (type == LUA_TTABLE)
      {
        // Keep the table at the top, but remove the table we came from.
        lua_remove(mL, -2);
      }
      else
      {
        throw LUAFunBindError("A module in the fully qualified name"
                                      "not of type table.");
      }
    }
  }

  // This is a programmer error, NOT forwardly visible to the user.
  assert(baseStackIndex == lua_gettop(mL));
}



//==============================================================================
//
// UNIT TESTING
//
//==============================================================================

#ifdef EXTERNAL_UNIT_TESTING
namespace
{
  int dfun(int a, int b, int c);

  TEST( TestDynamicModuleRegistration )
  {
    auto_ptr<LUAScripting> sc(new LUAScripting());  // want to use unique_ptr
    lua_State* L = sc->getLUAState();

    // Test successful bindings and their results.
    sc->registerFunction(&dfun, "test.dummyFun", "My test dummy func.");
    sc->registerFunction(&dfun, "p1.p2.p3.dummy", "Test");
    sc->registerFunction(&dfun, "p1.p2.p.dummy", "Test");
    sc->registerFunction(&dfun, "p1.np.p3.p4.dummy", "Test");
    sc->registerFunction(&dfun, "test.dummyFun2", "Test");
    sc->registerFunction(&dfun, "test.test2.dummy", "Test");
    sc->registerFunction(&dfun, "func", "Test");

    luaL_dostring(L, "return getmetatable(test.dummyFun).__call(1,2,39)");
    luaL_dostring(L, "return test.dummyFun(1,2,39)");
    CHECK_EQUAL(42, lua_tointeger(L, -1));
    lua_pop(L, 1);

    luaL_dostring(L, "return p1.p2.p3.dummy(1,2,39)");
    CHECK_EQUAL(42, lua_tointeger(L, -1));
    lua_pop(L, 1);

    luaL_dostring(L, "return p1.p2.p.dummy(1,2,39)");
    CHECK_EQUAL(42, lua_tointeger(L, -1));
    lua_pop(L, 1);

    luaL_dostring(L, "return p1.np.p3.p4.dummy(1,2,39)");
    CHECK_EQUAL(42, lua_tointeger(L, -1));
    lua_pop(L, 1);

    luaL_dostring(L, "return test.dummyFun2(1,2,39)");
    CHECK_EQUAL(42, lua_tointeger(L, -1));
    lua_pop(L, 1);

    luaL_dostring(L, "return test.test2.dummy(1,2,39)");
    CHECK_EQUAL(42, lua_tointeger(L, -1));
    lua_pop(L, 1);

    //luaL_dostring(L, "return getmetatable(func).__call(1,2,39)");
    //luaL_dostring(L, "return func(1,2,39)");
    luaL_loadstring(L, "func(1,2,39)");
    luaL_loadstring(L, "return func(1,2,39)");
    lua_pcall(L, 0, LUA_MULTRET, 0);
    CHECK_EQUAL(42, lua_tointeger(L, -1));
    lua_pop(L, 1);

    // Test failure cases.

    // Check for appropriate exceptions.
    // XXX: We could make more exception classes whose names more closely
    // match the exceptions we are getting below.

    // Exception: No trailing name after period.
    CHECK_THROW(
        sc->registerFunction(&dfun, "err.err.dummyFun.", "Func."),
        LUAFunBindError);

    // Exception: Duplicate name already exists in globals.
    CHECK_THROW(
        sc->registerFunction(&dfun, "p1", "Func."),
        LUAFunBindError);

    // Exception: Duplicate name already exists at last descendant.
    CHECK_THROW(
        sc->registerFunction(&dfun, "p1.p2", "Func."),
        LUAFunBindError);

    // Exception: A module in the fully qualified name not of type table.
    // (descendant case).
    CHECK_THROW(
        sc->registerFunction(&dfun, "test.dummyFun.Func", "Func."),
        LUAFunBindError);

    // Exception: A module in the fully qualified name not of type table.
    // (global case).
    CHECK_THROW(
        sc->registerFunction(&dfun, "func.Func2", "Func."),
        LUAFunBindError);
  }

  int dfun(int a, int b, int c)
  {
    printf("dfun called\n");
    return a + b + c;
  }
}

namespace
{
  string str_int(int in)
  {
    ostringstream os;
    os << "(" << in << ")";
    return os.str();
  }

  string str_int2(int in, int in2)
  {
    ostringstream os;
    os << "(" << in << "," << in2 << ")";
    return os.str();
  }

  // Maximum number of parameters.
  float flt_flt2_int2_dbl2(float a, float b, int c, int d, double e, double f)
  {
    return a * float(c + d) + b * float(e + f);
  }

  int int_()
  {
    return 79;
  }

  void print_flt(float in)
  {
    printf("%f", in);
  }

  string mixer(bool a, int b, float c, double d, string s)
  {
    ostringstream os;
    os << s << " " << a << " " << b << " " << c << " " << d;
    return os.str();
  }

  // When you add new types to LUAFunBinding.h, test them here.
  TEST( TestRegistration )
  {
    auto_ptr<LUAScripting> sc(new LUAScripting());  // want to use unique_ptr
    lua_State* L = sc->getLUAState();

    sc->registerFunction(&str_int, "str.int", "");
    sc->registerFunction(&str_int2, "str.int2", "");
    sc->registerFunction(&flt_flt2_int2_dbl2, "flt.flt2.int2.dbl2", "");
    sc->registerFunction(&mixer, "mixer", "");

    luaL_dostring(L, "return str.int(97)");
    CHECK_EQUAL("(97)", lua_tostring(L, -1));
    lua_pop(L, 1);

    luaL_dostring(L, "return str.int2(978, 42)");
    CHECK_EQUAL("(978,42)", lua_tostring(L, -1));
    lua_pop(L, 1);

    luaL_dostring(L, "return mixer(true, 10, 12.6, 392.9, \"My sTrIng\")");
    CHECK_EQUAL("My sTrIng 1 10 12.6 392.9", lua_tostring(L, -1));
    lua_pop(L, 1);

    luaL_dostring(L, "return flt.flt2.int2.dbl2(2,2,1,4,5,5)");
    CHECK_CLOSE(30.0f, lua_tonumber(L, -1), 0.0001f);
    lua_pop(L, 1);
  }



  // Tests a series of functions closure metadata.
  TEST( TestClosureMetadata )
  {
    auto_ptr<LUAScripting> sc(new LUAScripting());  // want to use unique_ptr
    lua_State* L = sc->getLUAState();

    sc->registerFunction(&str_int, "str.fint", "desc str_int");
    sc->registerFunction(&str_int2, "str.fint2", "desc str_int2");
    sc->registerFunction(&int_, "fint", "desc int_");
    sc->registerFunction(&print_flt, "print_flt", "Prints Floats");

    string exe;

    //------------------
    // Test description
    //------------------
    exe = string("return str.fint.") + LUAScripting::TBL_MD_DESC;
    luaL_dostring(L, exe.c_str());
    CHECK_EQUAL("desc str_int", lua_tostring(L, -1));
    lua_pop(L, 1);

    exe = string("return str.fint2.") + LUAScripting::TBL_MD_DESC;
    luaL_dostring(L, exe.c_str());
    CHECK_EQUAL("desc str_int2", lua_tostring(L, -1));
    lua_pop(L, 1);

    exe = string("return fint.") + LUAScripting::TBL_MD_DESC;
    luaL_dostring(L, exe.c_str());
    CHECK_EQUAL("desc int_", lua_tostring(L, -1));
    lua_pop(L, 1);

    exe = string("return print_flt.") + LUAScripting::TBL_MD_DESC;
    luaL_dostring(L, exe.c_str());
    CHECK_EQUAL("Prints Floats", lua_tostring(L, -1));
    lua_pop(L, 1);


    //----------------
    // Test signature
    //----------------
    exe = "return str.fint.";
    luaL_dostring(L, string(exe + LUAScripting::TBL_MD_SIG).c_str());
    CHECK_EQUAL("string (int)", lua_tostring(L, -1));
    lua_pop(L, 1);
    luaL_dostring(L, string(exe + LUAScripting::TBL_MD_SIG_NAME).c_str());
    CHECK_EQUAL("string fint(int)", lua_tostring(L, -1));
    lua_pop(L, 1);

    exe = "return str.fint2.";
    luaL_dostring(L, string(exe + LUAScripting::TBL_MD_SIG).c_str());
    CHECK_EQUAL("string (int, int)", lua_tostring(L, -1));
    lua_pop(L, 1);
    luaL_dostring(L, string(exe + LUAScripting::TBL_MD_SIG_NAME).c_str());
    CHECK_EQUAL("string fint2(int, int)", lua_tostring(L, -1));
    lua_pop(L, 1);

    exe = "return fint.";
    luaL_dostring(L, string(exe + LUAScripting::TBL_MD_SIG).c_str());
    CHECK_EQUAL("int ()", lua_tostring(L, -1));
    lua_pop(L, 1);
    luaL_dostring(L, string(exe + LUAScripting::TBL_MD_SIG_NAME).c_str());
    CHECK_EQUAL("int fint()", lua_tostring(L, -1));
    lua_pop(L, 1);

    exe = "return print_flt.";
    luaL_dostring(L, string(exe + LUAScripting::TBL_MD_SIG).c_str());
    CHECK_EQUAL("void (float)", lua_tostring(L, -1));
    lua_pop(L, 1);
    luaL_dostring(L, string(exe + LUAScripting::TBL_MD_SIG_NAME).c_str());
    CHECK_EQUAL("void print_flt(float)", lua_tostring(L, -1));
    lua_pop(L, 1);


    //------------------------------------------------------------------
    // Number of executions (simple value -- only testing one function)
    //------------------------------------------------------------------
    exe = string("return print_flt.")
        + LUAScripting::TBL_MD_NUM_EXEC;
    luaL_dostring(L, exe.c_str());
    CHECK_EQUAL(0, lua_tointeger(L, -1));
    lua_pop(L, 1);


    //------------------------------------------------------------
    // Qualified name (simple value -- only testing one function)
    //------------------------------------------------------------
    exe = string("return str.fint2.")
        + LUAScripting::TBL_MD_QNAME;
    luaL_dostring(L, exe.c_str());
    CHECK_EQUAL("str.fint2", lua_tostring(L, -1));
    lua_pop(L, 1);

    //---------------
    // Test defaults
    //---------------
    // Use a new function, 'mixer' to test the defaults and last table.
    sc->registerFunction(&mixer, "mixer", "Testing all parameter types");

    exe = string("return mixer.")
        + LUAScripting::TBL_MD_FUN_PDEFS;
    luaL_dostring(L, exe.c_str());
    CHECK_EQUAL(LUA_TTABLE, lua_type(L, -1));

    // First param: bool
    lua_pushnumber(L, 0);
    lua_gettable(L, -2);
    CHECK_EQUAL(LUA_TBOOLEAN, lua_type(L, -1));
    CHECK_EQUAL(0, lua_toboolean(L, -1));
    lua_pop(L, 1);

    // Second param: int
    lua_pushnumber(L, 1);
    lua_gettable(L, -2);
    CHECK_EQUAL(LUA_TNUMBER, lua_type(L, -1));
    CHECK_CLOSE(0.0f, lua_tonumber(L, -1), 0.001f);
    lua_pop(L, 1);

    // Third param: float
    lua_pushnumber(L, 2);
    lua_gettable(L, -2);
    CHECK_EQUAL(LUA_TNUMBER, lua_type(L, -1));
    CHECK_CLOSE(0.0f, lua_tonumber(L, -1), 0.001f);
    lua_pop(L, 1);

    // Fourth param: double
    lua_pushnumber(L, 3);
    lua_gettable(L, -2);
    CHECK_EQUAL(LUA_TNUMBER, lua_type(L, -1));
    CHECK_CLOSE(0.0f, lua_tonumber(L, -1), 0.001f);
    lua_pop(L, 1);

    // Fifth param: string
    lua_pushnumber(L, 4);
    lua_gettable(L, -2);
    CHECK_EQUAL(LUA_TSTRING, lua_type(L, -1));
    CHECK_EQUAL("", lua_tostring(L, -1));
    lua_pop(L, 1);

    // Remove table
    lua_pop(L, 1);

    // No need to test last exec table, it is just a copy of thedefaults table
    // at this point
  }

  TEST(LastExecMetaPopulation)
  {

  }

  TEST(Provenance)
  {

  }

  TEST(UndoRedo)
  {

  }
}

#endif


} /* namespace tuvok */
