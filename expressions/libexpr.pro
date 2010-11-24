TEMPLATE          = lib
win32:TEMPLATE    = vclib
CONFIG            = create_prl exceptions largefile static staticlib stl
CONFIG           += warn_on
DEFINES          += YY_NO_UNPUT
TARGET            = tuvokexpr
DEPENDPATH       += . ../../Basics ../
INCLUDEPATH      += ../../ ../../Basics ../ ../3rdParty/boost
unix:QMAKE_CXXFLAGS += -fno-strict-aliasing
unix:QMAKE_CFLAGS += -fno-strict-aliasing

include(flex.pri)
include(bison.pri)

FLEXSOURCES = tvk-scan.lpp
BISONSOURCES = tvk-parse.ypp

SOURCES += \
  binary-expression.cpp \
  conditional-expression.cpp \
  constant.cpp          \
  treenode.cpp          \
  ../IO/VariantArray.cpp \
  volume.cpp
