"""Small lossless LZ stream for immutable indexed art (128-byte literal/match runs)."""
from collections import defaultdict, deque


def pack(data):
    data = bytes(data)
    history = defaultdict(lambda: deque(maxlen=32))
    out, literal = bytearray(), bytearray()

    def flush():
        while literal:
            n = min(128, len(literal))
            out.append(n - 1)
            out.extend(literal[:n])
            del literal[:n]

    pos = 0
    while pos < len(data):
        key = data[pos:pos + 3]
        best, distance = 0, 0
        if len(key) == 3:
            for previous in reversed(history[key]):
                gap = pos - previous
                if gap > 65535:
                    break
                n = 3
                while n < 130 and pos + n < len(data) and data[pos + n] == data[pos + n - gap]:
                    n += 1
                if n > best:
                    best, distance = n, gap
                    if n == 130:
                        break
        n = best if best >= 4 else 1
        if n > 1:
            flush()
            out.extend((128 + n - 3, distance & 255, distance >> 8))
        else:
            literal.append(data[pos])
            if len(literal) == 128:
                flush()
        for i in range(pos, pos + n):
            if i + 3 <= len(data):
                history[data[i:i + 3]].append(i)
        pos += n
    flush()
    assert unpack(out) == data
    return out


def unpack(data):
    out, pos = bytearray(), 0
    while pos < len(data):
        token = data[pos]
        pos += 1
        if token < 128:
            n = token + 1
            out.extend(data[pos:pos + n])
            pos += n
        else:
            n = (token & 127) + 3
            distance = data[pos] | data[pos + 1] << 8
            pos += 2
            for _ in range(n):
                out.append(out[-distance])
    return out


def c_packed(name, data):
    """Raw DEFLATE, decoded into the final buffer by the bounded tinf decoder."""
    import zlib
    from pathlib import Path
    from palette_art import c_array
    check = Path(__file__).resolve().parents[1]/"build/check/art-lossless"
    check.mkdir(parents=True, exist_ok=True)
    (check/(name+".raw")).write_bytes(bytes(data))
    compressor = zlib.compressobj(9, zlib.DEFLATED, -15)
    encoded = compressor.compress(bytes(data))+compressor.flush()
    assert zlib.decompress(encoded, -15) == bytes(data)
    print(f"Lossless {name}: {len(data)} -> {len(encoded)} B (DEFLATE)")
    return ([f"#define {name}_SIZE {len(data)}u", f"static uint8_t {name}[{name}_SIZE] PLAT_POOL;"]
            + c_array(name+"_PACKED", encoded))
