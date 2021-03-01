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

/**
  \author  Tom Fogal
           University of Utah
  \brief   Performs volume rendering w/o applying a transfer function
*/

uniform sampler1D texTrans; ///< the 1D Transfer function

vec4 sampleVolume(vec3);

// since this is color data... we aren't really using a "bit width"-based idea
// to scale the transfer function.  However this matches the VRender1D_BitWidth
// interface as opposed to the _BiasScale interface, so we use the name anyway.
vec4 VRender1D_BitWidth(const vec3 tex_pos,
                        in float tf_scale,
                        in float opacity_correction)
{
  vec4 v = sampleVolume(tex_pos);
  v = v * texture1D(texTrans, (v.r+v.g+v.b)/3.0);
  v.a = 1.0 - pow(1.0 - v.a, opacity_correction); // opacity correction
  return v;
}
