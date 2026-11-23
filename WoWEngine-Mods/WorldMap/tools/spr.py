"""Read and write the game's .SPR sprites (RLE rows of (index, run)), as numpy arrays of colour indices."""
import struct
import numpy as np

def read(path, frame=0):
    d = open(path, "rb").read()
    w, h, n = struct.unpack_from("<HHH", d, 0)
    fs = struct.unpack_from("<I", d, 6 + 4 * frame)[0]
    out = np.zeros((h, w), np.uint8)
    for y in range(h):
        p = fs + struct.unpack_from("<I", d, fs + 4 * y)[0]
        x = 0
        while x < w and p + 1 < len(d):
            idx, cnt = d[p], d[p + 1]
            p += 2
            if cnt == 0:
                break
            r = min(cnt, w - x)
            out[y, x:x + r] = idx
            x += r
    return out

def write(path, img):
    """one frame; runs of up to 255"""
    h, w = img.shape
    rows = []
    for y in range(h):
        row = img[y]
        b = bytearray()
        x = 0
        while x < w:
            v = row[x]
            r = 1
            while x + r < w and row[x + r] == v and r < 255:
                r += 1
            b += bytes((int(v), r))
            x += r
        rows.append(bytes(b))
    head = struct.pack("<HHH", w, h, 1)
    frame_start = 6 + 4
    table = bytearray()
    off = 4 * h
    for r in rows:
        table += struct.pack("<I", off)
        off += len(r)
    data = head + struct.pack("<I", frame_start) + bytes(table) + b"".join(rows)
    open(path, "wb").write(data)

def palette(path):
    d = open(path, "rb").read()
    return np.frombuffer(d[:768], np.uint8).reshape(256, 3).astype(np.uint16) * 255 // 63

def shade(path, level=11):
    """a .SHH table's level as 256 RGB colours (RGB565 entries)"""
    d = open(path, "rb").read()
    e = np.frombuffer(d[1 + level * 512: 1 + (level + 1) * 512], "<u2").astype(np.uint32)
    r = ((e >> 11) & 31) * 255 // 31
    g = ((e >> 5) & 63) * 255 // 63
    b = (e & 31) * 255 // 31
    return np.stack([r, g, b], 1).astype(np.uint8)
