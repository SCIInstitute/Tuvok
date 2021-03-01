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
  \date    August 2008
*/
#pragma once

#ifndef IOMANAGER_H
#define IOMANAGER_H

#include "StdTuvokDefines.h"
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <tuple>
#include <vector>
#include "Basics/TuvokException.h"
#include "Basics/Vectors.h"

typedef std::tuple<std::wstring,std::wstring,bool,bool> tConverterFormat;

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
  namespace io {
    class DSFactory;
  }
}

class IOManager {
public:
  IOManager();
  ~IOManager();

  std::vector<std::shared_ptr<FileStackInfo>>
    ScanDirectory(std::wstring strDirectory) const;
  bool ConvertDataset(FileStackInfo* pStack,
                      const std::wstring& strTargetFilename,
                      const std::wstring& strTempDir,
                      const uint64_t iMaxBrickSize,
                      uint64_t iBrickOverlap,
                      bool bQuantizeTo8Bit=false) const;
  bool ConvertDataset(const std::wstring& strFilename,
                      const std::wstring& strTargetFilename,
                      const std::wstring& strTempDir,
                      const bool bNoUserInteraction,
                      const uint64_t iMaxBrickSize,
                      uint64_t iBrickOverlap,
                      bool bQuantizeTo8Bit=false) const;
  bool ConvertDataset(const std::list<std::wstring>& files,
                      const std::wstring& strTargetFilename,
                      const std::wstring& strTempDir,
                      const bool bNoUserInteraction,
                      const uint64_t iMaxBrickSize,
                      uint64_t iBrickOverlap,
                      bool bQuantizeTo8Bit=false) const;
  bool MergeDatasets(const std::vector<std::wstring>& strFilenames,
                     const std::vector<double>& vScales,
                     const std::vector<double>& vBiases,
                     const std::wstring& strTargetFilename,
                     const std::wstring& strTempDir,
                     bool bUseMaxMode=true,
                     bool bNoUserInteraction=false) const;

  /// evaluates the given expression. v[n] in the expression refers to
  /// the volume given by volumes[n].
  void EvaluateExpression(const std::string& expr,
                          const std::vector<std::wstring>& volumes,
                          const std::wstring& out_fn) const
                          /* throw(tuvok::Exception) */;

  bool ReBrickDataset(const std::wstring& strSourceFilename,
                      const std::wstring& strTargetFilename,
                      const std::wstring& strTempDir,
                      const uint64_t iMaxBrickSize,
                      const uint64_t iBrickOverlap,
                      bool bQuantizeTo8Bit=false) const;

  bool ConvertDataset(FileStackInfo* pStack,
                      const std::wstring& strTargetFilename,
                      const std::wstring& strTempDir,
                      const bool bQuantizeTo8Bit=false) const{
    return ConvertDataset(pStack, strTargetFilename, strTempDir,
                          m_iBuilderBrickSize, m_iBrickOverlap,
                          bQuantizeTo8Bit);
  }
  bool ConvertDataset(const std::list<std::wstring>& files,
                      const std::wstring& strTargetFilename,
                      const std::wstring& strTempDir,
                      const bool bNoUserInteraction=false,
                      const bool bQuantizeTo8Bit=false) const {
    return ConvertDataset(files, strTargetFilename, strTempDir,
                          bNoUserInteraction, m_iBuilderBrickSize,
                          m_iBrickOverlap, bQuantizeTo8Bit);
  }

  std::shared_ptr<tuvok::Mesh> LoadMesh(const std::wstring& meshfile) const;

  void AddMesh(const UVF* sourceDataset,
               const std::wstring& trisoup_file,
               const std::wstring& uvf) const;

  ///@{
  /// We jump into the memory manager to load a data set, but the memory
  /// manager is defined elsewhere.  This sets/uses the callback to utilize the
  /// memory manager to call us.  The MMgr will in turn use the DSFactory from
  /// here to actually create the data set.
  void SetMemManLoadFunction(
    std::function<tuvok::Dataset*(const std::wstring&,
                                  tuvok::AbstrRenderer*)>& f
  );
  tuvok::Dataset* LoadDataset(const std::wstring& strFilename,
                              tuvok::AbstrRenderer* requester) const;

  /// @param filename the data to load
  /// @param the bricksize we should rebrick into
  /// @param minmaxType how we should handle brick min/maxes. 0=use the source
  /// dataset, 1=precompute on load (big delay), 2=compute on demand
  tuvok::Dataset* LoadRebrickedDataset(const std::wstring& filename,
                                       const UINTVECTOR3 bricksize,
                                       size_t minmaxType) const;
  ///@}
  tuvok::Dataset* CreateDataset(const std::wstring& filename,
                                uint64_t max_brick_size, bool verify) const;
  void AddReader(std::shared_ptr<tuvok::FileBackedDataset>);
  bool AnalyzeDataset(const std::wstring& strFilename, RangeInfo& info,
                      const std::wstring& strTempDir) const;
  bool NeedsConversion(const std::wstring& strFilename) const;
  bool Verify(const std::wstring& strFilename) const;

  bool ExportMesh(const std::shared_ptr<tuvok::Mesh> mesh, 
                  const std::wstring& strTargetFilename);
  bool ExportDataset(const tuvok::UVFDataset* pSourceData, uint64_t iLODlevel,
                     const std::wstring& strTargetFilename,
                     const std::wstring& strTempDir) const;
  bool ExtractIsosurface(const tuvok::UVFDataset* pSourceData,
                         uint64_t iLODlevel, double fIsovalue,
                         const FLOATVECTOR4& vfColor,
                         const std::wstring& strTargetFilename,
                         const std::wstring& strTempDir) const;
  bool ExtractImageStack(const tuvok::UVFDataset* pSourceData,
                         const TransferFunction1D* pTrans,
                         uint64_t iLODlevel, 
                         const std::wstring& strTargetFilename,
                         const std::wstring& strTempDir,
                         bool bAllDirs) const;


  void RegisterExternalConverter(std::shared_ptr<AbstrConverter> pConverter);
  void RegisterFinalConverter(std::shared_ptr<AbstrConverter> pConverter);

  std::wstring GetLoadDialogString() const;
  std::wstring GetExportDialogString() const;
  std::wstring ExportDialogFilterToExt(const std::wstring& filter) const;
  std::wstring GetImageExportDialogString() const;
  std::wstring ImageExportDialogFilterToExt(const std::wstring& filter) const;


  std::vector<std::pair<std::wstring, std::wstring>>
    GetImportFormatList() const;
  std::vector<std::pair<std::wstring, std::wstring>>
    GetExportFormatList() const;
  std::vector<tConverterFormat> GetFormatList() const;
  bool HasConverterForExt(std::wstring ext,
                          bool bMustSupportExport,
                          bool bMustSupportImport) const {
    return (GetConverterForExt(ext,bMustSupportExport,bMustSupportImport)!=NULL);
  }
  std::shared_ptr<AbstrConverter> GetConverterForExt(std::wstring ext,
                                     bool bMustSupportExport,
                                     bool bMustSupportImport) const;

  std::wstring GetLoadGeoDialogString() const;
  std::wstring GetGeoExportDialogString() const;
  std::vector<std::pair<std::wstring, std::wstring>>
    GetGeoImportFormatList() const;
  std::vector<std::pair<std::wstring, std::wstring>>
    GetGeoExportFormatList() const;
  std::vector<tConverterFormat> GetGeoFormatList() const;
  bool HasGeoConverterForExt(std::wstring ext,
                             bool bMustSupportExport,
                             bool bMustSupportImport) const {
    return GetGeoConverterForExt(ext, bMustSupportExport, bMustSupportImport)
        != NULL;
  }
  tuvok::AbstrGeoConverter* GetGeoConverterForExt(std::wstring ext,
                                                  bool bMustSupportExport,
                                                  bool bMustSupportImport) const;


  uint64_t GetMaxBrickSize() const {return m_iMaxBrickSize;}
  uint64_t GetBuilderBrickSize() const {return m_iBuilderBrickSize;}
  uint64_t GetBrickOverlap() const {return m_iBrickOverlap;}
  uint64_t GetIncoresize() const {return m_iIncoresize;}

  bool SetMaxBrickSize(uint64_t iMaxBrickSize, uint64_t iBuilderBrickSize);
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

  void SetCompression(uint32_t iCompression) {
    m_iCompression = iCompression;
  }

  void SetCompressionLevel(uint32_t iCompressionLevel) {
    m_iCompressionLevel = iCompressionLevel;
  }

  void SetLayout(uint32_t iLayout) {
    m_iLayout = iLayout;
  }

  bool GetClampToEdge() const {
    return m_bClampToEdge;
  }

private:
  std::vector<tuvok::AbstrGeoConverter*>        m_vpGeoConverters;
  std::vector<std::shared_ptr<AbstrConverter>>  m_vpConverters;
  std::shared_ptr<AbstrConverter>               m_pFinalConverter;
  std::unique_ptr<tuvok::io::DSFactory>         m_dsFactory;

  uint64_t m_iMaxBrickSize;
  uint64_t m_iBuilderBrickSize;
  uint64_t m_iBrickOverlap;
  uint64_t m_iIncoresize;
  bool m_bUseMedianFilter;
  bool m_bClampToEdge;
  uint32_t m_iCompression;
  uint32_t m_iCompressionLevel;
  uint32_t m_iLayout;
  std::function<tuvok::Dataset* (const std::wstring&,
                                 tuvok::AbstrRenderer*)> m_LoadDS;

  void CopyToTSB(const tuvok::Mesh& m, GeometryDataBlock* tsb) const;
};

#endif // IOMANAGER_H
