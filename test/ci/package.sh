#!/usr/bin/env bash
#
# Stage USBSID-Pico driver release files.
#
# Usage: test/ci/package.sh <native|java> <version> <out_dir>
#
#   native  source archives (.tar.gz and .zip) of src/ and libusbsiddrv-vice/,
#           each with LICENSE, README.md and a VERSION file
#   java    the built driver JAR and its dependency reduced POM, run after
#           `mvn package` in java/usbsid-usb-driver-library-java
#
# <version> is the tag without its prefix: 1.0.0 for v1.0.0, 1.2 for
# java-v1.2. For java it must equal the <version> in pom.xml.
# Writes SHA256SUMS next to the staged files. Existing files in <out_dir>
# with the same names are replaced, nothing else there is touched.

set -eu

KIND="${1:-}"
VERSION="${2:-}"
OUT_DIR="${3:-}"
if [ -z "$KIND" ] || [ -z "$VERSION" ] || [ -z "$OUT_DIR" ]; then
  echo "usage: $0 <native|java> <version> <out_dir>" >&2
  exit 2
fi

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"

# sha256sum on Linux and MSYS2, shasum on macOS
sha256() {
  if command -v sha256sum >/dev/null 2>&1; then sha256sum "$@"; else shasum -a 256 "$@"; fi
}

case "$KIND" in
  native)
    STAGE="$(mktemp -d)"
    FILES=""
    for PART in src libusbsiddrv-vice; do
      case "$PART" in
        src)               NAME="USBSID-Pico-driver-src-$VERSION" ;;
        libusbsiddrv-vice) NAME="USBSID-Pico-driver-vice-$VERSION" ;;
      esac
      mkdir -p "$STAGE/$NAME"
      cp "$ROOT/$PART"/*.cpp "$ROOT/$PART"/*.h "$STAGE/$NAME/"
      # The VICE copy ships its own LICENSE, README.md and Makefile.am
      for F in LICENSE README.md Makefile.am; do
        if [ -f "$ROOT/$PART/$F" ]; then
          cp "$ROOT/$PART/$F" "$STAGE/$NAME/"
        elif [ "$F" != "Makefile.am" ]; then
          cp "$ROOT/$F" "$STAGE/$NAME/"
        fi
      done
      echo "$VERSION" > "$STAGE/$NAME/VERSION"
      (cd "$STAGE" && tar -czf "$OUT_DIR/$NAME.tar.gz" "$NAME")
      rm -f "$OUT_DIR/$NAME.zip"
      (cd "$STAGE" && zip -qr "$OUT_DIR/$NAME.zip" "$NAME")
      FILES="$FILES $NAME.tar.gz $NAME.zip"
    done
    ;;

  java)
    JDIR="$ROOT/java/usbsid-usb-driver-library-java"
    ARTIFACT="usbsid-usb-driver-library-java"
    JAR="$JDIR/target/$ARTIFACT-$VERSION.jar"
    POM="$JDIR/dependency-reduced-pom.xml"
    # Project <version>: first <version> after the project's own <artifactId>
    POM_VERSION="$(awk -v a="<artifactId>$ARTIFACT</artifactId>" '
      index($0, a) { found = 1; next }
      found && /<version>/ { gsub(/.*<version>|<\/version>.*/, ""); print; exit }
    ' "$JDIR/pom.xml")"
    if [ "$POM_VERSION" != "$VERSION" ]; then
      echo "ERROR: version '$VERSION' does not match pom.xml version '$POM_VERSION'" >&2
      exit 1
    fi
    if [ ! -f "$JAR" ]; then
      echo "ERROR: $JAR not found" >&2
      echo "Tag version '$VERSION' must equal the <version> in pom.xml, built JARs:" >&2
      ls -l "$JDIR"/target/*.jar >&2 2>/dev/null || true
      exit 1
    fi
    if [ ! -f "$POM" ]; then
      echo "ERROR: $POM not found, run mvn package first" >&2
      exit 1
    fi
    cp "$JAR" "$OUT_DIR/$ARTIFACT-$VERSION.jar"
    cp "$POM" "$OUT_DIR/$ARTIFACT-$VERSION.pom"
    FILES="$ARTIFACT-$VERSION.jar $ARTIFACT-$VERSION.pom"
    ;;

  *)
    echo "usage: $0 <native|java> <version> <out_dir>" >&2
    exit 2
    ;;
esac

# shellcheck disable=SC2086
(cd "$OUT_DIR" && sha256 $FILES > SHA256SUMS)
echo "Staged in $OUT_DIR:"
(cd "$OUT_DIR" && ls -l $FILES SHA256SUMS)
