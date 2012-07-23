TEMPLATE          = lib
win32:TEMPLATE    = vclib
CONFIG            = create_prl exceptions largefile rtti static staticlib stl
CONFIG           += warn_on
DEFINES          += TUVOK_NO_QT
TARGET            = tuvokio
win32 { DESTDIR   = Build }
OBJECTS_DIR       = Build/objects
d = ../Basics ../ 3rdParty/cxxtest 3rdParty/boost 3rdParty exception .
DEPENDPATH        = $$d
INCLUDEPATH       = $$d
LIBS              = -Lexpressions -ltuvokexpr
unix:LIBS        += -lz
macx:LIBS        += -framework CoreFoundation
win32:LIBS       += shlwapi.lib
unix:QMAKE_CXXFLAGS += -std=c++0x
unix:QMAKE_CXXFLAGS += -fno-strict-aliasing
unix:QMAKE_CFLAGS += -fno-strict-aliasing

# Find the location of QtGui's prl file, and include it here so we can look at
# the QMAKE_PRL_CONFIG variable.
TEMP = $$[QT_INSTALL_LIBS] libQtGui.prl
PRL  = $$[QT_INSTALL_LIBS] QtGui.framework/QtGui.prl
TEMP = $$join(TEMP, "/")
PRL  = $$join(PRL, "/")
exists($$TEMP) {
  include($$TEMP)
}
exists($$PRL) {
  include($$PRL)
}

# Input
SOURCES += \
  ./3rdParty/bzip2/blocksort.c \
  ./3rdParty/bzip2/bzlib.c \
  ./3rdParty/bzip2/compress.c \
  ./3rdParty/bzip2/crctable.c \
  ./3rdParty/bzip2/decompress.c \
  ./3rdParty/bzip2/huffman.c \
  ./3rdParty/bzip2/randtable.c \
  ./3rdParty/jpeglib/cdjpeg.c \
  ./3rdParty/jpeglib/jcapimin.c \
  ./3rdParty/jpeglib/jcapistd.c \
  ./3rdParty/jpeglib/jccoefct.c \
  ./3rdParty/jpeglib/jccolor.c \
  ./3rdParty/jpeglib/jcdctmgr.c \
  ./3rdParty/jpeglib/jcdiffct.c \
  ./3rdParty/jpeglib/jchuff.c \
  ./3rdParty/jpeglib/jcinit.c \
  ./3rdParty/jpeglib/jclhuff.c \
  ./3rdParty/jpeglib/jclossls.c \
  ./3rdParty/jpeglib/jclossy.c \
  ./3rdParty/jpeglib/jcmainct.c \
  ./3rdParty/jpeglib/jcmarker.c \
  ./3rdParty/jpeglib/jcmaster.c \
  ./3rdParty/jpeglib/jcodec.c \
  ./3rdParty/jpeglib/jcomapi.c \
  ./3rdParty/jpeglib/jcparam.c \
  ./3rdParty/jpeglib/jcphuff.c \
  ./3rdParty/jpeglib/jcpred.c \
  ./3rdParty/jpeglib/jcprepct.c \
  ./3rdParty/jpeglib/jcsample.c \
  ./3rdParty/jpeglib/jcscale.c \
  ./3rdParty/jpeglib/jcshuff.c \
  ./3rdParty/jpeglib/jctrans.c \
  ./3rdParty/jpeglib/jdapimin.c \
  ./3rdParty/jpeglib/jdapistd.c \
  ./3rdParty/jpeglib/jdatadst.c \
  ./3rdParty/jpeglib/jdatasrc.c \
  ./3rdParty/jpeglib/jdcoefct.c \
  ./3rdParty/jpeglib/jdcolor.c \
  ./3rdParty/jpeglib/jddctmgr.c \
  ./3rdParty/jpeglib/jddiffct.c \
  ./3rdParty/jpeglib/jdhuff.c \
  ./3rdParty/jpeglib/jdinput.c \
  ./3rdParty/jpeglib/jdlhuff.c \
  ./3rdParty/jpeglib/jdlossls.c \
  ./3rdParty/jpeglib/jdlossy.c \
  ./3rdParty/jpeglib/jdmainct.c \
  ./3rdParty/jpeglib/jdmarker.c \
  ./3rdParty/jpeglib/jdmaster.c \
  ./3rdParty/jpeglib/jdmerge.c \
  ./3rdParty/jpeglib/jdphuff.c \
  ./3rdParty/jpeglib/jdpostct.c \
  ./3rdParty/jpeglib/jdpred.c \
  ./3rdParty/jpeglib/jdsample.c \
  ./3rdParty/jpeglib/jdscale.c \
  ./3rdParty/jpeglib/jdshuff.c \
  ./3rdParty/jpeglib/jdtrans.c \
  ./3rdParty/jpeglib/jerror.c \
  ./3rdParty/jpeglib/jfdctflt.c \
  ./3rdParty/jpeglib/jfdctfst.c \
  ./3rdParty/jpeglib/jfdctint.c \
  ./3rdParty/jpeglib/jidctflt.c \
  ./3rdParty/jpeglib/jidctfst.c \
  ./3rdParty/jpeglib/jidctint.c \
  ./3rdParty/jpeglib/jidctred.c \
  ./3rdParty/jpeglib/jmemmgr.c \
  ./3rdParty/jpeglib/jmemnobs.c \
  ./3rdParty/jpeglib/jmemsrc.c \
  ./3rdParty/jpeglib/jquant1.c \
  ./3rdParty/jpeglib/jquant2.c \
  ./3rdParty/jpeglib/jutils.c \
  ./3rdParty/jpeglib/rdcolmap.c \
  ./3rdParty/jpeglib/rdswitch.c \
  ./3rdParty/jpeglib/transupp.c \
  ./3rdParty/tiff/tif_aux.c \
  ./3rdParty/tiff/tif_close.c \
  ./3rdParty/tiff/tif_codec.c \
  ./3rdParty/tiff/tif_color.c \
  ./3rdParty/tiff/tif_compress.c \
  ./3rdParty/tiff/tif_dir.c \
  ./3rdParty/tiff/tif_dirinfo.c \
  ./3rdParty/tiff/tif_dirread.c \
  ./3rdParty/tiff/tif_dirwrite.c \
  ./3rdParty/tiff/tif_dumpmode.c \
  ./3rdParty/tiff/tif_error.c \
  ./3rdParty/tiff/tif_extension.c \
  ./3rdParty/tiff/tif_fax3.c \
  ./3rdParty/tiff/tif_fax3sm.c \
  ./3rdParty/tiff/tif_flush.c \
  ./3rdParty/tiff/tif_getimage.c \
  ./3rdParty/tiff/tif_luv.c \
  ./3rdParty/tiff/tif_lzw.c \
  ./3rdParty/tiff/tif_next.c \
  ./3rdParty/tiff/tif_open.c \
  ./3rdParty/tiff/tif_packbits.c \
  ./3rdParty/tiff/tif_pixarlog.c \
  ./3rdParty/tiff/tif_predict.c \
  ./3rdParty/tiff/tif_print.c \
  ./3rdParty/tiff/tif_read.c \
  ./3rdParty/tiff/tif_strip.c \
  ./3rdParty/tiff/tif_swab.c \
  ./3rdParty/tiff/tif_thunder.c \
  ./3rdParty/tiff/tif_tile.c \
  ./3rdParty/tiff/tif_unix.c \
  ./3rdParty/tiff/tif_version.c \
  ./3rdParty/tiff/tif_warning.c \
  ./3rdParty/tiff/tif_write.c \
  ./3rdParty/tiff/tif_zip.c \
  ./AbstrConverter.cpp \
  ./AbstrGeoConverter.cpp \
  ./AnalyzeConverter.cpp \
  ./BOVConverter.cpp \
  ./BrickedDataset.cpp \
  ./Dataset.cpp \
  ./DICOM/DICOMParser.cpp \
  ./DirectoryParser.cpp \
  ./DSFactory.cpp \
  ./ExternalDataset.cpp \
  ./FileBackedDataset.cpp \
  ./G3D.cpp \
  ./gzio.c \
  ./I3MConverter.cpp \
  ./IASSConverter.cpp \
  ./Images/ImageParser.cpp \
  ./Images/StackExporter.cpp \
  ./InveonConverter.cpp \
  ./IOManager.cpp \
  ./KeyValueFileParser.cpp \
  ./KitwareConverter.cpp \
  ./MedAlyVisFiberTractGeoConverter.cpp \
  ./MedAlyVisGeoConverter.cpp \
  ./MobileGeoConverter.cpp \
  ./NRRDConverter.cpp \
  ./OBJGeoConverter.cpp \
  ./PLYGeoConverter.cpp \
  ./XML3DGeoConverter.cpp \
  ./QVISConverter.cpp \
  ./RAWConverter.cpp \
  ./REKConverter.cpp \
  ./StkConverter.cpp \
  ./TiffVolumeConverter.cpp \
  ./TransferFunction1D.cpp \
  ./TransferFunction2D.cpp \
  ./TuvokJPEG.cpp \
  ./UVF/DataBlock.cpp \
  ./uvfDataset.cpp \
  ./UVF/GeometryDataBlock.cpp \
  ./UVF/GlobalHeader.cpp \
  ./UVF/Histogram1DDataBlock.cpp \
  ./UVF/Histogram2DDataBlock.cpp \
  ./UVF/KeyValuePairDataBlock.cpp \
  ./UVF/MaxMinDataBlock.cpp \
  ./UVF/ExtendedOctree/ExtendedOctree.cpp
  ./UVF/ExtendedOctree/ExtendedOctreeConverter.cpp
  ./UVF/ExtendedOctree/VolumeTools.cpp
  ./uvfMesh.cpp \
  ./UVF/RasterDataBlock.cpp \
  ./UVF/UVF.cpp \
  ./UVF/UVFTables.cpp \
  ./VariantArray.cpp \
  ./VFFConverter.cpp \
  ./VGIHeaderParser.cpp \
  ./VGStudioConverter.cpp

HEADERS += \
  ./3rdParty/bzip2/bzlib.h \
  ./3rdParty/bzip2/bzlib_private.h \
  ./3rdParty/jpeglib/cderror.h \
  ./3rdParty/jpeglib/cdjpeg.h \
  ./3rdParty/jpeglib/jchuff.h \
  ./3rdParty/jpeglib/jconfig.h \
  ./3rdParty/jpeglib/jdct.h \
  ./3rdParty/jpeglib/jdhuff.h \
  ./3rdParty/jpeglib/jerror.h \
  ./3rdParty/jpeglib/jinclude.h \
  ./3rdParty/jpeglib/jlossls.h \
  ./3rdParty/jpeglib/jlossy.h \
  ./3rdParty/jpeglib/jmemsys.h \
  ./3rdParty/jpeglib/jmorecfg.h \
  ./3rdParty/jpeglib/jpegint.h \
  ./3rdParty/jpeglib/jpeglib.h \
  ./3rdParty/jpeglib/jversion.h \
  ./3rdParty/jpeglib/mangle_jpeg.h \
  ./3rdParty/jpeglib/transupp.h \
  ./3rdParty/tiff/t4.h \
  ./3rdParty/tiff/tif_config.h \
  ./3rdParty/tiff/tif_dir.h \
  ./3rdParty/tiff/tif_fax3.h \
  ./3rdParty/tiff/tiffconf.h \
  ./3rdParty/tiff/tiff.h \
  ./3rdParty/tiff/tiffio.h \
  ./3rdParty/tiff/tiffiop.h \
  ./3rdParty/tiff/tiffvers.h \
  ./3rdParty/tiff/tif_predict.h \
  ./3rdParty/tiff/uvcode.h \
  ./3rdParty/zlib/zconf.h \
  ./3rdParty/zlib/zlib.h \
  ./AbstrConverter.h \
  ./AbstrGeoConverter.h \
  ./AnalyzeConverter.h \
  ./BOVConverter.h \
  ./BrickedDataset.h \
  ./Brick.h \
  ./Dataset.h \
  ./DICOM/DICOMParser.h \
  ./DirectoryParser.h \
  ./DSFactory.h \
  ./exception/FileNotFound.h \
  ./exception/IOException.h \
  ./exception/ReadTimeout.h \
  ./exception/UnmergeableDatasets.h \
  ./ExternalDataset.h \
  ./FileBackedDataset.h \
  ./G3D.h \
  ./gzio.h \
  ./I3MConverter.h \
  ./IASSConverter.h \
  ./Images/ImageParser.h \
  ./Images/StackExporter.h \
  ./InveonConverter.h \
  ./IOManager.h \
  ./KeyValueFileParser.h \
  ./KitwareConverter.h \
  ./MedAlyVisFiberTractGeoConverter.h \
  ./MedAlyVisGeoConverter.h \
  ./MobileGeoConverter.h \
  ./NRRDConverter.h \
  ./OBJGeoConverter.h \
  ./PLYGeoConverter.h \
  ./XML3DGeoConverter.h \
  ./Quantize.h \
  ./QVISConverter.h \
  ./RAWConverter.h \
  ./REKConverter.h \
  ./StkConverter.h \
  ./test/dicom.h \
  ./test/jpeg.h \
  ./test/minmax.h \
  ./test/multisrc.h \
  ./test/quantize.h \
  ./test/util-test.h \
  ./TiffVolumeConverter.h \
  ./TransferFunction1D.h \
  ./TransferFunction2D.h \
  ./TuvokIOError.h \
  ./TuvokJPEG.h \
  ./Tuvok_QtPlugins.h \
  ./UVF/DataBlock.h \
  ./uvfDataset.h \
  ./UVF/GeometryDataBlock.h \
  ./UVF/GlobalHeader.h \
  ./UVF/Histogram1DDataBlock.h \
  ./UVF/Histogram2DDataBlock.h \
  ./UVF/KeyValuePairDataBlock.h \
  ./UVF/MaxMinDataBlock.h \
  ./uvfMesh.h \
  ./UVF/RasterDataBlock.h \
  ./UVF/UVFBasic.h \
  ./UVF/UVF.h \
  ./UVF/UVFTables.h \
  ./UVF/ExtendedOctree/ExtendedOctree.h
  ./UVF/ExtendedOctree/ExtendedOctreeConverter.h
  ./UVF/ExtendedOctree/VolumeTools.h
  ./VariantArray.h \
  ./VFFConverter.h \
  ./VGIHeaderParser.h \
  ./VGStudioConverter.h
