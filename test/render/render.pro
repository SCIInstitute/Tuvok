TEMPLATE          = app
CONFIG           += exceptions qt rtti staticlib static stl warn_on
TARGET            = tuvok
p                 = . ../ ../../
p                += ../../Basics/3rdParty
p                += ../../IO/3rdParty
p                += ../../IO/3rdParty/boost
p                += ../../3rdParty/GLEW
DEPENDPATH        = $$p
INCLUDEPATH       = $$p
macx:INCLUDEPATH += /usr/X11R6/include
macx:QMAKE_LIBDIR+= /usr/X11R6/lib
QMAKE_LIBDIR     += ../../Build ../../IO/expressions
QT               += opengl
LIBS             += -lTuvok -ltuvokexpr -lz
unix:LIBS        += -lGL -lGLU -lX11
macx:LIBS        += -lX11 -framework CoreFoundation
unix:QMAKE_CXXFLAGS += -fno-strict-aliasing -g
unix:QMAKE_CFLAGS += -fno-strict-aliasing -g

### Should we link Qt statically or as a shared lib?
# Find the location of QtCore's prl file, and include it here so we can look at
# the QMAKE_PRL_CONFIG variable.
TEMP = $$[QT_INSTALL_LIBS] libQtCore.prl
PRL  = $$[QT_INSTALL_LIBS] QtCore.framework/QtCore.prl
TEMP = $$join(TEMP, "/")
PRL  = $$join(PRL, "/")
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
  ../context.cpp \
  render.cpp

unix:!macx { SOURCES += ../glx-context.cpp }
macx { SOURCES += ../cgl-context.cpp }
win32 { SOURCES += ../wgl-context.cpp }

HEADERS += \
  ../context.h \
  ../cgl-context.h \
  ../glx-context.h \
  ../wgl-context.h
