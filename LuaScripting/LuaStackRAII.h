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
  /// \param valsConsumed   The # of values removed from the stack. 
  ///                       This number should NOT take into account
  ///                       valsReturned.
  /// \param valsReturned   The number of return values pushed onto the stack.
  ///                       This number should NOT take into account 
  ///                       valsConsumed.
  /// Example:
  ///  void someFun(..)
  ///  {
  ///    LuaStackRAII _a(mL, 2, 1);
  ///  ...
  ///  }
  ///  After successful completion of this function, LuaStackRAII's destructor
  ///  expects that the stack will contain one less element relative to when 
  ///  LuaStackRAII was constructed.
  LuaStackRAII(lua_State* L, int valsConsumed, int valsReturned);
  virtual ~LuaStackRAII();

private:

  int           mInitialStackTop;
  int           mValsConsumed;
  int           mValsReturned;

  lua_State*    mL;

};

}

#endif
