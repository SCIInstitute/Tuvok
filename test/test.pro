TEMPLATE          = app
win32:TEMPLATE    = vcapp
CONFIG           += exceptions largefile link_prl static stl warn_on
TARGET            = cxxtester
DEFINES          += _FILE_OFFSET_BITS=64
DEPENDPATH       += . ../
INCLUDEPATH      += ../ ../../ ../3rdParty/boost ../3rdParty/cxxtest
INCLUDEPATH      += ../../Basics
QT               += opengl
QMAKE_LIBDIR     += ../../Build ../expressions
LIBS             += -lTuvok -ltuvokexpr
unix:LIBS        += -lz
win32:LIBS       += shlwapi.lib
unix:QMAKE_CXXFLAGS += -fno-strict-aliasing
unix:QMAKE_CFLAGS += -fno-strict-aliasing
unix:QMAKE_CFLAGS += -D_GLIBCXX_DEBUG
unix:QMAKE_CXXFLAGS += -D_GLIBCXX_DEBUG

# If this is a 10.5 machine, build for both x86 and x86_64.  Not
# the best idea (there's no guarantee the machine will have a
# 64bit compiler), but the best we can do via qmake.
macx {
    exists(/Developer/SDKs/MacOSX10.5.sdk/) {
        CONFIG += x86 x86_64
    }
}

### Should we link Qt statically or as a shared lib?
# Find the location of QtGui's prl file, and include it here so we can look at
# the QMAKE_PRL_CONFIG variable.
TEMP = $$[QT_INSTALL_LIBS] libQtGui.prl
PRL  = $$[QT_INSTALL_LIBS] QtGui.framework/QtGui.prl
include($$join(TEMP, "/"))
include($$join(PRL, "/"))

# If that contains the `shared' configuration, the installed Qt is shared.
# In that case, disable the image plugins.
contains(QMAKE_PRL_CONFIG, shared) {
  message("Shared build, ensuring there will be image plugins linked in.")
  QTPLUGIN -= qgif qjpeg
} else {
  message("Static build, forcing image plugins to get loaded.")
  QTPLUGIN += qgif qjpeg
}

#TEST_HEADERS=multisrc.h dicom.h jpeg.h minmax.h
TEST_HEADERS=quantize.h jpeg.h

alltests.target = alltests.cpp
alltests.commands = python ../3rdParty/cxxtest/cxxtestgen.py --no-static-init --error-printer -o alltests.cpp $$TEST_HEADERS
alltests.depends = $$TEST_HEADERS

#cxxtester.depends = alltests.cpp

QMAKE_EXTRA_TARGETS += alltests

SOURCES += alltests.cpp
