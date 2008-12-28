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
  \file    RAWConverter.cpp
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    December 2008
*/
#include <cstdio>

#include "RAWConverter.h"
#include "IOManager.h"  // for the size defines
#include <Controller/MasterController.h>
#include <Basics/SysTools.h>
#include <IO/gzio.h>

using namespace std;

bool RAWConverter::ConvertRAWDataset(const string& strFilename, const string& strTargetFilename, const string& strTempDir, MasterController* pMasterController, 
                                     UINT64 iHeaderSkip, UINT64 iComponentSize, UINT64 iComponentCount, bool bConvertEndianness, bool bSigned,
                                     UINTVECTOR3 vVolumeSize,FLOATVECTOR3 vVolumeAspect, const string& strDesc, const string& strSource, UVFTables::ElementSemanticTable eType)
{
  if (iComponentSize < 16) bConvertEndianness = false; // catch silly user input

  pMasterController->DebugOut()->Message("RAWConverter::ConvertRAWDataset","Converting RAW dataset %s to %s", strFilename.c_str(), strTargetFilename.c_str());

  string strSourceFilename;
  string tmpFilename0 = strTempDir+SysTools::GetFilename(strFilename)+".endianess";
  string tmpFilename1 = strTempDir+SysTools::GetFilename(strFilename)+".quantized";

  if (bConvertEndianness) {
    pMasterController->DebugOut()->Message("RAWConverter::ConvertRAWDataset","Performing endianess conversion ...");

    if (iComponentSize != 16 && iComponentSize != 32 && iComponentSize != 64) {
      pMasterController->DebugOut()->Error("RAWConverter::ConvertRAWDataset","Unable to endian convert anything but 16bit, 32bit, or 64bit values (requested %i)", iComponentSize);
      return false;
    }

    LargeRAWFile WrongEndianData(strFilename, iHeaderSkip);
    WrongEndianData.Open(false);

    if (!WrongEndianData.IsOpen()) {
      pMasterController->DebugOut()->Error("RAWConverter::ConvertRAWDataset","Unable to open source file %s", strFilename.c_str());
      return false;
    }

    LargeRAWFile ConvEndianData(tmpFilename0);
    ConvEndianData.Create();

    if (!ConvEndianData.IsOpen()) {
      pMasterController->DebugOut()->Error("RAWConverter::ConvertRAWDataset","Unable to open temp file %s for endianess conversion", tmpFilename0.c_str());
      WrongEndianData.Close();
      return false;
    }

    UINT64 ulFileLength = WrongEndianData.GetCurrentSize();
    size_t iBufferSize = min<size_t>(size_t(ulFileLength), size_t(BRICKSIZE*BRICKSIZE*BRICKSIZE*iComponentSize/8)); // hint: this must fit into memory otherwise other subsystems would break
    UINT64 ulBufferConverted = 0;

    unsigned char* pBuffer = new unsigned char[iBufferSize];

    while (ulBufferConverted < ulFileLength) {

      size_t iBytesRead = WrongEndianData.ReadRAW(pBuffer, iBufferSize);

      switch (iComponentSize) {
        case 16 : for (size_t i = 0;i<iBytesRead;i+=2)
                    EndianConvert::Swap<unsigned short>((unsigned short*)(pBuffer+i));                     
                  break;
        case 32 : for (size_t i = 0;i<iBytesRead;i+=4)
                    EndianConvert::Swap<float>((float*)(pBuffer+i)); 
                  break;
        case 64 : for (size_t i = 0;i<iBytesRead;i+=8) 
                    EndianConvert::Swap<double>((double*)(pBuffer+i)); 
                  break;
      }

      size_t iBytesWritten = ConvEndianData.WriteRAW(pBuffer, iBytesRead);

      if (iBytesRead != iBytesWritten)  {
        pMasterController->DebugOut()->Error("RAWConverter::ConvertRAWDataset","Read/Write error converting endianess from %s to %s", strFilename.c_str(), tmpFilename0.c_str());
        WrongEndianData.Close();
        ConvEndianData.Close();
        remove(tmpFilename0.c_str());
        delete [] pBuffer;
        return false;
      }

      ulBufferConverted += UINT64(iBytesWritten);
    }

    delete [] pBuffer;

    WrongEndianData.Close();
    ConvEndianData.Close();
    strSourceFilename = tmpFilename0;
    iHeaderSkip = 0;  // the new file is straight raw without any header
  } else strSourceFilename = strFilename;

  Histogram1DDataBlock Histogram1D;

  switch (iComponentSize) {
    case 8 : 
      strSourceFilename = Process8BitsTo8Bits(iHeaderSkip, strSourceFilename, tmpFilename1, iComponentCount*vVolumeSize.volume(), bSigned, Histogram1D, pMasterController);
      break;
    case 16 : 
      strSourceFilename = QuantizeShortTo12Bits(iHeaderSkip, strSourceFilename, tmpFilename1, iComponentCount*vVolumeSize.volume(), bSigned, Histogram1D, pMasterController);
      break;
    case 32 :
      strSourceFilename = QuantizeFloatTo12Bits(iHeaderSkip, strSourceFilename, tmpFilename1, iComponentCount*vVolumeSize.volume(), Histogram1D, pMasterController);
      iComponentSize = 16;
      break;
  }
  
  if (strSourceFilename == "")  {
    pMasterController->DebugOut()->Error("RAWConverter::ConvertRAWDataset","Read/Write error quantizing from to %s", strFilename.c_str());
    return false;
  }
  
  bool bQuantized;
  if (strSourceFilename == tmpFilename1) {
    bQuantized = true;
    
    // if we actually created two temp file so far we can delete the first one
    if (bConvertEndianness) {
      remove(tmpFilename0.c_str());
      bConvertEndianness = false;
    }
      
    iHeaderSkip = 0; // the new file is straight raw without any header
  } else {
    bQuantized = false;
  }

  LargeRAWFile SourceData(strSourceFilename, iHeaderSkip);
  SourceData.Open(false);

  if (!SourceData.IsOpen()) {
    pMasterController->DebugOut()->Error("RAWConverter::ConvertRAWDataset","Unable to open source file %s", strSourceFilename.c_str());
    return false;
  }

  wstring wstrUVFName(strTargetFilename.begin(), strTargetFilename.end());
  UVF uvfFile(wstrUVFName);

  UINT64 iLodLevelCount = 1;
  unsigned int iMaxVal = vVolumeSize.maxVal();

  while (iMaxVal > BRICKSIZE) {
    iMaxVal /= 2;
    iLodLevelCount++;
  }

  GlobalHeader uvfGlobalHeader;
  uvfGlobalHeader.bIsBigEndian = EndianConvert::IsBigEndian();
  uvfGlobalHeader.ulChecksumSemanticsEntry = UVFTables::CS_MD5;
  uvfFile.SetGlobalHeader(uvfGlobalHeader);

  RasterDataBlock dataVolume;

  if (strSource == "") 
    dataVolume.strBlockID = (strDesc!="") ? strDesc + " volume converted by ImageVis3D" : "Volume converted by ImageVis3D";
  else
    dataVolume.strBlockID = (strDesc!="") ? strDesc + " volume converted from " + strSource + " by ImageVis3D" : "Volume converted from " + strSource + " by ImageVis3D";

  dataVolume.ulCompressionScheme = UVFTables::COS_NONE;
  dataVolume.ulDomainSemantics.push_back(UVFTables::DS_X);
  dataVolume.ulDomainSemantics.push_back(UVFTables::DS_Y);
  dataVolume.ulDomainSemantics.push_back(UVFTables::DS_Z);

  dataVolume.ulDomainSize.push_back(vVolumeSize.x);
  dataVolume.ulDomainSize.push_back(vVolumeSize.y);
  dataVolume.ulDomainSize.push_back(vVolumeSize.z);

  dataVolume.ulLODDecFactor.push_back(2);
  dataVolume.ulLODDecFactor.push_back(2);
  dataVolume.ulLODDecFactor.push_back(2);

  dataVolume.ulLODGroups.push_back(0);
  dataVolume.ulLODGroups.push_back(0);
  dataVolume.ulLODGroups.push_back(0);

  dataVolume.ulLODLevelCount.push_back(iLodLevelCount);

  vector<UVFTables::ElementSemanticTable> vSem;

  switch (iComponentCount) {
    case 3 : vSem.push_back(UVFTables::ES_RED);
         vSem.push_back(UVFTables::ES_GREEN);
         vSem.push_back(UVFTables::ES_BLUE); break;
    case 4 : vSem.push_back(UVFTables::ES_RED);
         vSem.push_back(UVFTables::ES_GREEN);
         vSem.push_back(UVFTables::ES_BLUE);
         vSem.push_back(UVFTables::ES_ALPHA); break;
    default : for (uint i = 0;i<iComponentCount;i++) vSem.push_back(eType);
  }

  dataVolume.SetTypeToVector(iComponentSize/iComponentCount,
                             iComponentSize == 32 ? 23 : iComponentSize/iComponentCount,
                             bSigned,
                             vSem);

  dataVolume.ulBrickSize.push_back(BRICKSIZE);
  dataVolume.ulBrickSize.push_back(BRICKSIZE);
  dataVolume.ulBrickSize.push_back(BRICKSIZE);

  dataVolume.ulBrickOverlap.push_back(BRICKOVERLAP);
  dataVolume.ulBrickOverlap.push_back(BRICKOVERLAP);
  dataVolume.ulBrickOverlap.push_back(BRICKOVERLAP);

  vector<double> vScale;
  vScale.push_back(vVolumeAspect.x);
  vScale.push_back(vVolumeAspect.y);
  vScale.push_back(vVolumeAspect.z);
  dataVolume.SetScaleOnlyTransformation(vScale);

  MaxMinDataBlock MaxMinData;

  switch (iComponentSize) {
    case 8 :
          switch (iComponentCount) {
            case 1 : dataVolume.FlatDataToBrickedLOD(&SourceData, strTempDir+"tempFile.tmp", CombineAverage<unsigned char,1>, SimpleMaxMin<unsigned char>, &MaxMinData, pMasterController->DebugOut()); break;
            case 2 : dataVolume.FlatDataToBrickedLOD(&SourceData, strTempDir+"tempFile.tmp", CombineAverage<unsigned char,2>, NULL, NULL, pMasterController->DebugOut()); break;
            case 3 : dataVolume.FlatDataToBrickedLOD(&SourceData, strTempDir+"tempFile.tmp", CombineAverage<unsigned char,3>, NULL, NULL, pMasterController->DebugOut()); break;
            case 4 : dataVolume.FlatDataToBrickedLOD(&SourceData, strTempDir+"tempFile.tmp", CombineAverage<unsigned char,4>, NULL, NULL, pMasterController->DebugOut()); break;
            default: pMasterController->DebugOut()->Error("RAWConverter::ConvertRAWDataset","Unsupported iComponentCount %i for iComponentSize %i.", int(iComponentCount), int(iComponentSize)); uvfFile.Close(); SourceData.Close(); return false;
          } break;
    case 16 :
          switch (iComponentCount) {
            case 1 : dataVolume.FlatDataToBrickedLOD(&SourceData, strTempDir+"tempFile.tmp", CombineAverage<unsigned short,1>, SimpleMaxMin<unsigned short>, &MaxMinData, pMasterController->DebugOut()); break;
            case 2 : dataVolume.FlatDataToBrickedLOD(&SourceData, strTempDir+"tempFile.tmp", CombineAverage<unsigned short,2>, NULL, NULL, pMasterController->DebugOut()); break;
            case 3 : dataVolume.FlatDataToBrickedLOD(&SourceData, strTempDir+"tempFile.tmp", CombineAverage<unsigned short,3>, NULL, NULL, pMasterController->DebugOut()); break;
            case 4 : dataVolume.FlatDataToBrickedLOD(&SourceData, strTempDir+"tempFile.tmp", CombineAverage<unsigned short,4>, NULL, NULL, pMasterController->DebugOut()); break;
            default: pMasterController->DebugOut()->Error("RAWConverter::ConvertRAWDataset","Unsupported iComponentCount %i for iComponentSize %i.", int(iComponentCount), int(iComponentSize)); uvfFile.Close(); SourceData.Close(); return false;
          } break;
    case 32 :
          switch (iComponentCount) {
            case 1 : dataVolume.FlatDataToBrickedLOD(&SourceData, strTempDir+"tempFile.tmp", CombineAverage<float,1>, SimpleMaxMin<float>, &MaxMinData, pMasterController->DebugOut()); break;
            case 2 : dataVolume.FlatDataToBrickedLOD(&SourceData, strTempDir+"tempFile.tmp", CombineAverage<float,2>, NULL, NULL, pMasterController->DebugOut()); break;
            case 3 : dataVolume.FlatDataToBrickedLOD(&SourceData, strTempDir+"tempFile.tmp", CombineAverage<float,3>, NULL, NULL, pMasterController->DebugOut()); break;
            case 4 : dataVolume.FlatDataToBrickedLOD(&SourceData, strTempDir+"tempFile.tmp", CombineAverage<float,4>, NULL, NULL, pMasterController->DebugOut()); break;
            default: pMasterController->DebugOut()->Error("RAWConverter::ConvertRAWDataset","Unsupported iComponentCount %i for iComponentSize %i.", int(iComponentCount), int(iComponentSize)); uvfFile.Close(); SourceData.Close(); return false;
          } break;
    default: pMasterController->DebugOut()->Error("RAWConverter::ConvertRAWDataset","Unsupported iComponentSize %i.", int(iComponentSize)); uvfFile.Close(); SourceData.Close(); return false;
  }

  string strProblemDesc;
  if (!dataVolume.Verify(&strProblemDesc)) {
    pMasterController->DebugOut()->Error("RAWConverter::ConvertRAWDataset","Verify failed with the following reason: %s", strProblemDesc.c_str()); 
    uvfFile.Close(); 
    SourceData.Close();
    if (bConvertEndianness) remove(tmpFilename0.c_str());
    if (bQuantized) remove(tmpFilename1.c_str());
    return false;
  }

  if (!uvfFile.AddDataBlock(&dataVolume,dataVolume.ComputeDataSize(), true)) {
    pMasterController->DebugOut()->Error("RAWConverter::ConvertRAWDataset","AddDataBlock failed!"); 
    uvfFile.Close(); 
    SourceData.Close();
    if (bConvertEndianness) remove(tmpFilename0.c_str());
    if (bQuantized) remove(tmpFilename1.c_str());
    return false;
  }


  // if no resampling was perfomed above we need to compute the 1d histogram here
  if (Histogram1D.GetHistogram().size() == 0) {
    pMasterController->DebugOut()->Message("RAWConverter::ConvertRAWDataset","Computing 1D Histogram...");
    if (!Histogram1D.Compute(&dataVolume)) {
      pMasterController->DebugOut()->Error("RAWConverter::ConvertRAWDataset","Computation of 1D Histogram failed!"); 
      uvfFile.Close(); 
      SourceData.Close();
      if (bConvertEndianness) remove(tmpFilename0.c_str());
      if (bQuantized) remove(tmpFilename1.c_str());
      return false;
    }
  }

  pMasterController->DebugOut()->Message("RAWConverter::ConvertRAWDataset","Computing 2D Histogram...");
  Histogram2DDataBlock Histogram2D;
  if (!Histogram2D.Compute(&dataVolume, Histogram1D.GetHistogram().size())) {
    pMasterController->DebugOut()->Error("RAWConverter::ConvertRAWDataset","Computation of 2D Histogram failed!"); 
    uvfFile.Close(); 
    SourceData.Close();
    if (bConvertEndianness) remove(tmpFilename0.c_str());
    if (bQuantized) remove(tmpFilename1.c_str());
    return false;
  }

  pMasterController->DebugOut()->Message("RAWConverter::ConvertRAWDataset","Merging data...");

  uvfFile.AddDataBlock(&Histogram1D,Histogram1D.ComputeDataSize());
  uvfFile.AddDataBlock(&Histogram2D,Histogram2D.ComputeDataSize());
  uvfFile.AddDataBlock(&MaxMinData, MaxMinData.ComputeDataSize());

/*
  /// \todo maybe add information from the source file to the UVF, like DICOM desc etc.

  KeyValuePairDataBlock testPairs;
  testPairs.AddPair("SOURCE","DICOM");
  testPairs.AddPair("CONVERTED BY","DICOM2UVF V1.0");
  UINT64 iDataSize = testPairs.ComputeDataSize();
  uvfFile.AddDataBlock(testPairs,iDataSize);
*/

  pMasterController->DebugOut()->Message("RAWConverter::ConvertRAWDataset","Computing checksum and writing file...");

  uvfFile.Create();
  SourceData.Close();
  uvfFile.Close();

  pMasterController->DebugOut()->Message("RAWConverter::ConvertRAWDataset","Removing temporarie files...");

  if (bConvertEndianness) remove(tmpFilename0.c_str());
  if (bQuantized) remove(tmpFilename1.c_str());

  pMasterController->DebugOut()->Message("RAWConverter::ConvertRAWDataset","Done!");
  return true;
}


#ifdef WIN32
  #pragma warning( disable : 4996 ) // disable deprecated warning 
#endif

/** Converts a gzip-compressed chunk of a file to a raw file.
 * @param strFilename the input (compressed) file
 * @param strTargetFilename the target uvf file 
 * @param strTempDir directory prefix for raw file.
 * @param pMasterController controller, for reporting errors
 * @param iHeaderSkip number of bytes to skip off of strFilename
 * @param iComponentSize size in bit of one component (e.g. 8 for a byte)
 * @param iComponentCount how many components (e.g. 1 for scalar)
 * @param bSigned is the value a signed variable
 * @param bConvertEndianness if we need to flip the endianness of the data
 * @param vVolumeSize dimensions of the volume
 * @param vVolumeAspect per-dimension aspect ratio
 * @param strDesc a string decribing the input data
 * @param strSource a string decribing the input filename (usually the filnemae without the path)
 * @param eType data type enumerator (defaults to undefined) */
bool RAWConverter::ConvertGZIPDataset(const string& strFilename,
                                      const string& strTargetFilename,
                                      const string& strTempDir,
                                      MasterController* pMasterController,
                                      UINT64 iHeaderSkip,
                                      UINT64 iComponentSize,
                                      UINT64 iComponentCount,
                                      bool bConvertEndianness,
                                      bool bSigned,
                                      UINTVECTOR3 vVolumeSize,
                                      FLOATVECTOR3 vVolumeAspect,
                                      const string& strDesc,
                                      const string& strSource,
                                      UVFTables::ElementSemanticTable eType)
{
  string strUncompressedFile = strTempDir+SysTools::GetFilename(strFilename)+".uncompressed";

  FILE *f_compressed;
  FILE *f_inflated;
  int ret;
  static const char method[] = "RAWConverter::ConvertGZIPDataset";
  AbstrDebugOut *dbg = pMasterController->DebugOut();

  dbg->Message(method, "Deflating GZIP data ...");

  f_compressed = fopen(strFilename.c_str(), "rb");
  f_inflated = fopen(strUncompressedFile.c_str(), "wb");

  if(f_compressed == NULL) {
    dbg->Error(method, "Could not open %s", strFilename.c_str());
    return false;
  }
  if(f_inflated == NULL) {
    dbg->Error(method, "Could not open %s", strUncompressedFile.c_str());
    return false;
  }

  if(fseek(f_compressed, iHeaderSkip, SEEK_SET) != 0) {
    /// \todo use strerror(errno) and actually report the damn error.
    dbg->Error(method, "Seek failed");
    return false;
  }

  gz_skip_header(f_compressed); // always needed?

  ret = gz_inflate(f_compressed, f_inflated);

  fclose(f_compressed);
  fclose(f_inflated);

  switch(ret) {
    case Z_OK:
      dbg->Message(method, "Decompression successful.");
      break;
    case Z_MEM_ERROR:
      dbg->Error(method, "Not enough memory decompress %s",
                 strFilename.c_str());
      return false;
      break;
    case Z_DATA_ERROR:
      dbg->Error(method, "Deflation invalid or incomplete");
      return false;
      break;
    case Z_VERSION_ERROR:
      dbg->Error(method, "Zlib library versioning error!");
      return false;
      break;
    default:
      dbg->Warning(method, "Unknown / unhandled case %d", ret);
      return false;
      break;
  }

  bool bResult = ConvertRAWDataset(strUncompressedFile, strTargetFilename,
                                   strTempDir, pMasterController, 0,
                                   iComponentSize, iComponentCount, bConvertEndianness,
                                   bSigned, vVolumeSize,
                                   vVolumeAspect, strDesc, strSource, eType);

  if( remove(strUncompressedFile.c_str()) != 0 ) {
    dbg->Warning(method, "Unable to delete temp file %s",
                 strUncompressedFile.c_str());
  }
  return bResult;
}

/** Converts a bzip2-compressed file chunk to a raw file.
 * @param strFilename the input (compressed) file
 * @param strTargetFilename the target uvf file
 * @param strTempDir directory prefix for raw file.
 * @param pMasterController controller, for reporting errors
 * @param iHeaderSkip number of bytes to skip of strFilename's header.
 * @param iComponentSize size in bits of one component (e.g. 8 for a byte)
 * @param iComponentCount how many components (e.g. 1 for scalar)
 * @param bSigned is the field a signed field?
 * @param bConvertEndianness if we need to flip the endianness of the data
 * @param vVolumeSize dimensions of the volume
 * @param vVolumeAspect per-dimension aspect ratio
 * @param strDesc a string decribing the input data
 * @param strSource a string decribing the input filename (usually the filname
 *                  without the path)
 * @param eType data type enumerator (defaults to undefined) */
bool RAWConverter::ConvertBZIP2Dataset(const string& strFilename,
                                       const string& strTargetFilename,
                                       const string& strTempDir,
                                       MasterController* pMasterController,
                                       UINT64 iHeaderSkip,
                                       UINT64 iComponentSize,
                                       UINT64 iComponentCount, bool bSigned,
                                       bool bConvertEndianness,
                                       UINTVECTOR3 vVolumeSize,
                                       FLOATVECTOR3 vVolumeAspect,
                                       const string& strDesc,
                                       const string& strSource,
                                       UVFTables::ElementSemanticTable eType)
{
  AbstrDebugOut *dbg = pMasterController->DebugOut();
  static const char method[] = "RAWConverter::ConvertBZIP2Dataset";
  string strUncompressedFile = strTempDir + SysTools::GetFilename(strFilename)
                                          + ".uncompressed";

  FILE *f_compressed = fopen(strFilename.c_str(), "rb");
  FILE *f_inflated = fopen(strUncompressedFile.c_str(), "wb");

  if(f_compressed == NULL) {
    dbg->Error(method, "Could not open %s", strFilename.c_str());
    return false;
  }
  if(f_inflated == NULL) {
    dbg->Error(method, "Could not open %s", strUncompressedFile.c_str());
    return false;
  }

  if(fseek(f_compressed, iHeaderSkip, SEEK_SET) != 0) {
    /// \todo use strerror(errno) and actually report the damn error.
    dbg->Error(method, "Seek failed");
    return false;
  }

  /// \todo Tom: add bzip2 decompression code here 
  ///            uncompressing strFilename into strUncompressedFile
  ///            and do not forget to skip the first "iHeaderSkip" bytes
  ///            before heanding the stream over to the bzip2 lib

  return false;

  bool bResult = ConvertRAWDataset(strUncompressedFile, strTargetFilename,
                                   strTempDir, pMasterController, 0,
                                   iComponentSize, iComponentCount, bSigned,
                                   bConvertEndianness, vVolumeSize,
                                   vVolumeAspect, strDesc, strSource, eType);

  if( remove(strUncompressedFile.c_str()) != 0 ) {
    dbg->Warning(method, "Unable to delete temp file '%s'.",
                 strUncompressedFile.c_str());
  }

  return bResult;
}

bool RAWConverter::ConvertTXTDataset(const string& strFilename, const string& strTargetFilename, const string& strTempDir, MasterController* pMasterController, 
                                     UINT64 iHeaderSkip, UINT64 iComponentSize, UINT64 iComponentCount, bool bSigned,
                                     UINTVECTOR3 vVolumeSize,FLOATVECTOR3 vVolumeAspect, const string& strDesc, const string& strSource, UVFTables::ElementSemanticTable eType)
{
  string strBinaryFile = strTempDir+SysTools::GetFilename(strFilename)+".binary";

  ifstream sourceFile(strFilename.c_str(),ios::binary);  
  if (!sourceFile.is_open()) {
    pMasterController->DebugOut()->Error("NRRDConverter::ConvertTXTDataset","Unable to open source file %s.", strFilename.c_str());
    return false;
  }

  LargeRAWFile binaryFile(strBinaryFile);
  binaryFile.Create(iComponentSize/8 * iComponentCount * vVolumeSize.volume());
  if (!binaryFile.IsOpen()) {
    pMasterController->DebugOut()->Error("NRRDConverter::ConvertTXTDataset","Unable to open temp file %s.", strBinaryFile.c_str());
    sourceFile.close();
    return false;
  }

  sourceFile.seekg(iHeaderSkip);
 
  switch (iComponentSize) {
    case 8 : {
               if (bSigned) {
                 while (! sourceFile.eof() )
                 {
                   int tmp;
                   sourceFile >> tmp;
                   signed char tmp2 = static_cast<signed char>(tmp);
                   binaryFile.WriteRAW((unsigned char*)&tmp2,1);
                 }
               } else {
                 while (! sourceFile.eof() )
                 {
                   int tmp;
                   sourceFile >> tmp;
                   unsigned char tmp2 = static_cast<unsigned char>(tmp);
                   binaryFile.WriteRAW((unsigned char*)&tmp2,1);
                 }
               }
               break;
             }
    case 16 : {
                if (bSigned) {
                  while (! sourceFile.eof() )
                  {
                    signed short tmp;
                    sourceFile >> tmp;
                    binaryFile.WriteRAW((unsigned char*)&tmp,2);
                  }
                } else {
                  while (! sourceFile.eof() )
                  {
                    unsigned short tmp;
                    sourceFile >> tmp;
                    binaryFile.WriteRAW((unsigned char*)&tmp,2);
                  }
                }
                break;
              }
    case 32 : {
                if (bSigned) {
                  while (! sourceFile.eof() )
                  {
                    signed int tmp;
                    sourceFile >> tmp;
                    binaryFile.WriteRAW((unsigned char*)&tmp,4);
                  }
                } else {
                  while (! sourceFile.eof() )
                  {
                    unsigned int tmp;
                    sourceFile >> tmp;
                    binaryFile.WriteRAW((unsigned char*)&tmp,4);
                  }
                }
               break;
             }
    default : {
                pMasterController->DebugOut()->Error("NRRDConverter::ConvertTXTDataset","Unable unsupported data type.");
                sourceFile.close();
                binaryFile.Delete();
                return false;
              }
  }

  binaryFile.Close();
  sourceFile.close();

  bool bResult = ConvertRAWDataset(strBinaryFile, strTargetFilename, strTempDir, pMasterController, 
                                   0, iComponentSize, iComponentCount, false, bSigned,
                                   vVolumeSize, vVolumeAspect, strDesc, strSource, eType);

  if( remove(strBinaryFile.c_str()) != 0 )
      pMasterController->DebugOut()->Warning("NRRDConverter::ConvertTXTDataset","Unable to delete temp file %s.", strBinaryFile.c_str());

  return bResult;
}
