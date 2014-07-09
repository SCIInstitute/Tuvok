QT              += core opengl

TEMPLATE        = app

CONFIG          += console
CONFIG          += c++11
CONFIG          -= app_bundle
TARGET           = batchrender/bren
OBJECTS_DIR      = batchrender/build/obj/
unix:DEFINES    += LZHAM_ANSI_CPLUSPLUS=1

#add shaders to build dir
shaders.path    = $$OUT_PWD/batchrender/Shaders
shaders.files  += Shaders/*
luas.path       = $$OUT_PWD/batchrender
luas.files     += batchrender/*.lua
INSTALLS       += shaders luas

# Operating system definitions now in the makefile instead of
# a header file.
unix:!macx  { DEFINES += "DETECTED_OS_LINUX" }
macx        { DEFINES += "DETECTED_OS_APPLE" }
win32       { DEFINES += "DETECTED_OS_WINDOWS" }

!macx {
    QMAKE_CXXFLAGS  += -fopenmp
    QMAKE_LFLAGS    += -fopenmp

    QMAKE_CFLAGS    += -std=c99 -Werror
    QMAKE_CXXFLAGS  += -std=c++0x
}
macx {
    QMAKE_CFLAGS    += -mmacosx-version-min=10.7
    QMAKE_CXXFLAGS  += -mmacosx-version-min=10.7

    LIBS            += -framework CoreFoundation
    LIBS            += -framework OpenGL
    LIBS            += -framework Cocoa
}

incpath           = . Basics Basics/3rdParty IO/3rdParty/boost 3rdParty/GLEW
incpath          += Renderer Renderer/GL
incpath          += IO IO/sockethelper TuvokServer/BatchRenderer TuvokServer/QuickTests
DEPENDPATH       += $$incpath
INCLUDEPATH      += $$incpath

# We need to link against the tuvok lib
QMAKE_LIBDIR += Build
LIBS         += -lTuvok -lz
!macx:LIBS   += -lGLU -lX11

# use the contexts from the server.
SOURCES += \
  batchrender/main.cpp \
  batchrender/TuvokLuaScriptExec.cpp \
  TuvokServer/BatchRenderer/BatchContext.cpp \
  IO/netds.cpp

unix:!macx  { SOURCES += TuvokServer/BatchRenderer/GLXContext.cpp }
macx        { SOURCES += TuvokServer/BatchRenderer/CGLContext.cpp }
macx        { OBJECTIVE_SOURCES += TuvokServer/BatchRenderer/NSContext.mm }
win32       { SOURCES += TuvokServer/BatchRenderer/WGLContext.cpp }

HEADERS += \
  batchrender/TuvokLuaScriptExec.h \
  TuvokServer/BatchRenderer/BatchContext.h \
  TuvokServer/BatchRenderer/CGLContext.h \
  TuvokServer/BatchRenderer/NSContext.h \
  TuvokServer/BatchRenderer/GLXContext.h \
  TuvokServer/BatchRenderer/WGLContext.h \
  TuvokServer/QuickTests/Tests.h \
  IO/netds.h
