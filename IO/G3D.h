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

#include "StdTuvokDefines.h"
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
		GeometryInfo() : vertexType(AoS), numberPrimitives(0), primitiveType(Triangle), numberIndices(0), numberVertices(0), vertexSize(0), indexSize(0), isOpaque(true) {}
		uint32_t vertexType;
		uint32_t numberPrimitives;
		uint32_t primitiveType;
		uint32_t numberIndices;
		uint32_t numberVertices;
		uint32_t vertexSize;
		uint32_t indexSize;
		bool isOpaque;
		
		std::vector<uint32_t> attributeSemantics;
	};

	struct Geometry
	{
		Geometry() : indices(NULL) {}
		GeometryInfo info;

		uint32_t * indices;
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

	static uint32_t floats(uint32_t semantic)
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

	static void write(const std::string & file, const GeometryAoS * const geometry, const uint32_t vertexType = AoS);
	static void write(const std::string & file, const GeometrySoA * const geometry, const uint32_t vertexType = SoA);
	static void read(const std::string & file, GeometryAoS * const geometry);
	static void read(const std::string & file, GeometrySoA * const geometry);
	static void print(const Geometry * const geometry, std::ostream & output);
	static void clean(GeometryAoS * geometry);
	static void clean(GeometrySoA * geometry);
	static bool merge(GeometrySoA * a, const GeometrySoA * const b); // merge a and b into a

private:
	static void writeHeader(std::fstream & fs, const GeometryInfo & info, const uint32_t * const vertexType = NULL);
	static void writeIndices(std::fstream & fs, const uint32_t * const indices, const GeometryInfo & info);
	static void writeVertices(std::fstream & fs, const float * const vertices, const GeometryInfo & info);
	static void writeVertices(std::fstream & fs, const std::vector<float*> & vertexAttributes, const GeometryInfo & info);
	static void writeContent(std::fstream & fs, const GeometryAoS & geometry);
	static void writeContent(std::fstream & fs, const GeometrySoA & geometry);

	static void readHeader(std::fstream & fs, GeometryInfo & info);
	static void readIndices(std::fstream & fs, uint32_t *& indices, const GeometryInfo & info);
	static void readVertices(std::fstream & fs, float *& vertices, const GeometryInfo & info);
	static void readVertices(std::fstream & fs, std::vector<float*> & vertexAttributes, const GeometryInfo & info);
	static void readContent(std::fstream & fs, GeometryAoS & geometry);
	static void readContent(std::fstream & fs, GeometrySoA & geometry);

	static void convertVertices(const std::vector<float*> & vertexAttributes, float *& vertices, const GeometryInfo & info);
	static void convertVertices(const float * const vertices, std::vector<float*> & vertexAttributes, const GeometryInfo & info);

	static void cleanIndices(uint32_t * indices);
	static void cleanVertices(float * vertices);
	static void cleanVertices(std::vector<float*> & vertexAttributes);
};

#endif
