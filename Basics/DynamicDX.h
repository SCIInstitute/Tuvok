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
  \file    DynamicDX.h
  \author    Jens Krueger
        SCI Institute
        University of Utah
  \date    January 2009
*/

#pragma once

#if defined(_WIN32) && defined(USE_DIRECTX)

#ifndef DYNAMICDX_H
#define DYNAMICDX_H

#include "../StdTuvokDefines.h"

#include <windows.h>
#include <dxgi.h>
#include <d3d10.h>
#include <d3dx10.h>

class DynamicDX {
public:
  static bool InitializeDX();
  static void CleanupDX();
  static bool IsInitialized() {return m_bDynamicDXIsInitialized;}

  // DXGI calls
  typedef HRESULT ( WINAPI* LPCREATEDXGIFACTORY )( REFIID, void** );
  static LPCREATEDXGIFACTORY CreateDXGIFactory;

  // D3D10 calls
  typedef HRESULT ( WINAPI* LPD3D10CREATEDEVICE )( IDXGIAdapter*, D3D10_DRIVER_TYPE, HMODULE, UINT, UINT, ID3D10Device** );
  static LPD3D10CREATEDEVICE D3D10CreateDevice;

private:
  static bool m_bDynamicDXIsInitialized;
  static HINSTANCE m_hD3D10;
  static HINSTANCE m_hDXGI;
  static HINSTANCE m_hD3DX10;

};

#endif // DYNAMICDX_H

#endif // _WIN32 && USE_DIRECTX