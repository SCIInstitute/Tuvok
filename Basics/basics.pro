TEMPLATE          = lib
win32:TEMPLATE    = vclib
CONFIG            = create_prl exceptions largefile static staticlib stl
CONFIG           += warn_on
TARGET            = tuvokbasics
win32 { DESTDIR   = Build }
OBJECTS_DIR       = Build/objects
d = 3rdParty/boost 3rdParty/tclap . ../
DEPENDPATH        = $$d
INCLUDEPATH       = $$d
win32:LIBS       += shlwapi.lib
unix:QMAKE_CXXFLAGS += -fno-strict-aliasing
unix:QMAKE_CFLAGS += -fno-strict-aliasing

macx:QMAKE_CXXFLAGS += -stdlib=libc++ -mmacosx-version-min=11.1
macx:QMAKE_CFLAGS += -mmacosx-version-min=11.1
macx:LIBS        += -stdlib=libc++ -framework CoreFoundation -mmacosx-version-min=11.1

# Input
SOURCES += \
  ./Appendix.cpp \
  ./ArcBall.cpp \
  ./Checksums/MD5.cpp \
  ./DynamicDX.cpp \
  ./GeometryGenerator.cpp \
  ./KDTree.cpp \
  ./LargeRAWFile.cpp \
  ./MathTools.cpp \
  ./MC.cpp \
  ./MemMappedFile.cpp \
  ./Mesh.cpp \
  ./Plane.cpp \
  ./SystemInfo.cpp \
  ./Systeminfo/VidMemViaDDraw.cpp \
  ./Systeminfo/VidMemViaDXGI.cpp \
  ./SysTools.cpp \
  ./Timer.cpp \
  ./ProgressTimer.cpp \
  ./Clipper.cpp

HEADERS += \
  ./Appendix.h \
  ./ArcBall.h \
  ./Checksums/crc32.h \
  ./Checksums/MD5.h \
  ./Console.h \
  ./DynamicDX.h \
  ./EndianConvert.h \
  ./GeometryGenerator.h \
  ./Grids.h \
  ./KDTree.h \
  ./LargeRAWFile.h \
  ./MathTools.h \
  ./MC.h \
  ./MemMappedFile.h \
  ./Mesh.h \
  ./Plane.h \
  ./Ray.h \
  ./StdDefines.h \
  ./SystemInfo.h \
  ./SysTools.h \
  ./Timer.h \
  ./ProgressTimer.h \
  ./Clipper.h \
  ./tr1.h \
  ./TuvokException.h \
  ./Vectors.h
