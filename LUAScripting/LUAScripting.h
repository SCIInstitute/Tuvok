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

template <typename FunPtr, typename Ret >
struct LUACallback
{
  static int exec(lua_State* L)
  {
    FunPtr fp = (FunPtr)lua_touserdata(L, lua_upvalueindex(1));
    Ret r = LUACFunExec<FunPtr>().run(fp, L);
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
    LUACFunExec<FunPtr>().run(fp, L);
    return 0;
  }
};

//=====================
// LUA SCRIPTING CLASS
//=====================


class LUAScripting
{
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

  // Ability to unregister all functions?


  // Return all function descriptions.
  // This vector can be very large. This function will not generally be used
  // in performance critical code. If it was, pass an internal reference
  // to a non-local vector.
  std::vector<FunctionDesc> getAllFuncDescs() const;


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
  /// TODO: Allow registration of member functions.
  template <typename FunPtr>
  void registerFunction(FunPtr f, const std::string& name,
                        const std::string& desc)
  {
    int initStackTop = lua_gettop(mL);

    // Idea: Build a 'callable' table.
    // Its metatable will have a __call metamethod that points to the
    // function closure.

    // We do this because all metatables are unique per-type which eliminates
    // the possibilty of using a metatable on the function closure itself.
    // The only exception to this rule is the table type, its metatable is
    // unique per table.

    // Table containing the function closure.
    lua_newtable(mL);
    int tableIndex = lua_gettop(mL);

    // Create a new metatable
    lua_newtable(mL);

    // Push C Closure containing our function pointer onto the LUA stack.
    lua_pushlightuserdata(mL, (void*)f);
    lua_pushcclosure(mL,
                     &LUACallback<FunPtr, typename
                       LUACFunExec<FunPtr>::returnType>::exec,
                     1);

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

    // Function description
    lua_pushstring(mL, TBL_MD_DESC);
    lua_pushstring(mL, desc.c_str());
    lua_settable(mL, tableIndex);

    // Function signature
    lua_pushstring(mL, TBL_MD_SIG);
    lua_pushstring(mL, LUACFunExec<FunPtr>::getSignature("").c_str());
    lua_settable(mL, tableIndex);

    // Function signature with name
    lua_pushstring(mL, TBL_MD_SIG_NAME);
    lua_pushstring(mL, LUACFunExec<FunPtr>::
                   getSignature(getUnqualifiedName(name)).c_str());
    lua_settable(mL, tableIndex);

    // Number of times this function has been executed
    // (takes into account undos, so if a function is undone then this
    //  count will decrease).
    lua_pushstring(mL, TBL_MD_NUM_EXEC);
    lua_pushnumber(mL, 0);
    lua_settable(mL, tableIndex);

    // Fully qualified function name.
    lua_pushstring(mL, TBL_MD_QNAME);
    lua_pushstring(mL, name.c_str());
    lua_settable(mL, tableIndex);

    // Create and populate the defaults table.
    lua_newtable(mL);

    int defTablePos = lua_gettop(mL);
    LUACFunExec<FunPtr> defaultParams = LUACFunExec<FunPtr>();
    lua_checkstack(mL, 10); // This should be the maximum number of parameters
                            // accepted by the system.
    defaultParams.pushParamsToStack(mL);

    int defParamBot = lua_gettop(mL);
    int numFunParams = defParamBot - defTablePos;

    // Generate ordered default parameter table.
    // XXX: We could blow the LUA stack depending on the number of parameters
    // given in the function. Need to resize the size of the lua stack
    // dynamically.
    for (int i = 0; i < numFunParams; i++)
    {
      int stackIndex = defTablePos + i + 1;
      lua_pushnumber(mL, i);
      lua_pushvalue(mL, stackIndex);
      lua_settable(mL, defTablePos);
    }

    // Remove parameters from the stack.
    lua_settop(mL, defTablePos);

    // Insert defaults table in closure metatable.
    lua_pushstring(mL, TBL_MD_FUN_PDEFS);
    lua_pushvalue(mL, -2);
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

    // Install the closure in the appropriate module based on its
    // fully qualified name.
    bindClosureTableWithFQName(name, tableIndex);

    lua_pop(mL, 1);   // Pop the closure table.

    // This is a programmer error, NOT forwardly visible to the user.
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

  /// List of registered modules/functions that are in the globals tabl
  /// (built from the functions that are registered with the system).
  /// Used to iterate through all registered functions and return metadata
  /// associated with those functions.
  std::vector<std::string>  mRegisteredGlobals;


};


} /* namespace tuvok */
#endif /* LUASCRIPTING_H_ */
