#version 420 core

layout(binding=2) uniform sampler2D transferFunction;

uniform float fTransScale;
uniform float fGradientScale;

uniform vec3 vLightAmbient;
uniform vec3 vLightDiffuse;
uniform vec3 vLightSpecular;
uniform vec3 vModelSpaceLightDir;
uniform vec3 vModelSpaceEyePos;
uniform vec3 vDomainScale;

vec4 samplePool4(vec3 coords);
vec3 ComputeGradientAlpha(vec3 vCenter, vec3 sampleDelta);
vec3 Lighting(vec3 vEyePos, vec3 vPosition, vec3 vNormal, vec3 vLightAmbient,
              vec3 vLightDiffuse, vec3 vLightSpecular, vec3 vLightDir);

vec4 ComputeColorFromVolume(vec3 currentPoolCoords, vec3 modelSpacePosition, vec3 sampleDelta) {
  // fetch volume
  vec4 data = samplePool4(currentPoolCoords);

  // compute the gradient
  vec3  vGradient = ComputeGradientAlpha(currentPoolCoords, sampleDelta);
  float fGradientMag = length(vGradient);

  // Apply 2D TF
  data.a = texture(transferFunction, vec2(data.a*fTransScale, 1.0-fGradientMag*fGradientScale)).a;

  // compute normal
  vec3 normal = vDomainScale * ((fGradientMag > 0) ? vGradient/fGradientMag : vGradient);

  // compute lighting
  vec3 litColor = Lighting(vModelSpaceEyePos, modelSpacePosition, normal, vLightAmbient, data.rgb*vLightDiffuse, vLightSpecular, vModelSpaceLightDir);
  return vec4(litColor,data.a);
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