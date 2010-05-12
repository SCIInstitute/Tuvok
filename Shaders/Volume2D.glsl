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
  \file    Volume2D.glsl
  \author  Jens Krueger
           DFKI Saarbrücken & SCI Institute, University of Utah
  \version 1.0
  \date    May 2010
*/

uniform sampler2D texSlicem1;
uniform sampler2D texSlice0;
uniform sampler2D texSlice1;

vec4 sampleVolume(vec3 coords){
	vec4 v0 = texture2D(texSlice0, coords.xy);
	vec4 v1 = texture2D(texSlice1, coords.xy);
	
	return (1.0-coords.z) * v0 + coords.z * v1;
}

vec3 ComputeGradient(vec3 vCenter, vec3 StepSize) {
  float fVolumValXp = sampleVolume( vCenter+vec3(+StepSize.x,0,0)).x;
  float fVolumValXm = sampleVolume( vCenter+vec3(-StepSize.x,0,0)).x;
  float fVolumValYp = sampleVolume( vCenter+vec3(0,-StepSize.y,0)).x;
  float fVolumValYm = sampleVolume( vCenter+vec3(0,+StepSize.y,0)).x;

  float t = vCenter.z;

  float fVolumValZm = (1.0-t) * texture2D(texSlicem1, vCenter.xy).x +
                         t    * texture2D(texSlice0, vCenter.xy).x;
  float fVolumValZp = (1.0-t) * texture2D(texSlice0, vCenter.xy).x +
                         t    * texture2D(texSlice1, vCenter.xy).x;

  return (gl_TextureMatrix[0] *
          vec4((fVolumValXm - fVolumValXp)/2.0f,
               (fVolumValYp - fVolumValYm)/2.0f,
                fVolumValZm - fVolumValZp,1.0)).xyz;
}

vec3 ComputeNormal(vec3 vCenter, vec3 StepSize, vec3 DomainScale) {
  vec3 vGradient =  ComputeGradient(vCenter, StepSize);
  vec3 vNormal     = gl_NormalMatrix * (vGradient * DomainScale);
  float l = length(vNormal); if (l>0.0) vNormal /= l; // safe normalization
  return vNormal;
}
