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
  \file    DXTexture1D.cpp
  \author    Jens Krueger
        SCI Institute
        University of Utah
  \date    August 2008
*/

#if defined(_WIN32) && defined(USE_DIRECTX)

#include "DXTexture1D.h"
#include <cassert>
using namespace tuvok;


DXTexture1D::DXTexture1D(ID3D10Device* pd3dDevice, uint32_t iSize, DXGI_FORMAT format) :
  DXTexture(pd3dDevice, g_dx10Format[int(format)].m_iByteSize, false),
  m_iSize(iSize),
  m_pTexture(NULL)
{
  // create texture and fill with zeros
  D3D10_TEXTURE1D_DESC texDesc = {
    m_iSize,
    1, 1,
    format,
    D3D10_USAGE_DEFAULT,
    D3D10_BIND_SHADER_RESOURCE,
    0, 0
  };

  m_pd3dDevice->CreateTexture1D( &texDesc, NULL, &m_pTexture);

  // create shader resource views
  D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc = {
    texDesc.Format,
    D3D10_SRV_DIMENSION_TEXTURE1D, 0, 1
  };
  m_pd3dDevice->CreateShaderResourceView( m_pTexture, &SRVDesc, &m_pTexture_SRV );
}

DXTexture1D::DXTexture1D(ID3D10Device* pd3dDevice, uint32_t iSize, DXGI_FORMAT format,
                         const void* pInitialData, bool bIsReadOnly) :
  DXTexture(pd3dDevice, g_dx10Format[int(format)].m_iByteSize, bIsReadOnly),
  m_iSize(iSize),
  m_pTexture(NULL)
{
  assert(pInitialData || !bIsReadOnly);

  // create texture (if no inital data is specified the textures are filled with zeros)
  D3D10_TEXTURE1D_DESC texDesc = {
    m_iSize,
    1, 1,
    format,
    bIsReadOnly ? D3D10_USAGE_IMMUTABLE : D3D10_USAGE_DEFAULT,
    D3D10_BIND_SHADER_RESOURCE,
    0, 0
  };

  D3D10_SUBRESOURCE_DATA vbInitDataTex = {
    pInitialData,
    0,
    0
  };
  m_pd3dDevice->CreateTexture1D( &texDesc,
                                 pInitialData == NULL ? NULL : &vbInitDataTex,
                                 &m_pTexture);

  // create shader resource views
  D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc = {
    texDesc.Format,
    D3D10_SRV_DIMENSION_TEXTURE2D, 0, 1
  };
  m_pd3dDevice->CreateShaderResourceView( m_pTexture, &SRVDesc, &m_pTexture_SRV );
}


DXTexture1D::~DXTexture1D() {
  Delete();
}

void DXTexture1D::SetData(const void *pData) {
  assert(!m_bIsReadOnly);

  // Create a staging resource to copy the data
  ID3D10Texture1D* pStagingTexture = NULL;

  D3D10_TEXTURE1D_DESC desc;
  m_pTexture->GetDesc(&desc);
  desc.Usage = D3D10_USAGE_STAGING;
  desc.BindFlags = 0;
  desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
  m_pd3dDevice->CreateTexture1D( &desc, NULL, &pStagingTexture );

  char* pcData = (char*)pData;

  void *pMapData;
  pStagingTexture->Map(0, D3D10_MAP_WRITE, 0, &pMapData );
  char *pStagingData = (char*)pMapData;
  size_t iDataSize = m_iSizePerElement * m_iSize;
  memcpy(pStagingData, pcData, iDataSize);
  pStagingTexture->Unmap(0);

  m_pd3dDevice->CopyResource(m_pTexture, pStagingTexture);
  SAFE_RELEASE(pStagingTexture);
}

void DXTexture1D::Delete() {
  SAFE_RELEASE(m_pTexture);
}

#endif // _WIN32 && USE_DIRECTX
