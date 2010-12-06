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
  \file    GLSBVR-1D-light-FS.glsl
  \author    Jens Krueger
        SCI Institute
        University of Utah
  \version  1.0
  \date    October 2008
*/
uniform sampler1D texTrans; ///< the 1D Transfer function
uniform float fTransScale;    ///< scale for 1D Transfer function lookup
uniform float fStepScale;   ///< opacity correction quotient
uniform vec3 vVoxelStepsize;  ///< Stepsize (in texcoord) to get to the next voxel
uniform vec3 vDomainScale;

uniform vec3 vLightAmbient;
uniform vec3 vLightDiffuse;
uniform vec3 vLightSpecular;
uniform vec3 vLightDir;

varying vec3 vPosition;

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

vec4 TraversalOrderDepColor(const vec4 color);

void main(void)
{
#if defined(BIAS_SCALE)
  gl_FragColor = VRender1DLit(
    gl_TexCoord[0].xyz, fTransScale, TFuncBias, fStepScale,
    vVoxelStepsize, vDomainScale,
    vPosition.xyz, vLightAmbient, vLightDiffuse, vLightSpecular, vLightDir
  );
#else
  gl_FragColor = VRender1DLit(
    gl_TexCoord[0].xyz, fTransScale, fStepScale,
    vVoxelStepsize, vDomainScale,
    vPosition.xyz, vLightAmbient, vLightDiffuse, vLightSpecular, vLightDir
  );
#endif


  // pre-multiplication of alpha, if needed.
  gl_FragColor = TraversalOrderDepColor(gl_FragColor);
}
