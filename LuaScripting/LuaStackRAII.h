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
  \brief   Implements RAII for the Lua stack.
           For internal LuaScripting uses only. Requires an unshared pointer to
           LuaScripting, and is only meant to exist for the lifetime of one
           block.
*/


#ifndef TUVOK_LUA_STACK_RAII_H_
#define TUVOK_LUA_STACK_RAII_H_

namespace tuvok
{

class LuaStackRAII
{
public:
  /// Use this class to ensure the Lua stack is cleaned up properly when an
  /// exception is thrown.
  /// \param  finalRelStackHeight   Final relative stack height.
  ///                               Accepts any valid integer. Positive values
  ///                               indicate that values will be left on the
  ///                               stack. 0 indicates that the stack should be
  ///                               left unchanged. Negative values indicate
  ///                               that values should be removed from the
  ///                               stack.
  LuaStackRAII(lua_State* L, int finalRelStackHeight,
               const char* where = nullptr, int line = 0);

  virtual ~LuaStackRAII();

private:

  int           mInitialStackTop;
  int           mFinalRelStackHeight;

  const char*   mWhere;
  int           mLine;

  lua_State*    mL;

};

}

#endif
