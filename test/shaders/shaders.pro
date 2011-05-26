TEMPLATE          = app
CONFIG           += staticlib static create_prl warn_on stl exceptions
TARGET            = shtest
DEPENDPATH       += . ../../
INCLUDEPATH      += ../
INCLUDEPATH      += ../../ ../../IO/3rdParty/boost
INCLUDEPATH      += ../../Basics/3rdParty
INCLUDEPATH      += ../../3rdParty/GLEW
# for glX
macx:INCLUDEPATH += /usr/X11R6/include
macx:QMAKE_LIBDIR+= /usr/X11R6/lib
QMAKE_LIBDIR     += ../../Build ../../IO/expressions
QT               += opengl
LIBS             += -lTuvok
macx:LIBS        += -lX11 -lGL
unix:QMAKE_CXXFLAGS += -fno-strict-aliasing -g
unix:QMAKE_CFLAGS += -fno-strict-aliasing -g
unix:!macx:QMAKE_CFLAGS += -D_GLIBCXX_DEBUG
unix:!macx:QMAKE_CXXFLAGS += -D_GLIBCXX_DEBUG

### Should we link Qt statically or as a shared lib?
# Find the location of QtGui's prl file, and include it here so we can look at
# the QMAKE_PRL_CONFIG variable.
TEMP = $$[QT_INSTALL_LIBS] libQtCore.prl
PRL  = $$[QT_INSTALL_LIBS] QtCore.framework/QtCore.prl
exists($$TEMP) {
  include($$join(TEMP, "/"))
}
exists($$PRL) {
  include($$join(PRL, "/"))
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

SOURCES += \
  shadertest.cpp \
  ../context.cpp
