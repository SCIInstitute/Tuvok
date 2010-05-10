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
  \file    Compose-Scanline-FS.glsl
  \author  Jens Krueger
           DFKI & SCI Institute, University of Utah
  \version 1.0
  \date    March 2010
*/

uniform sampler2D texRightEye;
uniform sampler2D texLeftEye;  
uniform vec2 vScreensize;      ///< the size of the screen in pixels

void main(void){
  float fLine = floor(gl_TexCoord[0].y * vScreensize.y);

  if (fLine/2.0 == floor(fLine/2.0))
    gl_FragColor = texture2D(texLeftEye,  gl_TexCoord[0].xy);
  else 
    gl_FragColor = texture2D(texRightEye, gl_TexCoord[0].xy);
}