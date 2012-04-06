#include "StdTuvokDefines.h"
#if defined(_MSC_VER)
# include <functional>
#else
# include <tr1/functional>
#endif
#include <algorithm>
#include <cstdarg>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "ShaderDescriptor.h"
#include "Basics/SysTools.h"
#include "Basics/SystemInfo.h"
#include "Controller/Controller.h"

namespace tuvok {

enum shader_type { SHADER_VERTEX_DISK, SHADER_VERTEX_STRING,
                   SHADER_FRAGMENT_DISK, SHADER_FRAGMENT_STRING };

struct ShaderDescriptor::sinfo {
  std::vector<std::pair<std::string, enum shader_type> > vertex;
  std::vector<std::pair<std::string, enum shader_type> > fragment;
  bool operator==(const ShaderDescriptor::sinfo& sdi) const;
};
bool ShaderDescriptor::sinfo::operator==(const ShaderDescriptor::sinfo& sdi)
const {
  return vertex.size() == sdi.vertex.size() &&
         fragment.size() == sdi.fragment.size() &&
         std::equal(vertex.begin(), vertex.end(), sdi.vertex.begin()) &&
         std::equal(fragment.begin(), fragment.end(), sdi.fragment.begin());
}

ShaderDescriptor::ShaderDescriptor() : si(new struct sinfo()) { }
ShaderDescriptor::ShaderDescriptor(const ShaderDescriptor& sd) : si(sd.si) {}
ShaderDescriptor::ShaderDescriptor(const std::vector<std::string>& vertex,
                                   const std::vector<std::string>& fragment) :
  si(new struct sinfo())
{
  typedef std::vector<std::string> sv;
  for(sv::const_iterator v = vertex.begin(); v != vertex.end(); ++v) {
    this->si->vertex.push_back(std::make_pair(*v, SHADER_VERTEX_DISK));
  }
  for(sv::const_iterator f = fragment.begin(); f != fragment.end(); ++f) {
    this->si->fragment.push_back(std::make_pair(*f, SHADER_FRAGMENT_DISK));
  }
}

// SysTools::FileExists can take a std::string OR a std::wstring.  This makes
// it hard to use in a function composition, because the compiler cannot figure
// out which one we want, and it's not a template or anything so we cannot just
// be explicit.
// This serves to rename it to avoid the ambiguity.
static bool exists(std::string s) { return SysTools::FileExists(s); }
// we could technically achieve this by composing std::plus with
// std::plus, but my god is that a nightmare in c++03.
static std::string concat(std::string a, std::string b, std::string c) {
  return a + b + c;
}

// Searches for the given filename in the given directories.  Returns the fully
// qualified path of the file's location.
static std::string find_filename(std::vector<std::string> directories,
                                 std::string filename)
{
  // if we're on Mac, first try to see if the file is in our bundle.
#ifdef DETECTED_OS_APPLE
  if (SysTools::FileExists(SysTools::GetFromResourceOnMac(filename))) {
    filename = SysTools::GetFromResourceOnMac(filename);
    MESSAGE("Found %s in bundle, using that.", filename.c_str());
    return filename;
  }
#endif

  typedef std::vector<std::string> sv;
  // check for garbage directories and warn the user about them
  sv::iterator end = std::remove_if(directories.begin(), directories.end(),
                                    std::not1(std::ptr_fun(exists)));
  for(sv::const_iterator e = end; e != directories.end(); ++e) {
    WARNING("Directory %s does not exist!", e->c_str());
  }
  // also, we know they're junk, so don't search in them
  directories.erase(end, directories.end());

  // okay, now prepend each directory into our flename and see if we find a
  // match.
  using namespace std::tr1::placeholders;
  const std::string dirsep = "/";
  // the functor is a composition: 'exists(add(_1, dirsep, filename))'
  sv::const_iterator fn =
    std::find_if(directories.begin(), directories.end(),
                 std::tr1::bind(
                   std::ptr_fun(exists),
                   std::tr1::bind(concat, _1, dirsep, filename)
                 ));

  if(fn == directories.end()) { // file not found.
    throw std::runtime_error("could not find file");
  }
  return *fn + dirsep + filename;
}

ShaderDescriptor ShaderDescriptor::Create(
  std::vector<std::string> directories, ...
) {
  ShaderDescriptor rv;
  va_list args;
  va_start(args, directories);

  const char* filename;
  // we expect two NULLs: first terminates vertex list, second fragment list.
  do {
    filename = va_arg(args, const char*);
    if(filename != NULL) {
      rv.si->vertex.push_back(std::make_pair(std::string(filename),
                              SHADER_VERTEX_DISK));
    }
  } while(filename != NULL);

  // now second: fragment shaders.
  do {
    filename = va_arg(args, const char*);
    if(filename != NULL) {
      rv.si->fragment.push_back(std::make_pair(std::string(filename),
                                SHADER_FRAGMENT_DISK));
    }
  } while(filename != NULL);
  va_end(args);

  // now try to clean up all those paths.
  // The user gave us some directories to search, but let's make sure we also
  // search the location of our binary.
  std::vector<std::string> dirs = SysTools::GetSubDirList(
    Controller::Instance().SysInfo()->GetProgramPath()
  );
  directories.insert(directories.end(), dirs.begin(), dirs.end());
  directories.push_back(Controller::Instance().SysInfo()->GetProgramPath());
    
  typedef std::vector<std::pair<std::string, enum shader_type> > sv;
  for(sv::iterator v = rv.si->vertex.begin(); v != rv.si->vertex.end(); ++v) {
    v->first = find_filename(directories, v->first);
  }
  for(sv::iterator f = rv.si->fragment.begin(); f != rv.si->fragment.end();
      ++f) {
    f->first = find_filename(directories, f->first);
  }

  return rv;
}

/// Adds a vertex shader in a string (i.e. not from a filename)
void ShaderDescriptor::AddVertexShaderString(const std::string shader) {
  this->si->vertex.push_back(std::make_pair(shader, SHADER_VERTEX_STRING));
}

/// Adds a fragment shader in a string (i.e. not from a filename)
void ShaderDescriptor::AddFragmentShaderString(const std::string shader) {
  this->si->fragment.push_back(std::make_pair(shader, SHADER_FRAGMENT_STRING));
}

/// Two shaders are equal if they utilize the same set of filenames/strings
/// to compose the shader.
bool ShaderDescriptor::operator ==(const ShaderDescriptor& sd) const
{
  return this->si == sd.si;
}

static std::string readfile(const std::string& filename) {
  // open in append mode so the file pointer will be at EOF and we can
  // therefore easily/quickly figure out the file size.
  std::ifstream ifs(filename.c_str(), std::ios::in | std::ios::ate);
  if(!ifs.is_open()) {
    T_ERROR("Could not open shader '%s'", filename.c_str());
    throw std::runtime_error("file could not be opened");
  }
  std::ifstream::pos_type len = ifs.tellg();
  ifs.seekg(0, std::ios::beg);

  std::vector<char> file(size_t(len+std::ifstream::pos_type(1)), 0);
  size_t offset=0;
  do {
    std::streamsize length = std::streamsize(len) - std::streamsize(offset);
    ifs.read(&file[offset], length);
    offset += size_t(ifs.gcount());
  } while(!ifs.eof() && std::ifstream::pos_type(offset) < len);
  ifs.close();

  return std::string(&file[0]);
}

typedef std::vector<std::pair<std::string, enum shader_type> > slist;
// internal implementation: we keep track of which object (ShaderDescriptor) we
// came from, the location within that object, and what type we are.  The
// latter helps us with equality; no location in the vertex shader list is
// equal to any location in the fragment shader list.
struct ShaderDescriptor::SIterator::siterinfo {
  const ShaderDescriptor* sd;
  slist::const_iterator location;
  enum VertFrag { ITER_VERTEX, ITER_FRAGMENT } vf;

  siterinfo(const ShaderDescriptor* sdesc, slist::const_iterator loc,
            enum VertFrag typ) : sd(sdesc), location(loc), vf(typ) { }
  bool operator==(const siterinfo& sit) const {
    return this->vf == sit.vf &&
           this->location == sit.location;
  }
};

ShaderDescriptor::SIterator::SIterator(const ShaderDescriptor::SIterator& sit) :
  si(sit.si) { }
ShaderDescriptor::SIterator::SIterator(
  struct ShaderDescriptor::SIterator::siterinfo sit
) : si(new ShaderDescriptor::SIterator::siterinfo(sit)) { }
ShaderDescriptor::SIterator& ShaderDescriptor::SIterator::operator++() {
  ++this->si->location;
  return *this;
}
ShaderDescriptor::SIterator& ShaderDescriptor::SIterator::operator++(int n) {
  std::advance(this->si->location, n);
  return *this;
}
bool
ShaderDescriptor::SIterator::operator==(const ShaderDescriptor::SIterator& sit)
const {
  return *(this->si) == (*sit.si);
}
bool
ShaderDescriptor::SIterator::operator!=(const ShaderDescriptor::SIterator& sit)
const {
  return !(*this == sit);
}
std::pair<std::string, std::string>
ShaderDescriptor::SIterator::operator*() const {
  std::pair<std::string, std::string> rv(
    std::make_pair(this->si->location->first, "(in-memory)")
  );
  if(this->si->location->second == SHADER_VERTEX_DISK ||
     this->si->location->second == SHADER_FRAGMENT_DISK) {
    // load it from disk and replace those parameters.
    rv.first = readfile(this->si->location->first);
    rv.second = this->si->location->first;
  }
  return rv;
}

ShaderDescriptor::SIterator ShaderDescriptor::begin_vertex() const {
  return ShaderDescriptor::SIterator(
    ShaderDescriptor::SIterator::siterinfo(
      this, this->si->vertex.begin(),
      ShaderDescriptor::SIterator::siterinfo::ITER_VERTEX
    )
  );
}
ShaderDescriptor::SIterator ShaderDescriptor::end_vertex() const {
  return ShaderDescriptor::SIterator(
    ShaderDescriptor::SIterator::siterinfo(
      this, this->si->vertex.end(),
      ShaderDescriptor::SIterator::siterinfo::ITER_VERTEX
    )
  );
}

ShaderDescriptor::SIterator ShaderDescriptor::begin_fragment() const {
  return ShaderDescriptor::SIterator(
    ShaderDescriptor::SIterator::siterinfo(
      this, this->si->fragment.begin(),
      ShaderDescriptor::SIterator::siterinfo::ITER_FRAGMENT
    )
  );
}
ShaderDescriptor::SIterator ShaderDescriptor::end_fragment() const {
  return ShaderDescriptor::SIterator(
    ShaderDescriptor::SIterator::siterinfo(
      this, this->si->fragment.end(),
      ShaderDescriptor::SIterator::siterinfo::ITER_FRAGMENT
    )
  );
}

} // namespace tuvok

/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2012 Interactive Visualization and Data Analysis Group.

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
