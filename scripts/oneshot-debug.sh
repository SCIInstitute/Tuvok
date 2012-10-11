#!/bin/sh
# Script to build everything we can in a single invocation, using
# a set of options which is appropriate for creating debug builds.

VIS="-fvisibility=hidden"
INL="-fvisibility-inlines-hidden"
if test `uname -s` != "Darwin"; then
  COVERAGE="-fprofile-arcs -ftest-coverage"
else
  COVERAGE=""
fi
CF="-g -Wall -Wextra -O0 -D_DEBUG ${COVERAGE}"
CXF="${COVERAGE}"
LDFLAGS="${COVERAGE}"
MKSPEC=""
# Darwin's debug STL support is broken.
if test `uname -s` != "Darwin"; then
  CXF="${CXF} -D_GLIBCXX_DEBUG -D_GLIBCXX_CONCEPT_CHECK -Werror -fopenmp"
  LDFLAGS="${LDFLAGS} -fopenmp"
else
  MKSPEC="-spec unsupported/macx-clang"
fi

# Users can set the QT_BIN env var to point at a different Qt implementation.
if test -n "${QT_BIN}" ; then
  echo "Using qmake from '${QT_BIN}' instead of the default from PATH."
  qm="${QT_BIN}/qmake"
else
  qm="qmake"
fi

echo "Configuring..."
${qm} \
  ${MKSPEC} \
  QMAKE_CONFIG+="debug" \
  QMAKE_CFLAGS+="${VIS} ${CF}" \
  QMAKE_CXXFLAGS+="${VIS} ${INL} ${CF} ${CXF}" \
  QMAKE_LFLAGS+="${VIS} ${LDFLAGS}" \
  -recursive Tuvok.pro || exit 1

pushd IO/test &> /dev/null || exit 1
  ${qm} \
    ${MKSPEC} \
    QMAKE_CONFIG+="debug" \
    QMAKE_CFLAGS+="${VIS} ${CF}" \
    QMAKE_CXXFLAGS+="${VIS} ${INL} ${CF} ${CXF}" \
    QMAKE_LFLAGS+="${VIS} ${LDFLAGS}" \
    -recursive test.pro || exit 1
popd &>/dev/null


# Unless the user gave us input as to options to use, default to a small-scale
# parallel build.
if test -z "${MAKE_OPTIONS}" ; then
  MAKE_OPTIONS="-j2 -l 2.0"
fi

for d in . IO/test ; do
  echo "BUILDING IN ${d}"
  pushd ${d} &> /dev/null || exit 1
    make --no-print-directory ${MAKE_OPTIONS} || exit 1
  popd &> /dev/null
done
