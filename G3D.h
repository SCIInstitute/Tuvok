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

//!    File   : G3D.h
//!    Author : Georg Tamm
//!             DFKI, Saarbruecken
//!    Date   : July 2010
//
//!    Copyright (C) 2010 DFKI

#pragma once

#ifndef G3D_H
#define G3D_H

#include "../StdTuvokDefines.h"
#include <iostream>
#include <fstream>
#include <vector>

class G3D
{
public:

	enum AttributeSemantic
	{
		Position,
		Normal,
		Tangent,
		Color,
		Tex,
		Float
	};

	enum PrimitiveType
	{
		Point,
		Line,
		Triangle,
		TriangleAdj
	};

	enum VertexType
	{
		SoA,
		AoS
	};

	struct GeometryInfo
	{
		GeometryInfo() : 
      vertexType(AoS), numberPrimitives(0), primitiveType(Triangle), 
      numberIndices(0), numberVertices(0), vertexSize(0), indexSize(0),
      isOpaque(true) {}

		VertexType vertexType;
		UINT32 numberPrimitives;
		PrimitiveType primitiveType;
		UINT32 numberIndices;
		UINT32 numberVertices;
		UINT32 vertexSize;
		UINT32 indexSize;
		bool isOpaque;
		
		std::vector<AttributeSemantic> attributeSemantics;
	};

	struct Geometry
	{
		Geometry() : indices(NULL) {}
		GeometryInfo info;

		UINT32 * indices;
	};

	struct GeometryAoS : Geometry
	{
		GeometryAoS() : Geometry(), vertices(NULL)
		{
			info.vertexType = AoS;
		}

		float * vertices;
	};

	struct GeometrySoA : Geometry
	{
		GeometrySoA() : Geometry()
		{
			info.vertexType = SoA;
		}
		
		std::vector<float*> vertexAttributes;
	};

	static UINT32 floats(const AttributeSemantic semantic)
	{
		switch (semantic)
		{
			case Position:
				return 3;
			case Normal:
				return 3;
			case Tangent:
				return 3;
			case Color:
				return 4;
			case Tex:
				return 2;
			case Float:
				return 1;
			default:
				return 0;
		}
	}

	static void write(const std::string & file,
                    const GeometryAoS * const geometry, 
                    const VertexType vertexType = AoS);
	static void write(const std::string & file, 
                    const GeometrySoA * const geometry, 
                    const VertexType vertexType = SoA);
	static void read(const std::string & file, GeometryAoS * const geometry);
	static void read(const std::string & file, GeometrySoA * const geometry);
	static void print(const Geometry * const geometry, 
                    std::ostream& output = std::cout);
	static void clean(GeometryAoS * geometry);
	static void clean(GeometrySoA * geometry);

private:
	static void writeHeader(std::fstream & fs, const GeometryInfo & info,
                          const VertexType * const vertexType = NULL);
	static void writeIndices(std::fstream & fs, const UINT32 * const indices,
                          const GeometryInfo & info);
	static void writeVertices(std::fstream & fs, const float * const vertices,
                            const GeometryInfo & info);
	static void writeVertices(std::fstream & fs, 
                            const std::vector<float*> & vertexAttributes, 
                            const GeometryInfo & info);
	static void writeContent(std::fstream & fs, const GeometryAoS & geometry);
	static void writeContent(std::fstream & fs, const GeometrySoA & geometry);

	static void readHeader(std::fstream & fs, GeometryInfo & info);
	static void readIndices(std::fstream & fs, UINT32 *& indices, 
                          const GeometryInfo & info);
	static void readVertices(std::fstream & fs, float *& vertices, 
                           const GeometryInfo & info);
	static void readVertices(std::fstream & fs, 
                           std::vector<float*> & vertexAttributes, 
                           const GeometryInfo & info);
	static void readContent(std::fstream & fs, GeometryAoS & geometry);
	static void readContent(std::fstream & fs, GeometrySoA & geometry);

	static void convertVertices(const std::vector<float*> & vertexAttributes,
                              float *& vertices, const GeometryInfo & info);
	static void convertVertices(const float * const vertices, 
                              std::vector<float*> & vertexAttributes,
                              const GeometryInfo & info);

	static void cleanIndices(UINT32 * indices);
	static void cleanVertices(float * vertices);
	static void cleanVertices(std::vector<float*> & vertexAttributes);
};

#endif
