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
  \file    SystemInfo.cpp
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    July 2008
*/

#include "SystemInfo.h"

#include "StdDefines.h"
#ifdef _WIN32
  #include <windows.h>
#else
  #ifdef DETECTED_OS_APPLE
    #include <sys/sysctl.h>
  #else
    #include <cstdio>
    #include <iostream>
    #include <sstream>
    #include <sys/resource.h>
    #include <sys/sysinfo.h>
    #include <sys/time.h>
    #include "IO/KeyValueFileParser.h"
  #endif
#endif


SystemInfo::SystemInfo(const std::wstring& strProgramPath, 
                       uint64_t iDefaultCPUMemSize, 
                       uint64_t iDefaultGPUMemSize) :
  m_strProgramPath(strProgramPath),
  m_iProgramBitWidth(sizeof(void*)*8),
  m_iUseMaxCPUMem(iDefaultCPUMemSize),
  m_iUseMaxGPUMem(iDefaultGPUMemSize),
  m_iCPUMemSize(iDefaultCPUMemSize),
  m_iGPUMemSize(iDefaultGPUMemSize),
  m_bIsCPUSizeComputed(false),
  m_bIsGPUSizeComputed(false),
  m_bIsNumberOfCPUsComputed(false),
  m_bIsDirectX10Capable(false)
{
  uint32_t iNumberOfCPUs = ComputeNumCPUs();
  if (iNumberOfCPUs > 0) {
    m_iNumberOfCPUs = iNumberOfCPUs;
    m_bIsNumberOfCPUsComputed = true;
  }

  uint64_t iCPUMemSize = ComputeCPUMemSize();
  if (iCPUMemSize > 0) {
    m_iCPUMemSize = iCPUMemSize;
    m_bIsCPUSizeComputed = true;
  }

  uint64_t iGPUMemSize = ComputeGPUMemory();  // also sets m_bIsDirectX10Capable
  if (iGPUMemSize > 0) {
    m_iGPUMemSize = iGPUMemSize;
    m_bIsGPUSizeComputed = true;
  }
}

uint32_t SystemInfo::ComputeNumCPUs() {
  #ifdef _WIN32
    SYSTEM_INFO siSysInfo;
    GetSystemInfo(&siSysInfo);
    return siSysInfo.dwNumberOfProcessors;
  #else
    #ifdef DETECTED_OS_APPLE
      return 0;
    #else
      return 0;
    #endif
  #endif
}

#ifdef __linux__

#ifndef NDEBUG
#   define DBG(str) do { std::cerr << str << std::endl; } while(0)
#else
#   define DBG(str) /* nothing */
#endif
static unsigned long lnx_mem_sysinfo() {
  struct sysinfo si;
  if(sysinfo(&si) < 0) {
    perror("sysinfo");
    return 0;
  }
  return si.totalram;
}

static uint64_t lnx_mem_rlimit() {
    struct rlimit limit;
    // RLIMIT_MEMLOCK returns 32768 (that's *bytes*) on my 6gb-RAM system, so
    // that number is worthless to us.
    // RLIMIT_DATA isn't great, because it gets the entire data segment size
    // allowable, not just the heap size ...
    if(getrlimit(RLIMIT_DATA, &limit) != 0) {
        perror("getrlimit");
        return 0;
    }
    // Frequently the information given by getrlimit(2) is essentially
    // worthless, because it assumes we don't care about swapping.
    if(limit.rlim_cur == RLIM_INFINITY || limit.rlim_max == RLIM_INFINITY) {
        DBG("getrlimit gave useless info...");
        return 0;
    }
    return limit.rlim_max;
}

static uint64_t lnx_mem_proc() {
  KeyValueFileParser meminfo("/proc/meminfo");

  if(!meminfo.FileReadable()) {
    DBG("could not open proc memory filesystem");
    return 0;
  }

  KeyValPair *mem_total = meminfo.GetData("MemTotal");
  if(mem_total == NULL) {
    DBG("proc mem fs did not have a `MemTotal' entry.");
    return 0;
  }
  std::istringstream mem_strm(mem_total->strValue);
  uint64_t mem;
  mem_strm >> mem;
  return mem * 1024;
}

static uint64_t lnx_mem() {
  uint64_t m;
  if((m = lnx_mem_proc()) == 0) {
    DBG("proc failed, falling back to rlimit");
    if((m = lnx_mem_rlimit()) == 0) {
      DBG("rlimit failed, falling back to sysinfo");
      if((m = lnx_mem_sysinfo()) == 0) {
        DBG("all memory lookups failed; pretending you have 1Gb of memory.");
        return 1024*1024*1024;
      }
    }
  }
  return m;
}
#endif

uint64_t SystemInfo::ComputeCPUMemSize() {
  #ifdef _WIN32
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof (statex);
    GlobalMemoryStatusEx (&statex);
    return statex.ullTotalPhys;
  #else
    #ifdef DETECTED_OS_APPLE
      uint64_t phys = 0;
      int mib[2] = { CTL_HW, HW_MEMSIZE };
      size_t len = sizeof(phys);
      if(sysctl(mib, sizeof(mib)/sizeof(mib[0]), &phys, &len, NULL, 0) != 0) {
        perror("max memory query");
        return 0;
      }
      return phys;
    #elif defined(__linux__)
      return static_cast<uint64_t>(lnx_mem());
    #else
      std::cerr << "Unknown system, can't lookup max memory.  "
                << "Using a hard setting of 10 gb." << std::endl;
      return 1024ULL*1024ULL*1024ULL*10ULL;
    #endif
  #endif
}

#if defined(_WIN32) && defined(USE_DIRECTX)
  #define INITGUID
  #include <string.h>
  #include <stdio.h>
  #include <assert.h>
  #include <d3d9.h>
  #include <multimon.h>

  #pragma comment( lib, "d3d9.lib" )

  HRESULT GetVideoMemoryViaDirectDraw( HMONITOR hMonitor, DWORD* pdwAvailableVidMem );
  HRESULT GetVideoMemoryViaDXGI( HMONITOR hMonitor, SIZE_T* pDedicatedVideoMemory, SIZE_T* pDedicatedSystemMemory, SIZE_T* pSharedSystemMemory );

  #ifndef SAFE_RELEASE
    #define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
  #endif

  typedef IDirect3D9* ( WINAPI* LPDIRECT3DCREATE9 )( UINT  );
  LPDIRECT3DCREATE9 pDirect3DCreate9;

  uint64_t SystemInfo::ComputeGPUMemory( )
  {
    HINSTANCE hD3D9 = LoadLibrary( L"d3d9.dll" );
    if (!hD3D9) return 0;
    pDirect3DCreate9 = ( LPDIRECT3DCREATE9 )GetProcAddress( hD3D9, "Direct3DCreate9" );

    if (!pDirect3DCreate9) {
      FreeLibrary( hD3D9 );
      return 0;
    }

    IDirect3D9* pD3D9 = NULL;
    pD3D9 = pDirect3DCreate9( D3D_SDK_VERSION );
    if( pD3D9 )
    {
        UINT dwAdapterCount = pD3D9->GetAdapterCount();
        if (dwAdapterCount > 0) {
            UINT iAdapter = 0;

            HMONITOR hMonitor = pD3D9->GetAdapterMonitor( iAdapter );

            SIZE_T DedicatedVideoMemory;
            SIZE_T DedicatedSystemMemory;
            SIZE_T SharedSystemMemory;
            if( SUCCEEDED( GetVideoMemoryViaDXGI( hMonitor, &DedicatedVideoMemory, &DedicatedSystemMemory, &SharedSystemMemory ) ) )
            {
              m_bIsDirectX10Capable = true;
              SAFE_RELEASE( pD3D9 );
              return uint64_t(DedicatedVideoMemory);
            } else {
              DWORD dwAvailableVidMem;
              if( SUCCEEDED( GetVideoMemoryViaDirectDraw( hMonitor, &dwAvailableVidMem ) ) ) {
                SAFE_RELEASE( pD3D9 );
                FreeLibrary( hD3D9 );
                return uint64_t(dwAvailableVidMem);
              } else {
                SAFE_RELEASE( pD3D9 );
                FreeLibrary( hD3D9 );
                return 0;
              }
            }
        }

        SAFE_RELEASE( pD3D9 );
        FreeLibrary( hD3D9 );
        return 0;
    }
    else
    {
        FreeLibrary( hD3D9 );
        return 0;
    }
  }

#else
  uint64_t SystemInfo::ComputeGPUMemory( )
  {
    #ifdef DETECTED_OS_APPLE
      return 0;
    #else // Linux
      return 0;
    #endif
  }
#endif
