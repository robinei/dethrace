# Minimal BRender .PIX/.PAL reader: python3 pix.py <file> -> list of (name, w, h, type, rowbytes, pixels)
import struct, sys
def read_pix(path):
    d = open(path, 'rb').read(); off = 0; out = []; cur = None
    while off + 8 <= len(d):
        cid, ln = struct.unpack_from('>II', d, off); body = d[off+8:off+8+ln]; off += 8 + ln
        if cid in (0x03, 0x3D):
            t, rb, w, h = struct.unpack_from('>BHHH', body, 0)
            p = 7 + 4 + (2 if cid == 0x3D else 0)
            name = body[p:].split(b'\0')[0].decode('latin1')
            cur = [name, w, h, t, rb, None]
        elif cid == 0x21 and cur:
            cnt, es = struct.unpack_from('>II', body, 0); cur[5] = body[8:8+cnt*es]; out.append(tuple(cur)); cur = None
    return out
if __name__ == '__main__':
    for n, w, h, t, rb, px in read_pix(sys.argv[1]): print(n, w, h, t, rb, len(px))
