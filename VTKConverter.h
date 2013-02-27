#ifndef TUVOK_VTK_CONVERTER_H
#define TUVOK_VTK_CONVERTER_H

#include "RAWConverter.h"

/// This class converts a VTK dataset into raw data, so that it can be easily
/// converted into alternate formats---UVF, for us.
class VTKConverter : public RAWConverter {
public:
  VTKConverter();
  virtual ~VTKConverter() {}

  bool CanRead(const std::string&, const std::vector<int8_t>&) const;

  virtual bool ConvertToRAW(
    const std::string& strSourceFilename,
    const std::string& strTempDir, bool bNoUserInteraction,
    uint64_t& iHeaderSkip, unsigned& iComponentSize, uint64_t& iComponentCount,
    bool& bConvertEndianess, bool& bSigned, bool& bIsFloat,
    UINT64VECTOR3& vVolumeSize, FLOATVECTOR3& vVolumeAspect,
    std::string& strTitle, std::string& strIntermediateFile,
    bool& bDeleteIntermediateFile
  );

  virtual bool CanImportData() const { return true; }
};
#endif // TUVOK_VTK_CONVERTER_H
/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2013 IVDA Group.

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
