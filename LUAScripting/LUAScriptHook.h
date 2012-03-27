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
 \file    LUAScriptHook.h
 \author  James Hughes
          SCI Institute
          University of Utah
 \date    Mar 26, 2012
 \brief   A hook into the LUAScriptingSystem.
          Gets called when hooked LUA functions are executed.
          Used for updating the UI when actions, such as undo/redo, are
          executed.
          Instantiate this class alongside your class.
 */

#ifndef LUASCRIPT_HOOK_H_
#define LUASCRIPT_HOOK_H_


namespace tuvok
{

class LUAScripting;

class LUAScriptHook
{
public:

  LUAScriptHook(std::tr1::shared_ptr<LUAScripting> scriptSys);
  virtual ~LUAScriptHook();

  /// Hooks the indicated fully qualified function name with the given function.
  /// \param  fqName    Fully qualified name to hook.
  /// \param  f         Function pointer.
  template <typename FunPtr>
  void hookFunction(const std::string& fqName, FunPtr f);

private:

  /// Scripting system we are bound to.
  std::tr1::shared_ptr<LUAScripting>  mScriptSystem;

  /// Functions registered with this hook.
  std::vector<std::string>            mHookedFunctions;
};

}

#endif

