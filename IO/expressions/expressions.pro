TEMPLATE          = lib
win32:TEMPLATE    = vclib
CONFIG            = exceptions largefile rtti static staticlib stl
CONFIG           += warn_on
DEFINES          += YY_NO_UNPUT _FILE_OFFSET_BITS=64
TARGET            = tuvokexpr
DEPENDPATH       += . ../../Basics ../
INCLUDEPATH      += ../../ ../../Basics ../ ../3rdParty/boost
unix:QMAKE_CXXFLAGS += -std=c++0x -Wno-error=sign-compare
unix:QMAKE_CXXFLAGS += -fno-strict-aliasing
unix:QMAKE_CFLAGS += -fno-strict-aliasing

# On mac we completely disable warnings in this library. flex and bison are not playing nice with clang.
macx:QMAKE_CXXFLAGS += -stdlib=libc++ -mmacosx-version-min=11.1 -w
macx:QMAKE_CFLAGS += -mmacosx-version-min=11.1
macx:LIBS        += -stdlib=libc++ -framework CoreFoundation -mmacosx-version-min=11.1

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
