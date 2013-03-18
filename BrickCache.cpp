#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include "BrickCache.h"

namespace tuvok {

// This is used to basically get rid of a type.  You can do:
//   std::vector<TypeErase> data;
//   MyTypeOne avar;
//   MyTypeTwo bvar;
//   data.push_back(TypeErase<MyObj>(avar));
//   data.push_back(TypeErase<MyObj>(bvar));
// with 'MyTypeOne' and 'MyTypeTwo' being completely unrelated (they do not
// need to have a common base class)
struct TypeErase {
  struct GenericType {
    virtual ~GenericType() {}
  };
  template<typename T> struct TypeEraser : GenericType {
    TypeEraser(const T& t) : thing(t) {}
    virtual ~TypeEraser() {}
    T& get() { return thing; }
    private: T thing;
  };

  std::shared_ptr<GenericType> gt;
  template<typename T> TypeErase(const T& t) : gt(new TypeEraser<T>(t)) {}
};

struct BrickInfo {
  const BrickKey& key;
  uint64_t access_time;
  BrickInfo(const BrickKey& k, time_t t) : key(k),
                                           access_time(uint64_t(t)) {}
};

struct CacheLRU {
  bool operator()(const BrickInfo& a, const BrickInfo& b) const {
    return a.access_time > b.access_time;
  }
};

struct BrickCache::bcinfo {
    bcinfo() {}
    // this is wordy but they all just forward to a real implementation below.
    std::vector<uint8_t> lookup(const BrickKey& k, uint8_t) {
      return this->typed_lookup<uint8_t>(k);
    }
    std::vector<uint16_t> lookup(const BrickKey& k, uint16_t) {
      return this->typed_lookup<uint16_t>(k);
    }
    std::vector<uint32_t> lookup(const BrickKey& k, uint32_t) {
      return this->typed_lookup<uint32_t>(k);
    }
    std::vector<uint64_t> lookup(const BrickKey& k, uint64_t) {
      return this->typed_lookup<uint64_t>(k);
    }
    std::vector<int8_t> lookup(const BrickKey& k, int8_t) {
      return this->typed_lookup<int8_t>(k);
    }
    std::vector<int16_t> lookup(const BrickKey& k, int16_t) {
      return this->typed_lookup<int16_t>(k);
    }
    std::vector<int32_t> lookup(const BrickKey& k, int32_t) {
      return this->typed_lookup<int32_t>(k);
    }
    std::vector<int64_t> lookup(const BrickKey& k, int64_t) {
      return this->typed_lookup<int64_t>(k);
    }
    std::vector<float> lookup(const BrickKey& k, float) {
      return this->typed_lookup<float>(k);
    }

    // the erasure means we can just do the insert with the thing we already
    // have: it'll make a shared_ptr out of it and insert it into the
    // container.
    ///@{
    std::vector<uint8_t>& add(const BrickKey& k, std::vector<uint8_t>& data) {
      this->cache.insert(std::make_pair(BrickInfo(k, time(NULL)), data));
      return data;
    }
    std::vector<uint16_t>& add(const BrickKey& k, std::vector<uint16_t>& data) {
      this->cache.insert(std::make_pair(BrickInfo(k, time(NULL)), data));
      return data;
    }
    std::vector<uint32_t>& add(const BrickKey& k, std::vector<uint32_t>& data) {
      this->cache.insert(std::make_pair(BrickInfo(k, time(NULL)), data));
      return data;
    }
    std::vector<uint64_t>& add(const BrickKey& k, std::vector<uint64_t>& data) {
      this->cache.insert(std::make_pair(BrickInfo(k, time(NULL)), data));
      return data;
    }
    std::vector<int8_t>& add(const BrickKey& k, std::vector<int8_t>& data) {
      this->cache.insert(std::make_pair(BrickInfo(k, time(NULL)), data));
      return data;
    }
    std::vector<int16_t>& add(const BrickKey& k, std::vector<int16_t>& data) {
      this->cache.insert(std::make_pair(BrickInfo(k, time(NULL)), data));
      return data;
    }
    std::vector<int32_t>& add(const BrickKey& k, std::vector<int32_t>& data) {
      this->cache.insert(std::make_pair(BrickInfo(k, time(NULL)), data));
      return data;
    }
    std::vector<int64_t>& add(const BrickKey& k, std::vector<int64_t>& data) {
      this->cache.insert(std::make_pair(BrickInfo(k, time(NULL)), data));
      return data;
    }
    std::vector<float>& add(const BrickKey& k, std::vector<float>& data) {
      this->cache.insert(std::make_pair(BrickInfo(k, time(NULL)), data));
      return data;
    }
    //void remove(const BrickKey& k) { this->cache->erase(k); }
    ///@}

  private:
    template<typename T> std::vector<T> typed_lookup(const BrickKey& k);

  private:
    std::map<BrickInfo, TypeErase, CacheLRU> cache;
};

struct KeyMatches {
  bool operator()(const BrickKey& key,
                  const std::pair<BrickInfo,TypeErase>& a) const {
    return key == a.first.key;
  }
};

// if the key doesn't exist, you get an empty vector.
template<typename T>
std::vector<T> BrickCache::bcinfo::typed_lookup(const BrickKey& k) {
  using namespace std::placeholders;
  KeyMatches km;
  auto func = std::bind(&KeyMatches::operator(), km, k, _1);

  // gcc can't seem to deduce this with 'auto'.
  typedef std::map<BrickInfo, TypeErase, CacheLRU> maptype;
  maptype::iterator i = std::find_if(this->cache.begin(), this->cache.end(),
                                     func);
  if(i == this->cache.end()) { return std::vector<T>(); }

  TypeErase::GenericType& gt = *(i->second.gt);
  auto te_vec = dynamic_cast<TypeErase::TypeEraser<std::vector<T>>&>(gt);
  return te_vec.get();
}

BrickCache::BrickCache() : ci(new BrickCache::bcinfo) {}
BrickCache::~BrickCache() {}

std::vector< uint8_t> BrickCache::lookup(const BrickKey& k, uint8_t) {
  return this->ci->lookup(k, uint8_t());
}
std::vector<uint16_t> BrickCache::lookup(const BrickKey& k, uint16_t) {
  return this->ci->lookup(k, uint16_t());
}
std::vector<uint32_t> BrickCache::lookup(const BrickKey& k, uint32_t) {
  return this->ci->lookup(k, uint32_t());
}
std::vector<uint64_t> BrickCache::lookup(const BrickKey& k, uint64_t) {
  return this->ci->lookup(k, uint64_t());
}
std::vector< int8_t> BrickCache::lookup(const BrickKey& k, int8_t) {
  return this->ci->lookup(k, int8_t());
}
std::vector<int16_t> BrickCache::lookup(const BrickKey& k, int16_t) {
  return this->ci->lookup(k, int16_t());
}
std::vector<int32_t> BrickCache::lookup(const BrickKey& k, int32_t) {
  return this->ci->lookup(k, int32_t());
}
std::vector<int64_t> BrickCache::lookup(const BrickKey& k, int64_t) {
  return this->ci->lookup(k, int64_t());
}
std::vector<float> BrickCache::lookup(const BrickKey& k, float) {
  return this->ci->lookup(k, float());
}

std::vector<uint8_t>& BrickCache::add(const BrickKey& k,
                                      std::vector<uint8_t>& data) {
  return this->ci->add(k, data);
}
std::vector<uint16_t>& BrickCache::add(const BrickKey& k,
                                      std::vector<uint16_t>& data) {
  return this->ci->add(k, data);
}
std::vector<uint32_t>& BrickCache::add(const BrickKey& k,
                                      std::vector<uint32_t>& data) {
  return this->ci->add(k, data);
}
std::vector<uint64_t>& BrickCache::add(const BrickKey& k,
                                      std::vector<uint64_t>& data) {
  return this->ci->add(k, data);
}
std::vector<int8_t>& BrickCache::add(const BrickKey& k,
                                     std::vector<int8_t>& data) {
  return this->ci->add(k, data);
}
std::vector<int16_t>& BrickCache::add(const BrickKey& k,
                                      std::vector<int16_t>& data) {
  return this->ci->add(k, data);
}
std::vector<int32_t>& BrickCache::add(const BrickKey& k,
                                      std::vector<int32_t>& data) {
  return this->ci->add(k, data);
}
std::vector<int64_t>& BrickCache::add(const BrickKey& k,
                                      std::vector<int64_t>& data) {
  return this->ci->add(k, data);
}
std::vector<float>& BrickCache::add(const BrickKey& k,
                                    std::vector<float>& data) {
  return this->ci->add(k, data);
}

}
/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2013 Scientific Computing and Imaging Institute,
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
