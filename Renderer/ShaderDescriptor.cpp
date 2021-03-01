#include "StdTuvokDefines.h"
#include <algorithm>
#include <cstdarg>
#include <fstream>
#include <functional>
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
  std::vector<std::string> defines;
  std::vector<std::pair<std::wstring, enum shader_type>> vertex;
  std::vector<std::pair<std::wstring, enum shader_type>> fragment;
  bool operator==(const ShaderDescriptor::sinfo& sdi) const;
};
bool ShaderDescriptor::sinfo::operator==(const ShaderDescriptor::sinfo& sdi)
const {
  return vertex.size() == sdi.vertex.size() &&
         fragment.size() == sdi.fragment.size() &&
         std::equal(vertex.begin(), vertex.end(), sdi.vertex.begin()) &&
         std::equal(fragment.begin(), fragment.end(), sdi.fragment.begin()) &&
         std::equal(defines.begin(), defines.end(), sdi.defines.begin());
}

ShaderDescriptor::ShaderDescriptor() : si(new struct sinfo()) { }
ShaderDescriptor::ShaderDescriptor(const ShaderDescriptor& sd) : si(sd.si) {}
ShaderDescriptor::ShaderDescriptor(const std::vector<std::wstring>& vertex,
                                   const std::vector<std::wstring>& fragment) :
  si(new struct sinfo())
{
  typedef std::vector<std::wstring> sv;
  for(sv::const_iterator v = vertex.cbegin(); v != vertex.cend(); ++v) {
    this->si->vertex.push_back(std::make_pair(*v, SHADER_VERTEX_DISK));
  }
  for(sv::const_iterator f = fragment.cbegin(); f != fragment.cend(); ++f) {
    this->si->fragment.push_back(std::make_pair(*f, SHADER_FRAGMENT_DISK));
  }
}

// SysTools::FileExists can take a std::string OR a std::wstring.  This makes
// it hard to use in a function composition, because the compiler cannot figure
// out which one we want, and it's not a template or anything so we cannot just
// be explicit.
// This serves to rename it to avoid the ambiguity.
static bool exists(std::wstring s) { return SysTools::FileExists(s); }
// we could technically achieve this by composing std::plus with
// std::plus, but my god is that a nightmare in c++03.
static std::wstring concat(std::wstring a, std::wstring b, std::wstring c) {
  return a + b + c;
}

/// expects a list of directories (filenames).  Removes any from the
/// list which don't exist.
static std::vector<std::wstring> existing(std::vector<std::wstring> directories)
{
  typedef std::vector<std::wstring> sv;
  sv::iterator end = std::remove_if(directories.begin(), directories.end(),
                                    std::not1(std::ptr_fun(exists)));
  for(sv::const_iterator e = end; e != directories.end(); ++e) {
    if (!e->empty())
      WARNING("Directory %s does not exist!", SysTools::toNarrow(*e).c_str());
  }
  // also, we know they're junk, so don't search in them
  directories.erase(end, directories.end());
  return directories;
}

// Searches for the given filename in the given directories.  Returns the fully
// qualified path of the file's location.
static std::wstring find_filename(const std::vector<std::wstring>& directories,
                                 std::wstring filename)
{
  // if we're on Mac, first try to see if the file is in our bundle.
#ifdef DETECTED_OS_APPLE
  if (SysTools::FileExists(SysTools::GetFromResourceOnMac(filename))) {
    filename = SysTools::GetFromResourceOnMac(filename);
    MESSAGE("Found %s in bundle, using that.", SysTools::toNarrow(filename).c_str());
    return filename;
  }
#endif

  typedef std::vector<std::wstring> sv;
  // okay, now prepend each directory into our flename and see if we find a
  // match.
  using namespace std::placeholders;
  const std::wstring dirsep = L"/";
  // the functor is a composition: 'exists(add(_1, dirsep, filename))'
  sv::const_iterator fn =
    std::find_if(directories.begin(), directories.end(),
                 std::bind(
                   std::ptr_fun(exists),
                   std::bind(concat, _1, dirsep, filename)
                 ));

  if(fn == directories.end()) { // file not found.
    throw std::runtime_error(std::string("could not find file ") + SysTools::toNarrow(filename));
  }
  return *fn + dirsep + filename;
}

std::vector<std::wstring>
ShaderDescriptor::ValidPaths(const std::vector<std::wstring>& dirs) {
  return existing(dirs);
}

ShaderDescriptor ShaderDescriptor::Create(
  std::vector<std::wstring> directories, 
  std::vector<std::pair<uint32_t, std::string>> fragmentDataBindings,
  ...
) {
  ShaderDescriptor rv;
  va_list args;
  va_start(args, fragmentDataBindings);

  const wchar_t* filename;
  // we expect two NULLs: first terminates vertex list, second fragment list.
  do {
    filename = va_arg(args, const wchar_t*);
    if(filename != nullptr) {
      rv.si->vertex.push_back(std::make_pair(std::wstring(filename),
                              SHADER_VERTEX_DISK));
    }
  } while(filename != nullptr);

  // now second: fragment shaders.
  do {
    filename = va_arg(args, const wchar_t*);
    if(filename != nullptr) {
      rv.si->fragment.push_back(std::make_pair(std::wstring(filename),
                                SHADER_FRAGMENT_DISK));
    }
  } while(filename != nullptr);
  va_end(args);

  // now try to clean up all those paths.
  // The user gave us some directories to search, but let's make sure we also
  // search the location of our binary.
  std::vector<std::wstring> dirs = SysTools::GetSubDirList(
    Controller::ConstInstance().SysInfo().GetProgramPath()
  );
  directories.insert(directories.end(), dirs.begin(), dirs.end());
  directories.push_back(Controller::ConstInstance().SysInfo().GetProgramPath());
  directories = existing(directories); // prune bad directories
    
  typedef std::vector<std::pair<std::wstring, enum shader_type>> sv;
  for(sv::iterator v = rv.si->vertex.begin(); v != rv.si->vertex.end(); ++v) {
    v->first = find_filename(directories, v->first);
  }
  for(sv::iterator f = rv.si->fragment.begin(); f != rv.si->fragment.end();
      ++f) {
    f->first = find_filename(directories, f->first);
  }

  rv.fragmentDataBindings = fragmentDataBindings;

  return rv;
}
ShaderDescriptor ShaderDescriptor::Create(
  std::vector<std::wstring> directories, ...
) {
  ShaderDescriptor rv;
  va_list args;
  va_start(args, directories);

  const wchar_t* filename;
  // we expect two NULLs: first terminates vertex list, second fragment list.
  do {
    filename = va_arg(args, const wchar_t*);
    if(filename != NULL) {
      rv.si->vertex.push_back(std::make_pair(std::wstring(filename),
                              SHADER_VERTEX_DISK));
    }
  } while(filename != NULL);

  // now second: fragment shaders.
  do {
    filename = va_arg(args, const wchar_t*);
    if(filename != NULL) {
      rv.si->fragment.push_back(std::make_pair(std::wstring(filename),
                                SHADER_FRAGMENT_DISK));
    }
  } while(filename != NULL);
  va_end(args);

  // now try to clean up all those paths.
  // The user gave us some directories to search, but let's make sure we also
  // search the location of our binary.
  std::vector<std::wstring> dirs = SysTools::GetSubDirList(
    Controller::ConstInstance().SysInfo().GetProgramPath()
  );
  directories.insert(directories.end(), dirs.begin(), dirs.end());
  directories.push_back(Controller::ConstInstance().SysInfo().GetProgramPath());
  directories = existing(directories); // prune bad directories
    
  typedef std::vector<std::pair<std::wstring, enum shader_type>> sv;
  for(sv::iterator v = rv.si->vertex.begin(); v != rv.si->vertex.end(); ++v) {
    v->first = find_filename(directories, v->first);
  }
  for(sv::iterator f = rv.si->fragment.begin(); f != rv.si->fragment.end();
      ++f) {
    f->first = find_filename(directories, f->first);
  }

  return rv;
}

void ShaderDescriptor::AddDefine(const std::string& define) {
  this->si->defines.push_back(define);
}

void ShaderDescriptor::AddDefines(const std::vector<std::string>& defines) {
  for (auto define = defines.cbegin(); define != defines.cend(); define++)
    AddDefine(*define);
}

/// Adds a vertex shader in a string (i.e. not from a filename)
void ShaderDescriptor::AddVertexShaderString(const std::string& shader) {
  this->si->vertex.push_back(std::make_pair(SysTools::toWide(shader), SHADER_VERTEX_STRING));
}

/// Adds a fragment shader in a string (i.e. not from a filename)
void ShaderDescriptor::AddFragmentShaderString(const std::string& shader) {
  this->si->fragment.push_back(std::make_pair(SysTools::toWide(shader), SHADER_FRAGMENT_STRING));
}

/// Two shaders are equal if they utilize the same set of filenames/strings
/// to compose the shader.
bool ShaderDescriptor::operator ==(const ShaderDescriptor& sd) const
{
  return this->si == sd.si;
}

static std::string readfile(const std::wstring& filename) {
  // open in append mode so the file pointer will be at EOF and we can
  // therefore easily/quickly figure out the file size.
  std::ifstream ifs(SysTools::toNarrow(filename).c_str(), std::ios::in | std::ios::ate);
  if(!ifs.is_open()) {
    T_ERROR("Could not open shader '%s'", SysTools::toNarrow(filename).c_str());
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

typedef std::vector<std::pair<std::wstring, enum shader_type>> slist;
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
  siterinfo& operator=(const siterinfo& other) {
      this->sd = other.sd;
      this->location = other.location;
      this->vf = other.vf;
      return *this;
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

static std::string
vectorStringToString(std::vector<std::string> const& vs) {
  std::string defines;
  for (auto define = vs.cbegin(); define != vs.cend(); define++) {
    defines.append(*define + "\n");
  }
  return defines;
}


size_t findNextLineAfterVersionDirective(std::string const& source) {
  std::string const version = "#version";
  size_t pos = source.find(version);
  if (pos != std::string::npos) {
    pos = source.find("\n", pos);
    if (pos != std::string::npos) {
      pos += 1; // get next position where to insert our defines
    }
    else
      pos = 0;
  }
  else
    pos = 0;
  return pos;
}

std::string includeDefines(std::string const& defines, std::string const& source) {
  size_t pos = findNextLineAfterVersionDirective(source);

  // no #version directive found, proceed as usual
  if (pos == 0) {
    return defines + source;
  }

  std::string modifiedSource = source.substr(0, pos);
  modifiedSource += defines;
  modifiedSource += source.substr(pos);
  return modifiedSource;
}

std::pair<std::string, std::wstring>
ShaderDescriptor::SIterator::operator*() const {
  // #version is required to be the first statement in a shader file
  // and may not be repeated
  // thus we need to set all defines after this statement
  const std::string defines = vectorStringToString(this->si->sd->si->defines);

  std::pair<std::string, std::wstring> rv;

  if (this->si->location->second == SHADER_VERTEX_DISK ||
      this->si->location->second == SHADER_FRAGMENT_DISK) {
    // load it from disk and replace those parameters.
    rv.first = includeDefines(defines, readfile(this->si->location->first));
    rv.second = this->si->location->first;
  } else {
    std::string source = includeDefines(defines, SysTools::toNarrow(this->si->location->first));
    rv = std::make_pair(source, L"(in-memory)");
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
