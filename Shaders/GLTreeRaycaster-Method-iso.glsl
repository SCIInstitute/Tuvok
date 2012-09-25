#version 420 core

uniform float fIsoval;
uniform vec3 vDomainScale;

float samplePool(vec3 coords);
vec3 ComputeNormal(vec3 vCenter, vec3 StepSize, vec3 DomainScale);

bool GetVolumeHit(vec3 currentPoolCoords, out vec4 color) {
  color = vec4(1.0, 1.0, 1.0, 1.0);
  return samplePool(currentPoolCoords) >= fIsoval;
}

vec3 GetVolumeNormal(vec3 currentPoolCoords, vec3 sampleDelta) {
  return ComputeNormal(currentPoolCoords, sampleDelta, vDomainScale);
}


/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2012 Interactive Visualization and Data Analysis Group.


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