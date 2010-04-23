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
  \file  lighting.glsl
*/

vec3 Lighting(vec3 vPosition, vec3 vNormal, vec3 vLightAmbient,
              vec3 vLightDiffuse, vec3 vLightSpecular, vec3 vLightDir) {
	vNormal.z = abs(vNormal.z);

	vec3 vViewDir    = normalize(vec3(0.0,0.0,0.0)-vPosition);
	vec3 vReflection = normalize(reflect(vViewDir, vNormal));
	return clamp(
    vLightAmbient +
    vLightDiffuse * max(abs(dot(vNormal, -vLightDir)),0.0) +
		vLightSpecular * pow(max(dot(vReflection, vLightDir),0.0),8.0), 0.0,1.0
  );
}

vec3 ComputeNormal(vec3 vHitPosTex, sampler3D volume, vec3 StepSize,
                   vec3 DomainScale) {
  float fVolumValXp = texture3D(volume, vHitPosTex+vec3(+StepSize.x,0,0)).x;
  float fVolumValXm = texture3D(volume, vHitPosTex+vec3(-StepSize.x,0,0)).x;
  float fVolumValYp = texture3D(volume, vHitPosTex+vec3(0,-StepSize.y,0)).x;
  float fVolumValYm = texture3D(volume, vHitPosTex+vec3(0,+StepSize.y,0)).x;
  float fVolumValZp = texture3D(volume, vHitPosTex+vec3(0,0,+StepSize.z)).x;
  float fVolumValZm = texture3D(volume, vHitPosTex+vec3(0,0,-StepSize.z)).x;
  vec3 vGradient = vec3(fVolumValXm - fVolumValXp,
                        fVolumValYp - fVolumValYm,
                        fVolumValZm - fVolumValZp);
  vec3 vNormal     = gl_NormalMatrix * (vGradient * DomainScale);
  float l = length(vNormal); if (l>0.0) vNormal /= l; // secure normalization
  return vNormal;
}

vec4 ColorBlend(vec4 src, vec4 dst) {
	vec4 result = dst;
	result.rgb   += src.rgb*(1.0-dst.a)*src.a;
	result.a     += (1.0-dst.a)*src.a;
	return result;
}
