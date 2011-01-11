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
  \file    Plane.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
*/

#include "Plane.h"

const PLANE<float> ExtendedPlane::ms_Plane(0,0,1,0);

ExtendedPlane::ExtendedPlane(): m_Plane(ms_Plane) {
  m_mat[0] = FLOATMATRIX4();
  m_mat[1] = FLOATMATRIX4();
}


ExtendedPlane ExtendedPlane::FarawayPlane() {
  ExtendedPlane p;
  FLOATMATRIX4 translation;
  translation.Translation(0,0,100000);
  p.Transform(translation,false);
  return p;
}

void ExtendedPlane::Transform(const FLOATMATRIX4& mat, bool bSecondary)
{
  if (bSecondary) {
  
    // perform the rotation of the clip plane always relative to the
    // object center, therefore, we need to shift the plane to the center first 
    // then perform the transformation (e.g. rotation) and then shift it back
    FLOATMATRIX4 transComp, invTransComp;
    transComp.m41 = m_mat[0].m41;
    transComp.m42 = m_mat[0].m42;
    transComp.m43 = m_mat[0].m43;
    invTransComp.m41 = -m_mat[0].m41;
    invTransComp.m42 = -m_mat[0].m42;
    invTransComp.m43 = -m_mat[0].m43;
   
    m_mat[1] = m_mat[1] * transComp * mat * invTransComp;
  } else {
    m_mat[0] = m_mat[0] * mat;
  }

  UpdatePlane();
}

void ExtendedPlane::UpdatePlane() {
  m_Plane = ms_Plane * GetCompleteTransform();
}


bool ExtendedPlane::Quad(const FLOATVECTOR3& vEye,
                         std::vector<FLOATVECTOR3>& quad,
                         const float fWidgetSize) const
{
  FLOATMATRIX4 complete = GetCompleteTransform();

  // transform the coordinate frame of the quad
  FLOATVECTOR3 v1 = (FLOATVECTOR4(1,0,0,0) * complete).xyz();
  FLOATVECTOR3 v2 = (FLOATVECTOR4(0,1,0,0) * complete).xyz();

  // normalize just to be sure
  v1.normalize();
  v2.normalize();

  // construct a line from the center of the dataset in the direction of the plane
  FLOATVECTOR3 centerOfDataset        = (FLOATVECTOR4(0,0,0,1) * m_mat[0]).xyz();
  FLOATVECTOR3 centerOfDatasetToPlane = centerOfDataset+m_Plane.normal();

  // find the intersection of that line with the clip plane
  // this is the closest point of the plane to the object center and
  // we will use this point as the center of the widget
  FLOATVECTOR3 pt_on_plane;
  m_Plane.intersect(centerOfDataset,centerOfDatasetToPlane, pt_on_plane);

  FLOATVECTOR3 viewDir = pt_on_plane-vEye;

  // "push back" the triangulated quad
  if((m_Plane.xyz() ^ viewDir) < 0) {
    quad.push_back((pt_on_plane + (fWidgetSize*(v1  + v2))));
    quad.push_back((pt_on_plane + (fWidgetSize*(v1  - v2))));
    quad.push_back((pt_on_plane + (fWidgetSize*(-v1 - v2))));

    quad.push_back((pt_on_plane + (fWidgetSize*(-v1 - v2))));
    quad.push_back((pt_on_plane + (fWidgetSize*(-v1 + v2))));
    quad.push_back((pt_on_plane + (fWidgetSize*(v1  + v2))));
  } else {
    quad.push_back((pt_on_plane + (fWidgetSize*(-v1 - v2))));
    quad.push_back((pt_on_plane + (fWidgetSize*(v1  - v2))));
    quad.push_back((pt_on_plane + (fWidgetSize*(v1  + v2))));

    quad.push_back((pt_on_plane + (fWidgetSize*(v1  + v2))));
    quad.push_back((pt_on_plane + (fWidgetSize*(-v1 + v2))));
    quad.push_back((pt_on_plane + (fWidgetSize*(-v1 - v2))));
  }

  // "push back" the lines for the border
  quad.push_back((pt_on_plane + (fWidgetSize*(v1  + v2))));
  quad.push_back((pt_on_plane + (fWidgetSize*(v1  - v2))));

  quad.push_back((pt_on_plane + (fWidgetSize*(v1  - v2))));
  quad.push_back((pt_on_plane + (fWidgetSize*(-v1 - v2))));

  quad.push_back((pt_on_plane + (fWidgetSize*(-v1 - v2))));
  quad.push_back((pt_on_plane + (fWidgetSize*(-v1 + v2))));

  quad.push_back((pt_on_plane + (fWidgetSize*(-v1 + v2))));
  quad.push_back((pt_on_plane + (fWidgetSize*(v1  + v2))));

  return (m_Plane.xyz() ^ viewDir) < 0;
}

/// Sets the plane back to default values.
void ExtendedPlane::Default(bool bSecondary)
{
  m_mat[(bSecondary) ? 1 : 0] = FLOATMATRIX4();
  UpdatePlane();
}


FLOATMATRIX4 ExtendedPlane::GetCompleteTransform() const {
  return m_mat[1] * m_mat[0];
}