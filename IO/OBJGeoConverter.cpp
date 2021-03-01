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

OBJGeoConverter::OBJGeoConverter() :
  AbstrGeoConverter()
{
  m_vConverterDesc = L"Wavefront Object File";
  m_vSupportedExt.push_back(L"OBJ");
  m_vSupportedExt.push_back(L"OBJX");
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

std::shared_ptr<Mesh>
OBJGeoConverter::ConvertToMesh(const std::wstring& strFilename) {
  bool bFlipVertices = false;

  VertVec       vertices;
  NormVec       normals;
  TexCoordVec   texcoords;
  ColorVec      colors;

  IndexVec      VertIndices;
  IndexVec      NormalIndices;
  IndexVec      TCIndices;
  IndexVec      COLIndices;

	std::ifstream fs;
	std::string line;

	fs.open(SysTools::toNarrow(strFilename).c_str());
  if (fs.fail()) {
    // hack, we really want some kind of 'file not found' exception.
    throw tuvok::io::DSOpenFailed(SysTools::toNarrow(strFilename).c_str(), __FILE__, __LINE__);
  }

  float x,y,z,w;
  size_t iVerticesPerPoly = 0;

  fs.seekg(0,std::ios::end);
  std::streamoff iFileLength = fs.tellg();
  fs.seekg(0,std::ios::beg);
  size_t iBytesRead = 0;
  size_t iLine = 0;

  while (!fs.fail()) {
		getline(fs, line);

    iBytesRead += line.size() + 1;
    iLine++;

		if (fs.fail()) break; // no more lines to read
    line = SysTools::ToLowerCase(SysTools::TrimStr(line));

    // remove comments
    size_t cPos = line.find_first_of('#');
    if (cPos != std::string::npos) line = line.substr(0,cPos);
    line = SysTools::TrimStr(line);
    if (line.length() == 0) continue; // skips empty and comment lines

    // find the linetype
    size_t off = line.find_first_of(" \r\n\t");
    if (off == std::string::npos) continue;
    std::string linetype = SysTools::TrimStrRight(line.substr(0,off));

    line = SysTools::TrimStr(line.substr(linetype.length()));

    if (linetype == "o") { 
      WARNING("Skipping Object Tag in OBJ file");
    } else
    if (linetype == "mtllib") { 
      WARNING("Skipping Material Library Tag in OBJ file");
    } else
		if (linetype == "v") { // vertex attrib found
      std::vector< std::string > pos = SysTools::Tokenize(line, SysTools::PM_NONE);

      if (pos.size() < 3) {
        WARNING("Found broken v tag (to few coordinates, "
                "filling with zeroes");
        x = (pos.size() > 0) ? SysTools::FromString<float>(pos[0]) : 0.0f;
	      y = (pos.size() > 1) ? SysTools::FromString<float>(pos[1]) : 0.0f;
	      z = 0.0f;
      } else {
        x = SysTools::FromString<float>(pos[0]);
	      y = SysTools::FromString<float>(pos[1]);
	      z = SysTools::FromString<float>(pos[2]);

        if (pos.size() >= 6) {
          // this is a "meshlab extended" obj file that includes vertex colors
	        float r = SysTools::FromString<float>(pos[3]);
	        float g = SysTools::FromString<float>(pos[4]);
	        float b = SysTools::FromString<float>(pos[5]);
          float a = (pos.size() > 6) ? SysTools::FromString<float>(pos[6]):1.0f;
          colors.push_back(FLOATVECTOR4(r,g,b,a));
        } else if (pos.size() > 3) {
          // file specifies homogeneous coordinate
	        float hc = SysTools::FromString<float>(pos[3]);
          if (hc != 0) {
            x /= hc;
            y /= hc;
            z /= hc;
          }
        }
      }
      vertices.push_back(FLOATVECTOR3(x,y,(bFlipVertices) ? -z : z));

  	} else
	  if (linetype == "vt") {  // vertex texcoord found
      x = float(atof(GetToken(line).c_str()));
			y = float(atof(GetToken(line).c_str()));
			texcoords.push_back(FLOATVECTOR2(x,y));
		} else
	  if (linetype == "vc") {  // vertex color found
      x = float(atof(GetToken(line).c_str()));
			y = float(atof(GetToken(line).c_str()));
			z = float(atof(GetToken(line).c_str()));
			w = float(atof(GetToken(line).c_str()));
			colors.push_back(FLOATVECTOR4(x,y,z,w));
		} else
    if (linetype == "vn") { // vertex normal found
      x = float(atof(GetToken(line).c_str()));
			y = float(atof(GetToken(line).c_str()));
			z = float(atof(GetToken(line).c_str()));
      FLOATVECTOR3 n(x,y,z);
      n.normalize();
      normals.push_back(n);
		} else
    if (linetype == "f" || linetype == "l") { // face or line found
      size_t offset = line.find_first_of(" \r\n\t");
      if (offset == std::string::npos) continue;
      std::string analysis = SysTools::TrimStrRight(line.substr(0, offset));
      int count = CountOccurences(analysis,"/");

      IndexVec v, n, t, c;
      
      while (line.length() > 0)  {
        switch (count) {
          case 0 : {
                int vI = atoi(GetToken(line).c_str())-1;
                v.push_back(vI);
                break;
               }
          case 1 : {
                int vI = atoi(GetToken(line,"/",true).c_str())-1;
                v.push_back(vI);
                int vT = atoi(GetToken(line).c_str())-1;
                t.push_back(vT);
                line = TrimToken(line);
                break;
               }
          case 2 : {
                int vI = atoi(GetToken(line,"/",true).c_str())-1;
                v.push_back(vI);
                if (line[0] != '/') {
                  int vT = atoi(GetToken(line,"/",true).c_str())-1;
                  t.push_back(vT);
                }else line = TrimToken(line,"/",true);
                int vN = atoi(GetToken(line).c_str())-1;
                n.push_back(vN);
                break;
               }
          case 3 : {
                int vI = atoi(GetToken(line,"/",true).c_str())-1;
                v.push_back(vI);
                if (line[0] != '/') {
                  int vT = atoi(GetToken(line,"/",true).c_str())-1;
                  t.push_back(vT);
                }else line = TrimToken(line,"/",true);
                if (line[0] != '/') {
                  int vN = atoi(GetToken(line,"/",true).c_str())-1;
                  n.push_back(vN);
                } else line = TrimToken(line,"/",true);
                int vC = atoi(GetToken(line).c_str())-1;
                c.push_back(vC);
                break;
               }
        }
        SysTools::TrimStrLeft(line);
      }

      if (v.size() == 1) {
        WARNING("Skipping points in OBJ file");
        continue;
      }

      if (iVerticesPerPoly == 0) iVerticesPerPoly = v.size();

      if (v.size() == 2) {
        if ( iVerticesPerPoly != 2 ) {
          WARNING("Skipping a line in a file that also contains polygons");
          continue;
        }
        AddToMesh(vertices,v,n,t,c,VertIndices,NormalIndices,TCIndices,COLIndices);
      } else {
        if ( iVerticesPerPoly == 2 ) {
          WARNING("Skipping polygon in file that also contains lines");
          continue;
        }
        AddToMesh(vertices,v,n,t,c,VertIndices,NormalIndices,TCIndices,COLIndices);
      }

    } else {
      WARNING("Skipping unknown tag %s in OBJ file", linetype.c_str());
    }

    if (iLine % 5000 == 0) {
      MESSAGE("Reading line %u (%u / %u kb)", unsigned(iLine),
              unsigned(iBytesRead/1024),unsigned(iFileLength/1024));
    }

  }
	fs.close();

  std::wstring desc = m_vConverterDesc + L" data converted from " + SysTools::GetFilename(strFilename);

  // generate color indies for "meshlab extended" format
  if (COLIndices.size() == 0 && vertices.size() == colors.size()) 
    COLIndices = VertIndices;


  std::shared_ptr<Mesh> m(
    new Mesh(vertices,normals,texcoords,colors,
             VertIndices,NormalIndices,TCIndices,COLIndices,
             false, false, desc, 
             ((iVerticesPerPoly == 2) 
                ? Mesh::MT_LINES 
                : Mesh::MT_TRIANGLES))
  );
  return m;
}


bool OBJGeoConverter::ConvertToNative(const Mesh& m,
                                      const std::wstring& strTargetFilename) {

  bool bUseExtension = SysTools::ToUpperCase(
                            SysTools::GetExt(strTargetFilename)
                                            ) == L"OBJX";

  std::ofstream outStream(SysTools::toNarrow(strTargetFilename).c_str());
  if (outStream.fail()) return false;

  std::stringstream statLine1, statLine2;
  statLine1 << "Vertices: " << m.GetVertices().size();
  statLine2 << "Primitives: " << m.GetVertexIndices().size()/
                                 m.GetVerticesPerPoly();
  size_t iCount = std::max(m.Name().size(), 
                           std::max(statLine1.str().size(),
                                    statLine2.str().size()
                           ));

  for (size_t i = 0;i<iCount+4;i++) outStream << "#";
  outStream << std::endl;
  outStream << "# " << SysTools::toNarrow(m.Name());
  for (size_t i = m.Name().size();i<iCount;i++) outStream << " ";
  outStream << " #" << std::endl;

  outStream << "# " << statLine1.str();
  for (size_t i =statLine1.str().size();i<iCount;i++) outStream << " ";
  outStream << " #" << std::endl;

  outStream << "# " << statLine2.str();
  for (size_t i = statLine2.str().size();i<iCount;i++) outStream << " ";
  outStream << " #" << std::endl;

  for (size_t i = 0;i<iCount+4;i++) outStream << "#";
  outStream << std::endl;

  // vertices
  for (size_t i = 0;i<m.GetVertices().size();i++) {
      outStream << "v " 
                  << m.GetVertices()[i].x << " "
                  << m.GetVertices()[i].y << " "
                  << m.GetVertices()[i].z << std::endl;;
  }

  for (size_t i = 0;i<m.GetNormals().size();i++) {
      outStream << "vn " 
                  << m.GetNormals()[i].x << " "
                  << m.GetNormals()[i].y << " "
                  << m.GetNormals()[i].z << std::endl;
  }

  for (size_t i = 0;i<m.GetTexCoords().size();i++) {
      outStream << "vt " 
                  << m.GetTexCoords()[i].x << " "
                  << m.GetTexCoords()[i].y << std::endl;
  }

  if (bUseExtension) {
    // this is our own extension, originally colors are 
    // not supported by OBJ files
    for (size_t i = 0;i<m.GetColors().size();i++) {
        outStream << "vc " 
                    << m.GetColors()[i].x << " "
                    << m.GetColors()[i].y << " "
                    << m.GetColors()[i].z << " "
                    << m.GetColors()[i].w << std::endl;
    }
  } else {
    if (!m.GetColors().empty()) 
      WARNING("Ignoring mesh colors for standart OBJ files, "
              "use OBJX files to also export colors.");
  }

  bool bHasTexCoords = m.GetTexCoordIndices().size() == m.GetVertexIndices().size();
  bool bHasNormals = m.GetNormalIndices().size() == m.GetVertexIndices().size();
  bool bHasColors = (bUseExtension && m.GetColorIndices().size() == m.GetVertexIndices().size());

  size_t iVPP = m.GetVerticesPerPoly();
  for (size_t i = 0;i<m.GetVertexIndices().size();i+=iVPP) {
      if (iVPP == 1)
         outStream << "p "; else
      if (iVPP == 2)
         outStream << "l ";
      else 
         outStream << "f ";

      for (size_t j = 0;j<iVPP;j++) {
        outStream << m.GetVertexIndices()[i+j]+1;
        if (bHasTexCoords || bHasNormals || bHasColors) {
            outStream << "/";
            if (m.GetTexCoordIndices().size() == m.GetVertexIndices().size()) {
              outStream << m.GetTexCoordIndices()[i+j]+1;
            }
        }
        if (bHasNormals || bHasColors) {
            outStream << "/";
            if (m.GetNormalIndices().size() == m.GetVertexIndices().size()) {
              outStream << m.GetNormalIndices()[i+j]+1;
            }
        }
        if (bHasColors) {
            outStream << "/";
            outStream << m.GetColorIndices()[i+j]+1;
        }
        if (j+1 < iVPP) outStream << " ";
      }
      outStream << std::endl;
  }

  outStream.close();

  return true;
}
