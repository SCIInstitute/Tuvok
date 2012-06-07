/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2008 Scientific Computing and Imaging Institute,
   University of Utah.


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
/**
  \file    Dataset.cpp
  \author  Tom Fogal
           SCI Institute
           University of Utah
*/
#include "Dataset.h"
#include "Basics/MathTools.h"
#include "Basics/Mesh.h"

namespace tuvok {

Dataset::Dataset():
  m_pHist1D(NULL),
  m_pHist2D(NULL),
  m_UserScale(1.0,1.0,1.0),
  m_DomainScale(1.0, 1.0, 1.0)
{
  m_DomainScale = DOUBLEVECTOR3(1.0, 1.0, 1.0);
}

Dataset::~Dataset()
{
  delete m_pHist1D;  m_pHist1D = NULL;
  delete m_pHist2D;  m_pHist2D = NULL;

  DeleteMeshes();
}

void Dataset::DeleteMeshes() {
  for (std::vector<Mesh*>::iterator i = m_vpMeshList.begin();
       i != m_vpMeshList.end();
       i++) {
    delete (*i);
  }
  m_vpMeshList.clear();
}

void Dataset::SetRescaleFactors(const DOUBLEVECTOR3& rescale) {
  m_UserScale = rescale;
}

DOUBLEVECTOR3 Dataset::GetRescaleFactors() const {
  return m_UserScale;
}

std::pair<FLOATVECTOR3, FLOATVECTOR3> Dataset::GetTextCoords(BrickTable::const_iterator brick, bool bUseOnlyPowerOfTwo) const {

  FLOATVECTOR3 vTexcoordsMin;
  FLOATVECTOR3 vTexcoordsMax;

  UINTVECTOR3 vOverlap = GetBrickOverlapSize();
  bool first_x = BrickIsFirstInDimension(0, brick->first);
  bool first_y = BrickIsFirstInDimension(1, brick->first);
  bool first_z = BrickIsFirstInDimension(2, brick->first);
  bool last_x = BrickIsLastInDimension(0, brick->first);
  bool last_y = BrickIsLastInDimension(1, brick->first);
  bool last_z = BrickIsLastInDimension(2, brick->first);

  if (bUseOnlyPowerOfTwo) {
    UINTVECTOR3 vRealVoxelCount(MathTools::NextPow2(brick->second.n_voxels.x),
                                MathTools::NextPow2(brick->second.n_voxels.y),
                                MathTools::NextPow2(brick->second.n_voxels.z)
                                );
    vTexcoordsMin = FLOATVECTOR3(
      (first_x) ? 0.5f/vRealVoxelCount.x : vOverlap.x*0.5f/vRealVoxelCount.x,
      (first_y) ? 0.5f/vRealVoxelCount.y : vOverlap.y*0.5f/vRealVoxelCount.y,
      (first_z) ? 0.5f/vRealVoxelCount.z : vOverlap.z*0.5f/vRealVoxelCount.z
      );
    vTexcoordsMax = FLOATVECTOR3(
      (last_x) ? 1.0f-0.5f/vRealVoxelCount.x : 1.0f-vOverlap.x*0.5f/vRealVoxelCount.x,
      (last_y) ? 1.0f-0.5f/vRealVoxelCount.y : 1.0f-vOverlap.y*0.5f/vRealVoxelCount.y,
      (last_z) ? 1.0f-0.5f/vRealVoxelCount.z : 1.0f-vOverlap.z*0.5f/vRealVoxelCount.z
      );

    vTexcoordsMax -= FLOATVECTOR3(vRealVoxelCount - brick->second.n_voxels) /
                                    FLOATVECTOR3(vRealVoxelCount);
  } else {

    vTexcoordsMin = FLOATVECTOR3(
      (first_x) ? 0.5f/brick->second.n_voxels.x : vOverlap.x*0.5f/brick->second.n_voxels.x,
      (first_y) ? 0.5f/brick->second.n_voxels.y : vOverlap.y*0.5f/brick->second.n_voxels.y,
      (first_z) ? 0.5f/brick->second.n_voxels.z : vOverlap.z*0.5f/brick->second.n_voxels.z
      );
    // for padded volume adjust texcoords
    vTexcoordsMax = FLOATVECTOR3(
      (last_x) ? 1.0f-0.5f/brick->second.n_voxels.x : 1.0f-vOverlap.x*0.5f/brick->second.n_voxels.x,
      (last_y) ? 1.0f-0.5f/brick->second.n_voxels.y : 1.0f-vOverlap.y*0.5f/brick->second.n_voxels.y,
      (last_z) ? 1.0f-0.5f/brick->second.n_voxels.z : 1.0f-vOverlap.z*0.5f/brick->second.n_voxels.z
      );
  }

  return std::make_pair(vTexcoordsMin, vTexcoordsMax);
}


}; // tuvok namespace.
