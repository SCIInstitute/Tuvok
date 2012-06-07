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
 \brief Class made to be composited inside of a Lua class instance.
        All class instance function registrations must happen through
        this class.
 */

#ifndef TUVOK_LUACLASSREGISTRATION_H_
#define TUVOK_LUACLASSREGISTRATION_H_

#include "LUACommon.h"
#include "LUAClassInstance.h"
#include "LUAMemberRegUnsafe.h"
#include "LUAProvenance.h"

namespace tuvok
{

class LuaClassConstructor;

template <typename T>
class LuaClassRegistration
{
  friend class LuaClassConstructor; // The only class that can instantiate us.
public:

  ~LuaClassRegistration() {}

  bool canRegister() const    {return (mPtr != NULL);}

  /// Registers a member function. Same parameters as
  /// LuaScripting::registerFunction but with the unqualifiedName parameter.
  /// unqualifiedName is the name of the function to register without the class
  /// name appended to it.
  /// \return The fully qualified name for this function. Useful for applying
  ///         option parameters and defaults.
  template <typename FunPtr>
  std::string function(FunPtr f, const std::string& unqualifiedName,
                       const std::string& desc, bool undoRedo);

  std::string getFQName() const {return getLuaInstance().fqName();}
  LuaClassInstance getLuaInstance() const {return LuaClassInstance(mGlobalID);}


  /// Makes this class instance inherit methods from the given class instance.
  /// Use this function sparingly. A strong reference to the given instance will
  /// be generated. If the instance is every destroyed, we will run into access
  /// violations.
  /// TODO: Detect inherited classes and make them weak references.
  void inherit(LuaClassInstance from);

private:

  // Feel free to copy the class.
  LuaClassRegistration(LuaScripting* ss, T* t, int globalID)
  : mSS(ss)
  , mGlobalID(globalID)
  , mPtr(t)
  , mRegistration(ss)
  {}

  LuaScripting*         mSS;

  int                   mGlobalID;
  T*                    mPtr;

  LuaMemberRegUnsafe    mRegistration;

};

template <typename T>
template <typename FunPtr>
std::string LuaClassRegistration<T>::function(FunPtr f,
                                           const std::string& unqualifiedName,
                                           const std::string& desc,
                                           bool undoRedo)
{
  if (canRegister() == false)
    throw LuaError("Check canRegister before registering functions! This "
        "error indicates that you have a Lua class that was not created using "
        "the scripting system.");

  LuaClassInstance inst(mGlobalID);

  std::ostringstream qualifiedName;
  qualifiedName << inst.fqName() << "." << unqualifiedName;

  // Function pointer f is guaranteed to be a member function pointer.
  mRegistration.registerFunction(mPtr, f, qualifiedName.str(), desc,
                                 undoRedo);

  return qualifiedName.str();
}

template <typename T>
void LuaClassRegistration<T>::inherit(LuaClassInstance them)
{
  LuaScripting* ss(mSS);
  lua_State* L = ss->getLUAState();

  LuaStackRAII _a(L, 0);

  // Generate our own LuaClassInstance.
  LuaClassInstance us(mGlobalID);

  // Obtain the metatable for 'us'.
  ss->getFunctionTable(us.fqName());
  int ourTable = lua_gettop(L);
  ss->getFunctionTable(them.fqName());
  int theirTable = lua_gettop(L);

  if (lua_getmetatable(L, ourTable) == 0)
    throw LuaError("Unable to find metatable for our class!");

  // Set the index metamethod.
  lua_pushvalue(L, theirTable);
  lua_setfield(L, -2, "__index");

  lua_pop(L, 1);  // pop off the metatable.

  lua_pop(L, 2);
}

} /* namespace tuvok */
#endif /* LUACLASSREGISTRATION_H_ */
