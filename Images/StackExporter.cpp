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

/**
  \file    StackExporter.cpp
  \author    Jens Krueger
        SCI Institute
        University of Utah
*/
#include "StdTuvokDefines.h"
#include <memory>

#ifndef TUVOK_NO_QT
 #include <QtGui/QImage>
 #include <QtGui/QImageWriter>
#endif

#include "StackExporter.h"
#include "Basics/SysTools.h"
#include "Basics/LargeRAWFile.h"
#include "Basics/nonstd.h"
#include "Controller/Controller.h"


std::vector<std::pair<std::string,std::string>> StackExporter::GetSuportedImageFormats() {
  std::vector<std::pair<std::string,std::string>> formats;

  formats.push_back(std::make_pair("raw","RAW RGBA file without header information"));

#ifndef TUVOK_NO_QT
  QList<QByteArray> listImageFormats = QImageWriter::supportedImageFormats();
  for (int i = 0;i<listImageFormats.size();i++) {
    QByteArray imageFormat = listImageFormats[i];
    QString qStrImageFormat(imageFormat);
    formats.push_back(std::make_pair(qStrImageFormat.toStdString(),"Qt Image Format"));
  }
#endif

  return formats;
}

bool StackExporter::WriteImage(unsigned char* pData,
                               const std::string& strFilename,
                               const UINT64VECTOR2& vSize,
                               uint64_t iComponentCount) {

  if (iComponentCount != 4 && iComponentCount != 3) return false;
  
  if (SysTools::ToLowerCase(SysTools::GetExt(strFilename)) == "raw") {
    LargeRAWFile out(strFilename); 
    if (!out.Create()) return false;
    out.WriteRAW(pData, vSize.area()*iComponentCount);
    out.Close();
  }

#ifndef TUVOK_NO_QT
  QImage qTargetFile(QSize(int(vSize.x), int(vSize.y)), iComponentCount == 4 ? QImage::Format_ARGB32 : QImage::Format_RGB32);

  size_t i = 0;
  if (iComponentCount == 4) {
    for (int y = 0;y<int(vSize.y);y++) {
      for (int x = 0;x<int(vSize.x);x++) {
        qTargetFile.setPixel(x,y,
                              qRgba(int(pData[i+0]),
                                    int(pData[i+1]),
                                    int(pData[i+2]),
                                    int(pData[i+3])));
        i+=4;
      }
    }
  } else {
    for (int y = 0;y<int(vSize.y);y++) {
      for (int x = 0;x<int(vSize.x);x++) {
        qTargetFile.setPixel(x,y,
                              qRgb(int(pData[i+0]),
                                    int(pData[i+1]),
                                    int(pData[i+2])));
        i+=3;
      }
    }
  }

  return qTargetFile.save(strFilename.c_str());
#else
  return false;
#endif

}


bool StackExporter::WriteSlice(unsigned char* pData,
                               const TransferFunction1D* pTrans,
                               uint64_t iBitWidth,
                               const std::string& strCurrentDiFilename,
                               const UINT64VECTOR2& vSize,
                               float fRescale,
                               uint64_t iComponentCount) {
    size_t iImageCompCount = (iComponentCount == 3 || iComponentCount == 2) ? 3 : 4;

    using namespace boost; // for uintXX_t types.
    switch (iComponentCount)  {
      case 1 : switch (iBitWidth)  {
                case 8 : ApplyTFInplace(pData, vSize, fRescale, pTrans); break;
                case 16 : ApplyTFInplace(reinterpret_cast<uint16_t*>(pData),
                                         vSize, fRescale, pTrans); break;
                case 32 : ApplyTFInplace(reinterpret_cast<uint32_t*>(pData),
                                         vSize, fRescale, pTrans); break;
                default : return false; 
              }
              break;
      case 2 : PadInplace(pData, vSize, 3, 1, 0); break;
      default : break; // all other cases (RGB & RGBA) are written as they are
    }

    // write data to disk
    std::string strCurrentTargetFilename = SysTools::FindNextSequenceName(strCurrentDiFilename);

    return WriteImage(pData,strCurrentTargetFilename, vSize, iImageCompCount);
}


void StackExporter::PadInplace(unsigned char* pData,
                               UINT64VECTOR2 vSize,
                               unsigned int iStepping,
                               unsigned int iPadcount,
                               unsigned char iValue) {
  assert(iStepping > iPadcount) ;

  for (size_t i = 0;i<vSize.area();++i) {
     size_t sourcePos = (iStepping - iPadcount) * (vSize.area()-(i+1));
     size_t targetPos = iStepping * (vSize.area()-(i+1));

     for (unsigned int j = 0;j<iStepping;++j) {

       if ( j < iStepping - iPadcount)
         pData[targetPos] = pData[sourcePos];
       else
         pData[targetPos] = iValue;

       targetPos++;
       sourcePos++;
     }
     
  }


}


bool StackExporter::WriteStacks(const std::string& strRAWFilename, 
                                const std::string& strTargetFilename,
                                const TransferFunction1D* pTrans,
                                uint64_t iBitWidth,
                                uint64_t iComponentCount,
                                float fRescale,
                                UINT64VECTOR3 vDomainSize,
                                bool bAllDirs) {
  if (iComponentCount > 4)  {
    T_ERROR("Invalid channel count, no more than four components are accepted by the stack exporter.");
    return false;
  }
  
  size_t iDataByteWith = size_t(iBitWidth/8);

  // convert to 8bit for more than 1 comp data
  if (iBitWidth != 8 && iComponentCount > 1) {
    T_ERROR("Invalid bit depth, only 8bit data is accepted by the stack exporter for multi channel data.");
    return false;
/*
    LargeRAWFile quantizeDataSource(strRAWFilename);
    if (!quantizeDataSource.Open(true)) return false;

        // TODO quantize

    quantizeDataSource.Close();
    iDataByteWith = 1;
*/
  }


  LargeRAWFile dataSource(strRAWFilename);
  if (!dataSource.Open()) return false;

  size_t iMaxPair = size_t((vDomainSize.x <= vDomainSize.y && vDomainSize.x <= vDomainSize.z) ? vDomainSize.z * vDomainSize.y : (
                           (vDomainSize.y <= vDomainSize.x && vDomainSize.y <= vDomainSize.z) ? vDomainSize.x * vDomainSize.z : vDomainSize.x * vDomainSize.y));


  std::shared_ptr<unsigned char> data(
    new unsigned char[4*iMaxPair*iDataByteWith],
    nonstd::DeleteArray<unsigned char>()
  );

  UINT64VECTOR2 vSize(vDomainSize.z, vDomainSize.y);
  std::string strCurrentDirTargetFilename = SysTools::AppendFilename(strTargetFilename, "_x");
  size_t elemSize = iComponentCount*iDataByteWith;

  if (bAllDirs)  {
    for (uint64_t x = 0;x<vDomainSize.x;x++) {
      MESSAGE("Exporting X-Axis Stack. Processing Image %llu of %llu", x+1, vDomainSize.x);

      uint64_t offset = 0;
      for (uint64_t v = 0;v<vDomainSize.y;v++) {
        for (uint64_t u = 0;u<vDomainSize.z;u++) {
          dataSource.SeekPos(elemSize * (x+u*vDomainSize.x*vDomainSize.y+v*vDomainSize.x) );
          dataSource.ReadRAW(data.get()+offset, elemSize);
          offset += elemSize;
        }
      }

      if (!WriteSlice(data.get(), pTrans, iBitWidth, strCurrentDirTargetFilename, vSize, fRescale, iComponentCount)) {
        T_ERROR("Unable to write stack image %llu.",x);
        dataSource.Close();
      }
    }

    vSize = UINT64VECTOR2(vDomainSize.x, vDomainSize.z);
    strCurrentDirTargetFilename = SysTools::AppendFilename(strTargetFilename, "_y");
    for (uint64_t y = 0;y<vDomainSize.y;y++) {
      MESSAGE("Exporting Y-Axis Stack. Processing Image %llu of %llu", y+1, vDomainSize.y);

      uint64_t offset = 0;
      for (uint64_t u = 0;u<vDomainSize.z;u++) {
        dataSource.SeekPos(elemSize * (y*vDomainSize.x+u*vDomainSize.x*vDomainSize.y) );
        dataSource.ReadRAW(data.get()+offset, vDomainSize.x*elemSize);
        offset += vDomainSize.x*elemSize;
      }

      if (!WriteSlice(data.get(), pTrans, iBitWidth, strCurrentDirTargetFilename, vSize, fRescale, iComponentCount)) {
        T_ERROR("Unable to write stack image %llu.",y);
        dataSource.Close();
      }
    }

    dataSource.SeekPos(0);
    strCurrentDirTargetFilename = SysTools::AppendFilename(strTargetFilename, "_z");
  } else {
    strCurrentDirTargetFilename = strTargetFilename;
  }

  vSize = UINT64VECTOR2(vDomainSize.x, vDomainSize.y);
  for (uint64_t z = 0;z<vDomainSize.z;z++) {
    MESSAGE("Exporting Z-Axis Stack. Processing Image %llu of %llu", z+1, vDomainSize.z);

    dataSource.ReadRAW(data.get(), vDomainSize.x*vDomainSize.y*elemSize);

    if (!WriteSlice(data.get(), pTrans, iBitWidth, strCurrentDirTargetFilename, vSize, fRescale, iComponentCount)) {
      T_ERROR("Unable to write stack image %llu.",z);
      dataSource.Close();
    }
  }

  dataSource.Close();
  return true;
}
