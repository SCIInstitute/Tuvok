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
 \file    LUAMemberReg.h
 \author  James Hughes
          SCI Institute
          University of Utah
 \date    Mar 26, 2012
 \brief   This mediator class handles member function registration.
          Should be instantiated alongside an encapsulating class.
 */

#ifndef TUVOK_LUAMEMBER_REG_H_
#define TUVOK_LUAMEMBER_REG_H_

#include "LuaScripting.h"

namespace tuvok
{

class LuaScripting;

//===============================
// LUA BINDING HELPER STRUCTURES
//===============================


// Member registration/hooking mediator class.
class LuaMemberReg
{
public:

  LuaMemberReg(std::tr1::shared_ptr<LuaScripting> scriptSys);
  virtual ~LuaMemberReg();


  template <typename FunPtr, typename Ret>
  struct LuaMemberCallback
  {
    static int exec(lua_State* L)
    {
      // TODO: Add provenance here.
      FunPtr fp = *static_cast<FunPtr*>(lua_touserdata(L, lua_upvalueindex(1)));
      typename LuaCFunExec<FunPtr>::classType* C =
                  static_cast<typename LuaCFunExec<FunPtr>::classType*>(
                      lua_touserdata(L, lua_upvalueindex(2)));
      Ret r;
      if (lua_toboolean(L, lua_upvalueindex(3)) == 0)
      {
        // We are not a hook call.
        r = LuaCFunExec<FunPtr>::run(L, 2, C, fp);
        LuaScripting::doHooks(L, 1);
        // TODO: Provenance here.
        // TODO: Add last exec parameters to the last exec table here.
      }
      else
      {
        // We are a hook call.
        r = LuaCFunExec<FunPtr>::run(L, 1, C, fp);
      }

      LuaStrictStack<Ret>().push(L, r);
      return 1;
    }
  };

  // Without a return value.
  template <typename FunPtr>
  struct LuaMemberCallback <FunPtr, void>
  {
    static int exec(lua_State* L)
    {
      // TODO: Add provenance here.
      FunPtr fp = *static_cast<FunPtr*>(lua_touserdata(L, lua_upvalueindex(1)));
      typename LuaCFunExec<FunPtr>::classType* C =
          static_cast<typename LuaCFunExec<FunPtr>::classType*>(
              lua_touserdata(L, lua_upvalueindex(2)));

      if (lua_toboolean(L, lua_upvalueindex(3)) == 0)
      {
        LuaCFunExec<FunPtr>::run(L, 2, C, fp);
        LuaScripting::doHooks(L, 1);
      }
      else
      {
        LuaCFunExec<FunPtr>::run(L, 1, C, fp);
      }

      return 0;
    }
  };


  // See LuaScripting::registerFunction for an in depth description of params.
  // The only difference is the parameter C, which the context class for the
  // member function pointer.
  template <typename T, typename FunPtr>
  void registerFunction(T* C, FunPtr f, const std::string& name,
                        const std::string& desc)
  {
    LuaScripting* ss  = mScriptSystem.get();
    lua_State*    L   = ss->getLUAState();

    int initStackTop = lua_gettop(L);

    // Member function pointers are not pointing to a function, they are
    // compiler dependent and are pointing to a memory address.
    // They need to be copied into lua in an portable manner.
    // See the C++ standard.
    // Create a callable function table and leave it on the stack.
    lua_CFunction proxyFunc = &LuaMemberCallback<FunPtr, typename
        LuaCFunExec<FunPtr>::returnType>::exec;

    // Table containing the function closure.
    lua_newtable(L);
    int tableIndex = lua_gettop(L);

    // Create a new metatable
    lua_newtable(L);

    // Create a full user data and store the function pointer data inside of it.
    void* udata = lua_newuserdata(L, sizeof(FunPtr));
    memcpy(udata, &f, sizeof(FunPtr));
    lua_pushlightuserdata(L, static_cast<void*>(C));
    lua_pushboolean(L, 0);  // We are NOT a hook.
    lua_pushcclosure(L, proxyFunc, 3);

    // Associate closure with __call metamethod.
    lua_setfield(L, -2, "__call");

    // Add boolean to the metatable indicating that this table is a registered
    // function. Used to ensure that we can't register functions 'on top' of
    // other functions.
    // e.g. If we register renderer.eye as a function, without this check, we
    // could also register renderer.eye.ball as a function.
    // While it works just fine, it's confusing, so we're disallowing it.
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "isRegFunc");

    // Associate metatable with primary table.
    lua_setmetatable(L, -2);

    // Add function metadata to the table.
    std::string sig = LuaCFunExec<FunPtr>::getSignature("");
    std::string sigWithName = LuaCFunExec<FunPtr>::getSignature(
        ss->getUnqualifiedName(name));
    std::string sigNoRet = LuaCFunExec<FunPtr>::getSigNoReturn("");
    ss->populateWithMetadata(name, desc, sig, sigWithName, sigNoRet,
                             tableIndex);

    // Push default values for function parameters onto the stack.
    LuaCFunExec<FunPtr> defaultParams = LuaCFunExec<FunPtr>();
    lua_checkstack(L, 10); // Max num parameters accepted by the system.
    defaultParams.pushParamsToStack(L);
    int numFunParams = lua_gettop(L) - tableIndex;
    ss->createDefaultsAndLastExecTables(tableIndex, numFunParams);

    int testPos = lua_gettop(L);

    // Install the callable table in the appropriate module based on its
    // fully qualified name.
    ss->bindClosureTableWithFQName(name, tableIndex);

    testPos = lua_gettop(L);

    lua_pop(L, 1);   // Pop the callable table.

    // Add this function to the list of registered functions.
    // (an exception would have gotten thrown before this point if there
    //  was a duplicate function registered in the system).
    mRegisteredFunctions.push_back(name);

    assert(initStackTop == lua_gettop(L));
  }


  template <typename T, typename FunPtr>
  void strictHook(T* C, FunPtr f, const std::string& name)
  {
    LuaScripting* ss  = mScriptSystem.get();
    lua_State*    L   = ss->getLUAState();

    // Need to check the signature of the function that we are trying to bind
    // into the script system.
    int initStackTop = lua_gettop(L);

    if (ss->getFunctionTable(name) == false)
    {
      lua_settop(L, initStackTop);
      throw LUANonExistantFunction("Unable to find function with which to"
                                   "associate a hook.");
    }

    int funcTable = lua_gettop(L);

    // Check function signatures.
    lua_getfield(L, funcTable, LuaScripting::TBL_MD_SIG_NO_RET);
    std::string sigReg = lua_tostring(L, -1);
    std::string sigHook = LuaCFunExec<FunPtr>::getSigNoReturn("");
    if (sigReg.compare(sigHook) != 0)
    {
      lua_settop(L, initStackTop);
      std::ostringstream os;
      os << "Hook's parameter signature and the parameter signature of the "
            "function to hook must match. Hook's signature: \"" << sigHook <<
            "\"Function to hook's signature: " << sigReg;
      throw LUAInvalidFunSignature(os.str().c_str());
    }
    lua_pop(L, 1);

    // Obtain hook table.
    lua_getfield(L, -1, LuaScripting::TBL_MD_MEMBER_HOOKS);
    int hookTable = lua_gettop(L);

    // Ensure our hook descriptor is not already there.
    lua_getfield(L, -1, mHookID.c_str());
    if (lua_isnil(L, -1) == 0)
    {
      lua_settop(L, initStackTop);
      std::ostringstream os;
      os << "Instance of LuaMemberReg has already bound " << name;
      throw LUAFunBindError(os.str().c_str());
    }
    lua_pop(L, 1);

    // Push closure
    lua_CFunction proxyFunc = &LuaMemberCallback<FunPtr, typename
            LuaCFunExec<FunPtr>::returnType>::exec;
    void* udata = lua_newuserdata(L, sizeof(FunPtr));
    memcpy(udata, &f, sizeof(FunPtr));
    lua_pushlightuserdata(L, static_cast<void*>(C));
    lua_pushboolean(L, 1);  // We ARE a hook. This affects the stack, and
                            // whether we want to perform provenance.
    lua_pushcclosure(L, proxyFunc, 3);

    // Associate closure with hook table.
    lua_setfield(L, hookTable, mHookID.c_str());
  }

private:

  /// Scripting system we are bound to.
  std::tr1::shared_ptr<LuaScripting>  mScriptSystem;

  /// Functions registered.
  /// Used to unregister all functions.
  std::vector<std::string>            mRegisteredFunctions;

  /// Functions registered with this hook -- used for unregistering hooks.
  std::vector<std::string>            mHookedFunctions;

  /// ID used by LUA in order to identify the functions hooked by this class.
  /// This ID is used as the key in the hook table.
  const std::string                   mHookID;
};

}

#endif
