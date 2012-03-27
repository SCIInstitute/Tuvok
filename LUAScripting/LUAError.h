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
  \file    LUAError.h
  \author  James Hughes
           SCI Institute
           University of Utah
  \date    Mar 21, 2012
  \brief   Defines exceptions thrown in the LUA scripting system.
*/

#ifndef LUAERROR_H_
#define LUAERROR_H_

namespace tuvok
{

/// Generic LUA error.
class LUAError : virtual public tuvok::Exception
{
public:

  explicit LUAError(const char* e, const char* where = NULL, size_t ln = 0)
    : tuvok::Exception(e, where, ln)
  {}

  virtual ~LUAError() throw() { }

};

/// Errors dealing with the LUA-Based function registration system.
class LUAFunBindError : virtual public LUAError
{
public:
  explicit LUAFunBindError(const char* e, const char* where = NULL, size_t ln = 0)
    : LUAError(e, where, ln)
  {}

  virtual ~LUAFunBindError() throw() { }
};


} /* namespace tuvok */

#endif /* LUAERROR_H_ */
