#!/bin/sh

VIS="-fvisibility=hidden"
CF="-ggdb3 -Wall -Wextra -O0 -D_DEBUG -D_GLIBCXX_DEBUG -fstack-protector"
CXF="-D_GLIBCXX_CONCEPT_CHECK -fvisibility-inlines-hidden"

mkdir shadowBuild
cd shadowBuild
qm="qmake"
if test -x /usr/bin/qmake-qt4 ; then
  qm="/usr/bin/qmake-qt4"
fi
${qm} \
  CONFIG+="debug" \
  QMAKE_CXX="mpic++" \
  QMAKE_CFLAGS+="${CF}" \
  QMAKE_CXXFLAGS+="${VIS} ${CF} ${CXF}" \
  -recursive \
  ../Tuvok.pro || exit 1
nice make -j2 || exit 1
nice make install
