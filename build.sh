#!/bin/sh
set -eu
cd "$(dirname "$0")"
ROOT=$(pwd)
export JIELI_TOOLCHAIN="$ROOT/.deps/jieli/toolchain"
export AC79_SDK="$ROOT/.deps/ac79-sdk"
export X0X_FONTSET=terminus
if [ ! -x .deps/python/bin/python3 ] || [ ! -f .deps/fm1-base/tools/build.py ]; then
    echo "Run ./setup.sh first." >&2
    exit 1
fi
.deps/python/bin/python3 scripts/ensure-battery-platform.py
exec .deps/python/bin/python3 .deps/fm1-base/tools/build.py "$ROOT" "$@"
