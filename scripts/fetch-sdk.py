#!/usr/bin/env python3
"""Fetch only the three packaging files and licence from the pinned JieLi SDK."""
import hashlib
from pathlib import Path
import urllib.request

REV = "d179b4484759423312073f5fbb232501aa491047"
ROOT = Path(__file__).resolve().parents[1] / ".deps" / "ac79-sdk"
FILES = {
    "cpu/wl82/tools/uboot.boot": "4e3b4c220dc96641cb5a723f41e68ce41d5261ae9434bb33fbd7f2c59976ded4",
    "cpu/wl82/tools/cfg_tool.bin": "276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b",
    "cpu/wl82/tools/cfg/eq_cfg_hw.bin": "41167491bffed4651750719c973d2758adeb9021a5670d02d6a53c85ed80ea7d",
    "LICENSE": None,
}
for relative, expected in FILES.items():
    path = ROOT / relative
    if path.exists() and (expected is None or hashlib.sha256(path.read_bytes()).hexdigest() == expected):
        continue
    url = f"https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK/raw/{REV}/{relative}"
    with urllib.request.urlopen(url, timeout=60) as response:
        data = response.read()
    if expected and hashlib.sha256(data).hexdigest() != expected:
        raise SystemExit(f"SDK checksum mismatch: {relative}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    print(f"SDK: {relative}")
