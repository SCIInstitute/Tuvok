#version 420 core

layout(binding=0) uniform sampler1D dummy0;
layout(binding=1) uniform sampler1D dummy1;
layout(binding=2) uniform sampler1D transferFunction;

uniform float fTransScale;

uniform vec3 vLightAmbient;
uniform vec3 vLightDiffuse;
uniform vec3 vLightSpecular;
uniform vec3 vModelSpaceLightDir;
uniform vec3 vModelSpaceEyePos;
uniform vec3 vDomainScale;

float samplePool(vec3 coords);
vec3 Lighting(vec3 vEyePos, vec3 vPosition, vec3 vNormal, vec3 vLightAmbient,
              vec3 vLightDiffuse, vec3 vLightSpecular, vec3 vLightDir);

vec3 ComputeNormal(vec3 vCenter, vec3 StepSize, vec3 DomainScale);

vec4 ComputeColorFromVolume(vec3 currentPoolCoords, vec3 modelSpacePosition, vec3 sampleDelta) {
  // fetch volume
  float data = samplePool(currentPoolCoords);

  // apply 1D TF
  vec4 color = texture(transferFunction, data*fTransScale);

  // compute normal
  vec3 normal = ComputeNormal(currentPoolCoords, sampleDelta, vDomainScale);

  // compute lighting
  vec3 litColor = Lighting(vModelSpaceEyePos, modelSpacePosition, normal, vLightAmbient, color.rgb*vLightDiffuse, vLightSpecular, vModelSpaceLightDir);
  return vec4(litColor,color.a);
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