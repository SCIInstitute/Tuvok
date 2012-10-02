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

#ifndef TUVOK_LUAPROVENANCE_H_
#define TUVOK_LUAPROVENANCE_H_

#include "LuaMemberRegUnsafe.h"
#include <algorithm>

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
                    std::shared_ptr<LuaCFunAbstract> funParams,
                    std::shared_ptr<LuaCFunAbstract> emptyParams);

  // Modifies the last provenance log.
  void ammendLastProvLog(const std::string& ammend);

  /// Performs an undo.
  void issueUndo();

  /// Performs a redo.
  void issueRedo();

  /// Registers provenance functions with Lua.
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

  /// Temporarily disables provenance. Used when setting defaults for
  /// functions in the scripting system. We don't want to register any calls
  /// to provenance when we are registering defaults.
  void setDisableProvTemporarily(bool disable);

  /// Begin a new command group.
  /// Command groups lump commands together that should be undone / redone
  /// together.
  void beginCommand();

  /// End a command group.
  void endCommand();

  /// Retrieve command depth.
  int getCommandDepth()   {return mCommandDepth;}

  /// Testing function to check if the last undo/redo item contains the list
  /// of deleted items.
  /// Returns true if all of the deleted items are present.
  bool testLastURItemHasDeletedItems(const std::vector<int>& delItems) const;

  /// Testing function to check if the last undo/redo item contains the list
  /// of created items.
  /// Returns true if all of the created items are present.
  bool testLastURItemHasCreatedItems(const std::vector<int>& createdItems)const;

  /// Looks at the top of the undo/redo stack, and sets the
  /// 'alsoRedoChildren' flag to true in the UndoRedoItem.
  void setLastURItemAlsoRedoChildren();

  // The following two functions are only meant for LuaClassConstructor
  // and LuaScripting.

  /// Adds a deleted instance to the last Undo/Redo item.
  void addDeletedInstanceToLastURItem(int globalID);

  /// Adds a constructed instance to the last Undo/Redo item.
  void addCreatedInstanceToLastURItem(int globalID);

private:

  /// Issue an undo that does NOT check whether instances were created /
  /// destroyed.
  void issueUndoInternal();

  /// Determines the number of undos that need to be executed in order to
  /// compeletly rebuild the instances at the given undo index.
  /// Returns the number of undos that must be executed to get to a state
  /// directly before the instance object was created.
  /// Returns 3 integers:
  /// 1) Number of undos that must be performed
  /// 2) Lower ID bound for the instances created (lowest ID created).
  /// 3) Higher ID bound for the instances created (highest ID created).
  int bruteRerollDetermineUndos(int undoIndex);

  struct UndoRedoItem
  {
    UndoRedoItem(const std::string& funName,
                 std::shared_ptr<LuaCFunAbstract> undo,
                 std::shared_ptr<LuaCFunAbstract> redo)
    : function(funName), undoParams(undo), redoParams(redo), childItems()
    , instCreations(), instDeletions(), alsoRedoChildren(false)
    {}

    /// Function name we operate on at this stack index.
    std::string function;

    /// Prior execution of undoFunction (saved in lastExec table entry).
    std::shared_ptr<LuaCFunAbstract> undoParams;

    /// Parameters of the function, exactly as it was called.
    std::shared_ptr<LuaCFunAbstract> redoParams;

    /// Child items will be RARELY used. Only by those few functions that
    /// nest lua provenance enabled function calls inside of other lua
    /// provenance enabled functions (function composition). Built to perform
    /// provenance on composited class instances and to handle function
    /// composition better.
    ///
    /// Generally, the undo step will be the only step that cares about these
    /// child items (redoing will call the function that generated these items
    /// in the first place -- so redoing will ignore the child items completely)
    /// I have some hand written notes on the structure of this provenance
    /// system, if there are any interested parties.
    std::shared_ptr<std::vector<UndoRedoItem>> childItems;

    /// Pushing child items like this (along with the way the provenance calls
    /// functions, then their children) reverses the order in which the
    /// functions were called originally. We want this for undo.
    ///
    /// Reversing the call sequence appropriately resets the state of the
    /// program back to before the composited function was called (see
    /// ProvenanceCompositeSingle unit test). Mostly has to deal with the top
    /// most composited function having different parameter defaults / current
    /// undoredo stack state.
    void addChildItem(const UndoRedoItem& item)
    {
      if (childItems.get() == NULL)
      {
        childItems = std::shared_ptr<std::vector<UndoRedoItem>>(
            new std::vector<UndoRedoItem>());
      }
      childItems->push_back(item);
    }

    /// Instance IDs that were created as a result of this call
    /// (these instance IDs will be contiguous. e.g. [2, 5] or [234, 265], but
    ///  never {245, 246, 248} where we are missing an instance ID in a specific
    ///  range).
    std::shared_ptr<std::vector<int>> instCreations;
    void addInstCreation(int id)
    {
      if (instCreations.get() == NULL)
      {
        instCreations = std::shared_ptr<std::vector<int>>(
            new std::vector<int>());
      }
      instCreations->push_back(id);
      std::sort(instCreations->begin(), instCreations->end());
    }

    /// Instance IDs that were deleted as a result of this call.
    std::shared_ptr<std::vector<int>> instDeletions;
    void addInstDeletion(int id)
    {
      if (instDeletions.get() == NULL)
      {
        instDeletions = std::shared_ptr<std::vector<int>>(
            new std::vector<int>());
      }
      instDeletions->push_back(id);
      std::sort(instDeletions->begin(), instDeletions->end());
    }

    /// This flag informs the system that the children of this undo/redo
    /// item must be explicitly called by the redo mechanism. This is only
    /// used to group command together.
    bool alsoRedoChildren;
  };

  typedef std::vector<UndoRedoItem> URStackType;

  // Calls the function at UndoRedoItem index: funcIndex using the params
  // specified by funcToUse.
  void performUndoRedoOp(const std::string& funcName,
                         std::shared_ptr<LuaCFunAbstract> params,
                         bool isUndo);

  std::vector<std::string> getUndoStackDesc();
  void printUndoStack();

  std::vector<std::string> getRedoStackDesc();
  void printRedoStack();

  std::vector<std::string> getFullProvenanceDesc();
  void printProvRecord();
  void printProvRecordToFile(const std::string& file);

  bool                      mEnabled;
  bool                      mTemporarilyDisabled;

  bool                      mUndoingInstanceDel;  ///< Used in error checking.

  URStackType               mUndoRedoStack; ///< Contains all undo/redo entries.
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

  /// Current command depth. Only the first command is placed in provenance
  /// and the undo/redo buffer. The calls deeper than the first command are
  /// still logged into the provenance logging system for debugging purposes.
  int                       mCommandDepth;
};

}

#endif
