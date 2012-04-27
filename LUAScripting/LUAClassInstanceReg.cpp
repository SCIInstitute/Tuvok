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
 \brief Provides class instance registration in Lua.
 */

#ifndef EXTERNAL_UNIT_TESTING

#include "Controller/Controller.h"
#include "3rdParty/LUA/lua.hpp"

#else

#include <assert.h>
#include "utestCommon.h"

#endif

#include <vector>

#include "LUAScripting.h"
#include "LUAStackRAII.h"
#include "LUAClassInstanceReg.h"
#include "LUAClassInstance.h"

using namespace std;

namespace tuvok
{

const char* LuaClassInstanceReg::CONS_MD_CLASS_DEFINITION   = "classDefFun";
const char* LuaClassInstanceReg::CONS_MD_FACTORY_NAME       = "factoryName";

LuaClassInstanceReg::LuaClassInstanceReg(LuaScripting* scriptSys,
                                         const std::string& fqName,
                                         LuaScripting::ClassDefFun f)
: mSS(scriptSys)
, mClassPath(fqName)
, mDoConstruction(true)
, mClassDefinition(f)
, mClassInstance(NULL)
, mRegistration(NULL)
{}

/// Used internally to construct instances of a class from a class instance
/// table. Instances are initialized at instanceLoc.
LuaClassInstanceReg::LuaClassInstanceReg(LuaScripting* scriptSys,
                                         const std::string& instanceLoc,
                                         void* instance)
: mSS(scriptSys)
, mClassPath(instanceLoc)
, mDoConstruction(false)
, mClassDefinition(NULL)
, mClassInstance(instance)
, mRegistration(new LuaMemberRegUnsafe(scriptSys))
{}

//==============================================================================
//
// UNIT TESTING
//
//==============================================================================

#ifdef EXTERNAL_UNIT_TESTING

SUITE(LuaTestClassInstanceRegistration)
{
  int a_constructor = 0;
  int a_destructor = 0;
  class A
  {
  public:

    A(int a, float b, string c)
    {
      ++a_constructor;
      i1 = a; i2 = 0;
      f1 = b; f2 = 0.0f;
      s1 = c;
    }

    ~A()
    {
      ++a_destructor;
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

    // Class definition. The real meat defining a class.
    static void luaDefineClass(LuaClassInstanceReg& d)
    {
      d.constructor(&luaConstruct, "A's constructor.");
      d.function(&A::set_i1, "set_i1", "", true);
      d.function(&A::set_i2, "set_i2", "", true);
      d.function(&A::get_i1, "get_i1", "", false);
      d.function(&A::get_i2, "get_i2", "", false);

      d.function(&A::set_f1, "set_f1", "", true);
      d.function(&A::set_f2, "set_f2", "", true);
      d.function(&A::get_f1, "get_f1", "", false);
      d.function(&A::get_f2, "get_f2", "", false);

      d.function(&A::set_s1, "set_s1", "", true);
      d.function(&A::set_s2, "set_s2", "", true);
      d.function(&A::get_s1, "get_s1", "", false);
      d.function(&A::get_s2, "get_s2", "", false);
    }

  private:

    static A* luaConstruct(int a, float b, string c)
    { return new A(a, b, c); }

  };

  int b_constructor = 0;
  int b_destructor = 0;
  class B
  {
  public:
    B()
    {
      ++b_constructor;
      i1 = 0; f1 = 0;
    }

    ~B()
    {
      ++b_destructor;
    }

    int i1;
    float f1;
    string s1;

    void set_i1(int i)    {i1 = i;}
    int get_i1()          {return i1;}

    void set_f1(float f)  {f1 = f;}
    float get_f1()        {return f1;}

    void set_s1(string s) {s1 = s;}
    string get_s1()       {return s1;}

    static void luaDefineClass(LuaClassInstanceReg& d)
    {
      d.constructor(&luaConstruct, "B's constructor.");
      d.function(&B::set_i1, "set_i1", "", true);
      d.function(&B::get_i1, "get_i1", "", false);

      d.function(&B::set_f1, "set_f1", "", true);
      d.function(&B::get_f1, "get_f1", "", false);

      d.function(&B::set_s1, "set_s1", "", true);
      d.function(&B::get_s1, "get_s1", "", false);
    }

  private:

    static B* luaConstruct() {return new B();}

  };

  TEST(ClassRegistration)
  {
    TEST_HEADER;

    a_destructor = 0;

    {
      tr1::shared_ptr<LuaScripting> sc(new LuaScripting());

      // Register class definitions.
      sc->addLuaClassDef(&A::luaDefineClass, "factory.a1");
      sc->addLuaClassDef(&B::luaDefineClass, "factory.b1");

      // Test the classes.
      LuaClassInstance a_1 = sc->execRet<LuaClassInstance>(
          "factory.a1.new(2, 2.63, 'str')");
      // Dummy instances to test destructors when the scripting system
      // goes out of scope.
      sc->execRet<LuaClassInstance>("factory.a1.new(2, 2.63, 'str')");
      sc->execRet<LuaClassInstance>("factory.b1.new()");

      // Testing only the first instance of a1.
      std::string aInst = a_1.fqName();
      A* a = a_1.getRawPointer<A>(sc);

      CHECK_EQUAL(2, a->i1);
      CHECK_CLOSE(2.63f, a->f1, 0.001f);
      CHECK_EQUAL("str", a->s1.c_str());

      // Call into the class.
      sc->exec(aInst + ".set_i1(15)");
      sc->exec(aInst + ".set_i2(60)");
      sc->exec(aInst + ".set_f1(1.5)");
      sc->exec(aInst + ".set_f2(3.5)");
      sc->cexec(aInst + ".set_s1", "String 1");
      sc->cexec(aInst + ".set_s2", "String 2");

      CHECK_EQUAL(15, a->i1);
      CHECK_EQUAL(15, sc->execRet<int>(aInst + ".get_i1()"));
      CHECK_EQUAL(60, a->i2);
      CHECK_EQUAL(60, sc->execRet<int>(aInst + ".get_i2()"));

      CHECK_CLOSE(1.5f, a->f1, 0.001f);
      CHECK_CLOSE(1.5f, sc->execRet<float>(aInst + ".get_f1()"), 0.001f);
      CHECK_CLOSE(3.5f, a->f2, 0.001f);
      CHECK_CLOSE(3.5f, sc->execRet<float>(aInst + ".get_f2()"), 0.001f);

      CHECK_EQUAL("String 1", a->s1.c_str());
      CHECK_EQUAL("String 1", sc->execRet<string>(aInst + ".get_s1()"));
      CHECK_EQUAL("String 2", a->s2.c_str());
      CHECK_EQUAL("String 2", sc->execRet<string>(aInst + ".get_s2()"));
    }

    // Ensure destructor was called (we created two instances of the class).
    CHECK_EQUAL(2, a_destructor);
    CHECK_EQUAL(1, b_destructor);

    a_destructor = 0;
    b_destructor = 0;

    // More thorough checks
    {
      tr1::shared_ptr<LuaScripting> sc(new LuaScripting());

      // Register class definitions.
      sc->addLuaClassDef(&A::luaDefineClass, "factory.a1");
      sc->addLuaClassDef(&B::luaDefineClass, "factory.b1");

      LuaClassInstance a1 = sc->execRet<LuaClassInstance>(
          "factory.a1.new(2, 6, 'mystr')");

      {
        const int csize = 1;
        int ra[csize] = {0};
        const vector<int> v(ra, ra+csize);
        CHECK_EQUAL(true,
                    sc->getProvenanceSys()->testLastURItemHasCreatedItems(v));
      }

      LuaClassInstance a2 = sc->execRet<LuaClassInstance>(
          "factory.a1.new(4, 2.63, 'str')");

      {
        const int csize = 1;
        int ra[csize] = {1};
        const vector<int> v(ra, ra+csize);
        CHECK_EQUAL(true,
                    sc->getProvenanceSys()->testLastURItemHasCreatedItems(v));
      }

      LuaClassInstance b1 = sc->execRet<LuaClassInstance>("factory.b1.new()");

      {
        const int csize = 1;
        int ra[csize] = {2};
        const vector<int> v(ra, ra+csize);
        CHECK_EQUAL(true,
                    sc->getProvenanceSys()->testLastURItemHasCreatedItems(v));
      }

      std::string a1Name = a1.fqName();
      A* a1Ptr = a1.getRawPointer<A>(sc);

      std::string a2Name = a2.fqName();
      A* a2Ptr = a2.getRawPointer<A>(sc);

      std::string b1Name = b1.fqName();
      B* b1Ptr = b1.getRawPointer<B>(sc);

      int a1InstID = a1.getGlobalInstID();
      int a2InstID = a2.getGlobalInstID();
      int b1InstID = b1.getGlobalInstID();

      // Check global ID (a1 == 0, a2 == 1, b1 == 2)
      CHECK_EQUAL(0, a1InstID);
      CHECK_EQUAL(1, a2InstID);
      CHECK_EQUAL(2, b1InstID);

      sc->exec(a1Name + ".set_i1(15)");
      sc->exec(a1Name + ".set_i2(60)");
      sc->exec(a1Name + ".set_f1(1.5)");
      sc->exec(a1Name + ".set_f2(3.5)");
      sc->cexec(a1Name + ".set_s1", "String 1");
      sc->cexec(a1Name + ".set_s2", "String 2");

      sc->exec(a2Name + ".set_i2(60)");
      sc->exec(a2Name + ".set_f2(3.5)");
      sc->cexec(a2Name + ".set_s2", "String 2");

      sc->exec(b1Name + ".set_i1(158)");
      sc->exec(b1Name + ".set_f1(345.89)");
      sc->cexec(b1Name + ".set_s1", "B1 str");

      // Check a1
      CHECK_EQUAL(15, a1Ptr->i1);
      CHECK_EQUAL(15, sc->execRet<int>(a1Name + ".get_i1()"));
      CHECK_EQUAL(60, a1Ptr->i2);
      CHECK_EQUAL(60, sc->execRet<int>(a1Name + ".get_i2()"));

      CHECK_CLOSE(1.5f, a1Ptr->f1, 0.001f);
      CHECK_CLOSE(1.5f, sc->execRet<float>(a1Name + ".get_f1()"), 0.001f);
      CHECK_CLOSE(3.5f, a1Ptr->f2, 0.001f);
      CHECK_CLOSE(3.5f, sc->execRet<float>(a1Name + ".get_f2()"), 0.001f);

      CHECK_EQUAL("String 1", a1Ptr->s1.c_str());
      CHECK_EQUAL("String 1", sc->execRet<string>(a1Name + ".get_s1()"));
      CHECK_EQUAL("String 2", a1Ptr->s2.c_str());
      CHECK_EQUAL("String 2", sc->execRet<string>(a1Name + ".get_s2()"));

      // Check a2
      CHECK_EQUAL(4, a2Ptr->i1);
      CHECK_CLOSE(2.63f, a2Ptr->f1, 0.001f);
      CHECK_EQUAL("str", a2Ptr->s1.c_str());
      CHECK_EQUAL(60, a2Ptr->i2);
      CHECK_CLOSE(3.5f, a2Ptr->f2, 0.001f);
      CHECK_EQUAL("String 2", a2Ptr->s2);

      // Check b1
      CHECK_EQUAL(158, b1Ptr->i1);
      CHECK_CLOSE(345.89f, b1Ptr->f1, 0.001f);
      CHECK_EQUAL("B1 str", b1Ptr->s1);

      // Check whether the class delete function works.
      sc->exec("deleteClass(" + a2Name + ")");
      sc->setExpectedExceptionFlag(true);
      CHECK_THROW(sc->exec(a2Name + ".set_i2(60)"), LuaError);
      sc->setExpectedExceptionFlag(false);
      CHECK_EQUAL(1, a_destructor);

      {
        const int csize = 1;
        int ra[csize] = {a2InstID};
        const vector<int> v(ra, ra+csize);
        CHECK_EQUAL(true,
                    sc->getProvenanceSys()->testLastURItemHasDeletedItems(v));
      }

      sc->exec("deleteClass(" + a1Name + ")");
      CHECK_EQUAL(2, a_destructor);

      {
        const int csize = 1;
        int ra[csize] = {a1InstID};
        const vector<int> v(ra, ra+csize);
        CHECK_EQUAL(true,
                    sc->getProvenanceSys()->testLastURItemHasDeletedItems(v));
      }

      sc->exec("deleteClass(" + b1Name + ")");
      CHECK_EQUAL(1, b_destructor);

      {
        const int csize = 1;
        int ra[csize] = {b1InstID};
        const vector<int> v(ra, ra+csize);
        CHECK_EQUAL(true,
                    sc->getProvenanceSys()->testLastURItemHasDeletedItems(v));
      }
    }

    // TODO: Test exceptions.
  }

  int hooki1 = 0;
  float hookf1 = 0.0;
  string hooks1;

  void testHookSeti1(int i)
  {
    hooki1 = i;
  }

  void testHookSetf1(float f)
  {
    hookf1 = f;
  }

  void testHookSets1(string s)
  {
    hooks1 = s;
  }

  TEST(ClassHooks)
  {
    TEST_HEADER;

    tr1::shared_ptr<LuaScripting> sc(new LuaScripting());

    {
      sc->addLuaClassDef(&A::luaDefineClass, "factory.a1");

      LuaClassInstance a_1 = sc->execRet<LuaClassInstance>(
          "factory.a1.new(2, 2.63, 'str')");

      std::string aInst = a_1.fqName();

      A* a = a_1.getRawPointer<A>(sc);

      // Hook i1, f1, and s1.
      // Member function hooks would work in the same way, but using
      // LuaMemberReg .
      sc->strictHook(&testHookSeti1, aInst + ".set_i1");
      sc->strictHook(&testHookSetf1, aInst + ".set_f1");
      sc->strictHook(&testHookSets1, aInst + ".set_s1");

      // Call into the class.
      sc->exec(aInst + ".set_i1(15)");
      sc->exec(aInst + ".set_f1(1.5)");
      sc->cexec(aInst + ".set_s1", "String 1");

      CHECK_EQUAL(15, a->i1);
      CHECK_EQUAL(15, hooki1);

      CHECK_CLOSE(1.5f, a->f1, 0.001f);
      CHECK_CLOSE(1.5f, hookf1, 0.001f);

      CHECK_EQUAL("String 1", a->s1.c_str());
      CHECK_EQUAL("String 1", hooks1);
    }

  }

  TEST(ClassProvenance)
  {
    TEST_HEADER;

    // Thoroughly test class provenance.
    tr1::shared_ptr<LuaScripting> sc(new LuaScripting());

    // Register class definitions.
    sc->addLuaClassDef(&A::luaDefineClass, "factory.a1");
    sc->addLuaClassDef(&B::luaDefineClass, "factory.b1");

    a_destructor = 0;
    LuaClassInstance a1 = sc->execRet<LuaClassInstance>(
        "factory.a1.new(2, 2.63, 'str')");

    // Testing only the first instance of a1.
    std::string a1Name = a1.fqName();
    A* a1p = a1.getRawPointer<A>(sc);

    // Call into the class.
    sc->exec(a1Name + ".set_i1(15)");
    sc->exec(a1Name + ".set_i2(60)");
    sc->exec(a1Name + ".set_f1(1.5)");
    sc->exec(a1Name + ".set_f2(3.5)");
    sc->cexec(a1Name + ".set_s1", "String 1");
    sc->cexec(a1Name + ".set_s2", "String 2");

    sc->exec("deleteClass(" + a1Name + ")");

    // RAW POINTER IS NOW BAD. NEED TO REFORM THE POINTER!
    // THIS IS WHY YOU DON'T USE THE POINTER!

    b_destructor = 0;
    LuaClassInstance b1 = sc->execRet<LuaClassInstance>("factory.b1.new()");

    // Testing only the first instance of a1.
    std::string b1Name = b1.fqName();
    
    sc->exec(b1Name + ".set_i1(158)");
    sc->exec(b1Name + ".set_f1(345.89)");
    sc->cexec(b1Name + ".set_s1", "B1 str");

    sc->exec("provenance.undo()");
    sc->exec("provenance.undo()");
    sc->exec("provenance.undo()");

    // Delete class b.
    sc->exec("provenance.undo()");
    CHECK_EQUAL(1, b_destructor);

    // Recreate class a (with its last state)
    sc->exec("provenance.undo()");

    // Since the class was just recreated on undo, we can now get the raw
    // pointer.
    a1p = a1.getRawPointer<A>(sc);

    // Notice, the state is back where it was when we deleted the class.
    // The provenance system handles this automatically.
    CHECK_EQUAL(15, a1p->i1);
    CHECK_EQUAL(60, a1p->i2);

    CHECK_CLOSE(1.5f, a1p->f1, 0.001f);
    CHECK_CLOSE(3.5f, a1p->f2, 0.001f);

    CHECK_EQUAL("String 1", a1p->s1.c_str());
    CHECK_EQUAL("String 2", a1p->s2.c_str());
  }

  tr1::shared_ptr<LuaScripting> gSS;

  static int c_constructor = 0;
  static int c_destructor = 0;
  class C
  {
  public:
    C()
    : a(gSS->execRet<LuaClassInstance>("factory.a.new(2, 2.63, 'str')"))
    {
      ++c_constructor;
      i1 = 0;
      f1 = 0.0f;
    }

    ~C()
    {
      ++c_destructor;
      gSS->exec("deleteClass(" + a.fqName() + ")"); // Our responsibility
    }

    LuaClassInstance a;

    int i1;
    float f1;
    string s1;

    void set_i1(int i)    {i1 = i;}
    int get_i1()          {return i1;}

    void set_f1(float f)  {f1 = f;}
    float get_f1()        {return f1;}

    void set_s1(string s) {s1 = s;}
    string get_s1()       {return s1;}

    // Methods to set i1 values in a.
    void set_a_i2(int i)
    {
      ostringstream os;
      os << a.fqName() << ".set_i2(" << i << ")";
      gSS->exec(os.str());
    }
    int get_a_i2()
    {
      return gSS->execRet<int>(a.fqName() + ".get_i2()");
    }
    void set_a_f2(float f)
    {
      ostringstream os;
      os << a.fqName() << ".set_f2(" << f << ")";
      gSS->exec(os.str());
    }
    float get_a_f2()
    {
      return gSS->execRet<float>(a.fqName() + ".get_f2()");
    }
    void set_a_s2(string s)
    {
      ostringstream os;
      os << a.fqName() << ".set_s2('" << s << "')";
      gSS->exec(os.str());
    }
    string get_a_s2()
    {
      return gSS->execRet<string>(a.fqName() + "get_s2()");
    }


    static void luaDefineClass(LuaClassInstanceReg& d)
    {
      d.constructor(&luaConstruct, "C's constructor.");
      d.function(&C::set_i1, "set_i1", "", true);
      d.function(&C::get_i1, "get_i1", "", false);

      d.function(&C::set_f1, "set_f1", "", true);
      d.function(&C::get_f1, "get_f1", "", false);

      d.function(&C::set_s1, "set_s1", "", true);
      d.function(&C::get_s1, "get_s1", "", false);

      d.function(&C::set_a_i2, "set_a_i2", "", true);
      d.function(&C::get_a_i2, "get_a_i2", "", false);

      d.function(&C::set_a_f2, "set_a_f2", "", true);
      d.function(&C::get_a_f2, "get_a_f2", "", false);

      d.function(&C::set_a_s2, "set_a_s2", "", true);
      d.function(&C::get_a_s2, "get_a_s2", "", false);
    }

  private:

    static C* luaConstruct() {return new C();}
  };

  static int d_constructor = 0;
  static int d_destructor = 0;
  class D
  {
  public:
    D()
    : b(gSS->execRet<LuaClassInstance>("factory.b.new()"))
    , c(gSS->execRet<LuaClassInstance>("factory.c.new()"))
    {
      ++d_constructor;
    }

    ~D()
    {
      ++d_destructor;
      gSS->exec("deleteClass(" + c.fqName() + ")"); // Our responsibility
      gSS->exec("deleteClass(" + b.fqName() + ")"); // Our responsibility
    }

    LuaClassInstance b;
    LuaClassInstance c;

    void set_i1(int i)
    {
      ostringstream os;
      os << b.fqName() << ".set_i1(" << i << ")";
      gSS->exec(os.str());
    }
    int get_i1()
    {
      return gSS->execRet<int>(b.fqName() + ".get_i1()");
    }

    void set_f1(float f)
    {
      ostringstream os;
      os << b.fqName() << ".set_f1(" << f << ")";
      gSS->exec(os.str());
    }
    float get_f1()
    {
      return gSS->execRet<float>(b.fqName() + "get_f1()");
    }

    void set_s1(string s)
    {
      ostringstream os;
      os << b.fqName() << ".set_s1('" << s << "')";
      gSS->exec(os.str());
    }
    string get_s1()
    {
      return gSS->execRet<string>(b.fqName() + "get_s1()");
    }

    // 3 levels of indirection (D -> C -> A)
    void set_a_i2(int i)
    {
      ostringstream os;
      os << c.fqName() << ".set_a_i2(" << i << ")";
      gSS->exec(os.str());
    }
    int get_a_i2()
    {
      return gSS->execRet<int>(c.fqName() + ".get_a_i2()");
    }
    void set_a_f2(float f)
    {
      ostringstream os;
      os << c.fqName() << ".set_a_f2(" << f << ")";
      gSS->exec(os.str());
    }
    float get_a_f2()
    {
      return gSS->execRet<float>(c.fqName() + "get_a_f2()");
    }
    void set_a_s2(string s)
    {
      ostringstream os;
      os << c.fqName() << ".set_a_s2('" << s << "')";
      gSS->exec(os.str());
    }
    string get_a_s2()
    {
      return gSS->execRet<string>(c.fqName() + "get_a_s2()");
    }

    static void luaDefineClass(LuaClassInstanceReg& d)
    {
      d.constructor(&luaConstruct, "D's constructor.");
      d.function(&D::set_i1, "set_i1", "", true);
      d.function(&D::get_i1, "get_i1", "", false);

      d.function(&D::set_f1, "set_f1", "", true);
      d.function(&D::get_f1, "get_f1", "", false);

      d.function(&D::set_s1, "set_s1", "", true);
      d.function(&D::get_s1, "get_s1", "", false);

      d.function(&D::set_a_i2, "set_a_i2", "", true);
      d.function(&D::get_a_i2, "get_a_i2", "", false);

      d.function(&D::set_a_f2, "set_a_f2", "", true);
      d.function(&D::get_a_f2, "get_a_f2", "", false);

      d.function(&D::set_a_s2, "set_a_s2", "", true);
      d.function(&D::get_a_s2, "get_a_s2", "", false);
    }

  private:

    static D* luaConstruct() {return new D();}

  };

  // Helps keeps track of destructor / constructor calls.
  int a_con = 0;
  int a_des = 0;

  int b_con = 0;
  int b_des = 0;

  int c_con = 0;
  int c_des = 0;

  int d_con = 0;
  int d_des = 0;

  void compareAccumulators()
  {
    CHECK_EQUAL(a_con, a_constructor);
    CHECK_EQUAL(a_des, a_destructor);

    CHECK_EQUAL(b_con, b_constructor);
    CHECK_EQUAL(b_des, b_destructor);

    CHECK_EQUAL(c_con, c_constructor);
    CHECK_EQUAL(c_des, c_destructor);

    CHECK_EQUAL(d_con, d_constructor);
    CHECK_EQUAL(d_des, d_destructor);
  }

  void clearAccumulators()
  {
    a_constructor = 0;
    a_destructor = 0;

    b_constructor = 0;
    b_destructor = 0;

    c_constructor = 0;
    c_destructor = 0;

    d_constructor = 0;
    d_destructor = 0;

    a_con = 0;
    a_des = 0;

    b_con = 0;
    b_des = 0;

    c_con = 0;
    c_des = 0;

    d_con = 0;
    d_des = 0;
  }

  TEST(ClassProvenanceCompositing)
  {
    // Test compositing classes together (new Lua classes are created from
    // another class' constructor).

    // This will be a complex case, testing the provenance system's integrity.
    TEST_HEADER;

    // Thoroughly test class provenance.
    gSS = tr1::shared_ptr<LuaScripting>(new LuaScripting());

    // Register class definitions.
    gSS->addLuaClassDef(&A::luaDefineClass, "factory.a");
    gSS->addLuaClassDef(&B::luaDefineClass, "factory.b");
    gSS->addLuaClassDef(&C::luaDefineClass, "factory.c");
    gSS->addLuaClassDef(&D::luaDefineClass, "factory.d");

    clearAccumulators();

    // Test Just the D class for now (composition).
    {
      LuaClassInstance d = gSS->execRet<LuaClassInstance>("factory.d.new()");
      ++a_con; ++b_con; ++c_con; ++d_con;
      compareAccumulators();

      // Obtain instances to all of our classes.
      string dn = d.fqName();
      D* dp = d.getRawPointer<D>(gSS);

      LuaClassInstance d_c = dp->c;
      LuaClassInstance d_b = dp->b;

      string d_cn = d_c.fqName();
      C* d_cp = d_c.getRawPointer<C>(gSS);

      string d_bn = d_b.fqName();
      B* d_bp = d_b.getRawPointer<B>(gSS);

      LuaClassInstance d_c_a = d_cp->a;

      string d_c_an = d_c_a.fqName();
      A* d_c_ap = d_c_a.getRawPointer<A>(gSS);

      // Set several variables using d's functions.

      // Set B's variables through C functions
      gSS->exec(dn + ".set_i1(643)");
      gSS->exec(dn + ".set_f1(34.83)");
      gSS->exec(dn + ".set_s1('James')");

      // Set C's A _#2's variables through C functions.
      gSS->exec(dn + ".set_a_i2(121)");
      gSS->exec(dn + ".set_a_f2(12.21)");
      gSS->exec(dn + ".set_a_s2('Hughes')");

      // Set C's variables through C.
      gSS->exec(d_cn + ".set_i1(823)");
      gSS->exec(d_cn + ".set_f1(230.212)");
      gSS->exec(d_cn + ".set_s1('C Vars')");

      // Set A's _#1 variables through A.
      gSS->exec(d_c_an + ".set_i1(2346)");
      gSS->exec(d_c_an + ".set_f1(543.4325)");
      gSS->exec(d_c_an + ".set_s1('A Vars')");

      // Test that B's variables were set.
      CHECK_EQUAL(643, d_bp->i1);
      CHECK_CLOSE(34.83, d_bp->f1, 0.0001f);
      CHECK_EQUAL("James", d_bp->s1.c_str());

      // Test that A's _#2 variables were set.
      CHECK_EQUAL(121, d_c_ap->i2);
      CHECK_CLOSE(12.21, d_c_ap->f2, 0.0001f);
      CHECK_EQUAL("Hughes", d_c_ap->s2.c_str());

      // Test that C's variables were set.
      CHECK_EQUAL(823, d_cp->i1);
      CHECK_CLOSE(230.212, d_cp->f1, 0.0001f);
      CHECK_EQUAL("C Vars", d_cp->s1.c_str());

      // Test that A's variables were set.
      CHECK_EQUAL(2346, d_c_ap->i1);
      CHECK_CLOSE(543.4325, d_c_ap->f1, 0.0001f);
      CHECK_EQUAL("A Vars", d_c_ap->s1.c_str());

      // Undo A's _#1 variables
      gSS->exec("provenance.undo()");
      gSS->exec("provenance.undo()");
      gSS->exec("provenance.undo()");
      // Remember: A has a constructor, but it does not set its values using
      //           its getter/setter functions, so there state will not be
      //           what the constructor set them as. They will be defaults.
      //           Their state will return to normal once we undo/redo D's
      //           constructor.
      CHECK_EQUAL(0, d_c_ap->i1);
      CHECK_CLOSE(0.0, d_c_ap->f1, 0.0001f);
      CHECK_EQUAL("", d_c_ap->s1.c_str());

      // Undo C's variables
      gSS->exec("provenance.undo()");
      gSS->exec("provenance.undo()");
      gSS->exec("provenance.undo()");
      CHECK_EQUAL(0, d_cp->i1);
      CHECK_CLOSE(0.0f, d_cp->f1, 0.0001f);
      CHECK_EQUAL("", d_cp->s1.c_str());

      // Undo C's A _#2's variables
      gSS->exec("provenance.undo()");
      gSS->exec("provenance.undo()");
      gSS->exec("provenance.undo()");
      CHECK_EQUAL(0, d_c_ap->i2);
      CHECK_CLOSE(0.0f, d_c_ap->f2, 0.0001f);
      CHECK_EQUAL("", d_c_ap->s2.c_str());

      // Undo B's variables
      gSS->exec("provenance.undo()");
      gSS->exec("provenance.undo()");
      gSS->exec("provenance.undo()");
      CHECK_EQUAL(0, d_bp->i1);
      CHECK_CLOSE(0.0f, d_bp->f1, 0.0001f);
      CHECK_EQUAL("", d_bp->s1.c_str());

      // Redo all of the way back.
      gSS->exec("provenance.redo()");
      gSS->exec("provenance.redo()");
      gSS->exec("provenance.redo()");

      gSS->exec("provenance.redo()");
      gSS->exec("provenance.redo()");
      gSS->exec("provenance.redo()");

      gSS->exec("provenance.redo()");
      gSS->exec("provenance.redo()");
      gSS->exec("provenance.redo()");

      gSS->exec("provenance.redo()");
      gSS->exec("provenance.redo()");
      gSS->exec("provenance.redo()");

      // Recheck variables
      // Test that B's variables were set.
      CHECK_EQUAL(643, d_bp->i1);
      CHECK_CLOSE(34.83, d_bp->f1, 0.0001f);
      CHECK_EQUAL("James", d_bp->s1.c_str());

      // Test that A's _#2 variables were set.
      CHECK_EQUAL(121, d_c_ap->i2);
      CHECK_CLOSE(12.21, d_c_ap->f2, 0.0001f);
      CHECK_EQUAL("Hughes", d_c_ap->s2.c_str());

      // Test that C's variables were set.
      CHECK_EQUAL(823, d_cp->i1);
      CHECK_CLOSE(230.212, d_cp->f1, 0.0001f);
      CHECK_EQUAL("C Vars", d_cp->s1.c_str());

      // Test that A's variables were set.
      CHECK_EQUAL(2346, d_c_ap->i1);
      CHECK_CLOSE(543.4325, d_c_ap->f1, 0.0001f);
      CHECK_EQUAL("A Vars", d_c_ap->s1.c_str());

      int oldInstTop = gSS->getCurrentClassInstID();

      // Delete the class.
      gSS->exec("deleteClass(" + dn + ")");
      ++a_des; ++b_des; ++c_des; ++d_des;
      compareAccumulators();

      CHECK_EQUAL(oldInstTop, gSS->getCurrentClassInstID());

      // Undo deletion (all classes should be recreated with the above state).
      gSS->exec("provenance.undo()");
      ++a_con; ++b_con; ++c_con; ++d_con;
      compareAccumulators();

      CHECK_EQUAL(oldInstTop, gSS->getCurrentClassInstID());

      // Re-grab pointers (the globalID's will not have changed).
      dp = d.getRawPointer<D>(gSS);
      d_cp = d_c.getRawPointer<C>(gSS);
      d_bp = d_b.getRawPointer<B>(gSS);
      d_c_ap = d_c_a.getRawPointer<A>(gSS);

      // Recheck variables
      // Test that B's variables were set.
      CHECK_EQUAL(643, d_bp->i1);
      CHECK_CLOSE(34.83, d_bp->f1, 0.0001f);
      CHECK_EQUAL("James", d_bp->s1.c_str());

      // Test that A's _#2 variables were set.
      CHECK_EQUAL(121, d_c_ap->i2);
      CHECK_CLOSE(12.21, d_c_ap->f2, 0.0001f);
      CHECK_EQUAL("Hughes", d_c_ap->s2.c_str());

      // Test that C's variables were set.
      CHECK_EQUAL(823, d_cp->i1);
      CHECK_CLOSE(230.212, d_cp->f1, 0.0001f);
      CHECK_EQUAL("C Vars", d_cp->s1.c_str());

      // Test that A's variables were set.
      CHECK_EQUAL(2346, d_c_ap->i1);
      CHECK_CLOSE(543.4325, d_c_ap->f1, 0.0001f);
      CHECK_EQUAL("A Vars", d_c_ap->s1.c_str());

      gSS->exec("provenance.undo()");
      gSS->exec("provenance.undo()");
      gSS->exec("provenance.undo()");

      gSS->exec("provenance.undo()");
      gSS->exec("provenance.undo()");
      gSS->exec("provenance.undo()");

      gSS->exec("provenance.undo()");
      gSS->exec("provenance.undo()");
      gSS->exec("provenance.undo()");

      gSS->exec("provenance.undo()");
      gSS->exec("provenance.undo()");
      gSS->exec("provenance.undo()");

      CHECK_EQUAL(0, d_c_ap->i1);
      CHECK_CLOSE(0.0, d_c_ap->f1, 0.0001f);
      CHECK_EQUAL("", d_c_ap->s1.c_str());

      CHECK_EQUAL(0, d_cp->i1);
      CHECK_CLOSE(0.0f, d_cp->f1, 0.0001f);
      CHECK_EQUAL("", d_cp->s1.c_str());

      CHECK_EQUAL(0, d_c_ap->i2);
      CHECK_CLOSE(0.0f, d_c_ap->f2, 0.0001f);
      CHECK_EQUAL("", d_c_ap->s2.c_str());

      CHECK_EQUAL(0, d_bp->i1);
      CHECK_CLOSE(0.0f, d_bp->f1, 0.0001f);
      CHECK_EQUAL("", d_bp->s1.c_str());

      // Undo class creation.
      gSS->exec("provenance.undo()");
      ++a_des; ++b_des; ++c_des; ++d_des;
      compareAccumulators();

      // Redo class creation.
      gSS->exec("provenance.redo()");
      ++a_con; ++b_con; ++c_con; ++d_con;
      compareAccumulators();

      // Re-grab pointers (the globalID's will not have changed).
      dp = d.getRawPointer<D>(gSS);
      d_cp = d_c.getRawPointer<C>(gSS);
      d_bp = d_b.getRawPointer<B>(gSS);
      d_c_ap = d_c_a.getRawPointer<A>(gSS);

      // Remember! The constructor was just called. So a's constructed
      // values sholud be there (2, 2.63, 'str')
      CHECK_EQUAL(2, d_c_ap->i1);
      CHECK_CLOSE(2.63, d_c_ap->f1, 0.0001f);
      CHECK_EQUAL("str", d_c_ap->s1.c_str());

      CHECK_EQUAL(0, d_cp->i1);
      CHECK_CLOSE(0.0f, d_cp->f1, 0.0001f);
      CHECK_EQUAL("", d_cp->s1.c_str());

      CHECK_EQUAL(0, d_c_ap->i2);
      CHECK_CLOSE(0.0f, d_c_ap->f2, 0.0001f);
      CHECK_EQUAL("", d_c_ap->s2.c_str());

      CHECK_EQUAL(0, d_bp->i1);
      CHECK_CLOSE(0.0f, d_bp->f1, 0.0001f);
      CHECK_EQUAL("", d_bp->s1.c_str());

      // Redo all of the way back.
      gSS->exec("provenance.redo()");
      gSS->exec("provenance.redo()");
      gSS->exec("provenance.redo()");

      gSS->exec("provenance.redo()");
      gSS->exec("provenance.redo()");
      gSS->exec("provenance.redo()");

      gSS->exec("provenance.redo()");
      gSS->exec("provenance.redo()");
      gSS->exec("provenance.redo()");

      gSS->exec("provenance.redo()");
      gSS->exec("provenance.redo()");
      gSS->exec("provenance.redo()");

      // Recheck variables
      // Test that B's variables were set.
      CHECK_EQUAL(643, d_bp->i1);
      CHECK_CLOSE(34.83, d_bp->f1, 0.0001f);
      CHECK_EQUAL("James", d_bp->s1.c_str());

      // Test that A's _#2 variables were set.
      CHECK_EQUAL(121, d_c_ap->i2);
      CHECK_CLOSE(12.21, d_c_ap->f2, 0.0001f);
      CHECK_EQUAL("Hughes", d_c_ap->s2.c_str());

      // Test that C's variables were set.
      CHECK_EQUAL(823, d_cp->i1);
      CHECK_CLOSE(230.212, d_cp->f1, 0.0001f);
      CHECK_EQUAL("C Vars", d_cp->s1.c_str());

      // Test that A's variables were set.
      CHECK_EQUAL(2346, d_c_ap->i1);
      CHECK_CLOSE(543.4325, d_c_ap->f1, 0.0001f);
      CHECK_EQUAL("A Vars", d_c_ap->s1.c_str());

      // Redo deletion
      gSS->exec("provenance.redo()");
      ++a_des; ++b_des; ++c_des; ++d_des;
      compareAccumulators();

      // We can check this class after we are done below.
    }

    // No need to clear the accumulators, just keep them going.


//    // Test interleaving creation of A,B,C, and D.
//    {
//      // Create d
//      // (a_ b_ c_ d_constructor = 1)
//      ++a_c; ++b_c; ++c_c; ++d_c;
//      compareAccumulators();
//
//      // Set misc values for d
//
//      // Create a
//      // (a_constructor = 2)
//      ++a_c;
//      compareAccumulators();
//
//      // Set misc values for a/d
//
//      // Create c
//      // (c_constructor = 2, a_constructor = 3)
//      ++c_c; ++a_c;
//      compareAccumulators();
//
//      // Set misc values a/c/d
//
//      // Create b
//      // (b_constructor = 2)
//      ++b_c;
//      compareAccumulators();
//
//      // Set misc values for all
//
//      // delete a
//      // (a_destructor = 1)
//      ++a_d;
//      compareAccumulators();
//
//      // set misc values for b,c,d
//
//      // delete d
//      // (a_destructor = 2, b_ c_ d_destructor = 1)
//      ++a_d; ++b_d; ++c_d; ++d_d;
//      compareAccumulators();
//
//      // set misc values for c and b
//
//      // delete b
//      // (b_destructor = 2)
//      ++b_d;
//      compareAccumulators();
//
//      // set misc values for c
//
//      // delete c
//      // (a_destructor = 3, c_destructor = 2)
//      ++a_d; ++c_d;
//      compareAccumulators();
//
//      // test undo/redo of this entire system
//
//      // undo - delete c
//      // (c_constructor = )
//
//      // Undoing c should travel all the way back to the creation of d.
//      // (d_constructor = 2
//    }

  }

  TEST(ClassHelpAndLog)
  {
    // Help should be given for classes, but not for any of their instances 
    // in the _sys_ table.
  }

  TEST(ClassRTTITypeChecks)
  {
    tr1::shared_ptr<LuaScripting> sc(new LuaScripting());




  }

}

#endif


} /* namespace tuvok */
