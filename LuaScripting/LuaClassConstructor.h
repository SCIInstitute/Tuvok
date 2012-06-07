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

#ifndef TUVOK_LUACLASSCONSTRUCTOR_H_
#define TUVOK_LUACLASSCONSTRUCTOR_H_

#include <cstring>
#include "LuaClassInstance.h"

namespace tuvok
{

class LuaClassConstructor
{
public:
  LuaClassConstructor(LuaScripting* ss);
  virtual ~LuaClassConstructor();

  /// Data stored in the constructor table
  ///@{
  static const char*    CONS_MD_FACTORY_NAME;
  static const char*    CONS_MD_FUNC_REGISTRATION_FPTR;
  ///@}

  template <typename CLS, typename FunPtr>
  void registerConstructor(
      FunPtr f, const std::string& name,
      const std::string& desc, bool undoRedo,
      typename LuaClassRegCallback<CLS>::Type& callback)
  {
    lua_State* L = mSS->getLuaState();

    LuaStackRAII _a = LuaStackRAII(L, 0);

    // Idea: Build a 'callable' table.
    // Its metatable will have a __call metamethod that points to the C
    // function closure.

    // We do this because all metatables are unique per-type which eliminates
    // the possibilty of using a metatable on the function closure itself.
    // The only exception to this rule is the table type itself.

    // Create a callable function table and leave it on the stack.
    lua_CFunction proxyFunc = &LuaConstructorCallback<CLS, FunPtr>::exec;
    mSS->createCallableFuncTable(proxyFunc, reinterpret_cast<void*>(f));

    int tableIndex = lua_gettop(L);

    // Add function metadata to the table.
    // We know the constructor will return a class that strict stack is not
    // capable of handling. So we insert our own return value.
    std::string sig = "LuaClassInstance "
        + LuaCFunExec<FunPtr>::getSigNoReturn("");
    std::string sigWithName = "LuaClassInstance " +
        LuaCFunExec<FunPtr>::getSigNoReturn(mSS->getUnqualifiedName(name));
    std::string sigNoRet = LuaCFunExec<FunPtr>::getSigNoReturn("");
    mSS->populateWithMetadata(name, desc, sig, sigWithName, sigNoRet,
                              tableIndex);

    // Push default values for function parameters onto the stack.
    LuaCFunExec<FunPtr> defaultParams = LuaCFunExec<FunPtr>();
    lua_checkstack(L, LUAC_MAX_NUM_PARAMS + 2);
    defaultParams.pushParamsToStack(L);
    int numFunParams = lua_gettop(L) - tableIndex;
    mSS->createDefaultsAndLastExecTables(tableIndex, numFunParams);
    lua_pushinteger(L, numFunParams);
    lua_setfield(L, tableIndex, LuaScripting::TBL_MD_NUM_PARAMS);

    // Add indicator that we are NOT a member function (

#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
    // Generate the type table (buildTypeTable places table on top of the stack).
    LuaCFunExec<FunPtr>::buildTypeTable(L);
    lua_setfield(L, tableIndex, LuaScripting::TBL_MD_TYPES_TABLE);
#endif

    lua_pushlightuserdata(L, new typename LuaClassRegCallback<CLS>::Type(
        callback));
    lua_setfield(L, tableIndex,
                 LuaClassConstructor::CONS_MD_FUNC_REGISTRATION_FPTR);

    // Install the callable table in the appropriate module based on its
    // fully qualified name.
    mSS->bindClosureTableWithFQName(name, tableIndex);

    lua_pop(L, 1);   // Pop the callable table.

    if (undoRedo == false)  mSS->setUndoRedoStackExempt(name);
  }

  // This is the exact same function as LuaMemberRegUnsafe::registerFunction
  // with the exception of the proxyFunc, and addition to the global
  // registered function list.
  template <typename CLS, typename T, typename FunPtr>
  void registerMemberConstructor(
      T* C, FunPtr f,
      const std::string& name,
      const std::string& desc,
      bool undoRedo,
      typename LuaClassRegCallback<CLS>::Type& callback)
  {
    lua_State* L = mSS->getLuaState();
    LuaStackRAII _a = LuaStackRAII(L, 0);

    // Member function pointers are not pointing to a function, they are
    // compiler dependent and are pointing to a memory address.
    // They need to be copied into lua in an portable manner.
    // See the C++ standard.
    // Create a callable function table and leave it on the stack.
    lua_CFunction proxyFunc = &LuaMemberConstructorCallback<CLS, FunPtr>::exec;

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
    // We are safe pushing this unprotected pointer: LuaScripting always
    // deregisters all functions it has registered, so no residual light
    // user data will be left in Lua.
    lua_pushlightuserdata(L, static_cast<void*>(mSS));
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
    std::string sig = "LuaClassInstance "
        + LuaCFunExec<FunPtr>::getSigNoReturn("");
    std::string sigWithName = "LuaClassInstance " +
        LuaCFunExec<FunPtr>::getSigNoReturn(mSS->getUnqualifiedName(name));
    std::string sigNoRet = LuaCFunExec<FunPtr>::getSigNoReturn("");
    mSS->populateWithMetadata(name, desc, sig, sigWithName, sigNoRet,
                             tableIndex);

    // Push default values for function parameters onto the stack.
    LuaCFunExec<FunPtr> defaultParams = LuaCFunExec<FunPtr>();
    lua_checkstack(L, LUAC_MAX_NUM_PARAMS); // Max num parameters supported
    defaultParams.pushParamsToStack(L);
    int numFunParams = lua_gettop(L) - tableIndex;
    mSS->createDefaultsAndLastExecTables(tableIndex, numFunParams);
    lua_pushinteger(L, numFunParams);
    lua_setfield(L, tableIndex, LuaScripting::TBL_MD_NUM_PARAMS);

#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
    // Generate the type table (buildTypeTable places table on top of the stack).
    LuaCFunExec<FunPtr>::buildTypeTable(L);
    lua_setfield(L, tableIndex, LuaScripting::TBL_MD_TYPES_TABLE);
#endif

    // This will be freed by LuaScripting -- in the destroyClassTable function.
    lua_pushlightuserdata(L, new typename LuaClassRegCallback<CLS>::Type(
        callback));
    lua_setfield(L, tableIndex,
                 LuaClassConstructor::CONS_MD_FUNC_REGISTRATION_FPTR);

    // Install the callable table in the appropriate module based on its
    // fully qualified name.
    mSS->bindClosureTableWithFQName(name, tableIndex);

    lua_pop(L, 1);   // Pop the callable table.

    if (undoRedo == false)  mSS->setUndoRedoStackExempt(name);
  }

  /// Signature for delFun in LuaConstructorCallback.
  typedef void (*DelFunSig)(void* inst);

private:

  template <typename CLS, typename FunPtr>
  struct LuaConstructorCallback
  {
    // Lines marked with a double forward slash are those that differ from
    // the function above (the exec function above).
    static int exec(lua_State* L)
    {
      LuaStackRAII _a = LuaStackRAII(L, 1); // 1 return value.

      // The function table that called us on the top of the stack.
      int consTable = 1;
      typename LuaCFunExec<FunPtr>::returnType r = NULL;

      FunPtr fp = reinterpret_cast<FunPtr>(                                   //
          lua_touserdata(L, lua_upvalueindex(1)));                            //
      LuaScripting* ss = static_cast<LuaScripting*>(                          //
                  lua_touserdata(L, lua_upvalueindex(3)));                    //

      std::tr1::shared_ptr<LuaCFunAbstract> execParams(
          new LuaCFunExec<FunPtr>());
      std::tr1::shared_ptr<LuaCFunAbstract> emptyParams(
          new LuaCFunExec<FunPtr>());
      // Fill execParams. Function parameters start at index 2.
      execParams->pullParamsFromStack(L, 2);

      // Obtain reference to LuaScripting in order to invoke provenance.
      // See createCallableFuncTable for justification on pulling an
      // instance of LuaScripting out of Lua.
      bool provExempt = ss->doProvenanceFromExec(L, execParams, emptyParams);

      // Places the instance table and its metatable at stack position -2 and
      // -1 respectively.
      LuaClassInstance inst = buildCoreInstanceTable(L, ss, consTable,
                                                     ss->getNewClassInstID());
      int mt = lua_gettop(L);
      int instTable = mt - 1;

      ss->beginCommand();
      try
      {
        r = LuaCFunExec<FunPtr>::run(L, 2, fp);           //
      }
      catch (std::exception& e)
      {
        ss->endCommand();
        ss->logExecFailure(e.what());
        throw;
      }
      catch (...)
      {
        ss->endCommand();
        ss->logExecFailure("");
        throw;
      }
      ss->endCommand();

      // Check to see if the function succeeded.
      if (r != NULL)
      {
        // Call registration fptr.
        lua_getfield(L, consTable,
                     LuaClassConstructor::CONS_MD_FUNC_REGISTRATION_FPTR);
        typename LuaClassRegCallback<CLS>::Type* cbFptr =
            reinterpret_cast<typename LuaClassRegCallback<CLS>::Type* >(
                lua_touserdata(L, -1));
        LuaClassRegistration<CLS> reg(ss, dynamic_cast<CLS*>(r),
                                      inst.getGlobalInstID());
        (*cbFptr)(reg, r, ss);
        lua_pop(L, 1);

        ss->doHooks(L, 1, provExempt);

        // Places function table on the top of the stack.
        finalize(L, ss, reinterpret_cast<void*>(r), inst, mt, instTable,
                 reinterpret_cast<void*>(
                     &LuaConstructorCallback<CLS, FunPtr>::del),
                 reinterpret_cast<void*>(
                     &LuaConstructorCallback<CLS, FunPtr>::delCallback));
      }
      else
      {
        ss->vPrint("Failed to load class.");
        // Create a default LuaClassInstance and return that.
        LuaClassInstance inst;
        LuaStrictStack<LuaClassInstance>::push(L, inst);
      }

      return 1;
    }

    // Casts the void* to the appropriate type, and deletes the class
    // instance.
    static void del(void* inst)
    {
      // Cast and delete the class instance.
      // NOTE: returnType should already be a pointer type.
      delete (reinterpret_cast<typename LuaCFunExec<FunPtr>::returnType>(
          inst));
    }

    static void delCallback(void* callback)
    {
      delete (reinterpret_cast<typename LuaClassRegCallback<CLS>::Type* >(
          callback));
    }
  };

  template <typename CLS, typename FunPtr>
  struct LuaMemberConstructorCallback
  {
    // Lines marked with a double forward slash are those that differ from
    // the function above (the exec function above).
    static int exec(lua_State* L)
    {
      LuaStackRAII _a = LuaStackRAII(L, 1); // 1 return value.

      // The function table that called us on the top of the stack.
      int consTable = 1;
      typename LuaCFunExec<FunPtr>::returnType r = NULL;

      FunPtr fp = *static_cast<FunPtr*>(lua_touserdata(L,                     //
                                                       lua_upvalueindex(1))); //
      typename LuaCFunExec<FunPtr>::classType* C =                            //
                  static_cast<typename LuaCFunExec<FunPtr>::classType*>(      //
                      lua_touserdata(L, lua_upvalueindex(2)));                //
      LuaScripting* ss = static_cast<LuaScripting*>(                          //
                  lua_touserdata(L, lua_upvalueindex(4)));                    //

      std::tr1::shared_ptr<LuaCFunAbstract> execParams(
          new LuaCFunExec<FunPtr>());
      std::tr1::shared_ptr<LuaCFunAbstract> emptyParams(
          new LuaCFunExec<FunPtr>());
      // Fill execParams. Function parameters start at index 2.
      execParams->pullParamsFromStack(L, 2);

      // Obtain reference to LuaScripting in order to invoke provenance.
      // See createCallableFuncTable for justification on pulling an
      // instance of LuaScripting out of Lua.
      bool provExempt = ss->doProvenanceFromExec(L, execParams, emptyParams);

      // Places the instance table and its metatable at stack position -2 and -1
      // respectively.
      LuaClassInstance inst = buildCoreInstanceTable(L, ss, consTable,
                                                     ss->getNewClassInstID());
      int mt = lua_gettop(L);
      int instTable = mt - 1;

      ss->beginCommand();
      try
      {
        r = LuaCFunExec<FunPtr>::run(L, 2, C, fp);        //
      }
      catch (std::exception& e)
      {
        ss->endCommand();
        ss->logExecFailure(e.what());
        throw;
      }
      catch (...)
      {
        ss->endCommand();
        ss->logExecFailure("");
        throw;
      }
      ss->endCommand();

      if (r != NULL)
      {
        // Call registration fptr.
        lua_getfield(L, consTable,
                     LuaClassConstructor::CONS_MD_FUNC_REGISTRATION_FPTR);
        typename LuaClassRegCallback<CLS>::Type* cbFptr =
            reinterpret_cast<typename LuaClassRegCallback<CLS>::Type* >(
                lua_touserdata(L, -1));
        LuaClassRegistration<CLS> reg(ss, dynamic_cast<CLS*>(r),
                                      inst.getGlobalInstID());
        (*cbFptr)(reg, r, ss);
        lua_pop(L, 1);

        ss->doHooks(L, 1, provExempt);

        // Places function table on the top of the stack.
        finalize(L, ss, reinterpret_cast<void*>(r), inst, mt, instTable,
           reinterpret_cast<void*>(
               &LuaMemberConstructorCallback<CLS, FunPtr>::del),
           reinterpret_cast<void*>(
               &LuaMemberConstructorCallback<CLS, FunPtr>::delCallback)
           );
      }
      else
      {
        ss->vPrint("Failed to load class.");
        // Create a default LuaClassInstance and return that.
        LuaClassInstance inst;
        LuaStrictStack<LuaClassInstance>::push(L, inst);
      }

      return 1;
    }

    // Casts the void* to the appropriate type, and deletes the class
    // instance.
    static void del(void* inst)
    {
      // Cast and delete the class instance.
      // NOTE: returnType should already be a pointer type.
      delete (reinterpret_cast<typename LuaCFunExec<FunPtr>::returnType>(
          inst));
    }

    static void delCallback(void* callback)
    {
      delete (reinterpret_cast<typename LuaClassRegCallback<CLS>::Type* >(
          callback));
    }
  };


  LuaScripting*     mSS;

  static LuaClassInstance buildCoreInstanceTable(lua_State* L, LuaScripting* ss,
                                                 int consTable, int instID);
  static void finalize(lua_State* L, LuaScripting* ss, void* r,
                       LuaClassInstance inst, int mt, int instTable,
                       void* delFun, void* delCallbackFun);


  // Utility functions for buildCoreInstanceTable and finalize.
  static void addToLookupTable(LuaScripting* ss, lua_State* L, void* ptr,
                               int instID);
  static int createCoreMetatable(lua_State* L,
                                 int instID,
                                 int consTable);
  static void finalizeMetatable(lua_State* L, int mt, void* ptr, void* delPtr,
                                void* delCallbackPtr);
  static LuaClassInstance finalizeInstanceTable(LuaScripting* ss,
                                                int instTable, int instID);


};

} /* namespace tuvok */
#endif /* LUACLASSCONSTRUCTOR_H_ */

