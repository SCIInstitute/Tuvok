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
 \file    LUAMemberHook.h
 \author  James Hughes
          SCI Institute
          University of Utah
 \date    Mar 26, 2012
 \brief   A mechanism to hook into the Scripting System using member functions.
          Used for updating the UI when actions, such as undo/redo, are
          executed.
          Instantiate this class alongside your encapsulating class.
 */

#ifndef TUVOK_LUAMEMBER_HOOK_H_
#define TUVOK_LUAMEMBER_HOOK_H_


namespace tuvok
{

class LuaScripting;

class LuaMemberHook
{
public:

  LuaMemberHook(std::tr1::shared_ptr<LuaScripting> scriptSys);
  virtual ~LuaMemberHook();

  /// Hooks a member function to the execution of the given lua function
  /// (fqName).
  /// The given function (f) MUST match the signature of the hooked function,
  /// including its return type. This is the 'strictness'.
  /// It is not permitted to install more than one hook on the same LUA function.
  /// I.E. you can not hook render.eye twice using the same LUAMemberHook.
  template <typename T, typename FunPtr>
  void strictHook(T* C, FunPtr f, const std::string& fqName)
  {

  }

  /// TODO: implement a 'loose' hook where the function must match a
  ///       particular signature (such as void myfunc(lua_State* L).
  ///       Only do this if there is an identifiable need for it.
  ///       I want imagevis3d to throw exceptions when its api changes and it
  ///       hasn't been updated to comply.


private:

  /// Scripting system we are bound to.
  std::tr1::shared_ptr<LuaScripting>  mScriptSystem;

  /// Functions registered with this hook -- used for unregistering hooks.
  std::vector<std::string>            mHookedFunctions;

  /// ID used by LUA in order to identify the functions hooked by this class.
  /// This ID is used as the key in the hook table.
  std::string                         mHookID;
};

}

#endif

