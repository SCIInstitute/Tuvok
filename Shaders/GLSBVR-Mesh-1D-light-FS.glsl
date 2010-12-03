/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2010 Interactive Visualization and Data Analysis Group.

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

//!    File   : GLSBVR-Mesh-1D-light-FS.glsl
//!    Author : Jens Krueger
//!             IVCI & DFKI & MMCI, Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : July 2010
//
//!    Copyright (C) 2010 DFKI, MMCI, SCI Institute

vec4 sampleVolume(vec3 coords);
vec3 ComputeNormal(vec3 vCenter, vec3 StepSize, vec3 DomainScale);

uniform sampler1D texTrans; ///< the 1D Transfer function
uniform float fTransScale;    ///< scale for 1D Transfer function lookup
uniform float fStepScale;   ///< opacity correction quotient
uniform vec3 vVoxelStepsize;  ///< Stepsize (in texcoord) to get to the next voxel
uniform vec3 vDomainScale;

uniform vec3 vLightAmbient;
uniform vec3 vLightDiffuse;
uniform vec3 vLightSpecular;

uniform vec3 vLightAmbientM;
uniform vec3 vLightDiffuseM;
uniform vec3 vLightSpecularM;

uniform vec3 vLightDir;

varying vec3 vPosition;
varying vec3 normal;

vec3 Lighting(vec3 vPosition, vec3 vNormal, vec3 vLightAmbient,
              vec3 vLightDiffuse, vec3 vLightSpecular, vec3 vLightDir);

vec4 TraversalOrderDepColor(const vec4 color);

void main(void)
{
  if (gl_TexCoord[0].a < 1.5) { // save way of testing for 2
    if (normal == vec3(2,2,2)) {
      gl_FragColor = gl_Color;
    } else {
      vec3 vLightColor = Lighting(vPosition, normal, vLightAmbientM*gl_Color.xyz,
                                  vLightDiffuseM*gl_Color.xyz, vLightSpecularM,
                                  vLightDir);
      gl_FragColor = vec4(vLightColor.x,vLightColor.y,vLightColor.z,gl_Color.w);
    }
  } else {
    // get volume value
    float fVolumVal = sampleVolume(gl_TexCoord[0].xyz).x;
    vec4  vTransVal = texture1D(texTrans, fVolumVal*fTransScale);

    // compute the gradient/normal
    vec3 vNormal = ComputeNormal(gl_TexCoord[0].xyz,vVoxelStepsize,vDomainScale);

    vec3 vLightColor = Lighting(vPosition.xyz, vNormal, vLightAmbient,
                                vLightDiffuse*vTransVal.xyz, vLightSpecular,
                                vLightDir);

    /// apply opacity correction
    vTransVal.a = 1.0 - pow(1.0 - vTransVal.a, fStepScale);

    /// write result to fragment color
    gl_FragColor    = vec4(vLightColor.x, vLightColor.y, vLightColor.z, vTransVal.a);
  }
  gl_FragColor = TraversalOrderDepColor(gl_FragColor);
}
