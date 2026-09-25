#!/usr/bin/env bash
#
# Build and test compile the USBSID-Pico driver sources.
#
# Usage: test/ci/build.sh <src|vice>
#
#   src   driver-repo/src: C++ library, C interfaces, C++ and C API tests
#   vice  driver-repo/libusbsiddrv-vice: same as src plus a test of the calls
#         VICE makes
#
# Besides CXXSTDS every target gets one build without C++ exceptions, the way
# VICE configure compiles C++ (-fno-exceptions), as C++11 (VICE's minimum
# standard).
#
# Environment:
#   CC, CXX   compilers (default: cc, c++)
#   CXXSTDS   C++ standards to build, space separated (default: c++17)
#   CSTDS     C standards to build the C tests with (default: c99 c11)
#   OUT_DIR   output directory (default: ci-out/<target>)
#
# Per C++ standard: every .cpp compiles to an object, the objects form a
# static and a shared library, the test programs link against the static
# library and run. The tests need no hardware.

set -eu

TARGET="${1:-}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TESTS="$ROOT/test/ci"

CC="${CC:-cc}"
CXX="${CXX:-c++}"
CXXSTDS="${CXXSTDS:-c++17}"
CSTDS="${CSTDS:-c99 c11}"
OUT_DIR="${OUT_DIR:-$ROOT/ci-out/$TARGET}"

case "$TARGET" in
  src)
    SRC_DIR="$ROOT/src"
    WARN="-Wall -Wextra -Werror"
    C_TESTS="c_api_test.c"
    CXX_TESTS="cpp_api_test.cpp"
    NOEXC_STD="c++11"
    ;;
  vice)
    SRC_DIR="$ROOT/libusbsiddrv-vice"
    WARN="-Wall -Wextra -Werror"
    C_TESTS="c_api_test.c vice_c_api_test.c"
    CXX_TESTS="cpp_api_test.cpp"
    # VICE configure falls back to -std=c++11 on older compilers
    NOEXC_STD="c++11"
    ;;
  *)
    echo "usage: $0 <src|vice>" >&2
    exit 2
    ;;
esac

case "$(uname -s)" in
  Linux*)                SHARED_FLAGS="-shared -Wl,--no-undefined"; SHARED_EXT="so";    EXE="" ;;
  Darwin*)               SHARED_FLAGS="-dynamiclib";                SHARED_EXT="dylib"; EXE="" ;;
  MINGW*|MSYS*|CYGWIN*)  SHARED_FLAGS="-shared";                    SHARED_EXT="dll";   EXE=".exe" ;;
  *)                     SHARED_FLAGS="-shared";                    SHARED_EXT="so";    EXE="" ;;
esac

LIBUSB_CFLAGS="$(pkg-config --cflags libusb-1.0)"
LIBUSB_LIBS="$(pkg-config --libs libusb-1.0)"

echo "== target:   $TARGET ($SRC_DIR)"
echo "== platform: $(uname -s) $(uname -m)"
echo "== CC:       $CC ($("$CC" --version | head -n 1))"
echo "== CXX:      $CXX ($("$CXX" --version | head -n 1))"
echo "== libusb:   $LIBUSB_CFLAGS $LIBUSB_LIBS"

mkdir -p "$OUT_DIR"

# Build list: <std> or <std>:noexc
BUILDS="$CXXSTDS $NOEXC_STD:noexc"

for SPEC in $BUILDS; do
  CXXSTD="${SPEC%%:*}"
  case "$SPEC" in
    *:noexc) MODE_FLAGS="-fno-exceptions"; BUILD="$OUT_DIR/$CXXSTD-noexc" ;;
    *)       MODE_FLAGS="";                BUILD="$OUT_DIR/$CXXSTD" ;;
  esac
  mkdir -p "$BUILD/obj"
  echo
  echo "==== $TARGET: -std=$CXXSTD $MODE_FLAGS"

  # Compile every driver source
  OBJS=""
  for SRC in "$SRC_DIR"/*.cpp; do
    OBJ="$BUILD/obj/$(basename "${SRC%.cpp}").o"
    echo "-- compile $(basename "$SRC")"
    # shellcheck disable=SC2086
    "$CXX" -std="$CXXSTD" $WARN $MODE_FLAGS -fPIC -pthread $LIBUSB_CFLAGS -I"$SRC_DIR" \
      -c "$SRC" -o "$OBJ"
    OBJS="$OBJS $OBJ"
  done

  # Static library
  STATIC="$BUILD/libusbsiddrv.a"
  rm -f "$STATIC"
  # shellcheck disable=SC2086
  ar rcs "$STATIC" $OBJS
  echo "-- static  $(basename "$STATIC")"

  # Shared library, fails on unresolved symbols
  SHARED="$BUILD/libusbsiddrv.$SHARED_EXT"
  # shellcheck disable=SC2086
  "$CXX" $SHARED_FLAGS -pthread $OBJS $LIBUSB_LIBS -o "$SHARED"
  echo "-- shared  $(basename "$SHARED")"

  # C API tests: strict C compile, C++ linker for the C++ runtime
  for T in $C_TESTS; do
    for CSTD in $CSTDS; do
      NAME="${T%.c}-$CSTD"
      echo "-- test    $NAME"
      "$CC" -std="$CSTD" -pedantic -Wall -Wextra -Werror -I"$SRC_DIR" \
        -c "$TESTS/$T" -o "$BUILD/$NAME.o"
      # shellcheck disable=SC2086
      "$CXX" -pthread "$BUILD/$NAME.o" "$STATIC" $LIBUSB_LIBS -o "$BUILD/$NAME$EXE"
      "$BUILD/$NAME$EXE"
    done
  done

  # C++ API tests
  for T in $CXX_TESTS; do
    NAME="${T%.cpp}"
    echo "-- test    $NAME"
    # shellcheck disable=SC2086
    "$CXX" -std="$CXXSTD" $WARN $MODE_FLAGS -pthread $LIBUSB_CFLAGS -I"$SRC_DIR" \
      "$TESTS/$T" "$STATIC" $LIBUSB_LIBS -o "$BUILD/$NAME$EXE"
    "$BUILD/$NAME$EXE"
  done
done

echo
echo "==== $TARGET: all builds and tests passed ($BUILDS)"
