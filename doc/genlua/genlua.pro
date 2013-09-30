TEMPLATE       = app
win32:TEMPLATE = vcapp
CONFIG = exceptions largefile qt rtti static stl warn_on
QT += core opengl
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
macx:LIBS       += -stdlib=libc++
macx:LIBS       += -mmacosx-version-min=10.7
macx:LIBS       += -framework CoreFoundation
unix:!macx:LIBS += -lGLU -lGL
# don't complain about not understanding OpenMP pragmas.
QMAKE_CXXFLAGS      += -Wno-unknown-pragmas
macx:QMAKE_CXXFLAGS += -stdlib=libc++
macx:QMAKE_CXXFLAGS += -mmacosx-version-min=10.7
unix:QMAKE_CXXFLAGS += -std=c++0x
unix:QMAKE_CXXFLAGS += -fno-strict-aliasing
unix:QMAKE_CFLAGS   += -fno-strict-aliasing
!macx:unix:QMAKE_LFLAGS += -fopenmp

SOURCES = \
  main.cpp
