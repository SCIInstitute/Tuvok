TEMPLATE          = app
win32:TEMPLATE    = vcapp
CONFIG           += c++11 exceptions largefile link_prl qt rtti static stl
CONFIG           += warn_on
TARGET            = cxxtester
DEFINES          += _FILE_OFFSET_BITS=64
DEPENDPATH       += . ../
INCLUDEPATH      += ../ ../../ ../3rdParty/boost ../3rdParty/cxxtest
INCLUDEPATH      += ../../Basics
QT               += core gui opengl
QMAKE_LIBDIR     += ../../Build ../expressions
LIBS             += -lTuvok -ltuvokexpr
linux*|mac*:LIBS += -lz
linux*:LIBS      += -lrt
win32:LIBS       += shlwapi.lib

include(../../flags.pro)

### Should we link Qt statically or as a shared lib?
# Find the location of QtGui's prl file, and include it here so we can look at
# the QMAKE_PRL_CONFIG variable.
TEMP = $$[QT_INSTALL_LIBS] libQtGui.prl
PRL  = $$[QT_INSTALL_LIBS] QtGui.framework/QtGui.prl
TEMP = $$join(TEMP, "/")
PRL  = $$join(PRL, "/")
exists($$TEMP) {
  include($$TEMP)
}
exists($$PRL) {
  include($$PRL)
}

# If that contains the `shared' configuration, the installed Qt is shared.
# In that case, disable the image plugins.
contains(QMAKE_PRL_CONFIG, shared) {
  message("Shared build, ensuring there will be image plugins linked in.")
  QTPLUGIN -= qgif qjpeg
} else {
  message("Static build, forcing image plugins to get loaded.")
  QTPLUGIN += qgif qjpeg
}

#TEST_HEADERS=quantize.h largefile.h rebricking.h cbi.h bcache.h
TEST_HEADERS=quantize.h largefile.h rebricking.h bcache.h

TG_PARAMS=--have-eh --abort-on-fail --no-static-init --error-printer
alltests.target = alltests.cpp
alltests.commands = python ../3rdParty/cxxtest/cxxtestgen.py \
  $$TG_PARAMS -o alltests.cpp $$TEST_HEADERS
alltests.depends = $$TEST_HEADERS

#cxxtester.depends = alltests.cpp

QMAKE_EXTRA_TARGETS += alltests

SOURCES += alltests.cpp
