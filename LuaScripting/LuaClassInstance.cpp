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
  \brief   
*/

#include <iostream>
#include <vector>

#include "LuaScripting.h"

using namespace std;

namespace tuvok
{

const char* LuaClassInstance::MD_GLOBAL_INSTANCE_ID = "globalID";
const char* LuaClassInstance::MD_FACTORY_NAME       = "factoryName";
const char* LuaClassInstance::MD_INSTANCE           = "instance";
const char* LuaClassInstance::MD_DEL_FUN            = "delFun";
const char* LuaClassInstance::MD_DEL_CALLBACK_PTR   = "delCallbackPtr";
const char* LuaClassInstance::MD_NO_DELETE_HINT     = "deleteHint";

// NOTE: Only keys that begin with an underscore and are followed by upper
// case letters are reserved by Lua. See:
// http://www.lua.org/manual/5.2/manual.html#4.5 . As such, we do not use
// uppercase lettersin SYSTEMT_TABLE, just lower case letters.
const char* LuaClassInstance::SYSTEM_TABLE          = "_sys_";
const char* LuaClassInstance::CLASS_INSTANCE_TABLE  = "_sys_.inst";
const char* LuaClassInstance::CLASS_INSTANCE_PREFIX = "m";
const char* LuaClassInstance::CLASS_LOOKUP_TABLE    = "_sys_.lookup";

LuaClassInstance::LuaClassInstance(int instanceID)
: mInstanceID(instanceID)
{

}

LuaClassInstance::LuaClassInstance()
: mInstanceID(DEFAULT_INSTANCE_ID)
{

}

//{return mFullyQualifiedName;}
std::string LuaClassInstance::fqName() const
{
  std::ostringstream os;
  os << LuaClassInstance::CLASS_INSTANCE_TABLE << "."
     << LuaClassInstance::CLASS_INSTANCE_PREFIX << mInstanceID;
  return os.str();
}


bool LuaClassInstance::isDefaultInstance() const
{
  return (mInstanceID == DEFAULT_INSTANCE_ID);
}

bool LuaClassInstance::isValid(std::tr1::shared_ptr<LuaScripting> ss) const
{
  return isValid(ss.get());
}

bool LuaClassInstance::isValid(LuaScripting* ss) const
{
  LuaStackRAII _a(ss->getLuaState(), 0);

  if (isDefaultInstance())
    return false;

  // Check to see whether an instance is present in the instance table.
  // If it is not, then this is NOT a valid class.
  if (ss->getFunctionTable(fqName()))
  {
    lua_pop(ss->getLuaState(), 1);
    return true;
  }
  else
  {
    return false;
  }
}

void* LuaClassInstance::getVoidPointer(LuaScripting* ss)
{
  lua_State* L = ss->getLuaState();
  LuaStackRAII _a(L, 0);

  if (ss->getFunctionTable(fqName()) == false)
    throw LuaError("Invalid function table.");
  if (lua_getmetatable(L, -1) == 0)
    throw LuaError("Unable to obtain function metatable");
  lua_getfield(L, -1, MD_INSTANCE);
  void* r = lua_touserdata(L, -1);
  lua_pop(L, 3);  // Table, metatable, and userdata.

  return r;
}

void LuaClassInstance::invalidate()
{
  mInstanceID = DEFAULT_INSTANCE_ID;
}

}

