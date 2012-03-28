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


namespace tuvok
{

class LUAScripting;

//===============================
// LUA BINDING HELPER STRUCTURES
//===============================

template <typename FunPtr, typename Ret>
struct LUAMemberCallback
{
  static int exec(lua_State* L)
  {
    FunPtr fp = *(FunPtr*)lua_touserdata(L, lua_upvalueindex(1));
    typename LUACFunExec<FunPtr>::classType* C =
        static_cast<typename LUACFunExec<FunPtr>::classType*>(
            lua_touserdata(L, lua_upvalueindex(2)));
    Ret r = LUACFunExec<FunPtr>::run(C, fp, L);
    LUAStrictStack<Ret>().push(L, r);
    return 1;
  }
};

// Without a return value.
template <typename FunPtr>
struct LUAMemberCallback <FunPtr, void>
{
  static int exec(lua_State* L)
  {
    FunPtr fp = *(FunPtr*)lua_touserdata(L, lua_upvalueindex(1));
    typename LUACFunExec<FunPtr>::classType* C =
        static_cast<typename LUACFunExec<FunPtr>::classType*>(
            lua_touserdata(L, lua_upvalueindex(2)));
    LUACFunExec<FunPtr>::run(C, fp, L);
    return 0;
  }
};


// Member registration mediator class.
class LUAMemberReg
{
public:

  LUAMemberReg(std::tr1::shared_ptr<LUAScripting> scriptSys);
  virtual ~LUAMemberReg();


  template <typename T, typename FunPtr>
  void registerFunction(T* C, FunPtr f, const std::string& name,
                        const std::string& desc)
  {
    LUAScripting* ss  = mScriptSystem.get();
    lua_State*    L   = ss->getLUAState();

    int initStackTop = lua_gettop(L);

    // Member function pointers are not pointing to a function, they are
    // compiler dependent and are pointing to a memory address.
    // They need to be copied into lua in an portable manner.
    // See the C++ standard.
    // Create a callable function table and leave it on the stack.
    lua_CFunction proxyFunc = &LUAMemberCallback<FunPtr, typename
        LUACFunExec<FunPtr>::returnType>::exec;

    // Table containing the function closure.
    lua_newtable(L);
    int tableIndex = lua_gettop(L);

    // Create a new metatable
    lua_newtable(L);

    // Create a full user data and store the function pointer data inside of it.
    void* udata = lua_newuserdata(L, sizeof(FunPtr));
    memcpy(udata, &f, sizeof(FunPtr));
    lua_pushlightuserdata(L, (void*)C);
    lua_pushcclosure(L, proxyFunc, 2);

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
    std::string sig = LUACFunExec<FunPtr>::getSignature("");
    std::string sigWithName = LUACFunExec<FunPtr>::getSignature(
        ss->getUnqualifiedName(name));
    ss->populateWithMetadata(name, desc, sig, sigWithName, tableIndex);

    // Push default values for function parameters onto the stack.
    LUACFunExec<FunPtr> defaultParams = LUACFunExec<FunPtr>();
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

private:

  /// Scripting system we are bound to.
  std::tr1::shared_ptr<LUAScripting>  mScriptSystem;

  /// Functions registered.
  /// Used to unregister all functions.
  std::vector<std::string>            mRegisteredFunctions;
};

}

#endif
