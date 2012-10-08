/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2012 Scientific Computing and Imaging Institute,
   University of Utah.

   License for the specific language governing rights and limitations under
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

#include "LuaTuvokTypes.h"

namespace tuvok {

VECTOR4<lua_Number> luaMakeV4(float x, float y, float z, float w)
{
  // Create the vector 4 and push it onto the lua stack.
  VECTOR4<lua_Number> v(x,y,z,w);
  return v;
}

MATRIX4<lua_Number> luaMakeM44()
{
  // Default constructor constructs an identity (see Tuvok/Basics/Vectors.h...)
  // matrix.
  MATRIX4<lua_Number> m;
  return m;
}

void LuaMathFunctions::registerMathFunctions(std::shared_ptr<LuaScripting> ss)
{
  ss->registerFunction(luaMakeV4, "math.v4", "Generates a numeric vector4.", 
                       false);
  ss->registerFunction(luaMakeM44, "math.m4x4", "Generates a numeric matrix4.",
                       false);
}

bool LuaMathFunctions::isOfType(lua_State* L, int object, int mt)
{
    LuaStackRAII _a(L, 0, 0);

    // Push our metatable onto the top of the stack.
    int ourMT = mt;

    // Grab the metatable of the object at 'index'.
    if (lua_getmetatable(L, object) == 0)
    {
      return false;
    }
    int theirMT = lua_gettop(L);

    // Test to see if the metatable of the type given at index is identical
    // to our types metatable (they will only be identical if they are the
    // same type).
    if (lua_rawequal(L, ourMT, theirMT) == 1)
    {
      lua_pop(L, 1);
      return true;
    }
    else
    {
      lua_pop(L, 1);
      return false;
    }
}

} // tuvok namespace

