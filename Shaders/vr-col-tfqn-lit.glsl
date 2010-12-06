/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2010 Scientific Computing and Imaging Institute,
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
  \author  Tom Fogal
           University of Utah
  \brief   Performs volume rendering w/o applying a transfer function
*/

uniform sampler1D texTrans; ///< the 1D Transfer function

vec4 sampleVolume(vec3);
vec3 ComputeNormal(vec3 vCenter, vec3 StepSize, vec3 DomainScale);
vec3 Lighting(vec3 vPosition, vec3 vNormal, vec3 vLightAmbient,
              vec3 vLightDiffuse, vec3 vLightSpecular, vec3 vLightDir);


// TODO:
// do something with scale and bias 

vec4 VRender1DLit(const vec3 tex_pos,
                  in float tf_scale,
#if defined(BIAS_SCALE)
                  in float tf_bias,
#endif
                  in float opacity_correction,
                  const vec3 voxel_step_size,
                  const vec3 domain_scale,
                  const vec3 position,
                  const vec3 l_ambient,
                  const vec3 l_diffuse,
                  const vec3 l_specular,
                  const vec3 l_direction)
{
  vec4 v = sampleVolume(tex_pos);
  v = v * texture1D(texTrans, (v.r+v.g+v.b)/3.0);
  v.a = 1.0 - pow(1.0 - v.a, opacity_correction); // opacity correction

  vec3 normal = ComputeNormal(tex_pos, voxel_step_size, domain_scale);
  vec3 light_color = Lighting(position, normal, l_ambient,
                              l_diffuse * v.xyz, l_specular, l_direction);
  return vec4(light_color.x, light_color.y, light_color.z, v.a);
}
