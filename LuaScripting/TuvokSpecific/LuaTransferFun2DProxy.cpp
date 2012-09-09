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

using namespace tuvok;

//------------------------------------------------------------------------------
LuaTransferFun2DProxy::LuaTransferFun2DProxy()
  : mReg(NULL),
    m2DTrans(NULL)
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
    id = mReg->functionProxy(tf, &TransferFunction2D::SwatchIsRadial,
                             "swatchIsRadial", "", false);
    id = mReg->functionProxy(tf, &TransferFunction2D::SwatchGetNumPoints,
                             "swatchGetNumPoints", "", false);
    id = mReg->functionProxy(tf, &TransferFunction2D::SwatchGetGradientCount,
                             "swatchGetGradientCount", "", false);
    id = mReg->functionProxy(tf, &TransferFunction2D::SwatchGetGradient,
                             "swatchGetGradient", "", false);
  }
}

//------------------------------------------------------------------------------
void LuaTransferFun2DProxy::defineLuaInterface(
    LuaClassRegistration<LuaTransferFun2DProxy>& reg,
    LuaTransferFun2DProxy* me,
    LuaScripting*)
{
  me->mReg = new LuaClassRegistration<LuaTransferFun2DProxy>(reg);

  /// @todo Determine if the following function should be provenance enabled.
  //reg.function(&LuaTransferFun2DProxy::proxyLoadWithFilenameAndSize,
  //             "loadFromFileWithSize", "Loads 'file' into the 2D "
  //             " transfer function with 'size'.", false);
}

