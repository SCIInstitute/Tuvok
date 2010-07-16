#include "Mesh.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include "KDTree.h"

inline int myTolower(int c) {return tolower(static_cast<unsigned char>(c));}

Mesh::Mesh(const VertVec& vertices, const NormVec& normals, const TexCoordVec& texcoords, 
           const IndexVec& vIndices, const IndexVec& nIndices, const IndexVec& tIndices) :
  m_KDTree(0),
  m_vertices(vertices),
  m_normals(normals),
  m_texcoords(texcoords),
  m_VertIndices(vIndices),
  m_NormalIndices(nIndices),
  m_TCIndices(tIndices)
{
  m_KDTree = new KDTree(this);

  if (m_vertices.size() > 0) {
    m_Bounds[0] = m_vertices[0];
    m_Bounds[1] = m_vertices[0];

    for (VertVec::iterator i = m_vertices.begin()+1;i<m_vertices.end();i++) {
      if (i->x < m_Bounds[0].x) m_Bounds[0].x = i->x;
      if (i->x > m_Bounds[1].x) m_Bounds[1].x = i->x;
      if (i->y < m_Bounds[0].y) m_Bounds[0].y = i->y;
      if (i->y > m_Bounds[1].y) m_Bounds[1].y = i->y;
      if (i->z < m_Bounds[0].z) m_Bounds[0].z = i->z;
      if (i->z > m_Bounds[1].z) m_Bounds[1].z = i->z;
    }
  }
}


Mesh::Mesh(const std::string& filename, bool bFlipNormals, const FLOATVECTOR3& translation, const FLOATVECTOR3& scale) : 
  m_KDTree(0)
{
  LoadFile(filename,bFlipNormals,translation,scale);
//  m_KDTree = new KDTree(this, filename+".kdt");
}

Mesh::~Mesh() {
  delete m_KDTree;
}

double Mesh::IntersectInternal(const Ray& ray, FLOATVECTOR3& normal, FLOATVECTOR2& tc, double tmin, double tmax) const {

  if (m_KDTree) {
    return m_KDTree->Intersect(ray, normal, tc, tmin, tmax);
  } else {
    double t = std::numeric_limits<double>::max();
    FLOATVECTOR3 _normal;
    FLOATVECTOR2 _tc;

    for (size_t i = 0;i<m_VertIndices.size();i++) {
      double currentT = IntersectTriangle(i, ray, _normal, _tc);

      if (currentT < t) {
        normal = _normal;
        t = currentT;
        tc = _tc;
      }

    }
    return t;
  }
}


double Mesh::IntersectTriangle(size_t i, const Ray& ray, FLOATVECTOR3& normal, FLOATVECTOR2& tc) const {

  double t = std::numeric_limits<double>::max();

  FLOATVECTOR3 vert0 = m_vertices[m_VertIndices[i].x];
  FLOATVECTOR3 vert1 = m_vertices[m_VertIndices[i].y];
  FLOATVECTOR3 vert2 = m_vertices[m_VertIndices[i].z];

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
  if (m_NormalIndices.size()) {
    FLOATVECTOR3 normal0 = m_normals[m_NormalIndices[i].x];
    FLOATVECTOR3 normal1 = m_normals[m_NormalIndices[i].y];
    FLOATVECTOR3 normal2 = m_normals[m_NormalIndices[i].z];

    FLOATVECTOR3 du = normal1 - normal0;
    FLOATVECTOR3 dv = normal2 - normal0;
    
    normal = normal0 + du * float(u) + dv * float(v);
  } else {
    // compute face normal
    normal = FLOATVECTOR3(edge1 % edge2);
  }
  normal.normalize();

  if ((FLOATVECTOR3(ray.direction) ^ normal) > 0) normal = normal *-1; 

  // interpolate texture coordinates
  if (m_TCIndices.size()) {
    FLOATVECTOR2 tc0 = m_texcoords[m_TCIndices[i].x];
    FLOATVECTOR2 tc1 = m_texcoords[m_TCIndices[i].y];
    FLOATVECTOR2 tc2 = m_texcoords[m_TCIndices[i].z];

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

  return t;
}


inline std::string Mesh::TrimLeft(const std::string& Src, const std::string& c)
{
  size_t p1 = Src.find_first_not_of(c);
  if (p1 == std::string::npos) return std::string();
  return Src.substr(p1);
}

inline std::string Mesh::TrimRight(const std::string& Src, const std::string& c)
{
  size_t p2 = Src.find_last_not_of(c);
  if (p2 == std::string::npos) return std::string();
  return Src.substr(0, p2+1);
}

inline std::string Mesh::TrimToken(const std::string& Src, const std::string& c, bool bOnlyFirst)
{
  size_t off = Src.find_first_of(c);
  if (off == std::string::npos) off = 0;
  if (bOnlyFirst) {
    return Src.substr(off+1);
  } else {
    size_t p1 = Src.find_first_not_of(c,off);
    if (p1 == std::string::npos) return std::string();
    return Src.substr(p1);
  }
}

inline std::string Mesh::Trim(const std::string& Src, const std::string& c)
{
  size_t p2 = Src.find_last_not_of(c);
  if (p2 == std::string::npos) return std::string();
  size_t p1 = Src.find_first_not_of(c);
  if (p1 == std::string::npos) p1 = 0;
  return Src.substr(p1, (p2-p1)+1);
}

inline std::string Mesh::ToLowerCase(const std::string& str) {
  std::string result(str);
  transform(str.begin(), str.end(), result.begin(), myTolower);
  return result;
}

inline int Mesh::CountOccurences(const std::string& str, const std::string& substr) {
  size_t found = str.find_first_of(substr);
  int count = 0;
  while (found!=std::string::npos)
  {
    count++;
    found=str.find_first_of(substr,found+1);
  }
  return count;
}

bool Mesh::LoadFile(const std::string& filename, bool bFlipNormals, const FLOATVECTOR3& translation, const FLOATVECTOR3& scale) {
  std::cout << "Loading Mesh " << filename << std::endl;
	std::ifstream fs;
	std::string line;

	fs.open(filename.c_str());
	if (fs.fail()) return false;

  double x,y,z;

	while (!fs.fail()) {
		getline(fs, line);
		if (fs.fail()) break; // no more lines to read
		line = ToLowerCase(Trim(line));

    std::string linetype = TrimRight(line.substr(0,2));
    if (linetype == "#") continue; // skip comment lines

    line = TrimLeft(line.substr(linetype.length()));

		if (linetype == "v") { // vertex attrib found
				x = atof(line.c_str());
				line = TrimToken(line);
				y = atof(line.c_str());
				line = TrimToken(line);
				z = atof(line.c_str());
				m_vertices.push_back(FLOATVECTOR3(x,y,(bFlipNormals) ? -z : z));
  	} else
	  if (linetype == "vt") {  // vertex texcoord found
			x = atof(line.c_str());
		  line = TrimToken(line);
			y = atof(line.c_str());
			m_texcoords.push_back(FLOATVECTOR2(x,y));
		} else
    if (linetype == "vn") { // vertex normal found
			x = atof(line.c_str());
      line = TrimToken(line);
			y = atof(line.c_str());
      line = TrimToken(line);
      z = atof(line.c_str());
      FLOATVECTOR3 n(x,y,z);
      n.normalize();
      m_normals.push_back(n);
		} else 
    if (linetype == "f") { // face found
      
      int indices[9] = {0,0,0,0,0,0,0,0,0};
      int count = CountOccurences(line,"/");

      switch (count) {
        case 0 : {
              indices[0] = atoi(line.c_str());
              line = TrimToken(line);
              indices[1] = atoi(line.c_str());
              line = TrimToken(line);
              indices[2] = atoi(line.c_str());

              INTVECTOR3 index(indices[0]-1,indices[1]-1,indices[2]-1); 
              m_VertIndices.push_back(index);
              break;
             }
        case 3 : {
              indices[0] = atoi(line.c_str());
              line = TrimToken(line,"/");
              indices[1] = atoi(line.c_str());
              line = TrimToken(line);
              indices[2] = atoi(line.c_str());
              line = TrimToken(line,"/");
              indices[3] = atoi(line.c_str());
              line = TrimToken(line);
              indices[4] = atoi(line.c_str());
              line = TrimToken(line,"/");
              indices[5] = atoi(line.c_str());
              line = TrimToken(line);
              INTVECTOR3 vIndex(indices[0]-1,indices[2]-1,indices[4]-1); 
              m_VertIndices.push_back(vIndex);
              INTVECTOR3 tIndex(indices[1]-1,indices[3]-1,indices[5]-1); 
              m_TCIndices.push_back(tIndex);
              break;
             }
        case 6 : {
              indices[0] = atoi(line.c_str());
              line = TrimToken(line,"/",true);
              indices[1] = atoi(line.c_str());
              line = TrimToken(line,"/",true);
              indices[2] = atoi(line.c_str());
              line = TrimToken(line);
              indices[3] = atoi(line.c_str());
              line = TrimToken(line,"/",true);
              indices[4] = atoi(line.c_str());
              line = TrimToken(line,"/",true);
              indices[5] = atoi(line.c_str());
              line = TrimToken(line);
              indices[6] = atoi(line.c_str());
              line = TrimToken(line,"/",true);
              indices[7] = atoi(line.c_str());
              line = TrimToken(line,"/",true);
              indices[8] = atoi(line.c_str());
              line = TrimToken(line);
              INTVECTOR3 vIndex(indices[0]-1,indices[3]-1,indices[6]-1); 
              if (indices[0]) m_VertIndices.push_back(vIndex);
              INTVECTOR3 tIndex(indices[1]-1,indices[4]-1,indices[7]-1); 
              if (indices[1]) m_TCIndices.push_back(tIndex);
              INTVECTOR3 nIndex(indices[2]-1,indices[5]-1,indices[8]-1); 
              if (indices[2]) m_NormalIndices.push_back(nIndex);
              break;
             }
      }
    }
  }
	fs.close();

  if (m_vertices.size() > 0) {
    m_Bounds[0] = m_vertices[0];
    m_Bounds[1] = m_vertices[0];

    for (VertVec::iterator i = m_vertices.begin()+1;i<m_vertices.end();i++) {
      if (i->x < m_Bounds[0].x) m_Bounds[0].x = i->x;
      if (i->x > m_Bounds[1].x) m_Bounds[1].x = i->x;
      if (i->y < m_Bounds[0].y) m_Bounds[0].y = i->y;
      if (i->y > m_Bounds[1].y) m_Bounds[1].y = i->y;
      if (i->z < m_Bounds[0].z) m_Bounds[0].z = i->z;
      if (i->z > m_Bounds[1].z) m_Bounds[1].z = i->z;
    }

	  DOUBLEVECTOR3 maxExtensionV = DOUBLEVECTOR3((m_Bounds[1]-m_Bounds[0])/scale);

    double maxExtension = (maxExtensionV.x > maxExtensionV.y)
                            ? ((maxExtensionV.x > maxExtensionV.z) ? maxExtensionV.x : maxExtensionV.z)
                            : ((maxExtensionV.y > maxExtensionV.z) ? maxExtensionV.y : maxExtensionV.z);

	  m_Bounds[0] = m_Bounds[0]/maxExtension;
	  m_Bounds[1] = m_Bounds[1]/maxExtension;
	  FLOATVECTOR3 center = (m_Bounds[1]+m_Bounds[0])/2;

    for (VertVec::iterator i = m_vertices.begin();i<m_vertices.end();i++) {
		  *i = (*i/maxExtension) - center + translation;
	  }

    m_Bounds[0] = (m_Bounds[0] - center) + translation;
    m_Bounds[1] = (m_Bounds[1] - center) + translation;
  }

  // recompute normals if necessary
  // if (m_NormalIndices.size() == 0) { // never trust the normals in the file
    m_normals.resize(m_vertices.size());
    for(size_t i = 0;i<m_normals.size();i++) m_normals[i] = FLOATVECTOR3();

	  for(IndexVec::iterator triIter =  m_VertIndices.begin();triIter<m_VertIndices.end();triIter++) {
		  INTVECTOR3 indices = (*triIter);

		  FLOATVECTOR3 tang = m_vertices[indices.x]-m_vertices[indices.y];
		  FLOATVECTOR3 bin  = m_vertices[indices.x]-m_vertices[indices.z];

		  FLOATVECTOR3 norm = bin % tang;
  		
		  m_normals[indices.x] = m_normals[indices.x]+norm;
		  m_normals[indices.y] = m_normals[indices.y]+norm;
		  m_normals[indices.z] = m_normals[indices.z]+norm;
	  }
    for(size_t i = 0;i<m_normals.size();i++) {
      float l = m_normals[i].length();
      if (l > 0) m_normals[i] = m_normals[i] / l;;
    }

	  m_NormalIndices = m_VertIndices;
  //}
  
  std::cout << "Done" << std::endl;
  return true;
}

const KDTree* Mesh::GetKDTree() const {
  return m_KDTree;
}

bool Mesh::AABBIntersect(const Ray& r, double& tmin, double& tmax) {
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

