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
  \file    GLRaycaster-MIP-Rot-FS.glsl
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \version 1.0
  \date    December 2008
*/

vec4 sampleVolume(vec3 coords);

uniform sampler2D texRayExitPos; ///< the backface (or ray exit point) texture in eyecoords
uniform vec3 vVoxelStepsize;     ///< Stepsize (in texcoord) to get to the next voxel
uniform float fRayStepsize;      ///< stepsize along the ray
uniform vec2 vScreensize;        ///< the size of the screen in pixel

varying vec3 vEyePos;

void main(void)
{
  // compute the coordinates to look up the previous pass
  vec2 vFragCoords = vec2(gl_FragCoord.x / vScreensize.x , gl_FragCoord.y / vScreensize.y);

  // compute the ray parameters
  vec3  vRayEntry    = texture2D(texRayExitPos, vFragCoords).xyz;
  vec3  vRayExit     = vEyePos;
  vec3  vRayEntryTex  = (gl_TextureMatrix[0] * vec4(vRayEntry,1.0)).xyz;
  vec3  vRayExitTex   = (gl_TextureMatrix[0] * vec4(vRayExit,1.0)).xyz;
  float fRayLength    = length(vRayExit - vRayEntry);
  float fRayLengthTex = length(vRayExitTex - vRayEntryTex);

  // compute the maximum number of steps before the domain is left
  int iStepCount = int(fRayLength/fRayStepsize)+1;
  vec3 vRayIncTex = (vRayExitTex-vRayEntryTex)/(fRayLength/fRayStepsize);

  // do the actual raycasting
  float fMaxVal = 0.0;
  vec3  vCurrentPosTex = vRayEntryTex;
  for (int i = 0;i<iStepCount;i++) {
    float fVolumVal = sampleVolume(vCurrentPosTex).x;

    fMaxVal = max(fMaxVal, fVolumVal);
    vCurrentPosTex += vRayIncTex;
  }

  gl_FragColor = vec4(fMaxVal, fMaxVal, fMaxVal, 1.0);
}
