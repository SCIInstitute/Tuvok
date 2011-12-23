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

//!    File   : GeometryDataBlock.h
//!    Author : Jens Krueger
//!             IVCI & DFKI & MMCI, Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : July 2010
//
//!    Copyright (C) 2010 DFKI, MMCI, SCI Institute

#pragma once

#ifndef GEOMETRYDATABLOCK_H
#define GEOMETRYDATABLOCK_H

#include <string>
#include <vector>
#include "DataBlock.h"

class AbstrDebugOut;

class GeometryDataBlock : public DataBlock {
public:
  GeometryDataBlock();
  GeometryDataBlock(const GeometryDataBlock &other);
  GeometryDataBlock(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian);
  virtual GeometryDataBlock& operator=(const GeometryDataBlock& other);
  virtual ~GeometryDataBlock();

  virtual bool Verify(uint64_t iSizeofData, std::string* pstrProblem = NULL) const;
  virtual uint64_t ComputeDataSize() const;

  std::vector< float > GetVertices() const;
  std::vector< float > GetNormals() const;
  std::vector< float > GetTexCoords() const;
  std::vector< float > GetColors() const;

  std::vector< uint32_t > GetVertexIndices() const;
  std::vector< uint32_t > GetNormalIndices() const;
  std::vector< uint32_t > GetTexCoordIndices() const;
  std::vector< uint32_t > GetColorIndices() const;

  void SetVertices(const std::vector< float >& v);
  void SetNormals(const std::vector< float >& n);
  void SetTexCoords(const std::vector< float >& tc);
  void SetColors(const std::vector< float >& c);

  void SetVertexIndices(const std::vector< uint32_t >& vI);
  void SetNormalIndices(const std::vector< uint32_t >& nI);
  void SetTexCoordIndices(const std::vector< uint32_t >& tcI);
  void SetColorIndices(const std::vector< uint32_t >& cI);

  const std::vector< float >& GetDefaultColor() const {return m_DefaultColor;}
  void SetDefaultColor(const std::vector< float >& color) {m_DefaultColor = color;}

  std::string m_Desc;
  uint64_t GetPolySize() const {return m_PolySize;}
  void SetPolySize(uint64_t polySize) {m_PolySize = polySize;}
  
protected:
  uint64_t ComputeHeaderSize() const;
  virtual uint64_t GetHeaderFromFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian);
  virtual uint64_t CopyToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian, bool bIsLastBlock);
  virtual uint64_t GetOffsetToNextBlock() const;

  virtual void CopyHeaderToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian, bool bIsLastBlock);
  virtual DataBlock* Clone() const;

  friend class UVF;

  // raw floats
  std::vector< float > vertices;
  std::vector< float > normals;
  std::vector< float > texcoords;
  std::vector< float > colors;

  // indices
  std::vector< uint32_t > vIndices;
  std::vector< uint32_t > nIndices;
  std::vector< uint32_t > tIndices;
  std::vector< uint32_t > cIndices;

  bool verticesValid;
  bool normalsValid;
  bool texcoordsValid;
  bool colorsValid;

  bool vertexIValid;
  bool normalIValid;
  bool texcoordIValid;
  bool colorIValid;

  std::vector< float > m_DefaultColor;

  uint64_t               m_PolySize;

private:
  bool   m_bIsBigEndian;


  uint64_t m_n_vertices;
  uint64_t m_n_normals;
  uint64_t m_n_texcoords;
  uint64_t m_n_colors;

  uint64_t m_n_vertex_indices;
  uint64_t m_n_normal_indices;
  uint64_t m_n_texcoord_indices;
  uint64_t m_n_color_indices;

};
#endif // GEOMETRYDATABLOCK_H
