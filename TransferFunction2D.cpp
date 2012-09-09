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
  \file    TransferFunction2D.cpp
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \version 1.0
  \date    September 2008
*/

#include <memory.h>
#include "TransferFunction2D.h"
#include "Controller/Controller.h"

using namespace std;

TransferFunction2D::TransferFunction2D() :
  m_pvSwatches(new vector<TFPolygon>),
  m_iSize(0,0),
  m_pColorData(NULL),
  m_pPixelData(NULL),
  m_pRCanvas(NULL),
  m_bUseCachedData(false)
{
}

TransferFunction2D::TransferFunction2D(const std::string& filename):
  m_pvSwatches(new vector<TFPolygon>),
  m_iSize(0,0),
  m_pColorData(NULL),
  m_pPixelData(NULL),
  m_pRCanvas(NULL),
  m_bUseCachedData(false)
{
  Load(filename);
}

TransferFunction2D::TransferFunction2D(const VECTOR2<size_t>& iSize):
  m_pvSwatches(new vector<TFPolygon>),
  m_iSize(iSize),
  m_pColorData(NULL),
  m_pPixelData(NULL),
  m_pRCanvas(NULL),
  m_bUseCachedData(false)
{
  Resize(m_iSize);
}


void TransferFunction2D::DeleteCanvasData()
{
  delete m_pColorData;
  delete [] m_pPixelData;
  delete m_pRCanvas;
}

TransferFunction2D::~TransferFunction2D(void)
{
  DeleteCanvasData();
}

void TransferFunction2D::Resize(const VECTOR2<size_t>& iSize) {
  m_iSize = iSize;
  m_Trans1D.Resize(iSize.x);
  m_Trans1D.Clear();

  DeleteCanvasData();
}

void TransferFunction2D::Resample(const VECTOR2<size_t>& iSize) {
  m_iSize = iSize;
  m_Trans1D.Resample(iSize.x);
}

bool TransferFunction2D::Load(const std::string& filename, const VECTOR2<size_t>& vTargetSize) {
  ifstream file(filename.c_str());

  if (!file.is_open()) return false;

  m_iSize = vTargetSize;

  // ignore the size in the file (read it but never use it again)
  VECTOR2<size_t> vSizeInFile;
  file >> vSizeInFile.x >> vSizeInFile.y;

  if(!file) {
    T_ERROR("2DTF '%s' ends after file size!", filename.c_str());
    return false;
  }

  // load 1D Trans
  if(!m_Trans1D.Load(file, vTargetSize.x)) {
    T_ERROR("Failed loading 1DTF within 2DTF! (in %s)", filename.c_str());
    return false;
  }

  // load swatch count
  uint32_t iSwatchCount;
  file >> iSwatchCount;

  if(!file) {
    T_ERROR("Swatch count is missing in %s.", filename.c_str());
    return false;
  }

  m_pvSwatches->resize(iSwatchCount);

  // load Swatches
  for (size_t i = 0;i<m_pvSwatches->size();i++) {
    if(!(*m_pvSwatches)[i].Load(file)) {
      T_ERROR("Failed loading swatch %u/%u in %s",
              static_cast<unsigned>(i),
              static_cast<unsigned>(m_pvSwatches->size()-1),
              filename.c_str());
      return false;
    }
  }

  file.close();

  return true;
}


bool TransferFunction2D::Load(const std::string& filename) {
  ifstream file(filename.c_str());

  if (!file.is_open()) return false;

  // load size
  file >> m_iSize.x >> m_iSize.y;

  if(!file) {
    T_ERROR("Could not get 1D TF size from stream (in %s).", filename.c_str());
    return false;
  }

  // load 1D Trans
  if(!m_Trans1D.Load(file, m_iSize.x)) {
    T_ERROR("2DTF '%s': Could not load 1D TF.", filename.c_str());
    return false;
  }

  // load swatch count
  uint32_t iSwatchCount;
  file >> iSwatchCount;
  if(!file) {
    T_ERROR("Invalid swatch count in %s", filename.c_str());
    return false;
  }
  m_pvSwatches->resize(iSwatchCount);
  if(iSwatchCount == 0) {
    WARNING("No swatches?!  Ridiculous 2D TFqn, bailing...");
    return false;
  }

  // load Swatches
  for (size_t i = 0;i<m_pvSwatches->size();i++) {
    if(!(*m_pvSwatches)[i].Load(file)) {
      T_ERROR("Failed loading swatch %u/%u in %s",
              static_cast<unsigned>(i),
              static_cast<unsigned>(m_pvSwatches->size()-1),
              filename.c_str());
      return false;
    }
  }

  file.close();

  return true;
}

bool TransferFunction2D::Save(const std::string& filename) const {
  ofstream file(filename.c_str());

  if (!file.is_open()) return false;

  // save size
  file << m_iSize.x << " " << m_iSize.y << endl;

  // save 1D Trans
  m_Trans1D.Save(file);

  // save swatch count
  file << m_pvSwatches->size() << endl;

  // save Swatches
  for (size_t i = 0;i<m_pvSwatches->size();i++) (*m_pvSwatches)[i].Save(file);

  file.close();

  return true;
}

void TransferFunction2D::GetByteArray(unsigned char** pcData) {
  if (*pcData == NULL) *pcData = new unsigned char[m_iSize.area()*4];

  size_t iSize = m_iSize.area();
  unsigned char *pcSourceDataIterator = RenderTransferFunction8Bit();
  unsigned char *pcDataIterator = *pcData;
  memcpy(pcDataIterator, pcSourceDataIterator, iSize*4);
  for (size_t i = 0;i<iSize;i++) {
    unsigned char r = *(pcDataIterator+2);
    unsigned char b = *(pcDataIterator+0);

    *(pcDataIterator+0) = r;
    *(pcDataIterator+2) = b;

    pcDataIterator+=4;
  }
}


void TransferFunction2D::GetByteArray(unsigned char** pcData,
                                      unsigned char cUsedRange) {
  if (*pcData == NULL) *pcData = new unsigned char[m_iSize.area()*4];

  float fScale = 255.0f/float(cUsedRange);

  size_t iSize = m_iSize.area();
  unsigned char *pcSourceDataIterator = RenderTransferFunction8Bit();
  unsigned char *pcDataIterator = *pcData;
  memcpy(pcDataIterator, pcSourceDataIterator, iSize*4);
  for (size_t i = 0;i<iSize;i++) {
    unsigned char r = *(pcDataIterator+2);
    unsigned char g = *(pcDataIterator+1);
    unsigned char b = *(pcDataIterator+0);
    unsigned char a = *(pcDataIterator+3);

    *(pcDataIterator+0) = (unsigned char)(float(r)*fScale);
    *(pcDataIterator+1) = (unsigned char)(float(g)*fScale);
    *(pcDataIterator+2) = (unsigned char)(float(b)*fScale);
    *(pcDataIterator+3) = (unsigned char)(float(a)*fScale);

    pcDataIterator+=4;
  }
}

void TransferFunction2D::GetShortArray(unsigned short** psData,
                                       unsigned short sUsedRange) {
  if (*psData == NULL) *psData = new unsigned short[m_iSize.area()*4];

  RenderTransferFunction();
  unsigned short *psDataIterator = *psData;
  FLOATVECTOR4  *piSourceIterator = m_pColorData->GetDataPointer();
  for (size_t i = 0;i<m_pColorData->GetSize().area();i++) {
    *psDataIterator++ = (unsigned short)((*piSourceIterator)[0]*sUsedRange);
    *psDataIterator++ = (unsigned short)((*piSourceIterator)[1]*sUsedRange);
    *psDataIterator++ = (unsigned short)((*piSourceIterator)[2]*sUsedRange);
    *psDataIterator++ = (unsigned short)((*piSourceIterator)[3]*sUsedRange);
    piSourceIterator++;
  }
}

void TransferFunction2D::GetFloatArray(float** pfData) {
  if (*pfData == NULL) *pfData = new float[4*m_iSize.area()];

  RenderTransferFunction();
  memcpy(*pfData, m_pColorData->GetDataPointer(), 4*sizeof(float)*m_iSize.area());
}

INTVECTOR2 TransferFunction2D::Normalized2Offscreen(FLOATVECTOR2 vfCoord, VECTOR2<size_t> iSize) const {
  return INTVECTOR2(int(vfCoord.x*int(iSize.x)),
                    int(vfCoord.y*int(iSize.y)));
}

unsigned char* TransferFunction2D::RenderTransferFunction8Bit() {
  VECTOR2<size_t> vRS = GetRenderSize();
  if (m_pColorData == NULL) m_pColorData = new ColorData2D(m_iSize);
  if (m_pPixelData == NULL) m_pPixelData = new unsigned char[4*m_iSize.area()];

#ifndef TUVOK_NO_QT
  if (m_pRCanvas == NULL)   m_pRCanvas   = new QImage(int(vRS.x), int(vRS.y),
                                                      QImage::Format_ARGB32);

  if (m_pPixelData != NULL && m_bUseCachedData) return m_pPixelData;

  m_pRCanvas->fill(0);

  // render 1D trans
  QRect imageRect(0, 0, int(vRS.x), int(vRS.y));
  m_Painter.begin(m_pRCanvas);
  m_Painter.drawImage(imageRect,m_Trans1DImage);

  // render swatches
  QPen noBorderPen(Qt::NoPen);
  m_Painter.setPen(noBorderPen);
  for (size_t i = 0;i<m_pvSwatches->size();i++) {
    TFPolygon& currentSwatch = (*m_pvSwatches)[i];

    std::vector<QPoint> pointList(currentSwatch.pPoints.size());
    for (size_t j = 0;j<currentSwatch.pPoints.size();j++) {
      INTVECTOR2 vPixelPos = Normalized2Offscreen(currentSwatch.pPoints[j],vRS);
      pointList[j] = QPoint(vPixelPos.x, vPixelPos.y);
    }

    INTVECTOR2 vPixelPos0 = Normalized2Offscreen(currentSwatch.pGradientCoords[0],vRS),
               vPixelPos1 = Normalized2Offscreen(currentSwatch.pGradientCoords[1],vRS);

    QGradient* pGradientBrush;
    if (currentSwatch.bRadial) {
      double r = sqrt( pow(double(vPixelPos0.x-vPixelPos1.x),2.0) + pow(double(vPixelPos0.y-vPixelPos1.y),2.0));
      pGradientBrush = new QRadialGradient(vPixelPos0.x, vPixelPos0.y, r);
    } else {
      pGradientBrush = new QLinearGradient(vPixelPos0.x, vPixelPos0.y, vPixelPos1.x, vPixelPos1.y);
    }

    for (size_t j = 0;j<currentSwatch.pGradientStops.size();j++) {
      pGradientBrush->setColorAt(currentSwatch.pGradientStops[j].first,
                   QColor(int(currentSwatch.pGradientStops[j].second[0]*255),
                          int(currentSwatch.pGradientStops[j].second[1]*255),
                          int(currentSwatch.pGradientStops[j].second[2]*255),
                          int(currentSwatch.pGradientStops[j].second[3]*255)));
    }

    m_Painter.setBrush(*pGradientBrush);
    if(!pointList.empty()) {
      m_Painter.drawPolygon(&pointList[0], int(currentSwatch.pPoints.size()));
    }
    delete pGradientBrush;
  }
  m_Painter.end();

  memcpy(m_pPixelData, m_pRCanvas->scaled(int(m_iSize.x), int(m_iSize.y)).bits(), 4*m_iSize.area());
  m_bUseCachedData = true;
#else
  if (!m_pvSwatches->empty()) {
	m_pvSwatches->clear();
	WARNING("Cannot render transfer functions without Qt, returning empty transfer function.");
	memset(m_pPixelData, 0, 4*m_iSize.area());	
  }
#endif
  return m_pPixelData;
}

ColorData2D* TransferFunction2D::RenderTransferFunction() {

  unsigned char* pPixelData = RenderTransferFunction8Bit();

  FLOATVECTOR4* p = (FLOATVECTOR4*)(m_pColorData->GetDataPointer());
  for (size_t i = 0;i<m_pColorData->GetSize().area();i++) {
    p[i] = FLOATVECTOR4(pPixelData[4*i+2]/255.0f,
                        pPixelData[4*i+1]/255.0f,
                        pPixelData[4*i+0]/255.0f,
                        pPixelData[4*i+3]/255.0f);
  }

  return m_pColorData;
}

void TransferFunction2D::ComputeNonZeroLimits() {
  unsigned char* pPixelData = RenderTransferFunction8Bit();

  m_vValueBBox    = UINT64VECTOR4(uint64_t(m_iSize.x),0,
                                  uint64_t(m_iSize.y),0);

  size_t i = 3;
  for (size_t y = 0;y<m_iSize.y;y++) {
    for (size_t x = 0;x<m_iSize.x;x++) {
      if (pPixelData[i] != 0) {
        m_vValueBBox.x = MIN(m_vValueBBox.x, x);
        m_vValueBBox.y = MAX(m_vValueBBox.y, x);

        m_vValueBBox.z = MIN(m_vValueBBox.z, y);
        m_vValueBBox.w = MAX(m_vValueBBox.w, y);
      }
      i+=4;
    }
  }
}

#ifdef TUVOK_NO_QT
void TransferFunction2D::Update1DTrans(const TransferFunction1D*) {
#else
void TransferFunction2D::Update1DTrans(const TransferFunction1D* p1DTrans) {
#endif
#ifndef TUVOK_NO_QT
  m_Trans1D = TransferFunction1D(*p1DTrans);

  size_t iSize = min<size_t>(m_iSize.x,  m_Trans1D.GetSize());

  m_Trans1DImage = QImage(int(iSize), 1, QImage::Format_ARGB32);
  shared_ptr<vector<FLOATVECTOR4> > tfdata = m_Trans1D.GetColorData();
  for (size_t i = 0;i<iSize;i++) {
    float r = std::max(0.0f,std::min((*tfdata)[i][0],1.0f));
    float g = std::max(0.0f,std::min((*tfdata)[i][1],1.0f));
    float b = std::max(0.0f,std::min((*tfdata)[i][2],1.0f));
    float a = std::max(0.0f,std::min((*tfdata)[i][3],1.0f));

    m_Trans1DImage.setPixel(int(i),0,qRgba(int(r*255),int(g*255),int(b*255),int(a*255)));
  }
#else
  T_ERROR("Unsupported without Qt.");
#endif
}

size_t TransferFunction2D::SwatchArrayGetSize() const {
  return m_pvSwatches->size();
}

void TransferFunction2D::SwatchPushBack(const TFPolygon& swatch) {
  m_pvSwatches->push_back(swatch);
}

void TransferFunction2D::SwatchErase(size_t swatchIndex) {
  vector<TFPolygon>::iterator nth = m_pvSwatches->begin() + swatchIndex;
  m_pvSwatches->erase(nth);
}

void TransferFunction2D::SwatchInsert(size_t i, const TFPolygon& swatch) {
  vector<TFPolygon>::iterator nth = m_pvSwatches->begin() + i;
  m_pvSwatches->insert(nth, swatch);
}

size_t TransferFunction2D::SwatchGetNumPoints(size_t i) const {
  return (*m_pvSwatches)[i].pPoints.size();
}

bool TransferFunction2D::SwatchIsRadial(size_t i) const {
  return (*m_pvSwatches)[i].bRadial;
}

size_t TransferFunction2D::SwatchGetGradientCount(size_t i) const {
  return (*m_pvSwatches)[i].pGradientStops.size();
}

GradientStop TransferFunction2D::SwatchGetGradient(size_t point, size_t i) const
{
  return (*m_pvSwatches)[point].pGradientStops[i];
}

void TransferFunction2D::SwatchUpdate(size_t i, const TFPolygon& swatch) {
  (*m_pvSwatches)[i] = swatch;
}

// ***************************************************************************

bool TFPolygon::Load(ifstream& file) {
  uint32_t iSize;
  file >> bRadial;
  file >> iSize;
  if(!file) { return false; }
  pPoints.resize(iSize);

  if(iSize == 0) {
    // We don't want to bail, though, because we should still read the
    // gradient stop information.
    WARNING("polygon with no points...");
  }

  for(size_t i=0;i<pPoints.size();++i){
    for(size_t j=0;j<2;++j){
      file >> pPoints[i][j];
    }
  }

  file >> pGradientCoords[0][0] >> pGradientCoords[0][1];
  file >> pGradientCoords[1][0] >> pGradientCoords[1][1];

  file >> iSize;
  if(!file) { return false; }
  pGradientStops.resize(iSize);
  for(size_t i=0;i<pGradientStops.size();++i){
    file >> pGradientStops[i].first;
    for(size_t j=0;j<4;++j){
      file >> pGradientStops[i].second[j];
    }
  }
  return !(file.fail());
}

void TFPolygon::Save(ofstream& file) const {
  file << bRadial << endl;
  file << uint32_t(pPoints.size()) << endl;

  for(size_t i=0;i<pPoints.size();++i){
    for(size_t j=0;j<2;++j){
      file << pPoints[i][j] << " ";
    }
    file << endl;
  }

  file << pGradientCoords[0][0] << " " << pGradientCoords[0][1] << " ";
  file << pGradientCoords[1][0] << " " << pGradientCoords[1][1];
  file << endl;
  file << pGradientStops.size() << endl;

  for(size_t i=0;i<pGradientStops.size();++i){
    file << pGradientStops[i].first << "  ";
    for(size_t j=0;j<4;++j){
      file << pGradientStops[i].second[j] << " ";
    }
    file << endl;
  }
}
