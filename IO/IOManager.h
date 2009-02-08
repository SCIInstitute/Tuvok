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
  \file    IOManager.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \version 1.0
  \date    August 2008
*/
#pragma once

#ifndef IOMANAGER_H
#define IOMANAGER_H

#include "../StdTuvokDefines.h"
#include <string>
#include "../Renderer/AbstrRenderer.h"
#include "../IO/DirectoryParser.h"
#include "../IO/UVF/UVF.h"
#include "RAWConverter.h"
#include "../Basics/MC.h"

#define BRICKSIZE (256)
#define BRICKOVERLAP (4)
#define INCORESIZE (BRICKSIZE*BRICKSIZE*BRICKSIZE)

class MasterController;

class MCData  {
public:  
  MCData(const std::string& strTargetFile) : 
    m_strTargetFile(strTargetFile)
  {}

  virtual ~MCData() {}

  virtual bool PerformMC(LargeRAWFile* pSourceFile, const std::vector<UINT64> vBrickSize, const std::vector<UINT64> vBrickOffset) = 0;

protected:
  std::string m_strTargetFile;
};


template <class T> class MCDataTemplate  : public MCData {
public:  
  MCDataTemplate(const std::string& strTargetFile, T TIsoValue, FLOATVECTOR3 vScale) :
    MCData(strTargetFile),
    m_TIsoValue(TIsoValue),
    m_pData(NULL),
    m_iIndexoffset(0),
    m_pMarchingCubes(new MarchingCubes<T>())
  {
    m_matScale.Scaling(vScale.x, vScale.y, vScale.z);
  }

  virtual ~MCDataTemplate() {
		m_outStream << "end" << std::endl;
		m_outStream.close();

    delete m_pMarchingCubes;
    delete m_pData;
  }

  virtual bool PerformMC(LargeRAWFile* pSourceFile, const std::vector<UINT64> vBrickSize, const std::vector<UINT64> vBrickOffset) {

    UINT64 iSize = 1;
    for (size_t i = 0;i<vBrickSize.size();i++) iSize *= vBrickSize[i];
    if (!m_pData) {   // since we know that no brick is larger than the first we can create a fixed array on first invocation 
      m_pData = new T[iSize];

      m_outStream.open(m_strTargetFile.c_str());
      if (m_outStream.fail()) return false;
      m_outStream << "###############################################" << std::endl;
      m_outStream << "# Mesh created via Marching Cubes by ImageVis3D" << std::endl;
      m_outStream << "###############################################" << std::endl << std::endl;
  		m_outStream << "begin" << std::endl;
    }

    pSourceFile->SeekStart();
    pSourceFile->ReadRAW((unsigned char*)m_pData, size_t(iSize*sizeof(T)));

    // extract isosurface 
    m_pMarchingCubes->SetVolume(int(vBrickSize[0]), int(vBrickSize[1]), int(vBrickSize[2]), m_pData);
    m_pMarchingCubes->Process(m_TIsoValue);

    // apply scale
  	m_pMarchingCubes->m_Isosurface->Transform(m_matScale);

    m_outStream << "# Marching Cubes mesh from a " << vBrickSize[0] << " " << vBrickSize[1] << " " << vBrickSize[2] << " brick. At " << vBrickOffset[0] << " " << vBrickOffset[1] << " " << vBrickOffset[2] << "." << std::endl;

		//Saving to disk (1/3 vertices)
		for (int i = 0;i<m_pMarchingCubes->m_Isosurface->iVertices;i++) {
			m_outStream << "v " << (m_pMarchingCubes->m_Isosurface->vfVertices[i].x + vBrickOffset[0]) << " " 
                          << (m_pMarchingCubes->m_Isosurface->vfVertices[i].y + vBrickOffset[1]) << " " 
                          << (m_pMarchingCubes->m_Isosurface->vfVertices[i].z + vBrickOffset[2]) << std::endl;
		}
		// Saving to disk (2/3 normals)
		for (int i = 0;i<m_pMarchingCubes->m_Isosurface->iVertices;i++) {
			m_outStream << "vn " << m_pMarchingCubes->m_Isosurface->vfNormals[i].x << " " << m_pMarchingCubes->m_Isosurface->vfNormals[i].y << " " << m_pMarchingCubes->m_Isosurface->vfNormals[i].z << std::endl;
		}
		// Saving to disk (3/3 faces)
		for (int i = 0;i<m_pMarchingCubes->m_Isosurface->iTriangles;i++) {
			m_outStream << "f " << m_pMarchingCubes->m_Isosurface->viTriangles[i].x+1+m_iIndexoffset << " " << 
                           m_pMarchingCubes->m_Isosurface->viTriangles[i].z+1+m_iIndexoffset << " " << 
                           m_pMarchingCubes->m_Isosurface->viTriangles[i].y+1+m_iIndexoffset << std::endl;
		}

    m_iIndexoffset += m_pMarchingCubes->m_Isosurface->iVertices;

    return true;

  }

protected:
  T                 m_TIsoValue;
  T*                m_pData;
  UINT64            m_iIndexoffset;
  MarchingCubes<T>* m_pMarchingCubes;
  FLOATMATRIX4      m_matScale;
  std::ofstream     m_outStream;
};

class IOManager {
public:
  IOManager(MasterController* masterController);
  ~IOManager();

  std::vector<FileStackInfo*> ScanDirectory(std::string strDirectory);
  bool ConvertDataset(FileStackInfo* pStack, const std::string& strTargetFilename);
  bool ConvertDataset(const std::string& strFilename, const std::string& strTargetFilename, bool bNoUserInteraction=false);
  VolumeDataset* ConvertDataset(FileStackInfo* pStack, const std::string& strTargetFilename, AbstrRenderer* requester);
  VolumeDataset* ConvertDataset(const std::string& strFilename, const std::string& strTargetFilename, AbstrRenderer* requester);
  VolumeDataset* LoadDataset(const std::string& strFilename, AbstrRenderer* requester);
  bool NeedsConversion(const std::string& strFilename, bool& bChecksumFail);
  bool NeedsConversion(const std::string& strFilename);

  bool ExportDataset(VolumeDataset* pSourceData, UINT64 iLODlevel, const std::string& strTargetFilename, const std::string& strTempDir);
  bool ExtractIsosurface(VolumeDataset* pSourceData, UINT64 iLODlevel, double fIsovalue, const DOUBLEVECTOR3& vfRescaleFactors, const std::string& strTargetFilename, const std::string& strTempDir);

  void RegisterExternalConverter(AbstrConverter* pConverter);
  void RegisterFinalConverter(AbstrConverter* pConverter);

  std::string GetLoadDialogString();
  std::string GetExportDialogString();

private:
  MasterController*             m_pMasterController;
  std::string                   m_TempDir;
  std::vector<AbstrConverter*>  m_vpConverters;
  AbstrConverter*               m_pFinalConverter;
};

#endif // IOMANAGER_H
