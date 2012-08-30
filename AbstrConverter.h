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
  \file    AbstrConverter.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \version 1.0
  \date    December 2008
*/
#pragma once

#ifndef ABSTRCONVERTER_H
#define ABSTRCONVERTER_H

#include "../StdTuvokDefines.h"
#include <map>
#include <sstream>

#include "Basics/Vectors.h"
#include "Controller/Controller.h"
#include "UVF/UVFTables.h"
#include "UVF/Histogram1DDataBlock.h"

class RangeInfo {
public:
  UINT64VECTOR3                 m_vDomainSize;
  FLOATVECTOR3                  m_vAspect;
  uint64_t                      m_iComponentSize;
  int                           m_iValueType;
  std::pair<double, double>     m_fRange;
  std::pair<int64_t, int64_t>   m_iRange;
  std::pair<uint64_t, uint64_t> m_uiRange;
};

class AbstrConverter {
public:
  virtual ~AbstrConverter() {}

  virtual bool ConvertToUVF(const std::string& strSourceFilename,
                            const std::string& strTargetFilename,
                            const std::string& strTempDir,
                            const bool bNoUserInteraction,
                            const uint64_t iTargetBrickSize,
                            const uint64_t iTargetBrickOverlap,
                            const bool bUseMedian,
                            const bool bClampToEdge,
                            const bool bQuantizeTo8Bit) = 0;

  virtual bool ConvertToUVF(const std::list<std::string>& files,
                            const std::string& strTargetFilename,
                            const std::string& strTempDir,
                            const bool bNoUserInteraction,
                            const uint64_t iTargetBrickSize,
                            const uint64_t iTargetBrickOverlap,
                            const bool bUseMedian,
                            const bool bClampToEdge,
                            const bool bQuantizeTo8Bit) = 0;

  virtual bool ConvertToRAW(const std::string& strSourceFilename,
                            const std::string& strTempDir,
                            bool bNoUserInteraction,
                            uint64_t& iHeaderSkip, uint64_t& iComponentSize,
                            uint64_t& iComponentCount,
                            bool& bConvertEndianess, bool& bSigned,
                            bool& bIsFloat, UINT64VECTOR3& vVolumeSize,
                            FLOATVECTOR3& vVolumeAspect, std::string& strTitle,
                            UVFTables::ElementSemanticTable& eType,
                            std::string& strIntermediateFile,
                            bool& bDeleteIntermediateFile) = 0;

  virtual bool ConvertToNative(const std::string& strRawFilename,
                               const std::string& strTargetFilename,
                               uint64_t iHeaderSkip, uint64_t iComponentSize,
                               uint64_t iComponentCount, bool bSigned,
                               bool bFloatingPoint,
                               UINT64VECTOR3 vVolumeSize,
                               FLOATVECTOR3 vVolumeAspect,
                               bool bNoUserInteraction,
                               const bool bQuantizeTo8Bit) = 0;

  virtual bool Analyze(const std::string& strSourceFilename,
                       const std::string& strTempDir,
                       bool bNoUserInteraction, RangeInfo& info) = 0;

  /// @param filename the file in question
  /// @param start the first few bytes of the file
  /// @return SupportedExtension() for the file's extension; ignores "start".
  virtual bool CanRead(const std::string& fn,
                       const std::vector<int8_t>& start) const;

  const std::vector<std::string>& SupportedExt() { return m_vSupportedExt; }
  virtual const std::string& GetDesc() { return m_vConverterDesc; }

  virtual bool CanExportData() const { return false; }
  virtual bool CanImportData() const { return true; }

  static bool Process8Bits(LargeRAWFile& InputData,
                           const std::string& strTargetFilename,
                           uint64_t iSize, bool bSigned,
                           Histogram1DDataBlock* Histogram1D=0);

  static bool QuantizeTo8Bit(LargeRAWFile& rawfile,
                             const std::string& strTargetFilename,
                             uint64_t iComponentSize, uint64_t iSize,
                             bool bSigned, bool bIsFloat,
                             Histogram1DDataBlock* Histogram1D=0);

  static size_t GetIncoreSize();

protected:
  /// @param ext the extension for the filename
  /// @return true if the filename is a supported extension for this converter
  bool SupportedExtension(const std::string& ext) const;

protected:
  std::string               m_vConverterDesc;
  std::vector<std::string>  m_vSupportedExt;
};

#endif // ABSTRCONVERTER_H
