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
 \brief A Lua class proxy for IO's TransferFunction1D class.
 */

#include <vector>
#include "Controller/Controller.h"
#include "IO/TransferFunction1D.h"
#include "3rdParty/LUA/lua.hpp"
#include "../LuaScripting.h"
#include "../LuaClassRegistration.h"
#include "LuaTuvokTypes.h"
#include "LuaTransferFun1DProxy.h"

using namespace tuvok;

//------------------------------------------------------------------------------
LuaTransferFun1DProxy::LuaTransferFun1DProxy()
  : mReg(NULL),
    m1DTrans(NULL)
{
}

//------------------------------------------------------------------------------
LuaTransferFun1DProxy::~LuaTransferFun1DProxy()
{
  if (mReg != NULL)
    delete mReg;
}

//------------------------------------------------------------------------------
void LuaTransferFun1DProxy::bind(TransferFunction1D* tf)
{
  if (mReg == NULL)
    throw LuaError("Unable to bind transfer function 1D , no class registration"
                   " available.");

  mReg->clearProxyFunctions();

  m1DTrans = tf;
  if (tf != NULL)
  {
    // Register TransferFunction1D functions using tf.
    std::string id;
    id = mReg->functionProxy(tf, &TransferFunction1D::GetSize,
                             "getSize", "", false);
    /// Note: Lua could accept the vector<FLOATVECTOR4> datatype, mutate it,
    /// and return it back to the transfer function.
    id = mReg->functionProxy(tf, &TransferFunction1D::GetColorData,
                             "getColorData", "Retrieves mutable color data. "
                             "This function is for C++ use only, to change "
                             "xfer function data in Lua, use getColor and "
                             "setColor.",
                             false);
    id = mReg->functionProxy(tf, &TransferFunction1D::GetColor,
                             "getColor", "Retrieves the color at 'index'.",
                             false);
    id = mReg->functionProxy(tf, &TransferFunction1D::SetColor,
                             "setColor", "Sets the color at 'index'.",
                             true);

    /// @todo In order for provenance to work, we need to always call SetColor
    ///       or have another function that sets all of the color data at once
    ///       (setting all of the color data at once is the more efficient route
    ///        and would transfer into a Lua setting quite easily).
  }
}

//------------------------------------------------------------------------------
void LuaTransferFun1DProxy::defineLuaInterface(
    LuaClassRegistration<LuaTransferFun1DProxy>& reg,
    LuaTransferFun1DProxy* me,
    LuaScripting*)
{
  me->mReg = new LuaClassRegistration<LuaTransferFun1DProxy>(reg);

  /// @todo Determine if the following function should be provenance enabled.
  reg.function(&LuaTransferFun1DProxy::proxyLoadWithFilenameAndSize,
               "loadFromFileWithSize", "Loads 'file' into the 1D "
               " transfer function with 'size'.", false);
  /// @todo Determine if the following function should be provenance enabled.
  reg.function(&LuaTransferFun1DProxy::proxySetStdFunction,
               "setStdFunction", "", true);
  reg.function(&LuaTransferFun1DProxy::proxySave,
               "save", "", false);
}

//------------------------------------------------------------------------------
bool LuaTransferFun1DProxy::proxyLoadWithFilenameAndSize(
    const std::string& file, size_t size)
{
  if (m1DTrans == NULL) return false;
  return m1DTrans->Load(file, size);
}

//------------------------------------------------------------------------------
void LuaTransferFun1DProxy::proxySetStdFunction(
    float centerPoint, float invGradient, float component, bool invertedStep)
{
  if (m1DTrans == NULL) return;
  m1DTrans->SetStdFunction(centerPoint, invGradient, component, invertedStep);
}

//------------------------------------------------------------------------------
bool LuaTransferFun1DProxy::proxySave(const std::string& filename) const
{
  if (m1DTrans == NULL) return false;
  return m1DTrans->Save(filename);
}
