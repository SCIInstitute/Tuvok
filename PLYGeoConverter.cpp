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

//!    File   : PLYGeoConverter.cpp
//!    Author : Jens Krueger
//!             IVCI & DFKI & MMCI, Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : July 2010
//
//!    Copyright (C) 2010 DFKI, MMCI, SCI Institute

#include "PLYGeoConverter.h"
#include "Controller/Controller.h"
#include "SysTools.h"
#include "Mesh.h"
#include <fstream>
#include "TuvokIOError.h"

using namespace tuvok;
using namespace std;

PLYGeoConverter::PLYGeoConverter() :
  AbstrGeoConverter()
{
  m_vConverterDesc = "Stanford Polygon File Format";
  m_vSupportedExt.push_back("PLY");
}


Mesh* PLYGeoConverter::ConvertToMesh(const std::string& strFilename) {
  
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
    throw tuvok::io::DSOpenFailed(strFilename.c_str(), __FILE__, __LINE__);
  }

  int iFormat = FORMAT_ASCII;
  int iReaderState = SEARCHING_MAGIC;
  size_t iVertexCount=0;
  size_t iFaceCount=0;
  size_t iLineCount=0;
  bool bNormalsFound = false;
  bool bTexCoordsFound = false;
  bool bColorsFound = false;

  MESSAGE("Reading Header");

  fs.seekg(0,std::ios::end);
  streamoff iFileLength = fs.tellg();
  fs.seekg(0,std::ios::beg);
  size_t iBytesRead = 0;
  size_t iLine = 0;

	while (!fs.fail() && iReaderState < PARSING_VERTEX_DATA) {
		getline(fs, line);
		if (fs.fail()) break; // no more lines to read

    iBytesRead += line.size() + 1;
    iLine++;
    if (iLine % 5000 == 0) {
        MESSAGE("Reading Header (Line %u %u/%u kb)", unsigned(iLine),
                unsigned(iBytesRead/1024),unsigned(iFileLength/1024));
    }

    // remove comments
    line = SysTools::TrimStr(line);
    if (line.length() == 0) continue; // skip empty lines

    // find the linetype
    string linetype = GetToken(line);
    if (linetype == "comment") continue; // skip comment lines

    switch (iReaderState) {
      case SEARCHING_MAGIC : {
                           if (linetype == "ply")
                                iReaderState = PARSING_GENERAL_HEADER;
                              else 
                                continue;
                           }
                           break;
      case PARSING_FACE_HEADER : 
      case PARSING_VERTEX_HEADER :
      case PARSING_EDGE_HEADER :
      case PARSING_GENERAL_HEADER : {
                              if (linetype == "format") {
                                string format = GetToken(line);
                                if (format == "ascii")
                                  iFormat = FORMAT_ASCII;
                                else if (format == "binary_little_endian")
                                  iFormat = FORMAT_BIN_LITTLE;
                                else if (format == "binary_big_endian")
                                  iFormat = FORMAT_BIN_BIG;
                                else {
                                  stringstream s;
                                  s << "unknown format " << format.c_str();
                                  throw tuvok::io::DSParseFailed(strFilename.c_str(), s.str().c_str(),__FILE__, __LINE__);
                                }
                                string version = GetToken(line);
                                if (version != "1.0") {
                                  stringstream s;
                                  s << "unknown version " << version.c_str();
                                  throw tuvok::io::DSParseFailed(strFilename.c_str(), s.str().c_str(),__FILE__, __LINE__);
                                }
                              } else if (linetype == "element") {
                                string elemType = GetToken(line);
                                if (elemType == "vertex") {
                                  iReaderState = PARSING_VERTEX_HEADER;
                                  string strCount = GetToken(line);
                                  iVertexCount = atoi(strCount.c_str());
                                } else if (elemType == "face") {
                                  iReaderState = PARSING_FACE_HEADER;
                                  string strCount = GetToken(line);
                                  iFaceCount = atoi(strCount.c_str());
                                } else if (elemType == "edge") {
                                  iReaderState = PARSING_EDGE_HEADER;
                                  string strCount = GetToken(line);
                                  iLineCount = atoi(strCount.c_str());
                                }
                              } else if (linetype == "property") {
                                if (iReaderState == PARSING_VERTEX_HEADER){
                                  propType t = StringToType(GetToken(line));
                                  vertexProp p = StringToVProp(GetToken(line));
                                  vertexProps.push_back(make_pair(t,p));
                                } else if (iReaderState == PARSING_FACE_HEADER) {
                                  faceProp p = StringToFProp(GetToken(line));
                                  propType t1 = StringToType(GetToken(line));
                                  propType t2 = StringToType(GetToken(line));
                                  faceProps.push_back(tr1::make_tuple(t1,t2,p));
                                } else if (iReaderState == PARSING_EDGE_HEADER) {
                                  propType t = StringToType(GetToken(line));
                                  edgeProp p = StringToEProp(GetToken(line));
                                  edgeProps.push_back(make_pair(t,p));
                                } else {
                                  WARNING("property outside vertex or face data found");
                                }
                              } else if (linetype == "end_header") {
                                iReaderState = PARSING_VERTEX_DATA;
                              } 
                           }
                           break;
      default : throw tuvok::io::DSParseFailed(strFilename.c_str(), "unknown parser state header",__FILE__, __LINE__);
    }
  }

  if (iFormat != FORMAT_ASCII) {
    throw tuvok::io::DSParseFailed(strFilename.c_str(), "Binary PLY files not supported yet.",__FILE__, __LINE__);
  }

  if (iFaceCount > 0 && iLineCount > 0) {
    WARNING("found both, polygons and lines, in the file, ignoring lines");
  }

  MESSAGE("Reading Vertices");

  size_t iFacesFound = 0;
  vertices.reserve(iVertexCount);
  // parse data body of PLY file
	while (!fs.fail() && iReaderState != PARSING_DONE) {
		getline(fs, line);
		if (fs.fail()) break; // no more lines to read

    iBytesRead += line.size() + 1;
    iLine++;
    if (iLine % 5000 == 0) {
      if (PARSING_VERTEX_DATA) 
        MESSAGE("Reading Vertices (Line %u %u/%u kb)", unsigned(iLine),
                unsigned(iBytesRead/1024),unsigned(iFileLength/1024));
      else
        MESSAGE("Reading Indices (Line %u %u/%u kb)", unsigned(iLine),
                unsigned(iBytesRead/1024),unsigned(iFileLength/1024));
    }


    line = SysTools::TrimStr(line);

    if (iReaderState == PARSING_VERTEX_DATA) {
      FLOATVECTOR3 pos;
      FLOATVECTOR3 normal(0,0,0);
      FLOATVECTOR4 color(0,0,0,1);
      
      for (size_t i = 0;i<vertexProps.size();i++) {
        string strValue = GetToken(line);


        double fValue=0.0; int iValue=0;
        if (vertexProps[i].first <= PROPT_DOUBLE) {
          fValue = atof(strValue.c_str());
          iValue = int(fValue);
        } else {
          iValue = atoi(strValue.c_str());
          fValue = double(iValue);
        }

        switch (vertexProps[i].second) {
          case VPROP_X         : pos.x = float(fValue); break;
          case VPROP_Y         : pos.y = float(fValue); break;
          case VPROP_Z         : pos.z = float(fValue); break;
          case VPROP_NX        : bNormalsFound = true; normal.x = float(fValue); break;
          case VPROP_NY        : bNormalsFound = true; normal.y = float(fValue); break;
          case VPROP_NZ        : bNormalsFound = true; normal.z = float(fValue); break;
          case VPROP_RED       : bColorsFound = true; color.x = (vertexProps[i].first <= PROPT_DOUBLE) ? float(fValue) : iValue/255.0f; break;
          case VPROP_GREEN     : bColorsFound = true; color.y = (vertexProps[i].first <= PROPT_DOUBLE) ? float(fValue) : iValue/255.0f; break;
          case VPROP_BLUE      : bColorsFound = true; color.z = (vertexProps[i].first <= PROPT_DOUBLE) ? float(fValue) : iValue/255.0f; break;
          case VPROP_OPACITY   : bColorsFound = true; color.w = (vertexProps[i].first <= PROPT_DOUBLE) ? float(fValue) : iValue/255.0f; break;
          case VPROP_INTENSITY : bColorsFound = true;                                 
                                 color = (vertexProps[i].first <= PROPT_DOUBLE) ? FLOATVECTOR4(float(fValue),float(fValue),float(fValue),1.0f)
                                                                                : FLOATVECTOR4(iValue/255.0f,iValue/255.0f,iValue/255.0f,1.0f);
                                 break;
          default: break;
        }
      }

      vertices.push_back(pos);
      if (bColorsFound) colors.push_back(color);
      if (bNormalsFound) normals.push_back(normal);

      if (vertices.size() == iVertexCount) {
        iReaderState = (iFaceCount > 0) ? PARSING_FACE_DATA : PARSING_EDGE_DATA;
        MESSAGE("Reading Faces");
      }
    } else if (iReaderState == PARSING_FACE_DATA) {

      IndexVec v, n, t, c;
      for (size_t i = 0;i<faceProps.size();i++) {
        string strValue = GetToken(line);

        double fValue=0.0; int iValue=0;
        if (tr1::get<1>(faceProps[i]) <= PROPT_DOUBLE) 
          fValue = atof(strValue.c_str());
        else
          iValue = atoi(strValue.c_str());

        switch (tr1::get<2>(faceProps[i])) {
          case FPROP_LIST : {
                              for (int j = 0;j<iValue;j++) {
                                // hack: read everything as int, regardless of the 
                                //       type stored in tr1::get<1>(faceProps[i])
                                strValue = GetToken(line);
                                int elem = atoi(strValue.c_str());
                                v.push_back(elem);
                                if (bNormalsFound) n.push_back(elem);
                                if (bTexCoordsFound) t.push_back(elem);
                                if (bColorsFound) c.push_back(elem);
                              }
                            }
                            break;
          default: break;
        }
      }
      AddToMesh(vertices,v,n,t,c,VertIndices,NormalIndices,TCIndices,COLIndices);

      iFacesFound++;
      if (iFacesFound == iFaceCount) iReaderState = PARSING_DONE;
    } else if (iReaderState == PARSING_EDGE_DATA) {
      FLOATVECTOR4 color(0,0,0,1);
      bool bEdgeColorsFound=false;

      for (size_t i = 0;i<edgeProps.size();i++) {
        string strValue = GetToken(line);

        double fValue=0.0; int iValue=0;
        if (edgeProps[i].first <= PROPT_DOUBLE) {
          fValue = atof(strValue.c_str());
          iValue = int(fValue);
        } else {
          iValue = atoi(strValue.c_str());
          fValue = double(iValue);
        }
        switch (edgeProps[i].second) {
          case EPROP_VERTEX1   : VertIndices.push_back(iValue); break;
          case EPROP_VERTEX2   : VertIndices.push_back(iValue); break;
          case EPROP_RED       : bEdgeColorsFound = true; color.x = (edgeProps[i].first <= PROPT_DOUBLE) ? float(fValue) : iValue/255.0f; break;
          case EPROP_GREEN     : bEdgeColorsFound = true; color.y = (edgeProps[i].first <= PROPT_DOUBLE) ? float(fValue) : iValue/255.0f; break;
          case EPROP_BLUE      : bEdgeColorsFound = true; color.z = (edgeProps[i].first <= PROPT_DOUBLE) ? float(fValue) : iValue/255.0f; break;
          case EPROP_OPACITY   : bEdgeColorsFound = true; color.w = (edgeProps[i].first <= PROPT_DOUBLE) ? float(fValue) : iValue/255.0f; break;
          case EPROP_INTENSITY : bEdgeColorsFound = true;                                 
                                 color = (edgeProps[i].first <= PROPT_DOUBLE) ? FLOATVECTOR4(float(fValue),float(fValue),float(fValue),1.0f)
                                                                              : FLOATVECTOR4(iValue/255.0f,iValue/255.0f,iValue/255.0f,1.0f);
                                 break;
          default: break;
        }
      }

      if (bEdgeColorsFound)  {
        COLIndices.push_back(UINT32(colors.size()));
        COLIndices.push_back(UINT32(colors.size()));
        colors.push_back(color);
      }

      if (VertIndices.size() == iLineCount*2) iReaderState = PARSING_DONE;
    } else throw tuvok::io::DSParseFailed(strFilename.c_str(), "unknown parser state data",__FILE__, __LINE__);
  }

  MESSAGE("Creating Mesh Object");

  std::string desc = m_vConverterDesc + " data converted from " + SysTools::GetFilename(strFilename);

  Mesh* m = new Mesh(vertices,normals,texcoords,colors,
                     VertIndices,NormalIndices,TCIndices,COLIndices,
                     false,false,desc, (iFaceCount > 0) ? Mesh::MT_TRIANGLES:Mesh::MT_LINES);
  return m;
}


PLYGeoConverter::propType PLYGeoConverter::StringToType(const std::string& token) {
  if (token == "float" || token == "float32") return PROPT_FLOAT;
  if (token == "double" || token == "float64") return PROPT_DOUBLE;
  if (token == "char" || token == "int8") return PROPT_INT8;
  if (token == "uchar" || token == "uint8") return PROPT_UINT8;
  if (token == "short" || token == "int16") return PROPT_INT16;
  if (token == "ushort" || token == "uint16") return PROPT_UINT16;
  if (token == "int" || token == "int32") return PROPT_INT32;
  if (token == "uint" || token == "uint32") return PROPT_UINT32;
  return PROPT_UNKNOWN;
}

PLYGeoConverter::vertexProp PLYGeoConverter::StringToVProp(const std::string& token) {
  if (token == "x") return VPROP_X;
  if (token == "y") return VPROP_Y;
  if (token == "z") return VPROP_Z;
  if (token == "nx") return VPROP_NX;
  if (token == "ny") return VPROP_NY;
  if (token == "nz") return VPROP_NZ;
  if (token == "red") return VPROP_RED;
  if (token == "green") return VPROP_GREEN;
  if (token == "blue") return VPROP_BLUE;
  if (token == "opacity") return VPROP_OPACITY;  
  if (token == "intensity") return VPROP_INTENSITY;
  if (token == "confidence") return VPROP_CONFIDENCE;
  return VPROP_UNKNOWN;
}

PLYGeoConverter::faceProp PLYGeoConverter::StringToFProp(const std::string& token) {
  if (token == "list") return FPROP_LIST;
  return FPROP_UNKNOWN;
}

PLYGeoConverter::edgeProp PLYGeoConverter::StringToEProp(const std::string& token) {
  if (token == "vertex1") return EPROP_VERTEX1;
  if (token == "vertex2") return EPROP_VERTEX2;
  if (token == "red") return EPROP_RED;
  if (token == "green") return EPROP_GREEN;
  if (token == "blue") return EPROP_BLUE;
  if (token == "opacity") return EPROP_OPACITY;  
  if (token == "intensity") return EPROP_INTENSITY;
  return EPROP_UNKNOWN;
}


bool PLYGeoConverter::ConvertToNative(const Mesh& m,
                                      const std::string& strTargetFilename) {

  std::ofstream outStream(strTargetFilename.c_str());
  if (outStream.fail()) return false;

  // write magic
  outStream << "ply" << std::endl;

  // format
  outStream << "format ascii 1.0" << std::endl;

  // some comments
  outStream << "comment " << m.Name() << std::endl;
  outStream << "comment Vertices: " << m.GetVertices().size() << std::endl;
  outStream << "comment Primitives: " << m.GetVertexIndices().size()/
                                         m.GetVerticesPerPoly() << std::endl;
                  
  // vertex info
  outStream << "element vertex " << m.GetVertices().size() << std::endl;
  outStream << "property float x" << std::endl;
  outStream << "property float y" << std::endl;
  outStream << "property float z" << std::endl;

  if (m.GetVertices().size() == m.GetNormals().size()) {
    outStream << "property float nx" << std::endl;
    outStream << "property float ny" << std::endl;
    outStream << "property float nz" << std::endl;
  }

  // face info
  if (m.GetMeshType() == Mesh::MT_TRIANGLES) {
    outStream << "element face " << m.GetVertexIndices().size()/
                                     m.GetVerticesPerPoly() << std::endl;
    outStream << "property list uchar int vertex_indices" << std::endl;
  } else {
    outStream << "element edge " << m.GetVertexIndices().size()/
                                     m.GetVerticesPerPoly() << std::endl;
    outStream << "property int vertex1" << std::endl;
    outStream << "property int vertex2" << std::endl;

    if (m.GetVertexIndices().size() == m.GetColorIndices().size()) {
      outStream << "property float red" << std::endl;
      outStream << "property float green" << std::endl;
      outStream << "property float blue" << std::endl;
      outStream << "property float opacity" << std::endl;
    }

  }

  // end header
  outStream << "end_header" << std::endl;

  // vertex data
  const VertVec& v = m.GetVertices();
  const NormVec& n = m.GetNormals();
  if (v.size() == n.size()) {
    for (size_t i = 0;i<v.size();i++){
      outStream << v[i].x << " " << v[i].y << " " << v[i].z << " " << n[i].x << " " << n[i].y << " " << n[i].z << " " <<  std::endl;
    }
  } else {
    for (size_t i = 0;i<v.size();i++){
      outStream << v[i].x << " " << v[i].y << " " << v[i].z << " " << std::endl;    
    }
  }

  // face data
  if (m.GetMeshType() == Mesh::MT_TRIANGLES) {
    const IndexVec& ind = m.GetVertexIndices();
    for (size_t i = 0;i<ind.size();i+=m.GetVerticesPerPoly()) {
      outStream << m.GetVerticesPerPoly();
      for (size_t j = 0;j<m.GetVerticesPerPoly();j++){
        outStream  << " " << ind[i+j];
      }
      outStream << std::endl;    
    }
  } else {
    const IndexVec& ind = m.GetVertexIndices();
    const IndexVec& col = m.GetColorIndices();
    for (size_t i = 0;i<ind.size();i+=m.GetVerticesPerPoly()) {
      outStream  << ind[i+0] << " " << ind[i+1];
      if (ind.size() == col.size()) {
        for (size_t j = 0;j<4;j++){
          outStream  << " " << m.GetColors()[col[i]][j];
        }
      }
      outStream << std::endl;
    }
  }

  return true;
}
