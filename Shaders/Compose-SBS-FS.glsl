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

//!    File   : Compose-SBS-FS.glsl
//!    Author : Jens Krueger
//!             IVDA, MMCI, DFKI Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : July 2010
//
//!    Copyright (C) 2010 IVDA, MMC, DFKI, SCI Institute

uniform sampler2D texRightEye;
uniform sampler2D texLeftEye;
uniform float fSplitCoord;

void main(void) {
  if (gl_TexCoord[0].x < fSplitCoord)
    gl_FragColor = texture2D(texLeftEye,
                             vec2(gl_TexCoord[0].x*2.0,gl_TexCoord[0].y));
  else
    gl_FragColor = texture2D(texRightEye,
                             vec2((gl_TexCoord[0].x-fSplitCoord)*2.0,gl_TexCoord[0].y));
}
