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

//!    File   : MedAlyVisFiberTractGeoConverter.cpp
//!    Author : Jens Krueger
//!             IVCI & DFKI & MMCI, Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : July 2010
//
//!    Copyright (C) 2010 DFKI, MMCI, SCI Institute

#include "MedAlyVisFiberTractGeoConverter.h"
#include "Controller/Controller.h"
#include "SysTools.h"
#include "Mesh.h"
#include <fstream>
#include "TuvokIOError.h"

using namespace tuvok;
using namespace std;

MedAlyVisFiberTractGeoConverter::MedAlyVisFiberTractGeoConverter()
{
  m_vConverterDesc = "MedAlyVis Fiber Tract File";
  m_vSupportedExt.push_back("TRK");
}


Mesh* MedAlyVisFiberTractGeoConverter::ConvertToMesh(const std::string& strFilename) {
  
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
    throw tuvok::io::DSOpenFailed(strFilename.c_str(), __FILE__, __LINE__);
  }

  int iReaderState = SEARCHING_DIM;
  UINTVECTOR3 iDim;
  FLOATVECTOR3 fScale;
  FLOATVECTOR3 fTranslation;
  INTVECTOR4 iMetadata;
  int iElementCounter=0;
  int iLineCounter=-1;
  std::vector<FLOATVECTOR3> linePosVec;

	while (!fs.fail() || iLineCounter == iMetadata[2]) {
		getline(fs, line);
		if (fs.fail()) break; // no more lines to read

    // remove comments
    size_t cPos = line.find_first_of('#');
    if (cPos != std::string::npos) line = line.substr(0,cPos);
    line = SysTools::TrimStr(line);

    if (line.length() == 0) continue; // skips empty and comment lines

    switch (iReaderState) {
      case SEARCHING_DIM : {
                              iDim[0] = atoi(line.c_str());
                              line = TrimToken(line);
                              iDim[1] = atoi(line.c_str());
                              line = TrimToken(line);
                              iDim[2] = atoi(line.c_str());
                              iReaderState++;
                           }
                           break;
      case SEARCHING_SCALE : {
                              fScale[0] = float(atof(line.c_str()));
                              line = TrimToken(line);
                              fScale[1] = float(atof(line.c_str()));
                              line = TrimToken(line);
                              fScale[2] = float(atof(line.c_str()));
                              iReaderState++;
                           }
                           break;
      case SEARCHING_TRANSLATION : {
                              fTranslation[0] = float(atof(line.c_str()));
                              line = TrimToken(line);
                              fTranslation[1] = float(atof(line.c_str()));
                              line = TrimToken(line);
                              fTranslation[2] = float(atof(line.c_str()));
                              iReaderState++;
                           }
                           break;
      case SEARCHING_METADATA : {
                              iMetadata[0] = atoi(line.c_str());
                              line = TrimToken(line);
                              iMetadata[1] = atoi(line.c_str());
                              line = TrimToken(line);
                              iMetadata[2] = atoi(line.c_str());
                              line = TrimToken(line);
                              iMetadata[3] = atoi(line.c_str());
                              iLineCounter = 0;
                              iReaderState++;
                           }
                           break;
      case PARSING_COUNTER : {
                              iElementCounter = atoi(line.c_str());
                              linePosVec.resize(iElementCounter);
                              iReaderState++;
                           }
                           break;
      case PARSING_DATA : {
                              size_t s = linePosVec.size();
                              linePosVec[s-iElementCounter][0] = float(atof(line.c_str()));
                              line = TrimToken(line);
                              linePosVec[s-iElementCounter][1] = float(atof(line.c_str()));
                              line = TrimToken(line);
                              linePosVec[s-iElementCounter][2] = float(atof(line.c_str()));

                              iElementCounter--;
                              if (iElementCounter == 0) {
                                // line to tri-mesh

                                if (linePosVec.size() > 1) { 
                                  VertIndices.push_back(UINT32(vertices.size()));
                                  VertIndices.push_back(UINT32(vertices.size()+1));

                                  vertices.push_back(linePosVec[0]);
                                  vertices.push_back(linePosVec[1]);

                                  for (size_t i = 1;i<linePosVec.size();i++) {
                                    VertIndices.push_back(UINT32(vertices.size()-1));
                                    VertIndices.push_back(UINT32(vertices.size()));
                                    vertices.push_back(linePosVec[i]);
                                  }
                                }

                                iLineCounter++;
                                iReaderState = PARSING_COUNTER;
                              }
                           }
                           break;
      default : throw std::runtime_error("unknown parser state");
    }

  }


  std::string desc = m_vConverterDesc + " data converted from " + SysTools::GetFilename(strFilename);

  Mesh* m = new Mesh(vertices,NormVec(),TexCoordVec(),ColorVec(),
                     VertIndices,IndexVec(),IndexVec(),IndexVec(),
                     false,false,desc,Mesh::MT_LINES);
  return m;
}


std::string MedAlyVisFiberTractGeoConverter::TrimToken(const std::string& Src,
                                              const std::string& c)
{
  size_t off = Src.find_first_of(c);
  if (off == std::string::npos) off = 0;
  size_t p1 = Src.find_first_not_of(c,off);
  if (p1 == std::string::npos) return std::string();
  return Src.substr(p1);
}
