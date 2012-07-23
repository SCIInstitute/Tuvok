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

//!    File   : Mesh.cpp
//!    Author : Jens Krueger
//!             IVCI & DFKI & MMCI, Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : July 2010
//
//!    Copyright (C) 2010 DFKI, MMCI, SCI Institute

#include <algorithm>
#include "Mesh.h"
#include "KDTree.h"

using namespace tuvok;

Mesh::Mesh() :
  m_KDTree(0),
  m_DefColor(1,1,1,1),
  m_MeshDesc("Generic Triangle Mesh"),
  m_meshType(MT_TRIANGLES)
{
  m_VerticesPerPoly = (m_meshType == Mesh::MT_TRIANGLES) ? 3 : 2;
}

Mesh::Mesh(const BasicMeshData& bmd,
  bool bBuildKDTree, bool bScaleToUnitCube,
  const std::string& desc, EMeshType meshType) :
  m_KDTree(0),
  m_Data(bmd),
  m_DefColor(1,1,1,1),
  m_MeshDesc(desc),
  m_meshType(meshType)
{
  m_VerticesPerPoly = (m_meshType == Mesh::MT_TRIANGLES) ? 3 : 2;

  ComputeAABB();
  if (bScaleToUnitCube) ScaleToUnitCube(); 
  if (bBuildKDTree) m_KDTree = new KDTree(this);
}

Mesh::Mesh(const VertVec& vertices, const NormVec& normals,
           const TexCoordVec& texcoords, const ColorVec& colors,
           const IndexVec& vIndices, const IndexVec& nIndices,
           const IndexVec& tIndices, const IndexVec& cIndices,
           bool bBuildKDTree, bool bScaleToUnitCube,
           const std::string& desc, EMeshType meshType) :
  m_KDTree(0),
  m_Data(vertices,normals,texcoords,colors,vIndices,nIndices,tIndices,cIndices),
  m_DefColor(1,1,1,1),
  m_MeshDesc(desc),
  m_meshType(meshType)
{
  m_VerticesPerPoly = (m_meshType == Mesh::MT_TRIANGLES) ? 3 : 2;

  ComputeAABB();
  if (bScaleToUnitCube) ScaleToUnitCube(); 
  if (bBuildKDTree) m_KDTree = new KDTree(this);
}


void Mesh::ComputeAABB() {
  if (m_Data.m_vertices.empty()) return;

  m_Bounds[0] = m_Data.m_vertices[0];
  m_Bounds[1] = m_Data.m_vertices[0];

  for (VertVec::iterator i = m_Data.m_vertices.begin()+1;i<m_Data.m_vertices.end();i++) {
    if (i->x < m_Bounds[0].x) m_Bounds[0].x = i->x;
    if (i->x > m_Bounds[1].x) m_Bounds[1].x = i->x;
    if (i->y < m_Bounds[0].y) m_Bounds[0].y = i->y;
    if (i->y > m_Bounds[1].y) m_Bounds[1].y = i->y;
    if (i->z < m_Bounds[0].z) m_Bounds[0].z = i->z;
    if (i->z > m_Bounds[1].z) m_Bounds[1].z = i->z;
  }
}

void Mesh::ComputeUnitCubeScale(FLOATVECTOR3& scale, 
                                FLOATVECTOR3& translation) {
  if (m_Data.m_vertices.empty()) {
    translation = FLOATVECTOR3(0,0,0);
    scale = FLOATVECTOR3(1,1,1);
    return;
  }

  FLOATVECTOR3 maxExtensionV = (m_Bounds[1]-m_Bounds[0]);

  float maxExtension = (maxExtensionV.x > maxExtensionV.y)
                          ? ((maxExtensionV.x > maxExtensionV.z) ?
                              maxExtensionV.x : maxExtensionV.z)
                          : ((maxExtensionV.y > maxExtensionV.z) ? 
                              maxExtensionV.y : maxExtensionV.z);

  scale = 1.0f/maxExtension;
  translation = - (m_Bounds[1]+m_Bounds[0])/(2*maxExtension);
}

void Mesh::Transform(const FLOATMATRIX4& m) {
  for (VertVec::iterator i = m_Data.m_vertices.begin();i<m_Data.m_vertices.end();i++) {
    *i = (FLOATVECTOR4((*i),1)*m).xyz();
  }

  m_TransformFromOriginal = m_TransformFromOriginal * m;
  GeometryHasChanged(true, true);
}


void Mesh::Clone(const Mesh* other) {
  m_Data.m_vertices = other->m_Data.m_vertices;
  m_Data.m_normals = other->m_Data.m_normals;
  m_Data.m_texcoords = other->m_Data.m_texcoords;
  m_Data.m_colors = other->m_Data.m_colors;

  m_Data.m_VertIndices = other->m_Data.m_VertIndices;
  m_Data.m_NormalIndices = other->m_Data.m_NormalIndices;
  m_Data.m_TCIndices = other->m_Data.m_TCIndices;
  m_Data.m_COLIndices = other->m_Data.m_COLIndices;

  m_DefColor = other->m_DefColor;
  m_MeshDesc = other->m_MeshDesc;
  m_meshType = other->m_meshType;

  m_VerticesPerPoly = other->m_VerticesPerPoly;
  m_TransformFromOriginal = other->m_TransformFromOriginal;

  GeometryHasChanged(true, true);
}


void Mesh::ScaleAndBias(const FLOATVECTOR3& scale,
                        const FLOATVECTOR3& translation) {

  for (VertVec::iterator i = m_Data.m_vertices.begin();i<m_Data.m_vertices.end();i++)
	  *i = (*i*scale) + translation;

  m_Bounds[0] = (m_Bounds[0] * scale) + translation;
  m_Bounds[1] = (m_Bounds[1] * scale) + translation;

  FLOATMATRIX4 s;  s.Scaling(scale);
  FLOATMATRIX4 b;  b.Translation(translation);
  m_TransformFromOriginal = m_TransformFromOriginal * s * b;

  GeometryHasChanged(false, true);
}

void Mesh::GeometryHasChanged(bool bUpdateAABB, bool bUpdateKDtree) {
  if (bUpdateAABB) ComputeAABB();
  if (bUpdateKDtree && m_KDTree) ComputeKDTree();
}

void Mesh::ScaleToUnitCube() {
  FLOATVECTOR3 scale, translation;
  ComputeUnitCubeScale(scale,translation);
  ScaleAndBias(scale,translation);
}

Mesh::~Mesh() {
  delete m_KDTree;
}

void Mesh::RecomputeNormals() {
  if (m_meshType != MT_TRIANGLES) return;

  m_Data.m_normals.resize(m_Data.m_vertices.size());
  for(size_t i = 0;i<m_Data.m_normals.size();i++) m_Data.m_normals[i] = FLOATVECTOR3();

  for (size_t i = 0;i<m_Data.m_VertIndices.size();i+=3) {
    UINTVECTOR3 indices(m_Data.m_VertIndices[i], m_Data.m_VertIndices[i+1], m_Data.m_VertIndices[i+2]);

    FLOATVECTOR3 tang = m_Data.m_vertices[indices.x]-m_Data.m_vertices[indices.y];
    FLOATVECTOR3 bin  = m_Data.m_vertices[indices.x]-m_Data.m_vertices[indices.z];

    FLOATVECTOR3 norm = bin % tang;
  	
    m_Data.m_normals[indices.x] = m_Data.m_normals[indices.x]+norm;
    m_Data.m_normals[indices.y] = m_Data.m_normals[indices.y]+norm;
    m_Data.m_normals[indices.z] = m_Data.m_normals[indices.z]+norm;
  }
  for(size_t i = 0;i<m_Data.m_normals.size();i++) {
    float l = m_Data.m_normals[i].length();
    if (l > 0) m_Data.m_normals[i] = m_Data.m_normals[i] / l;;
  }

  m_Data.m_NormalIndices = m_Data.m_VertIndices;
}

bool Mesh::UnifyIndices() {

  if (m_Data.m_NormalIndices.empty() &&
      m_Data.m_TCIndices.empty() && 
      m_Data.m_COLIndices.empty()) return true;


  if (!Validate()) return false;
  if (HasUniformIndices()) return true;

  VertVec     vertices(m_Data.m_vertices);
  NormVec     normals(m_Data.m_normals);
  TexCoordVec texcoords(m_Data.m_texcoords);
  ColorVec    colors(m_Data.m_colors);

  // compute an inverse lookup index, i.e. for each
  // vertex, store what indices point to it
  std::vector< std::vector<size_t> > inverseIndex(m_Data.m_vertices.size());
  for (size_t i = 0;i<m_Data.m_VertIndices.size();++i) {
    inverseIndex[m_Data.m_VertIndices[i]].push_back(i);
  }

  for (size_t i = 0;i<m_Data.m_VertIndices.size();++i) {
    uint32_t index = m_Data.m_VertIndices[i];

    // simple resort
    if (inverseIndex[index].size() == 1 || (inverseIndex[index].size() > 1 && inverseIndex[index][0] == i)) {      
      if (!m_Data.m_NormalIndices.empty()) normals[index] = m_Data.m_normals[m_Data.m_NormalIndices[i]];
      if (!m_Data.m_TCIndices.empty()) texcoords[index] = m_Data.m_texcoords[m_Data.m_TCIndices[i]];
      if (!m_Data.m_COLIndices.empty()) colors[index] = m_Data.m_colors[m_Data.m_COLIndices[i]];
      continue;
    }

    // more complicated case of a multiply used point
    if (inverseIndex[index].size() > 1) {

      if ((!m_Data.m_NormalIndices.empty() && m_Data.m_normals[m_Data.m_NormalIndices[i]] != m_Data.m_normals[m_Data.m_NormalIndices[inverseIndex[index][0]]]) ||
          (!m_Data.m_TCIndices.empty() && m_Data.m_texcoords[m_Data.m_TCIndices[i]] != m_Data.m_texcoords[m_Data.m_TCIndices[inverseIndex[index][0]]]) ||
          (!m_Data.m_COLIndices.empty() && m_Data.m_colors[m_Data.m_COLIndices[i]] != m_Data.m_colors[m_Data.m_COLIndices[inverseIndex[index][0]]])) {

        m_Data.m_VertIndices[i] = uint32_t(vertices.size());
        vertices.push_back(m_Data.m_vertices[index]);

        if (!m_Data.m_NormalIndices.empty()) normals.push_back( m_Data.m_normals[m_Data.m_NormalIndices[i]]);
        if (!m_Data.m_TCIndices.empty()) texcoords.push_back(m_Data.m_texcoords[m_Data.m_TCIndices[i]]);
        if (!m_Data.m_COLIndices.empty()) colors.push_back(m_Data.m_colors[m_Data.m_COLIndices[i]]);
      } else {
        if (!m_Data.m_NormalIndices.empty()) normals[index] = m_Data.m_normals[m_Data.m_NormalIndices[inverseIndex[index][0]]];
        if (!m_Data.m_TCIndices.empty()) texcoords[index] = m_Data.m_texcoords[m_Data.m_TCIndices[inverseIndex[index][0]]];
        if (!m_Data.m_COLIndices.empty()) colors[index] = m_Data.m_colors[m_Data.m_COLIndices[inverseIndex[index][0]]];
      }
    }
  }

  m_Data.m_vertices = vertices;
  m_Data.m_normals = normals;
  m_Data.m_texcoords = texcoords;
  m_Data.m_colors =colors;

  m_Data.m_NormalIndices = m_Data.m_TCIndices = m_Data.m_COLIndices = m_Data.m_VertIndices;

  return true;
}

std::vector<Mesh*> Mesh::PartitionMesh(size_t iMaxIndexCount, bool bOptimize) const {
  const Mesh* source;
  if (HasUniformIndices()) {
    source = this;
  } else {
    Mesh* unifiedIndexMesh = new Mesh();
    unifiedIndexMesh->Clone(this);
    unifiedIndexMesh->UnifyIndices();
    source = unifiedIndexMesh;
  }

  // march over all vertices and hash them into the sub-meshes
  // based on their index modulo iMaxIndex, those primitives that
  // have indices that spawn over multiple of those sub-meshes
  // are stored in an boundary list
  std::vector<size_t> boundaryList;
  std::vector<BasicMeshData> basicMeshVec( size_t( ceil(source->m_Data.m_vertices.size() / double(iMaxIndexCount)) ) );

  size_t iLastBinsize = source->m_Data.m_vertices.size()%iMaxIndexCount;

  for (size_t i = 0;i<basicMeshVec.size();++i) {
    size_t iBinsize = (i == basicMeshVec.size()-1 ) ? iLastBinsize : iMaxIndexCount;

    basicMeshVec[i].m_vertices.resize(iBinsize);
    if (!source->m_Data.m_NormalIndices.empty()) basicMeshVec[i].m_normals.resize(iBinsize);
    if (!source->m_Data.m_TCIndices.empty())     basicMeshVec[i].m_texcoords.resize(iBinsize);
    if (!source->m_Data.m_COLIndices.empty())    basicMeshVec[i].m_colors.resize(iBinsize);
  }


  for (size_t i = 0;i<source->m_Data.m_VertIndices.size();i+=m_VerticesPerPoly) {
    bool bConsistentBin = true;
    size_t iTargetBin = source->m_Data.m_VertIndices[i] / iMaxIndexCount;
    for (size_t j = 1;j<m_VerticesPerPoly;++j) {
      if (source->m_Data.m_VertIndices[i+j] / iMaxIndexCount != iTargetBin) {
        bConsistentBin = false;
        break;
      }
    }

    if (!bConsistentBin) {
      boundaryList.push_back(i);
      continue;
    }

    size_t iIndexTransform = iMaxIndexCount*iTargetBin;    

    for (size_t j = 0;j<m_VerticesPerPoly;++j) {
      uint32_t iNewIndex = uint32_t(source->m_Data.m_VertIndices[i+j] - iIndexTransform);
     
      basicMeshVec[iTargetBin].m_VertIndices.push_back( iNewIndex );
      basicMeshVec[iTargetBin].m_vertices[iNewIndex] = source->m_Data.m_vertices[source->m_Data.m_VertIndices[i+j]];

      if (!source->m_Data.m_NormalIndices.empty()) {
        basicMeshVec[iTargetBin].m_NormalIndices.push_back( iNewIndex );
        basicMeshVec[iTargetBin].m_normals[iNewIndex] = source->m_Data.m_normals[source->m_Data.m_VertIndices[i+j]];
      }
      if (!source->m_Data.m_TCIndices.empty()) {
        basicMeshVec[iTargetBin].m_TCIndices.push_back( iNewIndex );
        basicMeshVec[iTargetBin].m_texcoords[iNewIndex] = source->m_Data.m_texcoords[source->m_Data.m_VertIndices[i+j]];
      }
      if (!source->m_Data.m_COLIndices.empty())  {
        basicMeshVec[iTargetBin].m_COLIndices.push_back( iNewIndex );
        basicMeshVec[iTargetBin].m_colors[iNewIndex] = source->m_Data.m_colors[source->m_Data.m_VertIndices[i+j]];
      }
    }
  }

  if (bOptimize) {
    // remove unused items
    for (size_t i = 0;i<basicMeshVec.size();++i) basicMeshVec[i].RemoveUnusedVertices();
  }

  // insert boundary items into meshes
  size_t iTargetBin = 0;
  for (size_t i = 0;i<boundaryList.size();++i) {
    while (basicMeshVec[iTargetBin].m_VertIndices.size()+m_VerticesPerPoly > iMaxIndexCount) {
      ++iTargetBin;
      if (iTargetBin >= basicMeshVec.size()) {
        basicMeshVec.push_back(BasicMeshData());
        break;
      }
    }

    for (size_t j = 0;j<m_VerticesPerPoly;++j) {
      size_t sourceIndex = boundaryList[i]+j;
      uint32_t iNewIndex = uint32_t(basicMeshVec[iTargetBin].m_vertices.size());

      basicMeshVec[iTargetBin].m_VertIndices.push_back( iNewIndex );
      basicMeshVec[iTargetBin].m_vertices.push_back(source->m_Data.m_vertices[source->m_Data.m_VertIndices[sourceIndex]]);

      if (!source->m_Data.m_NormalIndices.empty()) {
        basicMeshVec[iTargetBin].m_NormalIndices.push_back( iNewIndex );
        basicMeshVec[iTargetBin].m_normals.push_back(source->m_Data.m_normals[source->m_Data.m_VertIndices[sourceIndex]]);
      }
      if (!source->m_Data.m_TCIndices.empty()) {
        basicMeshVec[iTargetBin].m_TCIndices.push_back( iNewIndex );
        basicMeshVec[iTargetBin].m_texcoords.push_back(source->m_Data.m_texcoords[source->m_Data.m_VertIndices[sourceIndex]]);
      }
      if (!source->m_Data.m_COLIndices.empty())  {
        basicMeshVec[iTargetBin].m_COLIndices.push_back( iNewIndex );
        basicMeshVec[iTargetBin].m_colors.push_back(source->m_Data.m_colors[source->m_Data.m_VertIndices[sourceIndex]]);
      }
    }
  }

  if (bOptimize) {
    // TODO: remove duplicate vertices
  }

  // cleanup and convert BasicMeshData back to "full featured" mesh
  if (source != this) delete source;
  std::vector<Mesh*> meshVec(basicMeshVec.size());
  for (size_t i = 0;i<meshVec.size();++i) {
    meshVec[i] = new Mesh(basicMeshVec[i], m_KDTree != nullptr, false, m_MeshDesc, m_meshType);
  }
  return meshVec;
}

bool Mesh::HasUniformIndices() const {
  if (!m_Data.m_NormalIndices.empty() && 
      m_Data.m_NormalIndices != m_Data.m_VertIndices) return false;
  if (!m_Data.m_TCIndices.empty() && 
      m_Data.m_TCIndices != m_Data.m_VertIndices) return false;
  if (!m_Data.m_COLIndices.empty() && 
      m_Data.m_COLIndices != m_Data.m_VertIndices) return false;

  return true;
}

bool Mesh::Validate(bool bDeepValidation) {
  // make sure the sizes of the vectors match
  if (!m_Data.m_NormalIndices.empty() && 
      m_Data.m_NormalIndices.size() != m_Data.m_VertIndices.size()) return false;
  if (!m_Data.m_TCIndices.empty() && 
      m_Data.m_TCIndices.size() != m_Data.m_VertIndices.size()) return false;
  if (!m_Data.m_COLIndices.empty() && 
      m_Data.m_COLIndices.size() != m_Data.m_VertIndices.size()) return false;

  if (!bDeepValidation) return true;

  // in deep validation mode check if all the indices are within range
  size_t count = m_Data.m_vertices.size();
  for (IndexVec::iterator i = m_Data.m_VertIndices.begin();
       i != m_Data.m_VertIndices.end();
       i++) {
    if ((*i) >= count) return false;
  }
  count = m_Data.m_normals.size();
  for (IndexVec::iterator i = m_Data.m_NormalIndices.begin();
       i != m_Data.m_NormalIndices.end();
       i++) {
    if ((*i) >= count) return false;
  }
  count = m_Data.m_texcoords.size();
  for (IndexVec::iterator i = m_Data.m_TCIndices.begin();
       i != m_Data.m_TCIndices.end();
       i++) {
    if ((*i) >= count) return false;
  }
  count = m_Data.m_colors.size();
  for (IndexVec::iterator i = m_Data.m_COLIndices.begin();
       i != m_Data.m_COLIndices.end();
       i++) {
    if ((*i) >= count) return false;
  }

  return true;
}

double Mesh::IntersectInternal(const Ray& ray, FLOATVECTOR3& normal,
                               FLOATVECTOR2& tc, FLOATVECTOR4& color, 
                               double tmin, double tmax) const {
  
  if (m_meshType != MT_TRIANGLES) return noIntersection;

  if (m_KDTree) {
    return m_KDTree->Intersect(ray, normal, tc, color, tmin, tmax);
  } else {
    double t = noIntersection;
    FLOATVECTOR3 _normal;
    FLOATVECTOR2 _tc;
    FLOATVECTOR4 _color;

    for (size_t i = 0;i<m_Data.m_VertIndices.size();i+=3) {
      double currentT = IntersectTriangle(i, ray, _normal, _tc, _color);

      if (currentT < t) {
        normal = _normal;
        t = currentT;
        tc = _tc;
        color = _color;
      }

    }
    return t;
  }
}

double Mesh::IntersectTriangle(size_t i, const Ray& ray, 
                               FLOATVECTOR3& normal, 
                               FLOATVECTOR2& tc, FLOATVECTOR4& color) const {

  double t = std::numeric_limits<double>::max();

  FLOATVECTOR3 vert0 = m_Data.m_vertices[m_Data.m_VertIndices[i]];
  FLOATVECTOR3 vert1 = m_Data.m_vertices[m_Data.m_VertIndices[i+1]];
  FLOATVECTOR3 vert2 = m_Data.m_vertices[m_Data.m_VertIndices[i+2]];

  // find vectors for two edges sharing vert0
  DOUBLEVECTOR3 edge1 = DOUBLEVECTOR3(vert1 - vert0);
  DOUBLEVECTOR3 edge2 = DOUBLEVECTOR3(vert2 - vert0);
   
  // begin calculating determinant - also used to calculate U parameter
  DOUBLEVECTOR3 pvec = ray.direction % edge2;

  // if determinant is near zero, ray lies in plane of triangle
  double det = edge1 ^ pvec;

  if (det > -0.00000001 && det < 0.00000001) return t;
  double inv_det = 1.0 / det;

  // calculate distance from vert0 to ray origin
  DOUBLEVECTOR3 tvec = ray.start - DOUBLEVECTOR3(vert0);

  // calculate U parameter and test bounds
  double u = tvec ^pvec * inv_det;
  if (u < 0.0 || u > 1.0) return t;

  // prepare to test V parameter
  DOUBLEVECTOR3 qvec = tvec % edge1;

  // calculate V parameter and test bounds
  double v = (ray.direction ^ qvec) * inv_det;
  if (v < 0.0 || u + v > 1.0) return t;

  // calculate t, ray intersects triangle
  t = (edge2 ^ qvec) * inv_det;

  if (t<0) return std::numeric_limits<double>::max();

  // interpolate normal
  if (m_Data.m_NormalIndices.size()) {
    FLOATVECTOR3 normal0 = m_Data.m_normals[m_Data.m_NormalIndices[i]];
    FLOATVECTOR3 normal1 = m_Data.m_normals[m_Data.m_NormalIndices[i+1]];
    FLOATVECTOR3 normal2 = m_Data.m_normals[m_Data.m_NormalIndices[i+1]];

    FLOATVECTOR3 du = normal1 - normal0;
    FLOATVECTOR3 dv = normal2 - normal0;
    
    normal = normal0 + du * float(u) + dv * float(v);
  } else {
    // compute face normal if no normals are given
    normal = FLOATVECTOR3(edge1 % edge2);
  }
  normal.normalize();

  if ((FLOATVECTOR3(ray.direction) ^ normal) > 0) normal = normal *-1; 

  // interpolate texture coordinates
  if (m_Data.m_TCIndices.size()) {
    FLOATVECTOR2 tc0 = m_Data.m_texcoords[m_Data.m_TCIndices[i]];
    FLOATVECTOR2 tc1 = m_Data.m_texcoords[m_Data.m_TCIndices[i+1]];
    FLOATVECTOR2 tc2 = m_Data.m_texcoords[m_Data.m_TCIndices[i+2]];

    double dtu1 = tc1.x - tc0.x;
    double dtu2 = tc2.x - tc0.x;
    double dtv1 = tc1.y - tc0.y;
    double dtv2 = tc2.y - tc0.y;
    tc.x = float(tc0.x  + u * dtu1 + v * dtu2);
    tc.y = float(tc0.y + u * dtv1 + v * dtv2);
  } else {
    tc.x = 0;
    tc.y = 0;
  }

  // interpolate color
  if (m_Data.m_COLIndices.size()) {
    FLOATVECTOR4 col0 = m_Data.m_colors[m_Data.m_TCIndices[i]];
    FLOATVECTOR4 col1 = m_Data.m_colors[m_Data.m_TCIndices[i+1]];
    FLOATVECTOR4 col2 = m_Data.m_colors[m_Data.m_TCIndices[i+2]];

    double dtu1 = col1.x - col0.x;
    double dtu2 = col2.x - col0.x;
    double dtv1 = col1.y - col0.y;
    double dtv2 = col2.y - col0.y;
    color.x = float(col0.x  + u * dtu1 + v * dtu2);
    color.y = float(col0.y + u * dtv1 + v * dtv2);
  } else {
    color.x = 0;
    color.y = 0;
  }

  return t;
}


void Mesh::ComputeKDTree() {
  delete m_KDTree;
  m_KDTree = new KDTree(this);
}

const KDTree* Mesh::GetKDTree() const {
  return m_KDTree;
}

bool Mesh::AABBIntersect(const Ray& r, double& tmin, double& tmax) const {
  double tymin, tymax, tzmin, tzmax;

  DOUBLEVECTOR3 inv_direction(1.0/r.direction.x, 
                          1.0/r.direction.y, 
                          1.0/r.direction.z);

  int sign[3]  = {inv_direction.x < 0,
                  inv_direction.y < 0,
                  inv_direction.z < 0};

  tmin  = (m_Bounds[sign[0]].x - r.start.x)   * inv_direction.x;
  tmax  = (m_Bounds[1-sign[0]].x - r.start.x) * inv_direction.x;
  tymin = (m_Bounds[sign[1]].y - r.start.y)   * inv_direction.y;
  tymax = (m_Bounds[1-sign[1]].y - r.start.y) * inv_direction.y;

  if ( (tmin > tymax) || (tymin > tmax) )  return false;
  if (tymin > tmin) tmin = tymin;
  if (tymax < tmax) tmax = tymax;
  tzmin = (m_Bounds[sign[2]].z - r.start.z) * inv_direction.z;
  tzmax = (m_Bounds[1-sign[2]].z - r.start.z) * inv_direction.z;
  if ( (tmin > tzmax) || (tzmin > tmax) )  return false;
  if (tzmin > tmin)
    tmin = tzmin;
  if (tzmax < tmax)
    tmax = tzmax;
  return tmax > 0;
}



void BasicMeshData::RemoveUnusedVertices() {
  RemoveUnusedEntries(m_VertIndices, m_vertices);
  if (!m_NormalIndices.empty()) RemoveUnusedEntries(m_NormalIndices, m_normals);
  if (!m_TCIndices.empty()) RemoveUnusedEntries(m_TCIndices, m_texcoords);
  if (!m_COLIndices.empty()) RemoveUnusedEntries(m_COLIndices, m_colors);
}

