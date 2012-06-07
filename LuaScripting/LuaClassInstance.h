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
  LuaClassInstance();
  ~LuaClassInstance() {}

  /// Retrieves the fully qualified name to the class instance.
  /// You can use this fully qualified name to hook functions associated
  /// with the class (using LuaMemberReg), or perform other operations.
  std::string fqName() const;

  /// Looks up the class constructor.
  std::string getClassConstructor() const;

  /// Retrieves global instance ID
  int getGlobalInstID() const       {return mInstanceID;}

  /// Returns true if this is the default instance of this class (no
  /// real data or functions).
  bool isDefaultInstance() const;
  bool isValid() const              {return !isDefaultInstance();}

  /// Data stored in the instance metatable.
  ///@{
  static const char*    MD_GLOBAL_INSTANCE_ID;  ///< Instance ID
  static const char*    MD_FACTORY_NAME;        ///< Generational factory.
  static const char*    MD_INSTANCE;            ///< Instance pointer.
  static const char*    MD_DEL_FUN;             ///< Delete function.
  static const char*    MD_DEL_CALLBACK_PTR;    ///< Delete callback function.
  static const char*    MD_NO_DELETE_HINT;      ///< If true, delete will not be
                                                ///< called on the class.
                                                ///< Used when the constructor
                                                ///< notifies us of destruction.
  ///@}

  /// SYSTEM_TABLE really should be stored in LuaScripting. But do to its
  /// use in LuaFunBinding.h, it is stored here.
  static const char* SYSTEM_TABLE;          ///< The LuaScripting system table

  static const char* CLASS_INSTANCE_TABLE;  ///< The global class instance table
  static const char* CLASS_INSTANCE_PREFIX; ///< Prefix for class instances
  static const char* CLASS_LOOKUP_TABLE;    ///< Global class lookup table.

  static const int DEFAULT_INSTANCE_ID = -1;

  /// Only use these two functions for testing.
  /// Please use LuaScripting::exec() instead.
  /// This pointer can die on you at any time. Undo/redo can invalidate
  /// this pointer.
  template <typename T>
  T* getRawPointer(std::tr1::shared_ptr<LuaScripting> ss);
  void* getVoidPointer(LuaScripting* ss);

private:

  int         mInstanceID;

};

template <typename T>
T* LuaClassInstance::getRawPointer(std::tr1::shared_ptr<LuaScripting> ss)
{
  // TODO: Add RTTI
  return reinterpret_cast<T*>(getVoidPointer(ss.get()));
}

} /* namespace tuvok */

#endif
