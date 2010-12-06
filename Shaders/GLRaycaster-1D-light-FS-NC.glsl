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

uniform sampler1D texTrans;    ///< the 1D Transfer function
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

#ifdef BIAS_SCALE
  uniform float TFuncBias;    ///< bias amount for transfer func
  vec4 VRender1DLit(const vec3 tex_pos,
                    in float tf_scale,
                    in float tf_bias,
                    in float opacity_correction,
                    const vec3 voxel_step_size,
                    const vec3 domain_scale,
                    const vec3 position,
                    const vec3 l_ambient,
                    const vec3 l_diffuse,
                    const vec3 l_specular,
                    const vec3 l_direction);
#else
  vec4 VRender1DLit(const vec3 tex_pos,
                    in float tf_scale,
                    in float opacity_correction,
                    const vec3 voxel_step_size,
                    const vec3 domain_scale,
                    const vec3 position,
                    const vec3 l_ambient,
                    const vec3 l_diffuse,
                    const vec3 l_specular,
                    const vec3 l_direction);
#endif
vec4 sampleVolume(vec3 coords); 
vec4 UnderCompositing(vec4 src, vec4 dst);

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
#if defined(BIAS_SCALE)
  vec4 stepColor = VRender1DLit(vCurrentPosTex, fTransScale, TFuncBias,
                              fStepScale, vVoxelStepsize,
                              vDomainScale, vCurrentPos,
                              vLightAmbient, vLightDiffuse,
                              vLightSpecular, vLightDir);
#else
  vec4 stepColor = VRender1DLit(vCurrentPosTex, fTransScale,
                              fStepScale, vVoxelStepsize,
                              vDomainScale, vCurrentPos,
                              vLightAmbient, vLightDiffuse,
                              vLightSpecular, vLightDir);
#endif

    vColor = UnderCompositing(stepColor, vColor);

    vCurrentPos    += fRayInc;
    vCurrentPosTex += vRayIncTex;

    if (vColor.a >= 0.99) break;
  }

  gl_FragColor = vColor;
}
