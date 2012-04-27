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
 \brief Provides a C++ class proxy in Lua.
 */

#ifndef TUVOK_LUACLASSINSTANCEREG_H_
#define TUVOK_LUACLASSINSTANCEREG_H_

#include "LUAClassInstance.h"
#include "LUAMemberRegUnsafe.h"
#include "LUAProvenance.h"

namespace tuvok
{

/// Testing documentation
class LuaClassInstanceReg
{
  friend class LuaScripting;
public:

  /// Registers a constructor. The constructor should return an instance
  /// of the class.
  template <typename FunPtr>
  void constructor(FunPtr f, const std::string& desc);

  /// Registers a member function. Same parameters as
  /// LuaScripting::registerFunction, with the unqualifiedName parameter.
  /// unqualifiedName should be just the name of the function to register,
  /// and should not include any module paths.
  template <typename FunPtr>
  void function(FunPtr f, const std::string& unqualifiedName,
                const std::string& desc, bool undoRedo);

private:


  /// Called by LuaScripting to construct a class instance table.
  explicit LuaClassInstanceReg(LuaScripting* scriptSys,
                               const std::string& fqName,
                               LuaScripting::ClassDefFun f);

  /// Used internally to construct instances of a class from a class instance
  /// table. Instances are initialized at instanceLoc.
  explicit LuaClassInstanceReg(LuaScripting* scriptSys,
                               const std::string& instanceLoc,
                               void* instance);

  /// Pointer to our encapsulating class.
  LuaScripting*                     mSS;
  std::string                       mClassPath; ///< fqName passed into init.

  bool                              mDoConstruction;
  LuaScripting::ClassDefFun         mClassDefinition;
  void*                             mClassInstance;

  // We use the unsafe version because we don't want our functions deleted
  // when this class goes out of scope, and we do not have access to a
  // shared_ptr instance of LuaScripting system.
  std::auto_ptr<LuaMemberRegUnsafe> mRegistration;

  /// Data stored in the constructor table
  ///@{
  static const char*    CONS_MD_CLASS_DEFINITION;
  static const char*    CONS_MD_FACTORY_NAME;
  ///@}

  /// Signature for delFun in LuaConstructorCallback.
  typedef void (*DelFunSig)(LuaScripting* ss, int id, void* inst);

  template <typename FunPtr>
  struct LuaConstructorCallback
  {
    static int exec(lua_State* L)
    {
      LuaStackRAII _a = LuaStackRAII(L, 1); // 1 return value.

      // The function table that called us on the top of the stack.
      int newFunTableIndex = 1;

      FunPtr fp = reinterpret_cast<FunPtr>(
          lua_touserdata(L, lua_upvalueindex(1)));

      void* r = NULL;

      LuaScripting* ss = static_cast<LuaScripting*>(
                  lua_touserdata(L, lua_upvalueindex(3)));

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

      // It is important that we obtain our class ID before any composited
      // classes. This has to do with the way we are destroyed when undoing
      // a constructor. The parent class is always deleted first, which
      // will cascade delete all children (in order).
      //
      // In other words, do not move this line below the
      // LuaCFunExec<FunPtr>::run below. This will cause the parent Lua class
      // having the highest global ID (which doesn't make sense anyways).
      int instID = ss->getNewClassInstID();

      // We are NOT a hook. Our parameters start at index 2 (because the
      // callable table is at the first index). We will want to call all
      // hooks associated with our table.
      ss->beginCommand();
      try
      {
         r = static_cast<void*>(LuaCFunExec<FunPtr>::run(L, 2, fp));
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

      // Call registered hooks.
      // Note: The first parameter on the stack (not on the top, but
      // on the bottom) is the table associated with the function.
      ss->doHooks(L, 1, provExempt);

      // Grab extra data from constructor table.
      lua_getfield(L, newFunTableIndex, CONS_MD_CLASS_DEFINITION);
      LuaScripting::ClassDefFun fDef =
          reinterpret_cast<LuaScripting::ClassDefFun>(lua_touserdata(L, -1));
      lua_pop(L, 1);

      lua_getfield(L, newFunTableIndex, CONS_MD_FACTORY_NAME);
      std::string factoryFQName = lua_tostring(L, -1);
      lua_pop(L, 1);

      // Create an instance of this class.
      // First we build where we will be placing this class instance
      // (in the global instance table).
      std::string instanceLoc;
      {
        std::ostringstream os;
        os << LuaClassInstance::CLASS_INSTANCE_TABLE;
        os << "." << LuaClassInstance::CLASS_INSTANCE_PREFIX << instID;
        instanceLoc = os.str();
      }

      // Build instance table where we will place class instance table.
      lua_newtable(L); // (1)

      // Metatable
      lua_newtable(L);

      lua_pushinteger(L, instID);
      lua_setfield(L, -2, LuaClassInstance::MD_GLOBAL_INSTANCE_ID);

      lua_pushstring(L, factoryFQName.c_str());
      lua_setfield(L, -2, LuaClassInstance::MD_FACTORY_NAME);

      lua_pushlightuserdata(L, r);
      lua_setfield(L, -2, LuaClassInstance::MD_INSTANCE);

      lua_pushlightuserdata(L, reinterpret_cast<void*>(
          &LuaConstructorCallback<FunPtr>::del));
      lua_setfield(L, -2, LuaClassInstance::MD_DEL_FUN);

      lua_setmetatable(L, -2);

      // Bind (1)
      ss->bindClosureTableWithFQName(instanceLoc, lua_gettop(L));
      lua_pop(L, 1);  // Pop instance table.

      // Construct functions in our instance table.
      LuaClassInstanceReg reg(ss, instanceLoc, r);
      fDef(reg);

      // Pass back our instance.
      LuaClassInstance instance(instID);
      if (ss->getFunctionTable(instance.fqName()) == false)
      {
        throw LuaFunBindError("Unable to find table after it was created!");
      }

      // Add this instance to the provenance system.
      ss->getProvenanceSys()->addCreatedInstanceToLastURItem(instID);

      return 1;
    }

    // Casts the void* to the appropriate type, and deletes the class
    // instance.
    static void del(LuaScripting* ss, int globalInstID, void* inst)
    {
      //addDeletedInstanceToLastURItem
      ss->getProvenanceSys()->addDeletedInstanceToLastURItem(globalInstID);

      // Cast and delete the class instance.
      // NOTE: returnType should already be a pointer type.
      delete (reinterpret_cast<typename LuaCFunExec<FunPtr>::returnType>(
          inst));
    }
  };

  template <typename FunPtr>
  void registerConstructor(FunPtr f, const std::string& name,
                           const std::string& desc, bool undoRedo)
  {
    lua_State* L = mSS->getLUAState();

    LuaStackRAII _a = LuaStackRAII(L, 0);

    // Idea: Build a 'callable' table.
    // Its metatable will have a __call metamethod that points to the C
    // function closure.

    // We do this because all metatables are unique per-type which eliminates
    // the possibilty of using a metatable on the function closure itself.
    // The only exception to this rule is the table type itself.
    int initStackTop = lua_gettop(L);

    // Create a callable function table and leave it on the stack.
    lua_CFunction proxyFunc = &LuaConstructorCallback<FunPtr>::exec;
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

#ifdef TUVOK_DEBUG_LUA_USE_RTTI_CHECKS
    // Generate the type table (buildTypeTable places table on top of the stack).
    LuaCFunExec<FunPtr>::buildTypeTable(L);
    lua_setfield(L, tableIndex, LuaScripting::TBL_MD_TYPES_TABLE);
#endif

    // Install the callable table in the appropriate module based on its
    // fully qualified name.
    mSS->bindClosureTableWithFQName(name, tableIndex);

    lua_pop(L, 1);   // Pop the callable table.

    if (undoRedo == false)  mSS->setUndoRedoStackExempt(name);

    assert(initStackTop == lua_gettop(L));
  }

};

template <typename FunPtr>
void LuaClassInstanceReg::constructor(FunPtr f, const std::string& desc)
{
  // Register f as the function _new. This function will be called to construct
  // a pointer to the class we want to create. This function is guaranteed
  // to be a static function.
  if (mDoConstruction)
  {
    // Construct 'constructor' table.
    lua_State* L = mSS->getLUAState();
    LuaStackRAII _a(L, 0);

    // Register 'new'. This will automatically create the necessary class table.
    std::string fqFunName = mClassPath;
    fqFunName += ".new";
    registerConstructor(f, fqFunName, desc, true);
    mSS->setNullUndoFun(fqFunName); // We do not want to exe new when undoing
                                    // All child undo items are still executed.

    // Since we did the registerConstructor above we will have our function
    // table present.
    if (mSS->getFunctionTable(mClassPath) == false)
      throw LuaFunBindError("Unable to find table we just built!");

    // Now we have the class table on the top of the stack
    //int funTable = lua_gettop(L);

    // TODO: Get metatable working (need to pass parameters through --
    // if __call points to new table, then __call needs to be weak).
//    // Setup metatable (just calls the new function)
//    {
//      std::ostringstream os;
//      os << fqFunName << "()";
//      lua_newtable(L);
//      luaL_loadstring(L, os.str().c_str());
//      lua_setfield(L, -2, "__call");
//
//      lua_setmetatable(L, funTable);
//    }

    // TODO typeID

    // Pop function table off stack.
    lua_pop(L, 1);
  }
}

template <typename FunPtr>
void LuaClassInstanceReg::function(FunPtr f,
                                   const std::string& unqualifiedName,
                                   const std::string& desc,
                                   bool undoRedo)
{
  if (mDoConstruction == false)
  {
    assert(mRegistration.get());

    std::ostringstream qualifiedName;
    qualifiedName << mClassPath << "." << unqualifiedName;

    // Function pointer f is guaranteed to be a member function pointer.
    mRegistration->registerFunction(
        reinterpret_cast<typename LuaCFunExec<FunPtr>::classType*>(
            mClassInstance),
        f, qualifiedName.str(), desc, undoRedo);
  }
}

} /* namespace tuvok */
#endif /* TUVOK_LUAINSTANCEREG_H_ */
