TEMPLATE          = app
win32:TEMPLATE    = vcapp
CONFIG            = exceptions largefile rtti static stl warn_on
DEFINES          += DEBUG_LEX YY_NO_UNPUT
macx:DEFINES     += QT_MAC_USE_COCOA=1
TARGET            = lexdbg
DEPENDPATH       += . ../../Basics ../
INCLUDEPATH      += ../../ ../../Basics ../ ../3rdParty/boost
win32:LIBS       += shlwapi.lib
unix:QMAKE_CXXFLAGS += -std=c++0x
unix:QMAKE_CXXFLAGS += -fno-strict-aliasing
unix:QMAKE_CFLAGS += -fno-strict-aliasing

# On mac we completely disable warnings in this library. flex and bison are not playing nice with clang.
macx:QMAKE_CXXFLAGS += -stdlib=libc++ -w
macx:QMAKE_CFLAGS +=
macx:LIBS        += -stdlib=libc++ -framework CoreFoundation
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
