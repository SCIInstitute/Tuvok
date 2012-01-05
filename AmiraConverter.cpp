/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2011 Scientific Computing and Imaging Institute,
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
#include "AmiraConverter.h"
#include <fstream>
#include <iterator>

// Your standard ostream_iterator will essentially do "stream << *iter".  That
// doesn't work well for binary files, however, in which we need to use
// "stream.write(&*iter, sizeof(T))".  Hence this implements a binary
// ostream_iterator.
template<typename T> class binary_ostream_iterator :
  public std::iterator<std::output_iterator_tag, void, void, void, void> {
public:
  binary_ostream_iterator(std::ostream& os) : stream(os) {}
  binary_ostream_iterator(const binary_ostream_iterator& boi) :
    stream(boi.stream) { }

  binary_ostream_iterator& operator=(const T& value) {
    this->stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
    return *this;
  }
  binary_ostream_iterator& operator*() { return *this; }
  binary_ostream_iterator& operator++() { return *this; }
  binary_ostream_iterator& operator++(int) { return *this; }
  std::output_iterator_tag iterator_category(const binary_ostream_iterator&) {
    return std::output_iterator_tag();
  }

private:
  std::ostream& stream;

private:
};

AmiraConverter::AmiraConverter()
{
  m_vConverterDesc = "Amira";
  m_vSupportedExt.push_back("AM");
}

bool AmiraConverter::CanRead(const std::string& fn,
                               const std::vector<int8_t>& start) const
{
  if(!AbstrConverter::CanRead(fn, start)) {
    MESSAGE("Base class says we can't read it...");
    return false;
  }

  // the file should start with:
  // # AmiraMesh ASCII 1.0
  // if it doesn't we probably don't know how to read it.
  std::vector<int8_t>::const_iterator nl =
    std::find(start.begin(), start.end(), '\n');
  if(nl == start.end()) {
    // no newline found.  This isn't one of our files.
    MESSAGE("No newline near the beginning of the file; not mine.");
    return false;
  }
  std::string firstline(
    static_cast<const signed char*>(&start[0]),
    static_cast<const signed char*>(&*nl)
  );
  if(firstline.find("AmiraMesh") == std::string::npos) {
    MESSAGE("No 'AmiraMesh'... not mine.");
    return false;
  }
  if(firstline.find("ASCII") == std::string::npos) {
    MESSAGE("Not in ASCII format... this might be mine, but I can't read it.");
    return false;
  }

  return true;
}

bool AmiraConverter::ConvertToRAW(const std::string& strSourceFilename,
                                    const std::string& strTempDir,
                                    bool,
                                    uint64_t& iHeaderSkip,
                                    uint64_t& iComponentSize,
                                    uint64_t& iComponentCount,
                                    bool& bConvertEndianness,
                                    bool& bSigned, bool& bIsFloat,
                                    UINT64VECTOR3& vVolumeSize,
                                    FLOATVECTOR3& vVolumeAspect,
                                    std::string& strTitle,
                                    UVFTables::ElementSemanticTable& eType,
                                    std::string& strIntermediateFile,
                                    bool& bDeleteIntermediateFile)
{
  strTitle = "from Amira converter";
  eType = UVFTables::ES_UNDEFINED;

  std::ifstream amira(strSourceFilename.c_str());
  if(!amira) {
    T_ERROR("Could not open %s!", strSourceFilename.c_str());
    return false;
  }

  iHeaderSkip = 0; // we'll create a new, raw file.
  iComponentSize = 64;
  iComponentCount = 1;
  bConvertEndianness = false;
  bSigned = true;
  bIsFloat = true;
  vVolumeAspect = FLOATVECTOR3(1.0, 1.0, 1.0);
  strIntermediateFile = strTempDir + "/" + "am.iv3d.tmp";
  bDeleteIntermediateFile = true;

  std::string junk;
  std::string version;
  amira >> junk >> junk >> junk >> version; // "# AmiraMesh ASCII 1.0"
  MESSAGE("Reading 'AmiraMesh' file, version %s", version.c_str());

  // "define Lattice    X    Y    Z"
  amira >> junk >> junk >> vVolumeSize[0] >> vVolumeSize[1] >> vVolumeSize[2];
  assert(vVolumeSize[0] > 0);
  assert(vVolumeSize[1] > 0);
  assert(vVolumeSize[2] > 0);

  MESSAGE("%llu-bit %llux%llux%llu data.", iComponentSize,
          vVolumeSize[0], vVolumeSize[1], vVolumeSize[2]);

  // The rest of the header is stuff we don't bother with right now, and then:
  //
  //    Lattice { float Data } = @1
  //
  //    @1
  //      first-elem 2nd-elem ...
  //
  // Presumably they could define multiple "Lattice"s and then use @2, @3,
  // etc., but I don't have any such example data files, so screw it.
  // We're just going to read up until that @1.  Then we'll read up until the
  // next @1.  At that point we can just copy each elem into an output file.
  do { amira >> junk; } while(junk != "@1"); // " ... } = @1"
  do { amira >> junk; } while(junk != "@1"); // "@1\n"

  std::ofstream inter(strIntermediateFile.c_str(),
                      std::ofstream::out | std::ofstream::out);
  if(!inter) {
    T_ERROR("Could not create intermediate file '%s'.",
            strIntermediateFile.c_str());
    bDeleteIntermediateFile = false;
    return false;
  }
  std::copy(std::istream_iterator<double>(amira),
            std::istream_iterator<double>(),
            binary_ostream_iterator<double>(inter));
 
  return true;
}

bool AmiraConverter::ConvertToNative(const std::string&,
                                       const std::string&,
                                       uint64_t,
                                       uint64_t,
                                       uint64_t, bool,
                                       bool,
                                       UINT64VECTOR3,
                                       FLOATVECTOR3,
                                       bool,
                                       const bool)
{
  return false;
}
