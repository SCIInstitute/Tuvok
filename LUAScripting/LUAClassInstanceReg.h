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
 \brief Provides a C++ class proxy in Lua.
 */

#ifndef TUVOK_LUACLASSINSTANCEREG_H_
#define TUVOK_LUACLASSINSTANCEREG_H_

namespace tuvok
{

class LuaClassInstanceReg
{
  friend class LuaScripting;
public:

  /// This constructor is called from LuaScripting.
  LuaClassInstanceReg(std::tr1::shared_ptr<LuaScripting> scriptSys,
                      const std::string& fqName);

  /// Registers the class instance constructor.
  /// This constructor is used to make the 'fqName' table executable, and to
  /// create the appropriate new() function underneath 'fqName'.
  template <typename FunPtr>
  void registerConstructor(FunPtr f);

  /// Registers a member function. Same parameters as
  /// LuaScripting::registerFunction, with the unqualifiedName parameter.
  /// unqualifiedName should be just the name of the function to register,
  /// and should not include any module paths.
  template <typename FunPtr>
  void registerFunction(FunPtr f, const std::string& unqualifiedName,
                        const std::string& desc, bool undoRedo);

private:


  /// Pointer to our encapsulating class.
  std::tr1::shared_ptr<LuaScripting>  mSS;
  std::string                         mClassPath; ///< fqName passed into init.

};

template <typename FunPtr>
void LuaClassInstanceReg::registerConstructor(FunPtr f)
{

}

template <typename FunPtr>
void LuaClassInstanceReg::registerFunction(FunPtr f,
                                           const std::string& unqualifiedName,
                                           const std::string& desc,
                                           bool undoRedo)
{

}

} /* namespace tuvok */
#endif /* TUVOK_LUAINSTANCEREG_H_ */
