#include <cinttypes>
#include <openvdb/version.h>
#include "Controller/Controller.h"
#include "VDBDataset.h"

namespace tuvok {

VDBDataset::VDBDataset(const std::string& fname) : filename(fname) {
}
VDBDataset::~VDBDataset() {
}

float
VDBDataset::MaxGradientMagnitude() const {
	return 42.424242f;
}

// Clear any caches or brick metadata
void
VDBDataset::Clear() {
}

/// Gets the number of voxels, per dimension.
UINTVECTOR3
VDBDataset::GetBrickVoxelCounts(const BrickKey&) const {
	return UINTVECTOR3(1,1,1); // HACK
}

/// World space extents.
FLOATVECTOR3 VDBDataset::GetBrickExtents(const BrickKey &) const {
	return FLOATVECTOR3(1,1,1); // HACK
}

bool VDBDataset::GetBrick(const BrickKey&, std::vector<uint8_t>&) const {
	return false;
}
bool VDBDataset::GetBrick(const BrickKey&, std::vector<int8_t>&) const {
	return false;
}
bool VDBDataset::GetBrick(const BrickKey&, std::vector<uint16_t>&) const {
	return false;
}
bool VDBDataset::GetBrick(const BrickKey&, std::vector<int16_t>&) const {
	return false;
}
bool VDBDataset::GetBrick(const BrickKey&, std::vector<uint32_t>&) const {
	return false;
}
bool VDBDataset::GetBrick(const BrickKey&, std::vector<int32_t>&) const {
	return false;
}
bool VDBDataset::GetBrick(const BrickKey&, std::vector<float>&) const {
	return false;
}
bool VDBDataset::GetBrick(const BrickKey&, std::vector<double>&) const {
	return false;
}

unsigned
VDBDataset::GetLODLevelCount() const {
	return 1; // HACK?
}

UINT64VECTOR3
VDBDataset::GetDomainSize(const size_t lod, const size_t) const {
	// Only a single brick in all but the root level.
	if(lod != 0) {
		return UINT64VECTOR3(1,1,1);
	}
	return UINT64VECTOR3(1,1,1); // HACK
}
                         

/// @returns the number of ghost cells in use
UINTVECTOR3
VDBDataset::GetBrickOverlapSize() const {
	return UINTVECTOR3(0,0,0); // HACK?
}

/// @returns the number of voxels in a brick, including ghost cells.
UINT64VECTOR3
VDBDataset::GetEffectiveBrickSize(const BrickKey &) const {
	return UINT64VECTOR3(1,1,1); // HACK!
}

unsigned VDBDataset::GetBitWidth() const {
	return 32;
}
uint64_t VDBDataset::GetComponentCount() const {
	return 1; // HACK.
}
bool VDBDataset::GetIsSigned() const {
	return true;
}
bool VDBDataset::GetIsFloat() const {
	return true;
}
bool VDBDataset::IsSameEndianness() const {
	return true; // HACK
}
std::pair<double,double>
VDBDataset::GetRange() const {
	const float mx = std::numeric_limits<float>::max();
	return std::make_pair(-mx, mx); // HACK
}

bool
VDBDataset::ContainsData(const BrickKey&, double /*isoval*/) const {
	// should test if there's a leaf brick at that key.  if so, test it's
	// min/max.
	return true; // HACK
}
bool
VDBDataset::ContainsData(const BrickKey&, double, double) const {
	return true; // HACK
}
bool
VDBDataset::ContainsData(const BrickKey&, double, double, double,
                         double) const {
	return true; // HACK
}

bool
VDBDataset::Export(uint64_t, const std::string&, bool) const {
	T_ERROR("VDB export unsupported.");
	return false;
}

bool
VDBDataset::ApplyFunction(uint64_t, Dataset::bfqn*, void*, uint64_t) const {
	T_ERROR("Function application unsupported for VBD");
	return false;
}

/// Virtual constructor.
Dataset*
VDBDataset::Create(const std::string& filename, uint64_t max_bs,
                   bool verify) const {
	if(verify) {
		WARNING("Cannot verify VDB files; ignoring verification request.");
	}
	if(max_bs > 0) {
		WARNING("Ignoring max brick size of %" PRIu64 ", VDBs bricks are always "
		        "tiny.");
	}
	return new VDBDataset(filename);
}

std::string VDBDataset::Filename() const {
	return this->filename;
}

bool
VDBDataset::CanRead(const std::string&,
                    const std::vector<int8_t>& bytes) const {
	const int32_t magic = bytes[0] << 3 | bytes[1] << 2 | bytes[2] << 1 |
	                      bytes[3];
	return magic == openvdb::OPENVDB_MAGIC;
}

/// @return a list of file extensions readable by this format
std::list<std::string> VDBDataset::Extensions() const {
	std::list<std::string> rv;
	rv.push_back("VDB");
	return rv;
}

/// @returns the min/max scalar and gradient values for the given brick.
MinMaxBlock
VDBDataset::MaxMinForKey(const BrickKey&) const {
	const float mx = std::numeric_limits<float>::max();
	return MinMaxBlock(-mx,mx, -mx,mx);
}

/// @returns the brick layout for a given LoD.  This is the number of
/// bricks which exist (given per-dimension)
UINTVECTOR3
VDBDataset::GetBrickLayout(size_t LoD, size_t) const {
	// Only a single brick in all but the root level.
	if(LoD != 0) {
		return UINTVECTOR3(1,1,1);
	}
	return UINTVECTOR3(1,1,1); // HACK!
}

}
/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) Tom Fogal

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
