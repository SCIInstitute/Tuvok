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

#pragma once

#ifndef TUVOK_PLANE_H
#define TUVOK_PLANE_H

#include <vector>
#include "Vectors.h"

/// Stores a plane as an always-normalized normal and a perpendicular vector.
/// The latter is used when rendering the plane: it tells us in which direction
/// we'd like the plane to (visibly) extend.  By packaging them together, we
/// can ensure that both are always transformed equally, keeping them in sync.
class ExtendedPlane {
  public:
    ExtendedPlane();

    static ExtendedPlane FarawayPlane();

    /// Transform the plane by the given matrix.
    void Transform(const FLOATMATRIX4&, bool bSecondar);

    /// Figures out the appropriate quadrilateral for rendering this plane (the
    /// quad's normal will be the plane's normal).
    /// @return true if the returned set of points should be rendered counter
    ///         clockwise.
    bool Quad(const FLOATVECTOR3& vEye, std::vector<FLOATVECTOR3>& quad,
              const float fWidgetSize=0.5f) const;

    /// Sets the plane back to default values.
    void Default(bool bSecondary=false);

    float& d() { return m_Plane.w; }
    const float& d() const { return m_Plane.w; }

    float x() const { return m_Plane.x; }
    float y() const { return m_Plane.y; }
    float z() const { return m_Plane.z; }

    const PLANE<float>& Plane() const { return m_Plane; }

    bool operator ==(const ExtendedPlane &ep) const {
      return m_Plane == ep.m_Plane;
    }

  private:
    void UpdatePlane();

    /// The default / initial settings for the plane and its perpendicular
    /// vector.  Use these when constructing initial copies of an
    /// ExtendedPlane (the default if you're using the default constructor).
    ///@{
    static const PLANE<float> ms_Plane;
    ///@}

    FLOATMATRIX4 GetCompleteTransform() const;

    PLANE<float> m_Plane;         ///< the plane's normal
    FLOATMATRIX4 m_mat[2];        ///< accumulated plane transformations
};
#endif // TUVOK_PLANE_H
