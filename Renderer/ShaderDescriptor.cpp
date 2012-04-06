#include "StdTuvokDefines.h"
#if defined(_MSC_VER)
# include <functional>
#else
# include <tr1/functional>
#endif
#include <algorithm>
#include <cstdarg>
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
  if (SysTools::FileExists(SysTools::GetFromResourceOnMac(file))) {
    file = SysTools::GetFromResourceOnMac(file);
    MESSAGE("Found %s in bundle, using that.", file.c_str());
    return file;
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

// temporary hack -- make an iterator instead
std::vector<std::string> ShaderDescriptor::GetVertexShaders() const
{
  std::vector<std::string> rv;
  typedef std::vector<std::pair<std::string, enum shader_type> > sv;
  for(sv::const_iterator v = this->si->vertex.begin();
      v != this->si->vertex.end(); ++v) {
    rv.push_back(v->first);
  }
  return rv;
}

// temporary hack -- make an iterator instead
std::vector<std::string> ShaderDescriptor::GetFragmentShaders() const
{
  std::vector<std::string> rv;
  typedef std::vector<std::pair<std::string, enum shader_type> > sv;
  for(sv::const_iterator f = this->si->fragment.begin();
      f != this->si->fragment.end(); ++f) {
    rv.push_back(f->first);
  }
  return rv;
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

