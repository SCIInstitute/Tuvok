#-------------------------------------------------
#
# Project created by QtCreator 2014-05-04T12:32:30
#
#-------------------------------------------------

QT       += core opengl

#QT       -= gui

TARGET = TuvokServer
CONFIG   += console
CONFIG   -= app_bundle



QMAKE_CC            = /usr/local/bin/mpicc
QMAKE_CXX           = /usr/local/bin/mpicxx
QMAKE_LINK          = $$QMAKE_CXX

!macx {
    QMAKE_CXXFLAGS  += -fopenmp
    QMAKE_LFLAGS    += -fopenmp -bullshit-link-flag

    QMAKE_CFLAGS    = $$system(mpicc --showme:compile)
    QMAKE_CXXFLAGS  = $$system(mpicxx --showme:compile) -DMPICH_IGNORE_CXX_SEEK
    QMAKE_LFLAGS    = $$system(mpicxx --showme:link)
}
macx {
    macx:CONFIG     += c++11
    QMAKE_CFLAGS    = -I/usr/local/include -mmacosx-version-min=10.7
    QMAKE_CXXFLAGS  = -I/usr/local/include -DMPICH_IGNORE_CXX_SEEK -mmacosx-version-min=10.7
    QMAKE_LFLAGS    = -L/usr/local/lib -lmpi_cxx -lmpi -lm
}
INCLUDEPATH += IO/sockethelper/ TuvokServer/ IO/3rdParty/boost IO/3rdParty/zlib
INCLUDEPATH += $$PWD/../ImageVis3D/Tuvok/
DEPENDPATH += $$PWD/../ImageVis3D/Tuvok/

TEMPLATE = app


SOURCES += \
    TuvokServer/main.cpp \
    TuvokServer/tvkserver.cpp \
    IO/sockethelper/parameterwrapper.cpp \
    IO/sockethelper/sockhelp.c \
    TuvokServer/callperformer.cpp \
    IO/netds.c

HEADERS += \
    IO/sockethelper/parameterwrapper.h \
    IO/sockethelper/sockhelp.h \
    TuvokServer/tvkserver.h \
    TuvokServer/callperformer.h \
    IO/netds.h \
    IO/sockethelper/order32.h

macx:LIBS += -L/Volumes/HDD/Dropbox/ -lTuvok
macx:LIBS += -L/Volumes/HDD/Dropbox/ -lTuvokexpr
macx:LIBS += -lz
macx:LIBS += -framework CoreFoundation
macx:LIBS += -framework OpenGL

macx: PRE_TARGETDEPS += /Volumes/HDD/Dropbox/libTuvok.a
macx: PRE_TARGETDEPS += /Volumes/HDD/Dropbox/libtuvokexpr.a



