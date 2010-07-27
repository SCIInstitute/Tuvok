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
  \file    SBVRGeogen.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    September 2008
*/
#pragma once

#ifndef SBVRGEOGEN_H
#define SBVRGEOGEN_H

#include <vector>
#include "../Basics/Vectors.h"
#include "../StdTuvokDefines.h"
#include "RenderMesh.h"

namespace tuvok {
  /**
   \class VERTEX_FORMAT
   \brief simple aggregate class to hold 3D position and 
   4D vertex info (either used as 3D texccords or 4d color)
  */
  #define AS_TEXCOORD 2
  class VERTEX_FORMAT
  {
  public:
    /**
      \brief standard constructor
      initializes both the position as well as the texture coodinate to (0,0,0,0)
    */
    VERTEX_FORMAT() : m_vPos(), m_vVertexData(), m_fOpacity(AS_TEXCOORD) {}
    /**    
      \brief initializes both the position as well as the texture coodinate to the
      given parameters
    */
    VERTEX_FORMAT(const FLOATVECTOR3 &vPos, const FLOATVECTOR3 &vTex)
      : m_vPos(vPos),
        m_vVertexData(vTex),
        m_fOpacity(AS_TEXCOORD) {}
    /**    
      \brief initializes the position to the x,y,z coordinate of the give position 
      (IGNORING the w component) and the texture coodinate to the
      given parameter
    */
    VERTEX_FORMAT(const FLOATVECTOR4 &vPos, const FLOATVECTOR3 &vTex)
      : m_vPos(vPos.xyz()),
        m_vVertexData(vTex),
        m_fOpacity(AS_TEXCOORD) {}
    /**    
      \brief initializes the position to give parameter and the texture coordinate
      to the position + 0.5
      
      this is useful for the realtively common case that the normlized volume
      is centered in the origin
    */
    VERTEX_FORMAT(const FLOATVECTOR3 &vPos) : 
      m_vPos(vPos),
      m_fOpacity(AS_TEXCOORD)
    {
      m_vVertexData = m_vPos + 0.5f;
    }
    //! the 3D position
    FLOATVECTOR3 m_vPos;
    //! the 3D texture coordinate or a RBG channels of a color
    FLOATVECTOR3 m_vVertexData;
    //! if the vertex is a color vertex then this holds the opacity otherwise
    //  it stores AS_TEXCOORD 
    float m_fOpacity;
    FLOATVECTOR3 m_vNormal;
  };

  /** \class SBVRGeoGen
   * Geometry generation for the slice-based volume renderer. */

  /** 
   \class SBVRGeoGen
   \brief Abstract base class for the geometry generation for 
   the slice-based volume renderer.
   
   This class implements all the accessor code and the required member variables
   for the geometry generation. It does not implement the most important call
   ComputeGeometry() but defines it as pure virtual call.
   */
  class SBVRGeogen
  {
  public:
    /** 
     \brief The Standard and also the only constructor  
     
     SBVRGeogen takes no parameters in the constructor as the information 
     required to compute the geometry mode is supplied later via accessor calls
    */
    SBVRGeogen(void);
    virtual ~SBVRGeogen(void);

    /** 
     \brief Modifies the default sampling rate
     
     \param fSamplingModifier user specified oversampling (if > 1) or 
     undersampling (if < 1) rate
    */
    void SetSamplingModifier(float fSamplingModifier) {
      m_fSamplingModifier = fSamplingModifier;
    }

    /** 
     \brief Sets the view matrix
     
     \param matView the new view matrix
    */
    void SetView(const FLOATMATRIX4 &matView);
    /** 
     \brief Sets the world matrix (or model matrix) without brick translation
     
     \param matWorld the new world matrix
    */
    void SetWorld(const FLOATMATRIX4& matWorld);
    /** 
     \brief Sets translation of the brick within the array of volume bricks
     
     \param brickTranslation the translation of the brick
     */
    void SetBrickTrans(const FLOATVECTOR3& brickTranslation);
    /** 
     \brief Sets the global volume parameters
     
     \param vAspect the global aspect ratio of the entire volume 
     (not just the current brick)
     \param vSize the global size of the highest resolution of the entire volume
     (not just the current brick)
    */
    void SetVolumeData(const FLOATVECTOR3& vAspect, const UINTVECTOR3& vSize);

    /** 
     \brief Sets the global LOD information
     
     \param vSize the global size of the current LOD of the entire volume
     (not just the current brick)
    */
    void SetLODData(const UINTVECTOR3& vSize);

    /** 
     \brief Sets the local brick parameters
     
     \param vAspect the aspect ratio of the current brick
     \param vSize the voxel size of the current brick
     \param vTexCoordMin minimum texture coordinates
     usualy this is not (0,0,0) due to brick overlaps
     \param vTexCoordMax maximum texture coordinates
     usualy this is not (1,1,1) due to brick overlaps
    */
    void SetBrickData(const FLOATVECTOR3& vAspect, const UINTVECTOR3& vSize,
                      const FLOATVECTOR3& vTexCoordMin=FLOATVECTOR3(0,0,0),
                      const FLOATVECTOR3& vTexCoordMax=FLOATVECTOR3(1,1,1));

    /** 
     \brief abstract call the is supposed to do the actual 
     geometry computation in the derived classes
    */
    virtual void ComputeGeometry(bool bMeshOnly) = 0;

    /** 
     \brief returns the opacity correction parameter based on the global
     volume paramerts set by SetVolumeData() and SetLODData()
    */
    float GetOpacityCorrection() const;

    //! enables the clip plane, does not recompute the geometry
    void DisableClipPlane() { m_bClipPlaneEnabled = false; }
    
    //! enables the clip plane, does not recompute the geometry
    void EnableClipPlane() { m_bClipPlaneEnabled = true; }
    
    /**
      \brief specifies the clipping plane
      
      recompute the geometry if the plane is differnt from the previously stored
      clip plane

      \param plane the new clipping plane
    */
    void SetClipPlane(const PLANE<float>& plane) {
      if(m_ClipPlane != plane) {
        m_ClipPlane = plane;
      }
    }

    virtual bool HasMesh() const {return m_mesh.size() != 0;}
    void ResetMesh() {m_mesh.clear();}
    void AddMesh(const SortIndexPList& mesh);

  protected:
    //! user specified oversampling (if > 1) or undersampling (if < 1) rate
    float             m_fSamplingModifier;
    //! the world matrix a.k.a. model matrix
    FLOATMATRIX4      m_matWorld;
    //! the view matrix
    FLOATMATRIX4      m_matView;
    //! caches m_matWorld * m_matView, updated via MatricesUpdated()
    FLOATMATRIX4      m_matWorldView;
    //! the translation of the brick
    FLOATVECTOR3      m_brickTranslation;

    /** 
      \brief size in voxels of the entire full-res dataset 
      (not just the current LOD)

      this parameter is used to compute the opacity correction
    */
    UINTVECTOR3       m_vGlobalSize;
    //! aspect ratio of the entire dataset (not just the current brick)
    FLOATVECTOR3      m_vGlobalAspect;
    /** 
      \brief aspect ratio of the entire dataset of the current 
      LOD (not just the current brick)

      this parameter is used to compute the opacity correction
    */
    UINTVECTOR3       m_vLODSize;

    //! the positions of the transformed vertices of the brick bounding box
    VERTEX_FORMAT   m_pfBBOXVertex[8];
    //! the positions of the untransformed vertices of the brick bounding box
    FLOATVECTOR3      m_pfBBOXStaticVertex[8];
    //! the aspect ratio of this brick
    FLOATVECTOR3      m_vAspect;
    //! the size in voxels of the current brick
    UINTVECTOR3       m_vSize;
    /** 
      \brief minimum texture coordinates

      usualy, this is not (0,0,0) due to brick overlaps
    */
    FLOATVECTOR3      m_vTexCoordMin;
    /** 
      \brief maximum texture coordinates

      usualy, this is not (1,1,1) due to brick overlaps
    */
    FLOATVECTOR3      m_vTexCoordMax;

    //! a user defined clip plane
    PLANE<float>      m_ClipPlane;
    //! specifies if the clip plane above is actually used
    bool              m_bClipPlaneEnabled;

    //! part of a transparent mesh to be interleaved with the volume
    SortIndexPList m_mesh;

    //! Computes the transformed vertices m_pfBBOXVertex from m_pfBBOXStaticVertex
    virtual void InitBBOX();
    
    //! Updates the internal matrix states and recomputes the geometry
    void MatricesUpdated();

    /** 
      \brief static method that splits a single
      triangle a,b,c at the plane (normal,D)

      \param a the first vertex of the source triangle
      \param b the second vertex of the source triangle
      \param c the third vertex of the source triangle
      \param normal normal of the splitting plane
      \param D distance of the splitting plane to the origin
      \result the vertices of the split triangles
    */
    static std::vector<VERTEX_FORMAT> SplitTriangle(VERTEX_FORMAT a,
                                                    VERTEX_FORMAT b,
                                                    VERTEX_FORMAT c,
                                                    const VECTOR3<float> &normal,
                                                    const float D);
    /** 
      \brief static method that clips a number of triangles 
      at the plane (normal,D)

      \param in the vertex list of the triangles, every three vertices are 
      interpreted as one triable
      \param normal normal of the splitting plane
      \param D distance of the splitting plane to the origin
      \result the vertices of the split triangles
    */
    static std::vector<VERTEX_FORMAT>
    ClipTriangles(const std::vector<VERTEX_FORMAT> &in,
                  const VECTOR3<float> &normal, const float D);

    /** 
      \brief static method that checks if a ray intersects a plane

      \param la start point of the ray
      \param lb end point of the ray
      \param normal normal of the plane
      \param D distance of the splitting plane to the origin
      \param hit output parameter that holds the intersection point, undfined if
      no intersection is found
      \result true iff the ray intersects the plane
    */
    static bool RayPlaneIntersection(const VERTEX_FORMAT &la,
                             const VERTEX_FORMAT &lb,
                             const FLOATVECTOR3 &n, const float D,
                             VERTEX_FORMAT &hit);

    /** 
      \brief Tests if a point is inside a AABB given min & max

      \param min the min coordinates of the AABB
      \param max the max coordinates of the AABB
      \param point the point to be tested
      \result true iff the point is inside this volume
    */
    static bool isInsideAABB(const FLOATVECTOR3& min,
                             const FLOATVECTOR3& max,
                             const FLOATVECTOR3& point);

    void SortMeshWithoutVolume(std::vector<VERTEX_FORMAT>& list);
  };
};
#endif // SBVRGEOGEN_H
