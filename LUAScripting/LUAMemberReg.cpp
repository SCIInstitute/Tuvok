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
 \file    LUAMemberReg.cpp
 \author  James Hughes
          SCI Institute
          University of Utah
 \date    Mar 26, 2012
 \brief   This mediator class handles member function registration.
          Should be instantiated alongside an encapsulating class.
 */


#ifndef EXTERNAL_UNIT_TESTING

#include "Controller/Controller.h"
#include "3rdParty/LUA/lua.hpp"

#else

#include "assert.h"
#include "utestCommon.h"

#endif

#include <vector>

#include "LUAError.h"
#include "LUAFunBinding.h"
#include "LUAScripting.h"
#include "LUAMemberReg.h"

using namespace std;

namespace tuvok
{

//-----------------------------------------------------------------------------
LuaMemberReg::LuaMemberReg(tr1::shared_ptr<LuaScripting> scriptSys)
: LuaMemberRegUnsafe(scriptSys.get())
, mScriptSystem_shared(scriptSys)
{

}

//-----------------------------------------------------------------------------
LuaMemberReg::~LuaMemberReg()
{
  // We need to make this call since LuaMemberRegUnsafe assumes that any
  // reference to mScriptSystem is unsafe in its destructor (since the shared
  // pointer we are using in derived classes will go out of scope before
  // its destructor is called).
  unregisterAll();
}

//==============================================================================
//
// UNIT TESTING
//
//==============================================================================

#ifdef EXTERNAL_UNIT_TESTING

SUITE(LuaTestMemberFunctionRegistration)
{
  class A
  {
  public:
    A(tr1::shared_ptr<LuaScripting> ss)
    : mReg(ss)
    {}

    bool m1()           {return true;}
    bool m2(int a)      {return (a > 40) ? true : false;}
    string m3(float a)  {return "Test str";}
    int m4(string reg)
    {
      printf("Str Print: %s\n", reg.c_str());
      return 67;
    }
    void m5()           {printf("Test scoping.\n");}

    int hookm2_var;
    void hookm2(int a)
    {
      hookm2_var = a;
    }

    float hookm3_var;
    void hookm3(float a)
    {
      hookm3_var = a;
    }

    // The registration instance.
    LuaMemberReg  mReg;
  };

  TEST(MemberFunctionRegistration)
  {
    TEST_HEADER;

    auto_ptr<A> a;  // Want unique_ptr
    lua_State* L;   // Storing this is a big no-no in real code. It is to test
                    // the scoping below.

    // Scope out the scripting system.
    {
      tr1::shared_ptr<LuaScripting> sc(new LuaScripting());
      L = sc->getLUAState();

      a = auto_ptr<A>(new A(sc));

      a->mReg.registerFunction(a.get(), &A::m1, "a.m1", "A::m1");
      a->mReg.registerFunction(a.get(), &A::m2, "a.m2", "A::m2");
      a->mReg.registerFunction(a.get(), &A::m3, "a.m3", "A::m3");
      a->mReg.registerFunction(a.get(), &A::m4, "m4",   "A::m4");

      luaL_dostring(L, "return a.m1()");
      CHECK_EQUAL(1, lua_toboolean(L, -1));
      lua_pop(L, 1);

      luaL_dostring(L, "return a.m2(41)");
      CHECK_EQUAL(1, lua_toboolean(L, -1));
      lua_pop(L, 1);

      luaL_dostring(L, "return a.m2(40)");
      CHECK_EQUAL(0, lua_toboolean(L, -1));
      lua_pop(L, 1);

      luaL_dostring(L, "return a.m3(4.2)");
      CHECK_EQUAL("Test str", lua_tostring(L, -1));
      lua_pop(L, 1);

      luaL_dostring(L, "return m4(\"This is my string\")");
      CHECK_EQUAL(67, lua_tointeger(L, -1));
      lua_pop(L, 1);
    }

    // Test more registrations even though the shared_ptr that created the
    // scripting system is now out of scope.
    {
      a->mReg.registerFunction(a.get(), &A::m5, "a.m5", "A::m5");
      luaL_dostring(L, "a.m5()");
    }
  }

  TEST(MemberFunctionDeregistration)
  {
    TEST_HEADER;

    tr1::shared_ptr<LuaScripting> sc(new LuaScripting());
    lua_State* L = sc->getLUAState();

    {
      auto_ptr<A> a(new A(sc));

      a->mReg.registerFunction(a.get(), &A::m1, "a.m1", "A::m1");
      a->mReg.registerFunction(a.get(), &A::m2, "a.m2", "A::m2");
      a->mReg.registerFunction(a.get(), &A::m5, "a.m5", "A::m5");

      luaL_dostring(L, "a.m5()");
    }

    {
      // Now check that all of the registered functions no longer exist.
      luaL_dostring(L, "return a.m1");
      CHECK_EQUAL(1, lua_isnil(L, -1));
      luaL_dostring(L, "return a.m2");
      CHECK_EQUAL(1, lua_isnil(L, -1));
      luaL_dostring(L, "return a.m5");
      CHECK_EQUAL(1, lua_isnil(L, -1));
    }
  }

  TEST(MemberFunctionCallHooksAndDereg)
  {
    TEST_HEADER;

    tr1::shared_ptr<LuaScripting> sc(new LuaScripting());
    lua_State* L = sc->getLUAState();

    auto_ptr<A> a(new A(sc));

    a->mReg.registerFunction(a.get(), &A::m1, "m1", "A::m1");
    a->mReg.registerFunction(a.get(), &A::m2, "m2", "A::m2");
    a->mReg.registerFunction(a.get(), &A::m3, "m3", "A::m3");
    a->mReg.registerFunction(a.get(), &A::m4, "m4", "A::m4");
    a->mReg.registerFunction(a.get(), &A::m5, "m5", "A::m5");

    a->mReg.strictHook(a.get(), &A::hookm2, "m2");
    a->mReg.strictHook(a.get(), &A::hookm3, "m3");

    luaL_dostring(L, "m2(34)");
    luaL_dostring(L, "m3(6.3)");

    // Check
    CHECK_EQUAL(34, a->hookm2_var);
    CHECK_CLOSE(6.3, a->hookm3_var, 0.001);

    CHECK_THROW(a->mReg.strictHook(a.get(), &A::hookm2, "m1a"),
                    LuaNonExistantFunction);

    CHECK_THROW(a->mReg.strictHook(a.get(), &A::hookm2, "m1"),
                LuaInvalidFunSignature);

    CHECK_THROW(a->mReg.strictHook(a.get(), &A::hookm2, "m2"),
                LuaFunBindError);

    a->mReg.unregisterHooks();

    // Test hooks with new values, and ensure they remain unchanged.
    luaL_dostring(L, "m2(42)");
    luaL_dostring(L, "m3(42.2)");

    CHECK_EQUAL(34, a->hookm2_var);
    CHECK_CLOSE(6.3, a->hookm3_var, 0.001);

    // Retest hooking after deregistration.
    a->mReg.strictHook(a.get(), &A::hookm2, "m2");

    luaL_dostring(L, "m2(452)");

    CHECK_EQUAL(452, a->hookm2_var);
  }

}

#endif

} /* namespace tuvok */



