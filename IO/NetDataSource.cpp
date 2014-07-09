#include "NetDataSource.h"
#include "DebugOut/debug.h"

DECLARE_CHANNEL(netsrc);

namespace tuvok {

NetDataSource::NetDataSource(const struct DSMetaData& meta) :
  dsm(meta)
{
    std::vector<BrickKey> keys;
    std::vector<BrickMD> brickMDs;

    keys.reserve(meta.brickCount);
    brickMDs.reserve(meta.brickCount);

    #pragma omp parallel for
    for(size_t i = 0; i < meta.brickCount; i++) {
        keys.push_back(BrickKey(0, meta.lods[i], meta.idxs[i]));

        FLOATVECTOR3 center(meta.md_centers[i*3 + 0], meta.md_centers[i*3 + 1], meta.md_centers[i*3 + 2]);
        FLOATVECTOR3 extent(meta.md_extents[i*3 + 0], meta.md_extents[i*3 + 1], meta.md_extents[i*3 + 2]);
        UINTVECTOR3 voxels(meta.md_n_voxels[i*3 + 0], meta.md_n_voxels[i*3 + 1], meta.md_n_voxels[i*3 + 2]);
        brickMDs.push_back({center, extent, voxels});
    }

    //Wasn't sure if addBrick was thread-safe, so a separate loop for it
    for(size_t i = 0; i < meta.brickCount; i++) {
        AddBrick(keys[i], brickMDs[i]);
    }

    //start_receiving_thread(socksrc); // maybe?
}
NetDataSource::~NetDataSource() {
    NETDS::closeFile(Filename());
}
void
NetDataSource::SetCache(std::shared_ptr<BrickCache> ch) { this->cache = ch; }

/// @returns true iff it is reasonable to assume the brick BK will arrive on
/// socket SOCK *without* user intervention.  That is, that BK exists in the
/// list of bricks the server will send us without us sending a new rotation or
/// whatever.
static bool data_are_coming(const BrickKey& bk) {
    std::shared_ptr<NETDS::RotateInfo> rInfo = NETDS::getLastRotationKeys();
    if(rInfo == NULL)
        return false;

    size_t keyLoD = std::get<1>(bk);
    size_t keyBidx = std::get<2>(bk);

    for(size_t i = 0; i < rInfo->brickCount; i++) {
        if(rInfo->lods[i] == keyLoD && rInfo->idxs[i] == keyBidx)
            return true;
    }
    return false;
}

/// @returns the brick key at the i'th index in the info.
static BrickKey construct_key(const struct BatchInfo bi, size_t index) {
    assert(index < bi.batchSize);
    return BrickKey(0, bi.lods[index], bi.idxs[index]);
}
/// @returns the number of *elements* (*not* bytes!) in the index'th brick.
static size_t bsize(const struct BatchInfo bi, size_t index) {
  assert(index < bi.batchSize);
  return bi.brickSizes[index];
}

namespace {
template<typename T> bool
getbrick(const BrickKey& key, std::vector<T>& data,
         std::shared_ptr<BrickCache> bc, const Dataset* ds)
{
    struct BatchInfo binfo;
    vector<vector<uint8_t>> batchData;

    while(data_are_coming(key)) {
        if(!NETDS::readBrickBatch(binfo, batchData)) {
            FIXME(netsrc, "Somehow handle failure...");
            abort();
        }

        for(size_t i=0; i < binfo.batchSize; ++i) {
            const BrickKey bkey = construct_key(binfo, i);
            bc->add(bkey, batchData[i]);
        }

        // did we get the data we were looking for?
        const uint8_t* dptr = (const uint8_t*)bc->lookup(key, uint8_t());
        const size_t nvoxels = ds->GetEffectiveBrickSize(key).volume();
        if(NULL != dptr) {
            std::copy(dptr, dptr+nvoxels, data.begin());
            return true;
        }
    }
    return false;
}
} // end anonymous namespace.

bool
NetDataSource::GetBrick(const BrickKey& k, std::vector<uint8_t>& data) const {
  return getbrick<uint8_t>(k, data, this->cache, this);
}
bool
NetDataSource::GetBrick(const BrickKey& k, std::vector<uint16_t>& data) const {
  return getbrick<uint16_t>(k, data, this->cache, this);
}
bool
NetDataSource::GetBrick(const BrickKey& k, std::vector<uint32_t>& data) const {
  return getbrick<uint32_t>(k, data, this->cache, this);
}

// we don't expect that this function is needed, and want to be notified if it
// is needed.
#define DO_NOT_THINK_NEEDED \
  FIXME(netsrc, "we did not expect this function was required."); \
  assert(false)

unsigned NetDataSource::GetLODLevelCount() const { return this->dsm.lodCount; }
UINT64VECTOR3
NetDataSource::GetEffectiveBrickSize(const BrickKey& k) const {
  TRACE(netsrc, "if this fails, we have not yet done the AddBrick ...");
  const BrickMD& bmd = this->GetBrickMetadata(k);
  return UINT64VECTOR3(bmd.n_voxels[0], bmd.n_voxels[1], bmd.n_voxels[2]);
}

UINTVECTOR3
NetDataSource::GetBrickLayout(size_t lod, size_t /*ts*/) const {
  return UINTVECTOR3(this->dsm.layouts[lod*3+0],
                     this->dsm.layouts[lod*3+1],
                     this->dsm.layouts[lod*3+2]);
}
UINTVECTOR3
NetDataSource::GetBrickOverlapSize() const {
    return UINTVECTOR3(this->dsm.overlap[0], this->dsm.overlap[1], this->dsm.overlap[2]);
}

unsigned
NetDataSource::GetBitWidth() const {
    return dsm.typeInfo.bitwidth;
}
MinMaxBlock
NetDataSource::MaxMinForKey(const BrickKey& bk) const {
    std::shared_ptr<NETDS::MMInfo> mmInfo = NETDS::getMinMaxInfo();
    size_t tgtLod = std::get<1>(bk);
    size_t tgtIdx = std::get<2>(bk);

    for(size_t i = 0; i < mmInfo->brickCount; i++) {
        if(mmInfo->lods[i] == tgtLod && mmInfo->idxs[i] == tgtIdx)
            return MinMaxBlock(mmInfo->minScalars[i],
                               mmInfo->maxScalars[i],
                               mmInfo->minGradients[i],
                               mmInfo->maxGradients[i]);
    }

    WARN(netsrc, "BrickKey (lod=%zu, idx=%zu) not found in minMaxInfo!", tgtLod, tgtIdx);
    abort();
    return MinMaxBlock();
}

bool NetDataSource::GetIsSigned() const { return dsm.typeInfo.is_signed; }
bool NetDataSource::GetIsFloat() const { return dsm.typeInfo.is_float; }
bool NetDataSource::IsSameEndianness() const { return true; }
std::pair<double,double>
NetDataSource::GetRange() const {
    return std::make_pair(this->dsm.range1, this->dsm.range2);
}
UINT64VECTOR3
NetDataSource::GetDomainSize(const size_t lod, const size_t) const {
    return UINT64VECTOR3(   dsm.domainSizes[lod*3+0],
                            dsm.domainSizes[lod*3+1],
                            dsm.domainSizes[lod*3+2]);
}
// multicomponent data would be nice, but... well, ignore for now.
uint64_t NetDataSource::GetComponentCount() const { return 1; }

bool
NetDataSource::ContainsData(const BrickKey&, double /*isoval*/) const {
  DO_NOT_THINK_NEEDED; return false;
}
bool
NetDataSource::ContainsData(const BrickKey&, double /*fMin*/,
                            double /*fMax*/) const {
  DO_NOT_THINK_NEEDED; return false;
}
bool
NetDataSource::ContainsData(const BrickKey&, double /*fMin*/, double /*fMax*/,
                            double /*fMinGradient*/,
                            double /*fMaxGradient*/) const {
  DO_NOT_THINK_NEEDED; return false;
}

NetDataSource*
NetDataSource::Create(const std::string&, uint64_t,bool) const {
  DO_NOT_THINK_NEEDED; return NULL;
}
std::string NetDataSource::Filename() const { DO_NOT_THINK_NEEDED; return ""; }
const char* NetDataSource::Name() const { DO_NOT_THINK_NEEDED; return "netDS"; }

bool
NetDataSource::CanRead(const std::string&, const std::vector<int8_t>&) const {
  DO_NOT_THINK_NEEDED;
  return false;
}
bool
NetDataSource::Verify(const std::string&) const {
  DO_NOT_THINK_NEEDED; return false;
}
std::list<std::string>
NetDataSource::Extensions() const {
  DO_NOT_THINK_NEEDED; return std::list<std::string>();
}

float
NetDataSource::MaxGradientMagnitude() const {
  DO_NOT_THINK_NEEDED; return 0.0f;
}

bool
NetDataSource::Export(uint64_t, const std::string&, bool) const {
  DO_NOT_THINK_NEEDED; return false;
}

bool
NetDataSource::ApplyFunction(uint64_t,
                        bool(*)(void*, const UINT64VECTOR3&,
                                const UINT64VECTOR3&, void*),
                        void*,
                        uint64_t) const {
  DO_NOT_THINK_NEEDED; return false;
}

}
