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
  \file    GLSBVR-Color-FS.glsl
  \author    Jens Krueger
        SCI Institute
        University of Utah
  \version  1.0
  \date    October 2008
*/

vec4 sampleVolume(vec3 coords);

uniform vec3 vVoxelStepsize;  ///< Stepsize (in texcoord) to get to the next voxel
uniform float fIsoval;        ///< the isovalue
uniform vec3 vDomainScale;

varying vec3 vPosition;

vec3 ComputeNormal(vec3 vHitPosTex, vec3 StepSize,
                   vec3 DomainScale);

void main(void)
{
  /// get volume value
  vec4 fVolumVal = sampleVolume(gl_TexCoord[0].xyz);

  // if we hit (or shot over) an isosurface
  if (fVolumVal.a >= fIsoval) {
    // store surface position and red channel
    gl_FragData[0] = vec4(vPosition.xyz,fVolumVal.r+1.0); // red chanel plus one (to make sure this value is not accideantially zero)

    // store normal and green and blue channel
    gl_FragData[1] = vec4(ComputeNormal(gl_TexCoord[0].xyz, vVoxelStepsize,
                                        vDomainScale),
                          floor(fVolumVal.g * 512.0) + fVolumVal.b);
  } else {
    discard;
  }
}
