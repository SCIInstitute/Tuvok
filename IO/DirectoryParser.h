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
  \file    DirectoryParser.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    September 2008
*/
#pragma once

#ifndef DIRECTORYPARSER_H
#define DIRECTORYPARSER_H

#include <fstream>
#include <vector>
#include <string>
#include "StdTuvokDefines.h"
#include "Basics/Vectors.h"

class SimpleFileInfo {
public:
  SimpleFileInfo();
  SimpleFileInfo(const std::string& strFileName);
  SimpleFileInfo(const std::wstring& wstrFileName);
  SimpleFileInfo(const SimpleFileInfo* info);
  virtual ~SimpleFileInfo() {}

  std::string   m_strFileName;
  std::wstring  m_wstrFileName;
  uint32_t        m_iImageIndex;

  uint32_t GetDataSize() const {
    return m_iDataSize; /* = m_iComponentCount*m_ivSize.volume()*m_iAllocated/8; */
  }
  virtual uint32_t GetComponentCount() const = 0;

  virtual bool GetData(std::vector<char>&);
  virtual bool GetData(std::vector<char>&, uint32_t iLength, uint32_t iOffset) = 0;
  virtual SimpleFileInfo* clone() = 0;

protected:
  uint32_t m_iDataSize;
};


class FileStackInfo {
public:
  FileStackInfo();
  FileStackInfo(const FileStackInfo* other);
  FileStackInfo(UINTVECTOR3 ivSize, FLOATVECTOR3 fvfAspect, 
                uint32_t iAllocated, uint32_t iStored,
                uint32_t iComponentCount, bool bSigned, bool bIsBigEndian, 
                bool bIsJPEGEncoded, const std::string& strDesc, 
                const std::string& strFileType);
  virtual ~FileStackInfo();

  std::vector<SimpleFileInfo*>  m_Elements;

  UINTVECTOR3  m_ivSize;
  FLOATVECTOR3 m_fvfAspect;
  uint32_t       m_iAllocated;
  uint32_t       m_iStored;
  /// @todo get rid of this, read it from the first file in the sequence
  /// instead.
  uint32_t       m_iComponentCount;
  bool         m_bSigned;
  bool         m_bIsBigEndian;
  bool         m_bIsJPEGEncoded;
  std::string  m_strDesc;
  std::string  m_strFileType;
};


class DirectoryParser
{
public:
  DirectoryParser(void);
  virtual ~DirectoryParser(void);

  virtual void GetDirInfo(std::string  strDirectory) = 0;
  virtual void GetDirInfo(std::wstring wstrDirectory) = 0;

  std::vector<FileStackInfo*> m_FileStacks;
};

#endif // DIRECTORYPARSER_H
