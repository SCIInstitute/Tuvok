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
 \brief   Provenance system composited inside of the LuaScripting class.
 */

#ifndef EXTERNAL_UNIT_TESTING

#include "Controller/Controller.h"
#include "3rdParty/LUA/lua.hpp"

#else

#include <assert.h>
#include "utestCommon.h"

#endif

#include <vector>

#include "LUAError.h"
#include "LUAFunBinding.h"
#include "LUAScripting.h"
#include "LUAProvenance.h"

using namespace std;

namespace tuvok
{

//-----------------------------------------------------------------------------
LuaProvenance::LuaProvenance(LuaScripting* scripting)
: mEnabled(true)
, mScripting(scripting)
{
  mUndoStack.reserve(50);
  mRedoStack.reserve(50);
}

//-----------------------------------------------------------------------------
LuaProvenance::~LuaProvenance()
{

}

//-----------------------------------------------------------------------------
bool LuaProvenance::isEnabled()
{
  return mEnabled;
}


//-----------------------------------------------------------------------------
void LuaProvenance::setEnabled(bool enabled)
{
  // Clear undo/redo stacks. They won't make any sense after disabled.
  // In reality, we should also clear the last execs to defaults.
}

//-----------------------------------------------------------------------------
void LuaProvenance::logProvenance(string function,
                                  tr1::shared_ptr<LuaCFunAbstract> funParams)
{

}

//-----------------------------------------------------------------------------
void LuaProvenance::issueUndo()
{

}

//-----------------------------------------------------------------------------
void LuaProvenance::issueRedo()
{

}

//==============================================================================
//
// UNIT TESTING
//
//==============================================================================


SUITE(LuaProvenanceTests)
{
  TEST(Provenance)
  {
    TEST_HEADER;

  }

  TEST(UndoRedo)
  {
    TEST_HEADER;

  }
}


}
