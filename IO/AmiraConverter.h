/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2011 Scientific Computing and Imaging Institute,
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
#ifndef TUVOK_AMIRA_CONVERTER_H
#define TUVOK_AMIRA_CONVERTER_H

#include "RAWConverter.h"

/// A converter for Amira files, we think.
class AmiraConverter : public RAWConverter {
public:
   AmiraConverter();
   virtual ~AmiraConverter() {}

   virtual bool CanRead(const std::wstring& fn,
                        const std::vector<int8_t>& start) const;

   virtual bool ConvertToRAW(const std::wstring& strSourceFilename,
                             const std::wstring& strTempDir,
                             bool bNoUserInteraction,
                             uint64_t& iHeaderSkip, unsigned& iComponentSize,
                             uint64_t& iComponentCount, bool& bConvertEndianness,
                             bool& bSigned, bool& bIsFloat,
                             UINT64VECTOR3& vVolumeSize,
                             FLOATVECTOR3& vVolumeAspect,
                             std::wstring& strTitle,
                             std::wstring& strIntermediateFile,
                             bool& bDeleteIntermediateFile);

   virtual bool ConvertToNative(const std::wstring& strRawFilename,
                                const std::wstring& strTargetFilename,
                                uint64_t iHeaderSkip, unsigned iComponentSize,
                                uint64_t iComponentCount, bool bSigned,
                                bool bFloatingPoint, UINT64VECTOR3 vVolumeSize,
                                FLOATVECTOR3 vVolumeAspect,
                                bool bNoUserInteraction,
                                const bool bQuantizeTo8Bit);

   virtual bool CanExportData() const { return false; }
   virtual bool CanImportData() const { return true; }

};
#endif // TUVOK_AMIRA_CONVERTER_H
