/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2008 Scientific Computing and Imaging Institute,
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
  \file    FrameCapture.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    November 2008
*/
#pragma once

#ifndef FRAMECAPTURE_H
#define FRAMECAPTURE_H

#include <string>
#include "../StdTuvokDefines.h"
#include "../Basics/Vectors.h"

#include "Controller/Controller.h"
#include "Basics/SysTools.h"
#include "IO/TTIFFWriter/TTIFFWriter.h"

#ifndef TUVOK_NO_QT
# include <QImage>
#endif


namespace tuvok {

class FrameCapture {
  public:
    FrameCapture() {}
    virtual ~FrameCapture() {}

    virtual bool CaptureSingleFrame(const std::string& strFilename,
                                    bool bPreserveTransparency) const = 0;
  private:

#ifndef TUVOK_NO_QT
  static int DemultiplyAlpha(unsigned char cmp,  unsigned char alpha)  {
    return int(0.5f+float(cmp)/(float(alpha)/255.0f));
  }
#endif

    template <typename T>
    static void RGBAToRGB(const std::vector<T>& in, std::vector<T>& out) {
      size_t j = 0;
      for (size_t i = 0;i<in.size();i+=4) {
        out[j++] = in[i+0];
        out[j++] = in[i+1];
        out[j++] = in[i+2];
      }
    }

  protected:

    template <typename T>
    bool SaveImage(const std::string& strFilename, 
                   const UINTVECTOR2& vSize,
                   const std::vector<T>& vInputData, 
                   bool bPreserveTransparency) const {
    
      // OpenGL Data is upside down so first flip it  
      std::vector<T> vData(vInputData.size());
      size_t i = 0;
      for (int y = 0;y<int(vSize.y);y++) {
        int j = int(vSize.x)*4*(int(vSize.y-1) - y);
        for (int x = 0;x<int(vSize.x)*4;x++) {
          vData[i++] = vInputData[j++];
        }
      }

      // capture TIFF files and run our own exporter on them
      std::string extension = SysTools::ToLowerCase(SysTools::GetExt(strFilename));  
      if (extension == "tif" || extension == "tiff") {
        try {
          if (bPreserveTransparency) {
            // our TIFF writer sets the TIFF file-format to "premultiplied-alpha"
            // so there is no need to demultiply here
            TTIFFWriter::Write(strFilename.c_str(), vSize.x, vSize.y, TTIFFWriter::TT_RGBA, vData);
          } else {
            std::vector<T> vDataRGB(vSize.area()*3);
            RGBAToRGB(vData, vDataRGB);
            TTIFFWriter::Write(strFilename.c_str(), vSize.x, vSize.y, TTIFFWriter::TT_RGB, vDataRGB);
          }
        } catch (const ttiff_error& e) {
          T_ERROR("Unable to save image %s: %s.",strFilename.c_str(), e.what());
          return false;
        }
        return true;
      } else {
        #ifdef TUVOK_NO_QT
            T_ERROR("Refusing to save image %s: Tuvok was compiled without Qt support so export is restricted to TIFF files.",
                    strFilename.c_str());
            return false;
        #else
          if (sizeof(T) > 1) {
            T_ERROR("Unable to save high precision data to image %s please select TIFF fromat.",strFilename.c_str());
            return false;
          }

          QImage qTargetFile(QSize(vSize.x, vSize.y), QImage::Format_ARGB32);

          size_t i = 0;
          if (bPreserveTransparency) {
            for (size_t y = 0;y<vSize.y;y++) {
              for (size_t x = 0;x<vSize.x;x++) {
                qTargetFile.setPixel(int(x),int(y),
                                      qRgba(DemultiplyAlpha(vData[i+0],vData[i+3]),
                                            DemultiplyAlpha(vData[i+1],vData[i+3]),
                                            DemultiplyAlpha(vData[i+2],vData[i+3]),
                                            int(vData[i+3])));
                i+=4;
              }
            }
          } else {
            for (size_t y = 0;y<vSize.y;y++) {
              for (size_t x = 0;x<vSize.x;x++) {
                qTargetFile.setPixel(int(x),int(y),
                                    qRgba(int(vData[i+0]),
                                          int(vData[i+1]),
                                          int(vData[i+2]),
                                          255));
                i+=4;
              }
            }
          }

          return qTargetFile.save(strFilename.c_str());
        #endif 
     }
    }
  };
};
#endif // FRAMECAPTURE_H
