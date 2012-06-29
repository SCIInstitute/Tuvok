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
          Not reentrant (logging and command depth).
 */

#ifndef EXTERNAL_UNIT_TESTING

#include "Controller/Controller.h"
#include "3rdParty/LUA/lua.hpp"

#else

#include <assert.h>
#include "utestCommon.h"

#endif

#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>

#include "LuaError.h"
#include "LuaFunBinding.h"
#include "LuaScripting.h"
#include "LuaProvenance.h"
#include "LuaMemberReg.h"

using namespace std;

#define DEFAULT_UNDOREDO_BUFFER_SIZE  (50)
#define DEFAULT_PROVENANCE_BUFFER_SIZE  (150)

namespace tuvok
{

//-----------------------------------------------------------------------------
LuaProvenance::LuaProvenance(LuaScripting* scripting)
: mEnabled(true)
, mTemporarilyDisabled(false)
, mUndoingInstanceDel(false)
, mStackPointer(0)
, mScripting(scripting)
, mMemberReg(scripting)
, mLoggingProvenance(false)
, mDoProvReenterException(true)
, mProvenanceDescLogEnabled(false)  // Disable the provenance log (performance)
, mUndoRedoProvenanceDisable(false)
, mCommandDepth(0)
{
  mUndoRedoStack.reserve(DEFAULT_PROVENANCE_BUFFER_SIZE);
  mProvenanceDescList.reserve(DEFAULT_PROVENANCE_BUFFER_SIZE);
}

//-----------------------------------------------------------------------------
LuaProvenance::~LuaProvenance()
{
  // We purposefully do NOT unregister our Lua functions.
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
                              "Undoes last command.",
                              false);
  mMemberReg.registerFunction(this, &LuaProvenance::issueRedo,
                              "provenance.redo",
                              "Redoes the last undo.",
                              false);
  mMemberReg.registerFunction(this, &LuaProvenance::setEnabled,
                              "provenance.enable",
                              "Enable/Disable provenance. This is not an "
                              "undoable action and will clear your provenance "
                              "history if disabled.",
                              false);
  mMemberReg.registerFunction(this, &LuaProvenance::enableLogAll,
                              "provenance.enableProvLog",
                              "Enables/Disables provenance log (def: false).",
                              false);
  mMemberReg.registerFunction(this, &LuaProvenance::clearProvenance,
                              "provenance.clear",
                              "Clears all provenance and undo/redo stacks. "
                              "This is not an undo-able action.",
                              false);
  mMemberReg.registerFunction(this, &LuaProvenance::enableProvReentryEx,
                              "provenance.enableReentryException",
                              "Enables/Disables the provenance reentry "
                              "exception.",
                              true);
  mMemberReg.registerFunction(this, &LuaProvenance::printUndoStack,
                              "provenance.logUndoStack",
                              "Prints the contents of the undo stack "
                              "to 'log.info'.",
                              false);
  mMemberReg.registerFunction(this, &LuaProvenance::printRedoStack,
                              "provenance.logRedoStack",
                              "Prints the contents of the redo stack "
                              "to 'log.info'.",
                              false);
  mMemberReg.registerFunction(this, &LuaProvenance::printProvRecordToFile,
                              "provenance.logProvRecord_toFile",
                              "Prints the entire provenance record "
                              "to 'log.info'.",
                              false);
  mMemberReg.registerFunction(this, &LuaProvenance::printProvRecord,
                              "provenance.logProvRecord_toConsole",
                              "Prints the entire provenance record "
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
  if (mProvenanceDescList.size() <= 0)
    return;

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

  if (mTemporarilyDisabled || mEnabled == false)
    return;

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

    os << fname << "(" << provParams << ")"
       << " - depth:" << mCommandDepth;
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
  int stackDiff = static_cast<int>(mUndoRedoStack.size()) - mStackPointer;
  for (int i = 0; i < stackDiff; i++)
  {
    mUndoRedoStack.pop_back();
  }
  assert(mUndoRedoStack.size() ==
         static_cast<URStackType::size_type>(mStackPointer));

  // Gather last execution parameters for inclusion in the undo stack.
  lua_State* L = mScripting->getLuaState();
  LuaStackRAII _a = LuaStackRAII(L, 0);
  if (mScripting->getFunctionTable(fname.c_str()) == false)
    throw LuaError("Provenance unable to find function");
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

  if (mCommandDepth == 0)
  {
    mUndoRedoStack.push_back(UndoRedoItem(fname, emptyParams, funParams));
    ++mStackPointer;
  }
  else
  {
    assert(mUndoRedoStack.size() > 0);
    // Push a child (child on the top of the stack -- we know there must be an
    // entry on the top of the stack because our depth is greater than 0).
    mUndoRedoStack.back().addChildItem(
        UndoRedoItem(fname, emptyParams, funParams));
  }

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
}

//-----------------------------------------------------------------------------
int LuaProvenance::bruteRerollDetermineUndos(int undoIndex)
{
  int numUndos = 0;
  vector<int> unresolved; // Unresolved instances.
  bool resolved = false;

  while (undoIndex >= 0)
  {
    UndoRedoItem undoItem = mUndoRedoStack[undoIndex];
    ++numUndos; // Always want to undo once more than resolved location.

    if (undoItem.instDeletions.get())
    {
      // It doesn't matter where we copy (front or back).
      // Don't need to worry about duplicates. We can't delete classes
      // multiple times.
      unresolved.reserve( unresolved.size() + undoItem.instDeletions->size());
      unresolved.insert( unresolved.end(),
                         undoItem.instDeletions->begin(),
                         undoItem.instDeletions->end());
      sort(unresolved.begin(), unresolved.end());

      // Sanity check that there are no duplicates, and that the list is
      // sequential (no gaps). These must be true due to non-duplication
      int last = *unresolved.begin();
      for (vector<int>::iterator it = unresolved.begin() + 1;
          it != unresolved.end(); ++it)
      {
        if (last == *it)
        {
          throw LuaProvenanceFailedUndo("Duplicate global IDs");
        }
        last = *it;
      }
    }
    if (undoItem.instCreations.get())
    {
      // instDeletions and instCreations are always sorted.
      vector<int> diff; // Could make this more efficient if stored above.
      diff.resize(max(unresolved.size(), undoItem.instCreations->size()), 0);
      vector<int>::iterator it = set_difference(
               unresolved.begin(), unresolved.end(),
               undoItem.instCreations->begin(),
               undoItem.instCreations->end(),
               diff.begin()); // Could just use back_inserter...

      unresolved.clear();
      vector<int>::size_type resultSize = it - diff.begin();

      if (resultSize > 0)
      {
        unresolved.resize(resultSize, 0);
        std::copy(diff.begin(), it, unresolved.begin());
      }
    }
    if (unresolved.size() == 0)
    {
      resolved = true;
      break;
    }
    --undoIndex;
  }

  if (resolved == false)
  {
    throw LuaProvenanceFailedUndo("Not enough information in undo buffer "
                                  "to undo specified operation.");
  }

  return numUndos;
}

//-----------------------------------------------------------------------------
void LuaProvenance::issueUndo()
{
  if (mEnabled == false)
    return;

  // If mStackPointer is at 1, then we can undo to the 'default' state.
  if (mStackPointer == 0)
  {
    throw LuaProvenanceInvalidUndo("Undo pointer at bottom of stack.");
  }

  int undoIndex         = mStackPointer - 1;
  UndoRedoItem undoItem = mUndoRedoStack[undoIndex];

  int numUndos = 1;

  // Check to see if this step deleted any instances. If it did, we will have
  // to check how far back we need to undo. Then force the entire system
  // to undo to that point.
  if (undoItem.instDeletions.get() != 0)
  {
    // Detect how far back we need to roll.
    mUndoingInstanceDel = true;
    numUndos = bruteRerollDetermineUndos(undoIndex);
  }

  // This is the brute reroll.
  // In order to deal with class deletions, we roll back to when they
  // were created and redo everything.
  // There is a more efficient way of doing this, but this is
  // guaranteed to work without any special code. It serves as a good
  // baseline algorithm.
  for (int i = 0; i < numUndos; i++)
  {
    issueUndoInternal();
  }

  // Redo back to directly before undo (the vast majority of the time -- the
  // times we don't have any instance deletions -- we never enter this loop).
  // NOTE: We don't need to set the global ID instance range here! That is
  // handled automatically inside issueRedo.
  for (int i = 0; i < numUndos - 1; i++)
  {
    issueRedo();
  }

  mUndoingInstanceDel = false;
}

//-----------------------------------------------------------------------------
void LuaProvenance::issueUndoInternal()
{
  if (mStackPointer == 0)
  {
    throw LuaProvenanceInvalidUndo("Undo pointer at bottom of stack.");
  }

  int undoIndex         = mStackPointer - 1;
  UndoRedoItem undoItem = mUndoRedoStack[undoIndex];

  try
  {
    performUndoRedoOp(undoItem.function, undoItem.undoParams, true);
  }
  catch (LuaProvenanceInvalidUndoOrRedo& e)
  {
    throw LuaProvenanceInvalidUndo(e.what(), e.where(), e.lineno());
  }

  if (undoItem.childItems != NULL)
  {
    // Iterate through all of the children, and undo those as well.
    // Note: Notice, we are undoing the parent first, then all of the children.
    //       This constitutes a reversal of the function calls from redo.

    for (std::vector<UndoRedoItem>::iterator it = undoItem.childItems->begin();
         it != undoItem.childItems->end(); it++)
    {
      try
      {
        performUndoRedoOp(it->function, it->undoParams, true);
      }
      catch (LuaProvenanceInvalidUndoOrRedo& e)
      {
        throw LuaProvenanceInvalidUndo(e.what(), e.where(), e.lineno());
      }
    }
  }

  // Delete any instances that were created at the end of the redo chain.
  if (undoItem.instCreations.get() != 0)
  {
    // Manually call the delete function (no provenance).
    mUndoRedoProvenanceDisable = true;
    for (vector<int>::iterator it = undoItem.instCreations->begin();
        it != undoItem.instCreations->end(); ++it)
    {
      mScripting->deleteLuaClassInstance(LuaClassInstance(*it));
    }
    mUndoRedoProvenanceDisable = false;
  }

  --mStackPointer;
}

//-----------------------------------------------------------------------------
void LuaProvenance::issueRedo()
{
  if (mEnabled == false)
    return;

  if (static_cast<URStackType::size_type>(mStackPointer) ==
      mUndoRedoStack.size())
  {
    throw LuaProvenanceInvalidRedo("Redo pointer at top of stack.");
  }

  // The stack pointer is 1 based, this is the next element on the stack.
  int redoIndex = mStackPointer;
  UndoRedoItem redoItem = mUndoRedoStack[redoIndex];

  // Upon recreation of instances, we need to ensure our instances have
  // the same ID's. This code block will ensure that.
  if (redoItem.instCreations.get() != 0)
  {
    // instCreations is always ordered.
    int lowestID = redoItem.instCreations->front();
    int highestID = redoItem.instCreations->back();
    mScripting->setNextTempClassInstRange(lowestID, highestID);
  }

  try
  {
    performUndoRedoOp(redoItem.function, redoItem.redoParams, false);
  }
  catch (LuaProvenanceInvalidUndoOrRedo& e)
  {
    throw LuaProvenanceInvalidRedo(e.what(), e.where(), e.lineno());
  }

  // Issue redo to all children ONLY if flag is set (the only function that
  // sets the flag is setLastItemAsAlsoRedoChildren).
  if (redoItem.alsoRedoChildren)
  {
    if (redoItem.childItems != NULL)
    {
      // Iterate through all of the children, and undo those as well.
      // Note: Notice, we are undoing the parent first, then all of the children.
      //       This constitutes a reversal of the function calls from redo.

      for (std::vector<UndoRedoItem>::iterator it=redoItem.childItems->begin();
           it != redoItem.childItems->end(); it++)
      {
        try
        {
          performUndoRedoOp(it->function, it->redoParams, false);
        }
        catch (LuaProvenanceInvalidUndoOrRedo& e)
        {
          throw LuaProvenanceInvalidRedo(e.what(), e.where(), e.lineno());
        }
      }
    }
  }

  // Notice, we ignore any child undo/redo items. They exist solely to help
  // undo reset the program state when a composited function is undone.

  ++mStackPointer;
}

//-----------------------------------------------------------------------------
void LuaProvenance::performUndoRedoOp(const string& funcName,
                                      tr1::shared_ptr<LuaCFunAbstract> params,
                                      bool isUndo)
{
  // Obtain function's table, then grab its metamethod __call.
  // Push parameters onto the stack after the __call method, and execute.
  lua_State* L = mScripting->getLuaState();
  LuaStackRAII _a = LuaStackRAII(L, 0);

  if (mScripting->getFunctionTable(funcName) == false)
  {
    if (mUndoingInstanceDel)
    {
      // Failed to find the function. Since we are undoing, we assume this was
      // due to an instance call that no longer exists. We ignore it for now.
      return;
    }
    else
    {
      throw LuaProvenanceInvalidUndoOrRedo("Function table does not exist.");
    }
  }
  int funTable = lua_gettop(L);
  if (lua_isnil(L, -1))
  {
    throw LuaProvenanceInvalidUndoOrRedo("Function table does not exist.");
  }

  if (lua_getmetatable(L, -1))
  {
    // Push function onto the stack.
    bool hasHook = false;

    if (isUndo)
    {
      // Check to see if we should not execute any undo.
      lua_getfield(L, funTable, LuaScripting::TBL_MD_NULL_UNDO);
      if (!(lua_isnil(L, -1) == 0 && lua_toboolean(L, -1) == 1))
      {
        lua_pop(L, 1);

        // Check for an undo hook
        lua_getfield(L, funTable, LuaScripting::TBL_MD_UNDO_FUNC);
        if (lua_isnil(L, -1) == 0)
          hasHook = true;
        else
          lua_pop(L, 1);
      }
      else
      {
        lua_pop(L, 1);
        luaL_dostring(L, "return function(...) end");  // null vararg function
        hasHook = true;
      }
    }
    else
    {
      // Check for redo hook
      lua_getfield(L, funTable, LuaScripting::TBL_MD_NULL_REDO);
      if (!(lua_isnil(L, -1) == 0 && lua_toboolean(L, -1) == 1))
      {
        lua_pop(L, 1);

        lua_getfield(L, funTable, LuaScripting::TBL_MD_REDO_FUNC);
        if (lua_isnil(L, -1) == 0)
          hasHook = true;
        else
          lua_pop(L, 1);
      }
      else
      {
        lua_pop(L, 1);
        luaL_dostring(L, "return function(...) end");  // null vararg function
        hasHook = true;
      }
    }

    // If we don't have a hook, just use the function itself.
    if (hasHook == false)
    {
      lua_getfield(L, -1, "__call");
      if (lua_isnil(L, -1))
      {
        throw LuaProvenanceInvalidUndoOrRedo("Function has invalid function "
                                             "pointer.");
      }

      // Before we push the parameters, we need to push the function table.
      // (this is always the first parameter for non-hook functions).
      lua_pushvalue(L, funTable);
    }

    // Push parameters onto the stack.
    int paramStart = lua_gettop(L);
    params->pushParamsToStack(L);
    int numParams = lua_gettop(L) - paramStart;
    paramStart += 1;

    // Determine the number of parameters. There is one extra parameter for
    // functions who are not hooks (the function table itself).
    int functionParams = numParams;
    if (hasHook == false) functionParams++;

    // Execute the call (ignore return values).
    // This will pop all parameters and the function off the stack.
    mUndoRedoProvenanceDisable = true;
    lua_call(L, functionParams, 0);
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
}



//-----------------------------------------------------------------------------
void LuaProvenance::clearProvenance()
{
  mUndoRedoStack.clear();
  mStackPointer = 0;

  // Clear out last exec for ALL functions. This will clean up any dangling
  // shared pointers.
  mScripting->clearAllLastExecTables();
  mScripting->exec("collectgarbage()"); // Force a gc cycle to clean up all
                                        // shared_ptrs.
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
vector<string> LuaProvenance::getRedoStackDesc()
{
  // Print from the current stack pointer downwards.
  vector<string> ret;
  string undoVals;
  string redoVals;
  string result;
  for (vector<string>::size_type i = mStackPointer; i < mUndoRedoStack.size();
      i++)
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
void LuaProvenance::beginCommand()
{
  if (mEnabled == false)
    return;

	++mCommandDepth;
}

//-----------------------------------------------------------------------------
void LuaProvenance::endCommand()
{
  if (mEnabled == false)
    return;

	--mCommandDepth;
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

  if (mEnabled == false)
    mScripting->exec("log.info('** Provenance disabled.')");

  vector<string> undoStack = getUndoStackDesc();
  for (vector<string>::reverse_iterator it = undoStack.rbegin();
      it != undoStack.rend(); ++it)
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

  if (mEnabled == false)
    mScripting->exec("log.info('** Provenance disabled.')");

  vector<string> undoStack = getRedoStackDesc();
  for (vector<string>::reverse_iterator it = undoStack.rbegin();
      it != undoStack.rend(); ++it)
  {
    mScripting->cexec("log.info", (*it));
  }
}

//-----------------------------------------------------------------------------
void LuaProvenance::printProvRecord()
{
  mScripting->exec("log.info(''); log.info('Provenance Record:');");

  if (mEnabled == false)
    mScripting->exec("log.info('** Provenance disabled.')");

  vector<string> undoStack = getFullProvenanceDesc();
  for (vector<string>::iterator it = undoStack.begin(); it != undoStack.end();
      ++it)
  {
    mScripting->cexec("log.info", (*it));
  }
}

//-----------------------------------------------------------------------------
void LuaProvenance::printProvRecordToFile(const std::string& file)
{
  ofstream f;
  f.open(file.c_str());
  f << "Provenance Record:" << endl;

  if (mEnabled == false)
  {
    f << "** Provenance disabled." << endl;
  }

  vector<string> undoStack = getFullProvenanceDesc();
  for (vector<string>::iterator it = undoStack.begin(); it != undoStack.end();
      ++it)
  {
    f << (*it) << endl;
  }

  f.close();
}

//-----------------------------------------------------------------------------
void LuaProvenance::setDisableProvTemporarily(bool disable)
{
  mTemporarilyDisabled = disable;
}

//-----------------------------------------------------------------------------
void LuaProvenance::addDeletedInstanceToLastURItem(int globalID)
{
  if (mUndoRedoProvenanceDisable == true || mEnabled == false)
    return;

  // The stack pointer might be zero if we are being deleted at program
  // termination.
  if (mStackPointer >= 1)
  {
    mUndoRedoStack.back().addInstDeletion(globalID);
  }
}

//-----------------------------------------------------------------------------
void LuaProvenance::addCreatedInstanceToLastURItem(int globalID)
{
  if (mUndoRedoProvenanceDisable == true || mEnabled == false)
    return;

  assert(mStackPointer >= 1);
  mUndoRedoStack.back().addInstCreation(globalID);
}

//-----------------------------------------------------------------------------
bool LuaProvenance::testLastURItemHasDeletedItems(const vector<int>& delItems)
  const
{
  if (mUndoRedoStack.size() == 0)
    return false;

  UndoRedoItem ur = mUndoRedoStack.back();

  if (ur.instDeletions.get() == 0)
  {
    return delItems.size() == 0 ? true : false;
  }

  for (vector<int>::const_iterator it = delItems.begin(); it != delItems.end();
      ++it)
  {
    if (find(ur.instDeletions->begin(), ur.instDeletions->end(), *it)
        == ur.instDeletions->end())
    {
      return false;
    }
  }

  return true;
}

//-----------------------------------------------------------------------------
bool LuaProvenance::testLastURItemHasCreatedItems(const vector<int>&
                                                  createdItems) const
{
  if (mUndoRedoStack.size() == 0)
    return false;

  UndoRedoItem ur = mUndoRedoStack.back();

  if (ur.instCreations.get() == 0)
  {
    return createdItems.size() == 0 ? true : false;
  }

  for (vector<int>::const_iterator it = createdItems.begin();
      it != createdItems.end(); ++it)
  {
    if (find(ur.instCreations->begin(), ur.instCreations->end(), *it)
        == ur.instCreations->end())
    {
      return false;
    }
  }

  return true;
}

//-----------------------------------------------------------------------------
void LuaProvenance::setLastURItemAlsoRedoChildren()
{
  if (mUndoRedoStack.size() > 0)
  {
    mUndoRedoStack.back().alsoRedoChildren = true;
  }
}

//==============================================================================
//
// UNIT TESTING
//
//==============================================================================

#ifdef EXTERNAL_UNIT_TESTING

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

    sc->setExpectedExceptionFlag(true);
    CHECK_THROW(sc->exec("provenance.redo()"), LuaProvenanceInvalidRedo);
    CHECK_THROW(sc->exec("provenance.undo()"), LuaProvenanceInvalidUndo);
    sc->setExpectedExceptionFlag(false);

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

    sc->setExpectedExceptionFlag(true);
    CHECK_THROW(sc->exec("provenance.redo()"), LuaProvenanceInvalidRedo);
    sc->setExpectedExceptionFlag(false);

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
    sc->setExpectedExceptionFlag(true);
    CHECK_THROW(sc->exec("provenance.undo()"), LuaProvenanceInvalidUndo);
    sc->setExpectedExceptionFlag(false);

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

    sc->setExpectedExceptionFlag(true);
    CHECK_THROW(sc->exec("provenance.redo()"), LuaProvenanceInvalidRedo);
    sc->setExpectedExceptionFlag(false);

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

    sc->setExpectedExceptionFlag(true);
    CHECK_THROW(sc->exec("provenance.redo()"), LuaProvenanceInvalidRedo);
    sc->setExpectedExceptionFlag(false);

    sc->exec("provenance.undo()");
    sc->exec("provenance.undo()");
    sc->exec("provenance.redo()");
    sc->exec("set_i1(45)");

    sc->setExpectedExceptionFlag(true);
    CHECK_THROW(sc->exec("provenance.redo()"), LuaProvenanceInvalidRedo);
    sc->setExpectedExceptionFlag(false);

    // Uncomment to view all provenance.
    //sc->exec("provenance.logProvRecord()");
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
    sc->setDefaults("set_s1", "nop", false);
    sc->registerFunction(&set_b1, "set_b1", "", true);

    sc->exec("set_i1(23)");
    sc->exec("set_s1('Test String')");
    sc->exec("set_b1(true)");

    CHECK_EQUAL(23, i1);
    CHECK_EQUAL("Test String", s1.c_str());
    CHECK_EQUAL(true, b1);

    sc->exec("provenance.undo()");
    CHECK_EQUAL(false, b1);

    sc->exec("provenance.undo()");
    CHECK_EQUAL("nop", s1.c_str());

    sc->exec("provenance.redo()");
    CHECK_EQUAL("Test String", s1.c_str());

    sc->exec("provenance.undo()");
    CHECK_EQUAL("nop", s1.c_str());

    sc->exec("provenance.undo()");
    CHECK_EQUAL(0, i1);
  }

  LuaScripting* sc = NULL;

  static void set_i1_s1_b1(int a, string b, bool c)
  {
    // This is roundabout, but the whole idea is to test composited lua
    // provenance enabled functions.
    {
      ostringstream os;
      os << "set_i1(" << a << ")";
      sc->exec(os.str());
    }

    {
      ostringstream os;
      os << "set_s1('" << b << "')";
      sc->exec(os.str());
    }

    {
      ostringstream os;
      if (c)    os << "set_b1(true)";
      else      os << "set_b1(false)";
      sc->exec(os.str());
    }
  }

  TEST(ProvenanceCompositeSingle)
  {
    TEST_HEADER;

    sc = new LuaScripting();

    sc->registerFunction(&set_i1, "set_i1", "", true);
    sc->registerFunction(&set_s1, "set_s1", "", true);
    sc->setDefaults("set_s1", "nop", false);
    sc->registerFunction(&set_b1, "set_b1", "", true);
    sc->registerFunction(&set_i1_s1_b1, "setAll", "", true);

    sc->exec("set_i1(23)");
    sc->exec("set_s1('Test String')");
    sc->exec("set_b1(true)");

    CHECK_EQUAL(23, i1);
    CHECK_EQUAL("Test String", s1.c_str());
    CHECK_EQUAL(true, b1);

    sc->exec("setAll(78, 'Str Test', false)");

    CHECK_EQUAL(78, i1);
    CHECK_EQUAL("Str Test", s1.c_str());
    CHECK_EQUAL(false, b1);

    sc->exec("provenance.undo()");

    // Check to make sure i1, s1, and b1 do not have default values.
    // (ensure composited functions implement undo correctly).
    CHECK_EQUAL(23, i1);
    CHECK_EQUAL("Test String", s1.c_str());
    CHECK_EQUAL(true, b1);

    delete sc;
  }

  static int i2     = 0;
  static string s2  = "nop";
  static bool b2    = false;

  static void set_i2(int a)     {i2 = a;}
  static void set_s2(string s)  {s2 = s;}
  static void set_b2(bool a)    {b2 = a;}

  static void dup_s2(string b)
  {
    {
      ostringstream os;
      os << "set_s2('" << b << "')";
      sc->exec(os.str());
    }
  }

  static void setMultPrelude(int a, string b)
  {
    {
      ostringstream os;
      os << "set_i2(" << a << ")";
      sc->exec(os.str());
    }

    {
      ostringstream os;
      os << "dup_s2('" << b << "')";
      sc->exec(os.str());
    }
  }

  static void setMult(int a, string b, bool c)
  {
    {
      ostringstream os;
      os << "setAll_1(" << a << ",'" << b << "'," << boolalpha << c << ")";
      sc->exec(os.str());
    }

    {
      ostringstream os;
      os << "setMult_2(" << a << ",'" << b << "')";
      sc->exec(os.str());
    }

    {
      ostringstream os;
      os << "set_b2(" << boolalpha << c << ")";
      sc->exec(os.str());
    }
  }

  TEST(ProvenanceCompositeMultiple)
  {
    TEST_HEADER;

    // Similar to ProvenanceSingleCommandDepth, but we want to test
    // composited functions of composited functions.

    sc = new LuaScripting();

    sc->registerFunction(&set_i1, "set_i1", "", true);
    sc->registerFunction(&set_s1, "set_s1", "", true);
    sc->registerFunction(&set_b1, "set_b1", "", true);
    sc->registerFunction(&set_i1_s1_b1, "setAll_1", "", true);

    sc->registerFunction(&set_i2, "set_i2", "", true);
    sc->registerFunction(&set_s2, "set_s2", "", true);
    sc->registerFunction(&dup_s2, "dup_s2", "", true);
    sc->registerFunction(&set_b2, "set_b2", "", true);
    sc->registerFunction(&setMultPrelude, "setMult_2", "", true);
    sc->registerFunction(&setMult, "setAll", "", true);

    sc->exec("set_i1(23)");
    sc->exec("set_s1('Test String')");
    sc->exec("set_b1(true)");

    CHECK_EQUAL(23, i1);
    CHECK_EQUAL("Test String", s1.c_str());
    CHECK_EQUAL(true, b1);

    sc->exec("set_i2(46)");
    sc->exec("set_s2('2nd string')");
    sc->exec("set_b2(true)");

    CHECK_EQUAL(46, i2);
    CHECK_EQUAL("2nd string", s2.c_str());
    CHECK_EQUAL(true, b2);

    sc->exec("setAll(78, 'Str Test', false)");

    CHECK_EQUAL(78, i1);
    CHECK_EQUAL("Str Test", s1.c_str());
    CHECK_EQUAL(false, b1);

    CHECK_EQUAL(78, i2);
    CHECK_EQUAL("Str Test", s2.c_str());
    CHECK_EQUAL(false, b2);

    sc->exec("provenance.undo()");

    // Check to make sure i1, s1, and b1 do not have default values.
    // (ensure composited functions implement undo correctly).
    CHECK_EQUAL(23, i1);
    CHECK_EQUAL("Test String", s1.c_str());
    CHECK_EQUAL(true, b1);

    CHECK_EQUAL(46, i2);
    CHECK_EQUAL("2nd string", s2.c_str());
    CHECK_EQUAL(true, b2);

    delete sc;

  }

  TEST(ProvenanceNullUndoRedo)
  {
    TEST_HEADER;

    auto_ptr<LuaScripting> sc(new LuaScripting());

    sc->registerFunction(&set_i1, "set_i1", "", true);
    sc->setNullUndoFun("set_i1");
    sc->registerFunction(&set_s1, "set_s1", "", true);
    sc->setNullUndoFun("set_s1");
    sc->registerFunction(&set_b1, "set_b1", "", true);
    sc->setNullRedoFun("set_b1");

    sc->exec("set_i1(23)");
    sc->exec("set_s1('Test String')");
    sc->exec("set_b1(true)");

    CHECK_EQUAL(23, i1);
    CHECK_EQUAL("Test String", s1.c_str());
    CHECK_EQUAL(true, b1);

    sc->exec("provenance.undo()");
    CHECK_EQUAL(false, b1);

    // Null undo function.
    sc->exec("provenance.undo()");
    CHECK_EQUAL("Test String", s1.c_str());

    sc->exec("provenance.redo()");
    CHECK_EQUAL("Test String", s1.c_str());

    sc->exec("provenance.undo()");
    CHECK_EQUAL("Test String", s1.c_str());

    // Null undo function
    sc->exec("provenance.undo()");
    CHECK_EQUAL(23, i1);

    sc->exec("provenance.redo()");
    sc->exec("provenance.redo()");

    // Null redo function
    sc->exec("provenance.redo()");
    CHECK_EQUAL(false, b1);
  }

  TEST(ProvenanceDisabling)
  {
    TEST_HEADER;

    // Test whether the provenance system can be properly disabled.
  }

}

#endif

}
