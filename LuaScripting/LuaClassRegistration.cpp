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

#include <vector>

#include "LuaScripting.h"

using namespace std;

namespace tuvok
{


} /* namespace tuvok */

//==============================================================================
//
// UNIT TESTING
//
//==============================================================================
#ifdef LUASCRIPTING_UNIT_TESTS
#include "utestCommon.h"
#include "LuaClassRegistration.h"
#include "LuaProvenance.h"

using namespace tuvok;

SUITE(LuaTestClassRegistration)
{
  int a_constructor = 0;
  int a_destructor = 0;
  class A
  {
  public:

    A(int a, float b, string c, shared_ptr<LuaScripting> ss)
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
    static void registerFunctions(LuaClassRegistration<A>& d, A* me,
                                  LuaScripting* ss)
    {
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

    static A* luaConstruct(int a, float b, string c,
                           shared_ptr<LuaScripting> ss)
    { return new A(a, b, c, ss); }

  };

  int b_constructor = 0;
  int b_destructor = 0;
  class B
  {
  public:
    B(shared_ptr<LuaScripting> ss)
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

    static void registerFunctions(LuaClassRegistration<B>& d, B* me,
                                  LuaScripting* ss)
    {
      d.function(&B::set_i1, "set_i1", "", true);
      d.function(&B::get_i1, "get_i1", "", false);

      d.function(&B::set_f1, "set_f1", "", true);
      d.function(&B::get_f1, "get_f1", "", false);

      d.function(&B::set_s1, "set_s1", "", true);
      d.function(&B::get_s1, "get_s1", "", false);
    }

    static B* luaConstruct(shared_ptr<LuaScripting> ss) {return new B(ss);}

  };

  TEST(ClassRegistration)
  {
    TEST_HEADER;

    a_destructor = 0;

    {
      shared_ptr<LuaScripting> sc(new LuaScripting());

      // Register class definitions.
      sc->registerClassStatic<A>(&A::luaConstruct, "factory.a1", "a class",
                                 LuaClassRegCallback<A>::Type(
                                     &A::registerFunctions));
      sc->registerClassStatic<B>(&B::luaConstruct, "factory.b1", "b class",
                                 LuaClassRegCallback<B>::Type(
                                     &B::registerFunctions));

      // Test the classes.
      LuaClassInstance a_1 = sc->cexecRet<LuaClassInstance>(
          "factory.a1.new", 2, 2.63, "str", sc);
      // Dummy instances to test destructors when the scripting system
      // goes out of scope.
      sc->cexecRet<LuaClassInstance>("factory.a1.new", 2, 2.63, "str", sc);
      sc->cexecRet<LuaClassInstance>("factory.b1.new", sc);

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

      sc->clean();
    }

    // Ensure destructor was called (we created two instances of the class).
    CHECK_EQUAL(2, a_destructor);
    CHECK_EQUAL(1, b_destructor);

    a_destructor = 0;
    b_destructor = 0;

    // More thorough checks
    {
      shared_ptr<LuaScripting> sc(new LuaScripting());

      // Register class definitions.
      sc->registerClassStatic<A>(&A::luaConstruct, "factory.a1", "a class",
                                 LuaClassRegCallback<A>::Type(
                                     &A::registerFunctions));
      sc->registerClassStatic<B>(&B::luaConstruct, "factory.b1", "b class",
                                 LuaClassRegCallback<B>::Type(
                                     &B::registerFunctions));

      LuaClassInstance a1 = sc->cexecRet<LuaClassInstance>(
          "factory.a1.new", 2, 6.0, "mystr", sc);

      {
        const int csize = 1;
        int ra[csize] = {0};
        const vector<int> v(ra, ra+csize);
        CHECK_EQUAL(true,
                    sc->getProvenanceSys()->testLastURItemHasCreatedItems(v));
      }

      LuaClassInstance a2 = sc->cexecRet<LuaClassInstance>(
          "factory.a1.new", 4, 2.63, "str", sc);

      {
        const int csize = 1;
        int ra[csize] = {1};
        const vector<int> v(ra, ra+csize);
        CHECK_EQUAL(true,
                    sc->getProvenanceSys()->testLastURItemHasCreatedItems(v));
      }

      LuaClassInstance b1 = sc->cexecRet<LuaClassInstance>(
          "factory.b1.new", sc);

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

      // We MUST do this since we are passing around shared_ptr references
      // to our LuaScripting class.
      sc->clean();
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

    shared_ptr<LuaScripting> sc(new LuaScripting());

    {
      sc->registerClassStatic<A>(&A::luaConstruct, "factory.a1", "a class",
                                 LuaClassRegCallback<A>::Type(
                                     &A::registerFunctions));

      LuaClassInstance a_1 = sc->cexecRet<LuaClassInstance>(
          "factory.a1.new", 2, 2.63, "str", sc);

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

    sc->clean();
  }

  TEST(ClassProvenance)
  {
    TEST_HEADER;

    // Thoroughly test class provenance.
    shared_ptr<LuaScripting> sc(new LuaScripting());

    // Register class definitions.
    sc->registerClassStatic<A>(&A::luaConstruct, "factory.a1", "a class",
                               LuaClassRegCallback<A>::Type(
                                   &A::registerFunctions));
    sc->registerClassStatic<B>(&B::luaConstruct, "factory.b1", "b class",
                               LuaClassRegCallback<B>::Type(
                                   &B::registerFunctions));

    a_destructor = 0;
    LuaClassInstance a1 = sc->cexecRet<LuaClassInstance>(
        "factory.a1.new", 2, 2.63, "str", sc);

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
    LuaClassInstance b1 = sc->cexecRet<LuaClassInstance>("factory.b1.new", sc);

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

    sc->clean();
  }

  shared_ptr<LuaScripting> gSS;

  static int c_constructor = 0;
  static int c_destructor = 0;
  class C
  {
  public:
    C()
    : a(gSS->cexecRet<LuaClassInstance>("factory.a.new", 2, 2.63, "str", gSS))
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


    static void registerFunctions(LuaClassRegistration<C>& d, C* me,
                                  LuaScripting* ss)
    {
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

    static C* luaConstruct() {return new C();}
  };

  static int d_constructor = 0;
  static int d_destructor = 0;
  class D
  {
  public:
    D()
    : b(gSS->cexecRet<LuaClassInstance>("factory.b.new", gSS))
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

    static void registerFunctions(LuaClassRegistration<D>& d, D* me,
                                  LuaScripting* ss)
    {
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
    gSS = shared_ptr<LuaScripting>(new LuaScripting());

    gSS->registerClassStatic<A>(&A::luaConstruct, "factory.a", "a class",
                                LuaClassRegCallback<A>::Type(
                                    &A::registerFunctions));
    gSS->registerClassStatic<B>(&B::luaConstruct, "factory.b", "b class",
                                LuaClassRegCallback<B>::Type(
                                    &B::registerFunctions));
    gSS->registerClassStatic<C>(&C::luaConstruct, "factory.c", "c class",
                                LuaClassRegCallback<C>::Type(
                                    &C::registerFunctions));
    gSS->registerClassStatic<D>(&D::luaConstruct, "factory.d", "d class",
                                LuaClassRegCallback<D>::Type(
                                    &D::registerFunctions));

    clearAccumulators();

    // Test Just the D class for now (composition).
    {
      lua_State* L = gSS->getLuaState();
      string lastExecTable = "return deleteClass.";
      lastExecTable += LuaScripting::TBL_MD_FUN_LAST_EXEC;
      int tableIndex, numParams;

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

      luaL_dostring(L, lastExecTable.c_str());
      tableIndex = lua_gettop(L);
      lua_pushnil(L);
      numParams = 0;
      while (lua_next(L, tableIndex))
      {
        ++numParams;
        lua_pop(L, 1);
      }

      CHECK_EQUAL(1, numParams);

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
      // values should be in the class (2, 2.63, 'str')
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

      // Check the last exec table for deleteClass...


      {
        luaL_dostring(L, lastExecTable.c_str());
        tableIndex = lua_gettop(L);
        lua_pushnil(L);
        numParams = 0;
        while (lua_next(L, tableIndex))
        {
          ++numParams;
          lua_pop(L, 1);
        }

        lua_pop(L, 1);

        CHECK_EQUAL(1, numParams);
      }

      // Redo deletion
      gSS->exec("provenance.redo()");
      ++a_des; ++b_des; ++c_des; ++d_des;
      compareAccumulators();

      {
        luaL_dostring(L, lastExecTable.c_str());
        tableIndex = lua_gettop(L);
        lua_pushnil(L);
        numParams = 0;
        while (lua_next(L, tableIndex))
        {
          ++numParams;
          lua_pop(L, 1);
        }

        lua_pop(L, 1);

        CHECK_EQUAL(1, numParams);
      }

      // We can check this class after we are done below.
    }

    // No need to clear the accumulators, just keep them going.

    // Test interleaving creation of A,B,C, and D.
    {
      // This 'z' class will be used to test creating / destroying all
      // classes when we issue an undo delete on 'z'.
      LuaClassInstance z = gSS->cexecRet<LuaClassInstance>("factory.b.new",gSS);
      ++b_con;
      compareAccumulators();
      string zn = z.fqName();

      // --== Create d ==--
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
     // A* d_c_ap = d_c_a.getRawPointer<A>(gSS);

      // Set misc values for d
      // Set B's variables through C functions
      gSS->exec(dn + ".set_i1(643)");
      gSS->exec(dn + ".set_f1(34.83)");
      gSS->exec(dn + ".set_s1('James')");

      // Test that B's variables were set.
      CHECK_EQUAL(643, d_bp->i1);
      CHECK_CLOSE(34.83, d_bp->f1, 0.0001f);
      CHECK_EQUAL("James", d_bp->s1.c_str());

      // --== Create a ==--
      LuaClassInstance a = gSS->cexecRet<LuaClassInstance>("factory.a.new",
          42, 42.42, "4242-10", gSS);
      ++a_con;
      compareAccumulators();

      string an = a.fqName();
      //A* ap = a.getRawPointer<A>(gSS);

      // Set misc values for a/d
      gSS->exec(an + ".set_i2(158)");
      gSS->exec(an + ".set_f2(345.89)");
      gSS->cexec(an + ".set_s2", "A str");

      gSS->exec(dn + ".set_i1(128)");
      gSS->exec(dn + ".set_f1(64.64)");
      gSS->exec(dn + ".set_s1('bit')");

      // --== Create c ==--
      LuaClassInstance c = gSS->execRet<LuaClassInstance>("factory.c.new()");
      ++c_con; ++a_con;
      compareAccumulators();

      string cn = c.fqName();
      //C* cp = c.getRawPointer<C>(gSS);

      gSS->exec(cn + ".set_i1(64)");
      gSS->exec(cn + ".set_f1(32.32)");
      gSS->exec(cn + ".set_s1('b--')");

      // --== Create b ==--
      LuaClassInstance b = gSS->cexecRet<LuaClassInstance>("factory.b.new",gSS);
      ++b_con;
      compareAccumulators();

      string bn = b.fqName();
      //B* bp = b.getRawPointer<B>(gSS);

      gSS->exec(bn + ".set_i1(32)");
      gSS->exec(bn + ".set_f1(16.16)");
      gSS->exec(bn + ".set_s1('-it')");

      // --== delete a ==--
      gSS->exec("deleteClass(" + an + ")");
      ++a_des;
      compareAccumulators();

      // set misc values for b,c,d
      gSS->exec(bn + ".set_i1(16)");
      gSS->exec(bn + ".set_f1(8.8)");
      gSS->exec(bn + ".set_s1('test')");

      gSS->exec(cn + ".set_i1(8)");
      gSS->exec(cn + ".set_f1(4.4)");
      gSS->exec(cn + ".set_s1('test2')");

      gSS->exec(dn + ".set_a_i2(4)");
      gSS->exec(dn + ".set_a_f2(2.2)");
      gSS->exec(dn + ".set_a_s2('test3')");

      // delete d
      gSS->exec("deleteClass(" + dn + ")");
      ++a_des; ++b_des; ++c_des; ++d_des;
      compareAccumulators();

      // set misc values for c and b
      gSS->exec(bn + ".set_i1(2)");
      gSS->exec(bn + ".set_f1(1.1)");
      gSS->exec(bn + ".set_s1('t1')");

      gSS->exec(cn + ".set_i1(256)");
      gSS->exec(cn + ".set_f1(128.128)");
      gSS->exec(cn + ".set_s1('t2')");

      // delete b
      gSS->exec("deleteClass(" + bn + ")");
      ++b_des;
      compareAccumulators();

      // set misc values for c
      gSS->exec(cn + ".set_a_i2(512)");
      gSS->exec(cn + ".set_a_f2(256.256)");
      gSS->exec(cn + ".set_a_s2('t3')");

      // delete c
      gSS->exec("deleteClass(" + cn + ")");
      ++a_des; ++c_des;
      compareAccumulators();

      // delete z
      gSS->exec("deleteClass(" + zn + ")");
      ++b_des;

      // test undo/redo of this system.

      clearAccumulators();

      // Undo the deletion of z -- this is an extremely large undertaking.
      // This is the worst case scenario for the brute reroll algorithm in
      // the provenance system. It ends undoing / redoing everything since
      // we began this system.
      gSS->exec("provenance.undo()");
      // Keep in mind, everything has been destroyed, so no destructor increase
      // will be recorded. BUT, every class should be created and destroyed...
      ++b_con;
      ++a_con; ++c_con; ++a_des; ++c_des;
      ++b_con; ++b_des;
      ++a_con; ++b_con; ++c_con; ++d_con; ++a_des; ++b_des; ++c_des; ++d_des;
      ++a_con; ++a_des;
      compareAccumulators();

      // All classes but z were created and destroyed in the last call
      // (due to the nature of brute reroll -- a more intelligent algorithm
      //  could be built. But this works for now, and serves as the base case
      //  we know works).

      // Undo the deletion of C.
      gSS->exec("provenance.undo()");
      ++c_con; ++a_con;
      ++b_con; ++b_des;
      ++a_con; ++b_con; ++c_con; ++d_con; ++a_des; ++b_des; ++c_des; ++d_des;
      ++a_con; ++a_des;
      compareAccumulators();


    }

    // Get rid of our global instance.
    // Because of the bad form we used above (not placing weak pointers in
    // the instances of the lua classes, and instead reference a global
    // variabl that contains a reference to the scripting class) we have to get
    // rid of all registered lua classes before we call reset).
    gSS->clean();
    gSS->removeAllRegistrations();
    gSS.reset();
  }

  // Keep a weak pointer reference to ss if stored in X.
  class X
  {
  public:

    X(shared_ptr<LuaScripting> ss)
    {}

    int i1;
    float f1;
    string s1;

    void set_i1(int i)    {i1 = i;}
    int get_i1()          {return i1;}

    void set_f1(float f)  {f1 = f;}
    float get_f1()        {return f1;}

    void set_s1(string s) {s1 = s;}
    string get_s1()       {return s1;}

    static void registerFunctions(LuaClassRegistration<X>& d, X* me,
                                  LuaScripting* ss)
    {
      d.function(&X::set_i1, "set_i1", "", true);
      d.function(&X::get_i1, "get_i1", "", false);

      d.function(&X::set_f1, "set_f1", "", true);
      d.function(&X::get_f1, "get_f1", "", false);

      d.function(&X::set_s1, "set_s1", "", true);
      d.function(&X::get_s1, "get_s1", "", false);
    }

  };

  class Y
  {
  public:

    Y(shared_ptr<LuaScripting> ss)
    : mSS(ss)
    {}

    X* constructX()
    {
      return new X(mSS);
    }

  private:
    shared_ptr<LuaScripting> mSS;
  };

  TEST(MemberFunctionConstructor)
  {
    TEST_HEADER;

    // Tests out using member functions as constructors.
    shared_ptr<LuaScripting> sc(new LuaScripting());

    Y* y = new Y(sc);

    // Register class definitions.
    sc->registerClass<X>(y, &Y::constructX, "factory.x", "",
                           LuaClassRegCallback<X>::Type(
                               &X::registerFunctions));

    // Test the classes.
    LuaClassInstance xc = sc->cexecRet<LuaClassInstance>(
        "factory.x.new");

    // Testing only the first instance of a1.
    std::string xInst = xc.fqName();
    X* x = xc.getRawPointer<X>(sc);

    // Call into the class.
    sc->exec(xInst + ".set_i1(15)");
    sc->exec(xInst + ".set_f1(1.5)");
    sc->cexec(xInst + ".set_s1", "String 1");

    CHECK_EQUAL(15, x->i1);
    CHECK_CLOSE(1.5f, x->f1, 0.001f);
    CHECK_EQUAL("String 1", x->s1.c_str());

    sc->clean();

    delete y;
  }

  TEST(PointerRetrievalOfClasses)
  {
    TEST_HEADER;

    shared_ptr<LuaScripting> sc(new LuaScripting());

    // Register class definitions.
    sc->registerClassStatic<A>(&A::luaConstruct, "factory.a1", "a class",
                                LuaClassRegCallback<A>::Type(
                                    &A::registerFunctions));

    // Test the classes.
    LuaClassInstance a_1 = sc->cexecRet<LuaClassInstance>(
        "factory.a1.new", 2, 2.63, "str", sc);

    // Testing only the first instance of a1.
    std::string aInst = a_1.fqName();
    A* a = a_1.getRawPointer<A>(sc);

    LuaClassInstance aAlt = sc->getLuaClassInstance(reinterpret_cast<void*>(a));

    CHECK_EQUAL(a_1.getGlobalInstID(), aAlt.getGlobalInstID());

    sc->clean();
  }

  TEST(ClassHelpAndLog)
  {
    // Help should be given for classes, but not for any of their instances
    // in the _sys_ table.
  }

  TEST(ClassRTTITypeChecks)
  {
    shared_ptr<LuaScripting> sc(new LuaScripting());
  }

}

SUITE(LuaTestClassRegistration_MultipleInheritenceFromTop)
{

  // The classes knowing about A is not realistic, but serves to test our
  // casting implmentation well.
  // A more realistic implementation is the from bottom unit test.
  class A;

  class B
  {
  public:

    virtual ~B() {};

    void registerBFuncs(LuaClassRegistration<A>& reg);

    void set_b1(int b)  {b1 = b;}
    int get_b1()        {return b1;}

    virtual string B_className() = 0;

    int b1;
  };

  class C
  {
  public:

    virtual ~C() {};

    void registerCFuncs(LuaClassRegistration<A>& reg);

    void set_c1(int v)  {c1 = v;}
    int get_c1()        {return c1;}

    virtual string C_className()  {return "C Class";}

    int c1;

  };

  class X
  {
  public:

    virtual ~X() {};

    void registerXFuncs(LuaClassRegistration<A>& reg);

    void set_x1(int v)  {x1 = v;}
    int get_x1()        {return x1;}

    virtual string X_className()  {return "X class";}

    int x1;

  };

  class Y
  {
  public:

    virtual ~Y() {};

    void registerYFuncs(LuaClassRegistration<A>& reg);

    void set_y1(int v)  {y1 = v;}
    int get_y1()        {return y1;}

    virtual string Y_className()  {return "Y class";}

    int y1;

  };

  class Z : public X, public Y
  {
  public:

    virtual ~Z() {};

    void registerZFuncs(LuaClassRegistration<A>& reg);

    // We only override X, not Y.
    virtual string Y_className() {return "Z from Y class";}
    virtual string X_className() {return "Z from X class";}

    virtual string Z_className() {return "Z class";}

    void set_z1(int v)  {z1 = v;}
    int get_z1()        {return z1;}

    void set_z2(int v)  {z2 = v;}
    int get_z2()        {return z2;}

    int z1;
    int z2;

  };


  int a_constructor = 0;
  int a_destructor = 0;
  class A : public C, public B, public Z
  {
  public:

    A(int a, float b, string c, shared_ptr<LuaScripting> ss)
    {
      ++a_constructor;
      i1 = a;
      f1 = b;
      s1 = c;
    }

    ~A()
    {
      ++a_destructor;
    }

    int     i1;
    float   f1;
    string  s1;

    void set_i1(int i)    {i1 = i;}
    int get_i1()          {return i1;}

    void set_f1(float f)  {f1 = f;}
    float get_f1()        {return f1;}

    void set_s1(string s) {s1 = s;}
    string get_s1()       {return s1;}

    // Class definition. The real meat defining a class.
    static void registerFunctions(LuaClassRegistration<A>& d, A* me,
                                  LuaScripting* ss)
    {
      d.function(&A::set_i1, "set_i1", "", true);
      d.function(&A::get_i1, "get_i1", "", false);

      d.function(&A::set_f1, "set_f1", "", true);
      d.function(&A::get_f1, "get_f1", "", false);

      d.function(&A::set_s1, "set_s1", "", true);
      d.function(&A::get_s1, "get_s1", "", false);

      me->registerBFuncs(d);
      me->registerCFuncs(d);
      me->registerXFuncs(d);
      me->registerYFuncs(d);
      me->registerZFuncs(d);
    }

    // Overriden methods
    // Override B (mandatory)
    virtual string B_className() {return "A from B class";}
    // Do NOT override Z_className
    // Do NOT override X_className
    // Override Y
    virtual string Y_className()  {return "A from Y class";}
    // Override C
    virtual string C_className()  {return "A from C class";}

    static A* luaConstruct(int a, float b, string c,
                           shared_ptr<LuaScripting> ss)
    { return new A(a, b, c, ss); }
  };

  void B::registerBFuncs(LuaClassRegistration<A>& reg)
  {
    reg.function(&B::set_b1, "set_b1", "", true);
    reg.function(&B::get_b1, "get_b1", "", false);
    reg.function(&B::B_className, "B_className", "", false);
  }

  void C::registerCFuncs(LuaClassRegistration<A>& reg)
  {
    reg.function(&C::set_c1, "set_c1", "", true);
    reg.function(&C::get_c1, "get_c1", "", false);
    reg.function(&C::C_className, "C_className", "", false);
  }

  void X::registerXFuncs(LuaClassRegistration<A>& reg)
  {
    reg.function(&X::set_x1, "set_x1", "", true);
    reg.function(&X::get_x1, "get_x1", "", false);
    reg.function(&X::X_className, "X_className", "", false);
  }

  void Y::registerYFuncs(LuaClassRegistration<A>& reg)
  {
    reg.function(&Y::set_y1, "set_y1", "", true);
    reg.function(&Y::get_y1, "get_y1", "", false);
    reg.function(&Y::Y_className, "Y_className", "", false);
  }

  void Z::registerZFuncs(LuaClassRegistration<A>& reg)
  {
    reg.function(&Z::set_z1, "set_z1", "", true);
    reg.function(&Z::get_z1, "get_z1", "", false);
    reg.function(&Z::set_z2, "set_z2", "", true);
    reg.function(&Z::get_z2, "get_z2", "", false);
    reg.function(&Z::Z_className, "Z_className", "", false);
  }

  TEST(TestMultipleInheritence_FromTop)
  {
    TEST_HEADER;

    // Help should be given for classes, but not for any of their instances
    // in the _sys_ table.

    shared_ptr<LuaScripting> sc(new LuaScripting());

    // Register class definitions.
    sc->registerClassStatic<A>(&A::luaConstruct, "factory.a1", "",
                               LuaClassRegCallback<A>::Type(
                                   &A::registerFunctions));

    // Test the classes.
    LuaClassInstance a_1 = sc->cexecRet<LuaClassInstance>(
        "factory.a1.new", 2, 2.63, "str", sc);

    // Testing only the first instance of a1.
    std::string nm = a_1.fqName();
    A* a = a_1.getRawPointer<A>(sc);

    // Test out function registrations and check the results.
    sc->cexec(nm + ".set_x1", 11);
    CHECK_EQUAL(11, sc->cexecRet<int>(nm + ".get_x1"));

    sc->cexec(nm + ".set_y1", 21);
    CHECK_EQUAL(21, sc->cexecRet<int>(nm + ".get_y1"));

    sc->cexec(nm + ".set_z1", 101);
    CHECK_EQUAL(101, sc->cexecRet<int>(nm + ".get_z1"));

    sc->cexec(nm + ".set_z2", 102);
    CHECK_EQUAL(102, sc->cexecRet<int>(nm + ".get_z2"));

    sc->cexec(nm + ".set_b1", 500);
    CHECK_EQUAL(500, sc->cexecRet<int>(nm + ".get_b1"));

    sc->cexec(nm + ".set_c1", 1000);
    CHECK_EQUAL(1000, sc->cexecRet<int>(nm + ".get_c1"));

    sc->exec(nm + ".set_i1(15)");
    sc->exec(nm + ".set_f1(1.5)");
    sc->cexec(nm + ".set_s1", "String 1");

    CHECK_EQUAL(15, a->i1);
    CHECK_CLOSE(1.5f, a->f1, 0.001f);
    CHECK_EQUAL("String 1", a->s1.c_str());

    // Test overriden methods.
    CHECK_EQUAL(sc->cexecRet<string>(nm + ".B_className").c_str(),
                "A from B class");
    CHECK_EQUAL(sc->cexecRet<string>(nm + ".C_className").c_str(),
                "A from C class");
    CHECK_EQUAL(sc->cexecRet<string>(nm + ".X_className").c_str(),
                "Z from X class");
    CHECK_EQUAL(sc->cexecRet<string>(nm + ".Y_className").c_str(),
                "A from Y class");
    CHECK_EQUAL(sc->cexecRet<string>(nm + ".Z_className").c_str(),
                "Z class");

    cout << "--";
    cout << "--" << endl;

    sc->clean();
  }
}

SUITE(LuaTestClassRegistration_MultipleInheritenceFromBottom)
{
  // The classes knowing about A is not realistic, but serves to test our
  // casting implmentation well.
  // A more realistic implementation is the from bottom unit test.
  class A;

  class B
  {
  public:

    virtual ~B() {};

    void set_b1(int b)  {b1 = b;}
    int get_b1()        {return b1;}

    virtual string B_className() = 0;

    int b1;
  };

  class C
  {
  public:

    virtual ~C() {};

    void set_c1(int v)  {c1 = v;}
    int get_c1()        {return c1;}

    virtual string C_className()  {return "C Class";}

    int c1;

  };


  class X
  {
  public:

    X(shared_ptr<LuaScripting> ss)
    {}
    virtual ~X() {};

    static void registerXFuncs(LuaClassRegistration<X>& d, X* me,
                               LuaScripting* ss)
    {
      d.function(&X::set_x1, "set_x1", "", true);
      d.function(&X::get_x1, "get_x1", "", false);
      d.function(&X::X_className, "X_className", "", false);

      me->registerDerived(d);
    }

    virtual void registerDerived(LuaClassRegistration<X>& d) {}

    void set_x1(int v)  {x1 = v;}
    int get_x1()        {return x1;}

    virtual string X_className()  {return "X class";}

    int x1;

  };

  class Y
  {
  public:

    virtual ~Y() {};

    void set_y1(int v)  {y1 = v;}
    int get_y1()        {return y1;}

    virtual string Y_className()  {return "Y class";}

    int y1;

  };

  class Z : public X, public Y
  {
  public:

    Z(shared_ptr<LuaScripting> ss)
    : X(ss)
    {}

    virtual ~Z() {};

    virtual void registerDerived(LuaClassRegistration<X>& d)
    {
      X::registerDerived(d);

      d.function(&Z::set_z1, "set_z1", "", true);
      d.function(&Z::get_z1, "get_z1", "", false);
      d.function(&Z::set_z2, "set_z2", "", true);
      d.function(&Z::get_z2, "get_z2", "", false);
      d.function(&Z::Z_className, "Z_className", "", false);
    }

    // We only override X, not Y.
    virtual string Y_className() {return "Z from Y class";}
    virtual string X_className() {return "Z from X class";}

    virtual string Z_className() {return "Z class";}

    void set_z1(int v)  {z1 = v;}
    int get_z1()        {return z1;}

    void set_z2(int v)  {z2 = v;}
    int get_z2()        {return z2;}

    int z1;
    int z2;

  };


  int a_constructor = 0;
  int a_destructor = 0;
  class A : public C, public B, public Z
  {
  public:

    A(int a, float b, string c, shared_ptr<LuaScripting> ss)
    : Z(ss)
    {
      ++a_constructor;
      i1 = a;
      f1 = b;
      s1 = c;
    }

    ~A()
    {
      ++a_destructor;
    }

    int     i1;
    float   f1;
    string  s1;

    void set_i1(int i)    {i1 = i;}
    int get_i1()          {return i1;}

    void set_f1(float f)  {f1 = f;}
    float get_f1()        {return f1;}

    void set_s1(string s) {s1 = s;}
    string get_s1()       {return s1;}

    // Class definition. The real meat defining a class.
    virtual void registerDerived(LuaClassRegistration<X>& d)
    {
      Z::registerDerived(d);

      d.function(&A::set_i1, "set_i1", "", true);
      d.function(&A::get_i1, "get_i1", "", false);

      d.function(&A::set_f1, "set_f1", "", true);
      d.function(&A::get_f1, "get_f1", "", false);

      d.function(&A::set_s1, "set_s1", "", true);
      d.function(&A::get_s1, "get_s1", "", false);

      // There is NO dynamic cast from X to B, C, or Y. Therefore, we cannot
      // register ANY of their functions.
      // But we can register these functions as A.
      d.function(&A::B_className, "B_className", "", false);
      d.function(&A::C_className, "C_className", "", false);
      d.function(&A::Y_className, "Y_className", "", false);

      d.function(&A::set_b1, "set_b1", "", true);
      d.function(&A::get_b1, "get_b1", "", false);

      d.function(&A::set_c1, "set_c1", "", true);
      d.function(&A::get_c1, "get_c1", "", false);

      d.function(&A::set_y1, "set_y1", "", true);
      d.function(&A::get_y1, "get_y1", "", false);
    }

    // Overriden methods
    // Override B (mandatory)
    virtual string B_className() {return "A from B class";}
    // Do NOT override Z_className
    // Do NOT override X_className
    // Override Y
    virtual string Y_className()  {return "A from Y class";}
    // Override C
    virtual string C_className()  {return "A from C class";}

    static A* luaConstruct(int a, float b, string c,
                           shared_ptr<LuaScripting> ss)
    { return new A(a, b, c, ss); }
  };

  TEST(TestMultipleInheritence_FromBottom)
  {
    TEST_HEADER;

    // Help should be given for classes, but not for any of their instances
    // in the _sys_ table.

    shared_ptr<LuaScripting> sc(new LuaScripting());

    // Register class definitions.
    sc->registerClassStatic<X>(&A::luaConstruct, "factory.a1", "",
                               LuaClassRegCallback<X>::Type(&X::registerXFuncs)
                               );

    // Test the classes.
    LuaClassInstance a_1 = sc->cexecRet<LuaClassInstance>(
        "factory.a1.new", 2, 2.63, "str", sc);

    // Testing only the first instance of a1.
    std::string nm = a_1.fqName();
    A* a = a_1.getRawPointer<A>(sc);

    // Test out function registrations and check the results.
    sc->cexec(nm + ".set_x1", 11);
    CHECK_EQUAL(11, sc->cexecRet<int>(nm + ".get_x1"));

    sc->cexec(nm + ".set_y1", 21);
    CHECK_EQUAL(21, sc->cexecRet<int>(nm + ".get_y1"));

    sc->cexec(nm + ".set_z1", 101);
    CHECK_EQUAL(101, sc->cexecRet<int>(nm + ".get_z1"));

    sc->cexec(nm + ".set_z2", 102);
    CHECK_EQUAL(102, sc->cexecRet<int>(nm + ".get_z2"));

    sc->cexec(nm + ".set_b1", 500);
    CHECK_EQUAL(500, sc->cexecRet<int>(nm + ".get_b1"));

    sc->cexec(nm + ".set_c1", 1000);
    CHECK_EQUAL(1000, sc->cexecRet<int>(nm + ".get_c1"));

    sc->exec(nm + ".set_i1(15)");
    sc->exec(nm + ".set_f1(1.5)");
    sc->cexec(nm + ".set_s1", "String 1");

    CHECK_EQUAL(15, a->i1);
    CHECK_CLOSE(1.5f, a->f1, 0.001f);
    CHECK_EQUAL("String 1", a->s1.c_str());

    // Test overriden methods.
    CHECK_EQUAL(sc->cexecRet<string>(nm + ".B_className").c_str(),
                "A from B class");
    CHECK_EQUAL(sc->cexecRet<string>(nm + ".C_className").c_str(),
                "A from C class");
    CHECK_EQUAL(sc->cexecRet<string>(nm + ".X_className").c_str(),
                "Z from X class");
    CHECK_EQUAL(sc->cexecRet<string>(nm + ".Y_className").c_str(),
                "A from Y class");
    CHECK_EQUAL(sc->cexecRet<string>(nm + ".Z_className").c_str(),
                "Z class");

    cout << "--";
    cout << "--" << endl;

    sc->clean();
  }
}

#endif

