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

namespace tuvok
{

class LuaScripting;

class LuaProvenance
{
public:
  // Class made for compositing inside LuaScripting, hence the raw pointer.
  LuaProvenance(LuaScripting* scripting);
  ~LuaProvenance();

  bool isEnabled();
  void setEnabled(bool enabled);

  void logProvenance(std::string function,
                     std::tr1::shared_ptr<LuaCFunAbstract> funParams);

  void issueUndo();
  void issueRedo();

private:

  bool                  mEnabled;

  struct UndoItem
  {
    /// prevExec contains the prior execution parameters for the function that
    /// was executed at this undo step. prevExec is for repopulating the
    /// lastExec table in the function's callable LUA table.
    std::string functionCalled;
    std::tr1::shared_ptr<LuaCFunAbstract> prevExec;
    std::tr1::shared_ptr<LuaCFunAbstract> undoEntry;
  };

  struct RedoItem
  {
    std::string functionCalled;
    std::tr1::shared_ptr<LuaCFunAbstract> redoEntry;
  };

  std::vector<UndoItem> mUndoStack;
  std::vector<RedoItem> mRedoStack;

  LuaScripting* const   mScripting;
};

}

#endif
