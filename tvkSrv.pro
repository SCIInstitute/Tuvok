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
OBJECTS_DIR      = Build

QMAKE_CC         = mpicc
QMAKE_CXX        = mpicxx
QMAKE_LINK       = $$QMAKE_CXX

QMAKE_CFLAGS    += $$system(mpicc --showme:compile)
QMAKE_CXXFLAGS  += $$system(mpicxx --showme:compile) -DMPICH_IGNORE_CXX_SEEK
QMAKE_LFLAGS    += $$system(mpicxx --showme:link)

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
}

incpath           = . OriginalTvk OriginalTvk/Basics OriginalTvk/IO/3rdParty/boost/
incpath          += OriginalTvk/IO
incpath          += IO/sockethelper
DEPENDPATH       += $$incpath
INCLUDEPATH      += $$incpath

#We unfortunately need to link against the original tuvok lib
QMAKE_LIBDIR += OriginalTvk/Build OriginalTvk/IO/expressions
LIBS         += -lTuvok -ltuvokexpr -lz
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
