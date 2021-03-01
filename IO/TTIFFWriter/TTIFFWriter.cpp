#include "TTIFFWriter.h"
#include "Basics/SysTools.h"

void TTIFFWriter::Write(const std::wstring& filename,
                        uint32_t width, uint32_t height, TTDataType dataType,
                        const std::vector<uint8_t>& data) {
  if (!VerifyVector(width, height, dataType, data))
    throw ttiff_error("Data vector too small");
  
  std::ofstream file;
  file.open(SysTools::toNarrow(filename).c_str(), std::ios::binary);
  if (file.bad()) throw ttiff_error("Unable to open file");
  WriteHeader(file);
  WriteIDF(file, width, height, 1, dataType, false);
  WriteData(file, width, height, dataType, data);
}

void TTIFFWriter::Write(const std::wstring& filename,
                        uint32_t width, uint32_t height, TTDataType dataType,
                        const std::vector<uint16_t>& data) {
  if (!VerifyVector(width, height, dataType, data))
    throw ttiff_error("Data vector too small");
  
  std::ofstream file;
  file.open(SysTools::toNarrow(filename).c_str(), std::ios::binary);
  if (file.bad()) throw ttiff_error("Unable to open file");
  WriteHeader(file);
  WriteIDF(file, width, height, 2, dataType, false);
  WriteData(file, width, height, dataType, data);
}

void TTIFFWriter::Write(const std::wstring& filename,
                        uint32_t width, uint32_t height, TTDataType dataType,
                        const std::vector<uint32_t>& data) {
  if (!VerifyVector(width, height, dataType, data))
    throw ttiff_error("Data vector too small");
  
  std::ofstream file;
  file.open(SysTools::toNarrow(filename).c_str(), std::ios::binary);
  if (file.bad()) throw ttiff_error("Unable to open file");
  WriteHeader(file);
  WriteIDF(file, width, height, 4, dataType, false);
  WriteData(file, width, height, dataType, data);
}

void TTIFFWriter::Write(const std::wstring& filename,
                        uint32_t width, uint32_t height, TTDataType dataType,
                        const std::vector<float>& data) {
  if (!VerifyVector(width, height, dataType, data))
    throw ttiff_error("Data vector too small");
  
  std::ofstream file;
  file.open(SysTools::toNarrow(filename).c_str(), std::ios::binary);
  if (file.bad()) throw ttiff_error("Unable to open file");
  WriteHeader(file);
  WriteIDF(file, width, height, 4, dataType, true);
  WriteData(file, width, height, dataType, data);
}

void TTIFFWriter::WriteHeader(std::ofstream& file) {

  uint32_t offset = 8;
  uint32_t magic = uint32_t(0x002a4949);
  
  file.write((const char*)&magic, 4);
  file.write((const char*)&offset, 4);
}


class TagItem {
public:  
  TagItem(uint16_t _tag) : m_tag(_tag) {}

  TagItem(uint16_t _tag, uint16_t data) : m_tag(_tag)
  {m_data16.push_back(data);}
  
  TagItem(uint16_t _tag, uint32_t data) : m_tag(_tag)
  {m_data32.push_back(data);}
  
  TagItem(uint16_t _tag, const std::vector<uint16_t>& data) :
  m_tag(_tag),  m_data16(data) {}
  
  TagItem(uint16_t _tag, const std::vector<uint32_t>& data) :
  m_tag(_tag), m_data32(data) {}
  
  uint16_t GetAdditionalOffset() {    
    if (m_data16.size() > 1 || m_data32.size() > 1)
      return uint16_t(m_data16.size()*2 + m_data32.size()*4);
    else
      return 0;
  }
  
  
  void Write(std::ofstream& file, uint16_t iCurrentOffset,
             uint16_t iTotalOffset) {
    if (m_data16.size() == 1) {
      WriteTag(file, m_tag, 1, m_data16[0]);
      return;
    }
    if (m_data32.size() == 1) {
      WriteTag(file, m_tag, 1, m_data32[0]);
      return;      
    }    
    if (m_data16.size() > 1) {
      WriteTag(file, m_tag, uint32_t(m_data16.size()), iCurrentOffset);
      return;
    }
    if (m_data32.size() > 1) {
      WriteTag(file, m_tag, uint32_t(m_data32.size()), iCurrentOffset);
      return;
    }
    
    // special case: write the offset as data (normally used for image location)
    WriteTag(file, m_tag, 1, iTotalOffset);
  }

  void WriteVectorData(std::ofstream& file) {
    if (m_data16.size() > 1) {
      for (std::vector<uint16_t>::iterator elem = m_data16.begin();
           elem<m_data16.end();elem++) {
        file.write((const char*)&(*elem), 2);
      }
    }
    if (m_data32.size() > 1) {
      for (std::vector<uint32_t>::iterator elem = m_data32.begin();
           elem<m_data32.end();elem++) {
        file.write((const char*)&(*elem), 4);
      }      
    }      
  }
  
private:
  uint16_t m_tag;
  std::vector<uint16_t> m_data16;
  std::vector<uint32_t> m_data32;
  
  void WriteTag(std::ofstream& file, uint16_t tag, uint32_t size, uint16_t data) {
    file.write((const char*)&tag, 2);
    uint16_t type = 3; // 16bit int
    file.write((const char*)&type, 2);
    file.write((const char*)&size, 4);
    uint32_t elem = data;
    file.write((const char*)&elem, 4);    
  }
  
  void WriteTag(std::ofstream& file, uint16_t tag, uint32_t size, uint32_t data) {
    file.write((const char*)&tag, 2);
    uint16_t type = 4; // 32bit int
    file.write((const char*)&type, 2);
    file.write((const char*)&size, 4);
    file.write((const char*)&data, 4);  
  }
  
};

class TagVector {
public:
  TagVector() {}
  void add(const TagItem& tag) {
    tags.push_back(tag);
  }
  
  void Write(std::ofstream& file, uint16_t iBaseOffset) {
    // write footer elem count
    uint16_t count = uint16_t(tags.size());
    file.write((const char*)&count, 2);
    
    iBaseOffset += 2         // footer elem count
                 + 12*count  // footer tags
                 + 4;        // footer termination;
    
    
    uint16_t iCurrentOffset = iBaseOffset;
    uint16_t iTotalOffset   = iBaseOffset;

    
    // compute total offset to find image data start
    for (std::vector<TagItem>::iterator tag = tags.begin();
         tag<tags.end();++tag) {
      iTotalOffset += tag->GetAdditionalOffset();
    }
    
    // write tags
    for (std::vector<TagItem>::iterator tag = tags.begin();
         tag<tags.end();++tag) {
      tag->Write(file, iCurrentOffset, iTotalOffset);
      iCurrentOffset += tag->GetAdditionalOffset();
    }
    
    // footer termination;
    uint32_t term = 0; // this is the one and only image plane so terminate
    file.write((const char*)&term, 4);
    
    // write tag's vector data
    for (std::vector<TagItem>::iterator tag = tags.begin();
         tag<tags.end();++tag) {
      tag->WriteVectorData(file);
    }
    
  }
  
private:
  std::vector<TagItem> tags;

};


void TTIFFWriter::WriteIDF(std::ofstream& file, uint32_t width,
                           uint32_t height, uint16_t bytesPerComponent,
                           TTDataType dataType, bool bIsFloat) {
  // for details check out:
  // http://www.awaresystems.be/imaging/tiff/tifftags/baseline.html
  
  TagVector tags;
  tags.add(TagItem(256, width));
  tags.add(TagItem(257, height));

  std::vector<uint16_t> bpp;
  bpp.push_back(bytesPerComponent*8);
  bpp.push_back(bytesPerComponent*8);
  bpp.push_back(bytesPerComponent*8);
  if (dataType == TT_RGBA)
    bpp.push_back(bytesPerComponent*8);
  tags.add(TagItem(258, bpp)); // vector tag bytesPerComponent
  
  tags.add(TagItem(259, uint16_t(1))); // no compression
  tags.add(TagItem(262, uint16_t(2))); // 2 = RGB (even for RGBA !!!)
  tags.add(TagItem(273)); // offset to data
  tags.add(TagItem(274, uint16_t(1))); // default orientation
  tags.add(TagItem(277, uint16_t(TypeToSize(dataType)))); // SamplesPerPixel
  tags.add(TagItem(278, height)); // Rows per strip
  uint32_t stripByteCount = uint32_t(width*height*bytesPerComponent*TypeToSize(dataType));
  
  tags.add(TagItem(279, stripByteCount)); // Strip Byte Counts
  tags.add(TagItem(284, uint16_t(1))); // Planar configuration
  
  if (dataType == TT_RGBA)
    tags.add(TagItem(338, uint16_t(1))); // 4th channel is alpha

  uint16_t iFormat = (bIsFloat) ? 3 : 1; // sample format (1=int, 3=float)
  std::vector<uint16_t> sampleFormat; 
  sampleFormat.push_back(iFormat);
  sampleFormat.push_back(iFormat);
  sampleFormat.push_back(iFormat);
  if (dataType == TT_RGBA)
    sampleFormat.push_back(iFormat);
  tags.add(TagItem(339, sampleFormat)); // vector tag bytesPerComponent   
  tags.Write(file, 8);
}


size_t TTIFFWriter::TypeToSize(TTDataType dataType) {
  switch (dataType) {
    case TT_RGB:
      return 3;
    case TT_RGBA:
      return 4;
    default :
      return 0;
  }
}
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
