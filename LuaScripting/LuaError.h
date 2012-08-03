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
  \file    LuaError.h
  \author  James Hughes
           SCI Institute
           University of Utah
  \date    Mar 21, 2012
  \brief   Defines exceptions thrown in the LUA scripting system.
*/

#ifndef TUVOK_LUAERROR_H_
#define TUVOK_LUAERROR_H_

#include <string>

#ifndef LUASCRIPTING_NO_TUVOK
#include "Basics/TuvokException.h"
#else
#include "NoTuvok/LuaTuvokException.h"
#endif

namespace tuvok
{

/// Generic Lua error.
class LuaError : public Exception
{
public:

  explicit LuaError(const char* e, const char* where = NULL, size_t ln = 0)
    : Exception(e, where, ln)
  {}
  virtual ~LuaError() throw() { }

};

/// Errors dealing with the Lua-Based function registration system.
class LuaFunBindError : public LuaError
{
public:
  explicit LuaFunBindError(const char* e, const char* where = NULL,
                           size_t ln = 0)
    : LuaError(e, where, ln)
  {}
  virtual ~LuaFunBindError() throw() { }
};

class LuaNonExistantFunction : public LuaError
{
public:
  explicit LuaNonExistantFunction(const char* e, const char* where = NULL,
                                  size_t ln = 0)
    : LuaError(e, where, ln)
  {}
  virtual ~LuaNonExistantFunction() throw() { }
};

class LuaInvalidFunSignature : public LuaError
{
public:
  explicit LuaInvalidFunSignature(const char* e, const char* where = NULL,
                                  size_t ln = 0)
    : LuaError(e, where, ln)
  {}
  virtual ~LuaInvalidFunSignature() throw() { }
};

class LuaProvenanceReenter : public LuaError
{
public:
  explicit LuaProvenanceReenter(const char* e, const char* where = NULL,
                                  size_t ln = 0)
    : LuaError(e, where, ln)
  {}
  virtual ~LuaProvenanceReenter() throw() { }
};

class LuaProvenanceInvalidUndoOrRedo : public LuaError
{
public:
  explicit LuaProvenanceInvalidUndoOrRedo(const char* e,
                                          const char* where = NULL,
                                          size_t ln = 0)
    : LuaError(e, where, ln)
  {}
  virtual ~LuaProvenanceInvalidUndoOrRedo() throw() { }
};

class LuaProvenanceInvalidRedo : public LuaError
{
public:
  explicit LuaProvenanceInvalidRedo(const char* e, const char* where = NULL,
                                    size_t ln = 0)
    : LuaError(e, where, ln)
  {}
  virtual ~LuaProvenanceInvalidRedo() throw() { }
};

class LuaProvenanceInvalidUndo : public LuaError
{
public:
  explicit LuaProvenanceInvalidUndo(const char* e, const char* where = NULL,
                                    size_t ln = 0)
    : LuaError(e, where, ln)
  {}
  virtual ~LuaProvenanceInvalidUndo() throw() { }
};

class LuaProvenanceFailedUndo : public LuaError
{
public:
  explicit LuaProvenanceFailedUndo(const char* e, const char* where = NULL,
                                   size_t ln = 0)
    : LuaError(e, where, ln)
  {}
  virtual ~LuaProvenanceFailedUndo() throw() { }
};

class LuaInvalidType : public LuaError
{
public:
  explicit LuaInvalidType(const char* e, const char* where = NULL,
                          size_t ln = 0)
    : LuaError(e, where, ln)
  {}
  virtual ~LuaInvalidType() throw() { }
};

class LuaUnequalNumParams : public LuaError
{
public:
  explicit LuaUnequalNumParams(const char* e, const char* where = NULL,
                               size_t ln = 0)
    : LuaError(e, where, ln)
  {}
  virtual ~LuaUnequalNumParams() throw() { }
};

class LuaUndoFuncAlreadySet : public LuaError {
public:
  explicit LuaUndoFuncAlreadySet(const char* e, const char* where = NULL,
                                 size_t ln = 0) : LuaError(e, where, ln) {}
};

class LuaRedoFuncAlreadySet : public LuaError {
public:
  explicit LuaRedoFuncAlreadySet(const char* e, const char* where = NULL,
                                 size_t ln = 0) : LuaError(e, where, ln) {}
};

class LuaNonExistantClassInstancePointer : public LuaError {
public:
  explicit LuaNonExistantClassInstancePointer(const char* e,
                                              const char* where = NULL,
                                              size_t ln = 0)
  : LuaError(e, where, ln) {}
};

} /* namespace tuvok */

#endif /* LUAERROR_H_ */
