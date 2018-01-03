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
linux*:QMAKE_LIBS += -lGL -lGLU
include(../../flags.pro)

SOURCES = \
  main.cpp
