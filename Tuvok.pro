SUBDIRS           = IO/expressions tvk.pro
unix {
  SUBDIRS        += test/render test/shaders
}
TEMPLATE          = subdirs
CONFIG           += ordered
