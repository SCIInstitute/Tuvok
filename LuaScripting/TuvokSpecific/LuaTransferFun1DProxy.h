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
 \brief Lua proxy for IO's 1D transfer function.
 */

#ifndef TUVOK_LUATRANSFERFUN1DPROXY_H
#define TUVOK_LUATRANSFERFUN1DPROXY_H

class TransferFunction1D;

namespace tuvok {

/// @brief classDescription
class LuaTransferFun1DProxy
{
public:
  LuaTransferFun1DProxy();
  virtual ~LuaTransferFun1DProxy();

  void bind(TransferFunction1D* tf);

  static LuaTransferFun1DProxy* luaConstruct() 
  {
    return new LuaTransferFun1DProxy();
  }
  static void defineLuaInterface(LuaClassRegistration<LuaTransferFun1DProxy>& reg,
                                 LuaTransferFun1DProxy* me,
                                 LuaScripting* ss);

  /// Transfer function pointer retrieval. 
  TransferFunction1D* get1DTransferFunction() {return m1DTrans;}

private:

  /// Auxiliary functions
  bool proxyLoadWithFilenameAndSize(const std::string& file, size_t size);
  void proxySetStdFunction(float centerPoint, float invGradient, 
                           int component, bool invertedStep);
  bool proxySave(const std::string& filename) const;

  /// Class registration we received from defineLuaInterface.
  /// @todo Change to unique pointer.
  LuaClassRegistration<LuaTransferFun1DProxy>*  mReg;

  /// The 1D transfer function that we represent.
  TransferFunction1D*                           m1DTrans;
};

} // namespace tuvok

#endif // TUVOK_LUATRANSFERFUN1DPROXY_H
