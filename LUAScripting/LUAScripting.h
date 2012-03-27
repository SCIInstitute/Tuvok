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

#ifndef LUASCRIPTING_H_
#define LUASCRIPTING_H_

namespace tuvok
{

//===============================
// LUA BINDING HELPER STRUCTURES
//===============================

// These structures were created in order to handle void return types *easily*
// and without code duplication.

/// TODO: Store LUAScripting class as an upvalue.

template <typename FunPtr, typename Ret>
struct LUACallback
{
  static int exec(lua_State* L)
  {
    FunPtr fp = (FunPtr)lua_touserdata(L, lua_upvalueindex(1));
    Ret r = LUACFunExec<FunPtr>::run(fp, L);
    LUAStrictStack<Ret>().push(L, r);
    return 1;
  }
};

// Without a return value.
template <typename FunPtr>
struct LUACallback <FunPtr, void>
{
  static int exec(lua_State* L)
  {
    FunPtr fp = (FunPtr)lua_touserdata(L, lua_upvalueindex(1));
    LUACFunExec<FunPtr>::run(fp, L);
    return 0;
  }
};

//=====================
// LUA SCRIPTING CLASS
//=====================

// TODO: Add pointer to member variables and user data.

class LUAScripting
{
  friend class LUAMemberHook; // For adding member function hooks.
  friend class LUAMemberReg;  // For adding member function regisration.
public:

  LUAScripting();
  virtual ~LUAScripting();

  /// Function description returned from getFuncDescs().
  struct FunctionDesc
  {
    std::string funcName;   ///< Name of the function.
    std::string funcDesc;   ///< Description of the function provided by the
                            ///< registrar.
    std::string funcSig;    ///< Function signature includes the function name.
  };

  /// Return all function descriptions.
  /// This vector can be very large. This function will not generally be used
  /// in performance critical code.
  std::vector<FunctionDesc> getAllFuncDescs() const;

  /// Unregisters the function associated with the fully qualified name.
  void unregisterFunction(const std::string& fqName);

  /// Hooks the indicated fully qualified function name with the given function.
  /// \param  fqName    Fully qualified name to hook.
  /// \param  f         Function pointer.
  //
  /// TO HOOK MEMBER FUNCTIONS: To install hooks using member functions,
  /// use the LUAMemberHook mediator class. This class will automatically
  /// unhook all hooked member functions through its destructor.
  template <typename FunPtr>
  void strictHook(FunPtr f, const std::string& fqName)
  {
    // Need to check the signature of the function that we are trying to bind
    // into the script system.
  }

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
  ///               just call: renderer.eye( ... ) in LUA.
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
    lua_CFunction proxyFunc = &LUACallback<FunPtr, typename
        LUACFunExec<FunPtr>::returnType>::exec;
    createCallableFuncTable(proxyFunc, (void*)f, NULL);

    int tableIndex = lua_gettop(mL);

    // Add function metadata to the table.
    std::string sig = LUACFunExec<FunPtr>::getSignature("");
    std::string sigWithName = LUACFunExec<FunPtr>::getSignature(
        getUnqualifiedName(name));
    populateWithMetadata(name, desc, sig, sigWithName, tableIndex);

    // Push default values for function parameters onto the stack.
    LUACFunExec<FunPtr> defaultParams = LUACFunExec<FunPtr>();
    lua_checkstack(mL, 10); // Max num parameters accepted by the system.
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

  // This function is to be used only for testing.
  // Upgrade to a shared_ptr if it is to be used outside the LUA scripting
  // system.
  lua_State* getLUAState()  {return mL;}

  // Names for data stored in a function's encapsulating table.
  // Exposed for unit testing purposes.
  static const char* TBL_MD_DESC;         ///< Description
  static const char* TBL_MD_SIG;          ///< Signature
  static const char* TBL_MD_SIG_NAME;     ///< Signature with name
  static const char* TBL_MD_NUM_EXEC;     ///< Number of executions
  static const char* TBL_MD_QNAME;        ///< Fully qualified function name
  static const char* TBL_MD_FUN_PDEFS;    ///< Function parameter defaults
  static const char* TBL_MD_FUN_LAST_EXEC;///< Parameters from last execution

private:


  /// Returns true if the table at stackIndex is a registered function.
  bool isRegisteredFunction(int stackIndex) const;

  /// Retrieves the function table with the associated with the fully qualified
  /// function name.
  /// Places the function table at the top of the stack. If the function fails
  /// to find the function table, false is returned and nothing is pushed
  /// onto the stack.
  bool getFunctionTable(const std::string& fqName);

  /// Creates a callable LUA table. classInstance can be NULL.
  /// Leaves the table on the top of the LUA stack.
  void createCallableFuncTable(lua_CFunction proxyFunc,
                               void* realFuncToCall,
                               void* classInstance);

  /// Populates the table at the given index with the given function metadata.
  void populateWithMetadata(const std::string& name,
                            const std::string& description,
                            const std::string& signature,
                            const std::string& signatureWithName,
                            int tableIndex);

  /// Creates the defaults and last exec tables, and places them inside the
  /// table given at tableIndex.
  /// Expects that the parameters are at the top of the stack.
  void createDefaultsAndLastExecTables(int tableIndex, int numParams);

  /// Binds the closure given at closureIndex to the fully qualified name (fq).
  /// ensureModuleExists and bindClosureToTable are helper functions.
  void bindClosureTableWithFQName(const std::string& fqName, int closureIndex);

  /// Retrieves the unqualified name given the qualified name.
  static std::string getUnqualifiedName(const std::string& fqName);

  /// Recursive function helper for getAllFuncDescs.
  void getTableFuncDefs(std::vector<LUAScripting::FunctionDesc>& descs) const;

  /// LUA panic function. LUA calls this when an unrecoverable error occurs
  /// in the interpreter.
  static int luaPanic(lua_State* L);

  /// Customized memory allocator called from within LUA.
  static void* luaInternalAlloc(void* ud, void* ptr, size_t osize, size_t nsize);


  /// The one true LUA state.
  lua_State*                mL;

  /// List of registered modules/functions that in LUA's global table.
  /// Used to iterate through all registered functions and return a description
  /// related to those functions.
  std::vector<std::string>  mRegisteredGlobals;


};


} /* namespace tuvok */
#endif /* LUASCRIPTING_H_ */
