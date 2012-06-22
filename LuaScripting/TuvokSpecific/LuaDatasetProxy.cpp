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
 \brief A Lua class proxy for IO's dataset class.
 */

#include "Controller/Controller.h"
#include "3rdParty/LUA/lua.hpp"
#include "IO/IOManager.h"
#include "IO/FileBackedDataset.h"

#include <vector>

#include "../LuaScripting.h"
#include "../LuaClassRegistration.h"

#include "LuaTuvokTypes.h"

#include "LuaDatasetProxy.h"

namespace tuvok
{

LuaDatasetProxy::LuaDatasetProxy()
{
  mReg = NULL;
}

LuaDatasetProxy::~LuaDatasetProxy()
{
  if (mReg != NULL)
    delete mReg;
}

void LuaDatasetProxy::bindDataset(Dataset* ds)
{
  if (mReg == NULL)
    throw LuaError("Unable to bind dataset, no class registration available.");

  mReg->clearProxyFunctions();

  if (ds != NULL)
  {
    // Register dataset functions using ds.
    std::string id;

    //GetDomainSize
    id = mReg->functionProxy(ds, &Dataset::GetDomainSize,
                             "getDomainSize", "", false);

    // Attempt to cast the dataset to a file backed dataset.
    FileBackedDataset* fileDataset = dynamic_cast<FileBackedDataset*>(ds);
    if (fileDataset != NULL)
    {
      id = mReg->functionProxy(fileDataset, &FileBackedDataset::Filename,
                               "path", "Full path to the dataset.", false);


    }

  }

}

void LuaDatasetProxy::defineLuaInterface(
    LuaClassRegistration<LuaDatasetProxy>& reg,
    LuaDatasetProxy* me,
    LuaScripting*)
{
  me->mReg = new LuaClassRegistration<LuaDatasetProxy>(reg);
}

} /* namespace tuvok */
