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
  \file    TransferFunction2D.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \version 1.0
  \date    July 2008
*/

#pragma once

#ifndef TRANSFERFUNCTION2D
#define TRANSFERFUNCTION2D

#include "StdTuvokDefines.h"

#include <fstream>
#include <string>
#include <vector>

/// @todo FIXME remove this dependency:
#ifdef TUVOK_NO_QT
typedef void* QImage;
typedef void* QPainter;
#else
# include <QImage>
# include <QPainter>
#endif

#include "Basics/Vectors.h"
#include "Basics/Grids.h"

#include "TransferFunction1D.h"

typedef Grid2D<uint32_t> Histogram2D;
typedef Grid2D<float> NormalizedHistogram2D;
typedef Grid2D<FLOATVECTOR4> ColorData2D;
typedef Grid2D< VECTOR4<char>> ColorData2D8Bit;

typedef std::pair< float, FLOATVECTOR4 > GradientStop;

class TFPolygon {
  public:
    TFPolygon() : bRadial(false) {}

    bool Load(std::ifstream& file);
    void Save(std::ofstream& file) const;

    bool                        bRadial;
    std::vector< FLOATVECTOR2 > pPoints;
    FLOATVECTOR2 pGradientCoords[2];
    std::vector< GradientStop > pGradientStops;
};


class TransferFunction2D
{
public:
  TransferFunction2D();
  TransferFunction2D(const VECTOR2<size_t>& iSize);
  TransferFunction2D(const std::string& filename);
  ~TransferFunction2D(void);

  void Resize(const Histogram2D& hist) {Resize(hist.GetSize());}
  void Resize(const NormalizedHistogram2D& hist) {Resize(hist.GetSize());}
  void Resize(const VECTOR2<size_t>& iSize);

  void Resample(const VECTOR2<size_t>& iSize);

  bool Load(const std::string& filename);
  bool Load(const std::string& filename, const VECTOR2<size_t>& vTargetSize);
  bool Save(const std::string& filename) const;

  void InvalidateCache() {m_bUseCachedData = false;}
  void GetByteArray(unsigned char** pcData);
  void GetByteArray(unsigned char** pcData, unsigned char cUsedRange);
  void GetShortArray(unsigned short** psData,
                     unsigned short sUsedRange=4095);
  void GetFloatArray(float** pfData);

  // Functions bound into Lua.
  std::shared_ptr<const std::vector<TFPolygon>> SwatchGet() const;
  size_t SwatchArrayGetSize() const;
  void SwatchPushBack(const TFPolygon& swatch);
  void SwatchErase(size_t swatchIndex);
  /// Inserts swatch at index 'i' whose value is 'swatch'.
  void SwatchInsert(size_t i, const TFPolygon& swatch);
  /// Replaces the swatch at index 'i' with 'swatch'.
  void SwatchUpdate(size_t i, const TFPolygon& swatch);
  bool SwatchIsRadial(size_t i) const;
  void SwatchSetRadial(size_t i, bool radial);

  size_t SwatchGetGradientCount(size_t i) const;
  GradientStop SwatchGetGradient(size_t swatch, size_t i) const;
  void SwatchInsertGradient(size_t swatch, size_t i, GradientStop stop);
  void SwatchPushBackGradient(size_t swatch, GradientStop stop);
  void SwatchEraseGradient(size_t swatch, size_t i);
  void SwatchUpdateGradient(size_t swatch, size_t i, GradientStop stop);

  void SwatchInsertPoint(size_t swatch, size_t i, FLOATVECTOR2 coord);
  void SwatchErasePoint(size_t swatch, size_t i);
  size_t SwatchGetNumPoints(size_t i) const;


  std::shared_ptr<std::vector<TFPolygon>> m_pvSwatches;

  const VECTOR2<size_t> GetSize() const {return m_iSize;}
  const VECTOR2<size_t> GetRenderSize() const {

    VECTOR2<size_t> vSize = m_iSize;
    vSize.StoreMin(VECTOR2<size_t>(4096, 4096));

    return vSize.x > vSize.y ?
           VECTOR2<size_t>(vSize.x, static_cast<size_t>(vSize.x/2.0)) :
           VECTOR2<size_t>(vSize.y*2, vSize.y);
  }

  void ComputeNonZeroLimits();
  const UINT64VECTOR4& GetNonZeroLimits() { return m_vValueBBox;}

  const TransferFunction1D* Get1DTrans() {return &m_Trans1D;}
  const QImage& Get1DTransImage() {return m_Trans1DImage;}
  void Update1DTrans(const TransferFunction1D* p1DTrans);

protected:
  TransferFunction1D m_Trans1D;
  QImage             m_Trans1DImage;
  VECTOR2<size_t>    m_iSize;
  ColorData2D* RenderTransferFunction();
  unsigned char* RenderTransferFunction8Bit();
  INTVECTOR2 Normalized2Offscreen(FLOATVECTOR2 vfCoord, VECTOR2<size_t> iSize) const;

private:
  ColorData2D*      m_pColorData;
  unsigned char*    m_pPixelData;
  QPainter          m_Painter;
  QImage*           m_pRCanvas;
  UINT64VECTOR4     m_vValueBBox;
  bool              m_bUseCachedData;

  void DeleteCanvasData();
};

#endif // TRANSFERFUNCTION2D
