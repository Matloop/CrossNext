#!/bin/sh
# Builds and runs the blend tests. No device and no PlatformIO: BlendLink is
# freestanding C++17; BlendRecord runs against the real Words engine.
#
#   host-tests/blend/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-blend-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/blend
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 $SRC/BlendLink.cpp \
  test_blend.cpp -o "$BUILD_DIR/test_blend"
"$BUILD_DIR/test_blend"
STUDY=../../src/apps_local/study
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I../../src \
  $SRC/BlendLink.cpp $SRC/BlendRecord.cpp ../../src/apps_local/words/Vocabulary.cpp \
  $STUDY/StudyScheduler.cpp $STUDY/StudyFsrs.cpp $STUDY/StudyDeck.cpp \
  test_record.cpp -o "$BUILD_DIR/test_record"
"$BUILD_DIR/test_record"
