#include "StLGeoConverter.h"
#include "Controller/Controller.h"
#include "Basics/LargeRAWFile.h"
#include "SysTools.h"
#include "Mesh.h"
#include <fstream>
#include "TuvokIOError.h"

using namespace tuvok;

StLGeoConverter::StLGeoConverter() :
  AbstrGeoConverter()
{
  m_vConverterDesc = "Stereo Lithography Format";
  m_vSupportedExt.push_back("STL");
}

FLOATVECTOR3 StLGeoConverter::ComputeFaceNormal(const Mesh& m, 
                                                size_t polyIndex, 
                                                bool bHasNormals) const {
  FLOATVECTOR3 faceNormal;
  if (bHasNormals) {
    // compute normal as average over the vertices
    for (size_t j = 0;j<m.GetVerticesPerPoly();j++) {
      size_t index = m.GetNormalIndices()[polyIndex+j];
      faceNormal += m.GetNormals()[index];
    }
  } else {
    // compute face normal as cross product 
    // of the first 3 vertices of the polygon

    size_t index0 = m.GetVertexIndices()[polyIndex+0];
    size_t index1 = m.GetVertexIndices()[polyIndex+1];
    size_t index2 = m.GetVertexIndices()[polyIndex+2];

    FLOATVECTOR3 vec1 = m.GetVertices()[index0]-m.GetVertices()[index1];
    FLOATVECTOR3 vec2 = m.GetVertices()[index0]-m.GetVertices()[index2];
    faceNormal = vec1 % vec2;
  }
  return faceNormal.normalized();
}

bool StLGeoConverter::ConvertToNative(const Mesh& m,
                                      const std::string& strTargetFilename) {
  // print in binary by default
  return ConvertToNative(m, strTargetFilename, false);
}


bool StLGeoConverter::ConvertToNative(const Mesh& m,
                                      const std::string& strTargetFilename,
                                      bool bASCII) {

  size_t iVPP = m.GetVerticesPerPoly();
  bool bHasNormals = m.GetNormalIndices().size()==m.GetVertexIndices().size();

  if (iVPP < 3) {
    T_ERROR("StL requires at least 2 vertices per object");
    return false;
  }

  if (bASCII) {
    std::ofstream outStream(strTargetFilename.c_str());
    if (outStream.fail()) return false;

    outStream << "solid isosurface" << "\n";

    for (size_t i = 0;i<m.GetVertexIndices().size();i+=iVPP) {
      FLOATVECTOR3 faceNormal = ComputeFaceNormal(m, i, bHasNormals);

      outStream << "  facet normal " << faceNormal.x << " " 
                                     << faceNormal.y << " " 
                                     << faceNormal.z << "\n";
      outStream << "    outer loop" << "\n";


      for (size_t j = 0;j<iVPP;j++) {
        size_t index = m.GetVertexIndices()[i+j];
        outStream << "      vertex " 
                    << m.GetVertices()[index].x << " "
                    << m.GetVertices()[index].y << " "
                    << m.GetVertices()[index].z << "\n";;
      }

      outStream << "    endloop" << "\n";
      outStream << "  endfacet" << "\n";
    }

    outStream << "endsolid isosurface" << "\n";

    outStream.close();
    return true;
  } else {
    LargeRAWFile outFile(strTargetFilename);
    outFile.Create();

    if (!outFile.IsOpen()) {
      return false;
    }

    // write 80 byte header 
    // this can be anthing but may not start with the word "solid"
    uint8_t header[80];
    const std::string text = "StL-Isosurface created by ImageVis3D";
    for (size_t i = 0; i<text.size() && i < 80;++i) {
      header[i] = text[i];
    }
    outFile.WriteRAW(header, 80);

    // write polygon count
    uint32_t iPolyCount = uint32_t(m.GetVertexIndices().size()/iVPP);
    outFile.WriteData(iPolyCount, false);

    // write data
    for (size_t i = 0;i<m.GetVertexIndices().size();i+=iVPP) {
      FLOATVECTOR3 faceNormal = ComputeFaceNormal(m, i, bHasNormals);
      outFile.WriteData(faceNormal.x, false);
      outFile.WriteData(faceNormal.y, false);
      outFile.WriteData(faceNormal.z, false);

      for (size_t j = 0;j<iVPP;j++) {
        size_t index = m.GetVertexIndices()[i+j];
        outFile.WriteData(m.GetVertices()[index].x, false);
        outFile.WriteData(m.GetVertices()[index].y, false);
        outFile.WriteData(m.GetVertices()[index].z, false);
      }

      uint16_t attributeCount = 0;
      outFile.WriteData(attributeCount, false);
    }

    outFile.Close();
    return true;
  }
}

std::shared_ptr<Mesh>
StLGeoConverter::ConvertToMesh(const std::string& strFilename) {
  VertVec       vertices;
  NormVec       normals;
  TexCoordVec   texcoords;
  ColorVec      colors;

  IndexVec      VertIndices;
  IndexVec      NormalIndices;
  IndexVec      TCIndices;
  IndexVec      COLIndices;

  // first figure out if this a binary or an ASCII file
  LargeRAWFile inFile(strFilename);
  inFile.Open();

  if (!inFile.IsOpen()) {
    // hack, we really want some kind of 'file not found' exception.
    throw tuvok::io::DSOpenFailed(strFilename.c_str(), __FILE__, __LINE__);
  }

  uint8_t header[80];
  inFile.ReadRAW(header, 80);

  // skip whitespaces
  size_t iStartPos = 0;
  for (;iStartPos<80;++iStartPos) {
    if (header[iStartPos] != ' ' && header[iStartPos] != '/t') 
      break;
  }

  // now search for the keyword "solid"
  if (iStartPos > 75 || 
      toupper(header[iStartPos+0]) != 'S' ||
      toupper(header[iStartPos+1]) != 'O' ||
      toupper(header[iStartPos+2]) != 'L' ||
      toupper(header[iStartPos+3]) != 'I' ||
      toupper(header[iStartPos+4]) != 'D') {
    
    // file must be binary, treat is as such
    uint32_t iNumFaces;
    inFile.ReadData(iNumFaces, false);

    for (uint32_t f = 0;f<iNumFaces;++f) {
      FLOATVECTOR3 normal;
      inFile.ReadData(normal.x,false);
      inFile.ReadData(normal.y,false);
      inFile.ReadData(normal.z,false);

      normals.push_back(normal);

      for (size_t i = 0;i<3;i++) {
        FLOATVECTOR3 pos;
        inFile.ReadData(pos.x,false);
        inFile.ReadData(pos.y,false);
        inFile.ReadData(pos.z,false);

        vertices.push_back(pos);
        VertIndices.push_back(uint32_t(vertices.size()-1));
        NormalIndices.push_back(uint32_t(normals.size()-1));
      }

      uint16_t iAttribCount;
      inFile.ReadData(iAttribCount,false);

      // iAttribCount should alwyas be 0
      if (iAttribCount != 0) break;
    }

    inFile.Close();
  } else {
    // file must be ASCII, treat is as such
    inFile.Close();

    std::ifstream fs;
    std::string line;
    fs.open(strFilename.c_str());
    if (fs.fail()) {
      // hack, we really want some kind of 'file not found' exception.
      throw tuvok::io::DSOpenFailed(strFilename.c_str(), __FILE__, __LINE__);
    }

    getline(fs, line); // read header again

    while (!fs.fail()) {
      getline(fs, line); // should be "facet normal n1 n2 n3"
      line = SysTools::ToLowerCase(SysTools::TrimStr(line));
      if (line.substr(0, 12) != "facet normal") break;

      std::vector< std::string > norm = SysTools::Tokenize(line, SysTools::PM_NONE);

      FLOATVECTOR3 normal(SysTools::FromString<float>(norm[2]),
                          SysTools::FromString<float>(norm[3]),
                          SysTools::FromString<float>(norm[4]));

      normals.push_back(normal);

      getline(fs, line); // should be "outer loop"
      line = SysTools::ToLowerCase(SysTools::TrimStr(line));
      if (line != "outer loop") break;

      for (size_t i = 0;i<3;i++) {
        getline(fs, line); // should be "vertex x y z"
        line = SysTools::ToLowerCase(SysTools::TrimStr(line));
        std::vector< std::string > v = SysTools::Tokenize(line, SysTools::PM_NONE);

        if (v[0] != "vertex") break;

        FLOATVECTOR3 pos(SysTools::FromString<float>(v[1]),
                         SysTools::FromString<float>(v[2]),
                         SysTools::FromString<float>(v[3]));

        vertices.push_back(pos);
        VertIndices.push_back(uint32_t(vertices.size()-1));
        NormalIndices.push_back(uint32_t(normals.size()-1));
      }

      getline(fs, line); // should be "endloop"
      line = SysTools::ToLowerCase(SysTools::TrimStr(line));
      if (line != "endloop") break;

      getline(fs, line); // should be "endfacet"
      line = SysTools::ToLowerCase(SysTools::TrimStr(line));
      if (line != "endfacet") break;
    }

    fs.close();
  }

  std::string desc = m_vConverterDesc + std::string(" data converted from ") 
                     + SysTools::GetFilename(strFilename);

  std::shared_ptr<Mesh> m(
    new Mesh(vertices,normals,texcoords,colors,
             VertIndices,NormalIndices,TCIndices,COLIndices,
             false, false, desc, Mesh::MT_TRIANGLES)
  );
  return m;
}

/*
   The MIT License

   Copyright (c) 2013 Jens Krueger

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

/*
std::shared_ptr<Mesh>
StLGeoConverter::ConvertToMesh(const std::string& strFilename) {
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
          float w = SysTools::FromString<float>(pos[3]);
          if (w != 0) {
            x /= w;
            y /= w;
            z /= w;
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
      size_t off = line.find_first_of(" \r\n\t");
      if (off == std::string::npos) continue;
      std::string analysis = SysTools::TrimStrRight(line.substr(0,off));
      int count = 0;//CountOccurences(analysis,"/");

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

  std::string desc = m_vConverterDesc + " data converted from " + SysTools::GetFilename(strFilename);

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

*/