#!/bin/sh
set -eu
cd "$(dirname "$0")"
./build.sh --host
mkdir -p build/check
build/host/neverhood_host tests/basic.txt build/check
mkdir -p build/check/player
build/host/neverhood_host tests/player.txt build/check/player
.deps/python/bin/python3 scripts/check-art-pack.py
.deps/python/bin/python3 scripts/check-ui.py
.deps/python/bin/python3 scripts/check-effects-animation.py
.deps/python/bin/python3 scripts/check-battery.py
.deps/python/bin/python3 scripts/check-walk.py
.deps/python/bin/python3 scripts/check-effects-kit.py
.deps/python/bin/python3 scripts/check-music-bank.py
.deps/python/bin/python3 scripts/check-drum-kit.py
cc -O2 -std=c99 -DPLAT_HOST -ffp-contract=off \
    -fsanitize=address,float-cast-overflow -fno-sanitize-recover=all \
    -I. -I.deps/fm1-base/include -Ibuild/gen \
    tests/dsp_stress.c clay_synth.c effects_kit.c music_bank.c -o build/check/dsp_stress
build/check/dsp_stress
cc -O2 -std=c99 -DPLAT_HOST -ffp-contract=off \
    -fsanitize=address,float-cast-overflow -fno-sanitize-recover=all \
    -I. -I.deps/fm1-base/include -Ibuild/gen \
    tests/mono.c effects_kit.c music_bank.c -o build/check/mono
cc -O2 -std=c99 -DPLAT_HOST -ffp-contract=off \
    -fsanitize=address,float-cast-overflow -fno-sanitize-recover=all \
    -I. -I.deps/fm1-base/include -Ibuild/gen \
    tests/player_engine.c effects_kit.c music_bank.c -o build/check/player-engine
build/check/player-engine
build/check/mono
cc -O2 -std=c99 -fsanitize=address,undefined -fno-sanitize-recover=all \
    -I. -Ibuild/gen tests/klaymen_stress.c klaymen.c -o build/check/klaymen_stress
build/check/klaymen_stress
.deps/python/bin/python3 scripts/render-bank.py
# The target package must be built before checking its machine code in fm1-emu.
if [ "${1:-}" = "--emulator" ]; then
    ./build.sh
    .deps/python/bin/python3 scripts/check-emulator.py
fi
