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
 \file    LuaDebug.cpp
 \author  James Hughes
 \date    Jun 29, 2012
 \brief   
 */

#include <string>
#include <vector>

#include "LuaDebug.h"

namespace tuvok
{

//-----------------------------------------------------------------------------
LuaDebug::LuaDebug(LuaScripting* scripting)
: mMemberReg(scripting)
{

}

//-----------------------------------------------------------------------------
LuaDebug::~LuaDebug()
{
  // We purposefully do NOT unregister our Lua functions.
  // Since we are being destroyed, it is likely the lua_State has already
  // been closed by the class that composited us.
}

//-----------------------------------------------------------------------------
void LuaDebug::registerLuaDebugFunctions()
{
  // NOTE: We cannot use the LuaMemberReg class to manage our registered
  // functions because it relies on a shared pointer to LuaScripting; since we
  // are composited inside of LuaScripting, no such shared pointer is available.
  /// @todo Implement this function. It's not as simple as inserting a strict
  ///       hook since we don't know the function's parameters. We should
  ///       implement it so that it is able to print out the parameters
  ///       that were used to call the function as well.
//  mMemberReg.registerFunction(this, &LuaDebug::watchFunction,
//                              "debug.watchToConsole",
//                              "Watches the given function. Whenever the "
//                              "function is executed, a notification is "
//                              "printed to the console.",
//                              false);
}

//-----------------------------------------------------------------------------
void LuaDebug::watchFunction(const std::string& function)
{

}

} /* namespace tuvok */
