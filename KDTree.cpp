#include "KDTree.h"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <list>

using namespace tuvok;

class splitElem {
public:
  splitElem(double p=0) : pos(p), n1count(0), n2count(0) {}
  double pos;
  size_t n1count;
  size_t n2count;

  bool operator<(const splitElem& other) const {
    return pos < other.pos;
  }
};

typedef std::vector<splitElem> slist;
typedef std::pair<double, double> dPair;

typedef std::vector< dPair > eventVec;
bool compMin(const dPair& a, const dPair& b) {return a.first < b.first;}
bool compMax(const dPair& a, const dPair& b) {return a.second < b.second;}
bool compFirst(const dPair& a, const double b) {return a.second < b;}


KDTree::KDTree(Mesh* mesh, const std::string& filename, unsigned int maxDepth) :
  m_mesh(mesh),
  m_maxDepth(maxDepth),
  m_Root(0)
{
  if (filename != "") {
    // try to load kd from disk if filename is given
    std::ifstream kdfile;
	  kdfile.open(filename.c_str());
    if (kdfile.is_open()) {
      std::cout << "Loading KD-Tree " << filename << std::endl;
      kdfile >> m_maxDepth;
      m_Root = new KDTreeNode(kdfile);
      kdfile.close();
      std::cout << "Done" << std::endl;
      return;
    }
  }
  assert(m_maxDepth>0);

  std::cout << "Generating KD-Tree" << std::endl;
  m_Root = new KDTreeNode();
	for ( size_t e = 0; e < m_mesh->m_Data.m_VertIndices.size(); e++ ) m_Root->Add( e );
  Subdivide( m_Root, DOUBLEVECTOR3(m_mesh->m_Bounds[0]),
                     DOUBLEVECTOR3(m_mesh->m_Bounds[1]), m_maxDepth );
  std::cout << "Done" << std::endl;

  if (filename != "") {
    std::cout << "Saving KD-Tree " << filename << std::endl;
    std::ofstream kdfile;
	  kdfile.open(filename.c_str());
    if (kdfile.is_open()) {
      kdfile << m_maxDepth << std::endl;
      m_Root->Save(kdfile);
      kdfile.close();
    }
    std::cout << "Done" << std::endl;
  }
}

KDTree::~KDTree(void)
{
  delete m_Root;  
}

#include <iostream>

struct StackElem {
  KDTreeNode* node;  // pointer to far child 
  double t;          // the entry/exit signed distance 
  DOUBLEVECTOR3 pb;          // the coordinates of entry/exit point 
  int prev;          // the pointer to the previous stack item 
};

double KDTree::Intersect(const Ray& ray, FLOATVECTOR3& normal,
                         FLOATVECTOR2& tc, FLOATVECTOR4& color,
                         double tmin, double tmax) const {

  StackElem* stack = new StackElem[m_maxDepth+3];

	// init stack
	int enPt = 0, exPt = 1;

  // init traversal
	KDTreeNode* farchild = 0, *currnode = m_Root;

	stack[enPt].t    = tmin;
  stack[enPt].pb   = (tmin > 0) ? (ray.start + ray.direction * tmin) :ray.start;
  stack[enPt].node = 0;
  stack[enPt].prev = 0;

	stack[exPt].t    = tmax;
	stack[exPt].pb   = ray.start + ray.direction * tmax;
	stack[exPt].node = 0;
  stack[enPt].prev = 0;
	
  // traverse kd-tree
	while (currnode)
	{
		while (!currnode->IsLeaf())
		{
			double splitpos = currnode->GetSplitPos();
			int axis = currnode->GetAxis();
			if (stack[enPt].pb[axis] <= splitpos)
			{
				if (stack[exPt].pb[axis] <= splitpos)
				{
					currnode = currnode->GetLeft();
					continue;
				}
				farchild = currnode->GetRight();
				currnode = currnode->GetLeft();
			}
			else
			{
				if (stack[exPt].pb[axis] > splitpos)
				{
					currnode = currnode->GetRight();
					continue;
				}
				farchild = currnode->GetLeft();
				currnode = currnode->GetRight();
			}

			double t = (splitpos - ray.start[axis]) / ray.direction[axis];
			int tmp = exPt++;
			if (exPt == enPt) exPt++;

			stack[exPt].prev = tmp;
			stack[exPt].t = t;
			stack[exPt].node = farchild;
      stack[exPt].pb = ray.start + ray.direction * t;
    }

    // check leaf cell
    double t = std::numeric_limits<double>::max();
    FLOATVECTOR3 _normal;   FLOATVECTOR2 _tc; FLOATVECTOR4 _color;

    for (size_t i = 0;i<currnode->GetList().size();i++) {
      double currentT = m_mesh->IntersectTriangle(currnode->GetList()[i],
                                                  ray, _normal, _tc, _color);
      if (currentT < t) {
        normal = _normal;
        t = currentT;
        tc = _tc;
        color = _color;
      }
    }
    if (t != std::numeric_limits<double>::max()) {
      delete [] stack;
      return t;
    }

		enPt = exPt;
		currnode = stack[exPt].node;
		exPt = stack[enPt].prev;
	}

  delete [] stack;
	return std::numeric_limits<double>::max();
}

void KDTree::Subdivide(KDTreeNode* node, const DOUBLEVECTOR3& min,
                       const DOUBLEVECTOR3& max, int recDepth) {
	// determine split axis (always split along the longest axis)
	DOUBLEVECTOR3 bboxSize = max-min;

  unsigned char axis = 2;
	if ((bboxSize.x >= bboxSize.y) && (bboxSize.x >= bboxSize.z))
    axis = 0;
	else 
    if ((bboxSize.y >= bboxSize.x) && (bboxSize.y >= bboxSize.z))
      axis = 1;

  node->SetAxis(axis);

	// make a list of the split position candidates
	double pos1 = min[axis];
	double pos2 = max[axis];
  slist splitCandidates;
  splitCandidates.reserve(node->GetList().size());
  eventVec events;
  for (size_t i = 0;i<node->GetList().size();i++) {
    size_t triIndex = node->GetList()[i];
    double vertices[3] = {
      m_mesh->m_Data.m_vertices[m_mesh->m_Data.m_VertIndices[triIndex*3+0]][axis],
      m_mesh->m_Data.m_vertices[m_mesh->m_Data.m_VertIndices[triIndex*3+1]][axis],
      m_mesh->m_Data.m_vertices[m_mesh->m_Data.m_VertIndices[triIndex*3+2]][axis]
    };

    
    double pMin = vertices[0];
    double pMax = vertices[0];
    for (size_t j = 1;j<3;j++) {
      if (vertices[j] < pMin) pMin = vertices[j];
      if (vertices[j] > pMax) pMax = vertices[j];
    }
    
    if ( pMin >= pos1 ) splitCandidates.push_back( pMin );
    if ( pMax <= pos2 ) splitCandidates.push_back( pMax );

    events.push_back(std::make_pair(pMin,pMax));
  }

  // calculate the inverse half surface area for
  // current node, used for normalization
  double halfInverseArea = 1.0/(bboxSize[0]*bboxSize[1] + 
                                bboxSize[0]*bboxSize[2] + 
                                bboxSize[1]*bboxSize[2]);
  
  double minCost = std::numeric_limits<double>::max();
  double bestpos = 0;

  // sort the triangles (events) bythe max and min index and then figure out
  // the number of triangles on the right and on the left by using binary search
  eventVec eventsMax = events;
  sort(events.begin(), events.end(), compMin);
  sort(eventsMax.begin(), eventsMax.end(), compMax);
  for (size_t i = 0;i<splitCandidates.size();i++) {
    splitCandidates[i].n1count = upper_bound(events.begin(), 
                                             events.end(),
                                             dPair(splitCandidates[i].pos, 0),
                                             compMin)-events.begin();
    splitCandidates[i].n2count = eventsMax.size()-
                                   (upper_bound(eventsMax.begin(),
                                                eventsMax.end(),
                                                dPair(0,splitCandidates[i].pos),
                                                compMax)-eventsMax.begin());
  }

  // compute costs for splits
  for (slist::iterator candidate = splitCandidates.begin();
       candidate!=splitCandidates.end(); ++candidate) {
    DOUBLEVECTOR3 b1 = bboxSize;  b1[axis] = candidate->pos - min[axis];
    DOUBLEVECTOR3 b2 = bboxSize;  b2[axis] -= b1[axis];

    // compute the cost for this split
		double halfArea1 = b1.x * b1.y + b1.y * b1.z + b1.x * b1.z;
		double halfArea2 = b2.x * b2.y + b2.y * b2.z + b2.x * b2.z;

    // the 0.3 is some wild guess for the tree 
    // (travesal/triangle intersect)-cost
		double splitcost = 0.3 + halfInverseArea * 
      (halfArea1 * candidate->n1count + halfArea2 * candidate->n2count);

		// update best cost tracking variables
		if (minCost > splitcost) {
      minCost = splitcost;
      bestpos = candidate->pos;
    }
  }
	// calculate cost for not splitting
	double noSplitCost = double(node->GetList().size());
  // if splitting makes things worse -> stop
  if (minCost > noSplitCost) return;

  // split
  node->SetLeaf(false);
  KDTreeNode* left  = new KDTreeNode;
  KDTreeNode* right = new KDTreeNode;

  // push objects into children
  for (size_t i = 0;i<node->GetList().size();i++) {
    size_t triIndex = node->GetList()[i];
    double vertices[3] = {
      m_mesh->m_Data.m_vertices[m_mesh->m_Data.m_VertIndices[triIndex*3+0]][axis],
      m_mesh->m_Data.m_vertices[m_mesh->m_Data.m_VertIndices[triIndex*3+1]][axis],
      m_mesh->m_Data.m_vertices[m_mesh->m_Data.m_VertIndices[triIndex*3+2]][axis]
    };

    if ( vertices[0] <= bestpos ||
         vertices[1] <= bestpos || 
         vertices[2] <= bestpos) 
      left->Add(triIndex);
    if ( vertices[0] > bestpos || 
         vertices[1] > bestpos || 
         vertices[2] > bestpos) 
      right->Add(triIndex);
  }
  
  node->SetLeft(left);
  node->SetRight(right);
  node->SetAxis(axis);
  node->SetSplitPos(bestpos);


	if (recDepth > 1)
	{
    DOUBLEVECTOR3 max1 = max; max1[axis] = bestpos;
		if (left->GetList().size() > 2) Subdivide( left,  min,  max1, recDepth-1);
    DOUBLEVECTOR3 min2 = min; min2[axis] = bestpos;
		if (right->GetList().size() > 2) Subdivide( right, min2, max,  recDepth-1);
	}
}

Mesh* KDTree::GetGeometry(unsigned int iDepth, bool buildKDTree) const {
    VertVec       vertices;
    NormVec       normals;
    TexCoordVec   texcoords;
    ColorVec      colors;

    IndexVec      vIndices;
    IndexVec      nIndices;
    IndexVec      tIndices;
    IndexVec      cIndices;

    // as the GetGeometry call does not create colors or texture coords
    // we do not pass these two to this call but since the constructor
    // requires them to be given we pass the empty vectors to it
    m_Root->GetGeometry(vertices, normals,  
                        vIndices, nIndices,
                        m_mesh->m_Bounds[0], m_mesh->m_Bounds[1], iDepth);

    return new Mesh(vertices, normals, texcoords, colors, 
                    vIndices, nIndices, tIndices, cIndices,
                    buildKDTree,false,"KD-Tree Mesh", Mesh::MT_TRIANGLES);
}

void KDTree::RescaleAndShift(KDTreeNode* node, 
                             const FLOATVECTOR3& translation,
                             const FLOATVECTOR3& scale) {

  unsigned char axis = node->GetAxis();
	double pos = node->GetSplitPos();
  pos = pos * scale[axis] + translation[axis];
  node->SetSplitPos(pos);

  if (!node->IsLeaf()) {
    RescaleAndShift(node->GetLeft(), translation, scale);
    RescaleAndShift(node->GetRight(), translation, scale);
  }
}
