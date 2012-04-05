SUBDIRS           = IO/expressions tvk.pro
unix {
  SUBDIRS        += test/render test/shaders test/context
}
TEMPLATE          = subdirs
CONFIG           += ordered
