/*
   The MIT License

   Copyright (c) 2010 Department Image Processing,
   Fraunhofer ITWM.


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
  \file    IASSConverter.h
  \author  Andre Liebscher
           Department Image Processing
           Fraunhofer ITWM
  \version 1.0
  \date    March 2010
*/
#pragma once

#ifndef TUVOK_IASSCONVERTER_H
#define TUVOK_IASSCONVERTER_H

#include "../StdTuvokDefines.h"
#include "RAWConverter.h"

/** A converter for Fraunhofer MAVI volume images.  */
class IASSConverter : public RAWConverter {
public:
  IASSConverter();
  virtual ~IASSConverter() {}

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

  virtual bool CanRead(const std::wstring& fn,
                       const std::vector<int8_t>& start) const;

  virtual bool CanExportData() const {return false;}
  virtual bool CanImportData() const { return true; }

private:
  /// supported pixel types
  enum ePixelType  {
    MONO = 0,       ///< monochrome, one bit per pixle runlenght encoded -
                    ///  0=background, 1=foreground
    GREY_8 = 1,     ///< greyscale 8 bit per pixel, unsigned integer
    GREY_16 = 2,    ///< greyscale 16 bit per pixel, unsigned integer
    GREY_32 = 3,    ///< greyscale 32 bit per pixel, unsigned integer
    GREY_F = 4,     ///< greyscale 32 bit per pixel, floating point
    COLOR = 5,      ///< 3 channel RGB, 1 byte/channel image
                    ///  Note: Images of this type are not actually used
    RGB_8 = COLOR,  ///< same as COLOR
    COMPLEX_F = 6,  ///< greyscale 64 bit per pixel, complex, ordered pair
                    ///  of floats
                    ///  Note: This is actually a two component image
    INVALID
  };

  /// iass header structure
  struct stHeader
  {
    // constructor
    stHeader();

    // reset header
    void Reset();

    /// value triple
    template<typename T>
    struct stTriple
    {
      T x;                      ///< x coord
      T y;                      ///< y coord
      T z;                      ///< z coord
    };

    typedef stTriple<uint64_t> sizeType;     ///< type for size triple
    typedef stTriple<double> spacingType;  ///< type for spacing triple

    ePixelType type;           ///< pixel/data type
    uint64_t bpp;                ///< bytes per pixel
    uint64_t skip;
    uint64_t rleLength;          ///< length of rle stream
    sizeType size;             ///< sample size
    spacingType spacing;       ///< dimensions of one pixel in meters
    std::string creator;       /*< creator of the sample/file */
    std::string history;       /*< history of the sample */
  };

  bool
  IsZipped(const std::wstring& strFile);

  bool
  ReadHeader(stHeader& header, std::istream& inputStream);
};

#endif // TUVOK_IASSCONVERTER_H
