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

MedAlyVisFiberTractGeoConverter::MedAlyVisFiberTractGeoConverter() :
  AbstrGeoConverter()
{
  m_vConverterDesc = "MedAlyVis Fiber Tract File";
  m_vSupportedExt.push_back("TRK");
}


Mesh* MedAlyVisFiberTractGeoConverter::ConvertToMesh(const std::string& strFilename) {
  
  VertVec       vertices;
  ColorVec      colors;

  IndexVec      VertIndices;
  IndexVec      COLIndices;

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
  size_t iElementCounter=0;
  size_t iElementReadCounter=0;
  int iLineCounter=-1;

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
                              iElementReadCounter = 0;
                              iReaderState++;
                           }
                           break;
      case PARSING_DATA : {
                              FLOATVECTOR3 vec;
                              vec[0] = float(atof(line.c_str()));
                              line = TrimToken(line);
                              vec[1] = float(atof(line.c_str()));
                              line = TrimToken(line);
                              vec[2] = float(atof(line.c_str()));

                              vec = (vec + 0.5f*FLOATVECTOR3(iDim)*fScale) / (FLOATVECTOR3(iDim)*fScale) - 0.5f;
                              
                              vertices.push_back(vec);

                              iElementReadCounter++;
                              if (iElementCounter == iElementReadCounter) {
                                // line to tri-mesh
                                size_t iStartIndex = vertices.size() - iElementCounter; 

                                for (size_t i = 0;i<iElementCounter-1;i++) {
                                  VertIndices.push_back(UINT32(iStartIndex));
                                  VertIndices.push_back(UINT32(iStartIndex+1));
                                  COLIndices.push_back(UINT32(iStartIndex));
                                  COLIndices.push_back(UINT32(iStartIndex+1));

                                  if (i == 0) {
                                    FLOATVECTOR3 direction = (vertices[iStartIndex+1]-vertices[iStartIndex]).normalized();
                                    colors.push_back((direction).abs());
                                  } else if (i == iElementCounter-2) {
                                     FLOATVECTOR3 directionB = (vertices[iStartIndex]-vertices[iStartIndex-1]).normalized();
                                     FLOATVECTOR3 directionF = (vertices[iStartIndex+1]-vertices[iStartIndex]).normalized();
                                     colors.push_back(((directionB+directionF)/2.0f).abs());
                                     colors.push_back((directionF).abs());
                                  } else {
                                     FLOATVECTOR3 directionB = (vertices[iStartIndex]-vertices[iStartIndex-1]).normalized();
                                     FLOATVECTOR3 directionF = (vertices[iStartIndex+1]-vertices[iStartIndex]).normalized();
                                     colors.push_back(((directionB+directionF)/2.0f).abs());
                                  }
                                  iStartIndex++;
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

  Mesh* m = new Mesh(vertices,NormVec(),TexCoordVec(),colors,
                     VertIndices,IndexVec(),IndexVec(),COLIndices,
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
