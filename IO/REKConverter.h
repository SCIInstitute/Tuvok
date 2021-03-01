/*
   The MIT License

   Copyright (c) 2009 Institut of Mechanics and Fluid Dynamics,
   TU Bergakademie Freiberg.


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
  \file    REKConverter.h
  \author  Andre Liebscher
           Institut of Mechanics and Fluid Dynamics
           TU Bergakademie Freiberg
  \version 1.1
  \date    March 2009
*/
#pragma once

#ifndef REKCONVERTER_H
#define REKCONVERTER_H

#include "../StdTuvokDefines.h"
#include "RAWConverter.h"

/** A converter for REK volumes used by Frauenhofer EZRT.  */
class REKConverter : public RAWConverter {
public:
  REKConverter();
  virtual ~REKConverter() {}

  virtual bool ConvertToRAW(const std::wstring& strSourceFilename,
                            const std::wstring& strTempDir,
                            bool bNoUserInteraction,
                            uint64_t& iHeaderSkip, unsigned& iComponentSize,
                            uint64_t& iComponentCount, bool& bConvertEndianess,
                            bool& bSigned, bool& bIsFloat,
                            UINT64VECTOR3& vVolumeSize,
                            FLOATVECTOR3& vVolumeAspect, std::wstring& strTitle,
                            std::wstring& strIntermediateFile,
                            bool& bDeleteIntermediateFile);

  /// @todo unimplemented!
  virtual bool ConvertToNative(const std::wstring& strRawFilename,
                               const std::wstring& strTargetFilename,
                               uint64_t iHeaderSkip, unsigned iComponentSize,
                               uint64_t iComponentCount, bool bSigned,
                               bool bFloatingPoint,
                               UINT64VECTOR3 vVolumeSize,
                               FLOATVECTOR3 vVolumeAspect,
                               bool bNoUserInteraction,
                               const bool bQuantizeTo8Bit);

  virtual bool CanExportData() const {return false;}
  virtual bool CanImportData() const { return true; }

private:
  template<typename T, size_t N>
  T Parse( char data[], bool convertEndianess = false ) {
    char buffer[N];

    for( size_t i = 0; i < N; i++ ) {
      buffer[i] = convertEndianess ? data[N-i+1] : data[i];
    }

    return *reinterpret_cast<T*>(buffer);
  }
};

#endif // REKCONVERTER_H
