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
  \file    DXTexture3D.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    August 2008
*/


#pragma once

#if defined(_WIN32) && defined(USE_DIRECTX)

#ifndef DXTEXTURE3D_H
#define DXTEXTURE3D_H

#include "../../StdTuvokDefines.h"
#include "DXTexture.h"
#include "../../Basics/Vectors.h"

namespace tuvok {

class DXTexture3D : public DXTexture {
  public:
    DXTexture3D(ID3D10Device* pd3dDevice, uint32_t iSizeX, uint32_t iSizeY, uint32_t iSizeZ, DXGI_FORMAT format);
    DXTexture3D(ID3D10Device* pd3dDevice, uint32_t iSizeX, uint32_t iSizeY, uint32_t iSizeZ, DXGI_FORMAT format,
                const void* pInitialData, bool bIsReadOnly=true);

    virtual ~DXTexture3D();

    virtual void SetData(const void *pData);
    virtual void Delete();

    virtual uint64_t GetCPUSize() {return uint64_t(m_iSizeX*m_iSizeY*m_iSizeZ*m_iSizePerElement);}
    virtual uint64_t GetGPUSize() {return uint64_t(m_iSizeX*m_iSizeY*m_iSizeZ*m_iSizePerElement);}

    UINTVECTOR3 GetSize() const {return UINTVECTOR3(uint32_t(m_iSizeX),uint32_t(m_iSizeY),uint32_t(m_iSizeZ));}

  protected:
    uint32_t m_iSizeX;
    uint32_t m_iSizeY;
    uint32_t m_iSizeZ;

    ID3D10Texture3D* m_pTexture;
};
}; //namespace tuvok

#endif // DXTEXTURE3D_H

#endif // _WIN32 && USE_DIRECTX
