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
  \file    DXTexture2D.cpp
  \author    Jens Krueger
        SCI Institute
        University of Utah
  \date    August 2008
*/

#if defined(_WIN32) && defined(USE_DIRECTX)

#include "DXTexture2D.h"


DXTexture2D::DXTexture2D(ID3D10Device* pd3dDevice, UINT32 iSizeX, UINT32 iSizeY, DXGI_FORMAT format, const void* pInitialData) :
  DXTexture(pd3dDevice, g_dx10Format[int(format)].m_iByteSize*8),
  m_iSizeX(iSizeX),
  m_iSizeY(iSizeY),
  m_pTexture(NULL)
{
	// create texture (if no inital data is spcified the textures are filled with zeros)
	D3D10_TEXTURE2D_DESC texDesc = {
		m_iSizeX,
		m_iSizeY,
		1, 1,
		format,
		1,0,
		D3D10_USAGE_IMMUTABLE,
		D3D10_BIND_SHADER_RESOURCE,
		0, 0
	};
	D3D10_SUBRESOURCE_DATA vbInitDataTex = {
	  pInitialData,
		g_dx10Format[int(format)].m_iByteSize,
		0
	};
	m_pd3dDevice->CreateTexture2D( &texDesc, pInitialData == NULL ? NULL : &vbInitDataTex, &m_pTexture);

	// create shader resource views
	D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc = {
    texDesc.Format, 
    D3D10_SRV_DIMENSION_TEXTURE2D, 0, 1
  };
	m_pd3dDevice->CreateShaderResourceView( m_pTexture, &SRVDesc, &m_pTexture_SRV );

}

DXTexture2D::~DXTexture2D() {
  Delete();
}

void DXTexture2D::SetData(const void *pInitialData) {
  // TODO
}

void DXTexture2D::Delete() {
  SAFE_RELEASE(m_pTexture);
}

#endif // _WIN32 && USE_DIRECTX
