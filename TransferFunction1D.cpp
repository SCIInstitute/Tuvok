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
  \file    TransferFunction1D.cpp
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \version 1.0
  \date    September 2008
*/

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory.h>
#include <sstream>
#include "Basics/MathTools.h"
#include "Controller/Controller.h"
#include "TransferFunction1D.h"

using namespace std;

TransferFunction1D::TransferFunction1D(size_t iSize) :
  m_vValueBBox(0,0),
  m_pvColorData(new vector<FLOATVECTOR4>())
{
  Resize(iSize);
}

TransferFunction1D::TransferFunction1D(const std::string& filename) :
  m_pvColorData(new vector<FLOATVECTOR4>())
{
  Load(filename);
}

TransferFunction1D::~TransferFunction1D(void)
{
}

void TransferFunction1D::Resize(size_t iSize) {
  m_pvColorData->resize(iSize);
}

float TransferFunction1D::Smoothstep(float x) const {
  return 3*x*x-2*x*x*x;
}

void TransferFunction1D::SetStdFunction(float fCenterPoint, float fInvGradient) {
  SetStdFunction(fCenterPoint, fInvGradient,0, false);
  SetStdFunction(fCenterPoint, fInvGradient,1, false);
  SetStdFunction(fCenterPoint, fInvGradient,2, false);
  SetStdFunction(fCenterPoint, fInvGradient,3, false);
}

void TransferFunction1D::SetStdFunction(float fCenterPoint, float fInvGradient, int iComponent, bool bInvertedStep) {
  size_t iCenterPoint = size_t((m_pvColorData->size()-1) * float(std::min<float>(std::max<float>(0,fCenterPoint),1)));
  size_t iInvGradient = size_t((m_pvColorData->size()-1) * float(std::min<float>(std::max<float>(0,fInvGradient),1)));

  size_t iRampStartPoint = (iInvGradient/2 > iCenterPoint)                      ? 0                 : iCenterPoint-(iInvGradient/2);
  size_t iRampEndPoint   = (iInvGradient/2 + iCenterPoint > m_pvColorData->size())  ? m_pvColorData->size() : iCenterPoint+(iInvGradient/2);

  if (bInvertedStep) {
    for (size_t i = 0;i<iRampStartPoint;i++)
      (*m_pvColorData)[i][iComponent] = 1;

    for (size_t i = iRampStartPoint;i<iRampEndPoint;i++) {
      float fValue = Smoothstep(float(i-iCenterPoint+(iInvGradient/2))/float(iInvGradient));
      (*m_pvColorData)[i][iComponent] = 1.0f-fValue;
    }

    for (size_t i = iRampEndPoint;i<m_pvColorData->size();i++)
      (*m_pvColorData)[i][iComponent] = 0;
  } else {
    for (size_t i = 0;i<iRampStartPoint;i++)
      (*m_pvColorData)[i][iComponent] = 0;

    for (size_t i = iRampStartPoint;i<iRampEndPoint;i++) {
      float fValue = Smoothstep(float(i-iCenterPoint+(iInvGradient/2))/float(iInvGradient));
      (*m_pvColorData)[i][iComponent] = fValue;
    }

    for (size_t i = iRampEndPoint;i<m_pvColorData->size();i++)
      (*m_pvColorData)[i][iComponent] = 1;
  }

  ComputeNonZeroLimits();
}

/// Finds the minimum and maximum per-channel of a 4-component packed vector.
template<typename ForwardIter, typename Out>
void minmax_component4(ForwardIter first, ForwardIter last,
                       Out c_min[4], Out c_max[4])
{
  c_min[0] = c_min[1] = c_min[2] = c_min[3] = *first;
  c_max[0] = c_max[1] = c_max[2] = c_max[3] = *first;
  if(first == last) { return; }
  while(first < last) {
    if(*(first+0) < c_min[0]) { c_min[0] = *(first+0); }
    if(*(first+1) < c_min[1]) { c_min[1] = *(first+1); }
    if(*(first+2) < c_min[2]) { c_min[2] = *(first+2); }
    if(*(first+3) < c_min[3]) { c_min[3] = *(first+3); }

    if(*(first+0) > c_max[0]) { c_max[0] = *(first+0); }
    if(*(first+1) > c_max[1]) { c_max[1] = *(first+1); }
    if(*(first+2) > c_max[2]) { c_max[2] = *(first+2); }
    if(*(first+3) > c_max[3]) { c_max[3] = *(first+3); }

    // Ugh.  Bail out if incrementing the iterator would go beyond the end.
    // We'd never actually deref the iterator in that case (because of the
    // while conditional), but we hit internal libstdc++ asserts anyway.
    if(static_cast<size_t>(std::distance(first, last)) < 4) {
      break;
    }
    std::advance(first, 4);
  }
}

/// Set the transfer function from an external source.  Assumes the vector
/// has 4-components per element, in RGBA order.
void TransferFunction1D::Set(const std::vector<unsigned char>& tf)
{
  assert(!tf.empty());
  m_pvColorData->resize(tf.size()/4);

  unsigned char tfmin[4];
  unsigned char tfmax[4];
  // A bit tricky.  We need the min/max of our vector so that we know how to
  // interpolate, but it needs to be a per-channel min/max.
  minmax_component4(tf.begin(),tf.end(), tfmin,tfmax);

  // Similarly, we need the min/max of our output format.
  const float fmin = 0.0;
  const float fmax = 1.0;

  assert(tfmin[0] <= tfmax[0]);
  assert(tfmin[1] <= tfmax[1]);
  assert(tfmin[2] <= tfmax[2]);
  assert(tfmin[3] <= tfmax[3]);

  for(size_t i=0; i < m_pvColorData->size(); ++i) {
    (*m_pvColorData)[i] = FLOATVECTOR4(
      MathTools::lerp(tf[4*i+0], tfmin[0],tfmax[0], fmin,fmax),
      MathTools::lerp(tf[4*i+1], tfmin[1],tfmax[1], fmin,fmax),
      MathTools::lerp(tf[4*i+2], tfmin[2],tfmax[2], fmin,fmax),
      MathTools::lerp(tf[4*i+3], static_cast<unsigned char>(0),
                                 static_cast<unsigned char>(255), fmin,fmax)
    );
  }

  ComputeNonZeroLimits();
}

void TransferFunction1D::Clear() {
  for (size_t i = 0;i<m_pvColorData->size();i++)
    (*m_pvColorData)[i] = FLOATVECTOR4(0,0,0,0);

  m_vValueBBox = UINT64VECTOR2(0,0);
}

void TransferFunction1D::FillOrTruncate(size_t iTargetSize) {
  vector< FLOATVECTOR4 > vTmpColorData(iTargetSize);  
  for (size_t i = 0;i<vTmpColorData.size();i++) {   
    if (i < m_pvColorData->size()) 
      vTmpColorData[i] = (*m_pvColorData)[i];
    else
      vTmpColorData[i] = FLOATVECTOR4(0,0,0,0);
  }
  *m_pvColorData = vTmpColorData;
  ComputeNonZeroLimits();
}

void TransferFunction1D::Resample(size_t iTargetSize) {
  size_t iSourceSize = m_pvColorData->size();

  if (iTargetSize == iSourceSize) return;

  vector< FLOATVECTOR4 > vTmpColorData(iTargetSize);

  if (iTargetSize < iSourceSize) {
    // downsample
    size_t iFrom = 0;
    for (size_t i = 0;i<vTmpColorData.size();i++) {

      size_t iTo = iFrom + iSourceSize/iTargetSize;

      vTmpColorData[i] = 0;
      for (size_t j = iFrom;j<iTo;j++) {
        vTmpColorData[i] += (*m_pvColorData)[j];
      }
      vTmpColorData[i] /= float(iTo-iFrom);

      iTargetSize -= 1;
      iSourceSize -= iTo-iFrom;

      iFrom = iTo;
    }
  } else {
    // upsample
    for (size_t i = 0;i<vTmpColorData.size();i++) {
      float fPos = float(i) * float(iSourceSize-1)/float(iTargetSize);
      size_t iFloor = size_t(floor(fPos));
      size_t iCeil  = std::min(iFloor+1, m_pvColorData->size()-1);
      float fInterp = fPos - float(iFloor);

      vTmpColorData[i] = (*m_pvColorData)[iFloor] * (1-fInterp) + (*m_pvColorData)[iCeil] * fInterp;
    }

  }

  *m_pvColorData = vTmpColorData;
  ComputeNonZeroLimits();
}

bool TransferFunction1D::Load(const std::string& filename, size_t iTargetSize) {
  if (!Load(filename)) {
    T_ERROR("Load from %s failed", filename.c_str());
    return false;
  } else {
    Resample(iTargetSize);
    return true;
  }
}


bool TransferFunction1D::Load(const std::string& filename) {
  ifstream file(filename.c_str());
  if (!Load(file)) {
    T_ERROR("Load of '%s' failed.", filename.c_str());
    return false;
  }
  file.close();
  ComputeNonZeroLimits();
  return true;
}

bool TransferFunction1D::Load(std::istream& tf, size_t iTargetSize) {
  if (!Load(tf)) {
    T_ERROR("Load from stream failed.");
    return false;
  } else {
    Resample(iTargetSize);
    return true;
  }
}

bool TransferFunction1D::Save(const std::string& filename) const {
  ofstream file(filename.c_str());
  if (!Save(file)) return false;
  file.close();
  return true;
}

bool TransferFunction1D::Load(std::istream& tf) {
  uint32_t iSize;
  tf >> iSize;
  if(!tf) {
    T_ERROR("Size information invalid.");
    return false;
  }
  m_pvColorData->resize(iSize);

  for(size_t i=0;i<m_pvColorData->size();++i){
    for(size_t j=0;j<4;++j){
      tf >> (*m_pvColorData)[i][j];
    }
  }

  return tf.good();
}


bool TransferFunction1D::Save(std::ostream& file) const {
  file << m_pvColorData->size() << endl;

  for(size_t i=0;i<m_pvColorData->size();++i){
    for(size_t j=0;j<4;++j){
      file << (*m_pvColorData)[i][j] << " ";
    }
    file << endl;
  }

  return true;
}

void TransferFunction1D::GetByteArray(std::vector<unsigned char>& vData,
                                      unsigned char cUsedRange) const {
  // bail out immediately if we've got no data
  if(m_pvColorData->empty()) { return; }

  vData.resize(m_pvColorData->size() * 4);

  unsigned char *pcDataIterator = &vData.at(0);
  for (size_t i = 0;i<m_pvColorData->size();i++) {
    unsigned char r = (unsigned char)(std::max(0.0f,std::min((*m_pvColorData)[i][0],1.0f))*cUsedRange);
    unsigned char g = (unsigned char)(std::max(0.0f,std::min((*m_pvColorData)[i][1],1.0f))*cUsedRange);
    unsigned char b = (unsigned char)(std::max(0.0f,std::min((*m_pvColorData)[i][2],1.0f))*cUsedRange);
    unsigned char a = (unsigned char)(std::max(0.0f,std::min((*m_pvColorData)[i][3],1.0f))*cUsedRange);

    *pcDataIterator++ = r;
    *pcDataIterator++ = g;
    *pcDataIterator++ = b;
    *pcDataIterator++ = a;
  }
}

void TransferFunction1D::GetShortArray(unsigned short** psData,
                                       unsigned short sUsedRange) const {
  // bail out immediately if we've got no data
  if(m_pvColorData->empty()) { return; }

  if (*psData == NULL) *psData = new unsigned short[m_pvColorData->size()*4];

  unsigned short *psDataIterator = *psData;
  for (size_t i = 0;i<m_pvColorData->size();i++) {
    unsigned short r = (unsigned short)(std::max(0.0f,std::min((*m_pvColorData)[i][0],1.0f))*sUsedRange);
    unsigned short g = (unsigned short)(std::max(0.0f,std::min((*m_pvColorData)[i][1],1.0f))*sUsedRange);
    unsigned short b = (unsigned short)(std::max(0.0f,std::min((*m_pvColorData)[i][2],1.0f))*sUsedRange);
    unsigned short a = (unsigned short)(std::max(0.0f,std::min((*m_pvColorData)[i][3],1.0f))*sUsedRange);

    *psDataIterator++ = r;
    *psDataIterator++ = g;
    *psDataIterator++ = b;
    *psDataIterator++ = a;
  }
}

void TransferFunction1D::GetFloatArray(float** pfData) const {
  // bail out immediately if we've got no data
  if(m_pvColorData->empty()) { return; }

  if (*pfData == NULL) *pfData = new float[4*m_pvColorData->size()];
  memcpy(*pfData, &pfData[0], sizeof(float)*4*m_pvColorData->size());
}


void TransferFunction1D::ComputeNonZeroLimits() {
  m_vValueBBox = UINT64VECTOR2(uint64_t(m_pvColorData->size()),0);

  for (size_t i = 0;i<m_pvColorData->size();i++) {
    if ((*m_pvColorData)[i][3] != 0) {
      m_vValueBBox.x = MIN(m_vValueBBox.x, i);
      m_vValueBBox.y = i;
    }
  }
}

std::shared_ptr<std::vector<FLOATVECTOR4> > TransferFunction1D::GetColorData() {
  return m_pvColorData;
}

FLOATVECTOR4 TransferFunction1D::GetColor(size_t index) const {
  return (*m_pvColorData)[index];
}

void TransferFunction1D::SetColor(size_t index, FLOATVECTOR4 color) {
  (*m_pvColorData)[index] = color;
}
