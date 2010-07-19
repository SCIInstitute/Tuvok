#ifndef KDTREE_H
#define KDTREE_H

#include "Mesh.h"
#include <fstream>

typedef std::vector<size_t> triVec;

class KDTreeNode
{
public:
  KDTreeNode(std::ifstream& kdfile) :     
    m_Axis(0),
    m_SplitPos(0),
    m_leftChild(0), 
    m_rightChild(0),
    m_bIsLeaf(true) 
  {
    unsigned int itemCount;
    int iAxis;
    kdfile >> iAxis>> m_SplitPos>> m_bIsLeaf >> itemCount;
    m_Axis = (unsigned char)(iAxis);
    m_items.resize(itemCount);
    for (unsigned int i=0;i<itemCount;i++) kdfile >> m_items[i];
    if (!m_bIsLeaf) { 
      m_leftChild  = new KDTreeNode(kdfile);
      m_rightChild = new KDTreeNode(kdfile);      
    }
  }

  KDTreeNode() : 
    m_Axis(0),
    m_SplitPos(0),
    m_leftChild(0), 
    m_rightChild(0),
    m_bIsLeaf(true)
  {}

  ~KDTreeNode() {
    delete m_leftChild;
    delete m_rightChild;
  }

  void Save(std::ofstream& kdfile) const {
    int iAxis = m_Axis;
    kdfile << iAxis << " " << m_SplitPos << " " << m_bIsLeaf 
           << " " << (unsigned int)(m_items.size()) << std::endl;
    for (unsigned int i=0;i<(unsigned int)m_items.size();i++) 
      kdfile << m_items[i] << " ";
    kdfile << std::endl;
    if (!m_bIsLeaf) { 
      m_leftChild->Save(kdfile);
      m_rightChild->Save(kdfile);   
    }
  }

  void GetGeometry(VertVec& vertices, NormVec& normals,
                   IndexVec& vIndices, IndexVec& nIndices,
                   const FLOATVECTOR3& min, const FLOATVECTOR3& max,
                   unsigned int iDepth) {
    UINT32 sNormals = UINT32(normals.size());
    UINTVECTOR3 iNormals(sNormals,sNormals,sNormals);
    nIndices.push_back(iNormals);
    nIndices.push_back(iNormals);
    FLOATVECTOR3 normal(0,0,0);
    normal[m_Axis] = 1.0;
    normals.push_back(normal);

    UINT32 sVertices = UINT32(vertices.size());
    UINTVECTOR3 iVertices1(sVertices,sVertices+1,sVertices+3);
    UINTVECTOR3 iVertices2(sVertices+2,sVertices+3,sVertices+0);
    vIndices.push_back(iVertices1);
    vIndices.push_back(iVertices2);

    FLOATVECTOR3 vertex1 = min;
    FLOATVECTOR3 vertex2 = min;
    FLOATVECTOR3 vertex3 = min;
    FLOATVECTOR3 vertex4 = max;
    vertex1[m_Axis] = float(m_SplitPos);
    vertex2[m_Axis] = float(m_SplitPos);
    vertex3[m_Axis] = float(m_SplitPos);
    vertex4[m_Axis] = float(m_SplitPos);

    switch (m_Axis) {
      case 0  : vertex2.y = max.y; vertex3.z = max.z; break;
      case 1  : vertex2.x = max.x; vertex3.z = max.z; break;
      default : vertex2.x = max.x; vertex3.y = max.y; break;
    }

    vertices.push_back(vertex1);
    vertices.push_back(vertex2);
    vertices.push_back(vertex3);
    vertices.push_back(vertex4);

    if (!m_bIsLeaf && iDepth > 0) {
      FLOATVECTOR3 max1 = max; max1[m_Axis] = float(m_SplitPos);
      FLOATVECTOR3 min2 = min; min2[m_Axis] = float(m_SplitPos);

      m_leftChild->GetGeometry(vertices, normals,
                               vIndices, nIndices,
                               min, max1, iDepth-1);
      m_rightChild->GetGeometry(vertices, normals,
                                vIndices, nIndices,
                                min2, max, iDepth-1);
    }

  }


  void SetAxis( unsigned char axis ) { m_Axis = axis; }
  int GetAxis() const { return m_Axis; }
  void SetSplitPos( double pos ) { m_SplitPos = pos; }
  double GetSplitPos() const  { return m_SplitPos; }
  void SetLeft( KDTreeNode* leftChild ) { m_leftChild = leftChild; }
  void SetRight( KDTreeNode* rightChild ) { m_rightChild = rightChild; }
  KDTreeNode* GetLeft() const { return m_leftChild; }
  KDTreeNode* GetRight() const  { return m_rightChild; }
  void Add( size_t triIndex ) {m_items.push_back(triIndex);}
  bool IsLeaf() const { return m_bIsLeaf; }
  void SetLeaf( bool bIsLeaf ) { m_bIsLeaf = bIsLeaf; }	  
  triVec& GetList() { return m_items; }

private:
  unsigned char m_Axis;
  double        m_SplitPos;
  KDTreeNode*   m_leftChild;
  KDTreeNode*   m_rightChild;
  triVec        m_items;     
  bool          m_bIsLeaf;
};

class KDTree
{
public:
  KDTree(Mesh* mesh, const std::string& filename = "",
         unsigned int maxDepth = 20);
  ~KDTree(void);

  double Intersect(const Ray& ray, FLOATVECTOR3& normal,
                   FLOATVECTOR2& tc, FLOATVECTOR4& color,
                   double tmin, double tmax) const;
  Mesh* GetGeometry(unsigned int iDepth, bool buildKDTree) const;

private:
  Mesh*        m_mesh;
  unsigned int m_maxDepth;
  KDTreeNode*  m_Root;

  void Subdivide(KDTreeNode* node, const DOUBLEVECTOR3& min,
                 const DOUBLEVECTOR3& max, int recDepth);
};
#endif // KDTREE_H
