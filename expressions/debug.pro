TEMPLATE          = app
win32:TEMPLATE    = vcapp
CONFIG            = exceptions largefile static stl warn_on
DEFINES          += DEBUG_LEX YY_NO_UNPUT
macx:DEFINES     += QT_MAC_USE_COCOA=1
TARGET            = lexdbg
DEPENDPATH       += . ../../Basics ../
INCLUDEPATH      += ../../ ../../Basics ../ ../3rdParty/boost
macx:LIBS        += -framework CoreFoundation
win32:LIBS       += shlwapi.lib
unix:QMAKE_CXXFLAGS += -fno-strict-aliasing
unix:QMAKE_CFLAGS += -fno-strict-aliasing
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
