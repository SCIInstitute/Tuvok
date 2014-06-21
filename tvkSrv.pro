#-------------------------------------------------
#
# Project created by QtCreator 2014-05-04T12:32:30
#
#-------------------------------------------------

QT              += core opengl

TEMPLATE        = app

CONFIG          += console
CONFIG          += c++11
CONFIG          -= app_bundle
TARGET           = TuvokServer/TuvokServer
OBJECTS_DIR      = TuvokServer/Build

#add shaders to build dir
shaders.path    = $$OUT_PWD/TuvokServer/Shaders
shaders.files  += Shaders/*
INSTALLS       += shaders

# Operating system definitions now in the makefile instead of
# a header file.
unix:!macx  { DEFINES += "DETECTED_OS_LINUX" }
macx        { DEFINES += "DETECTED_OS_APPLE" }
win32       { DEFINES += "DETECTED_OS_WINDOWS" }

#CONFIG          += mpi
mpi {
    QMAKE_CC         = mpicc
    QMAKE_CXX        = mpicxx
    QMAKE_LINK       = $$QMAKE_CXX
    DEFINES         += "MPI_ACTIVE=1"

    QMAKE_CFLAGS    += $$system(mpicc --showme:compile)
    QMAKE_CXXFLAGS  += $$system(mpicxx --showme:compile) -DMPICH_IGNORE_CXX_SEEK
    QMAKE_LFLAGS    += $$system(mpicxx --showme:link)
}

!macx {
    QMAKE_CXXFLAGS  += -fopenmp
    QMAKE_LFLAGS    += -fopenmp

    QMAKE_CFLAGS    += -std=c99 -Werror
    QMAKE_CXXFLAGS  += -std=c++0x
}
USE_MPI              = 1
macx {
    QMAKE_CFLAGS    += -mmacosx-version-min=10.7
    QMAKE_CXXFLAGS  += -mmacosx-version-min=10.7

    LIBS            += -framework CoreFoundation
    LIBS            += -framework OpenGL
    LIBS            += -framework Cocoa
}

incpath           = . Basics IO/3rdParty/boost 3rdParty/GLEW Renderer Renderer/GL
incpath          += IO IO/sockethelper TuvokServer/BatchRenderer
DEPENDPATH       += $$incpath
INCLUDEPATH      += $$incpath

#We unfortunately need to link against the original tuvok lib
QMAKE_LIBDIR += Build IO/expressions
LIBS         += -lTuvok -lz
!macx:LIBS   += -lGLU

SOURCES += \
    TuvokServer/main.cpp \
    TuvokServer/tvkserver.cpp \
    TuvokServer/callperformer.cpp \
    IO/sockethelper/parameterwrapper.cpp \
    IO/sockethelper/sockhelp.c \
    IO/netds.c \
    DebugOut/debug.c

HEADERS += \
    TuvokServer/tvkserver.h \
    TuvokServer/callperformer.h \
    IO/sockethelper/parameterwrapper.h \
    IO/sockethelper/sockhelp.h \
    IO/sockethelper/order32.h \
    IO/netds.h \
    DebugOut/debug.h

#For contexts
SOURCES += \
    TuvokServer/BatchRenderer/BatchContext.cpp

unix:!macx  { SOURCES += TuvokServer/BatchRenderer/GLXContext.cpp }
macx        { SOURCES += TuvokServer/BatchRenderer/CGLContext.cpp }
macx        { OBJECTIVE_SOURCES += TuvokServer/BatchRenderer/NSContext.mm }
win32       { SOURCES += TuvokServer/BatchRenderer/WGLContext.cpp }

HEADERS += \
  TuvokServer/BatchRenderer/BatchContext.h \
  TuvokServer/BatchRenderer/CGLContext.h \
  TuvokServer/BatchRenderer/NSContext.h \
  TuvokServer/BatchRenderer/GLXContext.h \
  TuvokServer/BatchRenderer/WGLContext.h
