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
  \file    Appendix.cpp
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    January 2009
*/

#pragma once

#ifndef APPENDIX_H
#define APPENDIX_H

#include <vector>
#include <string>
#include "StdDefines.h"

/** \class FileInfo */
class FileInfo {
public:
  FileInfo(std::string strName, uint64_t iSize) :
    m_strName(strName),
    m_iSize(iSize) {}

    std::string m_strName;
    uint64_t m_iSize;
};

class InternalFileInfo : public FileInfo {
public:
  InternalFileInfo(std::string strName, uint64_t iSize, uint64_t iOffset) :
    FileInfo(strName, iSize),
    m_iOffset(iOffset) {}

    uint64_t m_iOffset;
};

/** \class Appendix */
class Appendix
{
  public:
    Appendix(std::string strTarget, const std::vector<std::string>& vstrSource);
    Appendix(std::string strAPXFile);

    bool IsOK() const {return m_bOK;}
    bool ExtractFile(size_t i, std::string strTarget);

    std::vector<FileInfo> ListFiles();

  protected:
    std::string                   m_strAPXFile;
    uint64_t                      m_iHeaderLength;
    bool                          m_bOK;
    std::vector<InternalFileInfo> m_vHeaderData;

};

#endif // APPENDIX_H
