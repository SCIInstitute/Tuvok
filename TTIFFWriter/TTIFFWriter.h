#ifndef TTIFFWRITER
#define TTIFFWRITER

#include "StdTuvokDefines.h"
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <stdexcept>

class ttiff_error : public std::runtime_error {
public:
  ttiff_error(const std::string& m) : std::runtime_error(m) { }
};

class TTIFFWriter {
public:
  
  enum TTDataType {
    TT_RGB,
    TT_RGBA
  };
    
  static void Write(const std::string& filename,
                    uint32_t width, uint32_t height, TTDataType dataType,
                    const std::vector<uint8_t>& data);
  static void Write(const std::string& filename,
                    uint32_t width, uint32_t height, TTDataType dataType,
                    const std::vector<uint16_t>& data);
  static void Write(const std::string& filename,
                    uint32_t width, uint32_t height, TTDataType dataType,
                    const std::vector<uint32_t>& data);
  static void Write(const std::string& filename,
                    uint32_t width, uint32_t height, TTDataType dataType,
                    const std::vector<float>& data);
  
private:
  static void WriteHeader(std::ofstream& file);
  static void WriteIDF(std::ofstream& file, uint32_t width, uint32_t height,
                       uint16_t bytesPerComponent, TTDataType dataType,
                       bool isFloat);
  
  template <typename T>
  static bool VerifyVector(uint32_t width, uint32_t height,
                           TTDataType dataType,
                           const std::vector<T>& data) {
    return data.size() >= width*height*TypeToSize(dataType);
  }
  
  template <typename T>
  static  void WriteData(std::ofstream& file,
                         uint32_t width, uint32_t height,
                         TTDataType dataType,
                         const std::vector<T>& data) {
    size_t iElementCount = width*height*TypeToSize(dataType);
    file.write((const char*)&(data[0]), iElementCount*sizeof(T));
  }
  
  static size_t TypeToSize(TTDataType dataType);
  
};

#endif // TTIFFWRITER

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
