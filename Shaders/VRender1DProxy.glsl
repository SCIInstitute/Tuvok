/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2011 Scientific Computing and Imaging Institute,
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
  \brief Proxy function to provide a unified interface over the VRender
         functions.

  'VRender1D' takes different parameters depending on whether or not we are
  using the 'bias + scale' version or the version that assumes ranges based on
  bit widths.  This version calls the appropriate one based on a runtime
  setting.
*/

uniform int ScaleMethod;
uniform float TFuncBias;      ///< bias for 1D TFqn lookup
uniform float fTransScale;    ///< scale for 1D Transfer function lookup
uniform float fStepScale;     ///< opacity correction quotient

vec4 VRender1D_BitWidth(const vec3 tex_pos, in float scale, in float opac);
vec4 VRender1D_BiasScale(const vec3 tex_pos, in float scale, in float bias,
                         in float opac);

vec4 VRender1D(const vec3 tex_pos) {
  if(0 == ScaleMethod) {
    // default / normal operation.
    return VRender1D_BitWidth(tex_pos, fTransScale, fStepScale);
  } else if(1 == ScaleMethod) {
    // bias and scale method
    return VRender1D_BiasScale(tex_pos, fTransScale, TFuncBias, fStepScale);
  }
  // error case: make things red.
  return vec4(1.0, 0.0, 0.0, 0.01);
}
