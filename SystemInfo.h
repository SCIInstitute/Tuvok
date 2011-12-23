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
  \file    SystemInfo.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    August 2008
*/

#pragma once

#ifndef SYSTEMINFO_H
#define SYSTEMINFO_H

#include "StdDefines.h"
#include <string>

class SystemInfo
{
public:
  SystemInfo(std::string strProgramPath="", uint64_t iDefaultCPUMemSize=uint64_t(32)*uint64_t(1024)*uint64_t(1024)*uint64_t(1024), uint64_t iDefaultGPUMemSize=uint64_t(8)*uint64_t(1024)*uint64_t(1024)*uint64_t(1024));

  void SetProgramPath(std::string strProgramPath) {m_strProgramPath = strProgramPath;}

  std::string GetProgramPath() const {return m_strProgramPath;}
  uint32_t GetProgramBitWidth() const {return m_iProgramBitWidth;}
  uint64_t GetCPUMemSize() const {return m_iCPUMemSize;}
  uint64_t GetGPUMemSize() const {return m_iGPUMemSize;}
  bool IsCPUSizeComputed() const {return m_bIsCPUSizeComputed;}
  bool IsGPUSizeComputed() const {return m_bIsGPUSizeComputed;}
  uint64_t GetMaxUsableCPUMem() const {return m_iUseMaxCPUMem;}
  uint64_t GetMaxUsableGPUMem() const {return m_iUseMaxGPUMem;}
  void SetMaxUsableCPUMem(uint64_t iUseMaxCPUMem) {m_iUseMaxCPUMem = iUseMaxCPUMem;}
  void SetMaxUsableGPUMem(uint64_t iUseMaxGPUMem) {m_iUseMaxGPUMem = iUseMaxGPUMem;}
  bool IsNumberOfCPUsComputed() const {return m_bIsNumberOfCPUsComputed;}
  uint32_t GetNumberOfCPUs() const {return m_iNumberOfCPUs;}
  bool IsDirectX10Capable() const {return m_bIsDirectX10Capable; }

private:
  uint32_t ComputeNumCPUs();
  uint64_t ComputeCPUMemSize();
  uint64_t ComputeGPUMemory();

  std::string m_strProgramPath;
  uint32_t  m_iProgramBitWidth;
  uint64_t  m_iUseMaxCPUMem;
  uint64_t  m_iUseMaxGPUMem;
  uint64_t  m_iCPUMemSize;
  uint64_t  m_iGPUMemSize;
  uint32_t  m_iNumberOfCPUs;

  bool m_bIsCPUSizeComputed;
  bool m_bIsGPUSizeComputed;
  bool m_bIsNumberOfCPUsComputed;
  bool m_bIsDirectX10Capable;
};

#endif // SYSTEMINFO_H
