SUBDIRS           = IO/expressions tvk.pro doc/genlua
unix {
  SUBDIRS        += test/render test/shaders test/context
}
TEMPLATE          = subdirs
CONFIG           += ordered
