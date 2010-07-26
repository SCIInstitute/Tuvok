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
  \file    Mesh-FS.glsl
  \author    Jens Krueger
        SCI Institute
        University of Utah
  \date    June 2010
*/


varying vec3 vPosition;
varying vec3 normal;

vec4 TraversalOrderDepColor(vec4 color);

void main(void)
{

  if (gl_MultiTexCoord0.a < 1.5) {
    if (gl_Normal == vec3(2,2,2)) {
      normal = gl_Normal;
    } else {
      normal = gl_NormalMatrix * gl_Normal;
    }
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    vec4 color = TraversalOrderDepColor(gl_Color);
    gl_FrontColor = color;
    gl_BackColor = color;
  } else {
    gl_Position = gl_ProjectionMatrix * gl_Vertex;    
  }
  gl_TexCoord[0] = gl_MultiTexCoord0;
  vPosition = gl_Vertex.xyz;

}