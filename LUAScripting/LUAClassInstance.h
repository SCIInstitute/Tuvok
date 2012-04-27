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
  \brief  Class used to identify class instances inside of Lua.
          Class proxies are easier to use than some management scheme
          to keep track of class instances internally.

          These class proxies also give us a chance to perform undo/redo
          appropriately for the lifetime of objects.
*/

#ifndef TUVOK_LUACLASSINSTANCE_H_
#define TUVOK_LUACLASSINSTANCE_H_

namespace tuvok
{

class LuaScripting;

class LuaClassInstance
{
public:
  LuaClassInstance(int instanceID);
  ~LuaClassInstance() {}

  /// Retrieves the fully qualified name to the class instance.
  /// You can use this fully qualified name to hook functions associated
  /// with the class (using LuaMemberReg), or perform other operations.
  std::string fqName() const;

  /// Looks up the class constructor.
  std::string getClassConstructor() const;

  /// Retrieves global instance ID
  int getGlobalInstID() const       {return mInstanceID;}

  /// Data stored in the instance metatable.
  ///@{
  static const char*    MD_GLOBAL_INSTANCE_ID;  ///< Instance ID
  static const char*    MD_FACTORY_NAME;        ///< Generational factory.
  static const char*    MD_INSTANCE;            ///< Instance pointer.
  static const char*    MD_DEL_FUN;             ///< Delete function.
  ///@}

  /// SYSTEM_TABLE really should be stored in LuaScripting. But do to its
  /// use in LuaFunBinding.h, it is stored here.
  static const char* SYSTEM_TABLE;          ///< The LuaScripting system table

  static const char* CLASS_INSTANCE_TABLE;  ///< The global class instance table
  static const char* CLASS_INSTANCE_PREFIX; ///< Prefix for class instances

  /// Only use for testing.
  /// Please use LuaScripting::exec() instead.
  /// This pointer can die on you at any time. Also, undo/redo can invalidate
  /// this pointer.
  template <typename T>
  T* getRawPointer(std::tr1::shared_ptr<LuaScripting> ss);

private:

  lua_State* internalGetLuaState(std::tr1::shared_ptr<LuaScripting> ss);

  int         mInstanceID;

};

template <typename T>
T* LuaClassInstance::getRawPointer(std::tr1::shared_ptr<LuaScripting> ss)
{
  std::ostringstream os;
  os << "return getmetatable(" << fqName() << ")." << MD_INSTANCE;
  std::string luaCall = os.str();
  luaL_dostring(internalGetLuaState(ss), luaCall.c_str());

  // TODO: Add RTTI

  return reinterpret_cast<T*>(lua_touserdata(internalGetLuaState(ss), -1));
}

} /* namespace tuvok */

#endif
