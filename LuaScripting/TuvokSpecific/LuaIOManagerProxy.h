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
  
  /// The following evaluateExpression proxy was made because of the 
  /// "throw (tuvok::Exception)" exception specification on 
  /// IOManager::EvaluateExpression. 
  /// When MSVC 2010 attempts to bind IOManager::evaluateExpression, the 
  /// compiler uses a LuaCFunExec template specialization that specifies a 
  /// return type (even though this function is of void return type!).
  /// Does not happen on clang.
  void evaluateExpression(const std::string& expr,
                          const std::vector<std::string>& volumes,
                          const std::string& out_fn) const;
};

template<>
class LuaStrictStack<RangeInfo>
{
public:

  typedef RangeInfo Type;

  // This proxy allows us to easily serialize to/from Lua.
  // (we don't need to write any lua code, tuple handles it for us).
  typedef std::tuple<UINT64VECTOR3, FLOATVECTOR3, uint64_t, int, 
                     std::pair<double, double>, std::pair<int64_t, int64_t>,
                     std::pair<uint64_t, uint64_t>> TupleStructProxy;

  static RangeInfo get(lua_State* L, int pos)
  {
    TupleStructProxy tupleProxy = LuaStrictStack<TupleStructProxy>::get(L, pos);

    RangeInfo ret;
    ret.m_vDomainSize     = std::get<0>(tupleProxy);
    ret.m_vAspect         = std::get<1>(tupleProxy);
    ret.m_iComponentSize  = std::get<2>(tupleProxy);
    ret.m_iValueType      = std::get<3>(tupleProxy);
    ret.m_fRange          = std::get<4>(tupleProxy);
    ret.m_iRange          = std::get<5>(tupleProxy);
    ret.m_uiRange         = std::get<6>(tupleProxy);

    return ret;
  }

  static TupleStructProxy makeTupleFromStruct(RangeInfo in)
  {
    TupleStructProxy tupleProxy = make_tuple(in.m_vDomainSize, 
                                             in.m_vAspect,
                                             in.m_iComponentSize,
                                             in.m_iValueType,
                                             in.m_fRange,
                                             in.m_iRange,
                                             in.m_uiRange);
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
