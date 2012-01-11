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
  \brief   Function for performing "standard" volume rendering.
*/

uniform sampler3D texVolume;  ///< the data volume
uniform sampler1D texTrans; ///< the 1D Transfer function

vec4 sampleVolume(vec3);

/* bias and scale method for mapping a TF to a value. */
vec4 bias_scale(const vec3 tex_pos, const float bias, const float scale)
{
  float vol_val = sampleVolume(tex_pos).x;
  vol_val = (vol_val + bias) / scale;

  return texture1D(texTrans, vol_val);
}

/* Performs the basic 1D volume rendering; sampling, looking up the value in
 * the LUT (tfqn), and doing opacity correction. */
vec4 VRender1D(const vec3 tex_pos,
               in float tf_scale,
               in float tf_bias,
               in float opacity_correction)
{
  vec4 lut_v = bias_scale(tex_pos, tf_bias, tf_scale);

  // apply opacity correction
  lut_v.a = 1.0 - pow(1.0 - lut_v.a, opacity_correction);

  return lut_v;
}
