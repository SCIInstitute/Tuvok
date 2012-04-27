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
 \brief   This mediator class handles member function registration.
          This class is used for internal purposes only, please use
          LuaMemberReg instead.

          Exists solely to provide member function registration to LuaScripting
          and its composited classes (without the need for a shared_ptr).

          If you do use this class instead of LuaMemberReg, be sure to call
          the unregisterAll function manually when your class is destroyed.
          It is your responsibility to ensure that the LuaScripting pointer
          remains valid throughout the lifetime of LuaMemberRegUnsafe.
 */


#ifndef EXTERNAL_UNIT_TESTING

#include "Controller/Controller.h"
#include "3rdParty/LUA/lua.hpp"

#else

#include "assert.h"
#include "utestCommon.h"

#endif

#include <vector>

#include "LUAError.h"
#include "LUAFunBinding.h"
#include "LUAScripting.h"
#include "LuaMemberRegUnsafe.h"

using namespace std;

namespace tuvok
{

//-----------------------------------------------------------------------------
LuaMemberRegUnsafe::LuaMemberRegUnsafe(LuaScripting* scriptSys)
: mScriptSystem(scriptSys)
, mHookID(scriptSys->getNewMemberHookID())
{

}

//-----------------------------------------------------------------------------
LuaMemberRegUnsafe::~LuaMemberRegUnsafe()
{
  // We assume it is UNSAFE to do anything with the scriptSys variable here!
  // The majority of the time, this class will be subclassed by
  // LuaMemberReg, and LuaMemberReg may end up destroying the shared pointer to
  // LuaScripting. So we have no guarantees that the pointer to our
  // LuaScripting class is still valid.
}

//-----------------------------------------------------------------------------
void LuaMemberRegUnsafe::unregisterAll()
{
  // Unhook BEFORE unregister!
  unregisterHooks();
  unregisterUndoRedoFunctions();
  unregisterFunctions();
}

//-----------------------------------------------------------------------------
void LuaMemberRegUnsafe::unregisterFunctions()
{
  // Loop through the registered functions and unregister them with the system.
  for (vector<string>::iterator it = mRegisteredFunctions.begin();
       it != mRegisteredFunctions.end(); ++it)
  {
    try
    {
      mScriptSystem->unregisterFunction(*it);
    }
    catch (LuaError&)
    {
      // If this happens, it means that someone else unregistered our
      // function.
      // Ignore it and move on.
    }
  }

  mRegisteredFunctions.clear();
}

//-----------------------------------------------------------------------------
void LuaMemberRegUnsafe::unregisterHooks()
{
  lua_State* L = mScriptSystem->getLUAState();
  LuaStackRAII _a = LuaStackRAII(L, 0);

  // Loop through the hooks, and unregister them from their functions
  for (vector<string>::iterator it = mHookedFunctions.begin();
       it != mHookedFunctions.end(); ++it)
  {
    // Push the table associated with the function to the top.
    mScriptSystem->getFunctionTable(*it);

    if (lua_isnil(L, -1))
    {
      // Ignore missing function table and move on.
      lua_pop(L,1);
      continue;
    }

    // Obtain the hooked member function table.
    lua_getfield(L, -1, LuaScripting::TBL_MD_MEMBER_HOOKS);

    // Just set the member hook ID field to nil, don't check to see if its there
    // since we do not wan to throw an exception (likely called from
    // destructor).
    lua_pushnil(L);
    lua_setfield(L, -2, mHookID.c_str());

    // Pop function table and hooks table off the stack.
    lua_pop(L, 2);
  }

  mHookedFunctions.clear();
}

//-----------------------------------------------------------------------------
void LuaMemberRegUnsafe::unregisterUndoRedoFunctions()
{
  lua_State* L = mScriptSystem->getLUAState();
  LuaStackRAII _a = LuaStackRAII(L, 0);

  // Loop through the hooks, and unregister them from their functions
  for (vector<UndoRedoReg>::iterator it = mRegisteredUndoRedo.begin();
       it != mRegisteredUndoRedo.end(); ++it)
  {
    // Push the table associated with the function to the top.
    mScriptSystem->getFunctionTable(it->functionName);

    if (lua_isnil(L, -1))
    {
      // Ignore missing function table and move on.
      lua_pop(L,1);
      continue;
    }

    lua_pushnil(L);
    if (it->isUndo)
    {
      lua_setfield(L, -2, LuaScripting::TBL_MD_UNDO_FUNC);
    }
    else
    {
      lua_setfield(L, -2, LuaScripting::TBL_MD_REDO_FUNC);
    }

    // Pop function table off the stack.
    lua_pop(L, 1);
  }

  mRegisteredUndoRedo.clear();
}

//==============================================================================
//
// UNIT TESTING
//
//==============================================================================

// Unit testing for this class is in LuaMemberReg.

} /* namespace tuvok */

