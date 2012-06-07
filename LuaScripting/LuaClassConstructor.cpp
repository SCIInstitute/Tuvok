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
 \brief
 */

#ifndef EXTERNAL_UNIT_TESTING

#include "Controller/Controller.h"
#include "3rdParty/LUA/lua.hpp"

#else

#include <assert.h>
#include "utestCommon.h"

#endif

#include <vector>

#include "LuaScripting.h"
#include "LuaStackRAII.h"
#include "LuaClassInstance.h"
#include "LuaProvenance.h"

using namespace std;
namespace tuvok
{

const char* LuaClassConstructor::CONS_MD_FACTORY_NAME           = "factoryName";
const char* LuaClassConstructor::CONS_MD_FUNC_REGISTRATION_FPTR = "consFptr";

LuaClassConstructor::LuaClassConstructor(LuaScripting* ss)
: mSS(ss)
{

}

LuaClassConstructor::~LuaClassConstructor()
{

}

void LuaClassConstructor::addToLookupTable(LuaScripting* ss,
                                           lua_State* L,
                                           void* ptr,
                                           int instID)
{
  // Push our index into the lookup table. Use the class instance pointer
  // we created earlier as the lookup in this table.
  if (ss->getFunctionTable(LuaClassInstance::CLASS_LOOKUP_TABLE) == false)
    throw LuaError("Unable to obtain class lookup table!");
  lua_pushlightuserdata(L, ptr);
  lua_pushinteger(L, instID);
  lua_settable(L, -3);
  lua_pop(L, 1);
}

int LuaClassConstructor::createCoreMetatable(lua_State* L, int instID,
                                             int consTable)
{
  // Pull factory name from the constructor table.
  lua_getfield(L, consTable, CONS_MD_FACTORY_NAME);
  std::string factoryFQName = lua_tostring(L, -1);
  lua_pop(L, 1);

  lua_newtable(L);
  int mt = lua_gettop(L);

  lua_pushinteger(L, instID);
  lua_setfield(L, mt, LuaClassInstance::MD_GLOBAL_INSTANCE_ID);

  lua_pushstring(L, factoryFQName.c_str());
  lua_setfield(L, mt, LuaClassInstance::MD_FACTORY_NAME);

  lua_pushboolean(L, 0);
  lua_setfield(L, mt, LuaClassInstance::MD_NO_DELETE_HINT);

  return mt;
}

void LuaClassConstructor::finalizeMetatable(lua_State* L, int mt,
                                            void* ptr, void* delPtr,
                                            void* delCallbackPtr)
{
  // Setup metatable attributes that depend on the class pointer and
  // the type FunPtr.
  lua_pushlightuserdata(L, ptr);
  lua_setfield(L, mt, LuaClassInstance::MD_INSTANCE);

  lua_pushlightuserdata(L, delPtr);
  lua_setfield(L, mt, LuaClassInstance::MD_DEL_FUN);

  lua_pushlightuserdata(L, delCallbackPtr);
  lua_setfield(L, mt, LuaClassInstance::MD_DEL_CALLBACK_PTR);
}

LuaClassInstance LuaClassConstructor::finalizeInstanceTable(LuaScripting* ss,
                                                            int instTable,
                                                            int instID)
{
  LuaClassInstance instance(instID);

  ss->bindClosureTableWithFQName(instance.fqName(), instTable);
  ss->getProvenanceSys()->addCreatedInstanceToLastURItem(instID);

  return instance;
}

LuaClassInstance LuaClassConstructor::buildCoreInstanceTable(lua_State* L,
                                                             LuaScripting* ss,
                                                             int consTable,
                                                             int instID)
{
  // Build instance table.
  lua_newtable(L);
  int instTable = lua_gettop(L);
  createCoreMetatable(L, instID, consTable);

  // Attach an instance of the instance table so that the constructor
  // can register its functions.
  lua_pushvalue(L, -1);
  lua_setmetatable(L, instTable);

  // Bind the instance table so that the user can register functions in
  // their constructor.
  return finalizeInstanceTable(ss, instTable, instID);
}

void LuaClassConstructor::finalize(lua_State* L, LuaScripting* ss, void* r,
                                   LuaClassInstance inst, int mt, int instTable,
                                   void* delFun, void* delCallbackFun)
{
  addToLookupTable(ss, L, r, inst.getGlobalInstID());

  finalizeMetatable(L, mt, r, delFun, delCallbackFun);

  // Remove the metatable first, then the instance table (otherwise,
  // metatable's index would be one lower than what we recorded).
  lua_remove(L, mt);
  lua_remove(L, instTable);

  // Place function table on the top of the stack (could just leave instTable
  // on the top of the stack).
  if (ss->getFunctionTable(inst.fqName()) == false)
    throw LuaFunBindError("Unable to find table after it was created!");
}

} /* namespace tuvok */
