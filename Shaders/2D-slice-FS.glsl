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
  \file    2D-slice-FS.glsl
  \author    Jens Krueger
        SCI Institute
        University of Utah
  \version  1.0
  \date    October 2008
*/

vec4 sampleVolume(vec3 coords);
uniform sampler2D texTrans; ///< the 2D Transfer function
uniform float fTransScale;    ///< value scale for 2D Transfer function lookup
uniform float fGradientScale; ///< gradient scale for 2D Transfer function lookup
uniform vec3 vVoxelStepsize;  ///< Stepsize (in texcoord) to get to the next voxel

void main(void)
{
  /// get volume value
  float fVolumVal = sampleVolume(gl_TexCoord[0].xyz).x;

  /// compute the gradient
  float fVolumValXp  = sampleVolume( gl_TexCoord[0].xyz+vec3(+vVoxelStepsize.x,0,0)).x;
  float fVolumValXm  = sampleVolume( gl_TexCoord[0].xyz+vec3(-vVoxelStepsize.x,0,0)).x;
  float fVolumValYp  = sampleVolume( gl_TexCoord[0].xyz+vec3(0,-vVoxelStepsize.y,0)).x;
  float fVolumValYm  = sampleVolume( gl_TexCoord[0].xyz+vec3(0,+vVoxelStepsize.y,0)).x;
  float fVolumValZp  = sampleVolume( gl_TexCoord[0].xyz+vec3(0,0,+vVoxelStepsize.z)).x;
  float fVolumValZm  = sampleVolume( gl_TexCoord[0].xyz+vec3(0,0,-vVoxelStepsize.z)).x;
  vec3  vGradient = vec3((fVolumValXm-fVolumValXp)/2.0,
                         (fVolumValYp-fVolumValYm)/2.0,
                         (fVolumValZm-fVolumValZp)/2.0);
  float fGradientMag = length(vGradient);

  /// apply 2D transfer function
  vec4  vTransVal = texture2D(texTrans, vec2(fVolumVal*fTransScale, 1.0-fGradientMag*fGradientScale));
  vTransVal.rgb *= vTransVal.a;
  vTransVal.a = 1.0;
  /// write result to fragment color
  gl_FragColor    = vTransVal;
}
