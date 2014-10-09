#include "NetDataSource.h"
#include "DebugOut/debug.h"

DECLARE_CHANNEL(netsrc);

namespace tuvok {

NetDataSource::NetDataSource(const struct DSMetaData& meta) :
  dsm(meta)
{
    std::vector<BrickKey> keys;
    std::vector<BrickMD> brickMDs;

    keys.resize(meta.brickCount);
    brickMDs.resize(meta.brickCount);

    #pragma omp parallel for
    for(size_t i = 0; i < meta.brickCount; i++) {
        keys[i] = BrickKey(0, meta.lods[i], meta.idxs[i]);

        FLOATVECTOR3 center(meta.md_centers[i*3 + 0], meta.md_centers[i*3 + 1], meta.md_centers[i*3 + 2]);
        FLOATVECTOR3 extent(meta.md_extents[i*3 + 0], meta.md_extents[i*3 + 1], meta.md_extents[i*3 + 2]);
        UINTVECTOR3 voxels(meta.md_n_voxels[i*3 + 0], meta.md_n_voxels[i*3 + 1], meta.md_n_voxels[i*3 + 2]);
        brickMDs[i] = {center, extent, voxels};
    }

    //Wasn't sure if addBrick was thread-safe, so a separate loop for it
    for(size_t i = 0; i < meta.brickCount; i++) {
        AddBrick(keys[i], brickMDs[i]);
    }

    NETDS::MMInfo minMaxInfo;
    NETDS::calcMinMax(minMaxInfo);
    GetHistograms(0);
    //start_receiving_thread(socksrc); // maybe?
}
NetDataSource::~NetDataSource() {
    NETDS::closeFile(Filename());
    NETDS::clearMinMaxValues();
    NETDS::clearRotationKeys();
}
void
NetDataSource::SetCache(std::shared_ptr<BrickCache> ch) { this->cache = ch; }

/// @returns true if it is reasonable to assume the brick BK will arrive on
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

void
NetDataSource::GetHistograms(size_t) {
    FIXME(netsrc, "The histogram is not being generated properly... this is just a placeholder. @Tom plz check if this makes sense");

    // generate a zero 1D histogram (max 4k) if none is found in the file
    m_pHist1D.reset(new Histogram1D(
                        std::min(MAX_TRANSFERFUNCTION_SIZE, 1<<GetBitWidth())));

    // set all values to one so "getFilledsize" later does not return a
    // completely empty dataset
    for (size_t i = 0;i<m_pHist1D->GetSize();i++) {
        m_pHist1D->Set(i, 1);
    }

    VECTOR2<size_t> vec(256, std::min(MAX_TRANSFERFUNCTION_SIZE, 1<<GetBitWidth()));

    m_pHist2D.reset(new Histogram2D(vec));
    for (size_t y=0; y < m_pHist2D->GetSize().y; y++) {
        // set all values to one so "getFilledsize" later does not return a
        // completely empty dataset
        for (size_t x=0; x < m_pHist2D->GetSize().x; x++) {
            m_pHist2D->Set(x,y,1);
        }
    }
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

    //If we arrive here, we requested a key that was not in the queue
    WARN(netsrc, "A brick was requested that is not in the sending queue! Falling back to requesting it!");
    size_t keyLoD = std::get<1>(key);
    size_t keyBidx = std::get<2>(key);
    return NETDS::getBrick(keyLoD, keyBidx, data);
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
NetDataSource::ContainsData(const BrickKey& k, double isoval) const {
    const MinMaxBlock maxMinElement = MaxMinForKey(k);
    return (isoval <= maxMinElement.maxScalar);
}
bool
NetDataSource::ContainsData(const BrickKey& k, double fMin,
                            double fMax) const {

    const MinMaxBlock maxMinElement = MaxMinForKey(k);
    return (fMax >= maxMinElement.minScalar && fMin <= maxMinElement.maxScalar);
}
bool
NetDataSource::ContainsData(const BrickKey& k, double fMin, double fMax,
                            double fMinGradient,
                            double fMaxGradient) const {
    const MinMaxBlock maxMinElement = MaxMinForKey(k);
    return (fMax >= maxMinElement.minScalar &&
            fMin <= maxMinElement.maxScalar)
                           &&
           (fMaxGradient >= maxMinElement.minGradient &&
            fMinGradient <= maxMinElement.maxGradient);
}

NetDataSource*
NetDataSource::Create(const std::string&, uint64_t,bool) const {
  DO_NOT_THINK_NEEDED; return NULL;
}
std::string NetDataSource::Filename() const {return dsm.filename;}
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
    return dsm.maxGradientMagnitude;
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
