TEMPLATE          = lib
CONFIG           += staticlib static create_prl warn_on stl exceptions
TARGET            = Tuvok
VERSION           = 0.0.1
TARGET            = Build/Tuvok
RCC_DIR           = Build/rcc
OBJECTS_DIR       = Build/objects
DEPENDPATH       += . Basics Controller DebugOut IO Renderer Scripting
INCLUDEPATH      += . 3rdParty/GLEW IO/3rdParty/boost
QT               += opengl
LIBS             += -lz
unix:QMAKE_CXXFLAGS += -fno-strict-aliasing
unix:QMAKE_CFLAGS += -fno-strict-aliasing

# If this is a 10.5 machine, build for both x86 and x86_64.  Not
# the best idea (there's no guarantee the machine will have a
# 64bit compiler), but the best we can do via qmake.
macx {
    exists(/Developer/SDKs/MacOSX10.5.sdk/) {
        CONFIG += x86 x86_64
    }
}

### Should we link Qt statically or as a shared lib?
# Find the location of QtGui's prl file, and include it here so we can look at
# the QMAKE_PRL_CONFIG variable.
TEMP = $$[QT_INSTALL_LIBS] libQtGui.prl
PRL  = $$[QT_INSTALL_LIBS] QtGui.framework/QtGui.prl
include($$join(TEMP, "/"))
include($$join(PRL, "/"))

# If that contains the `shared' configuration, the installed Qt is shared.
# In that case, disable the image plugins.
contains(QMAKE_PRL_CONFIG, shared) {
  message("Shared build, ensuring there will be image plugins linked in.")
  QTPLUGIN -= qgif qjpeg
} else {
  message("Static build, forcing image plugins to get loaded.")
  QTPLUGIN += qgif qjpeg
}

# Input
HEADERS += \
           3rdParty/bzip2/bzlib.h \
           3rdParty/bzip2/bzlib_private.h \
           3rdParty/GLEW/GL/glew.h \
           3rdParty/GLEW/GL/glxew.h \
           3rdParty/GLEW/GL/wglew.h \
           3rdParty/jpeglib/cderror.h \
           3rdParty/jpeglib/cdjpeg.h \
           3rdParty/jpeglib/jchuff.h \
           3rdParty/jpeglib/jconfig.h \
           3rdParty/jpeglib/jdct.h \
           3rdParty/jpeglib/jdhuff.h \
           3rdParty/jpeglib/jerror.h \
           3rdParty/jpeglib/jinclude.h \
           3rdParty/jpeglib/jlossls.h \
           3rdParty/jpeglib/jlossy.h \
           3rdParty/jpeglib/jmemsys.h \
           3rdParty/jpeglib/jmorecfg.h \
           3rdParty/jpeglib/jpegint.h \
           3rdParty/jpeglib/jpeglib.h \
           3rdParty/jpeglib/jversion.h \
           3rdParty/jpeglib/mangle_jpeg.h \
           3rdParty/jpeglib/transupp.h \
           3rdParty/tiff/t4.h \
           3rdParty/tiff/tif_dir.h \
           3rdParty/tiff/tif_fax3.h \
           3rdParty/tiff/tiffconf.h \
           3rdParty/tiff/tiff.h \
           3rdParty/tiff/tiffio.h \
           3rdParty/tiff/tiffiop.h \
           3rdParty/tiff/tiffvers.h \
           3rdParty/tiff/tif_predict.h \
           3rdParty/tiff/uvcode.h \
           Basics/Appendix.h \
           Basics/ArcBall.h \
           Basics/Checksums/crc32.h \
           Basics/Checksums/MD5.h \
           Basics/GeometryGenerator.h \
           Basics/Grids.h \
           Basics/LargeRAWFile.h \
           Basics/MathTools.h \
           Basics/MC.h \
           Basics/Plane.h \
           Basics/Timer.h \
           Basics/SysTools.h \
           Basics/Vectors.h \
           Controller/Controller.h \
           Controller/MasterController.h \
           DebugOut/AbstrDebugOut.h \
           DebugOut/ConsoleOut.h \
           DebugOut/MultiplexOut.h \
           DebugOut/TextfileOut.h \
           IO/AbstrConverter.h \
           IO/BOVConverter.h \
           IO/BrickedDataset.h \
           IO/Dataset.h \
           IO/DICOM/DICOMParser.h \
           IO/DirectoryParser.h \
           IO/ExternalDataset.h \
           IO/gzio.h \
           IO/I3MConverter.h \
           IO/Images/ImageParser.h \
           IO/IOManager.h \
           IO/KeyValueFileParser.h \
           IO/NRRDConverter.h \
           IO/Quantize.h \
           IO/QVISConverter.h \
           IO/RAWConverter.h \
           IO/REKConverter.h \
           IO/StkConverter.h \
           IO/TiffVolumeConverter.h \
           IO/Transferfunction1D.h \
           IO/Transferfunction2D.h \
           IO/TuvokJPEG.h \
           IO/Tuvok_QtPlugins.h \
           IO/UVF/DataBlock.h \
           IO/uvfDataset.h \
           IO/UVF/GlobalHeader.h \
           IO/UVF/Histogram1DDataBlock.h \
           IO/UVF/Histogram2DDataBlock.h \
           IO/UVF/KeyValuePairDataBlock.h \
           IO/UVF/MaxMinDataBlock.h \
           IO/UVF/RasterDataBlock.h \
           IO/UVF/UVFBasic.h \
           IO/UVF/UVF.h \
           IO/UVF/UVFTables.h \
           IO/VariantArray.h \
           IO/VFFConverter.h \
           Renderer/AbstrRenderer.h \
           Renderer/Context.h \
           Renderer/ContextID.h \
           Renderer/CullingLOD.h \
           Renderer/FrameCapture.h \
           Renderer/GL/GLContextID.h \
           Renderer/GL/GLFrameCapture.h \
           Renderer/GL/GLInclude.h \
           Renderer/GL/GLObject.h \
           Renderer/GL/GLRaycaster.h \
           Renderer/GL/GLRenderer.h \
           Renderer/GL/GLSBVR.h \
           Renderer/GL/GLSLProgram.h \
           Renderer/GL/GLTargetBinder.h \
           Renderer/GL/GLTexture1D.h \
           Renderer/GL/GLTexture2D.h \
           Renderer/GL/GLTexture3D.h \
           Renderer/GL/GLTexture.h \
           Renderer/GL/QtGLContextID.h \
           Renderer/GPUMemMan/GPUMemManDataStructs.h \
           Renderer/GPUMemMan/GPUMemMan.h \
           Renderer/GPUObject.h \
           Renderer/SBVRGeoGen.h \
           Renderer/TFScaling.h \
           Scripting/Scriptable.h \
           Scripting/Scripting.h \
           StdTuvokDefines.h

SOURCES += \
           3rdParty/bzip2/blocksort.c \
           3rdParty/bzip2/bzlib.c \
           3rdParty/bzip2/compress.c \
           3rdParty/bzip2/crctable.c \
           3rdParty/bzip2/decompress.c \
           3rdParty/bzip2/huffman.c \
           3rdParty/bzip2/randtable.c \
           3rdParty/GLEW/GL/glew.c \
           3rdParty/jpeglib/cdjpeg.c \
           3rdParty/jpeglib/jcapimin.c \
           3rdParty/jpeglib/jcapistd.c \
           3rdParty/jpeglib/jccoefct.c \
           3rdParty/jpeglib/jccolor.c \
           3rdParty/jpeglib/jcdctmgr.c \
           3rdParty/jpeglib/jcdiffct.c \
           3rdParty/jpeglib/jchuff.c \
           3rdParty/jpeglib/jcinit.c \
           3rdParty/jpeglib/jclhuff.c \
           3rdParty/jpeglib/jclossls.c \
           3rdParty/jpeglib/jclossy.c \
           3rdParty/jpeglib/jcmainct.c \
           3rdParty/jpeglib/jcmarker.c \
           3rdParty/jpeglib/jcmaster.c \
           3rdParty/jpeglib/jcodec.c \
           3rdParty/jpeglib/jcomapi.c \
           3rdParty/jpeglib/jcparam.c \
           3rdParty/jpeglib/jcphuff.c \
           3rdParty/jpeglib/jcpred.c \
           3rdParty/jpeglib/jcprepct.c \
           3rdParty/jpeglib/jcsample.c \
           3rdParty/jpeglib/jcscale.c \
           3rdParty/jpeglib/jcshuff.c \
           3rdParty/jpeglib/jctrans.c \
           3rdParty/jpeglib/jdapimin.c \
           3rdParty/jpeglib/jdapistd.c \
           3rdParty/jpeglib/jdatadst.c \
           3rdParty/jpeglib/jdatasrc.c \
           3rdParty/jpeglib/jdcoefct.c \
           3rdParty/jpeglib/jdcolor.c \
           3rdParty/jpeglib/jddctmgr.c \
           3rdParty/jpeglib/jddiffct.c \
           3rdParty/jpeglib/jdhuff.c \
           3rdParty/jpeglib/jdinput.c \
           3rdParty/jpeglib/jdlhuff.c \
           3rdParty/jpeglib/jdlossls.c \
           3rdParty/jpeglib/jdlossy.c \
           3rdParty/jpeglib/jdmainct.c \
           3rdParty/jpeglib/jdmarker.c \
           3rdParty/jpeglib/jdmaster.c \
           3rdParty/jpeglib/jdmerge.c \
           3rdParty/jpeglib/jdphuff.c \
           3rdParty/jpeglib/jdpostct.c \
           3rdParty/jpeglib/jdpred.c \
           3rdParty/jpeglib/jdsample.c \
           3rdParty/jpeglib/jdscale.c \
           3rdParty/jpeglib/jdshuff.c \
           3rdParty/jpeglib/jdtrans.c \
           3rdParty/jpeglib/jerror.c \
           3rdParty/jpeglib/jfdctflt.c \
           3rdParty/jpeglib/jfdctfst.c \
           3rdParty/jpeglib/jfdctint.c \
           3rdParty/jpeglib/jidctflt.c \
           3rdParty/jpeglib/jidctfst.c \
           3rdParty/jpeglib/jidctint.c \
           3rdParty/jpeglib/jidctred.c \
           3rdParty/jpeglib/jmemmgr.c \
           3rdParty/jpeglib/jmemnobs.c \
           3rdParty/jpeglib/jquant1.c \
           3rdParty/jpeglib/jquant2.c \
           3rdParty/jpeglib/jutils.c \
           3rdParty/jpeglib/rdcolmap.c \
           3rdParty/jpeglib/rdswitch.c \
           3rdParty/jpeglib/transupp.c \
           3rdParty/tiff/tif_aux.c \
           3rdParty/tiff/tif_close.c \
           3rdParty/tiff/tif_codec.c \
           3rdParty/tiff/tif_color.c \
           3rdParty/tiff/tif_compress.c \
           3rdParty/tiff/tif_dir.c \
           3rdParty/tiff/tif_dirinfo.c \
           3rdParty/tiff/tif_dirread.c \
           3rdParty/tiff/tif_dirwrite.c \
           3rdParty/tiff/tif_dumpmode.c \
           3rdParty/tiff/tif_error.c \
           3rdParty/tiff/tif_extension.c \
           3rdParty/tiff/tif_fax3.c \
           3rdParty/tiff/tif_fax3sm.c \
           3rdParty/tiff/tif_flush.c \
           3rdParty/tiff/tif_getimage.c \
           3rdParty/tiff/tif_luv.c \
           3rdParty/tiff/tif_lzw.c \
           3rdParty/tiff/tif_next.c \
           3rdParty/tiff/tif_open.c \
           3rdParty/tiff/tif_packbits.c \
           3rdParty/tiff/tif_pixarlog.c \
           3rdParty/tiff/tif_predict.c \
           3rdParty/tiff/tif_print.c \
           3rdParty/tiff/tif_read.c \
           3rdParty/tiff/tif_strip.c \
           3rdParty/tiff/tif_swab.c \
           3rdParty/tiff/tif_thunder.c \
           3rdParty/tiff/tif_tile.c \
           3rdParty/tiff/tif_unix.c \
           3rdParty/tiff/tif_version.c \
           3rdParty/tiff/tif_warning.c \
           3rdParty/tiff/tif_write.c \
           3rdParty/tiff/tif_zip.c \
           Basics/Appendix.cpp \
           Basics/ArcBall.cpp \
           Basics/Checksums/MD5.cpp \
           Basics/GeometryGenerator.cpp \
           Basics/LargeRAWFile.cpp \
           Basics/MathTools.cpp \
           Basics/MC.cpp \
           Basics/Plane.cpp \
           Basics/SystemInfo.cpp \
           Basics/Timer.cpp \           
           Basics/SysTools.cpp \
           Controller/MasterController.cpp \
           DebugOut/AbstrDebugOut.cpp \
           DebugOut/ConsoleOut.cpp \
           DebugOut/MultiplexOut.cpp \
           DebugOut/TextfileOut.cpp \
           IO/AbstrConverter.cpp \
           IO/BOVConverter.cpp \
           IO/BrickedDataset.cpp \
           IO/Dataset.cpp \
           IO/DICOM/DICOMParser.cpp \
           IO/DirectoryParser.cpp \
           IO/ExternalDataset.cpp \
           IO/gzio.c \
           IO/I3MConverter.cpp \
           IO/Images/ImageParser.cpp \
           IO/IOManager.cpp \
           IO/KeyValueFileParser.cpp \
           IO/NRRDConverter.cpp \
           IO/QVISConverter.cpp \
           IO/RAWConverter.cpp \
           IO/REKConverter.cpp \
           IO/StkConverter.cpp \
           IO/TiffVolumeConverter.cpp \
           IO/Transferfunction1D.cpp \
           IO/Transferfunction2D.cpp \
           IO/TuvokJPEG.cpp \
           IO/UVF/DataBlock.cpp \
           IO/uvfDataset.cpp \
           IO/UVF/GlobalHeader.cpp \
           IO/UVF/Histogram1DDataBlock.cpp \
           IO/UVF/Histogram2DDataBlock.cpp \
           IO/UVF/KeyValuePairDataBlock.cpp \
           IO/UVF/MaxMinDataBlock.cpp \
           IO/UVF/RasterDataBlock.cpp \
           IO/UVF/UVF.cpp \
           IO/UVF/UVFTables.cpp \
           IO/VariantArray.cpp \
           IO/VFFConverter.cpp \
           Renderer/AbstrRenderer.cpp \
           Renderer/CullingLOD.cpp \
           Renderer/FrameCapture.cpp \
           Renderer/GL/GLFBOTex.cpp \
           Renderer/GL/GLFrameCapture.cpp \
           Renderer/GL/GLRaycaster.cpp \
           Renderer/GL/GLRenderer.cpp \
           Renderer/GL/GLSBVR.cpp \
           Renderer/GL/GLSLProgram.cpp \
           Renderer/GL/GLTargetBinder.cpp \
           Renderer/GL/GLTexture1D.cpp \
           Renderer/GL/GLTexture2D.cpp \
           Renderer/GL/GLTexture3D.cpp \
           Renderer/GL/GLTexture.cpp \
           Renderer/GPUMemMan/GPUMemMan.cpp \
           Renderer/GPUMemMan/GPUMemManDataStructs.cpp \
           Renderer/SBVRGeoGen.cpp \
           Renderer/TFScaling.cpp \
           Scripting/Scripting.cpp

win32 {
  HEADERS += \
             Basics/DynamicDX.h \
             Renderer/DX/DXContextID.h \
             Renderer/DX/DXObject.h \
             Renderer/DX/DXRaycaster.h \
             Renderer/DX/DXRenderer.h \
             Renderer/DX/DXSBVR.h \
             Renderer/DX/DXTexture.h \
             Renderer/DX/DXTexture1D.h \
             Renderer/DX/DXTexture2D.h \
             Renderer/DX/DXTexture3D.h

  SOURCES += \
             Basics/DynamicDX.cpp \
             Renderer/DX/DXRaycaster.cpp \
             Renderer/DX/DXRenderer.cpp \
             Renderer/DX/DXSBVR.cpp \
             Renderer/DX/DXTexture.cpp \
             Renderer/DX/DXTexture1D.cpp \
             Renderer/DX/DXTexture2D.cpp \
             Renderer/DX/DXTexture3D.cpp
}
