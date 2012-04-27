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
  \brief  Hooks and cleans up Lua class instance function pointers.
*/

// Used to hook functions associated with class instances.
// This file will remove all hooks in its destructor.

#ifndef TUVOK_LUACLASSINSTANCEHOOK_H_
#define TUVOK_LUACLASSINSTANCEHOOK_H_

#include "LUAClassInstance.h"

namespace tuvok
{

// You can hook as many class instances as you want with this class
// It will deregister them all when you are done.
class LuaClassInstanceHook
{
public:
  LuaClassInstanceHook(std::tr1::shared_ptr<LuaScripting> ss);
  ~LuaClassInstanceHook();

  /// Generates a hook given the Lua class instance,
  /// and the function to call when the class instance
  /// is called.
  template <typename FunPtr>
  void strictHook(const LuaClassInstance& hook,
		  const std::string& funNameToHook,
		  FunPtr funToCall);

private:

  struct HookedLuaClass
  {
    LuaClassInstance  inst;       ///< The Lua class instance.
    std::string       funName;    ///< Not fully qualified.
  };

  std::tr1::shared_ptr<LuaScripting>  mScriptSystem;
  std::vector<HookedLuaClass>         mHookedFunctions;

  /// ID used by LUA in order to identify the functions hooked by this class.
  /// This ID is used as the key in the hook table.
  const std::string                   mHookID;

};

} /* namespace tuvok */

#endif

