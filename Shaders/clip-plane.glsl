/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2010 Scientific Computing and Imaging Institute,
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

/** \file clip-plane.glsl
 *  \brief implements functions related to clipping.
 */

bool ClipByPlane(inout vec3 vRayEntry, inout vec3 vRayExit,
                 in vec4 clip_plane)
{
  float denom = dot(clip_plane.xyz , (vRayEntry-vRayExit));
  float tPlane = (dot(clip_plane.xyz, vRayEntry) + clip_plane.w) / denom;

  if (tPlane > 1.0) {
    if (denom < 0.0)
      return true;
    else
      return false;
  } else {
    if (tPlane < 0.0) {
      if (denom <= 0.0)
        return false;
      else
        return true;
    } else {
      if (denom > 0.0)
        vRayEntry = vRayEntry + (vRayExit-vRayEntry)*tPlane;
      else
        vRayExit  = vRayEntry + (vRayExit-vRayEntry)*tPlane;
      return true;
    }
  }
}
