#include "NetDataSource.h"
#include "DebugOut/debug.h"

DECLARE_CHANNEL(netsrc);

namespace tuvok {

NetDataSource::NetDataSource(int socksrc, const struct DSMetaData& meta,
                             std::shared_ptr<BrickCache> ch) :
  dsm(meta),
  src(socksrc),
  cache(ch)
{
  //start_receiving_thread(socksrc); // maybe?
  FIXME(netsrc, "we need to do a bunch of AddBrick calls here, one per brick");
}

/// @returns true iff it is reasonable to assume the brick BK will arrive on
/// socket SOCK *without* user intervention.  That is, that BK exists in the
/// list of bricks the server will send us without us sending a new rotation or
/// whatever.
static bool data_are_coming(int sock, const BrickKey& bk) {
  FIXME(netsrc, "Rainer needs to implement me.");
  (void) sock;
  (void) bk;
  return true;
}

/// @returns the brick key at the i'th index in the info.
static BrickKey construct_key(const struct BatchInfo bi, size_t index) {
  assert(index < bi.batchSize);
  FIXME(netsrc, "Rainer needs to implement me.");
  return BrickKey(0,0,0);
}
/// @returns the number of *elements* (*not* bytes!) in the index'th brick.
static size_t bsize(const struct BatchInfo bi, size_t index) {
  assert(index < bi.batchSize);
  FIXME(netsrc, "Rainer needs to implement me.");
  return 42;
}

namespace {
template<typename T> bool
getbrick(const BrickKey& key, std::vector<T>& data, int socket,
         std::shared_ptr<BrickCache> bc, const Dataset* ds)
{
  struct BatchInfo binfo;
  while(data_are_coming(socket, key)) {
    uint8_t** bricks = netds_readBrickBatch_ui8(&binfo);
    for(size_t i=0; i < binfo.batchSize; ++i) {
      // the cache can only accept data in vector form.
      std::vector<uint8_t> tmpdata(bsize(binfo, i));
      std::copy(bricks[i], bricks[i]+bsize(binfo, i), tmpdata.begin());

      const BrickKey bkey = construct_key(binfo, i);
      bc->add(bkey, tmpdata);
    }
    FIXME(netsrc, "we probably need to free 'bricks' here, somehow.");

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
  return getbrick<uint8_t>(k, data, this->src, this->cache, this);
}
bool
NetDataSource::GetBrick(const BrickKey& k, std::vector<uint16_t>& data) const {
  return getbrick<uint16_t>(k, data, this->src, this->cache, this);
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
unsigned
NetDataSource::GetBitWidth() const {
  FIXME(netsrc, "return 8 or 16 depending on sizeof the underlying data type");
  return 8;
}
MinMaxBlock
NetDataSource::MaxMinForKey(const BrickKey& k) const {
  FIXME(netsrc, "Rainer: need to fill in a tuvok MinMaxBlock here!");
  return MinMaxBlock();
}

bool NetDataSource::GetIsSigned() const { return false; }
bool NetDataSource::GetIsFloat() const { return false; }
bool NetDataSource::IsSameEndianness() const { return true; }
std::pair<double,double>
NetDataSource::GetRange() const {
  DO_NOT_THINK_NEEDED;
  return std::make_pair(0.0, 1000.0);
}
UINT64VECTOR3
NetDataSource::GetDomainSize(const size_t, const size_t) const {
  DO_NOT_THINK_NEEDED; return UINT64VECTOR3(0,0,0);
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

}
