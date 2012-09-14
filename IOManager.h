/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2008 Scientific Computing and Imaging Institute,
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
  \file    IOManager.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \version 1.0
  \date    August 2008
*/
#pragma once

#ifndef IOMANAGER_H
#define IOMANAGER_H

#include "StdTuvokDefines.h"
#include <algorithm>
#include <fstream>
#include <functional>
#include <limits>
#include <list>
#include <memory>
#include <string>
#include <tuple>
#include "Basics/TuvokException.h"
#include "Basics/Vectors.h"

typedef std::tuple<std::string,std::string,bool,bool> tConverterFormat;

class AbstrConverter;
class FileStackInfo;
class RangeInfo;
class UVF;
class GeometryDataBlock;
class TransferFunction1D;

namespace tuvok {
  class AbstrGeoConverter;
  class AbstrRenderer;
  class Dataset;
  class FileBackedDataset;
  class Mesh;
  class UVFDataset;
  class MasterController;
  namespace io {
    class DSFactory;
  }
}

class MergeDataset {
public:
  MergeDataset(std::string _strFilename="", uint64_t _iHeaderSkip=0, bool _bDelete=false,
               double _fScale=1.0, double _fBias=0.0) :
    strFilename(_strFilename),
    iHeaderSkip(_iHeaderSkip),
    bDelete(_bDelete),
    fScale(_fScale),
    fBias(_fBias)
  {}

  std::string strFilename;
  uint64_t iHeaderSkip;
  bool bDelete;
  double fScale;
  double fBias;
};

class IOManager {
public:
  IOManager();
  ~IOManager();

  std::vector<std::shared_ptr<FileStackInfo> >
    ScanDirectory(std::string strDirectory) const;
  bool ConvertDataset(FileStackInfo* pStack,
                      const std::string& strTargetFilename,
                      const std::string& strTempDir,
                      const uint64_t iMaxBrickSize,
                      uint64_t iBrickOverlap,
                      bool bQuantizeTo8Bit=false) const;
  bool ConvertDataset(const std::string& strFilename,
                      const std::string& strTargetFilename,
                      const std::string& strTempDir,
                      const bool bNoUserInteraction,
                      const uint64_t iMaxBrickSize,
                      uint64_t iBrickOverlap,
                      bool bQuantizeTo8Bit=false) const;
  bool ConvertDataset(const std::list<std::string>& files,
                      const std::string& strTargetFilename,
                      const std::string& strTempDir,
                      const bool bNoUserInteraction,
                      const uint64_t iMaxBrickSize,
                      uint64_t iBrickOverlap,
                      bool bQuantizeTo8Bit=false) const;
  bool MergeDatasets(const std::vector <std::string>& strFilenames,
                     const std::vector <double>& vScales,
                     const std::vector<double>& vBiases,
                     const std::string& strTargetFilename,
                     const std::string& strTempDir,
                     bool bUseMaxMode=true,
                     bool bNoUserInteraction=false) const;
  tuvok::UVFDataset* ConvertDataset(FileStackInfo* pStack,
                                    const std::string& strTargetFilename,
                                    const std::string& strTempDir,
                                    tuvok::AbstrRenderer* requester,
                                    const uint64_t iMaxBrickSize,
                                    uint64_t iBrickOverlap,
                                    bool bQuantizeTo8Bit=false) const;
  tuvok::UVFDataset* ConvertDataset(const std::string& strFilename,
                                    const std::string& strTargetFilename,
                                    const std::string& strTempDir,
                                    tuvok::AbstrRenderer* requester,
                                    const uint64_t iMaxBrickSize,
                                    uint64_t iBrickOverlap,
                                    bool bQuantizeTo8Bit=false) const;

  /// evaluates the given expression. v[n] in the expression refers to
  /// the volume given by volumes[n].
  void EvaluateExpression(const char* expr,
                          const std::vector<std::string> volumes,
                          const std::string& out_fn) const
                          throw(tuvok::Exception);

  bool ReBrickDataset(const std::string& strSourceFilename,
                      const std::string& strTargetFilename,
                      const std::string& strTempDir,
                      const uint64_t iMaxBrickSize,
                      const uint64_t iBrickOverlap,
                      bool bQuantizeTo8Bit=false) const;

  // convenience calls that use the default bricksizes and overlaps
  tuvok::UVFDataset* ConvertDataset(FileStackInfo* pStack,
                                    const std::string& strTargetFilename,
                                    const std::string& strTempDir,
                                    tuvok::AbstrRenderer* requester,
                                    const bool bQuantizeTo8Bit=false) const {
    return ConvertDataset(pStack, strTargetFilename, strTempDir, requester,
                          m_iBuilderBrickSize, m_iBrickOverlap, bQuantizeTo8Bit);
  }
  tuvok::UVFDataset* ConvertDataset(const std::string& strFilename,
                                    const std::string& strTargetFilename,
                                    const std::string& strTempDir,
                                    tuvok::AbstrRenderer* requester,
                                    const bool bQuantizeTo8Bit=false) {
    return ConvertDataset(strFilename, strTargetFilename, strTempDir, requester,
                          m_iBuilderBrickSize, m_iBrickOverlap,bQuantizeTo8Bit);
  }
  bool ConvertDataset(FileStackInfo* pStack,
                      const std::string& strTargetFilename,
                      const std::string& strTempDir,
                      const bool bQuantizeTo8Bit=false) const{
    return ConvertDataset(pStack, strTargetFilename, strTempDir, m_iBuilderBrickSize,
                          m_iBrickOverlap, bQuantizeTo8Bit);
  }
  bool ConvertDataset(const std::list<std::string>& files,
                      const std::string& strTargetFilename,
                      const std::string& strTempDir,
                      const bool bNoUserInteraction=false,
                      const bool bQuantizeTo8Bit=false) const {
    return ConvertDataset(files, strTargetFilename, strTempDir, bNoUserInteraction,
                          m_iBuilderBrickSize,m_iBrickOverlap, bQuantizeTo8Bit);
  }
  bool ReBrickDataset(const std::string& strSourceFilename,
                      const std::string& strTargetFilename,
                      const std::string& strTempDir,
                      bool bQuantizeTo8Bit=false) const {
    return ReBrickDataset(strSourceFilename, strTargetFilename, strTempDir,
                          m_iBuilderBrickSize,m_iBrickOverlap,bQuantizeTo8Bit);
  }

  std::shared_ptr<tuvok::Mesh> LoadMesh(const std::string& meshfile) const;

  void AddMesh(const UVF* sourceDataset,
                  const std::string& trisoup_file,
                  const std::string& uvf) const;

  ///@{
  /// We jump into the memory manager to load a data set, but the memory
  /// manager is defined elsewhere.  This sets/uses the callback to utilize the
  /// memory manager to call us.  The MMgr will in turn use the DSFactory from
  /// here to actually create the data set.
  void SetMemManLoadFunction(
    std::function<tuvok::Dataset*(const std::string&,
                                  tuvok::AbstrRenderer*)>& f
  );
  tuvok::Dataset* LoadDataset(const std::string& strFilename,
                              tuvok::AbstrRenderer* requester) const;
  ///@}
  tuvok::Dataset* CreateDataset(const std::string& filename,
                                uint64_t max_brick_size, bool verify) const;
  void AddReader(std::shared_ptr<tuvok::FileBackedDataset>);
  bool AnalyzeDataset(const std::string& strFilename, RangeInfo& info,
                      const std::string& strTempDir) const;
  bool NeedsConversion(const std::string& strFilename) const;
  bool Verify(const std::string& strFilename) const;

  bool ExportMesh(const std::shared_ptr<tuvok::Mesh> mesh, 
                  const std::string& strTargetFilename);
  bool ExportDataset(const tuvok::UVFDataset* pSourceData, uint64_t iLODlevel,
                     const std::string& strTargetFilename,
                     const std::string& strTempDir) const;
  bool ExtractIsosurface(const tuvok::UVFDataset* pSourceData,
                         uint64_t iLODlevel, double fIsovalue,
                         const FLOATVECTOR4& vfColor,
                         const std::string& strTargetFilename,
                         const std::string& strTempDir) const;
  bool ExtractImageStack(const tuvok::UVFDataset* pSourceData,
                         const TransferFunction1D* pTrans,
                         uint64_t iLODlevel, 
                         const std::string& strTargetFilename,
                         const std::string& strTempDir,
                         bool bAllDirs) const;


  void RegisterExternalConverter(AbstrConverter* pConverter);
  void RegisterFinalConverter(AbstrConverter* pConverter);

  std::string GetLoadDialogString() const;
  std::string GetExportDialogString() const;
  std::string GetImageExportDialogString() const;


  std::vector< std::pair <std::string, std::string > >
    GetImportFormatList() const;
  std::vector< std::pair <std::string, std::string > >
    GetExportFormatList() const;
  std::vector< tConverterFormat > GetFormatList() const;
  AbstrConverter* GetConverterForExt(std::string ext,
                                     bool bMustSupportExport,
                                     bool bMustSupportImport) const;

  std::string GetLoadGeoDialogString() const;
  std::string GetGeoExportDialogString() const;
  std::vector< std::pair <std::string, std::string > >
    GetGeoImportFormatList() const;
  std::vector< std::pair <std::string, std::string > >
    GetGeoExportFormatList() const;
  std::vector< tConverterFormat > GetGeoFormatList() const;
  tuvok::AbstrGeoConverter* GetGeoConverterForExt(std::string ext,
                                                  bool bMustSupportExport,
                                                  bool bMustSupportImport) const;


  uint64_t GetMaxBrickSize() const {return m_iMaxBrickSize;}
  uint64_t GetBuilderBrickSize() const {return m_iBuilderBrickSize;}
  uint64_t GetBrickOverlap() const {return m_iBrickOverlap;}
  uint64_t GetIncoresize() const {return m_iIncoresize;}

  bool SetMaxBrickSize(const uint64_t iMaxBrickSize, const uint64_t iBuilderBrickSize);
  bool SetBrickOverlap(const uint64_t iBrickOverlap);

  void SetUseMedianFilter(bool bUseMedianFilter) {
    m_bUseMedianFilter = bUseMedianFilter;
  }
  bool GetUseMedianFilter() const {
    return m_bUseMedianFilter;
  }
  
  void SetClampToEdge(bool bClampToEdge) {
    m_bClampToEdge = bClampToEdge;
  }
  bool GetClampToEdge() const {
    return m_bClampToEdge;
  }


private:
  std::vector<tuvok::AbstrGeoConverter*> m_vpGeoConverters;
  std::vector<AbstrConverter*>    m_vpConverters;
  AbstrConverter*                 m_pFinalConverter;
  std::auto_ptr<tuvok::io::DSFactory> m_dsFactory;

  uint64_t m_iMaxBrickSize;
  uint64_t m_iBuilderBrickSize;
  uint64_t m_iBrickOverlap;
  uint64_t m_iIncoresize;
  bool m_bUseMedianFilter;
  bool m_bClampToEdge;
  std::function<tuvok::Dataset* (const std::string&,
                                 tuvok::AbstrRenderer*)> m_LoadDS;

  void CopyToTSB(const tuvok::Mesh& m, GeometryDataBlock* tsb) const;
};

#endif // IOMANAGER_H
