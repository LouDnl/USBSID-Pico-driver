#!/usr/bin/env bash
#
# Copy the driver sources from src/ and the root README.md and LICENSE into
# libusbsiddrv-vice/.
#
# Usage: test/ci/sync_vice.sh
#
# libusbsiddrv-vice/ holds only Makefile.am and NOTICE.md, the rest of the
# VICE driver is src/*.cpp, src/*.h, README.md and LICENSE. Existing files with
# the same names are replaced, nothing else in the directory is touched.

set -eu

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

cp "$ROOT"/src/*.cpp "$ROOT"/src/*.h "$ROOT/README.md" "$ROOT/LICENSE" "$ROOT/libusbsiddrv-vice/"
echo "== copied src/*.cpp src/*.h README.md LICENSE into libusbsiddrv-vice/"
