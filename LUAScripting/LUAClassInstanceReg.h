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

#include "LuaClassInstance.h"

namespace tuvok
{

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

  /// The constructors are called from LuaScripting.

  /// Since LuaClassInstanceReg is used in a limited capacity, and only by the
  /// LuaScripting class, we use a non-shared pointer to LuaScripting.
  LuaClassInstanceReg(LuaScripting* scriptSys, const std::string& fqName,
                      bool doConstruction);

  /// Pointer to our encapsulating class.
  LuaScripting*             mSS;
  std::string               mClassPath; ///< fqName passed into init.
  bool                      mDoConstruction;

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


      // TODO: Take pointer, and create appropriate instance table.
      // **** Functionize ****
      // Create instance table (including its functions)

      // Assign instance table to global index (we need to determine what
      // global index to use, likely just a variable in LuaScripting::
      // SYSTEM_TABLE .
      // **** Functionize ****

      LuaStrictStack<LuaClassInstance>().push(L, r);
      return 1;
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
    std::string sig = LuaCFunExec<FunPtr>::getSignature("");
    std::string sigWithName = LuaCFunExec<FunPtr>::getSignature(
        mSS->getUnqualifiedName(name));
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
    LuaStackRAII _a = new LuaStackRAII(L, 0);

    // Register 'new'. This will automatically create the necessary class table.
    std::string fqFunName = mClassPath;
    fqFunName += ".new";
    registerConstructor(f, fqFunName, desc, true);

    // Since we did the registerConstructor above we will have our function
    // table present.
    std::ostringstream os;
    os << "return " << mClassPath;
    luaL_dostring(L, os.str().c_str());

    // Now we have the class table on the top of the stack
    int funTable = lua_gettop(L);

    // Setup metatable (just calls the new function)
    {
      std::ostringstream os;
      os << fqFunName << "()";
      lua_newtable(L);
      luaL_loadstring(L, os.str().c_str());
      lua_setfield(L, -2, "__call");

      lua_setmetatable(L, funTable);
    }

    // Delete function
    // TODO

    // typeID
    // TODO
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
    // Function pointer f is guaranteed to be a member function pointer.
  }
}

} /* namespace tuvok */
#endif /* TUVOK_LUAINSTANCEREG_H_ */
