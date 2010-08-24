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
  \file    GLRaycaster-1D-light-FS-NC.glsl
  \author    Jens Krueger
        SCI Institute
        University of Utah
  \version  1.0
  \date    October 2008
*/

vec4 sampleVolume(vec3 coords);

uniform sampler1D texTrans1D;    ///< the 1D Transfer function
uniform sampler2D texRayExit; ///< the backface (or ray exit point) texture in texcoords
uniform sampler2D texRayExitPos; ///< the backface (or ray exit point) texture in eyecoords
uniform float fTransScale;       ///< scale for 1D Transfer function lookup
uniform float fStepScale;        ///< opacity correction quotient
uniform vec3 vVoxelStepsize;     ///< Stepsize (in texcoord) to get to the next voxel
uniform vec2 vScreensize;        ///< the size of the screen in pixels
uniform float fRayStepsize;      ///< stepsize along the ray
uniform vec3 vDomainScale;

uniform vec3 vLightAmbient;
uniform vec3 vLightDiffuse;
uniform vec3 vLightSpecular;
uniform vec3 vLightDir;

uniform vec4 vClipPlane;

varying vec3 vEyePos;

vec3 Lighting(vec3 vPosition, vec3 vNormal, vec3 vLightAmbient,
              vec3 vLightDiffuse, vec3 vLightSpecular, vec3 vLightDir);
vec3 ComputeNormal(vec3 vHitPosTex, vec3 StepSize,
                   vec3 DomainScale);
vec4 ColorBlend(vec4 src, vec4 dst);

void main(void)
{
  // compute the coordinates to look up the previous pass
  vec2 vFragCoords = vec2(gl_FragCoord.x / vScreensize.x , gl_FragCoord.y / vScreensize.y);

  // compute the ray parameters
  vec3  vRayEntry    = vEyePos;
  vec3  vRayExit     = texture2D(texRayExitPos, vFragCoords).xyz;

  vec3  vRayEntryTex = (gl_TextureMatrix[0] * vec4(vRayEntry,1.0)).xyz;
  vec3  vRayExitTex  = (gl_TextureMatrix[0] * vec4(vRayExit,1.0)).xyz;
  vec3  vRayDir      = vRayExit - vRayEntry;

  float fRayLength = length(vRayDir);
  vRayDir /= fRayLength;

  // compute the maximum number of steps before the domain is left
  int iStepCount = int(fRayLength/fRayStepsize)+1;

  vec3  fRayInc    = vRayDir*fRayStepsize;
  vec3  vRayIncTex = (vRayExitTex-vRayEntryTex)/(fRayLength/fRayStepsize);

  // do the actual raycasting
  vec4  vColor = vec4(0.0,0.0,0.0,0.0);
  vec3  vCurrentPosTex = vRayEntryTex;
  vec3  vCurrentPos    = vRayEntry;
  for (int i = 0;i<iStepCount;i++) {
    float fVolumVal = sampleVolume( vCurrentPosTex).x;

    /// apply 1D transfer function
    vec4  vTransVal = texture1D(texTrans1D, fVolumVal*fTransScale);

    // compute lighting
    vec3 vNormal     = ComputeNormal(vCurrentPosTex, vVoxelStepsize,
                                     vDomainScale);
    vec3 vLightColor = Lighting(vCurrentPos, vNormal, vLightAmbient,
                                vLightDiffuse*vTransVal.xyz, vLightSpecular,
                                vLightDir);

    /// apply opacity correction
    vTransVal.a = 1.0 - pow(1.0 - vTransVal.a, fStepScale);
    vTransVal = vec4(vLightColor.x, vLightColor.y, vLightColor.z, vTransVal.a);
    vColor = ColorBlend(vTransVal,vColor);

    vCurrentPos    += fRayInc;
    vCurrentPosTex += vRayIncTex;

    if (vColor.a >= 0.99) break;
  }

  gl_FragColor = vColor;
}
