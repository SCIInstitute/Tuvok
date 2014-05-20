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

TEMPLATE = app

QMAKE_CC         = mpicc
QMAKE_CXX        = mpicxx
QMAKE_LINK       = $$QMAKE_CXX

#We need to link against the original tuvok lib
TUVOKLIBPATH    = /Volumes/HDD/Dropbox

QMAKE_CFLAGS    += $$system(mpicc --showme:compile)
QMAKE_CXXFLAGS  += $$system(mpicxx --showme:compile) -DMPICH_IGNORE_CXX_SEEK
QMAKE_LFLAGS    += $$system(mpicxx --showme:link)

!macx {
    QMAKE_CXXFLAGS  += -fopenmp
    QMAKE_LFLAGS    += -fopenmp -bullshit-link-flag

    unix:QMAKE_CFLAGS   += -std=c99 -Werror
    unix:QMAKE_CXXFLAGS += -std=c++0x
}
macx {
    macx:CONFIG     += c++11
    QMAKE_CFLAGS    += -mmacosx-version-min=10.7
    QMAKE_CXXFLAGS  += -mmacosx-version-min=10.7

    macx:LIBS       += -framework CoreFoundation
    macx:LIBS       += -framework OpenGL
}

INCLUDEPATH += IO/ IO/sockethelper/ TuvokServer/ IO/3rdParty/boost IO/3rdParty/zlib

LIBS += -lz
LIBS += -L$$TUVOKLIBPATH -lTuvok
LIBS += -L$$TUVOKLIBPATH -lTuvokexpr

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



