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

//!    File   : G3D.cpp
//!    Author : Georg Tamm
//!             DFKI, Saarbruecken
//!    Date   : July 2010
//
//!    Copyright (C) 2010 DFKI Institute

#include "G3D.h"

#include "G3D.h"

void G3D::writeHeader(std::fstream & fs, const GeometryInfo & info, const UINT32 * const vertexType)
{
	fs.write((char*)&info.isOpaque, sizeof(bool));
	fs.write((char*)&info.numberPrimitives, sizeof(UINT32));
	fs.write((char*)&info.primitiveType, sizeof(UINT32));

	UINT32 numberSemantics = (UINT32)info.attributeSemantics.size();
	fs.write((char*)&numberSemantics, sizeof(UINT32));

	fs.write((char*)&info.numberIndices, sizeof(UINT32));
	fs.write((char*)&info.indexSize, sizeof(UINT32));
	fs.write((char*)&info.numberVertices, sizeof(UINT32));
	fs.write((char*)&info.vertexSize, sizeof(UINT32));

	fs.write((char*)(vertexType ? vertexType : &info.vertexType), sizeof(UINT32));
		
	fs.write((char*)&(info.attributeSemantics.at(0)), sizeof(UINT32) * numberSemantics);
}

void G3D::writeIndices(std::fstream & fs, const UINT32 * const indices, const GeometryInfo & info)
{
	fs.write((char*)indices, info.numberIndices * info.indexSize);
}

void G3D::writeVertices(std::fstream & fs, const float * const vertices, const GeometryInfo & info)
{
	fs.write((char*)vertices, info.numberVertices * info.vertexSize);
}

void G3D::writeContent(std::fstream & fs, const GeometryAoS & geometry)
{
	writeIndices(fs, geometry.indices, geometry.info);
	writeVertices(fs, geometry.vertices, geometry.info);
}

void G3D::write(const std::string & file, const GeometryAoS * const geometry, const UINT32 vertexType)
{
	std::fstream fs;
	fs.open(file.c_str(), std::fstream::out | std::fstream::binary | std::fstream::trunc);

	if (fs.is_open())
	{
		if (vertexType == AoS) 
		{
			writeHeader(fs, geometry->info);
			writeContent(fs, *geometry);
		}
		else if (vertexType == SoA)
		{
			writeHeader(fs, geometry->info, &vertexType);
			writeIndices(fs, geometry->indices, geometry->info);
			std::vector<float*> vertexAttributes;
			convertVertices(geometry->vertices, vertexAttributes, geometry->info);
			writeVertices(fs, vertexAttributes, geometry->info);
			cleanVertices(vertexAttributes);
		}
		fs.close();
	}
}

void G3D::writeVertices(std::fstream & fs, const std::vector<float*> & vertexAttributes, const GeometryInfo & info)
{
	UINT32 i = 0;
	for (std::vector<UINT32>::const_iterator it=info.attributeSemantics.begin(); it!=info.attributeSemantics.end(); ++it) 
	{
		fs.write((char*)vertexAttributes.at(i), info.numberVertices * floats(*it) * sizeof(float));
		++i;
	}
}

void G3D::writeContent(std::fstream & fs, const GeometrySoA & geometry)
{
	writeIndices(fs, geometry.indices, geometry.info);
	writeVertices(fs, geometry.vertexAttributes, geometry.info);
}

void G3D::write(const std::string & file, const GeometrySoA * const geometry, const UINT32 vertexType)
{
	std::fstream fs;
	fs.open(file.c_str(), std::fstream::out | std::fstream::binary | std::fstream::trunc);

	if (fs.is_open())
	{
		if (vertexType == SoA) 
		{
			writeHeader(fs, geometry->info);
			writeContent(fs, *geometry);
		}
		else if (vertexType == AoS)
		{
			writeHeader(fs, geometry->info, &vertexType);
			writeIndices(fs, geometry->indices, geometry->info);
			float * vertices = NULL;
			convertVertices(geometry->vertexAttributes, vertices, geometry->info);
			writeVertices(fs, vertices, geometry->info);
			cleanVertices(vertices);
		}
		fs.close();
	}
}

void G3D::readHeader(std::fstream & fs, GeometryInfo & info)
{
	char * buffer = new char[8 * sizeof(UINT32) + sizeof(bool)];
	fs.read(buffer, 8 * sizeof(UINT32) + sizeof(bool));
	info.isOpaque = ((buffer++)[0] == 1);
	info.numberPrimitives = ((UINT32*)buffer)[0];
	info.primitiveType = ((UINT32*)buffer)[1];
	UINT32 numberSemantics = ((UINT32*)buffer)[2];
	info.numberIndices = ((UINT32*)buffer)[3];
	info.indexSize = ((UINT32*)buffer)[4];
	info.numberVertices = ((UINT32*)buffer)[5];
	info.vertexSize = ((UINT32*)buffer)[6];
	info.vertexType = ((UINT32*)buffer)[7];
	delete [] --buffer;

	buffer = new char[numberSemantics * sizeof(UINT32)];
	fs.read(buffer, numberSemantics * sizeof(UINT32));
	for (UINT32 i=0; i<numberSemantics; ++i) 
	{
		info.attributeSemantics.push_back(((UINT32*)buffer)[i]);
	}
	delete [] buffer;
}

void G3D::readIndices(std::fstream & fs, UINT32 *& indices, const GeometryInfo & info)
{
	indices = (UINT32*)new char[info.numberIndices * info.indexSize];
	fs.read((char*)indices, info.numberIndices * info.indexSize);
}

void G3D::readVertices(std::fstream & fs, float *& vertices, const GeometryInfo & info)
{
	vertices = (float*)new char[info.numberVertices * info.vertexSize];
	fs.read((char*)vertices, info.numberVertices * info.vertexSize);
}

void G3D::readContent(std::fstream & fs, GeometryAoS & geometry)
{
	readIndices(fs, geometry.indices, geometry.info);
	readVertices(fs, geometry.vertices, geometry.info);
}

void G3D::convertVertices(const std::vector<float*> & vertexAttributes, float *& vertices, const GeometryInfo & info)
{
	vertices = (float*)new char[info.numberVertices * info.vertexSize];

	UINT32 vertexFloats = info.vertexSize / sizeof(float);
	for (UINT32 i=0; i<info.numberVertices; ++i)
	{
		UINT32 offset = 0;
		UINT32 attributeIndex = 0;
		for (std::vector<UINT32>::const_iterator it=info.attributeSemantics.begin(); it!=info.attributeSemantics.end(); ++it)
		{
			UINT32 attributeFloats = floats(*it);
			for (UINT32 j=0; j<attributeFloats; ++j) vertices[j + offset + (i * vertexFloats)] = vertexAttributes[attributeIndex][j + (i * attributeFloats)];
			offset += attributeFloats;
			++attributeIndex;
		}
	}
}

void G3D::read(const std::string & file, GeometryAoS * const geometry)
{
	std::fstream fs;
	fs.open(file.c_str(), std::fstream::in | std::fstream::binary);

	if (fs.is_open())
	{
		readHeader(fs, geometry->info);
		if (geometry->info.vertexType == AoS) readContent(fs, *geometry);
		else if (geometry->info.vertexType == SoA)
		{
			geometry->info.vertexType = AoS;
			readIndices(fs, geometry->indices, geometry->info);
			std::vector<float*> vertexAttributes;
			readVertices(fs, vertexAttributes, geometry->info);
			convertVertices(vertexAttributes, geometry->vertices, geometry->info);
			cleanVertices(vertexAttributes);
		}
		fs.close();
	}
}

void G3D::readVertices(std::fstream & fs, std::vector<float*> & vertexAttributes, const GeometryInfo & info)
{
	for (UINT32 i=0; i<info.attributeSemantics.size(); ++i) 
	{
		vertexAttributes.push_back(NULL);
		UINT32 attributeFloats = floats(info.attributeSemantics.at(i));
		vertexAttributes.at(i) = new float[info.numberVertices * attributeFloats];
		fs.read((char*)vertexAttributes.at(i), info.numberVertices * attributeFloats * sizeof(float));
	}
}

void G3D::readContent(std::fstream & fs, GeometrySoA & geometry)
{
	readIndices(fs, geometry.indices, geometry.info);
	readVertices(fs, geometry.vertexAttributes, geometry.info);
}

void G3D::convertVertices(const float * const vertices, std::vector<float*> & vertexAttributes, const GeometryInfo & info)
{
	UINT32 i = 0;
	for (std::vector<UINT32>::const_iterator it=info.attributeSemantics.begin(); it!=info.attributeSemantics.end(); ++it)
	{
		vertexAttributes.push_back(NULL);
		UINT32 attributeFloats = floats(*it);
		vertexAttributes.at(i) = new float[info.numberVertices * attributeFloats];
		++i;
	}

	UINT32 vertexFloats = info.vertexSize / sizeof(float);
	for (UINT32 i=0; i<info.numberVertices; ++i)
	{
		UINT32 offset = 0;
		UINT32 attributeIndex = 0;
		for (std::vector<UINT32>::const_iterator it=info.attributeSemantics.begin(); it!=info.attributeSemantics.end(); ++it)
		{
			UINT32 attributeFloats = floats(*it);
			for (UINT32 j=0; j<attributeFloats; ++j) vertexAttributes[attributeIndex][j + (i * attributeFloats)] = vertices[j + offset + (i * vertexFloats)];
			offset += attributeFloats;
			++attributeIndex;
		}
	}
}

void G3D::read(const std::string & file, GeometrySoA * const geometry)
{
	std::fstream fs;
	fs.open(file.c_str(), std::fstream::in | std::fstream::binary);

	if (fs.is_open())
	{
		readHeader(fs, geometry->info);
		if (geometry->info.vertexType == SoA) readContent(fs, *geometry);
		else if (geometry->info.vertexType == AoS)
		{
			geometry->info.vertexType = SoA;
			readIndices(fs, geometry->indices, geometry->info);
			float * vertices = NULL;
			readVertices(fs, vertices, geometry->info);
			convertVertices(vertices, geometry->vertexAttributes, geometry->info);
			cleanVertices(vertices);
		}
		fs.close();
	}
}

void G3D::cleanIndices(UINT32 * indices)
{
	delete [] indices;
	indices = NULL;
}

void G3D::cleanVertices(float * vertices)
{
	delete [] vertices;
	vertices = NULL;
}

void G3D::cleanVertices(std::vector<float*> & vertexAttributes)
{
	for (std::vector<float*>::iterator it=vertexAttributes.begin(); it!=vertexAttributes.end(); ++it) delete [] *it;
	vertexAttributes.clear();
}

void G3D::clean(GeometryAoS * geometry)
{
	cleanIndices(geometry->indices);
	cleanVertices(geometry->vertices);
	geometry->info.attributeSemantics.clear();
	geometry = NULL;
}

void G3D::clean(GeometrySoA * geometry)
{
	cleanIndices(geometry->indices);
	cleanVertices(geometry->vertexAttributes);
	geometry->info.attributeSemantics.clear();
	geometry = NULL;
}

void G3D::print(const Geometry * const geometry, std::ostream & output)
{
	if (geometry)
	{
		output << "Opaque: " << (geometry->info.isOpaque ? "yes" : "no") << std::endl;
		output << "Number primitives: " << geometry->info.numberPrimitives << std::endl;
		output << "Primitive type: " << ((geometry->info.primitiveType == Point) ? "Point" : 
				(geometry->info.primitiveType == Line) ? "Line" :
				(geometry->info.primitiveType == Triangle) ? "Triangle" :
				(geometry->info.primitiveType == TriangleAdj) ? "Triangle with adjacency" : "Unknown") << std::endl;
		output << "Number indices: " << geometry->info.numberIndices << std::endl;
		output << "Index size: " << geometry->info.indexSize << std::endl;
		output << "Number vertices: " << geometry->info.numberVertices << std::endl;
		output << "Vertex size: " << geometry->info.vertexSize << std::endl;
		output << "Vertex type: " << ((geometry->info.vertexType == AoS) ? "Array of Structs" : 
				(geometry->info.vertexType == SoA) ? "Struct of Arrays" : "Unknown") << std::endl;
		output << "Vertex attribute semantics:" << std::endl;
		for (std::vector<UINT32>::const_iterator it=geometry->info.attributeSemantics.begin(); it!=geometry->info.attributeSemantics.end(); ++it)
		{
			output << "\t" << (((*it) == Position) ? "Position" : 
				((*it) == Normal) ? "Normal" :
				((*it) == Tangent) ? "Tangent" :
				((*it) == Color) ? "Color" :
				((*it) == Tex) ? "Tex" :
				((*it) == Float) ? "Float" : "Unknown") << " (" << floats(*it) << "f)" << std::endl;
		}
	}
}
