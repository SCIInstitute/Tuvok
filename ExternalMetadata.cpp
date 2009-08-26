/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2009 Scientific Computing and Imaging Institute,
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
  \author  Tom Fogal
           SCI Institute
           University of Utah
*/

#include "ExternalMetadata.h"

namespace tuvok {

ExternalMetadata::ExternalMetadata() :
  m_dataType(MDT_FLOAT)
{
}

UINTVECTOR3 ExternalMetadata::GetMaxBrickSize() const
{
  return m_vMaxBrickSize;
}

UINTVECTOR3 ExternalMetadata::GetBrickOverlapSize() const
{
  return m_vOverlap;
}

void ExternalMetadata::SetMaxBrickSize(const UINTVECTOR3 &mx)
{
  m_vMaxBrickSize = mx;
}

void ExternalMetadata::SetBrickOverlap(const UINTVECTOR3 &overlap)
{
  m_vOverlap = overlap;
}

UINT64 ExternalMetadata::GetLODLevelCount() const
{
  return 1;
}

/// Data should not be scaled.
DOUBLEVECTOR3 ExternalMetadata::GetScale() const {
  return DOUBLEVECTOR3(1,1,1);
}

/// Disabled for now; force the brick to always get rendered.
bool ExternalMetadata::ContainsData(const BrickKey &, double) const
{
  return true;
}
bool ExternalMetadata::ContainsData(const BrickKey &, double, double) const
{
  return true;
}
bool ExternalMetadata::ContainsData(const BrickKey &, double,double,
                                       double,double) const
{
  return true;
}

/// Our parent knows/handles our range.
void ExternalMetadata::SetRange(std::pair<double,double> r)
{
  Metadata::SetRange(r);
}
void ExternalMetadata::SetRange(double l, double h)
{
  Metadata::SetRange(std::make_pair(l,h));
}


}; // namespace tuvok
