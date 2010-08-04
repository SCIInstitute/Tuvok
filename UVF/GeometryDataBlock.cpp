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

//!    File   : GeometryDataBlock.cpp
//!    Author : Jens Krueger
//!             IVCI & DFKI & MMCI, Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : July 2010
//
//!    Copyright (C) 2010 DFKI, MMCI, SCI Institute

#include <sstream>

#include "GeometryDataBlock.h"
#include "UVF.h"

using namespace std;
using namespace UVFTables;

GeometryDataBlock::GeometryDataBlock() : 
  DataBlock(),
  m_n_vertices(0),
  m_n_normals(0),
  m_n_texcoords(0),
  m_n_colors(0),
  m_n_vertex_indices(0),
  m_n_normal_indices(0),
  m_n_texcoord_indices(0),
  m_n_color_indices(0)
{
  ulBlockSemantics = BS_GEOMETRY;
  strBlockID       = "Geometry Block";
  m_DefaultColor.push_back(1);
  m_DefaultColor.push_back(1);
  m_DefaultColor.push_back(1);
  m_DefaultColor.push_back(1);
}

GeometryDataBlock::GeometryDataBlock(const GeometryDataBlock& other) :
  DataBlock(other), 
  m_Desc(other.m_Desc),
  vertices(other.GetVertices()),
  normals(other.GetNormals()),
  texcoords(other.GetTexCoords()),
  colors(other.GetColors()),
  vIndices(other.GetVertexIndices()),
  nIndices(other.GetNormalIndices()),
  tIndices(other.GetTexCoordIndices()),
  cIndices(other.GetColorIndices()),
  m_DefaultColor(other.m_DefaultColor),
  m_PolySize(other.m_PolySize),
  m_bIsBigEndian(other.m_bIsBigEndian),

  m_n_vertices(0),
  m_n_normals(0),
  m_n_texcoords(0),
  m_n_colors(0),
  m_n_vertex_indices(0),
  m_n_normal_indices(0),
  m_n_texcoord_indices(0),
  m_n_color_indices(0)
{
}

GeometryDataBlock::GeometryDataBlock(LargeRAWFile* pStreamFile, UINT64 iOffset,
                                     bool bIsBigEndian) :
  DataBlock(pStreamFile, iOffset, bIsBigEndian)
{
  m_DefaultColor.resize(4);
  GetHeaderFromFile(pStreamFile, iOffset, bIsBigEndian);
}

GeometryDataBlock::~GeometryDataBlock() {
  vertices.clear();
  normals.clear();
  texcoords.clear();
  colors.clear();

  vIndices.clear();
  nIndices.clear(); 
  tIndices.clear(); 
  cIndices.clear(); 
}

GeometryDataBlock& GeometryDataBlock::operator=(const GeometryDataBlock& other)
{
  strBlockID = other.strBlockID;
  ulBlockSemantics = other.ulBlockSemantics;

  vertices = other.GetVertices();
  normals = other.GetNormals();
  texcoords = other.GetTexCoords();
  colors = other.GetColors();

  vIndices = other.GetVertexIndices();
  nIndices = other.GetNormalIndices();
  tIndices = other.GetTexCoordIndices();
  cIndices = other.GetColorIndices();

  m_n_vertices = 0;
  m_n_normals = 0;
  m_n_texcoords = 0;
  m_n_colors = 0;
  m_n_vertex_indices = 0;
  m_n_normal_indices = 0;
  m_n_texcoord_indices = 0;
  m_n_color_indices = 0;
  m_PolySize = other.m_PolySize;

  return *this;
}

bool GeometryDataBlock::Verify(UINT64 iSizeofData, string* pstrProblem) const
{
  UINT64 iCorrectSize = ComputeDataSize();
  bool bResult = iCorrectSize == iSizeofData;

  if (!bResult && pstrProblem != NULL)  {
    stringstream s;
    s << "GeometryDataBlock::Verify: size mismatch. Should be " << iCorrectSize << " but parameter was " << iSizeofData << ".";
    *pstrProblem = s.str();
  }

  return bResult;
}

UINT64 GeometryDataBlock::ComputeHeaderSize() const {
  // n_vertices + n_normals + n_texcoords + n_colors + n_vertices_indices + 
  // n_normals_indices + n_texcoords_indices + n_colors_indices + 
  // m_DefaultColor + desc + 
  // polysize
  return sizeof(UINT64) * 8 + 4 * sizeof(float) + 
         sizeof(UINT64) + m_Desc.size() * sizeof(char)+
         sizeof(UINT64);
}

UINT64 GeometryDataBlock::ComputeDataSize() const
{
  return sizeof(float) * ((vertices.size())  ? vertices.size()  : m_n_vertices) +  // 3d vertices
         sizeof(float) * ((normals.size())   ? normals.size()   : m_n_normals) +   // 3d normals
         sizeof(float) * ((texcoords.size()) ? texcoords.size() : m_n_texcoords) + // 2d texcoords
         sizeof(float) * ((colors.size())    ? colors.size()    : m_n_colors) +    // 4d colors

         sizeof(UINT32) * ((vIndices.size()) ? vIndices.size() : m_n_vertex_indices) + // vertex indices
         sizeof(UINT32) * ((nIndices.size()) ? nIndices.size() : m_n_normal_indices) + // normal indices
         sizeof(UINT32) * ((tIndices.size()) ? tIndices.size() : m_n_texcoord_indices) + // texcoord indices
         sizeof(UINT32) * ((cIndices.size()) ? cIndices.size() : m_n_color_indices);  // color indices
}

DataBlock* GeometryDataBlock::Clone() const 
{
  return new GeometryDataBlock(*this);
}

UINT64 GeometryDataBlock::GetHeaderFromFile(LargeRAWFile* stream,
                                            UINT64 offset, bool big_endian)
{
  UINT64 start = offset + DataBlock::GetHeaderFromFile(stream, offset,
                                                       big_endian);
  stream->SeekPos(start);

  stream->ReadData(m_n_vertices, big_endian);
  stream->ReadData(m_n_normals, big_endian);
  stream->ReadData(m_n_texcoords, big_endian);
  stream->ReadData(m_n_colors, big_endian);

  stream->ReadData(m_n_vertex_indices, big_endian);
  stream->ReadData(m_n_normal_indices, big_endian);
  stream->ReadData(m_n_texcoord_indices, big_endian);
  stream->ReadData(m_n_color_indices, big_endian);

  stream->ReadData(m_DefaultColor[0], big_endian);
  stream->ReadData(m_DefaultColor[1], big_endian);
  stream->ReadData(m_DefaultColor[2], big_endian);
  stream->ReadData(m_DefaultColor[3], big_endian);

  UINT64 ulStringLengthDesc;
  m_pStreamFile->ReadData(ulStringLengthDesc, big_endian);
  m_pStreamFile->ReadData(m_Desc, ulStringLengthDesc);

  stream->ReadData(m_PolySize, big_endian);

  m_bIsBigEndian = big_endian;
  return stream->GetPos() - offset;
}

void GeometryDataBlock::CopyHeaderToFile(LargeRAWFile* pStreamFile, UINT64 iOffset, bool bIsBigEndian, bool bIsLastBlock) {
  DataBlock::CopyHeaderToFile(pStreamFile, iOffset, bIsBigEndian, bIsLastBlock);

  // have we read the data already, then use their size
  // otherwise me might just be rewriting the header
  if (!vertices.empty()) {
    m_n_vertices = vertices.size();
    m_n_normals = normals.size();
    m_n_texcoords  = texcoords.size();
    m_n_colors = colors.size();
    m_n_vertex_indices = vIndices.size();
    m_n_normal_indices = nIndices.size();
    m_n_texcoord_indices = tIndices.size();
    m_n_color_indices = cIndices.size();
  }

  pStreamFile->WriteData(m_n_vertices,         bIsBigEndian);
  pStreamFile->WriteData(m_n_normals,          bIsBigEndian);
  pStreamFile->WriteData(m_n_texcoords,        bIsBigEndian);
  pStreamFile->WriteData(m_n_colors,           bIsBigEndian);
  pStreamFile->WriteData(m_n_vertex_indices,   bIsBigEndian);
  pStreamFile->WriteData(m_n_normal_indices,   bIsBigEndian);
  pStreamFile->WriteData(m_n_texcoord_indices, bIsBigEndian);
  pStreamFile->WriteData(m_n_color_indices,    bIsBigEndian);
  pStreamFile->WriteData(m_DefaultColor[0],    bIsBigEndian);
  pStreamFile->WriteData(m_DefaultColor[1],    bIsBigEndian);
  pStreamFile->WriteData(m_DefaultColor[2],    bIsBigEndian);
  pStreamFile->WriteData(m_DefaultColor[3],    bIsBigEndian);
  pStreamFile->WriteData(UINT64(m_Desc.size()),bIsBigEndian);
  pStreamFile->WriteData(m_Desc);
  pStreamFile->WriteData(m_PolySize          , bIsBigEndian);
}


UINT64 GeometryDataBlock::CopyToFile(LargeRAWFile* pStreamFile, UINT64 iOffset,
                                     bool bIsBigEndian, bool bIsLastBlock)
{
  CopyHeaderToFile(pStreamFile, iOffset, bIsBigEndian, bIsLastBlock);

  pStreamFile->WriteData(vertices, bIsBigEndian);
  pStreamFile->WriteData(normals,  bIsBigEndian);
  pStreamFile->WriteData(texcoords,bIsBigEndian);
  pStreamFile->WriteData(colors,   bIsBigEndian);
  pStreamFile->WriteData(vIndices, bIsBigEndian);
  pStreamFile->WriteData(nIndices, bIsBigEndian);
  pStreamFile->WriteData(tIndices, bIsBigEndian);
  pStreamFile->WriteData(cIndices, bIsBigEndian);

  m_bIsBigEndian = bIsBigEndian;

  return pStreamFile->GetPos() - iOffset;
}

UINT64 GeometryDataBlock::GetOffsetToNextBlock() const
{
  return DataBlock::GetOffsetToNextBlock() + ComputeHeaderSize() + ComputeDataSize();
}

vector< float > GeometryDataBlock::GetVertices() const {
  vector< float > v;
  if (vertices.empty()) {
    m_pStreamFile->SeekPos(m_iOffset+DataBlock::GetOffsetToNextBlock() + ComputeHeaderSize());
    m_pStreamFile->ReadData(v, m_n_vertices, m_bIsBigEndian);
    return v;
  } 
  return vertices;
}

vector< float > GeometryDataBlock::GetNormals() const {
  vector< float > v;
  if (normals.empty()) {
    m_pStreamFile->SeekPos(m_iOffset+DataBlock::GetOffsetToNextBlock() + ComputeHeaderSize()+
                           sizeof(float)*m_n_vertices);
    m_pStreamFile->ReadData(v, m_n_normals, m_bIsBigEndian);
    return v;
  }
  return normals;
}

vector< float > GeometryDataBlock::GetTexCoords() const {
  vector< float > v;
  if (texcoords.empty()) {
    m_pStreamFile->SeekPos(m_iOffset+DataBlock::GetOffsetToNextBlock() + ComputeHeaderSize()+
                           sizeof(float)*m_n_vertices+
                           sizeof(float)*m_n_normals);
    m_pStreamFile->ReadData(v, m_n_texcoords, m_bIsBigEndian);
    return v;
  }
  return texcoords;
}

vector< float > GeometryDataBlock::GetColors() const {
  vector< float > v;
  if (colors.empty()) {
    m_pStreamFile->SeekPos(m_iOffset+DataBlock::GetOffsetToNextBlock() + ComputeHeaderSize()+
                           sizeof(float)*m_n_vertices+
                           sizeof(float)*m_n_normals+
                           sizeof(float)*m_n_texcoords);
    m_pStreamFile->ReadData(v, m_n_colors, m_bIsBigEndian);
    return v;
  }
  return colors;
}

vector< UINT32 > GeometryDataBlock::GetVertexIndices() const {
  vector< UINT32 > v;
  if (vIndices.empty()) {
    m_pStreamFile->SeekPos(m_iOffset+DataBlock::GetOffsetToNextBlock() + ComputeHeaderSize()+
                           sizeof(float)*m_n_vertices+
                           sizeof(float)*m_n_normals+
                           sizeof(float)*m_n_texcoords+
                           sizeof(float)*m_n_colors);
    m_pStreamFile->ReadData(v, m_n_vertex_indices, m_bIsBigEndian);
    return v;
  }
  return vIndices;
}

vector< UINT32 > GeometryDataBlock::GetNormalIndices() const {
  vector< UINT32 > v;
  if (nIndices.empty()) {
    m_pStreamFile->SeekPos(m_iOffset+DataBlock::GetOffsetToNextBlock() + ComputeHeaderSize()+
                           sizeof(float)*m_n_vertices+
                           sizeof(float)*m_n_normals+
                           sizeof(float)*m_n_texcoords+
                           sizeof(float)*m_n_colors+
                           sizeof(UINT32)*m_n_vertex_indices);
    m_pStreamFile->ReadData(v, m_n_normal_indices, m_bIsBigEndian);
    return v;
  }
  return nIndices;
}

vector< UINT32 > GeometryDataBlock::GetTexCoordIndices() const {
  vector< UINT32 > v;
  if (tIndices.empty()) {
    m_pStreamFile->SeekPos(m_iOffset+DataBlock::GetOffsetToNextBlock() + ComputeHeaderSize()+
                           sizeof(float)*m_n_vertices+
                           sizeof(float)*m_n_normals+
                           sizeof(float)*m_n_texcoords+
                           sizeof(float)*m_n_colors+
                           sizeof(UINT32)*m_n_vertex_indices+
                           sizeof(UINT32)*m_n_normal_indices);
    m_pStreamFile->ReadData(v, m_n_texcoord_indices, m_bIsBigEndian);
    return v;
  }
  return tIndices;
}

vector< UINT32 > GeometryDataBlock::GetColorIndices() const {
  vector< UINT32 > v;
  if (cIndices.empty()) {
    m_pStreamFile->SeekPos(m_iOffset+DataBlock::GetOffsetToNextBlock() + ComputeHeaderSize()+
                           sizeof(float)*m_n_vertices+
                           sizeof(float)*m_n_normals+
                           sizeof(float)*m_n_texcoords+
                           sizeof(float)*m_n_colors+
                           sizeof(UINT32)*m_n_vertex_indices+
                           sizeof(UINT32)*m_n_normal_indices+
                           sizeof(UINT32)*m_n_texcoord_indices);
    m_pStreamFile->ReadData(v, m_n_color_indices, m_bIsBigEndian);
    return v;
  }
  return cIndices;
}
