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
  m_vConverterDesc = "Wavefront Object File";
  m_vSupportedExt.push_back("OBJ");
  m_vSupportedExt.push_back("OBJX");
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
  if (off == std::string::npos) return std::string();
  if (bOnlyFirst) {
    return Src.substr(off+1);
  } else {
    size_t p1 = Src.find_first_not_of(c,off);
    if (p1 == std::string::npos) return std::string();
    return Src.substr(p1);
  }
}

void OBJGeoConverter::AddToMesh(const VertVec& vertices,
                                IndexVec& v, IndexVec& n,
                                IndexVec& t, IndexVec& c,
                                IndexVec& VertIndices, IndexVec& NormalIndices,
                                IndexVec& TCIndices, IndexVec& COLIndices) {
  if (v.size() > 3) {
    // per OBJ definition any poly with more than 3 verices has
    // to be planar and convex, so we can savely triangulate it

    SortByGradient(vertices,v,n,t,c);

    for (size_t i = 0;i<v.size()-2;i++) {
      IndexVec mv, mn, mt, mc;
      mv.push_back(v[0]);mv.push_back(v[i+1]);mv.push_back(v[i+2]);
      if (n.size() == v.size()) {mn.push_back(n[i]);mn.push_back(n[i+1]);mn.push_back(n[i+2]);}
      if (t.size() == v.size()) {mt.push_back(t[i]);mt.push_back(t[i+1]);mt.push_back(t[i+2]);}
      if (c.size() == v.size()) {mc.push_back(c[i]);mc.push_back(c[i+1]);mc.push_back(c[i+2]);}

      AddToMesh(vertices,
                mv,mn,mt,mc,
                VertIndices,
                NormalIndices,
                TCIndices,
                COLIndices);
    }

  } else {
    for (size_t i = 0;i<v.size();i++) {
      VertIndices.push_back(v[i]);
      if (n.size() == v.size()) NormalIndices.push_back(n[i]);
      if (t.size() == v.size()) TCIndices.push_back(t[i]);
      if (c.size() == v.size()) COLIndices.push_back(c[i]);
    }
  }
}

Mesh* OBJGeoConverter::ConvertToMesh(const std::string& strFilename) {

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

	fs.open(strFilename.c_str());
  if (fs.fail()) {
    // hack, we really want some kind of 'file not found' exception.
    throw tuvok::io::DSOpenFailed(strFilename.c_str(), __FILE__, __LINE__);
  }

  float x,y,z;
  size_t iVerticesPerPoly = 0;

  while (!fs.fail()) {
		getline(fs, line);
		if (fs.fail()) break; // no more lines to read
    line = SysTools::ToLowerCase(SysTools::TrimStr(line));

    // remove comments
    size_t cPos = line.find_first_of('#');
    if (cPos != std::string::npos) line = line.substr(0,cPos);
    line = SysTools::TrimStr(line);
    if (line.length() == 0) continue; // skips empty and comment lines

    // find the linetpe
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
    if (linetype == "f" || linetype == "l") { // face or line found
      size_t off = line.find_first_of(" \r\n\t");
      if (off == std::string::npos) continue;
      std::string analysis = SysTools::TrimStrRight(line.substr(0,off));
      int count = CountOccurences(analysis,"/");

      IndexVec v, n, t, c;
      
      while (line.length() > 0)  {
        switch (count) {
          case 0 : {
                int vI = atoi(line.c_str())-1;
                v.push_back(vI);
                line = TrimToken(line);
                break;
               }
          case 1 : {
                int vI = atoi(line.c_str())-1;
                v.push_back(vI);
                line = TrimToken(line,"/",true);
                int vT = atoi(line.c_str())-1;
                t.push_back(vT);
                line = TrimToken(line);
                break;
               }
          case 2 : {
                int vI = atoi(line.c_str())-1;
                v.push_back(vI);
                line = TrimToken(line,"/",true);
                if (line[0] != '/') {
                  int vT = atoi(line.c_str())-1;
                  t.push_back(vT);
                }
                line = TrimToken(line,"/",true);
                int vN = atoi(line.c_str())-1;
                n.push_back(vN);
                line = TrimToken(line);
                break;
               }
          case 3 : {
                int vI = atoi(line.c_str())-1;
                v.push_back(vI);
                line = TrimToken(line,"/",true);
                if (line[0] != '/') {
                  int vT = atoi(line.c_str())-1;
                  t.push_back(vT);
                }
                line = TrimToken(line,"/",true);
                if (line[0] != '/') {
                  int vN = atoi(line.c_str())-1;
                  n.push_back(vN);
                }
                line = TrimToken(line,"/",true);
                int vC = atoi(line.c_str())-1;
                c.push_back(vC);
                line = TrimToken(line);
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
  }
	fs.close();

  std::string desc = m_vConverterDesc + " data converted from " + SysTools::GetFilename(strFilename);

  Mesh* m = new Mesh(vertices,normals,texcoords,colors,
                     VertIndices,NormalIndices,TCIndices,COLIndices,
                     false, false, desc, 
                     ((iVerticesPerPoly == 2) 
                            ? Mesh::MT_LINES 
                            : Mesh::MT_TRIANGLES ));
  return m;
}


bool OBJGeoConverter::ConvertToNative(const Mesh& m,
                                      const std::string& strTargetFilename) {

  bool bUseExtension = SysTools::ToUpperCase(
                            SysTools::GetExt(strTargetFilename)
                                            ) == "OBJX";

  std::ofstream outStream(strTargetFilename.c_str());
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
  outStream << "# " << m.Name();
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
    if (m.GetColors().size() != 0) 
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
