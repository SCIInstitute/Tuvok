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

#ifndef TUVOK_LUAPROVENANCE_H_
#define TUVOK_LUAPROVENANCE_H_

#include "LuaMemberRegUnsafe.h"

namespace tuvok
{

class LuaScripting;

// TODO: Implement MAX size for undo / redo buffer. Think circular buffer, or
//       just std::vector::erase for simplicity.

class LuaProvenance
{
public:
  // This class is made for compositing inside LuaScripting, hence the pointer.
  LuaProvenance(LuaScripting* scripting);
  ~LuaProvenance();

  bool isEnabled() const;
  void setEnabled(bool enabled);

  /// Enable/Disable provenance logs of all commands.
  void enableLogAll(bool enabled);

  /// Logs the execution of a function.
  /// \param  function            Name of the function that executed.
  /// \param  undoRedoStackExempt True if no entry should be genereated inside
  ///                             the undo/redo stacks.
  /// \param  funParams           The parameters used when executing function.
  /// \param  emptyParams         The same type as funParams, but initialized
  ///                             to defaults.
  void logExecution(const std::string& function,
                    bool undoRedoStackExempt,
                    std::tr1::shared_ptr<LuaCFunAbstract> funParams,
                    std::tr1::shared_ptr<LuaCFunAbstract> emptyParams);

  // Modifies the last provenance log.
  void ammendLastProvLog(const std::string& ammend);

  /// Performs an undo.
  void issueUndo();

  /// Performs a redo.
  void issueRedo();

  /// Registers provenance functions with LUA.
  /// These functions are NEVER deregistered and persist for the lifetime
  /// of the associated LuaScripting system.
  void registerLuaProvenanceFunctions();

  /// Clears all provenance and the undo/redo stack.
  void clearProvenance();

  /// Enable / disable the provenance reentry exception.
  /// Disabling this will not make provenance reentrant. Instead it will not
  /// throw an exception, and it return from provenance function immediately
  /// without performing any work.
  void enableProvReentryEx(bool enable);

  /// Log hooks will be called directly after the function is called.
  /// Each parameter indicates the number of hooks called.
  void logHooks(int staticHooks, int memberHooks);

  /// Cursory description of the current undo stack.
  std::vector<std::string> getUndoStackDesc();

  /// Cursory description of the redo stack.
  std::vector<std::string> getRedoStackDesc();

  /// Description of all functions executed till this point.
  std::vector<std::string> getFullProvenanceDesc();



private:

  struct UndoRedoItem
  {
    UndoRedoItem(const std::string& funName,
                 std::tr1::shared_ptr<LuaCFunAbstract> undo,
                 std::tr1::shared_ptr<LuaCFunAbstract> redo)
    : function(funName), undoParams(undo), redoParams(redo)
    {}

    /// Function name we operate on at this stack index.
    std::string function;

    /// Prior execution of undoFunction (saved in lastExec table entry).
    std::tr1::shared_ptr<LuaCFunAbstract> undoParams;

    /// Parameters of the function, exactly as it was called.
    std::tr1::shared_ptr<LuaCFunAbstract> redoParams;
  };

  // Calls the function at UndoRedoItem index: funcIndex using the params
  // specified by funcToUse.
  void performUndoRedoOp(const std::string& funcName,
                         std::tr1::shared_ptr<LuaCFunAbstract> params);



  bool                      mEnabled;

  std::vector<UndoRedoItem> mUndoRedoStack; ///< Contains all undo/redo entries.
  int                       mStackPointer;  ///< 1 based Index into
                                            ///< mUndoRedoStack.

  /// Provenance description list. Text description of all functions executed to
  /// this point (including undo/redo exempt functions).
  std::vector<std::string>  mProvenanceDescList;

  LuaScripting* const       mScripting;
  LuaMemberRegUnsafe        mMemberReg;     ///< Used for member registration.


  /// This flag is only used when issuing an undo or redo. This is to ensure
  /// we don't get called when we are logging provenance.
  bool                      mLoggingProvenance;
  bool                      mDoProvReenterException;
  bool                      mProvenanceDescLogEnabled;

  /// Used to disable the provenance system when issuing an undo or redo
  /// call.
  bool                      mUndoRedoProvenanceDisable;
};

}

#endif
