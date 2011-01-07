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
const FLOATVECTOR3 ExtendedPlane::ms_Perpendicular(0,1,0);
const FLOATVECTOR3 ExtendedPlane::ms_Point(0,0,0);

ExtendedPlane::ExtendedPlane(): m_Plane(ms_Plane),
                                m_Perpendicular(ms_Perpendicular),
                                m_Point(ms_Point) {

  m_mat[0] = FLOATMATRIX4();
  m_mat[1] = FLOATMATRIX4();
}


ExtendedPlane ExtendedPlane::FarawayPlane() {
  ExtendedPlane p;
  FLOATMATRIX4 translation;
  translation.Translation(0,0,-100000);
  p.Transform(translation,false);
  return p;
}

void ExtendedPlane::Transform(const FLOATMATRIX4& mat, bool bSecondary)
{
  m_mat[(bSecondary) ? 1 : 0] = m_mat[(bSecondary) ? 1 : 0] * mat;
  UpdatePlane();
}

void ExtendedPlane::TransformIT(const FLOATMATRIX4& mat, bool bSecondary)
{
  FLOATMATRIX4 mIT(mat.inverse());
  mIT = mIT.Transpose();
  m_mat[(bSecondary) ? 1 : 0] = m_mat[(bSecondary) ? 1 : 0] * mIT;
  UpdatePlane();
}

void ExtendedPlane::UpdatePlane() {
  FLOATMATRIX4 complete = m_mat[1] * m_mat[0];
  m_Plane = ms_Plane * complete;
  m_Perpendicular = (FLOATVECTOR4(ms_Perpendicular,0) * complete).xyz();
  m_Perpendicular.normalize();
  m_Point = (FLOATVECTOR4(ms_Point,1) * complete).xyz();
}


bool ExtendedPlane::Quad(const FLOATVECTOR3& vEye,
                         std::vector<FLOATVECTOR3>& quad,
                         const float fWidgetSize) const
{
  FLOATVECTOR3 vec = m_Plane.xyz() % m_Perpendicular;
  FLOATVECTOR3 pt_on_plane(m_Point);

  FLOATVECTOR3 viewDir = pt_on_plane-vEye;

  // "push back" the triangulated quad
  if((m_Plane.xyz() ^ viewDir) < 0) {
    quad.push_back((pt_on_plane + (fWidgetSize*(vec  + m_Perpendicular))));
    quad.push_back((pt_on_plane + (fWidgetSize*(vec  - m_Perpendicular))));
    quad.push_back((pt_on_plane + (fWidgetSize*(-vec - m_Perpendicular))));

    quad.push_back((pt_on_plane + (fWidgetSize*(-vec - m_Perpendicular))));
    quad.push_back((pt_on_plane + (fWidgetSize*(-vec + m_Perpendicular))));
    quad.push_back((pt_on_plane + (fWidgetSize*(vec  + m_Perpendicular))));
  } else {
    quad.push_back((pt_on_plane + (fWidgetSize*(-vec - m_Perpendicular))));
    quad.push_back((pt_on_plane + (fWidgetSize*(vec  - m_Perpendicular))));
    quad.push_back((pt_on_plane + (fWidgetSize*(vec  + m_Perpendicular))));

    quad.push_back((pt_on_plane + (fWidgetSize*(vec  + m_Perpendicular))));
    quad.push_back((pt_on_plane + (fWidgetSize*(-vec + m_Perpendicular))));
    quad.push_back((pt_on_plane + (fWidgetSize*(-vec - m_Perpendicular))));
  }

  // "push back" the lines for the border
  quad.push_back((pt_on_plane + (fWidgetSize*(vec  + m_Perpendicular))));
  quad.push_back((pt_on_plane + (fWidgetSize*(vec  - m_Perpendicular))));

  quad.push_back((pt_on_plane + (fWidgetSize*(vec  - m_Perpendicular))));
  quad.push_back((pt_on_plane + (fWidgetSize*(-vec - m_Perpendicular))));

  quad.push_back((pt_on_plane + (fWidgetSize*(-vec - m_Perpendicular))));
  quad.push_back((pt_on_plane + (fWidgetSize*(-vec + m_Perpendicular))));

  quad.push_back((pt_on_plane + (fWidgetSize*(-vec + m_Perpendicular))));
  quad.push_back((pt_on_plane + (fWidgetSize*(vec  + m_Perpendicular))));

  return (m_Plane.xyz() ^ viewDir) < 0;
}

/// Sets the plane back to default values.
void ExtendedPlane::Default(bool bSecondary)
{
  m_mat[(bSecondary) ? 1 : 0] = FLOATMATRIX4();
  UpdatePlane();
}
