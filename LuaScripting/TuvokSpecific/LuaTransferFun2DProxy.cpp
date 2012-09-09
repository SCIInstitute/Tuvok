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
 \brief A Lua class proxy for IO's TransferFunction2D class.
 */

#include <vector>
#include "Controller/Controller.h"
#include "3rdParty/LUA/lua.hpp"
#include "../LuaScripting.h"
#include "../LuaClassRegistration.h"
#include "LuaTuvokTypes.h"
#include "LuaTransferFun2DProxy.h"
#include "LuaTransferFun1DProxy.h"

using namespace tuvok;
using namespace std;

//------------------------------------------------------------------------------
LuaTransferFun2DProxy::LuaTransferFun2DProxy()
  : mReg(NULL),
    m2DTrans(NULL),
    mSS(NULL)
{
}

//------------------------------------------------------------------------------
LuaTransferFun2DProxy::~LuaTransferFun2DProxy()
{
  if (mReg != NULL)
    delete mReg;
}

//------------------------------------------------------------------------------
void LuaTransferFun2DProxy::bind(TransferFunction2D* tf)
{
  if (mReg == NULL)
    throw LuaError("Unable to bind transfer function 2D , no class registration"
                   " available.");

  mReg->clearProxyFunctions();

  m2DTrans = tf;
  if (tf != NULL)
  {
    // Register TransferFunction2D functions using tf.
    std::string id;
    /// @todo Determine which of the following functions should have provenance
    ///       enabled.
    id = mReg->functionProxy(tf, &TransferFunction2D::SwatchArrayGetSize,
                             "swatchGetCount", "", false);
    id = mReg->functionProxy(tf, &TransferFunction2D::SwatchPushBack,
                             "swatchPushBack", "", false);
    id = mReg->functionProxy(tf, &TransferFunction2D::SwatchErase,
                             "swatchErase", "", false);
    id = mReg->functionProxy(tf, &TransferFunction2D::SwatchInsert,
                             "swatchInsert", "", false);
    id = mReg->functionProxy(tf, &TransferFunction2D::SwatchUpdate,
                             "swatchUpdate", "", false);
    id = mReg->functionProxy(tf, &TransferFunction2D::SwatchIsRadial,
                             "swatchIsRadial", "", false);
    id = mReg->functionProxy(tf, &TransferFunction2D::SwatchSetRadial,
                             "swatchSetRadial", "", true);
    id = mReg->functionProxy(tf, &TransferFunction2D::SwatchGetNumPoints,
                             "swatchGetNumPoints", "", false);
    id = mReg->functionProxy(tf, &TransferFunction2D::SwatchErasePoint,
                             "swatchErasePoint", "", false);
    id = mReg->functionProxy(tf, &TransferFunction2D::SwatchInsertPoint,
                             "swatchInsertPoint", "", false);
    id = mReg->functionProxy(tf, &TransferFunction2D::SwatchGetGradientCount,
                             "swatchGetGradientCount", "", false);
    id = mReg->functionProxy(tf, &TransferFunction2D::SwatchGetGradient,
                             "swatchGetGradient", "", false);
    id = mReg->functionProxy(tf, &TransferFunction2D::SwatchInsertGradient,
                             "swatchInsertGradient", "", true);
    id = mReg->functionProxy(tf, &TransferFunction2D::SwatchPushBackGradient,
                             "swatchPushBackGradient", "", true);
    id = mReg->functionProxy(tf, &TransferFunction2D::SwatchEraseGradient,
                             "swatchEraseGradient", "", true);
    id = mReg->functionProxy(tf, &TransferFunction2D::SwatchUpdateGradient,
                             "swatchUpdateGradient", "", true);
    id = mReg->functionProxy(tf, &TransferFunction2D::SwatchGet,
                             "swatchGet", "", false);
  }
}

//------------------------------------------------------------------------------
void LuaTransferFun2DProxy::defineLuaInterface(
    LuaClassRegistration<LuaTransferFun2DProxy>& reg,
    LuaTransferFun2DProxy* me,
    LuaScripting* ss)
{
  me->mReg = new LuaClassRegistration<LuaTransferFun2DProxy>(reg);
  me->mSS = ss;

  reg.function(&LuaTransferFun2DProxy::proxyLoadWithSize,
               "loadWithSize", "Loads 'file' into the 2D "
               " transfer function given 'size'.", false);
  reg.function(&LuaTransferFun2DProxy::proxyGetRenderSize,
               "getRenderSize", "", false);
  reg.function(&LuaTransferFun2DProxy::proxyGetSize,
               "getSize", "", false);
  reg.function(&LuaTransferFun2DProxy::proxySave,
               "save", "", false);
  reg.function(&LuaTransferFun2DProxy::proxyUpdate1DTrans,
               "update1DTrans", "", false);
}

//------------------------------------------------------------------------------
bool LuaTransferFun2DProxy::proxyLoadWithSize(const string& file, 
                                              const VECTOR2<size_t>& size)
{
  if (m2DTrans == NULL) return false;
  return m2DTrans->Load(file, size);
}

//------------------------------------------------------------------------------
VECTOR2<size_t> LuaTransferFun2DProxy::proxyGetSize()
{
  if (m2DTrans == NULL) return VECTOR2<size_t>();
  return m2DTrans->GetSize();
}

//------------------------------------------------------------------------------
VECTOR2<size_t> LuaTransferFun2DProxy::proxyGetRenderSize()
{
  if (m2DTrans == NULL) return VECTOR2<size_t>();
  return m2DTrans->GetRenderSize();
}

//------------------------------------------------------------------------------
bool LuaTransferFun2DProxy::proxySave(const std::string& file)
{
  if (m2DTrans == NULL) return false;
  return m2DTrans->Save(file);
}

//------------------------------------------------------------------------------
void LuaTransferFun2DProxy::proxyUpdate1DTrans(LuaClassInstance tf1d)
{
  if (m2DTrans == NULL) return;

  // Extract TransferFunction1D pointer from LuaClassInstance.
  LuaTransferFun1DProxy* tfProxy = 
      tf1d.getRawPointer_NoSharedPtr<LuaTransferFun1DProxy>(mSS);
  const TransferFunction1D* p1DTrans = tfProxy->get1DTransferFunction();

  m2DTrans->Update1DTrans(p1DTrans);
}

