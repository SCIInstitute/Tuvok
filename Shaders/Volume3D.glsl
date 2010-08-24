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
  \file    Volume3D.glsl
  \author  Jens Krueger
           DFKI Saarbrücken & SCI Institute, University of Utah
  \version 1.0
  \date    May 2010
*/

uniform sampler3D texVolume;  ///< the data volume
uniform sampler1D texTrans1D; ///< the 1D Transfer function

vec4 sampleVolume(vec3 coords) {
  return texture3D(texVolume, coords);
}

vec3 ComputeGradient(vec3 vCenter, vec3 StepSize) {
  float fVolumValXp = sampleVolume(vCenter+vec3(+StepSize.x,0,0)).x;
  float fVolumValXm = sampleVolume(vCenter+vec3(-StepSize.x,0,0)).x;
  float fVolumValYp = sampleVolume(vCenter+vec3(0,-StepSize.y,0)).x;
  float fVolumValYm = sampleVolume(vCenter+vec3(0,+StepSize.y,0)).x;
  float fVolumValZp = sampleVolume(vCenter+vec3(0,0,+StepSize.z)).x;
  float fVolumValZm = sampleVolume(vCenter+vec3(0,0,-StepSize.z)).x;
  return vec3(fVolumValXm - fVolumValXp,
              fVolumValYp - fVolumValYm,
              fVolumValZm - fVolumValZp) / 2.0;
}

vec3 ComputeNormal(vec3 vCenter, vec3 StepSize, vec3 DomainScale) {
  vec3 vGradient = ComputeGradient(vCenter, StepSize);
  vec3 vNormal   = gl_NormalMatrix * (vGradient * DomainScale);
  float l = length(vNormal); if (l>0.0) vNormal /= l; // safe normalization
  return vNormal;
}

/* Performs the basic 1D volume rendering; sampling, looking up the value in
 * the LUT (tfqn), and doing opacity correction. */
vec4 VRender1D(const vec3 tex_pos,
               in float tf_scale,
               in float opacity_correction)
{
  // get data value
  float v = sampleVolume(tex_pos).x;

  // apply 1D TFqn
  vec4 lut_v = texture1D(texTrans1D, v*tf_scale);

  // apply opacity correction
  lut_v.a = 1.0 - pow(1.0 - lut_v.a, opacity_correction);

  return lut_v;
}
