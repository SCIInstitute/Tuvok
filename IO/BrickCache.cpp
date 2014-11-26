#include <algorithm>
#include <ctime>
#include <functional>
#include <map>
#include <memory>
#include "BrickCache.h"
#include "Controller/StackTimer.h"

namespace tuvok {

// This is used to basically get rid of a type.  You can do:
//   std::vector<TypeErase> data;
//   std::vector<uint8_t> foo;
//   std::vector<uint16_t> bar;
//   data.push_back(TypeErase<std::vector<uint8_t>>(foo));
//   data.push_back(TypeErase<std::vector<uint16_t>>(bar));
// with 'MyTypeOne' and 'MyTypeTwo' being completely unrelated (they do not
// need to have a common base class)
struct TypeErase {
  struct GenericType {
    virtual ~GenericType() {}
    virtual size_t elems() const { return 0; }
  };
  template<typename T> struct TypeEraser : GenericType {
    TypeEraser(const T& t) : thing(t) {}
    TypeEraser(T&& t) : thing(std::forward<T>(t)) {}
    virtual ~TypeEraser() {}
    T& get() { return thing; }
    size_t elems() const { return thing.size(); }
    private: T thing;
  };

  std::shared_ptr<GenericType> gt;
  size_t width;

  TypeErase(const TypeErase& other)
    : gt(other.gt)
    , width(other.width)
  {}

  TypeErase(TypeErase&& other)
    : gt(std::move(other.gt))
    , width(other.width) // width should stay the same right?
  {}

  TypeErase& operator=(const TypeErase& other) {
    if (this != &other) {
      gt = other.gt;
      width = other.width;
    }
    return *this;
  }

  TypeErase& operator=(TypeErase&& other) {
    if (this != &other) {
      gt = std::move(other.gt);
      width = other.width;  // width should stay the same right?
    }
    return *this;
  }

  // this isn't a very good type eraser, because we require that the type has
  // an internal typedef 'value_type' which we can use to obtain the size of
  // it.  not to mention the '.size()' member function that TypeEraser<>
  // requires, above.
  // But that's fine for our usage here; we're really just using this to store
  // vectors and erase the value_type in there anyway.
  template<typename T> TypeErase(const T& t)
    : gt(new TypeEraser<T>(t))
    , width(sizeof(typename T::value_type))
  {}

  template<typename T> TypeErase(T&& t)
    : gt(new TypeEraser<T>(std::forward<T>(t)))
    , width(sizeof(typename std::remove_reference<T>::type::value_type))
  {}
};

struct BrickInfo {
  BrickKey key;
  uint64_t access_time;
  BrickInfo(const BrickKey& k, time_t t) : key(k),
                                           access_time(uint64_t(t)) {}
};

struct CacheLRU {
  typedef std::pair<BrickInfo, TypeErase> CacheElem;
  bool operator()(const CacheElem& a, const CacheElem& b) const {
    return a.first.access_time > b.first.access_time;
  }
};

struct BrickCache::bcinfo {
    bcinfo(): bytes(0) {}
    // this is wordy but they all just forward to a real implementation below.
    const void* lookup(const BrickKey& k, uint8_t) {
      return this->typed_lookup<uint8_t>(k);
    }
    const void* lookup(const BrickKey& k, uint16_t) {
      return this->typed_lookup<uint16_t>(k);
    }
    const void* lookup(const BrickKey& k, uint32_t) {
      return this->typed_lookup<uint32_t>(k);
    }
    const void* lookup(const BrickKey& k, uint64_t) {
      return this->typed_lookup<uint64_t>(k);
    }

    const void* lookup(const BrickKey& k, int8_t) {
      return this->typed_lookup<int8_t>(k);
    }
    const void* lookup(const BrickKey& k, int16_t) {
      return this->typed_lookup<int16_t>(k);
    }
    const void* lookup(const BrickKey& k, int32_t) {
      return this->typed_lookup<int32_t>(k);
    }
    const void* lookup(const BrickKey& k, int64_t) {
      return this->typed_lookup<int64_t>(k);
    }

    const void* lookup(const BrickKey& k, float) {
      return this->typed_lookup<float>(k);
    }

    // the erasure means we can just do the insert with the thing we already
    // have: it'll make a shared_ptr out of it and insert it into the
    // container.
    ///@{
    const void* add(const BrickKey& k, std::vector<uint8_t>& data) {
      return this->typed_add<uint8_t>(k, data);
    }
    const void* add(const BrickKey& k, std::vector<uint16_t>& data) {
      return this->typed_add<uint16_t>(k, data);
    }
    const void* add(const BrickKey& k, std::vector<uint32_t>& data) {
      return this->typed_add<uint32_t>(k, data);
    }
    const void* add(const BrickKey& k, std::vector<uint64_t>& data) {
      return this->typed_add<uint64_t>(k, data);
    }
    const void* add(const BrickKey& k, std::vector<int8_t>& data) {
      return this->typed_add<int8_t>(k, data);
    }
    const void* add(const BrickKey& k, std::vector<int16_t>& data) {
      return this->typed_add<int16_t>(k, data);
    }
    const void* add(const BrickKey& k, std::vector<int32_t>& data) {
      return this->typed_add<int32_t>(k, data);
    }
    const void* add(const BrickKey& k, std::vector<int64_t>& data) {
      return this->typed_add<int64_t>(k, data);
    }
    const void* add(const BrickKey& k, std::vector<float>& data) {
      return this->typed_add<float>(k, data);
    }
    ///@}
    void remove() {
      if(!cache.empty()) {
        // libstdc++ complains it's not a heap otherwise.. somehow.  bug?
        std::make_heap(this->cache.begin(), this->cache.end(), CacheLRU());
        const CacheElem& entry = this->cache.front();
        TypeErase::GenericType& gt = *(entry.second.gt);
        assert((entry.second.width * gt.elems()) <= this->bytes);
        this->bytes -= entry.second.width * gt.elems();

        std::pop_heap(this->cache.begin(), this->cache.end(), CacheLRU());
        this->cache.pop_back();
      }
      assert(this->size() == this->bytes);
    }
    void clear() {
      this->cache.clear();
    }
    size_t size() const { return this->bytes; }

  private:
    template<typename T> const void* typed_lookup(const BrickKey& k);
    template<typename T> const void* typed_add(const BrickKey&,
                                               std::vector<T>&);

  private:
    typedef std::pair<BrickInfo, TypeErase> CacheElem;
    std::vector<CacheElem> cache;
    size_t bytes; ///< how much memory we're currently using for data.
};

struct KeyMatches {
  bool operator()(const BrickKey& key,
                  const std::pair<BrickInfo,TypeErase>& a) const {
    return key == a.first.key;
  }
};

// if the key doesn't exist, you get an empty vector.
template<typename T>
const void* BrickCache::bcinfo::typed_lookup(const BrickKey& k) {
  using namespace std::placeholders;
  KeyMatches km;
  auto func = std::bind(&KeyMatches::operator(), km, k, _1);

  // gcc can't seem to deduce this with 'auto'.
  typedef std::vector<CacheElem> maptype;
  maptype::iterator i = std::find_if(this->cache.begin(), this->cache.end(),
                                     func);
  if(i == this->cache.end()) { return NULL; }

  i->first.access_time = time(NULL);
  TypeErase::GenericType& gt = *(i->second.gt);
  assert(this->size() == this->bytes);
  return dynamic_cast<TypeErase::TypeEraser<std::vector<T>>&>(gt).get().data();
}

template<typename T>
const void* BrickCache::bcinfo::typed_add(const BrickKey& k,
                                          std::vector<T>& data) {
  // maybe the case of a general cache allows duplicate insert, but for our uses
  // there should never be a duplicate entry.
#ifndef NDEBUG
  KeyMatches km;
  using namespace std::placeholders;
  auto func = std::bind(&KeyMatches::operator(), km, k, _1);
  assert(std::find_if(this->cache.begin(), this->cache.end(), func) ==
         this->cache.end());
#endif
  this->bytes += sizeof(T) * data.size();
  this->cache.push_back(std::make_pair(BrickInfo(k, time(NULL)),
                        std::move(data)));

  assert(this->size() == this->bytes);
  TypeErase::GenericType& gt = *this->cache.back().second.gt;
  return dynamic_cast<TypeErase::TypeEraser<std::vector<T>>&>(gt).get().data();
}

BrickCache::BrickCache() : ci(new BrickCache::bcinfo) {}
BrickCache::~BrickCache() {}

const void* BrickCache::lookup(const BrickKey& k, uint8_t value) {
  return this->ci->lookup(k, value);
}
const void* BrickCache::lookup(const BrickKey& k, uint16_t value) {
  return this->ci->lookup(k, value);
}
const void* BrickCache::lookup(const BrickKey& k, uint32_t value) {
  return this->ci->lookup(k, value);
}
const void* BrickCache::lookup(const BrickKey& k, uint64_t value) {
  return this->ci->lookup(k, value);
}

const void* BrickCache::lookup(const BrickKey& k, int8_t value) {
  return this->ci->lookup(k, value);
}
const void* BrickCache::lookup(const BrickKey& k, int16_t value) {
  return this->ci->lookup(k, value);
}
const void* BrickCache::lookup(const BrickKey& k, int32_t value) {
  return this->ci->lookup(k, value);
}
const void* BrickCache::lookup(const BrickKey& k, int64_t value) {
  return this->ci->lookup(k, value);
}

const void* BrickCache::lookup(const BrickKey& k, float value) {
  return this->ci->lookup(k, value);
}


const void* BrickCache::add(const BrickKey& k,
                            std::vector<uint8_t>& data) {
  return this->ci->add(k, data);
}
const void* BrickCache::add(const BrickKey& k,
                            std::vector<uint16_t>& data) {
  return this->ci->add(k, data);
}
const void* BrickCache::add(const BrickKey& k,
                            std::vector<uint32_t>& data) {
  return this->ci->add(k, data);
}
const void* BrickCache::add(const BrickKey& k,
                            std::vector<uint64_t>& data) {
  return this->ci->add(k, data);
}
const void* BrickCache::add(const BrickKey& k,
                           std::vector<int8_t>& data) {
  return this->ci->add(k, data);
}
const void* BrickCache::add(const BrickKey& k,
                            std::vector<int16_t>& data) {
  return this->ci->add(k, data);
}
const void* BrickCache::add(const BrickKey& k,
                            std::vector<int32_t>& data) {
  return this->ci->add(k, data);
}
const void* BrickCache::add(const BrickKey& k,
                            std::vector<int64_t>& data) {
  return this->ci->add(k, data);
}
const void* BrickCache::add(const BrickKey& k,
                          std::vector<float>& data) {
  return this->ci->add(k, data);
}

void BrickCache::remove() { this->ci->remove(); }
void BrickCache::clear() { return this->ci->clear(); }
size_t BrickCache::size() const { return this->ci->size(); }

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
