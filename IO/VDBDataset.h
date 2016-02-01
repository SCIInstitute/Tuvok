#ifndef TUVOK_VDB_DATASET_H
#define TUVOK_VDB_DATASET_H

#include <list>
#include <string>
#include "FileBackedDataset.h"
#include "LinearIndexDataset.h"

namespace tuvok {

// VDBDataset loads data from VDB (i.e. OpenVDB) files.  Only the data in the
// root bricks are loaded; all other bricks are reported "empty".  This is,
// strictly speaking, not valid, but all datasets the authors have seen in
// practice actually do not care about these other levels.
class VDBDataset : public LinearIndexDataset, public FileBackedDataset {
public:
	VDBDataset();
	VDBDataset(const std::string& fname);
	virtual ~VDBDataset();

	virtual float MaxGradientMagnitude() const;
	// Clear any caches or brick metadata
	virtual void Clear();

	/// Gets the number of voxels, per dimension.
	virtual UINTVECTOR3 GetBrickVoxelCounts(const BrickKey&) const;
	/// World space extents.
	virtual FLOATVECTOR3 GetBrickExtents(const BrickKey &) const;

	virtual bool GetBrick(const BrickKey&, std::vector<uint8_t>&) const;
	virtual bool GetBrick(const BrickKey&, std::vector<int8_t>&) const;
	virtual bool GetBrick(const BrickKey&, std::vector<uint16_t>&) const;
	virtual bool GetBrick(const BrickKey&, std::vector<int16_t>&) const;
	virtual bool GetBrick(const BrickKey&, std::vector<uint32_t>&) const;
	virtual bool GetBrick(const BrickKey&, std::vector<int32_t>&) const;
	virtual bool GetBrick(const BrickKey&, std::vector<float>&) const;
	virtual bool GetBrick(const BrickKey&, std::vector<double>&) const;

	virtual unsigned GetLODLevelCount() const;

	virtual UINT64VECTOR3 GetDomainSize(const size_t lod=0,
                                      const size_t ts=0) const;

	/// @returns the number of ghost cells in use
  virtual UINTVECTOR3 GetBrickOverlapSize() const;

	/// @returns the number of voxels in a brick, including ghost cells.
  virtual UINT64VECTOR3 GetEffectiveBrickSize(const BrickKey &) const;

	virtual unsigned GetBitWidth() const;
	virtual uint64_t GetComponentCount() const;
	virtual bool GetIsSigned() const;
	virtual bool GetIsFloat() const;
	virtual bool IsSameEndianness() const;
	virtual std::pair<double,double> GetRange() const;

	virtual bool ContainsData(const BrickKey&, double /*isoval*/) const;
	virtual bool ContainsData(const BrickKey&, double, double) const;
	virtual bool ContainsData(const BrickKey&, double, double, double,
	                          double) const;

	virtual bool Export(uint64_t iLODLevel, const std::string& targetFilename, 
	                    bool bAppend) const;

  virtual bool ApplyFunction(uint64_t iLODLevel, 
                        bool (*brickFunc)(void* pData, 
                                          const UINT64VECTOR3& vBrickSize,
                                          const UINT64VECTOR3& vBrickOffset,
                                          void* pUserContext),
                        void *pUserContext,
                        uint64_t iOverlap) const;

	/// Virtual constructor.
	virtual Dataset* Create(const std::string&, uint64_t, bool) const;

	virtual std::string Filename() const;
	/// A user-visible name for your format.  This might get displayed in UI
	/// elements; e.g. the GUI might ask if the user wants to use the "Name()
	/// reader" to open a particular file.
	virtual const char* Name() const { return "VDB"; }

	virtual bool CanRead(const std::string&, const std::vector<int8_t>&) const;

	/// @return a list of file extensions readable by this format
	virtual std::list<std::string> Extensions() const;

  /// @returns the min/max scalar and gradient values for the given brick.
  virtual tuvok::MinMaxBlock MaxMinForKey(const BrickKey&) const;

  /// @returns the brick layout for a given LoD.  This is the number of
  /// bricks which exist (given per-dimension)
  virtual UINTVECTOR3 GetBrickLayout(size_t LoD, size_t timestep) const;

private:
	const std::string filename;
};
}
#endif
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
