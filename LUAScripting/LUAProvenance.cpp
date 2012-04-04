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
 \brief   Provenance system composited inside of the LuaScripting class.
 */

#ifndef EXTERNAL_UNIT_TESTING

#include "Controller/Controller.h"
#include "3rdParty/LUA/lua.hpp"

#else

#include <assert.h>
#include "utestCommon.h"

#endif

#include <vector>

#include "LUAError.h"
#include "LUAFunBinding.h"
#include "LUAScripting.h"
#include "LUAProvenance.h"

using namespace std;

#define DEFAULT_PROVENANCE_BUFFER_SIZE  (50)

namespace tuvok
{

//-----------------------------------------------------------------------------
LuaProvenance::LuaProvenance(LuaScripting* scripting)
: mEnabled(true)
, mScripting(scripting)
, mMemberReg(scripting)
, mStackPointer(0)
, mLoggingProvenance(false)
, mDoProvReenterException(true)
{
  mUndoRedoStack.reserve(DEFAULT_PROVENANCE_BUFFER_SIZE);
}

//-----------------------------------------------------------------------------
LuaProvenance::~LuaProvenance()
{
  // We purposefully do NOT call unregisterUndoRedo here.
  // Since we are being destroyed, it is likely the lua_State has already
  // been closed by the class that composited us.
}

//-----------------------------------------------------------------------------
void LuaProvenance::registerLuaProvenanceFunctions()
{
  // NOTE: We cannot use the LuaMemberReg class to manage our registered
  // functions because it relies on a shared pointer to LuaScripting, and
  // since we are composited inside of LuaScripting, no such shared pointer
  // is available.
  mMemberReg.registerFunction(this, &LuaProvenance::issueUndo,
                              "provenance.undo",
                              "Undoes last script call.");
  mScripting->setUndoRedoStackExempt("provenance.undo", true);
  mMemberReg.registerFunction(this, &LuaProvenance::issueRedo,
                              "provenance.redo",
                              "Redoes the last undo call.");
  mScripting->setUndoRedoStackExempt("provenance.redo", true);
  mMemberReg.registerFunction(this, &LuaProvenance::setEnabled,
                              "provenance.enable",
                              "Enable/Disable provenance. This is not an "
                              "undo-able action and will clear your provenance "
                              "history if disabled.");
  mScripting->setUndoRedoStackExempt("provenance.enable", true);
  mMemberReg.registerFunction(this, &LuaProvenance::clearProvenance,
                              "provenance.clear",
                              "Clears all provenance and undo/redo stacks. "
                              "This is not an undo-able action.");
  mScripting->setUndoRedoStackExempt("provenance.clear", true);
  mMemberReg.registerFunction(this, &LuaProvenance::enableProvReentryEx,
                              "provenance.enableReentryException",
                              "Enables/Disables the provenance reentry "
                              "exception. Disable this to (take a deep breath) "
                              "allow functions registered with LuaScripting to "
                              "call other functions registered within "
                              "LuaScripting from within Lua.");
  // Reentry exception does not need to be stack exempt.
}

//-----------------------------------------------------------------------------
bool LuaProvenance::isEnabled()
{
  return mEnabled;
}


//-----------------------------------------------------------------------------
void LuaProvenance::setEnabled(bool enabled)
{
  if (enabled == false && mEnabled == true)
  {
    clearProvenance();
  }

  mEnabled = enabled;
}

//-----------------------------------------------------------------------------
void LuaProvenance::logExecution(const string& fname,
                                 bool undoRedoStackExempt,
                                 tr1::shared_ptr<LuaCFunAbstract> funParams,
                                 tr1::shared_ptr<LuaCFunAbstract> emptyParams)
{
  if (mLoggingProvenance)
  {
    if (mDoProvReenterException)
    {
      /// Throw provenance reentry exception.
      throw LuaProvenanceReenter("LuaProvenance reentry not allowed. Consider"
                                 "disabling provenance.enableReentryException");
    }
    else
    {
      return;
    }
  }

  mLoggingProvenance = true;  // Used to tell when someone has done something
                              // bad: exec a registered lua function within
                              // another registered lua function.

  // Add provenance before this check. Undo and redo reside below this call.
  if (undoRedoStackExempt)
  {
    mLoggingProvenance = false;
    return;
  }

  // Erase redo hisory if we have a stack pointer beneath the top of the stack.
  for (int i = 0; i < mUndoRedoStack.size() - mStackPointer; i++)
  {
    mUndoRedoStack.pop_back();
  }

  // Gather last execution parameters for inclusion in the undo stack.
  lua_State* L = mScripting->getLUAState();
  int stackTop = lua_gettop(L);
  mScripting->getFunctionTable(fname.c_str());
  lua_getfield(L, -1, LuaScripting::TBL_MD_FUN_LAST_EXEC);
  int lastExecTable = lua_gettop(L);

  lua_checkstack(L, LUAC_MAX_NUM_PARAMS + 2); // key/value pair.

  // Count the number of parameters.
  int numParams = 0;
  lua_pushnil(L);
  while (lua_next(L, lastExecTable))
  {
    lua_pop(L, 1);
    ++numParams;
  }

  // Populate the stack in the correct order (order is incredibly important!)
  for (int i = 0;  i < numParams; i++)
  {
    lua_pushinteger(L, i);
    lua_gettable(L, lastExecTable);
  }

  // Now we have all of the parameters at the top of the stack, extract them
  // using emptyParams.
  if (numParams != 0)
  {
    int stackTopWithParams = lua_gettop(L);
    emptyParams->pullParamsFromStack(L, stackTopWithParams - (numParams - 1));
    lua_pop(L, numParams);
  }

  mUndoRedoStack.push_back(UndoRedoItem(fname, emptyParams, funParams));
  ++mStackPointer;

  // Repopulate the lastExec table to most recently executed function parameters
  // We are overwriting the previous entries (see
  // createDefaultsAndLastExecTables in LuaScripting).
  int firstParam = lua_gettop(L) + 1;
  funParams->pushParamsToStack(L);
  assert(numParams == (lua_gettop(L) - (firstParam - 1)));

  for (int i = 0; i < numParams; i++)
  {
    lua_pushinteger(L, i);
    lua_pushvalue(L, firstParam + i);
    lua_settable(L, lastExecTable);
  }

  lua_pop(L, numParams);
  lua_pop(L, 2);          // Function's table and last exec table.

  mLoggingProvenance = false;

  assert(stackTop == lua_gettop(L));
}

//-----------------------------------------------------------------------------
void LuaProvenance::issueUndo()
{
  if (mStackPointer == 0)
  {
    throw LuaProvenanceInvalidUndo("Undo pointer at bottom of stack.");
  }
}

//-----------------------------------------------------------------------------
void LuaProvenance::issueRedo()
{
  if (mStackPointer == mUndoRedoStack.size())
  {
    throw LuaProvenanceInvalidRedo("Redo pointer at top of stack.");
  }
}

//-----------------------------------------------------------------------------
void LuaProvenance::clearProvenance()
{
  mUndoRedoStack.clear();
  mStackPointer = 0;
}

//-----------------------------------------------------------------------------
void LuaProvenance::enableProvReentryEx(bool enable)
{
  mDoProvReenterException = enable;
}

//==============================================================================
//
// UNIT TESTING
//
//==============================================================================


SUITE(LuaProvenanceTests)
{
  class A
  {
  public:

    A(tr1::shared_ptr<LuaScripting> ss)
    : mReg(ss)
    {}

    int     i1 = 0, i2 = 0;
    float   f1 = 0.0f, f2 = 0.0f;
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

    LuaMemberReg mReg;
  };

  TEST(ProvenanceClassTests)
  {
    TEST_HEADER;

    tr1::shared_ptr<LuaScripting> sc(new LuaScripting());
    lua_State* L = sc->getLUAState();

    auto_ptr<A> a(new A(sc));

    a->mReg.registerFunction(a.get(), &A::set_i1, "set_i1", "");
    a->mReg.registerFunction(a.get(), &A::set_i2, "set_i2", "");
    a->mReg.registerFunction(a.get(), &A::get_i1, "get_i1", "");
    a->mReg.registerFunction(a.get(), &A::get_i2, "get_i2", "");

    a->mReg.registerFunction(a.get(), &A::set_f1, "set_f1", "");
    a->mReg.registerFunction(a.get(), &A::set_f2, "set_f2", "");
    a->mReg.registerFunction(a.get(), &A::get_f1, "get_f1", "");
    a->mReg.registerFunction(a.get(), &A::get_f2, "get_f2", "");

    a->mReg.registerFunction(a.get(), &A::set_s1, "set_s1", "");
    a->mReg.registerFunction(a.get(), &A::set_s2, "set_s2", "");
    a->mReg.registerFunction(a.get(), &A::get_s1, "get_s1", "");
    a->mReg.registerFunction(a.get(), &A::get_s2, "get_s2", "");

    luaL_dostring(L, "set_i1(1)");
    luaL_dostring(L, "set_i2(10)");
    luaL_dostring(L, "set_i1(2)");
    luaL_dostring(L, "set_i1(3)");
    luaL_dostring(L, "set_i2(20)");
    luaL_dostring(L, "set_f1(2.3)");
    luaL_dostring(L, "set_s1(\"T\")");
    luaL_dostring(L, "set_s1(\"Test\")");
    luaL_dostring(L, "set_s2(\"Test2\")");
    luaL_dostring(L, "set_f1(1.5)");
    luaL_dostring(L, "set_i1(100)");
    luaL_dostring(L, "set_i2(30)");
    luaL_dostring(L, "set_f2(-5.3)");

    // Check final values.
    CHECK_EQUAL(a->i1, 100);
    CHECK_EQUAL(a->i2, 30);

    CHECK_CLOSE(a->f1, 1.5f, 0.001f);
    CHECK_CLOSE(a->f2, -5.3f, 0.001f);

    CHECK_EQUAL(a->s1.c_str(), "Test");
    CHECK_EQUAL(a->s2.c_str(), "Test2");

    // Test to see if we 'throw' if we redo passed where there is no redo.
    // This should NOT affect the current state of the undo buffer.
    CHECK_THROW(luaL_dostring(L, "provenance.redo()"),
                LuaProvenanceInvalidRedo);
    CHECK_THROW(luaL_dostring(L, "provenance.undo()"),
                LuaProvenanceInvalidUndo);

    // Begin issuing undo / redos
    luaL_dostring(L, "provenance.undo()");
    CHECK_CLOSE(a->f2, 0.0f, 0.001f);
    luaL_dostring(L, "provenance.redo()");
    CHECK_CLOSE(a->f2, -5.3f, 0.001f);
    luaL_dostring(L, "provenance.undo()");
    CHECK_CLOSE(a->f2, 0.0f, 0.001f);
    luaL_dostring(L, "provenance.undo()");
    CHECK_EQUAL(a->i2, 20);
    luaL_dostring(L, "provenance.undo()");
    CHECK_EQUAL(a->i1, 3);
    luaL_dostring(L, "provenance.redo()");
    CHECK_EQUAL(a->i1, 100);
    luaL_dostring(L, "provenance.undo()");
    CHECK_EQUAL(a->i1, 3);
    luaL_dostring(L, "provenance.undo()");
    CHECK_CLOSE(a->f1, 2.3f, 0.001f);
    luaL_dostring(L, "provenance.undo()");
    CHECK_EQUAL(a->s2.c_str(), "");
    luaL_dostring(L, "provenance.redo()");
    CHECK_EQUAL(a->s2.c_str(), "Test2");
    luaL_dostring(L, "provenance.redo()");
    CHECK_CLOSE(a->f1, 1.5f, 0.001f);
    luaL_dostring(L, "provenance.redo()");
    CHECK_EQUAL(a->i1, 100);
    luaL_dostring(L, "provenance.undo()");
    CHECK_EQUAL(a->i1, 3);
    luaL_dostring(L, "provenance.undo()");
    CHECK_CLOSE(a->f1, 2.3f, 0.001f);
    luaL_dostring(L, "provenance.undo()");
    CHECK_EQUAL(a->s2.c_str(), "");
    luaL_dostring(L, "provenance.undo()");
    CHECK_EQUAL(a->s1.c_str(), "T");
    luaL_dostring(L, "provenance.undo()");
    CHECK_EQUAL(a->s1.c_str(), "");
    luaL_dostring(L, "provenance.undo()");
    CHECK_CLOSE(a->f1, 0.0f, 0.001f);
    luaL_dostring(L, "provenance.undo()");
    CHECK_EQUAL(a->i2, 10);
    luaL_dostring(L, "provenance.undo()");
    CHECK_EQUAL(a->i1, 2);
    luaL_dostring(L, "provenance.undo()");
    CHECK_EQUAL(a->i1, 1);
    luaL_dostring(L, "provenance.undo()");
    CHECK_EQUAL(a->i2, 0);
    luaL_dostring(L, "provenance.undo()");
    CHECK_EQUAL(a->i1, 0);

    // This invalid undo should not destroy state.
    CHECK_THROW(luaL_dostring(L, "provenance.undo()"),
                LuaProvenanceInvalidUndo);

    // Check to make sure default values are present.
    CHECK_EQUAL(a->i1, 0);
    CHECK_EQUAL(a->i2, 0);

    CHECK_CLOSE(a->f1, 0.0f, 0.001f);
    CHECK_CLOSE(a->f2, 0.0f, 0.001f);

    CHECK_EQUAL(a->s1.c_str(), "");
    CHECK_EQUAL(a->s2.c_str(), "");

    // Check redoing EVERYTHING.
    luaL_dostring(L, "provenance.redo()");
    CHECK_EQUAL(a->i1, 1);
    luaL_dostring(L, "provenance.redo()");
    CHECK_EQUAL(a->i2, 10);
    luaL_dostring(L, "provenance.redo()");
    CHECK_EQUAL(a->i1, 2);
    luaL_dostring(L, "provenance.redo()");
    CHECK_EQUAL(a->i1, 3);
    luaL_dostring(L, "provenance.redo()");
    CHECK_EQUAL(a->i2, 20);
    luaL_dostring(L, "provenance.redo()");
    CHECK_CLOSE(a->f1, 2.3f, 0.001f);
    luaL_dostring(L, "provenance.redo()");
    CHECK_EQUAL(a->s1, "T");
    luaL_dostring(L, "provenance.redo()");
    CHECK_EQUAL(a->s1, "Test");
    luaL_dostring(L, "provenance.redo()");
    CHECK_EQUAL(a->s2, "Test2");
    luaL_dostring(L, "provenance.redo()");
    CHECK_CLOSE(a->f1, 1.5f, 0.001f);
    luaL_dostring(L, "provenance.redo()");
    CHECK_EQUAL(a->i1, 100);
    luaL_dostring(L, "provenance.redo()");
    CHECK_EQUAL(a->i2, 30);
    luaL_dostring(L, "provenance.redo()");
    CHECK_CLOSE(a->f1, -5.3f, 0.001f);

    CHECK_THROW(luaL_dostring(L, "provenance.redo()"),
                LuaProvenanceInvalidRedo);

    // Check final values again.
    CHECK_EQUAL(a->i1, 100);
    CHECK_EQUAL(a->i2, 30);

    CHECK_CLOSE(a->f1, 1.5f, 0.001f);
    CHECK_CLOSE(a->f2, -5.3f, 0.001f);

    CHECK_EQUAL(a->s1.c_str(), "Test");
    CHECK_EQUAL(a->s2.c_str(), "Test2");

    // Check lopping off sections of the redo buffer.
    luaL_dostring(L, "provenance.undo()");
    luaL_dostring(L, "provenance.undo()");
    luaL_dostring(L, "provenance.undo()");
    luaL_dostring(L, "seti1(42)");

    CHECK_THROW(luaL_dostring(L, "provenance.redo()"),
                LuaProvenanceInvalidRedo);

    luaL_dostring(L, "provenance.undo()");
    luaL_dostring(L, "provenance.undo()");
    luaL_dostring(L, "provenance.redo()");
    luaL_dostring(L, "seti1(45)");

    CHECK_THROW(luaL_dostring(L, "provenance.redo()"),
                LuaProvenanceInvalidRedo);
  }

  TEST(ProvenanceStaticTests)
  {
    // We don't need to test the provenance functionality, just that it is
    // hooked up correctly. The above class test of provenance tests the
    // functionality of the provenance system fairly thoroughly.
  }

}


}
