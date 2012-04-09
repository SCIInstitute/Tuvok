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
#include "LUAMemberReg.h"

using namespace std;

#define DEFAULT_UNDOREDO_BUFFER_SIZE  (50)
#define DEFAULT_PROVENANCE_BUFFER_SIZE  (150)

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
, mUndoRedoProvenanceDisable(false)
, mProvenanceDescLogEnabled(true)
{
  mUndoRedoStack.reserve(DEFAULT_PROVENANCE_BUFFER_SIZE);
  mProvenanceDescList.reserve(DEFAULT_PROVENANCE_BUFFER_SIZE);
}

//-----------------------------------------------------------------------------
LuaProvenance::~LuaProvenance()
{
  // We purposefully do NOT unregister our undo/redo functions.
  // Since we are being destroyed, it is likely the lua_State has already
  // been closed by the class that composited us.
}

//-----------------------------------------------------------------------------
void LuaProvenance::registerLuaProvenanceFunctions()
{
  // NOTE: We cannot use the LuaMemberReg class to manage our registered
  // functions because it relies on a shared pointer to LuaScripting; since we
  // are composited inside of LuaScripting, no such shared pointer is available.
  mMemberReg.registerFunction(this, &LuaProvenance::issueUndo,
                              "provenance.undo",
                              "Undoes last call.",
                              false);
  mMemberReg.registerFunction(this, &LuaProvenance::issueRedo,
                              "provenance.redo",
                              "Redoes the last undo.",
                              false);
  mMemberReg.registerFunction(this, &LuaProvenance::setEnabled,
                              "provenance.enable",
                              "Enable/Disable provenance. This is not an "
                              "undo-able action and will clear your provenance "
                              "history if disabled.",
                              false);
  mMemberReg.registerFunction(this, &LuaProvenance::clearProvenance,
                              "provenance.clear",
                              "Clears all provenance and undo/redo stacks. "
                              "This is not an undo-able action.",
                              false);
  mMemberReg.registerFunction(this, &LuaProvenance::enableProvReentryEx,
                              "provenance.enableReentryException",
                              "Enables/Disables the provenance reentry "
                              "exception. Disable this to (take a deep breath) "
                              "allow functions registered with LuaScripting to "
                              "call other functions registered within "
                              "LuaScripting from within Lua.",
                              true);
  mMemberReg.registerFunction(this, &LuaProvenance::printUndoStack,
                              "provenance.logUndoStack",
                              "Prints the contents of the undo stack"
                              "to 'log.info'.",
                              false);
  mMemberReg.registerFunction(this, &LuaProvenance::printRedoStack,
                              "provenance.logRedoStack",
                              "Prints the contents of the redo stack"
                              "to 'log.info'.",
                              false);
  mMemberReg.registerFunction(this, &LuaProvenance::printProvRecord,
                              "provenance.logProvenanceRecord",
                              "Prints the entire provenance record to"
                              "to 'log.info'.",
                              false);
  // Reentry exception does not need to be stack exempt.
}

//-----------------------------------------------------------------------------
bool LuaProvenance::isEnabled() const
{
  return mEnabled;
}

//-----------------------------------------------------------------------------
void LuaProvenance::enableLogAll(bool enabled)
{
  mProvenanceDescLogEnabled = enabled;

  if (mProvenanceDescLogEnabled == false)
  {
    mProvenanceDescList.clear();
  }
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
void LuaProvenance::ammendLastProvLog(const string& ammend)
{
  assert(mProvenanceDescList.size() > 0);

  string ammendedLog = mProvenanceDescList.back();
  ammendedLog += ammend;

  mProvenanceDescList.pop_back();
  mProvenanceDescList.push_back(ammendedLog);
}

//-----------------------------------------------------------------------------
void LuaProvenance::logHooks(int staticHooks, int memberHooks)
{
  if (mEnabled == false || mProvenanceDescLogEnabled == false)
    return;

  int hooksCalled = staticHooks + memberHooks;

  ostringstream os;
  os << " -- " << hooksCalled << " hook(s) called";

  ammendLastProvLog(os.str());
}

//-----------------------------------------------------------------------------
void LuaProvenance::logExecution(const string& fname,
                                 bool undoRedoStackExempt,
                                 tr1::shared_ptr<LuaCFunAbstract> funParams,
                                 tr1::shared_ptr<LuaCFunAbstract> emptyParams)
{
  // TODO: Add indentation to provenance record for registered functions that
  //       are called within other registered functions.

  // TODO: Add checks on the parameters given. If they are the same, the
  //       function should be ignored.

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

  mLoggingProvenance = true;

  if (mProvenanceDescLogEnabled)
  {
    string provParams = funParams->getFormattedParameterValues();

    ostringstream os;
    if (mUndoRedoProvenanceDisable)
      os << " -- Called: \"";

    os << fname << "(" << provParams << ")";
    if (mUndoRedoProvenanceDisable)
    {
      os << "\"";
      ammendLastProvLog(os.str());
    }
    else
    {
      mProvenanceDescList.push_back(os.str());
    }
  }

  if (undoRedoStackExempt || mUndoRedoProvenanceDisable)
  {
    mLoggingProvenance = false;
    return;
  }

  // Erase redo hisory if we have a stack pointer beneath the top of the stack.
  int stackDiff = mUndoRedoStack.size() - mStackPointer;
  for (int i = 0; i < stackDiff; i++)
  {
    mUndoRedoStack.pop_back();
  }
  assert(mUndoRedoStack.size() == mStackPointer);

  // Gather last execution parameters for inclusion in the undo stack.
  lua_State* L = mScripting->getLUAState();
  int stackTop = lua_gettop(L);
  mScripting->getFunctionTable(fname.c_str());
  lua_getfield(L, -1, LuaScripting::TBL_MD_FUN_LAST_EXEC);
  int lastExecTable = lua_gettop(L);

  lua_checkstack(L, LUAC_MAX_NUM_PARAMS + 2); // 2 = key/value pair.

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
  // If mStackPointer is at 1, then we can undo to the 'default' state.
  if (mStackPointer == 0)
  {
    throw LuaProvenanceInvalidUndo("Undo pointer at bottom of stack.");
  }

  int undoIndex         = mStackPointer - 1;
  UndoRedoItem undoItem = mUndoRedoStack[undoIndex];

  try
  {
    performUndoRedoOp(undoItem.function, undoItem.undoParams);
  }
  catch (LuaProvenanceInvalidUndoOrRedo& e)
  {
    throw LuaProvenanceInvalidUndo(e.what(), e.where(), e.lineno());
  }

  --mStackPointer;
}

//-----------------------------------------------------------------------------
void LuaProvenance::issueRedo()
{
  if (mStackPointer == mUndoRedoStack.size())
  {
    throw LuaProvenanceInvalidRedo("Redo pointer at top of stack.");
  }

  // The stack pointer is 1 based, this is the next element on the stack.
  int redoIndex = mStackPointer;
  UndoRedoItem redoItem = mUndoRedoStack[redoIndex];

  try
  {
    performUndoRedoOp(redoItem.function, redoItem.redoParams);
  }
  catch (LuaProvenanceInvalidUndoOrRedo& e)
  {
    throw LuaProvenanceInvalidRedo(e.what(), e.where(), e.lineno());
  }

  ++mStackPointer;
}

//-----------------------------------------------------------------------------
void LuaProvenance::performUndoRedoOp(const string& funcName,
                                      tr1::shared_ptr<LuaCFunAbstract> params)
{
  // Obtain function's table, then grab its metamethod __call.
  // Push parameters onto the stack after the __call method, and execute.
  lua_State* L = mScripting->getLUAState();
  int initStackTop = lua_gettop(L);
  mScripting->getFunctionTable(funcName);
  int funTable = lua_gettop(L);
  if (lua_isnil(L, -1))
  {
    throw LuaProvenanceInvalidUndoOrRedo("Function table does not exist.");
  }

  if (lua_getmetatable(L, -1))
  {
    // Push function onto the stack.
    lua_getfield(L, -1, "__call");

    if (lua_isnil(L, -1))
    {
      throw LuaProvenanceInvalidUndoOrRedo("Function has invalid function "
                                           "pointer.");
    }

    // Before we push the parameters, we need to push the function table.
    // (this is always the first parameter).
    lua_pushvalue(L, funTable);

    // Push parameters onto the stack.
    int paramStart = lua_gettop(L);
    params->pushParamsToStack(L);
    int numParams = lua_gettop(L) - paramStart;
    paramStart += 1;

    // Execute the call (ignore return values).
    // This will pop all parameters and the function off the stack.
    mUndoRedoProvenanceDisable = true;
    lua_call(L, numParams + 1, 0);      // The + 1 is for the function table.
    mUndoRedoProvenanceDisable = false;

    // Pop the metatable
    lua_pop(L, 1);

    // Last exec table parameters will match what we just executed.
    paramStart = lua_gettop(L);
    params->pushParamsToStack(L);
    numParams = lua_gettop(L) - paramStart;
    paramStart += 1;

    lua_getfield(L, funTable, LuaScripting::TBL_MD_FUN_LAST_EXEC);

    mScripting->copyParamsToTable(lua_gettop(L), paramStart, numParams);

    // Remove last exec table from the stack.
    lua_pop(L, 1);

    // Remove parameters from stack.
    lua_pop(L, numParams);
  }
  else
  {
    throw LuaProvenanceInvalidUndoOrRedo("Does not appear to be a valid "
                                         "function.");
  }

  // Pop the function table
  lua_pop(L, 1);

  assert(initStackTop == lua_gettop(L));
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

//-----------------------------------------------------------------------------
vector<string> LuaProvenance::getUndoStackDesc()
{
  // Print from the current stack pointer downwards.
  vector<string> ret;
  string undoVals;
  string redoVals;
  string result;
  for (int i = mStackPointer - 1; i >= 0; i--)
  {
    undoVals = mUndoRedoStack[i].undoParams->getFormattedParameterValues();
    redoVals = mUndoRedoStack[i].redoParams->getFormattedParameterValues();

    ostringstream os;
    const string& fun = mUndoRedoStack[i].function;
    os << fun << "(" << undoVals << ") -- " << fun << "(" << redoVals << ")";
    ret.push_back(os.str());
  }

  return ret;
}

//-----------------------------------------------------------------------------
std::vector<std::string> LuaProvenance::getRedoStackDesc()
{
  // Print from the current stack pointer downwards.
  vector<string> ret;
  string undoVals;
  string redoVals;
  string result;
  for (vector<string>::size_type i = mStackPointer; i < mUndoRedoStack.size(); i++)
  {
    undoVals = mUndoRedoStack[i].undoParams->getFormattedParameterValues();
    redoVals = mUndoRedoStack[i].redoParams->getFormattedParameterValues();

    ostringstream os;
    const string& fun = mUndoRedoStack[i].function;
    os << fun << "(" << redoVals << ") -- " << fun << "(" << undoVals << ")";
    ret.push_back(os.str());
  }

  return ret;
}

//-----------------------------------------------------------------------------
std::vector<std::string> LuaProvenance::getFullProvenanceDesc()
{
  return mProvenanceDescList;
}

//-----------------------------------------------------------------------------
void LuaProvenance::printUndoStack()
{
  mScripting->exec("log.info(''); log.info('Undo Stack (left is undo, "
                   "right redo):');");

  vector<string> undoStack = getUndoStackDesc();
  for (vector<string>::iterator it = undoStack.begin(); it != undoStack.end();
      ++it)
  {
    // We use cexec for a little bit more efficiency.
    mScripting->cexec("log.info", (*it));
  }
}

//-----------------------------------------------------------------------------
void LuaProvenance::printRedoStack()
{
  mScripting->exec("log.info(''); log.info('Redo Stack (left is redo, "
                   "right undo):');");

  vector<string> undoStack = getRedoStackDesc();
  for (vector<string>::iterator it = undoStack.begin(); it != undoStack.end();
      ++it)
  {
    mScripting->cexec("log.info", (*it));
  }
}

//-----------------------------------------------------------------------------
void LuaProvenance::printProvRecord()
{
  mScripting->exec("log.info(''); log.info('Provenance Record:');");

  vector<string> undoStack = getFullProvenanceDesc();
  for (vector<string>::iterator it = undoStack.begin(); it != undoStack.end();
      ++it)
  {
    mScripting->cexec("log.info", (*it));
  }
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

    LuaMemberReg mReg;
  };

  TEST(ProvenanceClassTests)
  {
    TEST_HEADER;

    tr1::shared_ptr<LuaScripting> sc(new LuaScripting());

    auto_ptr<A> a(new A(sc));

    a->mReg.registerFunction(a.get(), &A::set_i1, "set_i1", "", true);
    a->mReg.registerFunction(a.get(), &A::set_i2, "set_i2", "", true);
    a->mReg.registerFunction(a.get(), &A::get_i1, "get_i1", "", false);
    a->mReg.registerFunction(a.get(), &A::get_i2, "get_i2", "", false);

    a->mReg.registerFunction(a.get(), &A::set_f1, "set_f1", "", true);
    a->mReg.registerFunction(a.get(), &A::set_f2, "set_f2", "", true);
    a->mReg.registerFunction(a.get(), &A::get_f1, "get_f1", "", false);
    a->mReg.registerFunction(a.get(), &A::get_f2, "get_f2", "", false);

    a->mReg.registerFunction(a.get(), &A::set_s1, "set_s1", "", true);
    a->mReg.registerFunction(a.get(), &A::set_s2, "set_s2", "", true);
    a->mReg.registerFunction(a.get(), &A::get_s1, "get_s1", "", false);
    a->mReg.registerFunction(a.get(), &A::get_s2, "get_s2", "", false);

    // Use an unprotected call to grab the exception. Change all lua_pcalls
    // to lua_call.
    // TODO: Need to think about this problem in general, and how we are going
    // to deal with it. More than likely, we will not expose pcall to the end
    // user of this system.
    CHECK_THROW(sc->exec("provenance.redo()"), LuaProvenanceInvalidRedo);
    CHECK_THROW(sc->exec("provenance.undo()"), LuaProvenanceInvalidUndo);

    sc->exec("set_i1(1)");
    sc->exec("set_i2(10)");
    sc->exec("set_i1(2)");
    sc->exec("set_i1(3)");
    sc->exec("set_i2(20)");
    sc->exec("set_f1(2.3)");
    sc->exec("set_s1('T')");
    sc->exec("set_s1('Test')");
    sc->exec("set_s2('Test2')");
    sc->exec("set_f1(1.5)");
    sc->exec("set_i1(100)");
    sc->exec("set_i2(30)");
    sc->exec("set_f2(-5.3)");

    // Check final values.
    CHECK_EQUAL(a->i1, 100);
    CHECK_EQUAL(a->i2, 30);

    CHECK_CLOSE(a->f1, 1.5f, 0.001f);
    CHECK_CLOSE(a->f2, -5.3f, 0.001f);

    CHECK_EQUAL(a->s1.c_str(), "Test");
    CHECK_EQUAL(a->s2.c_str(), "Test2");

    CHECK_THROW(sc->exec("provenance.redo()"), LuaProvenanceInvalidRedo);

    // Begin issuing undo / redos
    sc->exec("provenance.undo()");
    CHECK_CLOSE(a->f2, 0.0f, 0.001f);
    sc->exec("provenance.redo()");
    CHECK_CLOSE(a->f2, -5.3f, 0.001f);
    sc->exec("provenance.undo()");
    CHECK_CLOSE(a->f2, 0.0f, 0.001f);
    sc->exec("provenance.undo()");
    CHECK_EQUAL(a->i2, 20);
    sc->exec("provenance.undo()");
    CHECK_EQUAL(a->i1, 3);
    sc->exec("provenance.redo()");
    CHECK_EQUAL(a->i1, 100);
    sc->exec("provenance.undo()");
    CHECK_EQUAL(a->i1, 3);
    sc->exec("provenance.undo()");
    CHECK_CLOSE(a->f1, 2.3f, 0.001f);
    sc->exec("provenance.undo()");
    CHECK_EQUAL(a->s2.c_str(), "");
    sc->exec("provenance.redo()");
    CHECK_EQUAL(a->s2.c_str(), "Test2");
    sc->exec("provenance.redo()");
    CHECK_CLOSE(a->f1, 1.5f, 0.001f);
    sc->exec("provenance.redo()");
    CHECK_EQUAL(a->i1, 100);
    sc->exec("provenance.undo()");
    CHECK_EQUAL(a->i1, 3);
    sc->exec("provenance.undo()");
    CHECK_CLOSE(a->f1, 2.3f, 0.001f);
    sc->exec("provenance.undo()");
    CHECK_EQUAL(a->s2.c_str(), "");
    sc->exec("provenance.undo()");
    CHECK_EQUAL(a->s1.c_str(), "T");
    sc->exec("provenance.undo()");
    CHECK_EQUAL(a->s1.c_str(), "");

    sc->exec("provenance.undo()");
    CHECK_CLOSE(a->f1, 0.0f, 0.001f);
    sc->exec("provenance.undo()");
    CHECK_EQUAL(a->i2, 10);
    sc->exec("provenance.undo()");
    CHECK_EQUAL(a->i1, 2);
    sc->exec("provenance.undo()");
    CHECK_EQUAL(a->i1, 1);

    sc->exec("provenance.undo()");
    CHECK_EQUAL(a->i2, 0);
    sc->exec("provenance.undo()");
    CHECK_EQUAL(a->i1, 0);

    // This invalid undo should not destroy state.
    CHECK_THROW(sc->exec("provenance.undo()"), LuaProvenanceInvalidUndo);

    // Check to make sure default values are present.
    CHECK_EQUAL(a->i1, 0);
    CHECK_EQUAL(a->i2, 0);

    CHECK_CLOSE(a->f1, 0.0f, 0.001f);
    CHECK_CLOSE(a->f2, 0.0f, 0.001f);

    CHECK_EQUAL(a->s1.c_str(), "");
    CHECK_EQUAL(a->s2.c_str(), "");

    // Check redoing EVERYTHING.
    sc->exec("provenance.redo()");
    CHECK_EQUAL(a->i1, 1);
    sc->exec("provenance.redo()");
    CHECK_EQUAL(a->i2, 10);
    sc->exec("provenance.redo()");
    CHECK_EQUAL(a->i1, 2);
    sc->exec("provenance.redo()");
    CHECK_EQUAL(a->i1, 3);
    sc->exec("provenance.redo()");
    CHECK_EQUAL(a->i2, 20);
    sc->exec("provenance.redo()");
    CHECK_CLOSE(a->f1, 2.3f, 0.001f);
    sc->exec("provenance.redo()");
    CHECK_EQUAL(a->s1, "T");
    sc->exec("provenance.redo()");
    CHECK_EQUAL(a->s1, "Test");
    sc->exec("provenance.redo()");
    CHECK_EQUAL(a->s2, "Test2");
    sc->exec("provenance.redo()");
    CHECK_CLOSE(a->f1, 1.5f, 0.001f);
    sc->exec("provenance.redo()");
    CHECK_EQUAL(a->i1, 100);
    sc->exec("provenance.redo()");
    CHECK_EQUAL(a->i2, 30);
    sc->exec("provenance.redo()");
    CHECK_CLOSE(a->f2, -5.3f, 0.001f);

    CHECK_THROW(sc->exec("provenance.redo()"), LuaProvenanceInvalidRedo);

    // Check final values again.
    CHECK_EQUAL(a->i1, 100);
    CHECK_EQUAL(a->i2, 30);

    CHECK_CLOSE(a->f1, 1.5f, 0.001f);
    CHECK_CLOSE(a->f2, -5.3f, 0.001f);

    CHECK_EQUAL(a->s1.c_str(), "Test");
    CHECK_EQUAL(a->s2.c_str(), "Test2");

    // Check lopping off sections of the redo buffer.
    sc->exec("provenance.undo()");
    sc->exec("provenance.undo()");
    sc->exec("provenance.undo()");
    sc->exec("set_i1(42)");
    CHECK_EQUAL(42, a->i1);

    CHECK_THROW(sc->exec("provenance.redo()"), LuaProvenanceInvalidRedo);

    sc->exec("provenance.undo()");
    sc->exec("provenance.undo()");
    sc->exec("provenance.redo()");
    sc->exec("set_i1(45)");

    CHECK_THROW(sc->exec("provenance.redo()"), LuaProvenanceInvalidRedo);

    // Uncomment to view all provenance.
    //sc->exec("provenance.logProvenanceRecord()");
  }

  static int i1     = 0;
  static string s1  = "nop";
  static bool b1    = false;

  static void set_i1(int a)     {i1 = a;}
  static void set_s1(string s)  {s1 = s;}
  static void set_b1(bool a)    {b1 = a;}

  TEST(ProvenanceStaticTests)
  {
    // We don't need to test the provenance functionality, just that it is
    // hooked up correctly. The above TEST tests the provenance system fairly
    // thoroughly.
    TEST_HEADER;

    auto_ptr<LuaScripting> sc(new LuaScripting());

    sc->registerFunction(&set_i1, "set_i1", "", true);
    sc->registerFunction(&set_s1, "set_s1", "", true);
    sc->setDefaults("set_s1", "nop");
    sc->registerFunction(&set_b1, "set_b1", "", true);

    sc->exec("set_i1(23)");
    sc->exec("set_s1(\"Test String\")");
    sc->exec("set_b1(true)");

    CHECK_EQUAL(23, i1);
    CHECK_EQUAL("Test String", s1.c_str());
    CHECK_EQUAL(true, b1);

    sc->exec("provenance.undo()");
    CHECK_EQUAL(false, b1);

    // TODO: This should really be 'nop'. Fix it after we add default resets.
    sc->exec("provenance.undo()");
    CHECK_EQUAL("nop", s1.c_str());

    sc->exec("provenance.redo()");
    CHECK_EQUAL("Test String", s1.c_str());

    // TODO: This should really be 'nop'. Fix it after we add default resets.
    sc->exec("provenance.undo()");
    CHECK_EQUAL("nop", s1.c_str());

    sc->exec("provenance.undo()");
    CHECK_EQUAL(0, i1);
  }

}


}
