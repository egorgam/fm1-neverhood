#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p .deps
checkout() {
    if [ ! -d "$1/.git" ]; then
        git clone --no-checkout "$2" "$1"
        git -C "$1" checkout --detach "$3"
    fi
    [ "$(git -C "$1" rev-parse HEAD)" = "$3" ] || { echo "Unexpected dependency version: $1" >&2; exit 1; }
}
checkout .deps/fm1-base https://github.com/charlesvestal/fm1-base.git e35f7e2c6904ff1a3eed10035474c8cf089c0b88
checkout .deps/fm1-base/bsp https://github.com/hugelton/felucca-bsp.git 33cac693903e753e8783baeed05c19cadfb04812
checkout .deps/fm1-emu https://github.com/joakimeriksson/fm1-emu.git 7856393bc82f2821a2ac70c0c9a93dd743465dcb
if [ ! -x .deps/python/bin/python3 ]; then
    "${PYTHON:-python3}" -m venv .deps/python
fi
.deps/python/bin/python3 -m pip install Pillow fonttools 'audioop-lts; python_version >= "3.13"'
if [ ! -x .deps/jieli/toolchain/pi32v2/bin/clang ]; then
    sh .deps/fm1-base/tools/get_toolchain.sh .deps/jieli
fi
.deps/python/bin/python3 scripts/fetch-sdk.py
cargo build --locked --release --manifest-path .deps/fm1-emu/Cargo.toml -p fm1-live -p fm1-emu
echo "Ready. Run ./run.sh"
