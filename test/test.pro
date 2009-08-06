TEMPLATE          = app
CONFIG           += staticlib static create_prl warn_on stl exceptions
TARGET            = cxxtester
DEPENDPATH       += . ../
INCLUDEPATH      += ../ ../../ ../3rdParty/boost ../3rdParty/cxxtest
QT               += opengl
LIBS             += -L../../Build -lTuvok -lz
unix:QMAKE_CXXFLAGS += -fno-strict-aliasing
unix:QMAKE_CFLAGS += -fno-strict-aliasing

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

TEST_HEADERS=minmax.h dicom.h jpeg.h

alltests.target = alltests.cpp
alltests.commands = ../3rdParty/cxxtest/cxxtestgen.py --no-static-init --error-printer -o alltests.cpp $$TEST_HEADERS
alltests.depends = $$TEST_HEADERS

#cxxtester.depends = alltests.cpp

QMAKE_EXTRA_TARGETS += alltests

SOURCES += alltests.cpp
