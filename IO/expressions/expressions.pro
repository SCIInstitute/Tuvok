TEMPLATE          = lib
win32:TEMPLATE    = vclib
CONFIG            = exceptions largefile rtti static staticlib stl
CONFIG           += warn_on
CONFIG           += c++11
DEFINES          += YY_NO_UNPUT _FILE_OFFSET_BITS=64
TARGET            = tuvokexpr
DEPENDPATH       += . ../../Basics ../
INCLUDEPATH      += ../../ ../../Basics ../ ../3rdParty/boost
include(../../flags.pro)
# Generated flex/bison code isn't very diligent in regards to sign.
*g++*:QMAKE_CXXFLAGS += -Wno-error=sign-compare
QMAKE_MACOSX_DEPLOYMENT_TARGET=10.9

include(flex.pri)
include(bison.pri)

FLEXSOURCES = tvk-scan.lpp
BISONSOURCES = tvk-parse.ypp

SOURCES += \
  binary-expression.cpp \
  conditional-expression.cpp \
  constant.cpp          \
  treenode.cpp          \
  volume.cpp
