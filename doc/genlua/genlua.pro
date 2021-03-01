TEMPLATE       = app
win32:TEMPLATE = vcapp
CONFIG += qt largefile rtti static stl warn_on exceptions
QT += core opengl
DESTDIR = Build
TARGET = bluebook
DEPENDPATH = .
INCLUDEPATH  = ../../
INCLUDEPATH += ../../Basics/3rdParty
INCLUDEPATH += ../../IO/3rdParty/boost/
QMAKE_LIBDIR += ../../Build
QMAKE_LIBDIR += ../../IO/expressions
LIBS             = -lTuvok -ltuvokexpr
unix:LIBS       += -lz
win32:LIBS      += shlwapi.lib
macx-clang:LIBS       += -stdlib=libc++
macx-clang:LIBS       +=
macx-clang:LIBS       += -framework CoreFoundation
unix:!macx-clang:LIBS += -lGLU -lGL
# don't complain about not understanding OpenMP pragmas.
QMAKE_CXXFLAGS      += -Wno-unknown-pragmas
macx-clang:QMAKE_CXXFLAGS += -stdlib=libc++

unix:QMAKE_CXXFLAGS += -std=c++0x
unix:QMAKE_CXXFLAGS += -fno-strict-aliasing
unix:QMAKE_CFLAGS   += -fno-strict-aliasing
!macx-clang:unix:QMAKE_LFLAGS += -fopenmp

QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.13

SOURCES = \
  main.cpp
