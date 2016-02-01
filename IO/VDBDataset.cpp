#include <cinttypes>
#include <limits>
#include <utility>
#include <vector>
#include <openvdb/openvdb.h>
#include <openvdb/version.h>
#include "Controller/Controller.h"
#include "VDBDataset.h"

namespace tuvok {

// In version 3.0.0 of OpenVDB, and perhaps others, File's destructor will not
// close an open file.
struct vdbfile {
	vdbfile(const std::string& f) : v(f) {
		bool dont_read_now = true;
		this->v.open(dont_read_now);
	}
	~vdbfile() {
		this->v.close();
	}
	openvdb::io::File v;
};

static bool
openable(const std::string& f) {
	vdbfile vdb(f);
	return vdb.v.isOpen();
}

// We need some way to convey the field name to read without recompiling all of
// Tuvok.  There's no mechanism in the IV3D UI for setting any kind of option
// or querying the user for help reading a file (sans the raw dialog, which is
// 100% special-cased ...), so we hack it for now.
static std::string
field_name() {
	const char* const fld = getenv("IV3D_READ_FIELD");
	if(NULL == fld) {
		return std::string("density");
	}
	return std::string(fld);
}

// Computes the minmax in a VDB file.  Slowly/poorly.
static std::pair<float,float>
minmax(openvdb::io::File& vdb) {
	assert(vdb.isOpen());
	const openvdb::GridBase::Ptr voidfld = vdb.readGrid(field_name());
	const openvdb::FloatGrid::Ptr fieldf = openvdb::gridPtrCast<
		openvdb::FloatGrid>(fieldf);
	const float mx = std::numeric_limits<float>::max();
	std::pair<float,float> mm(mx, -mx);
	for(openvdb::FloatGrid::ValueOnIter i = fieldf->beginValueOn(); i; ++i) {
		mm.first = std::min(mm.first, i.getValue());
		mm.second = std::max(mm.second, i.getValue());
	}
	assert(mm.second >= mm.first);
	if(mm.first == mm.second) {
		WARNING("Strangely, the data consist of only a single value: %f", mm.first);
	}
	return mm;
}

static std::vector<uint32_t>
compute_histogram(openvdb::io::File& vdb) {
	assert(vdb.isOpen());
	openvdb::GridBase::Ptr voidfld = vdb.readGrid(field_name());
	openvdb::FloatGrid::Ptr fieldf = openvdb::gridPtrCast<openvdb::FloatGrid>(
		voidfld
	);
	// we assume we're always quantizing float down to 4096 bins.
	const size_t hist_size = 4096;
	const std::pair<float,float> mmax = minmax(vdb);
	std::vector<uint32_t> hist(hist_size);
	std::fill(hist.begin(), hist.end(), 0);
	const float qfactor = (hist_size-1) / (mmax.second-mmax.first);
	for(openvdb::FloatGrid::ValueOnIter i = fieldf->beginValueOn(); i; ++i) {
		const float v = i.getValue();
		const size_t hidx = std::min(hist_size-1, size_t((v-mmax.first)*qfactor));
		hist[hidx]++;
	}
	return hist;
}

VDBDataset::VDBDataset() { }
VDBDataset::VDBDataset(const std::string& fname) : filename(fname) {
	openvdb::initialize();
	if(!openable(fname)) {
		WARNING("could not open %s", fname.c_str());
	}
	vdbfile vdb(filename);
	if(!vdb.v.isOpen()) {
		vdb.v.open();
	}
	std::vector<uint32_t> histo = compute_histogram(vdb.v);
}
VDBDataset::~VDBDataset() {
	openvdb::uninitialize();
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
	const uint32_t magic = bytes[0] << 0*8 | bytes[1] << 1*8 | bytes[2] << 2*8 |
	                       bytes[3] << 3*8;
	MESSAGE("magic: 0x%x, vdb magic: 0x%x", magic, openvdb::OPENVDB_MAGIC);
	return (int32_t)magic == openvdb::OPENVDB_MAGIC;
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
