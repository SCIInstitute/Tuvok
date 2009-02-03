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
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <3rdParty/bzip2/bzlib.h>

#include "RAWConverter.h"
#include "IOManager.h"  // for the size defines
#include <Controller/MasterController.h>
#include <Basics/SysTools.h>
#include <IO/gzio.h>

using namespace std;

bool RAWConverter::ConvertRAWDataset(const string& strFilename, const string& strTargetFilename, const string& strTempDir, MasterController* pMasterController,
                                     UINT64 iHeaderSkip, UINT64 iComponentSize, UINT64 iComponentCount, bool bConvertEndianness, bool bSigned, bool bIsFloat,
                                     UINTVECTOR3 vVolumeSize, FLOATVECTOR3 vVolumeAspect, const string& strDesc, const string& strSource, UVFTables::ElementSemanticTable eType)
{
  if (iComponentCount > 1) {
    pMasterController->DebugOut()->Error("RAWConverter::ConvertRAWDataset","Color data currently not supported.");
    return false;
  }

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
    iHeaderSkip = 0;  // the new file is straigt raw without any header
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
      if (bIsFloat) 
        strSourceFilename = QuantizeFloatTo12Bits(iHeaderSkip, strSourceFilename, tmpFilename1, iComponentCount*vVolumeSize.volume(), Histogram1D, pMasterController);
      else
        strSourceFilename = QuantizeIntTo12Bits(iHeaderSkip, strSourceFilename, tmpFilename1, iComponentCount*vVolumeSize.volume(), bSigned, Histogram1D, pMasterController);
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

    iHeaderSkip = 0; // the new file is straigt raw without any header
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
  UINT32 iMaxVal = vVolumeSize.maxVal();

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
		default : for (UINT64 i = 0;i<iComponentCount;i++) vSem.push_back(eType);
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

  pMasterController->DebugOut()->Message("RAWConverter::ConvertRAWDataset","Removing temporary files...");

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
 * @param pMasterController controller, for reporting errors
 * @param iHeaderSkip number of bytes to skip off of strFilename */
bool RAWConverter::ExtractGZIPDataset(const string& strFilename,
                                      const string& strUncompressedFile,
                                      MasterController* pMasterController,
                                      UINT64 iHeaderSkip)
{
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
    fclose(f_inflated);
    return false;
  }
  if(f_inflated == NULL) {
    dbg->Error(method, "Could not open %s", strUncompressedFile.c_str());
    fclose(f_compressed);
    return false;
  }

  if(fseek(f_compressed, iHeaderSkip, SEEK_SET) != 0) {
    /// \todo use strerror(errno) and actually report the damn error.
    dbg->Error(method, "Seek failed");
    fclose(f_compressed);
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

  return true;
}

/** Tests a bzip return code for errors, and translates it to a string for the
 * debug logs.
 * @param bz_err the error code (given by the bzip2 library)
 * @param dbg    streams to print error information to
 * @return true if an error occurred */
static bool
bz_err_test(int bz_err, AbstrDebugOut * const dbg)
{
  static const char m[] = "bz_err_test (RAWConverter)";
  bool error_occurred = true;
  switch(bz_err) {
        case BZ_OK:        /* FALL THROUGH */
        case BZ_RUN_OK:    /* FALL THROUGH */
        case BZ_FLUSH_OK:  /* FALL THROUGH */
        case BZ_FINISH_OK:
            dbg->Message(m, "Bzip operation successful.");
            error_occurred = false;
            break;
        case BZ_STREAM_END:
            dbg->Message(m, "End of bzip stream.");
            break;
        case BZ_CONFIG_ERROR:
            dbg->Error(m, "Bzip configuration error");
            break;
        case BZ_SEQUENCE_ERROR:
            dbg->Error(m, "Bzip sequencing error");
            break;
        case BZ_PARAM_ERROR:
            dbg->Error(m, "Bzip parameter error");
            break;
        case BZ_MEM_ERROR:
            dbg->Error(m, "Bzip memory allocation failed.");
            break;
        case BZ_DATA_ERROR_MAGIC:
            dbg->Warning(m, "Bzip stream does not have correct magic bytes!");
            /* FALL THROUGH */
        case BZ_DATA_ERROR:
            dbg->Error(m, "Bzip data integrity error; this usually means the "
                          "compressed file is corrupt.");
            break;
        case BZ_IO_ERROR: {
            const char *err_msg = strerror(errno);
            dbg->Error(m, "Bzip IO error: %s", err_msg);
            break;
        }
        case BZ_UNEXPECTED_EOF:
            dbg->Warning(m, "EOF before end of Bzip stream.");
            break;
        case BZ_OUTBUFF_FULL:
            dbg->Error(m, "Bzip output buffer is not large enough");
            break;
    }
    return error_occurred;
}

/** Converts a bzip2-compressed file chunk to a raw file.
 * @param strFilename the input (compressed) file
 * @param strTargetFilename the target uvf file
 * @param pMasterController controller, for reporting errors
 * @param iHeaderSkip number of bytes to skip of strFilename's header*/
bool RAWConverter::ExtractBZIP2Dataset(const string& strFilename,
                                       const string& strUncompressedFile,
                                       MasterController* pMasterController,
                                       UINT64 iHeaderSkip)
{
  AbstrDebugOut *dbg = pMasterController->DebugOut();
  static const char method[] = "RAWConverter::ConvertBZIP2Dataset";
  BZFILE *bzf;
  int bz_err;
  std::vector<char> buffer(INCORESIZE);

  FILE *f_compressed = fopen(strFilename.c_str(), "rb");
  FILE *f_inflated = fopen(strUncompressedFile.c_str(), "wb");

  if(f_compressed == NULL) {
    dbg->Error(method, "Could not open %s", strFilename.c_str());
    fclose(f_inflated);
    return false;
  }
  if(f_inflated == NULL) {
    dbg->Error(method, "Could not open %s", strUncompressedFile.c_str());
    fclose(f_compressed);
    return false;
  }

  if(fseek(f_compressed, iHeaderSkip, SEEK_SET) != 0) {
    /// \todo use strerror(errno) and actually report the damn error.
    dbg->Error(method, "Seek failed");
    fclose(f_inflated);
    fclose(f_compressed);
    return false;
  }

  bzf = BZ2_bzReadOpen(&bz_err, f_compressed, 0, 0, NULL, 0);
  if(bz_err_test(bz_err, dbg)) {
    dbg->Error(method, "Bzip library error occurred; bailing.");
    fclose(f_inflated);
    fclose(f_compressed);
    return false;
  }

  do {
    int nbytes = BZ2_bzRead(&bz_err, bzf, &buffer[0], INCORESIZE);
    if(bz_err != BZ_STREAM_END && bz_err_test(bz_err, dbg)) {
      dbg->Error(method, "Bzip library error occurred; bailing.");
      fclose(f_inflated);
      fclose(f_compressed);
      return false;
    }
    if(1 != fwrite(&buffer[0], nbytes, 1, f_inflated)) {
      dbg->Warning(method, "%d-byte write of decompressed file failed.",
                   nbytes);
      fclose(f_inflated);
      fclose(f_compressed);
      return false;
    }
  } while(bz_err == BZ_OK);

  fclose(f_inflated);
  fclose(f_compressed);
  
  return true;
}

bool RAWConverter::ParseTXTDataset(const string& strFilename, 
                                     const string& strBinaryFile, 
                                     MasterController* pMasterController,
                                     UINT64 iHeaderSkip, 
                                     UINT64 iComponentSize, 
                                     UINT64 iComponentCount, 
                                     bool bSigned,
                                     bool bIsFloat,
                                     UINTVECTOR3 vVolumeSize)
{
  ifstream sourceFile(strFilename.c_str(),ios::binary);
  if (!sourceFile.is_open()) {
    pMasterController->DebugOut()->Error("RAWConverter::ConvertTXTDataset","Unable to open source file %s.", strFilename.c_str());
    return false;
  }

  LargeRAWFile binaryFile(strBinaryFile);
  binaryFile.Create(iComponentSize/8 * iComponentCount * vVolumeSize.volume());
  if (!binaryFile.IsOpen()) {
    pMasterController->DebugOut()->Error("RAWConverter::ConvertTXTDataset","Unable to open temp file %s.", strBinaryFile.c_str());
    sourceFile.close();
    return false;
  }

  sourceFile.seekg(iHeaderSkip);
  if (bIsFloat) { 
    if (!bSigned) {
      pMasterController->DebugOut()->Error("RAWConverter::ConvertTXTDataset","Unable unsupported data type. (unsiged float)");
      sourceFile.close();
      binaryFile.Delete();
      return false;
    }
    switch (iComponentSize) {
      case 32 : {
                  while (! sourceFile.eof() )
                  {
                    float tmp;
                    sourceFile >> tmp;
                    binaryFile.WriteRAW((unsigned char*)&tmp,4);
                  }
                 break;
               }
      case 64 : {
                  while (! sourceFile.eof() )
                  {
                    double tmp;
                    sourceFile >> tmp;
                    binaryFile.WriteRAW((unsigned char*)&tmp,8);
                  }
                 break;
               }
      default : {
                  pMasterController->DebugOut()->Error("RAWConverter::ConvertTXTDataset","Unable unsupported data type. (float)");
                  sourceFile.close();
                  binaryFile.Delete();
                  return false;
                }
    }
  } else {
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
                      UINT32 tmp;
                      sourceFile >> tmp;
                      binaryFile.WriteRAW((unsigned char*)&tmp,4);
                    }
                  }
                 break;
               }
      default : {
                  pMasterController->DebugOut()->Error("RAWConverter::ConvertTXTDataset","Unable unsupported data type. (int)");
                  sourceFile.close();
                  binaryFile.Delete();
                  return false;
                }
    }
  }
  binaryFile.Close();
  sourceFile.close();

  return true;
}

bool RAWConverter::ConvertToNative(const std::string& strRawFilename, const std::string& strTargetFilename, UINT64 iHeaderSkip,
                                   UINT64 iComponentSize, UINT64 , bool , bool, 
                                   UINTVECTOR3 , FLOATVECTOR3 , MasterController* pMasterController, bool) {
  // convert raw to raw is easy :-), just copy the file and ignore the metadata

  // if the file exists, delete it first
  if (SysTools::FileExists(strTargetFilename)) 
    remove(strTargetFilename.c_str());
  if (SysTools::FileExists(strTargetFilename)) {
    pMasterController->DebugOut()->Error("RAWConverter::ConvertToNative","Unable to remove exisitng target file %s.", strTargetFilename.c_str());
    return false;
  }

  return AppendRAW(strRawFilename, iHeaderSkip, strTargetFilename, iComponentSize, pMasterController, EndianConvert::IsBigEndian());
}

bool RAWConverter::AppendRAW(const std::string& strRawFilename, UINT64 iHeaderSkip, const std::string& strTargetFilename,
                             UINT64 iComponentSize, MasterController* pMasterController, bool bChangeEndianess, bool bToSigned) {
  // open source file
  LargeRAWFile fSource(strRawFilename, iHeaderSkip);
  fSource.Open(false);
  if (!fSource.IsOpen()) {
    pMasterController->DebugOut()->Error("RAWConverter::AppendRAW","Unable to open source file %s.", strRawFilename.c_str());
    return false;
  }
  // append to target file
  LargeRAWFile fTarget(strTargetFilename);
  fTarget.Append();
  if (!fTarget.IsOpen()) {
    fSource.Close();
    pMasterController->DebugOut()->Error("RAWConverter::AppendRAW","Unable to open target file %s.", strTargetFilename.c_str());
    return false;
  }

  UINT64 iCopySize = min(fSource.GetCurrentSize(),BLOCK_COPY_SIZE);
  unsigned char* pBuffer = new unsigned char[size_t(iCopySize)];

  do {
    iCopySize = fSource.ReadRAW(pBuffer, iCopySize);

    if (bToSigned) {
      switch (iComponentSize) {
        case 8  : // char to uchar
                  for (size_t i = 0;i<iCopySize;i++)
                    (*(char*)(pBuffer+i)) = char(*(unsigned char*)(pBuffer+i)) - std::numeric_limits<char>::max();
                  break;
        case 16 : // short to ushort
                  for (size_t i = 0;i<iCopySize;i+=2)
                    (*(short*)(pBuffer+i)) = short(*(unsigned short*)(pBuffer+i)) - std::numeric_limits<short>::max();
                  break;
        case 32 : // int to uint
                  for (size_t i = 0;i<iCopySize;i+=4)
                    (*(int*)(pBuffer+i)) = int(*(unsigned int*)(pBuffer+i)) - std::numeric_limits<int>::max();
                  break;
        case 64 : // ulonglong to longlong
                  for (size_t i = 0;i<iCopySize;i+=8)
                    (*(INT64*)(pBuffer+i)) = INT64(*(UINT64*)(pBuffer+i)) - std::numeric_limits<INT64>::max();
                  break;
        default : pMasterController->DebugOut()->Error("VFFConverter::ConvertToNative","Unsuported data type for vff files.");
                  return false;
      }
    }

    if (bChangeEndianess) {
      switch (iComponentSize) {
        case 16 : for (size_t i = 0;i<iCopySize;i+=2)
                    EndianConvert::Swap<unsigned short>((unsigned short*)(pBuffer+i));
                  break;
        case 32 : for (size_t i = 0;i<iCopySize;i+=4)
                    EndianConvert::Swap<float>((float*)(pBuffer+i));
                  break;
        case 64 : for (size_t i = 0;i<iCopySize;i+=8)
                    EndianConvert::Swap<double>((double*)(pBuffer+i));
                  break;
      }
    }

    fTarget.WriteRAW(pBuffer, iCopySize);
  } while (iCopySize > 0); 

  fSource.Close();
  fTarget.Close();
  delete [] pBuffer;    

  return true;
}


bool RAWConverter::ConvertToUVF(const std::string& strSourceFilename, const std::string& strTargetFilename, 
                                const std::string& strTempDir, MasterController* pMasterController, 
                                bool bNoUserInteraction) {

  UINT64        iHeaderSkip; 
  UINT64        iComponentSize;
  UINT64        iComponentCount; 
  bool          bConvertEndianess;
  bool          bSigned;
  bool          bIsFloat;
  UINTVECTOR3   vVolumeSize;
  FLOATVECTOR3  vVolumeAspect;
  string        strTitle;
  string        strSource;
  UVFTables::ElementSemanticTable eType;
  string        strIntermediateFile;
  bool          bDeleteIntermediateFile;

  bool bRAWCreated = ConvertToRAW(strSourceFilename, strTempDir, pMasterController, bNoUserInteraction, 
                                  iHeaderSkip, iComponentSize, iComponentCount, bConvertEndianess, bSigned, 
                                  bIsFloat, vVolumeSize, vVolumeAspect, strTitle, strSource, eType, strIntermediateFile, bDeleteIntermediateFile);

  if (!bRAWCreated) {
    pMasterController->DebugOut()->Error("RAWConverter::ConvertToUVF","Convert to RAW step failed, aborting.");
    return false;    
  }

  bool bUVFCreated = ConvertRAWDataset(strIntermediateFile, strTargetFilename, strTempDir, pMasterController, iHeaderSkip, iComponentSize, iComponentCount, bConvertEndianess, bSigned,
                                       bIsFloat, vVolumeSize, vVolumeAspect, strTitle, SysTools::GetFilename(strSourceFilename));

  if (bDeleteIntermediateFile) remove(strIntermediateFile.c_str());

  return bUVFCreated;
}
