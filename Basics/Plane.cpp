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

ExtendedPlane::ExtendedPlane(const PLANE<float>& p,
                             const FLOATVECTOR3& perp): m_Plane(p),
                                                        m_Perpendicular(perp) {
}

void ExtendedPlane::Transform(const FLOATMATRIX4& mat)
{
  m_Plane = m_Plane * mat;
  m_Perpendicular = m_Perpendicular * mat;
}

void ExtendedPlane::TransformIT(const FLOATMATRIX4& mat)
{
  FLOATMATRIX4 mIT(mat.inverse());
  mIT = mIT.Transpose();
  m_Plane = m_Plane * mIT;
  m_Perpendicular = (FLOATVECTOR4(m_Perpendicular,0) * mIT).xyz();
  m_Perpendicular.normalize();
}

bool ExtendedPlane::Quad(const FLOATVECTOR3& viewDir,
                         const FLOATVECTOR3& vDatasetCenter,
                         std::vector<FLOATVECTOR3>& quad)
{
  FLOATVECTOR3 vec = m_Plane.xyz() % m_Perpendicular;
  float plane_d = m_Plane.d();
  FLOATVECTOR3 pt_on_plane(-(plane_d-0.01f)*m_Plane.xyz());

  float dist = m_Plane.xyz() ^ vDatasetCenter;
  FLOATVECTOR3 vCenter = vDatasetCenter - (dist+plane_d)*m_Plane.xyz();
  pt_on_plane = vCenter;

  if((m_Plane.xyz() ^ viewDir) < 0) {
    quad.push_back((pt_on_plane + (vec  + m_Perpendicular)));
    quad.push_back((pt_on_plane + (vec  - m_Perpendicular)));
    quad.push_back((pt_on_plane + (-vec - m_Perpendicular)));

    quad.push_back((pt_on_plane + (-vec - m_Perpendicular)));
    quad.push_back((pt_on_plane + (-vec + m_Perpendicular)));
    quad.push_back((pt_on_plane + (vec  + m_Perpendicular)));
  } else {
    quad.push_back((pt_on_plane + (-vec - m_Perpendicular)));
    quad.push_back((pt_on_plane + (vec  - m_Perpendicular)));
    quad.push_back((pt_on_plane + (vec  + m_Perpendicular)));

    quad.push_back((pt_on_plane + (vec  + m_Perpendicular)));
    quad.push_back((pt_on_plane + (-vec + m_Perpendicular)));
    quad.push_back((pt_on_plane + (-vec - m_Perpendicular)));
  }
  return (m_Plane.xyz() ^ viewDir) < 0;
}
