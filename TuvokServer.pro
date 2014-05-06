#-------------------------------------------------
#
# Project created by QtCreator 2014-05-04T12:32:30
#
#-------------------------------------------------

QT       += core

QT       -= gui

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
    QMAKE_CFLAGS    = -I/usr/local/include
    QMAKE_CXXFLAGS  = -I/usr/local/include -DMPICH_IGNORE_CXX_SEEK
    QMAKE_LFLAGS    = -L/usr/local/lib -lmpi_cxx -lmpi -lm
}
INCLUDEPATH += IO/sockethelper/ TuvokServer/

TEMPLATE = app

SOURCES += \
    TuvokServer/main.cpp \
    TuvokServer/tvkserver.cpp \
    IO/sockethelper/parameterwrapper.cpp \
    IO/sockethelper/sockhelp.c

HEADERS += \
    IO/sockethelper/parameterwrapper.h \
    IO/sockethelper/sockhelp.h \
    TuvokServer/tvkserver.h
