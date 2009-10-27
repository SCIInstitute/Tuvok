TEMPLATE          = lib
CONFIG           += warn_on create_prl qt static staticlib stl largefile
CONFIG           += exceptions
macx:DEFINES     += QT_MAC_USE_COCOA=0
TARGET            = Build/Tuvok
RCC_DIR           = Build/rcc
OBJECTS_DIR       = Build/objects
DEPENDPATH       += . Basics Controller DebugOut IO Renderer Scripting
INCLUDEPATH      += . 3rdParty/GLEW IO/3rdParty/boost
QT               += opengl
LIBS             += -lz
macx:LIBS        += -framework CoreFoundation
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

### Should we link Qt statically or as a shared lib?
# If the PRL config contains the `shared' configuration, then the installed
# Qt is shared.  In that case, disable the image plugins.
contains(QMAKE_PRL_CONFIG, shared) {
  message("Shared build, ensuring there will be image plugins linked in.")
  QTPLUGIN -= qgif qjpeg qtiff
} else {
  message("Static build, forcing image plugins to get loaded.")
  QTPLUGIN += qgif qjpeg qtiff
}

# Input
HEADERS += \
           3rdParty/GLEW/GL/glew.h \
           3rdParty/GLEW/GL/glxew.h \
           3rdParty/GLEW/GL/wglew.h \
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
           IO/3rdParty/bzip2/bzlib.h \
           IO/3rdParty/bzip2/bzlib_private.h \
           IO/3rdParty/jpeglib/cderror.h \
           IO/3rdParty/jpeglib/cdjpeg.h \
           IO/3rdParty/jpeglib/jchuff.h \
           IO/3rdParty/jpeglib/jconfig.h \
           IO/3rdParty/jpeglib/jdct.h \
           IO/3rdParty/jpeglib/jdhuff.h \
           IO/3rdParty/jpeglib/jerror.h \
           IO/3rdParty/jpeglib/jinclude.h \
           IO/3rdParty/jpeglib/jlossls.h \
           IO/3rdParty/jpeglib/jlossy.h \
           IO/3rdParty/jpeglib/jmemsys.h \
           IO/3rdParty/jpeglib/jmorecfg.h \
           IO/3rdParty/jpeglib/jpegint.h \
           IO/3rdParty/jpeglib/jpeglib.h \
           IO/3rdParty/jpeglib/jversion.h \
           IO/3rdParty/jpeglib/mangle_jpeg.h \
           IO/3rdParty/jpeglib/transupp.h \
           IO/3rdParty/tiff/t4.h \
           IO/3rdParty/tiff/tif_dir.h \
           IO/3rdParty/tiff/tif_fax3.h \
           IO/3rdParty/tiff/tiffconf.h \
           IO/3rdParty/tiff/tiff.h \
           IO/3rdParty/tiff/tiffio.h \
           IO/3rdParty/tiff/tiffiop.h \
           IO/3rdParty/tiff/tiffvers.h \
           IO/3rdParty/tiff/tif_predict.h \
           IO/3rdParty/tiff/uvcode.h \
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
           Renderer/GL/GLSBVR2S.h \
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
           3rdParty/GLEW/GL/glew.c \
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
           IO/3rdParty/bzip2/blocksort.c \
           IO/3rdParty/bzip2/bzlib.c \
           IO/3rdParty/bzip2/compress.c \
           IO/3rdParty/bzip2/crctable.c \
           IO/3rdParty/bzip2/decompress.c \
           IO/3rdParty/bzip2/huffman.c \
           IO/3rdParty/bzip2/randtable.c \
           IO/3rdParty/jpeglib/cdjpeg.c \
           IO/3rdParty/jpeglib/jcapimin.c \
           IO/3rdParty/jpeglib/jcapistd.c \
           IO/3rdParty/jpeglib/jccoefct.c \
           IO/3rdParty/jpeglib/jccolor.c \
           IO/3rdParty/jpeglib/jcdctmgr.c \
           IO/3rdParty/jpeglib/jcdiffct.c \
           IO/3rdParty/jpeglib/jchuff.c \
           IO/3rdParty/jpeglib/jcinit.c \
           IO/3rdParty/jpeglib/jclhuff.c \
           IO/3rdParty/jpeglib/jclossls.c \
           IO/3rdParty/jpeglib/jclossy.c \
           IO/3rdParty/jpeglib/jcmainct.c \
           IO/3rdParty/jpeglib/jcmarker.c \
           IO/3rdParty/jpeglib/jcmaster.c \
           IO/3rdParty/jpeglib/jcodec.c \
           IO/3rdParty/jpeglib/jcomapi.c \
           IO/3rdParty/jpeglib/jcparam.c \
           IO/3rdParty/jpeglib/jcphuff.c \
           IO/3rdParty/jpeglib/jcpred.c \
           IO/3rdParty/jpeglib/jcprepct.c \
           IO/3rdParty/jpeglib/jcsample.c \
           IO/3rdParty/jpeglib/jcscale.c \
           IO/3rdParty/jpeglib/jcshuff.c \
           IO/3rdParty/jpeglib/jctrans.c \
           IO/3rdParty/jpeglib/jdapimin.c \
           IO/3rdParty/jpeglib/jdapistd.c \
           IO/3rdParty/jpeglib/jdatadst.c \
           IO/3rdParty/jpeglib/jdatasrc.c \
           IO/3rdParty/jpeglib/jdcoefct.c \
           IO/3rdParty/jpeglib/jdcolor.c \
           IO/3rdParty/jpeglib/jddctmgr.c \
           IO/3rdParty/jpeglib/jddiffct.c \
           IO/3rdParty/jpeglib/jdhuff.c \
           IO/3rdParty/jpeglib/jdinput.c \
           IO/3rdParty/jpeglib/jdlhuff.c \
           IO/3rdParty/jpeglib/jdlossls.c \
           IO/3rdParty/jpeglib/jdlossy.c \
           IO/3rdParty/jpeglib/jdmainct.c \
           IO/3rdParty/jpeglib/jdmarker.c \
           IO/3rdParty/jpeglib/jdmaster.c \
           IO/3rdParty/jpeglib/jdmerge.c \
           IO/3rdParty/jpeglib/jdphuff.c \
           IO/3rdParty/jpeglib/jdpostct.c \
           IO/3rdParty/jpeglib/jdpred.c \
           IO/3rdParty/jpeglib/jdsample.c \
           IO/3rdParty/jpeglib/jdscale.c \
           IO/3rdParty/jpeglib/jdshuff.c \
           IO/3rdParty/jpeglib/jdtrans.c \
           IO/3rdParty/jpeglib/jerror.c \
           IO/3rdParty/jpeglib/jfdctflt.c \
           IO/3rdParty/jpeglib/jfdctfst.c \
           IO/3rdParty/jpeglib/jfdctint.c \
           IO/3rdParty/jpeglib/jidctflt.c \
           IO/3rdParty/jpeglib/jidctfst.c \
           IO/3rdParty/jpeglib/jidctint.c \
           IO/3rdParty/jpeglib/jidctred.c \
           IO/3rdParty/jpeglib/jmemmgr.c \
           IO/3rdParty/jpeglib/jmemnobs.c \
           IO/3rdParty/jpeglib/jquant1.c \
           IO/3rdParty/jpeglib/jquant2.c \
           IO/3rdParty/jpeglib/jutils.c \
           IO/3rdParty/jpeglib/rdcolmap.c \
           IO/3rdParty/jpeglib/rdswitch.c \
           IO/3rdParty/jpeglib/transupp.c \
           IO/3rdParty/tiff/tif_aux.c \
           IO/3rdParty/tiff/tif_close.c \
           IO/3rdParty/tiff/tif_codec.c \
           IO/3rdParty/tiff/tif_color.c \
           IO/3rdParty/tiff/tif_compress.c \
           IO/3rdParty/tiff/tif_dir.c \
           IO/3rdParty/tiff/tif_dirinfo.c \
           IO/3rdParty/tiff/tif_dirread.c \
           IO/3rdParty/tiff/tif_dirwrite.c \
           IO/3rdParty/tiff/tif_dumpmode.c \
           IO/3rdParty/tiff/tif_error.c \
           IO/3rdParty/tiff/tif_extension.c \
           IO/3rdParty/tiff/tif_fax3.c \
           IO/3rdParty/tiff/tif_fax3sm.c \
           IO/3rdParty/tiff/tif_flush.c \
           IO/3rdParty/tiff/tif_getimage.c \
           IO/3rdParty/tiff/tif_luv.c \
           IO/3rdParty/tiff/tif_lzw.c \
           IO/3rdParty/tiff/tif_next.c \
           IO/3rdParty/tiff/tif_open.c \
           IO/3rdParty/tiff/tif_packbits.c \
           IO/3rdParty/tiff/tif_pixarlog.c \
           IO/3rdParty/tiff/tif_predict.c \
           IO/3rdParty/tiff/tif_print.c \
           IO/3rdParty/tiff/tif_read.c \
           IO/3rdParty/tiff/tif_strip.c \
           IO/3rdParty/tiff/tif_swab.c \
           IO/3rdParty/tiff/tif_thunder.c \
           IO/3rdParty/tiff/tif_tile.c \
           IO/3rdParty/tiff/tif_unix.c \
           IO/3rdParty/tiff/tif_version.c \
           IO/3rdParty/tiff/tif_warning.c \
           IO/3rdParty/tiff/tif_write.c \
           IO/3rdParty/tiff/tif_zip.c \
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
           Renderer/GL/GLSBVR2D.cpp \
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
