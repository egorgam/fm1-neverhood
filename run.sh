#!/bin/sh
set -eu
cd "$(dirname "$0")"
./build.sh
exec .deps/fm1-emu/target/release/fm1-live --fwsc "$(pwd)/build/neverhood.fwsc" --midi none "$@"
