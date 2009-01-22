TEMPLATE          = lib
CONFIG           += staticlib static create_prl warn_on stl exceptions x86 ppc
TARGET            = Tuvok
QTPLUGIN         += qjpeg qtiff qgif
VERSION           = 0.0.1
TARGET            = Build/Tuvok
RCC_DIR           = Build/rcc
OBJECTS_DIR       = Build/objects
DEPENDPATH       += .
INCLUDEPATH      += .
QT               += opengl
LIBS             += -lz

# Input
HEADERS += StdTuvokDefines.h \
           Basics/Grids.h \
           Basics/SysTools.h \
           Basics/Vectors.h \
           Basics/MathTools.h \
           Basics/ArcBall.h \
           Basics/GeometryGenerator.h \
           Basics/Checksums/MD5.h \
           Basics/Checksums/crc32.h \
           IO/gzio.h \
           IO/AbstrConverter.h \
           IO/NRRDConverter.h \
           IO/RAWConverter.h \
           IO/QVISConverter.h \
           IO/VFFConverter.h \
           IO/DirectoryParser.h \
           IO/KeyValueFileParser.h \
           IO/Transferfunction1D.h \
           IO/Transferfunction2D.h \
           IO/VolumeDataset.h \
           IO/IOManager.h \
           IO/DICOM/DICOMParser.h \
           IO/Images/ImageParser.h \
           IO/UVF/DataBlock.h \
           IO/UVF/GlobalHeader.h \
           IO/UVF/KeyValuePairDataBlock.h \
           IO/UVF/LargeRAWFile.h \
           IO/UVF/RasterDataBlock.h \
           IO/UVF/UVF.h \
           IO/UVF/UVFBasic.h \
           IO/UVF/UVFTables.h \
           IO/UVF/Histogram1DDataBlock.h \
           IO/UVF/Histogram2DDataBlock.h \
           IO/UVF/MaxMinDataBlock.h \
           Controller/MasterController.h \
           DebugOut/AbstrDebugOut.h \
           DebugOut/TextfileOut.h \
           DebugOut/ConsoleOut.h \
           DebugOut/MultiplexOut.h \
           3rdParty/GLEW/glew.h \
           3rdParty/GLEW/glxew.h \
           3rdParty/bzip2/bzlib.h \
           3rdParty/bzip2/bzlib_private.h \
           Scripting/Scripting.h \
           Scripting/Scriptable.h \
           Renderer/CullingLOD.h \
           Renderer/FrameCapture.h \
           Renderer/GPUObject.h \
           Renderer/GL/GLFrameCapture.h \
           Renderer/GL/GLSLProgram.h \
           Renderer/GL/GLInclude.h \
           Renderer/GL/GLObject.h \
           Renderer/GL/GLTexture.h \
           Renderer/GL/GLTexture1D.h \
           Renderer/GL/GLTexture2D.h \
           Renderer/GL/GLTexture3D.h \
           Renderer/GL/GLRenderer.h \
           Renderer/GL/GLRaycaster.h \
           Renderer/GL/GLSBVR.h \
           Renderer/AbstrRenderer.h \
           Renderer/GPUMemMan/GPUMemMan.h \
           Renderer/GPUMemMan/GPUMemManDataStructs.h \
           Renderer/SBVRGeogen.h

SOURCES += 3rdParty/GLEW/glew.c \
           3rdParty/bzip2/blocksort.c \
           3rdParty/bzip2/huffman.c \
           3rdParty/bzip2/crctable.c \
           3rdParty/bzip2/randtable.c \
           3rdParty/bzip2/compress.c \
           3rdParty/bzip2/decompress.c \
           3rdParty/bzip2/bzlib.c \
           Basics/SystemInfo.cpp \
           Basics/SysTools.cpp \
           Basics/MathTools.cpp \
           Basics/ArcBall.cpp \
           Basics/GeometryGenerator.cpp \
           Basics/Checksums/MD5.cpp \
           IO/gzio.c \
           IO/AbstrConverter.cpp \
           IO/NRRDConverter.cpp \
           IO/RAWConverter.cpp \
           IO/QVISConverter.cpp \
           IO/VFFConverter.cpp \
           IO/KeyValueFileParser.cpp \           
           IO/Transferfunction1D.cpp \
           IO/Transferfunction2D.cpp \
           IO/VolumeDataset.cpp \
           IO/IOManager.cpp \
           IO/DirectoryParser.cpp \
           IO/DICOM/DICOMParser.cpp \
           IO/Images/ImageParser.cpp \           
           IO/UVF/DataBlock.cpp \
           IO/UVF/GlobalHeader.cpp \
           IO/UVF/KeyValuePairDataBlock.cpp \
           IO/UVF/LargeRAWFile.cpp \
           IO/UVF/RasterDataBlock.cpp \
           IO/UVF/UVF.cpp \
           IO/UVF/UVFTables.cpp \
           IO/UVF/Histogram1DDataBlock.cpp \
           IO/UVF/Histogram2DDataBlock.cpp \                     
           IO/UVF/MaxMinDataBlock.cpp \
           Controller/MasterController.cpp \
           DebugOut/AbstrDebugOut.cpp \
           DebugOut/TextfileOut.cpp \
           DebugOut/ConsoleOut.cpp \
           DebugOut/MultiplexOut.cpp \ 
           Scripting/Scripting.cpp \
           Renderer/CullingLOD.cpp \
           Renderer/GL/GLFrameCapture.cpp \
           Renderer/GL/GLSLProgram.cpp \
           Renderer/GL/GLTexture.cpp \
           Renderer/GL/GLTexture1D.cpp \
           Renderer/GL/GLTexture2D.cpp \
           Renderer/GL/GLTexture3D.cpp \
           Renderer/GL/GLRenderer.cpp \           
           Renderer/GL/GLFBOTex.cpp \
           Renderer/GL/GLRaycaster.cpp \
           Renderer/GL/GLSBVR.cpp \
           Renderer/AbstrRenderer.cpp \
           Renderer/GPUMemMan/GPUMemMan.cpp \
           Renderer/GPUMemMan/GPUMemManDataStructs.cpp \
           Renderer/SBVRGeogen.cpp 
