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

//!    File   : OBJGeoConverter.cpp
//!    Author : Jens Krueger
//!             IVCI & DFKI & MMCI, Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : July 2010
//
//!    Copyright (C) 2010 DFKI, MMCI, SCI Institute

#include "OBJGeoConverter.h"
#include "Controller/Controller.h"
#include "SysTools.h"
#include "Mesh.h"
#include <fstream>
#include "TuvokIOError.h"

using namespace tuvok;

OBJGeoConverter::OBJGeoConverter()
{
  m_vConverterDesc = "Alias|Wavefront Object File";
  m_vSupportedExt.push_back("OBJ");
}

inline int OBJGeoConverter::CountOccurences(const std::string& str, const std::string& substr) {
  size_t found = str.find_first_of(substr);
  int count = 0;
  while (found!=std::string::npos)
  {
    count++;
    found=str.find_first_of(substr,found+1);
  }
  return count;
}


inline std::string OBJGeoConverter::TrimToken(const std::string& Src,
                                              const std::string& c,
                                              bool bOnlyFirst)
{
  size_t off = Src.find_first_of(c);
  if (off == std::string::npos) off = 0;
  if (bOnlyFirst) {
    return Src.substr(off+1);
  } else {
    size_t p1 = Src.find_first_not_of(c,off);
    if (p1 == std::string::npos) return std::string();
    return Src.substr(p1);
  }
}


Mesh* OBJGeoConverter::ConvertToMesh(const std::string& strFilename) {

  bool bFlipVertices = false;

  VertVec       vertices;
  NormVec       normals;
  TexCoordVec   texcoords;
  ColorVec      colors;     // no used here, passed on as empty vector

  IndexVec      VertIndices;
  IndexVec      NormalIndices;
  IndexVec      TCIndices;
  IndexVec      COLIndices;  // no used here, passed on as empty vector

	std::ifstream fs;
	std::string line;

	fs.open(strFilename.c_str());
  if (fs.fail()) {
    // hack, we really want some kind of 'file not found' exception.
    throw tuvok::io::DSOpenFailed(strFilename.c_str(), __FILE__, __LINE__);
  }

  float x,y,z;

	while (!fs.fail()) {
		getline(fs, line);
		if (fs.fail()) break; // no more lines to read
    line = SysTools::ToLowerCase(SysTools::TrimStr(line));

    std::string linetype = SysTools::TrimStrRight(line.substr(0,2));
    if (linetype == "#") continue; // skip comment lines

    line = SysTools::TrimStrLeft(line.substr(linetype.length()));

		if (linetype == "v") { // vertex attrib found
				x = float(atof(line.c_str()));
				line = TrimToken(line);
				y = float(atof(line.c_str()));
				line = TrimToken(line);
				z = float(atof(line.c_str()));
				vertices.push_back(FLOATVECTOR3(x,y,(bFlipVertices) ? -z : z));
  	} else
	  if (linetype == "vt") {  // vertex texcoord found
			x = float(atof(line.c_str()));
		  line = TrimToken(line);
			y = float(atof(line.c_str()));
			texcoords.push_back(FLOATVECTOR2(x,y));
		} else
    if (linetype == "vn") { // vertex normal found
			x = float(atof(line.c_str()));
      line = TrimToken(line);
			y = float(atof(line.c_str()));
      line = TrimToken(line);
      z = float(atof(line.c_str()));
      FLOATVECTOR3 n(x,y,z);
      n.normalize();
      normals.push_back(n);
		} else
    if (linetype == "f") { // face found
      int indices[9] = {0,0,0,0,0,0,0,0,0};
      int count = CountOccurences(line,"/");

      switch (count) {
        case 0 : {
              indices[0] = atoi(line.c_str());
              line = TrimToken(line);
              indices[1] = atoi(line.c_str());
              line = TrimToken(line);
              indices[2] = atoi(line.c_str());
              VertIndices.push_back(indices[0]-1);
              VertIndices.push_back(indices[1]-1);
              VertIndices.push_back(indices[2]-1);
              break;
             }
        case 3 : {
              indices[0] = atoi(line.c_str());
              line = TrimToken(line,"/");
              indices[1] = atoi(line.c_str());
              line = TrimToken(line);
              indices[2] = atoi(line.c_str());
              line = TrimToken(line,"/");
              indices[3] = atoi(line.c_str());
              line = TrimToken(line);
              indices[4] = atoi(line.c_str());
              line = TrimToken(line,"/");
              indices[5] = atoi(line.c_str());
              line = TrimToken(line);
              VertIndices.push_back(indices[0]-1);
              VertIndices.push_back(indices[2]-1);
              VertIndices.push_back(indices[4]-1);

              TCIndices.push_back(indices[1]-1);
              TCIndices.push_back(indices[3]-1);
              TCIndices.push_back(indices[5]-1);
              break;
             }
        case 6 : {
              indices[0] = atoi(line.c_str());
              line = TrimToken(line,"/",true);
              indices[1] = atoi(line.c_str());
              line = TrimToken(line,"/",true);
              indices[2] = atoi(line.c_str());
              line = TrimToken(line);
              indices[3] = atoi(line.c_str());
              line = TrimToken(line,"/",true);
              indices[4] = atoi(line.c_str());
              line = TrimToken(line,"/",true);
              indices[5] = atoi(line.c_str());
              line = TrimToken(line);
              indices[6] = atoi(line.c_str());
              line = TrimToken(line,"/",true);
              indices[7] = atoi(line.c_str());
              line = TrimToken(line,"/",true);
              indices[8] = atoi(line.c_str());
              line = TrimToken(line);

              VertIndices.push_back(indices[0]-1);
              VertIndices.push_back(indices[3]-1);
              VertIndices.push_back(indices[6]-1);

              TCIndices.push_back(indices[1]-1);
              TCIndices.push_back(indices[4]-1);
              TCIndices.push_back(indices[7]-1);

              NormalIndices.push_back(indices[2]-1);
              NormalIndices.push_back(indices[5]-1);
              NormalIndices.push_back(indices[8]-1);
              break;
             }
      }
    }
  }
	fs.close();

  std::string desc = m_vConverterDesc + " data converted from " + SysTools::GetFilename(strFilename);

  Mesh* m = new Mesh(vertices,normals,texcoords,colors,
                     VertIndices,NormalIndices,TCIndices,COLIndices,
                     false, false, desc, Mesh::MT_TRIANGLES);
  return m;
}
