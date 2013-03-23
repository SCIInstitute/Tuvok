#ifndef TUVOK_LUA_MATRIX_MATH
#define TUVOK_LUA_MATRIX_MATH

#include <memory>

namespace tuvok {

class LuaScripting;

namespace registrar {

/// Registers a set of matrix-related functions into Lua.
///   matrix.rotateX -- returns matrix rotated N degrees around X
///   matrix.rotateY -- returns matrix rotated N degrees around Y
///   matrix.rotateZ -- returns matrix rotated N degrees around Z
///   matrix.translate -- returns matrix translated by X,Y,Z
///   matrix.identity -- returns identity matrix
///   matrix.multiply -- multiplies two matrices
///   strvec -- converts a FLOATVECTOR3 to a string
void matrix_math(std::shared_ptr<LuaScripting>&);

} }
#endif
/*
 For more information, please see: http://software.sci.utah.edu

 The MIT License

 Copyright (c) 2013 IVDA Group

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
