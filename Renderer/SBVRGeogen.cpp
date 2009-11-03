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
  \file    SBVRGeogen.cpp
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    September 2008
*/

#include <algorithm>
#include <cassert>
#include <StdTuvokDefines.h>
#if defined(_MSC_VER) ||                                               \
    (defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ <= 1)) ||   \
    defined(DETECTED_OS_APPLE)
// MS puts tr1 in standard headers.  Old gcc's and all gcc's on Apple are
// broken, not including this part of tr1.
# include <cmath>
#else
# include <tr1/cmath>
#endif
#include <functional>
#include <limits>
#include "SBVRGeogen.h"

SBVRGeogen::SBVRGeogen(void) :
  m_fSamplingModifier(1.0f),
  m_vGlobalSize(1,1,1),
  m_vGlobalAspect(1,1,1),
  m_vLODSize(1,1,1),
  m_vAspect(1,1,1),
  m_vSize(1,1,1),
  m_vTexCoordMin(0,0,0),
  m_vTexCoordMax(1,1,1),
  m_bClipPlaneEnabled(false)
{
  m_pfBBOXStaticVertex[0] = FLOATVECTOR3(-0.5, 0.5,-0.5);
  m_pfBBOXStaticVertex[1] = FLOATVECTOR3( 0.5, 0.5,-0.5);
  m_pfBBOXStaticVertex[2] = FLOATVECTOR3( 0.5, 0.5, 0.5);
  m_pfBBOXStaticVertex[3] = FLOATVECTOR3(-0.5, 0.5, 0.5);
  m_pfBBOXStaticVertex[4] = FLOATVECTOR3(-0.5,-0.5,-0.5);
  m_pfBBOXStaticVertex[5] = FLOATVECTOR3( 0.5,-0.5,-0.5);
  m_pfBBOXStaticVertex[6] = FLOATVECTOR3( 0.5,-0.5, 0.5);
  m_pfBBOXStaticVertex[7] = FLOATVECTOR3(-0.5,-0.5, 0.5);

  m_pfBBOXVertex[0] = FLOATVECTOR3(0,0,0);
  m_pfBBOXVertex[1] = FLOATVECTOR3(0,0,0);
  m_pfBBOXVertex[2] = FLOATVECTOR3(0,0,0);
  m_pfBBOXVertex[3] = FLOATVECTOR3(0,0,0);
  m_pfBBOXVertex[4] = FLOATVECTOR3(0,0,0);
  m_pfBBOXVertex[5] = FLOATVECTOR3(0,0,0);
  m_pfBBOXVertex[6] = FLOATVECTOR3(0,0,0);
  m_pfBBOXVertex[7] = FLOATVECTOR3(0,0,0);
}

SBVRGeogen::~SBVRGeogen(void)
{
}

float SBVRGeogen::GetOpacityCorrection() const {
  return 1.0f/m_fSamplingModifier * (FLOATVECTOR3(m_vGlobalSize)/FLOATVECTOR3(m_vLODSize)).maxVal(); //  GetLayerDistance()*m_vSize.maxVal();
}


// Should be called after updating the world or view matrices.  Causes geometry
// to be recomputed with the updated matrices.
void SBVRGeogen::MatricesUpdated()
{
  m_matViewTransform = m_matWorld * m_matView;
  InitBBOX();
  ComputeGeometry();
}

void SBVRGeogen::SetWorld(const FLOATMATRIX4& matWorld, bool bForceUpdate) {
  if (bForceUpdate || m_matWorld != matWorld) {
    m_matWorld = matWorld;
    MatricesUpdated();
  }
}
void SBVRGeogen::SetView(const FLOATMATRIX4& mTransform,
                         bool forceUpdate)
{
  if(forceUpdate || m_matView != mTransform) {
    m_matView = mTransform;
    MatricesUpdated();
  }
}

void SBVRGeogen::InitBBOX() {

  FLOATVECTOR3 vVertexScale(m_vAspect);

  m_pfBBOXVertex[0] = POS3TEX3_VERTEX(FLOATVECTOR4(m_pfBBOXStaticVertex[0]*vVertexScale,1.0f) * m_matViewTransform, FLOATVECTOR3(m_vTexCoordMin.x,m_vTexCoordMax.y,m_vTexCoordMin.z));
  m_pfBBOXVertex[1] = POS3TEX3_VERTEX(FLOATVECTOR4(m_pfBBOXStaticVertex[1]*vVertexScale,1.0f) * m_matViewTransform, FLOATVECTOR3(m_vTexCoordMax.x,m_vTexCoordMax.y,m_vTexCoordMin.z));
  m_pfBBOXVertex[2] = POS3TEX3_VERTEX(FLOATVECTOR4(m_pfBBOXStaticVertex[2]*vVertexScale,1.0f) * m_matViewTransform, FLOATVECTOR3(m_vTexCoordMax.x,m_vTexCoordMax.y,m_vTexCoordMax.z));
  m_pfBBOXVertex[3] = POS3TEX3_VERTEX(FLOATVECTOR4(m_pfBBOXStaticVertex[3]*vVertexScale,1.0f) * m_matViewTransform, FLOATVECTOR3(m_vTexCoordMin.x,m_vTexCoordMax.y,m_vTexCoordMax.z));
  m_pfBBOXVertex[4] = POS3TEX3_VERTEX(FLOATVECTOR4(m_pfBBOXStaticVertex[4]*vVertexScale,1.0f) * m_matViewTransform, FLOATVECTOR3(m_vTexCoordMin.x,m_vTexCoordMin.y,m_vTexCoordMin.z));
  m_pfBBOXVertex[5] = POS3TEX3_VERTEX(FLOATVECTOR4(m_pfBBOXStaticVertex[5]*vVertexScale,1.0f) * m_matViewTransform, FLOATVECTOR3(m_vTexCoordMax.x,m_vTexCoordMin.y,m_vTexCoordMin.z));
  m_pfBBOXVertex[6] = POS3TEX3_VERTEX(FLOATVECTOR4(m_pfBBOXStaticVertex[6]*vVertexScale,1.0f) * m_matViewTransform, FLOATVECTOR3(m_vTexCoordMax.x,m_vTexCoordMin.y,m_vTexCoordMax.z));
  m_pfBBOXVertex[7] = POS3TEX3_VERTEX(FLOATVECTOR4(m_pfBBOXStaticVertex[7]*vVertexScale,1.0f) * m_matViewTransform, FLOATVECTOR3(m_vTexCoordMin.x,m_vTexCoordMin.y,m_vTexCoordMax.z));
}

void SBVRGeogen::SetVolumeData(const FLOATVECTOR3& vAspect, const UINTVECTOR3& vSize) {
  m_vGlobalAspect = vAspect;
  m_vGlobalSize = vSize;
}

void SBVRGeogen::SetLODData(const UINTVECTOR3& vSize) {
  m_vLODSize = vSize;
}

void SBVRGeogen::SetBrickData(const FLOATVECTOR3& vAspect,
                              const UINTVECTOR3& vSize,
                              const FLOATVECTOR3& vTexCoordMin,
                              const FLOATVECTOR3& vTexCoordMax) {
  m_vAspect       = vAspect;
  m_vSize         = vSize;
  m_vTexCoordMin  = vTexCoordMin;
  m_vTexCoordMax  = vTexCoordMax;
  InitBBOX();
}
