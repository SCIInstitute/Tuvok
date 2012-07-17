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
 \brief   This mediator class handles member function registration.
          This class is used for internal purposes only, please use
          LuaMemberReg instead.

          Exists solely to provide member function registration to LuaScripting
          and its composited classes (without the need for a shared_ptr).

          If you do use this class instead of LuaMemberReg, be sure to call
          the unregisterAll function manually when your class is destroyed.
          It is your responsibility to ensure that the LuaScripting pointer
          remains valid throughout the lifetime of LuaMemberRegUnsafe.
 */

#ifndef TUVOK_LUAMEMBER_REG_UNSAFE_H_
#define TUVOK_LUAMEMBER_REG_UNSAFE_H_

#include "LuaScripting.h"
#include <cstring>

namespace tuvok
{

class LuaScripting;

//===============================
// LUA BINDING HELPER STRUCTURES
//===============================

class LuaMemberRegUnsafe
{
public:

  LuaMemberRegUnsafe(LuaScripting* scriptSys);
  virtual ~LuaMemberRegUnsafe();

  /// See LuaScripting::registerFunction for an in depth description of params.
  /// The only difference is the parameter C, which is the context class for the
  /// member function pointer.
  /// \return The name parameter.
  template <typename T, typename FunPtr>
  std::string registerFunction(T* C, FunPtr f, const std::string& name,
                               const std::string& desc, bool undoRedo);

  /// See LuaScripting::strictHook.
  template <typename T, typename FunPtr>
  void strictHook(T* C, FunPtr f, const std::string& name);

  /// See LuaScripting::undoHook. Sets the undo function for the registered
  /// function at 'name'.
  template <typename T, typename FunPtr>
  void setUndoFun(T* C, FunPtr f, const std::string& name);

  /// See LuaScripting::redoHook. Sets the redo function for the registered
  /// function at 'name'.
  template <typename T, typename FunPtr>
  void setRedoFun(T* C, FunPtr f, const std::string& name);

  /// Unregisters all functions and hooks in the correct order.
  void unregisterAll();

  // Before issuing a call to any of the functions below, please
  // consider using unregisterAll instead. In certain circumstances,
  // unregisterHooks must be called before unregisterFunctions.
  void unregisterFunctions();
  void unregisterHooks();
  void unregisterUndoRedoFunctions();

private:

  /// Scripting system we are bound to.
  LuaScripting*                       mScriptSystem;

  /// Functions registered.
  /// Used to unregister all functions.
  std::vector<std::string>            mRegisteredFunctions;

  /// Functions registered with this hook -- used for unregistering hooks.
  std::vector<std::string>            mHookedFunctions;

  /// ID used by Lua in order to identify the functions hooked by this class.
  /// This ID is used as the key in the hook table.
  const std::string                   mHookID;


  struct UndoRedoReg
  {
    UndoRedoReg(const std::string& funName, bool undo)
    : functionName(funName), isUndo(undo)
    {}

    std::string     functionName;
    bool            isUndo;
  };
  std::vector<UndoRedoReg>            mRegisteredUndoRedo;

  /// Strict hook internal -- called by strictHook and the setUndoFun and
  /// setRedoFun methods.
  template <typename T, typename FunPtr>
  void strictHookInternal(T* C, FunPtr f, const std::string& name,
                          bool registerUndo, bool registerRedo);

  template <typename T, typename FunPtr, typename Ret>
  struct LuaMemberCallback
  {
    static int exec(lua_State* L)
    {
      LuaStackRAII _a = LuaStackRAII(L, 1); // 1 for the return value.

      FunPtr fp = *static_cast<FunPtr*>(lua_touserdata(L, lua_upvalueindex(1)));
      T* bClass = reinterpret_cast<T*>(lua_touserdata(L, lua_upvalueindex(2)));
      typename LuaCFunExec<FunPtr>::classType* C =
          dynamic_cast<typename LuaCFunExec<FunPtr>::classType*>(bClass);
      Ret r;
      if (lua_toboolean(L, lua_upvalueindex(3)) == 0)
      {
        LuaScripting* ss = static_cast<LuaScripting*>(
            lua_touserdata(L, lua_upvalueindex(4)));

        std::shared_ptr<LuaCFunAbstract> execParams(
            new LuaCFunExec<FunPtr>());
        std::shared_ptr<LuaCFunAbstract> emptyParams(
            new LuaCFunExec<FunPtr>());
        // Fill execParams. Function parameters start at index 2 (callable
        // table starts at index 1).
        execParams->pullParamsFromStack(L, 2);

        // Obtain reference to LuaScripting to invoke provenance.
        // See createCallableFuncTable for justification on pulling an
        // instance of LuaScripting out of Lua.
        bool provExempt = ss->doProvenanceFromExec(L, execParams, emptyParams);

        ss->beginCommand();
        try
        {
          r = LuaCFunExec<FunPtr>::run(L, 2, C, fp);
        }
        catch (std::exception& e)
        {
          ss->logExecFailure(e.what());
          ss->endCommand();
          throw;
        }
        catch (...)
        {
          ss->logExecFailure("");
          ss->endCommand();
          throw;
        }
        ss->endCommand();

        ss->doHooks(L, 1, provExempt);
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

  template <typename T, typename FunPtr>
  struct LuaMemberCallback <T, FunPtr, void>
  {
    static int exec(lua_State* L)
    {
      LuaStackRAII _a = LuaStackRAII(L, 0);

      FunPtr fp = *static_cast<FunPtr*>(lua_touserdata(L, lua_upvalueindex(1)));
      T* bClass = reinterpret_cast<T*>(lua_touserdata(L, lua_upvalueindex(2)));
      typename LuaCFunExec<FunPtr>::classType* C =
          dynamic_cast<typename LuaCFunExec<FunPtr>::classType*>(bClass);

      if (lua_toboolean(L, lua_upvalueindex(3)) == 0)
      {
        LuaScripting* ss = static_cast<LuaScripting*>(
            lua_touserdata(L, lua_upvalueindex(4)));

        std::shared_ptr<LuaCFunAbstract> execParams(
            new LuaCFunExec<FunPtr>());
        std::shared_ptr<LuaCFunAbstract> emptyParams(
            new LuaCFunExec<FunPtr>());
        execParams->pullParamsFromStack(L, 2);

        bool provExempt = ss->doProvenanceFromExec(L, execParams, emptyParams);

        ss->beginCommand();
        try
        {
          LuaCFunExec<FunPtr>::run(L, 2, C, fp);
        }
        catch (std::exception& e)
        {
          ss->logExecFailure(e.what());
          ss->endCommand();
          throw;
        }
        catch (...)
        {
          ss->logExecFailure("");
          ss->endCommand();
          throw;
        }
        ss->endCommand();

        ss->doHooks(L, 1, provExempt);
      }
      else
      {
        LuaCFunExec<FunPtr>::run(L, 1, C, fp);
      }

      return 0;
    }
  };
};

template <typename T, typename FunPtr>
std::string LuaMemberRegUnsafe::registerFunction(T* C, FunPtr f,
                                                 const std::string& name,
                                                 const std::string& desc,
                                                 bool undoRedo)
{
  LuaScripting* ss  = mScriptSystem;
  lua_State*    L   = ss->getLuaState();

  LuaStackRAII _a = LuaStackRAII(L, 0);

  // Member function pointers are not pointing to a function, they are
  // compiler dependent and are pointing to a memory address.
  // They need to be copied into lua in an portable manner.
  // See the C++ standard.
  // Create a callable function table and leave it on the stack.
  lua_CFunction proxyFunc = &LuaMemberCallback<T, FunPtr, typename
      LuaCFunExec<FunPtr>::returnType>::exec;

  // Table containing the function closure.
  lua_newtable(L);
  int tableIndex = lua_gettop(L);

  // Create a new metatable
  lua_newtable(L);

  // Create a full user data and store the function pointer data inside of it.
  void* udata = lua_newuserdata(L, sizeof(FunPtr));
  memcpy(udata, &f, sizeof(FunPtr));
  lua_pushlightuserdata(L, reinterpret_cast<void*>(C));
  lua_pushboolean(L, 0);  // We are NOT a hook.
  // We are safe pushing this unprotected pointer: LuaScripting always
  // deregisters all functions it has registered, so no residual light
  // user data will be left in Lua.
  lua_pushlightuserdata(L, static_cast<void*>(mScriptSystem));
  lua_pushcclosure(L, proxyFunc, 4);

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
  lua_checkstack(L, LUAC_MAX_NUM_PARAMS); // Max num parameters supported
  defaultParams.pushParamsToStack(L);
  int numFunParams = lua_gettop(L) - tableIndex;
  ss->createDefaultsAndLastExecTables(tableIndex, numFunParams);
  lua_pushinteger(L, numFunParams);
  lua_setfield(L, tableIndex, LuaScripting::TBL_MD_NUM_PARAMS);

#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
  // Generate the type table (buildTypeTable places table on top of the stack).
  LuaCFunExec<FunPtr>::buildTypeTable(L);
  lua_setfield(L, tableIndex, LuaScripting::TBL_MD_TYPES_TABLE);
#endif

  // Install the callable table in the appropriate module based on its
  // fully qualified name.
  ss->bindClosureTableWithFQName(name, tableIndex);

  lua_pop(L, 1);   // Pop the callable table.

  // Add this function to the list of registered functions.
  // (an exception would have gotten thrown before this point if there
  //  was a duplicate function registered in the system).
  mRegisteredFunctions.push_back(name);

  if (undoRedo == false)  ss->setUndoRedoStackExempt(name);

  return name;
}

template <typename T, typename FunPtr>
void LuaMemberRegUnsafe::strictHook(T* C, FunPtr f, const std::string& name)
{
  strictHookInternal(C, f, name, false, false);
}

template <typename T, typename FunPtr>
void LuaMemberRegUnsafe::strictHookInternal(
    T* C,
    FunPtr f,
    const std::string& name,
    bool registerUndo,
    bool registerRedo
    )
{
  LuaScripting* ss  = mScriptSystem;
  lua_State*    L   = ss->getLuaState();

  LuaStackRAII _a = LuaStackRAII(L, 0);

  // Need to check the signature of the function that we are trying to bind
  // into the script system.
  if (ss->getFunctionTable(name) == false)
  {
    throw LuaNonExistantFunction("Unable to find function with which to"
                                 "associate a hook.");
  }

  int funcTable = lua_gettop(L);

  // Check function signatures.
  lua_getfield(L, funcTable, LuaScripting::TBL_MD_SIG_NO_RET);
  std::string sigReg = lua_tostring(L, -1);
  std::string sigHook = LuaCFunExec<FunPtr>::getSigNoReturn("");
  if (sigReg.compare(sigHook) != 0)
  {
    std::ostringstream os;
    os << "Hook's parameter signature and the parameter signature of the "
          "function to hook must match. Hook's signature: \"" << sigHook <<
          "\"Function to hook's signature: " << sigReg;
    throw LuaInvalidFunSignature(os.str().c_str());
  }
  lua_pop(L, 1);

  // Obtain hook table.
  lua_getfield(L, -1, LuaScripting::TBL_MD_MEMBER_HOOKS);
  int hookTable = lua_gettop(L);

  bool regUndoRedo = (registerUndo || registerRedo);
  if (!regUndoRedo)
  {
    // Ensure our hook descriptor is not already there.
    lua_getfield(L, -1, mHookID.c_str());
    if (lua_isnil(L, -1) == 0)
    {
      std::ostringstream os;
      os << "Instance of LuaMemberReg has already bound " << name;
      throw LuaFunBindError(os.str().c_str());
    }
    lua_pop(L, 1);
  }

  // Push closure
  lua_CFunction proxyFunc = &LuaMemberCallback<T, FunPtr, typename
          LuaCFunExec<FunPtr>::returnType>::exec;
  void* udata = lua_newuserdata(L, sizeof(FunPtr));
  memcpy(udata, &f, sizeof(FunPtr));
  lua_pushlightuserdata(L, static_cast<void*>(C));
  lua_pushboolean(L, 1);  // We ARE a hook. This affects the stack, and
                          // whether we want to perform provenance.
                          // Also, we don't need to push a ref to
                          // mScriptSystem.
  lua_pushcclosure(L, proxyFunc, 3);

  if (!regUndoRedo)
  {
    // Associate closure with hook table.
    lua_setfield(L, hookTable, mHookID.c_str());
    mHookedFunctions.push_back(name);
  }
  else
  {
    std::string fieldToQuery = LuaScripting::TBL_MD_UNDO_FUNC;
    bool isUndo = true;

    if (registerRedo)
    {
      fieldToQuery = LuaScripting::TBL_MD_REDO_FUNC;
      isUndo = false;
    }

    lua_getfield(L, funcTable, fieldToQuery.c_str());

    if (lua_isnil(L, -1) == 0)
    {
      if (isUndo) throw LuaUndoFuncAlreadySet("Undo function already set.");
      else        throw LuaRedoFuncAlreadySet("Redo function already set.");
    }

    lua_pop(L, 1);
    lua_setfield(L, funcTable, fieldToQuery.c_str());

    mRegisteredUndoRedo.push_back(UndoRedoReg(name, isUndo));
  }

  lua_pop(L, 2);  // Remove the function table and the hooks table.
}

// See LuaScripting::undoHook.
template <typename T, typename FunPtr>
void LuaMemberRegUnsafe::setUndoFun(T* C, FunPtr f, const std::string& name)
{
  // Uses strict hook.
  strictHookInternal(C, f, name, true, false);
}

// See LuaScripting::redoHook.
template <typename T, typename FunPtr>
void LuaMemberRegUnsafe::setRedoFun(T* C, FunPtr f, const std::string& name)
{
  // Uses strict hook.
  strictHookInternal(C, f, name, false, true);
}

}

#endif
