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
 \file    LuaMemberReg.cpp
 \author  James Hughes
          SCI Institute
          University of Utah
 \date    Mar 26, 2012
 \brief   This mediator class handles member function registration.
          Should be instantiated alongside an encapsulating class.
 */

#include <vector>

#include "LuaScripting.h"
#include "LuaMemberReg.h"

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

#ifdef LUASCRIPTING_UNIT_TESTS
#include "utestCommon.h"

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

    auto_ptr<A> a;

    tr1::shared_ptr<LuaScripting> sc(new LuaScripting());

    a = auto_ptr<A>(new A(sc));

    a->mReg.registerFunction(a.get(), &A::m1, "a.m1", "A::m1", true);
    a->mReg.registerFunction(a.get(), &A::m2, "a.m2", "A::m2", true);
    a->mReg.registerFunction(a.get(), &A::m3, "a.m3", "A::m3", true);
    a->mReg.registerFunction(a.get(), &A::m4, "m4",   "A::m4", true);

    CHECK_EQUAL(true, sc->execRet<bool>("a.m1()"));
    CHECK_EQUAL(true, sc->execRet<bool>("a.m2(41)"));
    CHECK_EQUAL(false, sc->execRet<bool>("a.m2(40)"));
    CHECK_EQUAL("Test str", sc->execRet<string>("a.m3(4.2)").c_str());
    CHECK_EQUAL(67, sc->execRet<int>("m4('This is my string')"));
  }

  TEST(MemberFunctionDeregistration)
  {
    TEST_HEADER;

    tr1::shared_ptr<LuaScripting> sc(new LuaScripting());

    {
      auto_ptr<A> a(new A(sc));

      a->mReg.registerFunction(a.get(), &A::m1, "a.m1", "A::m1", true);
      a->mReg.registerFunction(a.get(), &A::m2, "a.m2", "A::m2", true);
      a->mReg.registerFunction(a.get(), &A::m5, "a.m5", "A::m5", true);

      sc->exec("a.m5()");
    }

    /// TODO  Ensure LuaNonExistantFunction is thrown. This will have to be
    ///       coordinated with Lua somehow when using exec. Should be
    ///       straightforward with cexec.
    sc->setExpectedExceptionFlag(true);
    CHECK_THROW(sc->exec("a.m1()"), LuaError);
    CHECK_THROW(sc->exec("a.m2(34)"), LuaError);
    CHECK_THROW(sc->exec("a.m5()"), LuaError);
    sc->setExpectedExceptionFlag(false);
  }

  TEST(MemberFunctionCallHooksAndDereg)
  {
    TEST_HEADER;

    tr1::shared_ptr<LuaScripting> sc(new LuaScripting());

    auto_ptr<A> a(new A(sc));

    a->mReg.registerFunction(a.get(), &A::m1, "m1", "A::m1", true);
    a->mReg.registerFunction(a.get(), &A::m2, "m2", "A::m2", true);
    a->mReg.registerFunction(a.get(), &A::m3, "m3", "A::m3", true);
    a->mReg.registerFunction(a.get(), &A::m4, "m4", "A::m4", true);
    a->mReg.registerFunction(a.get(), &A::m5, "m5", "A::m5", true);

    a->mReg.strictHook(a.get(), &A::hookm2, "m2");
    a->mReg.strictHook(a.get(), &A::hookm3, "m3");

    sc->exec("m2(34)");
    sc->exec("m3(6.3)");

    // Check
    CHECK_EQUAL(34, a->hookm2_var);
    CHECK_CLOSE(6.3, a->hookm3_var, 0.001);

    sc->setExpectedExceptionFlag(true);
    CHECK_THROW(a->mReg.strictHook(a.get(), &A::hookm2, "m1a"),
                LuaNonExistantFunction);

    CHECK_THROW(a->mReg.strictHook(a.get(), &A::hookm2, "m1"),
                LuaInvalidFunSignature);

    CHECK_THROW(a->mReg.strictHook(a.get(), &A::hookm2, "m2"),
                LuaFunBindError);
    sc->setExpectedExceptionFlag(false);

    a->mReg.unregisterHooks();

    // Test hooks with new values, and ensure they remain unchanged.
    sc->exec("m2(42)");
    sc->exec("m3(42.2)");

    CHECK_EQUAL(34, a->hookm2_var);
    CHECK_CLOSE(6.3, a->hookm3_var, 0.001);

    // Retest hooking after deregistration.
    a->mReg.strictHook(a.get(), &A::hookm2, "m2");

    sc->exec("m2(452)");

    CHECK_EQUAL(452, a->hookm2_var);

    sc->exec("provenance.logProvRecord_toConsole()");
  }

  class B
  {
  public:

    B(tr1::shared_ptr<LuaScripting> ss)
    : mReg(ss)
    {
      i1 = 0; i2 = 0;
      f1 = 0.0f; f2 = 0.0f;
    }

    int     i1, i2;
    float   f1, f2;
    string  s1, s2;

    void set_i1(int i)    {i1 = i;}
    void set_i2(int i)    {i2 = i;}
    int get_i1()          {return i1;}
    int get_i2()          {return i2;}

    void set_f1(float f)  {f1 = f;}
    void set_f2(float f)  {f2 = f;}
    float get_f1()        {return f1;}
    float get_f2()        {return f2;}

    void set_s1(string s) {s1 = s;}
    void set_s2(string s) {s2 = s;}
    string get_s1()       {return s1;}
    string get_s2()       {return s2;}

    void undo_i1(int i)   {i1 = i * 2;}
    void undo_f1(float f) {f1 = f + 2.5f;}
    void undo_s1(string s){s1 = s + "hi";}

    void redo_i1(int i)   {i1 = i * 4;}
    void redo_f1(float f) {f1 = f - 5.0f;}
    void redo_s1(string s){s1 = s + "hi2";}

    LuaMemberReg mReg;
  };

  TEST(MemberUndoRedoHooks)
  {
    TEST_HEADER;

    // Setup custom undo/redo hooks and test their efficacy.
    tr1::shared_ptr<LuaScripting> sc(new LuaScripting());

    auto_ptr<B> b(new B(sc));

    b->mReg.registerFunction(b.get(), &B::set_i1, "set_i1", "", true);
    b->mReg.registerFunction(b.get(), &B::set_i2, "set_i2", "", true);
    b->mReg.registerFunction(b.get(), &B::get_i1, "get_i1", "", false);
    b->mReg.registerFunction(b.get(), &B::get_i2, "get_i2", "", false);

    b->mReg.registerFunction(b.get(), &B::set_f1, "set_f1", "", true);
    b->mReg.registerFunction(b.get(), &B::set_f2, "set_f2", "", true);
    b->mReg.registerFunction(b.get(), &B::get_f1, "get_f1", "", false);
    b->mReg.registerFunction(b.get(), &B::get_f2, "get_f2", "", false);

    b->mReg.registerFunction(b.get(), &B::set_s1, "set_s1", "", true);
    b->mReg.registerFunction(b.get(), &B::set_s2, "set_s2", "", true);
    b->mReg.registerFunction(b.get(), &B::get_s1, "get_s1", "", false);
    b->mReg.registerFunction(b.get(), &B::get_s2, "get_s2", "", false);

    // Customize undo functions (these undo functions will result in invalid
    // undo state, but thats what we want in order to detect correct redo/undo
    // hook installation).
    b->mReg.setUndoFun(b.get(), &B::undo_i1, "set_i1");
    b->mReg.setUndoFun(b.get(), &B::undo_f1, "set_f1");
    b->mReg.setUndoFun(b.get(), &B::undo_s1, "set_s1");

    b->mReg.setRedoFun(b.get(), &B::redo_i1, "set_i1");
    b->mReg.setRedoFun(b.get(), &B::redo_f1, "set_f1");
    b->mReg.setRedoFun(b.get(), &B::redo_s1, "set_s1");

    sc->exec("set_i1(100)");
    sc->exec("set_f1(126.5)");
    sc->exec("set_s1('Test')");

    CHECK_EQUAL(b->i1, 100);
    CHECK_CLOSE(b->f1, 126.5f, 0.001f);
    CHECK_EQUAL(b->s1.c_str(), "Test");

    sc->exec("set_i1(1000)");
    sc->exec("set_f1(500.0)");
    sc->exec("set_s1('nop')");

    sc->exec("provenance.undo()");
    CHECK_EQUAL("Testhi", b->s1.c_str());

    sc->exec("provenance.undo()");
    CHECK_CLOSE(129.0f, b->f1, 0.001f);

    sc->exec("provenance.undo()");
    CHECK_EQUAL(200, b->i1);

    sc->exec("provenance.redo()");
    CHECK_EQUAL(4000, b->i1);

    sc->exec("provenance.redo()");
    CHECK_CLOSE(495.0, b->f1, 0.001f);

    sc->exec("provenance.redo()");
    CHECK_EQUAL("nophi2", b->s1.c_str());
  }

}

#endif

} /* namespace tuvok */



