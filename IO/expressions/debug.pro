TEMPLATE          = app
win32:TEMPLATE    = vcapp
CONFIG            = c++11 exceptions largefile rtti static stl warn_on
DEFINES          += DEBUG_LEX YY_NO_UNPUT
macx:DEFINES     += QT_MAC_USE_COCOA=1
TARGET            = lexdbg
DEPENDPATH       += . ../../Basics ../
INCLUDEPATH      += ../../ ../../Basics ../ ../3rdParty/boost
win32:LIBS       += shlwapi.lib
include(../../flags.pro)

QTPLUGIN -= qgif qjpeg qtiff

include(flex.pri)
include(bison.pri)

FLEXSOURCES = tvk-scan.lpp
BISONSOURCES = tvk-parse.ypp

SOURCES += \
  binary-expression.cpp \
  conditional-expression.cpp \
  constant.cpp          \
  test.cpp              \
  treenode.cpp          \
  ../IO/VariantArray.cpp \
  volume.cpp
