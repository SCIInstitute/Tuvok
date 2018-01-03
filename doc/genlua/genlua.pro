TEMPLATE       = app
win32:TEMPLATE = vcapp
CONFIG += c++11 exceptions largefile qt rtti static staticlib stl warn_on
QT += opengl
DESTDIR = Build
TARGET = bluebook
DEPENDPATH = .
INCLUDEPATH  = ../../
INCLUDEPATH += ../../Basics/3rdParty
INCLUDEPATH += ../../IO/3rdParty/boost/
QMAKE_LIBDIR += ../../Build
QMAKE_LIBDIR += ../../IO/expressions
QMAKE_LIBS             = -lTuvok -ltuvokexpr
linux*:QMAKE_LIBS += -lz
win32:QMAKE_LIBS      += shlwapi.lib
macx:QMAKE_LIBS       += -stdlib=libc++
macx:QMAKE_LIBS       += -mmacosx-version-min=10.7
macx:QMAKE_LIBS       += -framework CoreFoundation
linux*:QMAKE_LIBS += -lGL -lGLU
# don't complain about not understanding OpenMP pragmas.
QMAKE_CXXFLAGS      += -Wno-unknown-pragmas
include(../../flags.pro)

SOURCES = \
  main.cpp
