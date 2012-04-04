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
 \file    LUAScripting.h
 \author  James Hughes
          SCI Institute
          University of Utah
 \date    Mar 21, 2012
 \brief   Interface to the LUA scripting system built for Tuvok.
          Made to be unit tested externally.
 */

#ifndef TUVOK_LUASCRIPTING_H_
#define TUVOK_LUASCRIPTING_H_

namespace tuvok
{

class LuaProvenance;

class LuaScripting
{
  friend class LuaMemberRegUnsafe;  // For adding member function registration.
  friend class LuaProvenance;       // For obtaining function tables.

public:

  LuaScripting();             // Will generate a new Lua state.
  virtual ~LuaScripting();

  /// Default: Provenance is enabled.
  bool isProvenanceEnabled();
  void enableProvenance(bool enable);

  /// Function description returned from getFuncDescs().
  struct FunctionDesc
  {
    std::string funcName;   ///< Name of the function.
    std::string funcDesc;   ///< Description of the function provided by the
                            ///< registrar.
    std::string funcSig;    ///< Function signature, includes the function name.
  };

  /// Return all function descriptions.
  std::vector<FunctionDesc> getAllFuncDescs() const;

  /// These structures were created in order to handle void return types easily
  ///@{
  template <typename FunPtr, typename Ret>
  struct LuaCallback
  {
    static int exec(lua_State* L)
    {
      FunPtr fp = reinterpret_cast<FunPtr>(
          lua_touserdata(L, lua_upvalueindex(1)));

      Ret r;
      if (lua_toboolean(L, lua_upvalueindex(2)) == 0)
      {
        // We are NOT a hook. Our parameters start at index 2 (because the
        // callable table is at the first index). We will want to call all
        // hooks associated with our table.
        r = LuaCFunExec<FunPtr>::run(L, 2, fp);

        std::tr1::shared_ptr<LuaCFunAbstract> execParams(
            new LuaCFunExec<FunPtr>());
        std::tr1::shared_ptr<LuaCFunAbstract> emptyParams(
            new LuaCFunExec<FunPtr>());
        // Fill execParams. Function parameters start at index 2.
        execParams->pullParamsFromStack(L, 2);

        // Obtain reference to LuaScripting in order to invoke provenance.
        // See createCallableFuncTable for justification on pulling an
        // instance of LuaScripting out of Lua.
        LuaScripting* ss = static_cast<LuaScripting*>(
            lua_touserdata(L, lua_upvalueindex(3)));
        ss->doProvenanceFromExec(L, execParams, emptyParams);

        // Call registered hooks.
        // Note: The first parameter on the stack (not on the top, but
        // on the bottom) is the table associated with the function.
        LuaScripting::doHooks(L, 1);
      }
      else
      {
        r = LuaCFunExec<FunPtr>::run(L, 1, fp);
      }

      LuaStrictStack<Ret>().push(L, r);
      return 1;
    }
  };

  template <typename FunPtr>
  struct LuaCallback <FunPtr, void>
  {
    static int exec(lua_State* L)
    {
      FunPtr fp = reinterpret_cast<FunPtr>(
          lua_touserdata(L, lua_upvalueindex(1)));
      if (lua_toboolean(L, lua_upvalueindex(2)) == 0)
      {
        LuaCFunExec<FunPtr>::run(L, 2, fp);

        std::tr1::shared_ptr<LuaCFunAbstract> execParams(
            new LuaCFunExec<FunPtr>());
        std::tr1::shared_ptr<LuaCFunAbstract> emptyParams(
            new LuaCFunExec<FunPtr>());
        // Fill execParams. Function parameters start at index 2.
        execParams->pullParamsFromStack(L, 2);

        // Obtain reference to LuaScripting in order to invoke provenance.
        // See createCallableFuncTable for justification on pulling an
        // instance of LuaScripting out of Lua.
        LuaScripting* ss = static_cast<LuaScripting*>(
            lua_touserdata(L, lua_upvalueindex(3)));
        ss->doProvenanceFromExec(L, execParams, emptyParams);

        LuaScripting::doHooks(L, 1);
      }
      else
      {
        LuaCFunExec<FunPtr>::run(L, 1, fp);
      }

      return 0;
    }
  };
  ///@}

  /// Registers a static C++ function with LUA.
  /// Since LUA is compiled as CPP, it is safe to throw exceptions from the
  /// function pointed to by f.
  /// \param  f     Any function pointer.
  ///               f's parameters and return value will be handled
  ///               automatically.
  ///               The number of parameters allowed in f is limited by
  ///               the templates in LUAFunBinding.h. If you need more
  ///               parameters, create the appropriate template in
  ///               LUAFunBinding.h.
  /// \param  name  Period delimited fully qualified name of f inside of LUA.
  ///               No characters other than those regularly allowed inside
  ///               C++ functions are allowed, with the exception of periods.
  ///               Example: "renderer.eye"
  ///               To call a function registered with the name in the example,
  ///               just call renderer.eye( ... ) in Lua.
  /// \param  desc  Description of f.
  //
  /// TO REGISTER MEMBER FUNCTIONS: To register member functions,
  /// use the LUAMemberReg mediator class. This class will automatically
  /// unregister all registered member functions through its destructor.
  template <typename FunPtr>
  void registerFunction(FunPtr f, const std::string& name,
                        const std::string& desc)
  {
    // Idea: Build a 'callable' table.
    // Its metatable will have a __call metamethod that points to the C
    // function closure.

    // We do this because all metatables are unique per-type which eliminates
    // the possibilty of using a metatable on the function closure itself.
    // The only exception to this rule is the table type itself.
    int initStackTop = lua_gettop(mL);

    // Create a callable function table and leave it on the stack.
    lua_CFunction proxyFunc = &LuaCallback<FunPtr, typename
        LuaCFunExec<FunPtr>::returnType>::exec;
    createCallableFuncTable(proxyFunc, reinterpret_cast<void*>(f));

    int tableIndex = lua_gettop(mL);

    // Add function metadata to the table.
    std::string sig = LuaCFunExec<FunPtr>::getSignature("");
    std::string sigWithName = LuaCFunExec<FunPtr>::getSignature(
        getUnqualifiedName(name));
    std::string sigNoRet = LuaCFunExec<FunPtr>::getSigNoReturn("");
    populateWithMetadata(name, desc, sig, sigWithName, sigNoRet, tableIndex);

    // Push default values for function parameters onto the stack.
    LuaCFunExec<FunPtr> defaultParams = LuaCFunExec<FunPtr>();
    lua_checkstack(mL, LUAC_MAX_NUM_PARAMS + 2);
    defaultParams.pushParamsToStack(mL);
    int numFunParams = lua_gettop(mL) - tableIndex;
    createDefaultsAndLastExecTables(tableIndex, numFunParams);

    int testPos = lua_gettop(mL);

    // Install the callable table in the appropriate module based on its
    // fully qualified name.
    bindClosureTableWithFQName(name, tableIndex);

    testPos = lua_gettop(mL);

    lua_pop(mL, 1);   // Pop the callable table.

    assert(initStackTop == lua_gettop(mL));
  }

  /// Hooks a fully qualified function name with the given function.
  /// All hooks are called directly after the bound Lua function is called,
  /// but before the return values (if any) are sent back to Lua.
  /// \param  f         Function pointer.
  /// \param  fqName    Fully qualified name to hook.
  //
  /// TO HOOK MEMBER FUNCTIONS: To install hooks using member functions,
  /// use the LUAMemberHook mediator class. This class will automatically
  /// unhook all hooked member functions through its destructor.
  template <typename FunPtr>
  void strictHook(FunPtr f, const std::string& name)
  {
    // Need to check the signature of the function that we are trying to bind
    // into the script system.
    int initStackTop = lua_gettop(mL);

    if (getFunctionTable(name) == false)
    {
      throw LuaNonExistantFunction("Unable to find function with which to"
                                   "associate a hook.");
    }

    int funcTable = lua_gettop(mL);

    // Check function signatures.
    lua_getfield(mL, funcTable, TBL_MD_SIG_NO_RET);
    std::string sigReg = lua_tostring(mL, -1);
    std::string sigHook = LuaCFunExec<FunPtr>::getSigNoReturn("");
    if (sigReg.compare(sigHook) != 0)
    {
      std::ostringstream os;
      os << "Hook's parameter signature and the parameter signature of the "
            "function to hook must match. Hook's signature: \"" << sigHook <<
            "\"Function to hook's signature: " << sigReg;
      throw LuaInvalidFunSignature(os.str().c_str());
    }
    lua_pop(mL, 1);

    // Obtain hook table.
    lua_getfield(mL, -1, TBL_MD_HOOKS);
    int hookTable = lua_gettop(mL);

    // Generate a hook descriptor (we make it a name as opposed to a natural
    // number because we want Lua to use the hash table implementation behind
    // the scenes, not the array table).
    lua_getfield(mL, funcTable, TBL_MD_HOOK_INDEX);
    std::ostringstream os;
    int hookIndex = lua_tointeger(mL, -1);
    os << "h" << hookIndex;
    lua_pop(mL, 1);
    lua_pushinteger(mL, hookIndex + 1);
    lua_setfield(mL, funcTable, TBL_MD_HOOK_INDEX);

    // Push closure
    lua_CFunction proxyFunc = &LuaCallback<FunPtr, typename
            LuaCFunExec<FunPtr>::returnType>::exec;
    lua_pushlightuserdata(mL, reinterpret_cast<void*>(f));
    lua_pushboolean(mL, 1);  // We ARE a hook. This affects the stack, and
                             // whether we want to perform provenance.
    // As a hook, we don't need to add mScriptSystem to the closure, since
    // we don't perform provenance on hooks.
    lua_pushcclosure(mL, proxyFunc, 2);

    // Associate closure with hook table.
    lua_setfield(mL, hookTable, os.str().c_str());
  }

  /// Ensures the function is not added to the undo/redo stack. Examples include
  /// the undo and redo functions themselves.
  void setUndoRedoStackExempt(const std::string& funcName, bool exempt);

  /// NOTE: We cannot use shared_ptr for the lua_State. It allocates memory
  /// for this structure using luaInternalAlloc, and a call to delete, without
  /// informing Lua, could be disastrous.
  lua_State* getLUAState()  {return mL;}

  // Names for data stored in a function's encapsulating table.
  // Exposed for unit testing purposes.
  static const char* TBL_MD_DESC;         ///< Description
  static const char* TBL_MD_SIG;          ///< Signature
  static const char* TBL_MD_SIG_NO_RET;   ///< No name signature without return
  static const char* TBL_MD_SIG_NAME;     ///< Signature with name
  static const char* TBL_MD_NUM_EXEC;     ///< Number of executions
  static const char* TBL_MD_QNAME;        ///< Fully qualified function name
  static const char* TBL_MD_FUN_PDEFS;    ///< Function parameter defaults
  static const char* TBL_MD_FUN_LAST_EXEC;///< Parameters from last execution
  static const char* TBL_MD_HOOKS;        ///< Static function hooks table
  static const char* TBL_MD_HOOK_INDEX;   ///< Static function hook index
  static const char* TBL_MD_MEMBER_HOOKS; ///< Class member function hook table
  static const char* TBL_MD_CPP_CLASS;    ///< Light user data to LuaScripting
  static const char* TBL_MD_STACK_EXEMPT; ///< True if undo/redo stack exempt

private:

  // NEVER deregister member functions, always let LuaMemberReg deregister
  // the functions.

  /// Unregisters the function associated with the fully qualified name.
  void unregisterFunction(const std::string& fqName);

  /// Unregisters all functions registered using this LuaScripting instance.
  /// Should NEVER be called outside LuaScripting's destructor.
  void unregisterAllFunctions();

  /// Returns a new member hook ID.
  std::string getNewMemberHookID();

  /// Recursive helper function for unregisterAllFunctions().
  /// Expects that the table from which to remove functions is on the top of the
  /// stack.
  /// \param  parentTable If 0, the table is remove from globals if it is a
  ///                     function.
  /// \param  tableName   Name of the table on the top of the stack.
  void removeFunctionsFromTable(int parentTable, const char* tableName);

  /// Stack expectations: the function table at tableIndex, and the parameters
  /// required to call the function directly after the table on the stack.
  /// There must be no other values on the stack above tableIndex other than the
  /// table and the parameters to call the function.
  static void doHooks(lua_State* L, int tableIndex);

  /// Returns true if the table at stackIndex is a registered function.
  /// Makes no guarantees that it is a function registered by this class.
  bool isRegisteredFunction(int stackIndex) const;

  /// Returns true if the function table at stackIndex was create by this
  /// LuaScripting class. For now, we create our own unique Lua instance, so
  /// we are guaranteed this, but this is forward thinking for the future
  /// when we need to share the Lua state.
  bool isOurRegisteredFunction(int stackIndex) const;

  /// Returns true if the given fully qualified function name would be stored
  /// in lua globlas.
  bool isGlobalFunction(std::string fqName);

  /// Retrieves the function table given the fully qualified function name.
  /// Places the function table at the top of the stack. If the function fails
  /// to find the function table, false is returned and nothing is pushed
  /// onto the stack.
  bool getFunctionTable(const std::string& fqName);

  /// Creates a callable LUA table. classInstance can be NULL.
  /// Leaves the table on the top of the LUA stack.
  void createCallableFuncTable(lua_CFunction proxyFunc,
                               void* realFuncToCall);

  /// Populates the table at the given index with the given function metadata.
  void populateWithMetadata(const std::string& name,
                            const std::string& description,
                            const std::string& signature,
                            const std::string& signatureWithName,
                            const std::string& sigNoReturn,
                            int tableIndex);

  /// Creates the defaults and last exec tables and places them inside the
  /// table given at tableIndex.
  /// Expects that the parameters are at the top of the stack.
  void createDefaultsAndLastExecTables(int tableIndex, int numParams);

  /// Binds the closure given at closureIndex to the fully qualified name (fq).
  void bindClosureTableWithFQName(const std::string& fqName, int closureIndex);

  /// Retrieves the unqualified name given the qualified name.
  static std::string getUnqualifiedName(const std::string& fqName);

  /// Recursive function helper for getAllFuncDescs.
  void getTableFuncDefs(std::vector<LuaScripting::FunctionDesc>& descs) const;

  /// LUA panic function. LUA calls this when an unrecoverable error occurs
  /// in the interpreter.
  static int luaPanic(lua_State* L);

  /// Customized memory allocator called from within LUA.
  static void* luaInternalAlloc(void* ud, void* ptr, size_t osize,
                                size_t nsize);

  void doProvenanceFromExec(lua_State* L,
                            std::tr1::shared_ptr<LuaCFunAbstract> funParams,
                            std::tr1::shared_ptr<LuaCFunAbstract> emptyParams);

  /// The one true LUA state.
  lua_State*                        mL;

  /// List of registered modules/functions in LUA's global table.
  /// Used to iterate through all registered functions.
  std::vector<std::string>          mRegisteredGlobals;

  /// Index used to assign a unique ID to classes that wish to register
  /// hooks.
  int                               mMemberHookIndex;

  std::auto_ptr<LuaProvenance>      mProvenance;

};

// TODO Class to unwind the LUA stack when an exception occurs.
//      Something akin to: LuaStackRAII(L) at the top of a function.
//      This will grab the index at the top of the lua stack, and rewind to
//      it in the class' destructor.
//
//      Look into whether or not important information could be on the stack
//      related to the error.
//
//      Note. This shouldn't be used in all functions. Some functions
//      purposefully leave values on the top of the stack. In these cases, we
//      could have a parameter in the constructor indicating how many values
//      to leave on the stack.

} /* namespace tuvok */
#endif /* LUASCRIPTING_H_ */
