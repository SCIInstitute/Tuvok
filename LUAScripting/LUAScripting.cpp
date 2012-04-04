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

#include <assert.h>
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

const char* LuaScripting::TBL_MD_DESC           = "desc";
const char* LuaScripting::TBL_MD_SIG            = "signature";
const char* LuaScripting::TBL_MD_SIG_NO_RET     = "sigNoRet";
const char* LuaScripting::TBL_MD_SIG_NAME       = "sigName";
const char* LuaScripting::TBL_MD_NUM_EXEC       = "numExec";
const char* LuaScripting::TBL_MD_QNAME          = "fqName";
const char* LuaScripting::TBL_MD_FUN_PDEFS      = "tblDefaults";
const char* LuaScripting::TBL_MD_FUN_LAST_EXEC  = "tblLastExec";
const char* LuaScripting::TBL_MD_HOOKS          = "tblHooks";
const char* LuaScripting::TBL_MD_HOOK_INDEX     = "hookIndex";
const char* LuaScripting::TBL_MD_MEMBER_HOOKS   = "tblMHooks";


//-----------------------------------------------------------------------------
LuaScripting::LuaScripting()
: mMemberHookIndex(0)
{
  mL = lua_newstate(luaInternalAlloc, NULL);

  if (mL == NULL) throw LUAError("Failed to initialize LUA.");

  lua_atpanic(mL, &luaPanic);
  luaL_openlibs(mL);
}

//-----------------------------------------------------------------------------
LuaScripting::~LuaScripting()
{

}

//-----------------------------------------------------------------------------
int LuaScripting::luaPanic(lua_State* L)
{
  // Note: We compile LUA as c plus plus, so we won't have problems throwing
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
void* LuaScripting::luaInternalAlloc(void* ud, void* ptr, size_t osize,
                                     size_t nsize)
{
  // TODO: Convert to use new (mind the realloc).
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
vector<LuaScripting::FunctionDesc> LuaScripting::getAllFuncDescs() const
{
  vector<LuaScripting::FunctionDesc> ret;

  // Iterate over all registered modules and do a recursive descent through
  // all of the tables to find all functions.
  for (vector<string>::const_iterator it = mRegisteredGlobals.begin();
       it != mRegisteredGlobals.end(); ++it)
  {
    lua_getglobal(mL, (*it).c_str());
    getTableFuncDefs(ret);
    lua_pop(mL, 1);
  }

  return ret;
}

//-----------------------------------------------------------------------------
void LuaScripting::getTableFuncDefs(vector<LuaScripting::FunctionDesc>& descs)
  const
{
  // Iterate over the first table on the stack.
  int tablePos = lua_gettop(mL);

  // Check to see if it is a registered function.
  if (isRegisteredFunction(-1))
  {
    // The name of the function is the 'key'.
    FunctionDesc desc;

    lua_getfield(mL, -1, TBL_MD_QNAME);
    desc.funcName = getUnqualifiedName(string(lua_tostring(mL, -1)));
    lua_pop(mL, 1);

    lua_getfield(mL, -1, TBL_MD_DESC);
    desc.funcDesc = lua_tostring(mL, -1);
    lua_pop(mL, 1);

    lua_getfield(mL, -1, TBL_MD_SIG_NAME);
    desc.funcSig = lua_tostring(mL, -1);
    lua_pop(mL, 1);

    descs.push_back(desc);

    // This was a function, not a table. We do not recurse into its contents.
    return;
  }

  // Push first key.
  lua_pushnil(mL);
  int stackSize = lua_gettop(mL);
  while (lua_next(mL, tablePos))
  {
    // Check if the value is a table. If so, check to see if it is a function,
    // otherwise, recurse into the table.
    int type = lua_type(mL, -1);

    if (type == LUA_TTABLE)
    {
      // Recurse into the table.
      lua_checkstack(mL, 2);
      getTableFuncDefs(descs);
    }

    // Pop value off of the stack in preparation for the next iteration.
    lua_pop(mL, 1);
  }
}

//-----------------------------------------------------------------------------
string LuaScripting::getUnqualifiedName(const string& fqName)
{
  const string delims(QUALIFIED_NAME_DELIMITER);
  string::size_type beg = fqName.find_last_of(delims);
  if (beg == string::npos)
  {
    return fqName;
  }
  else
  {
    return fqName.substr(beg + 1, string::npos);
  }
}

//-----------------------------------------------------------------------------
void LuaScripting::bindClosureTableWithFQName(const string& fqName,
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
      lua_pop(mL, 1);           // Pop nil off the stack
      lua_newtable(mL);
      lua_pushvalue(mL, -1);    // Push table to keep it on the stack.
      lua_setglobal(mL, token.c_str());

      // Add table name to the list of registered globals.
      mRegisteredGlobals.push_back(token);
    }
    else if (type == LUA_TTABLE)  // Other
    {
      if (isRegisteredFunction(-1))
      {
        throw LUAFunBindError("Can't register functions on top of other"
                              "functions.");
      }
    }
    else
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

      // Since the function is registered at the global level, we need to add
      // it to the registered globals list. This ensures all functions are
      // covered during a getAllFuncDescs function call.
      mRegisteredGlobals.push_back(token);
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

        if (isRegisteredFunction(-1))
        {
          throw LUAFunBindError("Can't register functions on top of other"
                                "functions.");
        }
      }
      else
      {
        throw LUAFunBindError("A module in the fully qualified name"
                              "not of type table.");
      }
    }
  }

  assert(baseStackIndex == lua_gettop(mL));
}

//-----------------------------------------------------------------------------
bool LuaScripting::isRegisteredFunction(int stackIndex) const
{
  // Check to make sure this table is NOT a registered function.
  if (lua_getmetatable(mL, stackIndex) != 0)
  {
    // We have a metatable, check to see if isRegFunc exists and is 1.
    lua_getfield(mL, -1, "isRegFunc");
    if (lua_isnil(mL, -1) == 0)
    {
      // We already know that it is a function at this point, but lets go
      // through the motions anyways.
      if (lua_toboolean(mL, -1) == 1)
      {
        lua_pop(mL, 2); // Pop the metatable and isRegFunc off the stack.
        return true;
      }
    }
    lua_pop(mL, 2); // Pop the metatable and isRegFunc off the stack.
  }

  return false;
}


//-----------------------------------------------------------------------------
void LuaScripting::createCallableFuncTable(lua_CFunction proxyFunc,
                                           void* realFuncToCall)
{
  // Table containing the function closure.
  lua_newtable(mL);
  int tableIndex = lua_gettop(mL);

  // Create a new metatable
  lua_newtable(mL);

  // Push C Closure containing our function pointer onto the LUA stack.
  lua_pushlightuserdata(mL, realFuncToCall);
  lua_pushboolean(mL, 0);   // We are NOT a hook being called.
  lua_pushcclosure(mL, proxyFunc, 2);

  // Associate closure with __call metamethod.
  lua_setfield(mL, -2, "__call");

  // Add boolean to the metatable indicating that this table is a registered
  // function. Used to ensure that we can't register functions 'on top' of
  // other functions.
  // e.g. If we register renderer.eye as a function, without this check, we
  // could also register renderer.eye.ball as a function.
  // While it works just fine, it's confusing, so we're disallowing it.
  lua_pushboolean(mL, 1);
  lua_setfield(mL, -2, "isRegFunc");

  // Associate metatable with primary table.
  lua_setmetatable(mL, -2);

  // Leave the table on the top of the stack...
}

//-----------------------------------------------------------------------------
void LuaScripting::populateWithMetadata(const std::string& name,
                                        const std::string& desc,
                                        const std::string& sig,
                                        const std::string& sigWithName,
                                        const std::string& sigNoReturn,
                                        int tableIndex)
{
  int top = lua_gettop(mL);

  // Function description
  lua_pushstring(mL, desc.c_str());
  lua_setfield(mL, tableIndex, TBL_MD_DESC);

  // Function signature
  lua_pushstring(mL, sig.c_str());
  lua_setfield(mL, tableIndex, TBL_MD_SIG);

  // Function signature with name
  lua_pushstring(mL, sigWithName.c_str());
  lua_setfield(mL, tableIndex, TBL_MD_SIG_NAME);

  // Function signature no return.
  lua_pushstring(mL, sigNoReturn.c_str());
  lua_setfield(mL, tableIndex, TBL_MD_SIG_NO_RET);

  // Number of times this function has been executed
  // (takes into account undos, so if a function is undone then this
  //  count will decrease).
  lua_pushnumber(mL, 0);
  lua_setfield(mL, tableIndex, TBL_MD_NUM_EXEC);

  // Fully qualified function name.
  lua_pushstring(mL, name.c_str());
  lua_setfield(mL, tableIndex, TBL_MD_QNAME);

  // Build empty hook tables.
  lua_newtable(mL);
  lua_setfield(mL, tableIndex, TBL_MD_HOOKS);

  lua_newtable(mL);
  lua_setfield(mL, tableIndex, TBL_MD_MEMBER_HOOKS);

  lua_pushinteger(mL, 0);
  lua_setfield(mL, tableIndex, TBL_MD_HOOK_INDEX);

  assert(top == lua_gettop(mL));
}

//-----------------------------------------------------------------------------
void LuaScripting::createDefaultsAndLastExecTables(int tableIndex,
                                                   int numFunParams)
{
  int entryTop = lua_gettop(mL);
  int firstParamPos = (lua_gettop(mL) - numFunParams) + 1;

  // Create defaults table.
  lua_newtable(mL);
  int defTablePos = lua_gettop(mL);

  for (int i = 0; i < numFunParams; i++)
  {
    int stackIndex = firstParamPos + i;
    lua_pushnumber(mL, i);
    lua_pushvalue(mL, stackIndex);
    lua_settable(mL, defTablePos);
  }

  // Insert defaults table in closure table.
  lua_pushstring(mL, TBL_MD_FUN_PDEFS);
  lua_pushvalue(mL, defTablePos);
  lua_settable(mL, tableIndex);

  // Do a deep copy of the defaults table.
  // If we don't do this, we push another reference of the defaults table
  // instead of a deep copy of the table.
  lua_newtable(mL);
  int lastExecTablePos = lua_gettop(mL);

  lua_pushnil(mL);  // First key
  while (lua_next(mL, defTablePos))
  {
    lua_pushvalue(mL, -2);  // Push key
    lua_pushvalue(mL, -2);  // Push value
    lua_settable(mL, lastExecTablePos);
    lua_pop(mL, 1);         // Pop value, keep key for next iteration.
  }
  // lua_next has popped off our initial key.

  // Push a copy of the defaults table onto the stack, and use it as the
  // 'last executed values'.
  lua_pushstring(mL, TBL_MD_FUN_LAST_EXEC);
  lua_pushvalue(mL, -2);
  lua_settable(mL, tableIndex);

  lua_pop(mL, 2);   // Pop the last-exec and the default tables.

  // Remove parameters from the stack.
  lua_pop(mL, numFunParams);
}

//-----------------------------------------------------------------------------
bool LuaScripting::getFunctionTable(const std::string& fqName)
{
  int baseStackIndex = lua_gettop(mL);

  // Tokenize the fully qualified name.
  const string delims(QUALIFIED_NAME_DELIMITER);
  vector<string> tokens;

  string::size_type beg = 0;
  string::size_type end = 0;

  string token;
  int depth = 0;

  while (end != string::npos)
  {
    end = fqName.find_first_of(delims, beg);

    if (end != string::npos)
    {
      token = fqName.substr(beg, end - beg);
      beg = end + 1;

      // If the depth is 0, pull from globals, otherwise pull from the table
      // on the top of the stack.
      if (depth == 0)
      {
        lua_getglobal(mL, token.c_str());
      }
      else
      {
        lua_getfield(mL, -1, token.c_str());
        lua_remove(mL, -2); // Remove the old table from the stack.
      }

      if (lua_isnil(mL, -1))
      {
        lua_settop(mL, baseStackIndex);
        return false;
      }

      if (beg == fqName.size())
      {
        // Empty function name.
        lua_settop(mL, baseStackIndex);
        return false;
      }
    }
    else
    {
      token = fqName.substr(beg, string::npos);

      // Attempt to retrieve the function.
      if (depth == 0)
      {
        lua_getglobal(mL, token.c_str());
      }
      else
      {
        lua_getfield(mL, -1, token.c_str());
        lua_remove(mL, -2); // Remove the old table from the stack.
      }

      if (lua_isnil(mL, -1))
        return false;

      // Leave the function table on the top of the stack.
      return true;
    }

    ++depth;
  }

  lua_settop(mL, baseStackIndex);

  return false;
}

//-----------------------------------------------------------------------------
void LuaScripting::unregisterFunction(const std::string& fqName)
{
  // Lookup the function table based on the fully qualified name.
  int baseStackIndex = lua_gettop(mL);

  // Tokenize the fully qualified name.
  const string delims(QUALIFIED_NAME_DELIMITER);
  vector<string> tokens;

  string::size_type beg = 0;
  string::size_type end = 0;

  string token;
  int depth = 0;

  while (end != string::npos)
  {
    end = fqName.find_first_of(delims, beg);

    if (end != string::npos)
    {
      token = fqName.substr(beg, end - beg);
      beg = end + 1;

      // If the depth is 0, pull from globals, otherwise pull from the table
      // on the top of the stack.
      if (depth == 0)
      {
        lua_getglobal(mL, token.c_str());
      }
      else
      {
        lua_getfield(mL, -1, token.c_str());
        lua_remove(mL, -2);
      }

      if (lua_isnil(mL, -1))
      {
        lua_settop(mL, baseStackIndex);
        throw LUANonExistantFunction("Function not found in unregister.");
      }

      if (beg == fqName.size())
      {
        // Empty function name.
        lua_settop(mL, baseStackIndex);
        throw LUANonExistantFunction("Function not found in unregister.");
      }
    }
    else
    {
      token = fqName.substr(beg, string::npos);

      // Attempt to retrieve the function.
      if (depth == 0)
      {
        lua_getglobal(mL, token.c_str());
      }
      else
      {
        lua_getfield(mL, -1, token.c_str());
      }

      if (lua_isnil(mL, -1))
      {
        lua_settop(mL, baseStackIndex);
        throw LUANonExistantFunction("Function not found in unregister.");
      }

      if (isRegisteredFunction(lua_gettop(mL)))
      {
        // Remove the function from the top of the stack, we don't need it
        // anymore.
        lua_pop(mL, 1);
        if (depth == 0)
        {
          // Unregister from globals (just assign nil to the variable)
          // http://www.lua.org/pil/1.2.html
          lua_setglobal(mL, token.c_str());
        }
        else
        {
          // Unregister from parent table.
          lua_pushnil(mL);
          lua_setfield(mL, -2, token.c_str());
        }
      }
      else
      {
        lua_settop(mL, baseStackIndex);
        throw LUANonExistantFunction("Function not found in unregister.");
      }
    }

    ++depth;
  }

  lua_settop(mL, baseStackIndex);
}

//-----------------------------------------------------------------------------
void LuaScripting::doHooks(lua_State* L, int tableIndex)
{
  int stackTop = lua_gettop(L);
  int numArgs = stackTop - tableIndex;

  lua_checkstack(L, numArgs + 3);

  // Obtain the hooks table and iterate over it calling the lua closures.
  lua_getfield(L, tableIndex, TBL_MD_HOOKS);
  int hookTable = lua_gettop(L);

  lua_pushnil(L);
  while (lua_next(L, hookTable))
  {
    // The value at the top of the stack is the lua closure to call.
    // This call will automatically pop the function off the stack,
    // so we don't need a pop at the end of the loop

    // Push all of the arguments.
    for (int i = 0; i < numArgs; i++)
    {
      lua_pushvalue(L, tableIndex + i + 1);
    }
    lua_pcall(L, numArgs, 0, 0);
  }
  lua_pop(L, 1);  // Remove the hooks table.

  // Obtain the member function hooks table, and iterate over it.
  // XXX: Update to allow classes to register multiple hook for one function
  //      I don't see a need for this now, so I'm not implementing it.
  //      But a way to do it would be to make this member hooks table contain
  //      tables named after the member hook references, and index the
  //      function much like we index them in the hooks table above (with an
  //      index stored in the table).
  lua_getfield(L, tableIndex, TBL_MD_MEMBER_HOOKS);
  hookTable = lua_gettop(L);
  lua_pushnil(L);
  while (lua_next(L, hookTable))
  {
    // Push all of the arguments.
    for (int i = 0; i < numArgs; i++)
    {
      lua_pushvalue(L, tableIndex + i + 1);
    }
    lua_pcall(L, numArgs, 0, 0);
  }
  lua_pop(L, 1);  // Remove the member hooks table.

  assert(stackTop == lua_gettop(L));
}

//-----------------------------------------------------------------------------
std::string LuaScripting::getNewMemberHookID()
{
  ostringstream os;
  os << "mh" << mMemberHookIndex;
  ++mMemberHookIndex;
  return os.str();
}

//==============================================================================
//
// UNIT TESTING
//
//==============================================================================

#ifdef EXTERNAL_UNIT_TESTING

void printRegisteredFunctions(LuaScripting* s);

SUITE(TestLUAScriptingSystem)
{
  int dfun(int a, int b, int c)
  {
    return a + b + c;
  }


  TEST( TestDynamicModuleRegistration )
  {
    TEST_HEADER;

    auto_ptr<LuaScripting> sc(new LuaScripting());  // want to use unique_ptr
    lua_State* L = sc->getLUAState();

    // Test successful bindings and their results.
    sc->registerFunction(&dfun, "test.dummyFun", "My test dummy func.");
    sc->registerFunction(&dfun, "p1.p2.p3.dummy", "Test");
    sc->registerFunction(&dfun, "p1.p2.p.dummy", "Test");
    sc->registerFunction(&dfun, "p1.np.p3.p4.dummy", "Test");
    sc->registerFunction(&dfun, "test.dummyFun2", "Test");
    sc->registerFunction(&dfun, "test.test2.dummy", "Test");
    sc->registerFunction(&dfun, "func", "Test");

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

    luaL_loadstring(L, "return func(1,2,39)");
    lua_pcall(L, 0, LUA_MULTRET, 0);
    CHECK_EQUAL(42, lua_tointeger(L, -1));
    lua_pop(L, 1);

    // Test failure cases.

    // Check for appropriate exceptions.
    // XXX: We could make more exception classes whose names more closely
    // match the exceptions we are getting.

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

    //printRegisteredFunctions(sc.get());
  }


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
    TEST_HEADER;

    auto_ptr<LuaScripting> sc(new LuaScripting());  // want to use unique_ptr
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

//    printRegisteredFunctions(sc.get());
  }



  // Tests a series of functions closure metadata.
  TEST( TestClosureMetadata )
  {
    TEST_HEADER;

    auto_ptr<LuaScripting> sc(new LuaScripting());  // want to use unique_ptr
    lua_State* L = sc->getLUAState();

    sc->registerFunction(&str_int, "str.fint", "desc str_int");
    sc->registerFunction(&str_int2, "str.fint2", "desc str_int2");
    sc->registerFunction(&int_, "fint", "desc int_");
    sc->registerFunction(&print_flt, "print_flt", "Prints Floats");

    string exe;

    //------------------
    // Test description
    //------------------
    exe = string("return str.fint.") + LuaScripting::TBL_MD_DESC;
    luaL_dostring(L, exe.c_str());
    CHECK_EQUAL("desc str_int", lua_tostring(L, -1));
    lua_pop(L, 1);

    exe = string("return str.fint2.") + LuaScripting::TBL_MD_DESC;
    luaL_dostring(L, exe.c_str());
    CHECK_EQUAL("desc str_int2", lua_tostring(L, -1));
    lua_pop(L, 1);

    exe = string("return fint.") + LuaScripting::TBL_MD_DESC;
    luaL_dostring(L, exe.c_str());
    CHECK_EQUAL("desc int_", lua_tostring(L, -1));
    lua_pop(L, 1);

    exe = string("return print_flt.") + LuaScripting::TBL_MD_DESC;
    luaL_dostring(L, exe.c_str());
    CHECK_EQUAL("Prints Floats", lua_tostring(L, -1));
    lua_pop(L, 1);


    //----------------
    // Test signature
    //----------------
    exe = "return str.fint.";
    luaL_dostring(L, string(exe + LuaScripting::TBL_MD_SIG).c_str());
    CHECK_EQUAL("string (int)", lua_tostring(L, -1));
    lua_pop(L, 1);
    luaL_dostring(L, string(exe + LuaScripting::TBL_MD_SIG_NAME).c_str());
    CHECK_EQUAL("string fint(int)", lua_tostring(L, -1));
    lua_pop(L, 1);

    exe = "return str.fint2.";
    luaL_dostring(L, string(exe + LuaScripting::TBL_MD_SIG).c_str());
    CHECK_EQUAL("string (int, int)", lua_tostring(L, -1));
    lua_pop(L, 1);
    luaL_dostring(L, string(exe + LuaScripting::TBL_MD_SIG_NAME).c_str());
    CHECK_EQUAL("string fint2(int, int)", lua_tostring(L, -1));
    lua_pop(L, 1);

    exe = "return fint.";
    luaL_dostring(L, string(exe + LuaScripting::TBL_MD_SIG).c_str());
    CHECK_EQUAL("int ()", lua_tostring(L, -1));
    lua_pop(L, 1);
    luaL_dostring(L, string(exe + LuaScripting::TBL_MD_SIG_NAME).c_str());
    CHECK_EQUAL("int fint()", lua_tostring(L, -1));
    lua_pop(L, 1);

    exe = "return print_flt.";
    luaL_dostring(L, string(exe + LuaScripting::TBL_MD_SIG).c_str());
    CHECK_EQUAL("void (float)", lua_tostring(L, -1));
    lua_pop(L, 1);
    luaL_dostring(L, string(exe + LuaScripting::TBL_MD_SIG_NAME).c_str());
    CHECK_EQUAL("void print_flt(float)", lua_tostring(L, -1));
    lua_pop(L, 1);


    //------------------------------------------------------------------
    // Number of executions (simple value -- only testing one function)
    //------------------------------------------------------------------
    exe = string("return print_flt.")
        + LuaScripting::TBL_MD_NUM_EXEC;
    luaL_dostring(L, exe.c_str());
    CHECK_EQUAL(0, lua_tointeger(L, -1));
    lua_pop(L, 1);


    //------------------------------------------------------------
    // Qualified name (simple value -- only testing one function)
    //------------------------------------------------------------
    exe = string("return str.fint2.")
        + LuaScripting::TBL_MD_QNAME;
    luaL_dostring(L, exe.c_str());
    CHECK_EQUAL("str.fint2", lua_tostring(L, -1));
    lua_pop(L, 1);

    //---------------
    // Test defaults
    //---------------
    // Use a new function, 'mixer', to test the defaults table.
    sc->registerFunction(&mixer, "mixer", "Testing all parameter types");

    exe = string("return mixer.")
        + LuaScripting::TBL_MD_FUN_PDEFS;
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

    // No need to test last exec table, it is just a copy of defaults.

//    printRegisteredFunctions(sc.get());
  }

  TEST(Test_getAllFuncDescs)
  {
    TEST_HEADER;

    // Test retrieval of all function descriptions.
    auto_ptr<LuaScripting> sc(new LuaScripting());  // want to use unique_ptr
    lua_State* L = sc->getLUAState();

    sc->registerFunction(&str_int,            "str.int",            "Desc 1");
    sc->registerFunction(&str_int2,           "str2.int2",          "Desc 2");
    sc->registerFunction(&flt_flt2_int2_dbl2, "flt.flt2.int2.dbl2", "Desc 3");
    sc->registerFunction(&mixer,              "mixer",              "Desc 4");

    //
    std::vector<LuaScripting::FunctionDesc> d = sc->getAllFuncDescs();

    // Verify all of the function descriptions.
    // Since all of the functions are in different base tables, we can extract
    // them in the order that we registered them.
    // Otherwise, its determine by the hashing function used internally by
    // LUA (key/value pair association).
    CHECK_EQUAL("int", d[0].funcName.c_str());
    CHECK_EQUAL("Desc 1", d[0].funcDesc.c_str());
    CHECK_EQUAL("string int(int)", d[0].funcSig.c_str());

    CHECK_EQUAL("int2", d[1].funcName.c_str());
    CHECK_EQUAL("Desc 2", d[1].funcDesc.c_str());
    CHECK_EQUAL("string int2(int, int)", d[1].funcSig.c_str());

    CHECK_EQUAL("dbl2", d[2].funcName.c_str());
    CHECK_EQUAL("Desc 3", d[2].funcDesc.c_str());
    CHECK_EQUAL("float dbl2(float, float, int, int, double, double)",
                d[2].funcSig.c_str());

    CHECK_EQUAL("mixer", d[3].funcName.c_str());
    CHECK_EQUAL("Desc 4", d[3].funcDesc.c_str());
    CHECK_EQUAL("string mixer(bool, int, float, double, string)",
                d[3].funcSig.c_str());

//    printRegisteredFunctions(sc.get());
  }

  static int hook1Called = 0;
  static int hook1CallVal = 0;

  static int hook1aCalled = 0;
  static int hook1aCallVal = 0;

  static int hook2Called = 0;
  static int hook2CallVal1 = 0;
  static int hook2CallVal2 = 0;

  void myHook1(int a)
  {
    printf("Called my hook 1 with %d\n", a);
    ++hook1Called;
    hook1CallVal = a;
  }

  void myHook1a(int a)
  {
    printf("Called my hook 1a with %d\n", a);
    ++hook1aCalled;
    hook1aCallVal = a;
  }

  void myHook2(int a, int b)
  {
    printf("Called my hook 2 with %d %d\n", a, b);
    ++hook2Called;
    hook2CallVal1 = a;
    hook2CallVal2 = b;
  }

  TEST(StaticStrictHook)
  {
    TEST_HEADER;

    auto_ptr<LuaScripting> sc(new LuaScripting());
    lua_State* L = sc->getLUAState();

    sc->registerFunction(&str_int,            "func1", "Function 1");
    sc->registerFunction(&str_int2,           "a.func2", "Function 2");

    sc->strictHook(&myHook1, "func1");
    sc->strictHook(&myHook1, "func1");
    sc->strictHook(&myHook1a, "func1");
    sc->strictHook(&myHook2, "a.func2");

    // Test hooks on function 1 (the return value of hooks don't matter)
    luaL_dostring(L, "func1(23)");

    // Test hooks on function 2
    luaL_dostring(L, "a.func2(42, 53)");

    CHECK_EQUAL(2,  hook1Called);
    CHECK_EQUAL(23, hook1CallVal);
    CHECK_EQUAL(1,  hook1aCalled);
    CHECK_EQUAL(23, hook1aCallVal);
    CHECK_EQUAL(1,  hook2Called);
    CHECK_EQUAL(42, hook2CallVal1);
    CHECK_EQUAL(53, hook2CallVal2);

    // Test failure cases

    // Invalid function names
    CHECK_THROW(
        sc->strictHook(&myHook1, "func3"),
        LUANonExistantFunction);

    CHECK_THROW(
        sc->strictHook(&myHook2, "b.func2"),
        LUANonExistantFunction);

    // Incompatible function signatures
    CHECK_THROW(
        sc->strictHook(&myHook1, "a.func2"),
        LUAInvalidFunSignature);

    CHECK_THROW(
        sc->strictHook(&myHook1a, "a.func2"),
        LUAInvalidFunSignature);

    CHECK_THROW(
        sc->strictHook(&myHook2, "func1"),
        LUAInvalidFunSignature);
  }

  TEST(CallbackMethodsOnExec)
  {
    TEST_HEADER;

  }

  TEST(LastExecMetaPopulation)
  {
    TEST_HEADER;

  }

  TEST(Provenance)
  {
    TEST_HEADER;

  }

  TEST(UndoRedo)
  {
    TEST_HEADER;

  }
}

void printRegisteredFunctions(LuaScripting* s)
{
  vector<LuaScripting::FunctionDesc> regFuncs = s->getAllFuncDescs();

  printf("\n All registered functions \n");

  for (vector<LuaScripting::FunctionDesc>::iterator it = regFuncs.begin();
       it != regFuncs.end();
       ++it)
  {
    printf("\n  Function:     %s\n", it->funcName.c_str());
    printf("  Description:  %s\n", it->funcDesc.c_str());
    printf("  Signature:    %s\n", it->funcSig.c_str());
  }
}

#endif



} /* namespace tuvok */
