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

#ifndef EXTERNAL_UNIT_TESTING

#include "Controller/Controller.h"
#include "3rdParty/LUA/lua.hpp"

#else

#include <assert.h>
#include "utestCommon.h"

#endif

#include <vector>

#include "LUAScripting.h"
#include "LUAStackRAII.h"

using namespace std;

//#define TUVOK_LUARAII_ASSERT

namespace tuvok
{

//-----------------------------------------------------------------------------
LuaStackRAII::LuaStackRAII(lua_State* L, int finalRelStackHeight,
                           const char* where, int line)
{
  mL = L;

  mInitialStackTop      = lua_gettop(mL);
  mFinalRelStackHeight  = finalRelStackHeight;

  mWhere = where;
  mLine = line;
}

//-----------------------------------------------------------------------------
LuaStackRAII::~LuaStackRAII()
{
  int stackTop = lua_gettop(mL);
  int stackTarget = mInitialStackTop + mFinalRelStackHeight;

  if (stackTop != stackTarget)
  {
    lua_getfield(mL, LUA_REGISTRYINDEX,
                 LuaScripting::REG_EXPECTED_EXCEPTION_FLAG);
    bool expected = lua_toboolean(mL, -1) ? true : false;
    lua_pop(mL, 1);

    if (expected == false)
    {
      // Take advantage of the fact that this class will be used in conjunction
      // with LuaScripting.
      ostringstream os;
      os << "log.error([==[LuaStackRAII: unexpected stack size. Expected: ";
      os << stackTarget << ". Actual: " << stackTop << ".";

      if (mWhere != NULL)
      {
        os << " File: " << mWhere << ". Line: " << mLine << ".";
      }

      // To ensure we don't wipe out Lua error messages (generally, these will
      // be caught by lua_atpanic).
      if (lua_isstring(mL, -1))
      {
        std::string str = lua_tostring(mL, -1);
        os << " String on the top of the stack: " << str;
      }

      os << "]==])";

      // Pull value out of the Lua register to see if this is really an
      // error, or an expected event (in the case of unit testing this would
      // likely be an expected exception)

      // Bypass lua since this is a lua error.
      luaL_dostring(mL, os.str().c_str());

#ifdef TUVOK_LUARAII_ASSERT
      assert(stackTop == stackTarget);
#endif
    }

    // Ensure stack ends up at the stack target (RAII)
    lua_settop(mL, stackTarget);
  }
}

}
