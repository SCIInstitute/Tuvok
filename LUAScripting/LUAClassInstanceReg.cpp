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
  bool a_destructor = false;

  class A
  {
  public:

    A(int a, float b, string c)
    {
      i1 = a; i2 = 0;
      f1 = b; f2 = 0.0f;
      s1 = c;
    }

    ~A()
    {
      a_destructor = true;
      cout << "Destructor called" << endl;
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

  TEST(ClassRegistration)
  {
    TEST_HEADER;

    a_destructor = false;

    {
      tr1::shared_ptr<LuaScripting> sc(new LuaScripting());

      sc->addLuaClassDef(&A::luaDefineClass, "factory.a1");

      // Test the classes.
      LuaClassInstance a_1 = sc->execRet<LuaClassInstance>(
          "factory.a1.new(2, 2.63, 'str')");

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

    // Ensure destructor was called.
    CHECK_EQUAL(true, a_destructor);

  }

  TEST(ClassHooks)
  {

  }

  TEST(ClassProvenance)
  {

  }

  TEST(ClassHelpAndLog)
  {

  }

}

#endif


} /* namespace tuvok */
