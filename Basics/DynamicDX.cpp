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
  \file    DynamicDX.cpp
  \author    Jens Krueger
        SCI Institute
        University of Utah
  \date    January 2009
*/

#if defined(_WIN32) && defined(USE_DIRECTX)

#include "DynamicDX.h"

HINSTANCE DynamicDX::m_hD3D10 = NULL;
HINSTANCE DynamicDX::m_hDXGI = NULL;
HINSTANCE DynamicDX::m_hD3DX10 = NULL;
bool DynamicDX::m_bDynamicDXIsInitialized = false;

DynamicDX::LPCREATEDXGIFACTORY DynamicDX::CreateDXGIFactory = NULL;
DynamicDX::LPD3D10CREATEDEVICE DynamicDX::D3D10CreateDevice = NULL;

bool DynamicDX::InitializeDX() {
  if (m_bDynamicDXIsInitialized) return true;

  m_hD3D10 = LoadLibrary( L"d3d10.dll" );
  m_hDXGI  = LoadLibrary( L"dxgi.dll" );

#ifdef _DEBUG
  m_hD3DX10 = LoadLibrary( L"d3dx10d.dll" );
#else
  m_hD3DX10 = LoadLibrary( L"d3dx10.dll" );
#endif

  if( !m_hD3D10 || !m_hDXGI || !m_hD3DX10 ) return false;
  m_bDynamicDXIsInitialized = true;

  // DXGI calls
  CreateDXGIFactory = ( LPCREATEDXGIFACTORY )GetProcAddress( m_hDXGI, "CreateDXGIFactory" );
  D3D10CreateDevice = ( LPD3D10CREATEDEVICE )GetProcAddress( m_hDXGI, "D3D10CreateDevice" );

  return true;
}


void DynamicDX::CleanupDX( ) {
  if (m_bDynamicDXIsInitialized) return;

  FreeLibrary( m_hD3D10 );
  FreeLibrary( m_hDXGI );
  FreeLibrary( m_hD3DX10 );
}

#endif // _WIN32 && USE_DIRECTX