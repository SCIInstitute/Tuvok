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
  \brief Lua class proxy for IO's IOManager.
  */

#ifndef TUVOK_LUAIOMANAGERPROXY_H_
#define TUVOK_LUAIOMANAGERPROXY_H_

#include <sstream>
#include <tuple>
#include <utility>
#include "Basics/Vectors.h"
#include "../LuaScripting.h"
#include "../LuaClassRegistration.h"
#include "../LuaMemberReg.h"

class FileStackInfo;

namespace tuvok
{

class Dataset;

class LuaIOManagerProxy
{
public:

  LuaIOManagerProxy(IOManager* ioman, std::shared_ptr<LuaScripting> ss);
  virtual ~LuaIOManagerProxy();

private:

  IOManager*                          mIO;
  LuaMemberReg                        mReg;
  std::shared_ptr<LuaScripting>       mSS;

  void bind();
  /// Proxy functions for IOManager. These functions exist because IO
  /// does not known about LuaScripting. 
  /// @{
  bool ExportDataset(LuaClassInstance ds,
                            uint64_t iLODlevel,
                            const std::string& strTargetFilename,
                            const std::string& strTempDir) const;

  bool ExtractIsosurface(
      LuaClassInstance ds,
      uint64_t iLODlevel, double fIsovalue,
      const FLOATVECTOR4& vfColor,
      const std::string& strTargetFilename,
      const std::string& strTempDir) const;

  bool ExtractImageStack(
      LuaClassInstance ds,
      LuaClassInstance tf1d,
      uint64_t iLODlevel, 
      const std::string& strTargetFilename,
      const std::string& strTempDir,
      bool bAllDirs) const;

  bool ExportMesh(std::shared_ptr<Mesh> mesh,
                  const std::string& strTargetFilename) const;
  bool ReBrickDataset(const std::string& strSourceFilename,
                      const std::string& strTargetFilename,
                      const std::string& strTempDir) const;
  bool ConvertDataset(const std::list<std::string>& files,
                      const std::string& strTargetFilename,
                      const std::string& strTempDir,
                      bool bNoUserInteraction,
                      bool bQuantizeTo8Bit);
  bool ConvertDatasetWithStack(
      std::shared_ptr<FileStackInfo> stack,
      const std::string& strTargetFilename,
      const std::string& strTempDir,
      bool bQuantizeTo8Bit);
  std::tuple<bool, RangeInfo> AnalyzeDataset(
      const std::string& strFilename, const std::string& strTempDir);
  /// @}  
};

// This proxy allows us to "easily" serialize to/from Lua.
struct TupleStructProxy {
  UINT64VECTOR3 domSize;
  FLOATVECTOR3 aspect;
  unsigned componentSize;
  int valueType;
  std::pair<double,double> fRange;
  std::pair<int64_t, int64_t> iRange;
  std::pair<uint64_t, uint64_t> uiRange;
};

template<>
class LuaStrictStack<TupleStructProxy> {
public:
  typedef TupleStructProxy Type;

  static TupleStructProxy get(lua_State* L, int pos) {
    LuaStackRAII a_(L, 14, 0);

    Type ret;

    // if there's not a table on the top of the stack, something is very wrong.
    luaL_checktype(L, pos, LUA_TTABLE);

    for(size_t i=0; i < 3; ++i) {
      lua_pushinteger(L, i+1);
      lua_gettable(L, pos);
      ret.domSize[i] = LuaStrictStack<uint64_t>::get(L, lua_gettop(L));
      lua_pop(L, 1);
    }
    for(size_t i=0; i < 3; ++i) {
      lua_pushinteger(L, 3+i+1);
      lua_gettable(L, pos);
      ret.aspect[i] = LuaStrictStack<float>::get(L, lua_gettop(L));
      lua_pop(L, 1);
    }

    lua_pushinteger(L, 7);
    lua_gettable(L, pos);
    ret.componentSize = LuaStrictStack<unsigned>::get(L, lua_gettop(L));

    lua_pushinteger(L, 8);
    lua_gettable(L, pos);
    ret.valueType = LuaStrictStack<int>::get(L, lua_gettop(L));

    lua_pushinteger(L, 9);
    lua_gettable(L, pos);
    ret.fRange.first = LuaStrictStack<double>::get(L, lua_gettop(L));
    lua_pushinteger(L, 10);
    lua_gettable(L, pos);
    ret.fRange.second = LuaStrictStack<double>::get(L, lua_gettop(L));

    lua_pushinteger(L, 11);
    lua_gettable(L, pos);
    ret.iRange.first = LuaStrictStack<int64_t>::get(L, lua_gettop(L));
    lua_pushinteger(L, 12);
    lua_gettable(L, pos);
    ret.iRange.second = LuaStrictStack<int64_t>::get(L, lua_gettop(L));

    lua_pushinteger(L, 13);
    lua_gettable(L, pos);
    ret.uiRange.first = LuaStrictStack<uint64_t>::get(L, lua_gettop(L));
    lua_pushinteger(L, 14);
    lua_gettable(L, pos);
    ret.uiRange.second = LuaStrictStack<uint64_t>::get(L, lua_gettop(L));

    return ret;
  }

  static void push(lua_State* L, TupleStructProxy data) {
    LuaStackRAII a_(L, 0, 14);

    lua_newtable(L);
    int tblposition = lua_gettop(L);

    for(size_t i=0; i < 3; ++i) {
      lua_pushinteger(L, i+1);
      LuaStrictStack<uint64_t>::push(L, data.domSize[i]);
      lua_settable(L, tblposition);
    }

    for(size_t i=0; i < 3; ++i) {
      lua_pushinteger(L, 3+i+1);
      LuaStrictStack<float>::push(L, data.aspect[i]);
      lua_settable(L, tblposition);
    }

    lua_pushinteger(L, 7);
    LuaStrictStack<unsigned>::push(L, data.componentSize);
    lua_settable(L, tblposition);

    lua_pushinteger(L, 8);
    LuaStrictStack<int>::push(L, data.valueType);
    lua_settable(L, tblposition);

    lua_pushinteger(L, 9);
    LuaStrictStack<double>::push(L, data.fRange.first);
    lua_settable(L, tblposition);
    lua_pushinteger(L, 10);
    LuaStrictStack<double>::push(L, data.fRange.second);
    lua_settable(L, tblposition);

    lua_pushinteger(L, 11);
    LuaStrictStack<int64_t>::push(L, data.iRange.first);
    lua_settable(L, tblposition);
    lua_pushinteger(L, 12);
    LuaStrictStack<int64_t>::push(L, data.iRange.second);
    lua_settable(L, tblposition);

    lua_pushinteger(L, 13);
    LuaStrictStack<uint64_t>::push(L, data.uiRange.first);
    lua_settable(L, tblposition);
    lua_pushinteger(L, 14);
    LuaStrictStack<uint64_t>::push(L, data.uiRange.second);
    lua_settable(L, tblposition);
  }

  static std::string getValStr(TupleStructProxy in) {
    std::ostringstream oss;
    oss << "{ "
        << "[ " << in.domSize[0] << ", " << in.domSize[1] << ", "
        << in.domSize[2]  << " ], ["
        << in.aspect[0] << ", " << in.aspect[1] << ", " << in.aspect[2] << "],"
        << " " << in.componentSize << ", " << in.valueType << ", ("
        << in.fRange.first << ", " << in.fRange.second << "), ("
        << in.iRange.first << ", " << in.iRange.second << "), ("
        << in.uiRange.first << ", " << in.uiRange.second << ") }";
    return oss.str();
  }
  static std::string getTypesStr() {
    return "TupleStructProxy";
  }
  static TupleStructProxy getDefault() { return TupleStructProxy(); }
};

template<>
class LuaStrictStack<RangeInfo>
{
public:

  typedef RangeInfo Type;

  static RangeInfo get(lua_State* L, int pos)
  {
    TupleStructProxy tupleProxy = LuaStrictStack<TupleStructProxy>::get(L, pos);

    RangeInfo ret;
    ret.m_vDomainSize     = tupleProxy.domSize;
    ret.m_vAspect         = tupleProxy.aspect;
    ret.m_iComponentSize  = tupleProxy.componentSize;
    ret.m_iValueType      = tupleProxy.valueType;
    ret.m_fRange          = tupleProxy.fRange;
    ret.m_iRange          = tupleProxy.iRange;
    ret.m_uiRange         = tupleProxy.uiRange;

    return ret;
  }

  static TupleStructProxy makeTupleFromStruct(RangeInfo in)
  {
    TupleStructProxy tupleProxy = {
      /* .domSize = */       in.m_vDomainSize, 
      /* .aspect = */        in.m_vAspect,
      /* .componentSize = */ in.m_iComponentSize,
      /* .valueType = */     in.m_iValueType,
      /* .fRange = */        in.m_fRange,
      /* .iRange = */        in.m_iRange,
      /* .uiRange = */       in.m_uiRange
    };
    return tupleProxy;
  }

  static void push(lua_State* L, RangeInfo in)
  {
    TupleStructProxy tupleProxy = makeTupleFromStruct(in);
    LuaStrictStack<TupleStructProxy>::push(L, tupleProxy);
  }

  static std::string getValStr(RangeInfo in)
  {
    
    TupleStructProxy tupleProxy = makeTupleFromStruct(in);
    return LuaStrictStack<TupleStructProxy>::getValStr(tupleProxy);
  }

  static std::string getTypeStr() { return "RangeInfo"; }
  static RangeInfo   getDefault() { return RangeInfo(); }
};

} /* namespace tuvok */
#endif /* TUVOK_LUAIOMANAGERPROXY_H_ */
