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

          To see examples of how to use the system, consult the unit tests at
          the bottom of LuaMemberReg.cpp and LuaScripting.cpp.
 */

#ifndef EXTERNAL_UNIT_TESTING

#include "Controller/Controller.h"
#include "3rdParty/LUA/lua.hpp"

#else

#include <assert.h>
#include "utestCommon.h"

#endif

#include <vector>
#include <algorithm>

#include "LUAScripting.h"
#include "LUAProvenance.h"
#include "LuaMemberRegUnsafe.h"
#include "LuaClassInstanceReg.h"

using namespace std;

#define QUALIFIED_NAME_DELIMITER  "."

// Disable "'this' used in base member initializer list warning"
// this is not referenced in either of the initialized classes,
// but is merely stored.
#pragma warning( disable : 4355 )

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
const char* LuaScripting::TBL_MD_CPP_CLASS      = "scriptingCPP";
const char* LuaScripting::TBL_MD_STACK_EXEMPT   = "stackExempt";
const char* LuaScripting::TBL_MD_PROV_EXEMPT    = "provExempt";
const char* LuaScripting::TBL_MD_NUM_PARAMS     = "numParams";
const char* LuaScripting::TBL_MD_UNDO_FUNC      = "undoHook";
const char* LuaScripting::TBL_MD_REDO_FUNC      = "redoHook";
const char* LuaScripting::TBL_MD_NULL_UNDO      = "nullUndo";
const char* LuaScripting::TBL_MD_NULL_REDO      = "nullRedo";

// To avoid naming conflicts with other libraries, we prefix all of our
// registry values with tuvok_
const char* LuaScripting::REG_EXPECTED_EXCEPTION_FLAG = "tuvok_exceptFlag";

#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
const char* LuaScripting::TBL_MD_TYPES_TABLE    = "typesTable";
#endif


//-----------------------------------------------------------------------------
LuaScripting::LuaScripting()
: mMemberHookIndex(0)
, mGlobalInstanceID(0)
, mGlobalTempInstRange(false)
, mGlobalTempInstLow(0)
, mGlobalTempInstHigh(0)
, mGlobalTempCurrent(0)
, mProvenance(new LuaProvenance(this))
, mMemberReg(new LuaMemberRegUnsafe(this))
{
  mL = lua_newstate(luaInternalAlloc, NULL);

  if (mL == NULL) throw LuaError("Failed to initialize LUA.");

  lua_atpanic(mL, &luaPanic);
  luaL_openlibs(mL);

  setExpectedExceptionFlag(false);

  registerScriptFunctions();

  mProvenance->registerLuaProvenanceFunctions();
}

//-----------------------------------------------------------------------------
LuaScripting::~LuaScripting()
{
  removeAllRegistrations();

  lua_close(mL);
}

//-----------------------------------------------------------------------------
void LuaScripting::removeAllRegistrations()
{
  // Class instances should be destroyed before removing functions
  // (deleteClass needs to be present when deleting instances).
  deleteAllClassInstances();
  unregisterAllFunctions();
}

#pragma warning( disable : 4702 )  // Unreachable code warning.
//-----------------------------------------------------------------------------
int LuaScripting::luaPanic(lua_State* L)
{
  // Note: We compile LUA as c plus plus, so we won't have problems throwing
  // exceptions from functions called by LUA. When not compiling as CPP, LUA
  // uses set_jmp and long_jmp. This can cause issues when exceptions are
  // thrown, see:
  // http://developers.sun.com/solaris/articles/mixing.html#except .

  ostringstream os;
  os << lua_tostring(L, -1);

  lua_getfield(L, LUA_REGISTRYINDEX,
                 LuaScripting::REG_EXPECTED_EXCEPTION_FLAG);
  bool isExpectingException = lua_toboolean(L, -1) ? true : false;
  if (isExpectingException == false)
  {
    // Even though we are in the Lua panic function, we can still use Lua to
    // log information and errors.
    ostringstream luaOut;
    luaOut << "log.error([==[" << os.str() << "]==])";  // Could also use [[ ]]
    string error = luaOut.str();
    luaL_dostring(L, error.c_str());
  }

  throw LuaError(os.str().c_str());

  // Returning from this function would mean that abort() gets called by LUA.
  // We don't want this.
  return 0;
}
#pragma warning(default:4702)  // Reenable unreachable code arning

//-----------------------------------------------------------------------------
void* LuaScripting::luaInternalAlloc(void* /*ud*/, void* ptr, size_t /*osize*/,
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
void LuaScripting::registerScriptFunctions()
{
  // Note: All these functions are provenance exempt because we do not want
  // to even see that they were run in the provenance log. There is no reason
  // include them.
  lua_pushnil(mL);
  lua_setglobal(mL, "print");

  mMemberReg->registerFunction(this, &LuaScripting::printHelp,
                               "help",
                               "Same as log.printFunctions with an additional "
                               "header.",
                               false);
  setProvenanceExempt("help");

  mMemberReg->registerFunction(this, &LuaScripting::deleteLuaClassInstance,
                               "deleteClass",
                               "Deletes a Lua class instance.",
                               true);
  setNullUndoFun("deleteClass");  // Undo doesn't do anything. Instance
                                  // cleanup is done inside of provenance.
                                  // All child undo items are still executed.

  mMemberReg->registerFunction(this, &LuaScripting::logInfo,
                               "print",
                               "Logs general information.",
                               false);
  setProvenanceExempt("print");

  mMemberReg->registerFunction(this, &LuaScripting::logInfo,
                               "log.info",
                               "Logs general information.",
                               false);
  setProvenanceExempt("log.info");

  mMemberReg->registerFunction(this, &LuaScripting::logWarn,
                               "log.warn",
                               "Logs general information.",
                               false);
  setProvenanceExempt("log.warn");

  mMemberReg->registerFunction(this, &LuaScripting::logError,
                                "log.error",
                                "Logs an error.",
                                false);
  setProvenanceExempt("log.error");

  mMemberReg->registerFunction(this, &LuaScripting::printFunctions,
                               "log.printFunctions",
                               "Prints all registered functions using "
                               "'log.info'.",
                               false);
  setProvenanceExempt("log.printFunctions");
}

//-----------------------------------------------------------------------------
void LuaScripting::logInfo(string log)
{
  // TODO: Add logging functionality for Tuvok.
#ifdef EXTERNAL_UNIT_TESTING
  cout << log << endl;
#else
  MESSAGE(log.c_str());
#endif
}

//-----------------------------------------------------------------------------
void LuaScripting::logWarn(string log)
{
  // TODO: Add logging functionality for Tuvok.
#ifdef EXTERNAL_UNIT_TESTING
  cout << "Warn: " << log << endl;
#else
  MESSAGE(log.c_str());
#endif
}

//-----------------------------------------------------------------------------
void LuaScripting::logError(string log)
{
  // TODO: Add logging functionality for Tuvok.
#ifdef EXTERNAL_UNIT_TESTING
  cout << "Error: " << log << endl;
#else
  T_ERROR(log.c_str());
#endif
}

//-----------------------------------------------------------------------------
void LuaScripting::printFunctions()
{
  vector<FunctionDesc> funcDescs = getAllFuncDescs();
  for (vector<FunctionDesc>::iterator it = funcDescs.begin();
      it != funcDescs.end(); ++it)
  {
    FunctionDesc d = *it;
    ostringstream os;
    os << "'" << d.funcFQName << "' " << d.funcDesc;
    ostringstream osUsage;
    osUsage << "    Usage: '"<< d.funcFQName << d.paramSig << "'";
    cexec("log.info", os.str());
    cexec("log.info", osUsage.str());
  }
}

//-----------------------------------------------------------------------------
void LuaScripting::printHelp()
{
  // We execute the commands from lua because there may be hooks on the logInfo
  // and logError functions.
  cexec("log.info", "");
  cexec("log.info", "------------------------------");
  cexec("log.info", "Tuvok Scripting Interface");
  cexec("log.info", "List of all functions follows");
  cexec("log.info", "------------------------------");
  cexec("log.info", "");

  printFunctions();
}

//-----------------------------------------------------------------------------
bool LuaScripting::isProvenanceEnabled() const
{
  return mProvenance->isEnabled();
}

//-----------------------------------------------------------------------------
void LuaScripting::enableProvenance(bool enable)
{
  mProvenance->setEnabled(enable);
}

//-----------------------------------------------------------------------------
void LuaScripting::unregisterAllFunctions()
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
  for (vector<string>::const_iterator it = mRegisteredGlobals.begin();
       it != mRegisteredGlobals.end(); ++it)
  {
    string global = (*it);
    lua_getglobal(mL, (*it).c_str());
    // Don't need to check if the top of the stack is nil. unregisterFunction
    // removes all global functions from mRegisteredGlobals.
    removeFunctionsFromTable(0, (*it).c_str());
    lua_pop(mL, 1);
  }

  mRegisteredGlobals.clear();
}

//-----------------------------------------------------------------------------
void LuaScripting::deleteAllClassInstances()
{
  // Iterate over the class instance table and call destroyClassInstanceTable
  // on all of the key values.
  LuaStackRAII _a(mL, 0);

  lua_getglobal(mL, LuaClassInstance::SYSTEM_TABLE);
  if (lua_isnil(mL, -1) == 1)
  {
    lua_pop(mL, 1);
    return;
  }
  lua_pop(mL, 1);

  // Place the class instance table on the top of the stack.
  {
    ostringstream os;
    os << "return " << LuaClassInstance::CLASS_INSTANCE_TABLE;
    luaL_dostring(mL, os.str().c_str());
  }

  int instTable = lua_gettop(mL);
  if (lua_isnil(mL, instTable) == 0)
  {
    // Push first key.
    lua_pushnil(mL);
    while (lua_next(mL, instTable))
    {
      // Check if the value is a table. If so, check to see if it is a function,
      // otherwise, recurse into the table.
      destroyClassInstanceTable(lua_gettop(mL));
      lua_pop(mL, 1);  // Pop value off stack.
    }

    // Push a new table and replace the old instance table with it
    // (permanently removing all residual instance tables).
    {
      ostringstream os;
      os << LuaClassInstance::CLASS_INSTANCE_TABLE << " = {}";
      luaL_dostring(mL, os.str().c_str());
    }

  }

  // Pop off the instance table (or nil).
  lua_pop(mL, 1);
}

//-----------------------------------------------------------------------------
void LuaScripting::destroyClassInstanceTable(int tableIndex)
{
  LuaStackRAII _a(mL, 0);

  lua_getmetatable(mL, tableIndex);
  int mt = lua_gettop(mL);

  // Pull the delete function from the table.
  lua_getfield(mL, mt, LuaClassInstance::MD_DEL_FUN);
  LuaClassInstanceReg::DelFunSig fun = reinterpret_cast<
      LuaClassInstanceReg::DelFunSig>(lua_touserdata(mL, -1));
  lua_pop(mL, 1);

  // Pull the instance pointer from the table.
  lua_getfield(mL, mt, LuaClassInstance::MD_INSTANCE);
  void* cls = lua_touserdata(mL, -1);
  lua_pop(mL, 1);

  lua_getfield(mL, mt, LuaClassInstance::MD_GLOBAL_INSTANCE_ID);
  int instID = lua_tointeger(mL, -1);
  lua_pop(mL, 1);

  // Remove metatable from the stack.
  lua_pop(mL, 1);

  // Call the delete function with the instance pointer.
  // Permanently removes the memory for our class.
  fun(this, instID, cls);
}

//-----------------------------------------------------------------------------
void LuaScripting::removeFunctionsFromTable(int parentTable,
                                            const char* tableName)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
  // Iterate over the first table on the stack.
  int tablePos = lua_gettop(mL);

  // Check to see if it is a registered function.
  if (isRegisteredFunction(-1))
  {
    // Only output function info its registered to us.
    if (isOurRegisteredFunction(-1))
    {
      if (parentTable == 0)
      {
        lua_pushnil(mL);
        lua_setglobal(mL, tableName);
      }
      else
      {
        lua_pushnil(mL);
        lua_setfield(mL, parentTable, tableName);
      }
    }

    // This was a function, not a table.
    return;
  }

  string nextTableName;

  // Push first key.
  lua_pushnil(mL);
  while (lua_next(mL, tablePos))
  {
    // Check if the value is a table. If so, check to see if it is a function,
    // otherwise, recurse into the table.
    int type = lua_type(mL, -1);

    if (type == LUA_TTABLE)
    {
      // Obtain the key value (we don't want to call lua_tostring on the key
      // used for lua_next. This will confuse lua_next).
      lua_pushvalue(mL, -2);
      nextTableName = lua_tostring(mL, -1);
      lua_pop(mL, 1);

      // Recurse into the table.
      lua_checkstack(mL, 4);
      removeFunctionsFromTable(tablePos, nextTableName.c_str());
    }

    // Pop value off of the stack in preparation for the next iteration.
    lua_pop(mL, 1);
  }
}

//-----------------------------------------------------------------------------
vector<LuaScripting::FunctionDesc> LuaScripting::getAllFuncDescs() const
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
                        // ¤3.7.3.1 -- Automatic storage duration.
                        // ¤6.7.2 -- Destroyed at the end of the block in
                        //           reverse order of creation.

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
  LuaStackRAII _a = LuaStackRAII(mL, 0);

  // Iterate over the first table on the stack.
  int tablePos = lua_gettop(mL);

  // Check to see if it is a registered function.
  if (isRegisteredFunction(-1))
  {
    // Only output function info its registered to us.
    if (isOurRegisteredFunction(-1))
    {
      // The name of the function is the 'key'.
      FunctionDesc desc;

      lua_getfield(mL, -1, TBL_MD_QNAME);
      desc.funcName = getUnqualifiedName(string(lua_tostring(mL, -1)));
      desc.funcFQName = string(lua_tostring(mL, -1));
      lua_pop(mL, 1);

      lua_getfield(mL, -1, TBL_MD_DESC);
      desc.funcDesc = lua_tostring(mL, -1);
      lua_pop(mL, 1);

      lua_getfield(mL, -1, TBL_MD_SIG_NAME);
      desc.funcSig = string(lua_tostring(mL, -1));
      lua_pop(mL, 1);

      lua_getfield(mL, -1, TBL_MD_SIG_NO_RET);
      desc.paramSig = string(lua_tostring(mL, -1));
      lua_pop(mL, 1);

      descs.push_back(desc);
    }

    // This was a function, not a table.
    return;
  }

  // Push first key.
  lua_pushnil(mL);
  while (lua_next(mL, tablePos))
  {
    // Check if the value is a table. If so, check to see if it is a function,
    // otherwise, recurse into the table.
    int type = lua_type(mL, -1);

    if (type == LUA_TTABLE)
    {
      // Recurse into the table.
      lua_checkstack(mL, 4);
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
  LuaStackRAII _a = LuaStackRAII(mL, 0, __FILE__, __LINE__);

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
        throw LuaFunBindError("Invalid function name. No function "
                              "name after trailing period.");
    }
    else
    {
      tokens.push_back(fqName.substr(beg, string::npos));
    }
  }

  if (tokens.size() == 0) throw LuaFunBindError("No function name specified.");

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
      // Only add it if it is NOT in the system table. The system table
      // stores class instances, and other accumulations of functions
      // that we do not want showing up in help.
      // We also will clean up the system table manually.
      if (token.compare(LuaClassInstance::SYSTEM_TABLE) != 0)
        mRegisteredGlobals.push_back(token);
    }
    else if (type == LUA_TTABLE)  // Other
    {
      if (isRegisteredFunction(-1))
      {
        throw LuaFunBindError("Can't register functions on top of other"
                              "functions.");
      }
    }
    else
    {
      throw LuaFunBindError("A module in the fully qualified name"
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
      throw LuaFunBindError("Unable to bind function closure."
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
        throw LuaFunBindError("Unable to bind function closure."
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
          throw LuaFunBindError("Can't register functions on top of other"
                                "functions.");
        }
      }
      else
      {
        throw LuaFunBindError("A module in the fully qualified name"
                              "not of type table.");
      }
    }
  }
}

//-----------------------------------------------------------------------------
bool LuaScripting::isOurRegisteredFunction(int stackIndex) const
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);

  // Extract the light user data that holds a pointer to the class that was
  // used to register this function.
  lua_getfield(mL, stackIndex, TBL_MD_CPP_CLASS);
  if (lua_isnil(mL, -1) == 0)
  {
    if (lua_touserdata(mL, -1) == this)
    {
      lua_pop(mL, 1);
      return true;
    }
  }

  lua_pop(mL, 1);
  return false;
}

//-----------------------------------------------------------------------------
bool LuaScripting::isRegisteredFunction(int stackIndex) const
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);

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
  LuaStackRAII _a = LuaStackRAII(mL, 1, __FILE__, __LINE__);

  // Table containing the function closure.
  lua_newtable(mL);

  // Create a new metatable
  lua_newtable(mL);

  // Push C Closure containing our function pointer onto the LUA stack.
  lua_pushlightuserdata(mL, realFuncToCall);
  lua_pushboolean(mL, 0);   // We are NOT a hook being called.
  // We are safe pushing this unprotected pointer: LuaScripting always
  // deregisters all functions it has registered, so no residual light
  // user data will be left in Lua.
  lua_pushlightuserdata(mL, static_cast<void*>(this));
  lua_pushcclosure(mL, proxyFunc, 3);

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
  LuaStackRAII _a = LuaStackRAII(mL, 0);

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

  lua_pushboolean(mL, 0);
  lua_setfield(mL, tableIndex, TBL_MD_STACK_EXEMPT);

  lua_pushboolean(mL, 0);
  lua_setfield(mL, tableIndex, TBL_MD_PROV_EXEMPT);

  // Ensure our class is present as a light user data.
  // In this way, we can identify our own functions, and our functions
  // can modify state (such as provenance).
  lua_pushlightuserdata(mL, this);
  lua_setfield(mL, tableIndex, TBL_MD_CPP_CLASS);

  assert(top == lua_gettop(mL));
}

//-----------------------------------------------------------------------------
void LuaScripting::createDefaultsAndLastExecTables(int tableIndex,
                                                   int numFunParams)
{
  LuaStackRAII _a = LuaStackRAII(mL, -numFunParams);

  int firstParamPos = (lua_gettop(mL) - numFunParams) + 1;

  // Create defaults table.
  lua_newtable(mL);
  int defTablePos = lua_gettop(mL);

  copyParamsToTable(defTablePos, firstParamPos, numFunParams);

  // Insert defaults table in closure table.
  lua_pushstring(mL, TBL_MD_FUN_PDEFS);
  lua_pushvalue(mL, defTablePos);
  lua_settable(mL, tableIndex);

  // Pop the defaults table.
  lua_pop(mL, 1);

  // Remove parameters from the stack.
  lua_pop(mL, numFunParams);

  copyDefaultsTableToLastExec(tableIndex);
}

//-----------------------------------------------------------------------------
void LuaScripting::copyParamsToTable(int tableIndex,
                                     int paramStartIndex,
                                     int numParams)
{
  // Push table onto the top of the stack.
  // This is why you shouldn't use psuedo indices for paramStartIndex.
  for (int i = 0; i < numParams; i++)
  {
    int stackIndex = paramStartIndex + i;
    lua_pushinteger(mL, i);
    lua_pushvalue(mL, stackIndex);
    lua_settable(mL, tableIndex);
  }
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
      {
        lua_settop(mL, baseStackIndex);
        return false;
      }

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
        throw LuaNonExistantFunction("Function not found in unregister.");
      }

      if (beg == fqName.size())
      {
        // Empty function name.
        lua_settop(mL, baseStackIndex);
        throw LuaNonExistantFunction("Function not found in unregister.");
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
        throw LuaNonExistantFunction("Function not found in unregister.");
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

          // Also remove from mRegisteredGlobals.
          mRegisteredGlobals.erase(remove(mRegisteredGlobals.begin(),
                                          mRegisteredGlobals.end(),
                                          fqName),
                                   mRegisteredGlobals.end());
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
        throw LuaNonExistantFunction("Function not found in unregister.");
      }
    }

    ++depth;
  }

  lua_settop(mL, baseStackIndex);
}

//-----------------------------------------------------------------------------
void LuaScripting::doHooks(lua_State* L, int tableIndex, bool provExempt)
{
  int stackTop = lua_gettop(L);
  int numArgs = stackTop - tableIndex;

  lua_checkstack(L, numArgs + 3);

  int numStaticHooks = 0;

  // Obtain the hooks table and iterate over it calling the lua closures.
  lua_getfield(L, tableIndex, TBL_MD_HOOKS);
  int hookTable = lua_gettop(L);

  try
  {
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

      ++numStaticHooks;
    }
    lua_pop(L, 1);  // Remove the hooks table.
  }
  catch (exception& e)
  {
    ostringstream os;
    os << " Static Hook: " << e.what();
    logExecFailure(os.str());
    throw;
  }
  catch (...)
  {
    ostringstream os;
    os << " Static Hook";
    logExecFailure(os.str());
    throw;
  }

  int numMemberHooks = 0;

  // Obtain the member function hooks table, and iterate over it.
  // XXX: Update to allow classes to register multiple hook for one function
  //      I don't see a need for this now, so I'm not implementing it.
  //      But a way to do it would be to make this member hooks table contain
  //      tables named after the member hook references, and index the
  //      function much like we index them in the hooks table above (with an
  //      index stored in the table).
  lua_getfield(L, tableIndex, TBL_MD_MEMBER_HOOKS);
  hookTable = lua_gettop(L);

  try
  {
    lua_pushnil(L);
    while (lua_next(L, hookTable))
    {
      // Push all of the arguments.
      for (int i = 0; i < numArgs; i++)
      {
        lua_pushvalue(L, tableIndex + i + 1);
      }
      lua_pcall(L, numArgs, 0, 0);

      ++numMemberHooks;
    }
    lua_pop(L, 1);  // Remove the member hooks table.
  }
  catch (exception& e)
  {
    ostringstream os;
    os << " Member Hook: " << e.what();
    logExecFailure(os.str());
    throw;
  }
  catch (...)
  {
    ostringstream os;
    os << " Member Hook";
    logExecFailure(os.str());
    throw;
  }

  int totalHooks = numStaticHooks + numMemberHooks;
  if (totalHooks > 0 && provExempt == false)
    mProvenance->logHooks(numStaticHooks, numMemberHooks);

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

//-----------------------------------------------------------------------------
bool LuaScripting::doProvenanceFromExec(lua_State* L,
                            std::tr1::shared_ptr<LuaCFunAbstract> funParams,
                            std::tr1::shared_ptr<LuaCFunAbstract> emptyParams)
{
  if (mProvenance->isEnabled())
  {
    // Obtain fully qualified function name (doProvenanceFromExec is executed
    // from the context of the exec function in one of the LuaCallback structs).
    lua_getfield(L, 1, LuaScripting::TBL_MD_QNAME);
    std::string fqName = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, LuaScripting::TBL_MD_STACK_EXEMPT);
    bool stackExempt = lua_toboolean(L, -1) ? true : false;
    lua_pop(L, 1);

    lua_getfield(L, 1, LuaScripting::TBL_MD_PROV_EXEMPT);
    bool provExempt = lua_toboolean(L, -1) ? true : false;
    lua_pop(L, 1);

    if (provExempt == false)
    {
      // Execute provenace.
      mProvenance->logExecution(fqName,
                                stackExempt,
                                funParams,
                                emptyParams);
    }

    return provExempt;
  }

  return true;
}

//-----------------------------------------------------------------------------
void LuaScripting::setUndoRedoStackExempt(const string& funcName)
{
  lua_State* L = mL;
  getFunctionTable(funcName);

  lua_pushboolean(L, 1);
  lua_setfield(L, -2, TBL_MD_STACK_EXEMPT);

  // Remove tables that are usually associated with undo/redo functionality.
  lua_pushnil(L);
  lua_setfield(L, -2, TBL_MD_FUN_PDEFS);

  lua_pushnil(L);
  lua_setfield(L, -2, TBL_MD_FUN_LAST_EXEC);

  // Pop off the function table.
  lua_pop(L, 1);
}

//-----------------------------------------------------------------------------
void LuaScripting::setProvenanceExempt(const std::string& fqName)
{
  setUndoRedoStackExempt(fqName);

  lua_State* L = mL;
  getFunctionTable(fqName);

  lua_pushboolean(L, 1);
  lua_setfield(L, -2, TBL_MD_PROV_EXEMPT);

  // Pop off the function table.
  lua_pop(L, 1);
}

//-----------------------------------------------------------------------------
void LuaScripting::copyDefaultsTableToLastExec(int funTableIndex)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);

  // Push a copy of the defaults table onto the stack.
  lua_getfield(mL, funTableIndex, TBL_MD_FUN_PDEFS);
  int defTablePos = lua_gettop(mL);

  // Do a deep copy of the defaults table.
  // If we don't do this, we push another reference of the defaults table
  // instead of a deep copy of the table.
  lua_newtable(mL);
  int lastExecTablePos = lua_gettop(mL);

  lua_pushnil(mL);  // First key
  // We use lua_next because order is not important. Just getting the key
  // value pairs is important.
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
  lua_pushvalue(mL, lastExecTablePos);
  lua_settable(mL, funTableIndex);

  lua_pop(mL, 2);   // Pop the last-exec and the default tables.
}

//-----------------------------------------------------------------------------
void LuaScripting::prepForExecution(const std::string& fqName)
{
  getFunctionTable(fqName);
  lua_getmetatable(mL, -1);
  lua_getfield(mL, -1, "__call");

  // Remove metatable.
  lua_remove(mL, lua_gettop(mL) - 1);

  // Push a reference of the function table. This will be the first parameter
  // to the function we call.
  lua_pushvalue(mL, -2);

  // Remove the function table we pushed with getFunctionTable.
  lua_remove(mL, lua_gettop(mL) - 2);
}

//-----------------------------------------------------------------------------
void LuaScripting::executeFunctionOnStack(int nparams, int nret)
{
  // - 2 because we have the function table as a transparent parameter
  // and lua_call will also pop the function off the stack.
  LuaStackRAII _a = LuaStackRAII(mL, -nparams - 2 + nret);
  // + 1 is for the function table that was pushed by prepForExecution.
  lua_call(mL, nparams + 1, nret);
}

//-----------------------------------------------------------------------------
void LuaScripting::exec(const std::string& cmd)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
  luaL_loadstring(mL, cmd.c_str());
  lua_call(mL, 0, 0);
}

//-----------------------------------------------------------------------------
void LuaScripting::cexec(const std::string& cmd)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
  prepForExecution(cmd);
  executeFunctionOnStack(0, 0);
}

//-----------------------------------------------------------------------------
void LuaScripting::resetFunDefault(int argumentPos, int ftableStackPos)
{
  LuaStackRAII _a = LuaStackRAII(mL, -1);

  int valPos = lua_gettop(mL);
  lua_getfield(mL, ftableStackPos, TBL_MD_FUN_PDEFS);
  int defs = lua_gettop(mL);
  lua_getfield(mL, ftableStackPos, TBL_MD_FUN_LAST_EXEC);
  int exec = lua_gettop(mL);

  lua_pushinteger(mL, argumentPos);
  lua_pushvalue(mL, valPos);
  lua_settable(mL, defs);

  lua_pushinteger(mL, argumentPos);
  lua_pushvalue(mL, valPos);
  lua_settable(mL, exec);

  lua_pop(mL, 3); // Pop the defaults table, last exec table, and value at
                  // top of the stack.
}

//-----------------------------------------------------------------------------
void LuaScripting::logExecFailure(const std::string& failure)
{
  ostringstream os;
  os << " -- FAILED";
  if (failure.size() > 0)
  {
    os << ": " << failure;
  }
  mProvenance->ammendLastProvLog(os.str());
}

//-----------------------------------------------------------------------------
void LuaScripting::setExpectedExceptionFlag(bool expected)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);
  lua_pushboolean(mL, expected ? 1 : 0);
  lua_setfield(mL, LUA_REGISTRYINDEX,
               LuaScripting::REG_EXPECTED_EXCEPTION_FLAG);
}

//-----------------------------------------------------------------------------
void LuaScripting::setTempProvDisable(bool disable)
{
  mProvenance->setDisableProvTemporarily(disable);
}

//-----------------------------------------------------------------------------
void LuaScripting::beginCommand()
{
  mProvenance->beginCommand();
}

//-----------------------------------------------------------------------------
void LuaScripting::endCommand()
{
  mProvenance->endCommand();
}

//-----------------------------------------------------------------------------
void LuaScripting::addLuaClassDef(ClassDefFun f, const std::string fqName)
{
  // Build constructor into the Lua class table (at fqName).
  // (Setup appropriate LuaClassInstanceReg to grab constructor).
  LuaClassInstanceReg reg(this, fqName, f);

  // Call class definition function.
  // This class will construct an appropriate class instance table using the
  // constructor given in f.
  f(reg);

  // Populate the 'new' function's table with the class definition function,
  // and the full factory name.
  ostringstream retNewFun;
  retNewFun << "return " << fqName << ".new";
  luaL_dostring(mL, retNewFun.str().c_str());

  lua_pushlightuserdata(mL, reinterpret_cast<void*>(f));
  lua_setfield(mL, -2, LuaClassInstanceReg::CONS_MD_CLASS_DEFINITION);

  lua_pushstring(mL, fqName.c_str());
  lua_setfield(mL, -2, LuaClassInstanceReg::CONS_MD_FACTORY_NAME);

  // Pop the new function table.
  lua_pop(mL, 1);

}

//-----------------------------------------------------------------------------
void LuaScripting::deleteLuaClassInstance(LuaClassInstance inst)
{
  LuaStackRAII _a(mL, 0);

  if (getFunctionTable(inst.fqName()))
  {
    destroyClassInstanceTable(lua_gettop(mL));

    // Erase the class instance.
    {
      ostringstream os;
      os << inst.fqName() << " = nil";
      luaL_dostring(mL, os.str().c_str());
    }

    // Pop the class instance table.
    lua_pop(mL, 1);
  }
}

//-----------------------------------------------------------------------------
int LuaScripting::getNewClassInstID()
{
  if (mGlobalTempInstRange)
  {
    int ret = mGlobalTempCurrent;
    ++mGlobalTempCurrent;
    if (mGlobalTempCurrent > mGlobalTempInstHigh)
    {
      mGlobalTempInstRange = false;
    }
    return ret;
  }
  else
  {
    return mGlobalInstanceID++;
  }
}

//-----------------------------------------------------------------------------
void LuaScripting::setNextTempClassInstRange(int low, int high)
{
  mGlobalTempInstRange = true;
  mGlobalTempInstLow = low;
  mGlobalTempInstHigh = high;
  mGlobalTempCurrent = low;
}


//-----------------------------------------------------------------------------
void LuaScripting::setNullUndoFun(const std::string& name)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);

  // Need to check the signature of the function that we are trying to bind
  // into the script system.
  if (getFunctionTable(name) == false)
  {
    throw LuaNonExistantFunction("Unable to find function with which to"
                                 "associate a null undo function.");
  }

  lua_pushboolean(mL, 1);
  lua_setfield(mL, -2, TBL_MD_NULL_UNDO);

  lua_pop(mL, 1);
}

//-----------------------------------------------------------------------------
void LuaScripting::setNullRedoFun(const std::string& name)
{
  LuaStackRAII _a = LuaStackRAII(mL, 0);

  // Need to check the signature of the function that we are trying to bind
  // into the script system.
  if (getFunctionTable(name) == false)
  {
    throw LuaNonExistantFunction("Unable to find function with which to"
                                 "associate a null redo function.");
  }

  lua_pushboolean(mL, 1);
  lua_setfield(mL, -2, TBL_MD_NULL_REDO);

  lua_pop(mL, 1);
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

    // Test successful bindings and their results.
    sc->registerFunction(&dfun, "test.dummyFun", "My test dummy func.", true);
    sc->registerFunction(&dfun, "p1.p2.p3.dummy", "Test", true);
    sc->registerFunction(&dfun, "p1.p2.p.dummy", "Test", true);
    sc->registerFunction(&dfun, "p1.np.p3.p4.dummy", "Test", true);
    sc->registerFunction(&dfun, "test.dummyFun2", "Test", true);
    sc->registerFunction(&dfun, "test.test2.dummy", "Test", true);
    sc->registerFunction(&dfun, "func", "Test", true);

    CHECK_EQUAL(42, sc->execRet<int>("test.dummyFun(1,2,39)"));
    CHECK_EQUAL(42, sc->execRet<int>("p1.p2.p3.dummy(1,2,39)"));
    CHECK_EQUAL(65, sc->execRet<int>("p1.p2.p.dummy(5,21,39)"));
    CHECK_EQUAL(42, sc->execRet<int>("p1.np.p3.p4.dummy(1,2,39)"));
    CHECK_EQUAL(42, sc->execRet<int>("test.dummyFun2(1,2,39)"));
    CHECK_EQUAL(42, sc->execRet<int>("test.test2.dummy(1,2,39)"));
    CHECK_EQUAL(42, sc->execRet<int>("func(1,2,39)"));

    // Test failure cases.

    // Check for appropriate exceptions.
    // XXX: We could make more exception classes whose names more closely
    // match the exceptions we are getting.

    // Exception: No trailing name after period.
    sc->setExpectedExceptionFlag(true);
    CHECK_THROW(
        sc->registerFunction(&dfun, "err.err.dummyFun.", "Func.", true),
        LuaFunBindError);

    // Exception: Duplicate name already exists in globals.
    CHECK_THROW(
        sc->registerFunction(&dfun, "p1", "Func.", true),
        LuaFunBindError);

    // Exception: Duplicate name already exists at last descendant.
    CHECK_THROW(
        sc->registerFunction(&dfun, "p1.p2", "Func.", true),
        LuaFunBindError);

    // Exception: A module in the fully qualified name not of type table.
    // (descendant case).
    CHECK_THROW(
        sc->registerFunction(&dfun, "test.dummyFun.Func", "Func.", true),
        LuaFunBindError);

    // Exception: A module in the fully qualified name not of type table.
    // (global case).
    CHECK_THROW(
        sc->registerFunction(&dfun, "func.Func2", "Func.", true),
        LuaFunBindError);
    sc->setExpectedExceptionFlag(false);
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

    sc->registerFunction(&str_int, "str.int", "", true);
    sc->registerFunction(&str_int2, "str.int2", "", true);
    sc->registerFunction(&flt_flt2_int2_dbl2, "flt.flt2.int2.dbl2", "", true);
    sc->registerFunction(&mixer, "mixer", "", true);

    CHECK_EQUAL("(97)",     sc->execRet<string>("str.int(97)").c_str());
    CHECK_EQUAL("(978,42)", sc->execRet<string>("str.int2(978, 42)").c_str());
    CHECK_EQUAL("My sTrIng 1 10 12.6 392.9",
                sc->execRet<string>("mixer(true, 10, 12.6, 392.9, 'My sTrIng')")
                  .c_str());
    CHECK_CLOSE(30.0f,
                sc->execRet<float>("flt.flt2.int2.dbl2(2,2,1,4,5,5)"),
                0.0001f);
  }



  // Tests a series of functions closure metadata.
  TEST( TestClosureMetadata )
  {
    TEST_HEADER;

    auto_ptr<LuaScripting> sc(new LuaScripting());  // want to use unique_ptr

    sc->registerFunction(&str_int, "str.fint", "desc str_int", true);
    sc->registerFunction(&str_int2, "str.fint2", "desc str_int2", true);
    sc->registerFunction(&int_, "fint", "desc int_", true);
    sc->registerFunction(&print_flt, "print_flt", "Prints Floats", true);

    string exe;

    // The following sections exploit lua_call's ability to 'execute' variables.
    // The result is the variable itself (if 1+ returns or LUA_MULTRET).
    // We are using our internal function result evaluation methods (execRet)
    // to evaluate and check the types of variables coming out of lua.

    //------------------
    // Test description
    //------------------
    exe = string("str.fint.") + LuaScripting::TBL_MD_DESC;
    CHECK_EQUAL("desc str_int", sc->execRet<string>(exe).c_str());

    exe = string("str.fint2.") + LuaScripting::TBL_MD_DESC;
    CHECK_EQUAL("desc str_int2", sc->execRet<string>(exe).c_str());

    exe = string("fint.") + LuaScripting::TBL_MD_DESC;
    CHECK_EQUAL("desc int_", sc->execRet<string>(exe).c_str());

    exe = string("print_flt.") + LuaScripting::TBL_MD_DESC;
    CHECK_EQUAL("Prints Floats", sc->execRet<string>(exe).c_str());

    //----------------
    // Test signature
    //----------------
    std::string temp;

    exe = "str.fint.";
    temp = string(exe + LuaScripting::TBL_MD_SIG);
    CHECK_EQUAL("string (int)", sc->execRet<string>(temp).c_str());
    temp = string(exe + LuaScripting::TBL_MD_SIG_NAME);
    CHECK_EQUAL("string fint(int)", sc->execRet<string>(temp).c_str());

    exe = "str.fint2.";
    temp = string(exe + LuaScripting::TBL_MD_SIG);
    CHECK_EQUAL("string (int, int)", sc->execRet<string>(temp).c_str());
    temp = string(exe + LuaScripting::TBL_MD_SIG_NAME);
    CHECK_EQUAL("string fint2(int, int)", sc->execRet<string>(temp).c_str());

    exe = "fint.";
    temp = string(exe + LuaScripting::TBL_MD_SIG);
    CHECK_EQUAL("int ()", sc->execRet<string>(temp).c_str());
    temp = string(exe + LuaScripting::TBL_MD_SIG_NAME);
    CHECK_EQUAL("int fint()", sc->execRet<string>(temp).c_str());

    exe = "print_flt.";
    temp = string(exe + LuaScripting::TBL_MD_SIG);
    CHECK_EQUAL("void (float)", sc->execRet<string>(temp).c_str());
    temp = string(exe + LuaScripting::TBL_MD_SIG_NAME);
    CHECK_EQUAL("void print_flt(float)", sc->execRet<string>(temp).c_str());

    //------------------------------------------------------------------
    // Number of executions (simple value -- only testing one function)
    //------------------------------------------------------------------
    exe = string("print_flt.") + LuaScripting::TBL_MD_NUM_EXEC;
    CHECK_EQUAL(0, sc->execRet<int>(exe));

    //------------------------------------------------------------
    // Qualified name (simple value -- only testing one function)
    //------------------------------------------------------------
    exe = string("str.fint2.") + LuaScripting::TBL_MD_QNAME;
    CHECK_EQUAL("str.fint2", sc->execRet<string>(exe).c_str());
  }

  TEST(Test_getAllFuncDescs)
  {
    TEST_HEADER;

    // Test retrieval of all function descriptions.
    auto_ptr<LuaScripting> sc(new LuaScripting());  // want to use unique_ptr

    sc->registerFunction(&str_int,            "str.int",            "Desc 1",
                         true);
    sc->registerFunction(&str_int2,           "str2.int2",          "Desc 2",
                         true);
    sc->registerFunction(&flt_flt2_int2_dbl2, "flt.flt2.int2.dbl2", "Desc 3",
                         true);
    sc->registerFunction(&mixer,              "mixer",              "Desc 4",
                         true);

    //
    std::vector<LuaScripting::FunctionDesc> d = sc->getAllFuncDescs();

    // We want to skip all of the subsystems that were registered when
    // LuaScripting is created (like provenance).
    // So we just look at the size of d, and use that as an indexing
    // mechanism.
    int ds = d.size();
    int i;

    // Verify all of the function descriptions.
    // Since all of the functions are in different base tables, we can extract
    // them in the order that we registered them.
    // Otherwise, its determine by the hashing function used internally by
    // LUA (key/value pair association).
    i = ds - 4;
    CHECK_EQUAL("int", d[i].funcName.c_str());
    CHECK_EQUAL("Desc 1", d[i].funcDesc.c_str());
    CHECK_EQUAL("string int(int)", d[i].funcSig.c_str());

    i = ds - 3;
    CHECK_EQUAL("int2", d[i].funcName.c_str());
    CHECK_EQUAL("Desc 2", d[i].funcDesc.c_str());
    CHECK_EQUAL("string int2(int, int)", d[i].funcSig.c_str());

    i = ds - 2;
    CHECK_EQUAL("dbl2", d[i].funcName.c_str());
    CHECK_EQUAL("Desc 3", d[i].funcDesc.c_str());
    CHECK_EQUAL("float dbl2(float, float, int, int, double, double)",
                d[i].funcSig.c_str());

    i = ds - 1;
    CHECK_EQUAL("mixer", d[i].funcName.c_str());
    CHECK_EQUAL("Desc 4", d[i].funcDesc.c_str());
    CHECK_EQUAL("string mixer(bool, int, float, double, string)",
                d[i].funcSig.c_str());

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

    sc->registerFunction(&str_int,            "func1", "Function 1", true);
    sc->registerFunction(&str_int2,           "a.func2", "Function 2", true);

    sc->strictHook(&myHook1, "func1");
    sc->strictHook(&myHook1, "func1");
    sc->strictHook(&myHook1a, "func1");
    sc->strictHook(&myHook2, "a.func2");

    // Test hooks on function 1 (the return value of hooks don't matter)
    sc->exec("func1(23)");

    // Test hooks on function 2
    sc->exec("a.func2(42, 53)");

    CHECK_EQUAL(2,  hook1Called);
    CHECK_EQUAL(23, hook1CallVal);
    CHECK_EQUAL(1,  hook1aCalled);
    CHECK_EQUAL(23, hook1aCallVal);
    CHECK_EQUAL(1,  hook2Called);
    CHECK_EQUAL(42, hook2CallVal1);
    CHECK_EQUAL(53, hook2CallVal2);

    // Test failure cases

    // Invalid function names
    sc->setExpectedExceptionFlag(true);
    CHECK_THROW(
        sc->strictHook(&myHook1, "func3"),
        LuaNonExistantFunction);

    CHECK_THROW(
        sc->strictHook(&myHook2, "b.func2"),
        LuaNonExistantFunction);

    // Incompatible function signatures
    CHECK_THROW(
        sc->strictHook(&myHook1, "a.func2"),
        LuaInvalidFunSignature);

    CHECK_THROW(
        sc->strictHook(&myHook1a, "a.func2"),
        LuaInvalidFunSignature);

    CHECK_THROW(
        sc->strictHook(&myHook2, "func1"),
        LuaInvalidFunSignature);
    sc->setExpectedExceptionFlag(false);
  }

  static int    i1  = 0;
  static string s1  = "nop";
  static bool   b1  = false;

  static void   set_i1(int a)     {i1 = a;}
  static void   set_s1(string s)  {s1 = s;}
  static void   set_b1(bool a)    {b1 = a;}

  static int    get_i1()          {return i1;}
  static string get_s1()          {return s1;}
  static bool   get_b1()          {return b1;}

  static void   paste_i1()        {i1 = 25;}

  int ti1 = 0, ti2 = 0, ti3 = 0, ti4 = 0, ti5 = 0, ti6 = 0;
  static void set_1ti(int i1)     {ti1 = i1;}
  static void set_2ti(int i1, int i2)
  { ti1 = i1; ti2 = i2 ;}
  static void set_3ti(int i1, int i2, int i3)
  { ti1 = i1; ti2 = i2; ti3 = i3; }
  static void set_4ti(int i1, int i2, int i3, int i4)
  { ti1 = i1; ti2 = i2; ti3 = i3; ti4 = i4; }
  static void set_5ti(int i1, int i2, int i3, int i4, int i5)
  { ti1 = i1; ti2 = i2; ti3 = i3; ti4 = i4; ti5 = i5;}
  static void set_6ti(int i1, int i2, int i3, int i4, int i5, int i6)
  { ti1 = i1; ti2 = i2; ti3 = i3; ti4 = i4; ti5 = i5; ti6 = i6;}

  static string testParamReturn(int a, bool b, float c, string s)
  {
    ostringstream os;
    os << "Out: " << a << " " << b << " " << c << " " << s;
    return os.str();
  }

  TEST(CallingLuaScript)
  {
    TEST_HEADER;

    auto_ptr<LuaScripting> sc(new LuaScripting());

    sc->registerFunction(&set_i1, "set_i1", "", true);
    sc->registerFunction(&set_s1, "set_s1", "", true);
    sc->registerFunction(&set_b1, "set_b1", "", true);
    sc->registerFunction(&paste_i1, "paste_i1", "", true);

    sc->registerFunction(&get_i1, "get_i1", "", false);
    sc->registerFunction(&get_s1, "get_s1", "", false);
    sc->registerFunction(&get_b1, "get_b1", "", false);

    // Test execute function and executeRet function.
    sc->exec("set_i1(34)");
    CHECK_EQUAL(34, i1);
    sc->exec("provenance.undo()");
    CHECK_EQUAL(0, i1);

    CHECK_EQUAL(0, sc->execRet<int>("get_i1()"));
    sc->exec("set_i1(34)");
    CHECK_EQUAL(34, sc->execRet<int>("get_i1()"));
    sc->exec("set_s1('My String')");
    CHECK_EQUAL("My String", s1.c_str());
    CHECK_EQUAL("My String", sc->execRet<string>("get_s1()").c_str());

    // Test out c++ parameter execution
    sc->registerFunction(&set_1ti, "set_1ti", "", true);
    sc->registerFunction(&set_2ti, "set_2ti", "", true);
    sc->registerFunction(&set_3ti, "set_3ti", "", true);
    sc->registerFunction(&set_4ti, "set_4ti", "", true);
    sc->registerFunction(&set_5ti, "set_5ti", "", true);
    sc->registerFunction(&set_6ti, "set_6ti", "", true);

    // No parameter versions.
    sc->cexec("paste_i1");
    CHECK_EQUAL(25, sc->cexecRet<int>("get_i1"));

    // 1 parameter.
    sc->cexec("set_1ti", 10);
    CHECK_EQUAL(10, ti1);

    // 2 parameters
    sc->cexec("set_2ti", 20, 22);
    CHECK_EQUAL(20, ti1);
    CHECK_EQUAL(22, ti2);

    // 3 parameters
    sc->cexec("set_3ti", 30, 32, 34);
    CHECK_EQUAL(30, ti1);
    CHECK_EQUAL(32, ti2);
    CHECK_EQUAL(34, ti3);

    // 4 parameters
    sc->cexec("set_4ti", 40, 42, 44, 46);
    CHECK_EQUAL(40, ti1);
    CHECK_EQUAL(42, ti2);
    CHECK_EQUAL(44, ti3);
    CHECK_EQUAL(46, ti4);

    // 5 parameters
    sc->cexec("set_5ti", 50, 52, 54, 56, 58);
    CHECK_EQUAL(50, ti1);
    CHECK_EQUAL(52, ti2);
    CHECK_EQUAL(54, ti3);
    CHECK_EQUAL(56, ti4);
    CHECK_EQUAL(58, ti5);

    // 6 parameters
    sc->cexec("set_6ti", 60, 62, 64, 66, 68, 70);
    CHECK_EQUAL(60, ti1);
    CHECK_EQUAL(62, ti2);
    CHECK_EQUAL(64, ti3);
    CHECK_EQUAL(66, ti4);
    CHECK_EQUAL(68, ti5);
    CHECK_EQUAL(70, ti6);

    // Multiple parameters, and 1 return value.
    sc->registerFunction(&testParamReturn, "tpr", "", true);
    CHECK_EQUAL("Out: 65 1 4.3 str!",
                sc->cexecRet<string>("tpr", 65, true, 4.3f, "str!").c_str());
  }

  TEST(TestDefaultSettings)
  {
    TEST_HEADER;

    auto_ptr<LuaScripting> sc(new LuaScripting());

    sc->registerFunction(&set_i1, "set_i1", "", true);
    sc->setDefaults("set_i1", 40, true);
    sc->registerFunction(&set_s1, "set_s1", "", true);
    sc->setDefaults("set_s1", "s1", true);
    sc->registerFunction(&set_b1, "set_b1", "", true);
    sc->setDefaults("set_b1", true, true);
    sc->registerFunction(&paste_i1, "paste_i1", "", true);

    // Ensure we don't have anything on the undo stack already.
    // (tests to make sure the provenance system was disabled when a call
    //  was made to registered functions inside of setDefaults).
    sc->setExpectedExceptionFlag(true);
    CHECK_THROW(sc->exec("provenance.undo()"), LuaProvenanceInvalidUndo);
    sc->setExpectedExceptionFlag(false);

    CHECK_EQUAL(40, i1);
    CHECK_EQUAL("s1", s1.c_str());
    CHECK_EQUAL(true, b1);

    sc->cexec("set_i1", 42);
    CHECK_EQUAL(42, i1);
    sc->cexec("set_b1", false);
    CHECK_EQUAL(false, b1);
    sc->cexec("set_s1", "new");
    CHECK_EQUAL("new", s1.c_str());

    sc->exec("provenance.undo()");
    CHECK_EQUAL("s1", s1.c_str());
    sc->exec("provenance.undo()");
    CHECK_EQUAL(true, b1);
    sc->exec("provenance.undo()");
    CHECK_EQUAL(40, i1);

    sc->exec("provenance.redo()");
    CHECK_EQUAL(42, i1);
    sc->exec("provenance.undo()");

    sc->setExpectedExceptionFlag(true);
    CHECK_THROW(sc->exec("provenance.undo()"), LuaProvenanceInvalidUndo);
    sc->setExpectedExceptionFlag(false);

    // Test out c++ parameter execution
    sc->registerFunction(&set_1ti, "set_1ti", "", true);
    sc->setDefaults("set_1ti", 10, true);
    CHECK_EQUAL(10, ti1);
    sc->registerFunction(&set_2ti, "set_2ti", "", true);
    sc->setDefaults("set_2ti", 11, 21, true);
    CHECK_EQUAL(11, ti1);
    CHECK_EQUAL(21, ti2);
    sc->registerFunction(&set_3ti, "set_3ti", "", true);
    sc->setDefaults("set_3ti", 12, 22, 32, true);
    CHECK_EQUAL(12, ti1);
    CHECK_EQUAL(22, ti2);
    CHECK_EQUAL(32, ti3);
    sc->registerFunction(&set_4ti, "set_4ti", "", true);
    sc->setDefaults("set_4ti", 13, 23, 33, 43, true);
    CHECK_EQUAL(13, ti1);
    CHECK_EQUAL(23, ti2);
    CHECK_EQUAL(33, ti3);
    CHECK_EQUAL(43, ti4);
    sc->registerFunction(&set_5ti, "set_5ti", "", true);
    sc->setDefaults("set_5ti", 14, 24, 34, 44, 54, true);
    CHECK_EQUAL(14, ti1);
    CHECK_EQUAL(24, ti2);
    CHECK_EQUAL(34, ti3);
    CHECK_EQUAL(44, ti4);
    CHECK_EQUAL(54, ti5);
    sc->registerFunction(&set_6ti, "set_6ti", "", true);
    sc->setDefaults("set_6ti", 15, 25, 35, 45, 55, 65, true);
    CHECK_EQUAL(15, ti1);
    CHECK_EQUAL(25, ti2);
    CHECK_EQUAL(35, ti3);
    CHECK_EQUAL(45, ti4);
    CHECK_EQUAL(55, ti5);
    CHECK_EQUAL(65, ti6);

    sc->setExpectedExceptionFlag(true);
    CHECK_THROW(sc->exec("provenance.undo()"), LuaProvenanceInvalidUndo);
    sc->setExpectedExceptionFlag(false);

    sc->cexec("set_6ti", 60, 62, 64, 66, 68, 70);

    CHECK_EQUAL(60, ti1);
    CHECK_EQUAL(62, ti2);
    CHECK_EQUAL(64, ti3);
    CHECK_EQUAL(66, ti4);
    CHECK_EQUAL(68, ti5);
    CHECK_EQUAL(70, ti6);

    sc->exec("provenance.undo()");

    CHECK_EQUAL(15, ti1);
    CHECK_EQUAL(25, ti2);
    CHECK_EQUAL(35, ti3);
    CHECK_EQUAL(45, ti4);
    CHECK_EQUAL(55, ti5);
    CHECK_EQUAL(65, ti6);
  }


  TEST(MiscPrinting)
  {
    TEST_HEADER;

    auto_ptr<LuaScripting> sc(new LuaScripting());

    sc->registerFunction(&set_i1, "set_i1", "", true);
    sc->registerFunction(&set_s1, "set_s1", "", true);
    sc->registerFunction(&set_b1, "set_b1", "", true);
    sc->registerFunction(&paste_i1, "paste_i1", "", true);

    sc->registerFunction(&get_i1, "get_i1", "", false);
    sc->registerFunction(&get_s1, "get_s1", "", false);
    sc->registerFunction(&get_b1, "get_b1", "", false);

    sc->exec("help()");
  }

  static float f1 = 0.0;

  void set_f1(float f)  {f1 = f;}

  void undo_i1(int i)   {i1 = i * 2;}
  void undo_f1(float f) {f1 = f + 2.5f;}
  void undo_s1(string s){s1 = s + "hi";}

  void redo_i1(int i)   {i1 = i * 4;}
  void redo_f1(float f) {f1 = f - 5.0f;}
  void redo_s1(string s){s1 = s + "hi2";}

  TEST(TestUndoRedoHooks)
  {
    TEST_HEADER;

    // Setup custom undo/redo hooks and test their efficacy.
    tr1::shared_ptr<LuaScripting> sc(new LuaScripting());

    sc->registerFunction(&set_i1, "set_i1", "", true);
    sc->registerFunction(&set_s1, "set_s1", "", true);
    sc->registerFunction(&set_b1, "set_b1", "", true);
    sc->registerFunction(&set_f1, "set_f1", "", true);

    // Customize undo functions (these undo functions will result in invalid
    // undo state, but thats what we want in order to detect correct redo/undo
    // hook installation).
    sc->setUndoFun(&undo_i1, "set_i1");
    sc->setUndoFun(&undo_f1, "set_f1");
    sc->setUndoFun(&undo_s1, "set_s1");

    sc->setRedoFun(&redo_i1, "set_i1");
    sc->setRedoFun(&redo_f1, "set_f1");
    sc->setRedoFun(&redo_s1, "set_s1");

    sc->exec("set_i1(100)");
    sc->exec("set_f1(126.5)");
    sc->exec("set_s1('Test')");

    CHECK_EQUAL(i1, 100);
    CHECK_CLOSE(f1, 126.5f, 0.001f);
    CHECK_EQUAL(s1.c_str(), "Test");

    sc->exec("set_i1(1000)");
    sc->exec("set_f1(500.0)");
    sc->exec("set_s1('nop')");

    sc->exec("provenance.undo()");
    CHECK_EQUAL("Testhi", s1.c_str());

    sc->exec("provenance.undo()");
    CHECK_CLOSE(129.0f, f1, 0.001f);

    sc->exec("provenance.undo()");
    CHECK_EQUAL(200, i1);

    sc->exec("provenance.redo()");
    CHECK_EQUAL(4000, i1);

    sc->exec("provenance.redo()");
    CHECK_CLOSE(495.0, f1, 0.001f);

    sc->exec("provenance.redo()");
    CHECK_EQUAL("nophi2", s1.c_str());
  }

#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS

  TEST(TestLuaRTTIChecks)
  {
    TEST_HEADER;

    auto_ptr<LuaScripting> sc(new LuaScripting());

    sc->registerFunction(&testParamReturn, "tpr", "", true);
    CHECK_EQUAL("Out: 65 1 4.3 str!",
                sc->cexecRet<string>("tpr", 65, true, 4.3f, "str!").c_str());

    sc->setExpectedExceptionFlag(true);
    CHECK_THROW(sc->cexec("tpr", 12, true),               LuaUnequalNumParams);
    CHECK_THROW(sc->cexec("tpr", 12, true, 4.3f, "s", 1), LuaUnequalNumParams);
    CHECK_THROW(sc->cexec("tpr", 12, "s", 4.3f, "s"),     LuaInvalidType);
    CHECK_THROW(sc->cexec("tpr", "s", false, 4.3f, "s"),  LuaInvalidType);
    CHECK_THROW(sc->cexec("tpr", 5, false, 32, "s"),      LuaInvalidType);
    CHECK_THROW(sc->cexec("tpr", 5, false, 32.0, "s"),    LuaInvalidType);
    CHECK_THROW(sc->cexec("tpr", 5, false, 32.0f, 3),     LuaInvalidType);
    sc->setExpectedExceptionFlag(false);
  }

#endif


  /// TODO: Add tests for passing shared_ptr's around, and how they work
  /// with regards to the undo/redo stack.

  /// TODO: Add tests to check the exceptions thrown in the case of too many /
  /// too little parameters for cexec, and the return values for execRet.

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
