import sys, glob, os, struct, colorsys, pickle
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pix import read_pix
# usage: python3 load.py <game DATA directory>
D = sys.argv[1]
def load_pal(p):
    d = open(p, 'rb').read(); i = d.index(b'\x00\x00\x00\x21')
    size, count, elem = struct.unpack('>III', d[i+4:i+16]); raw = d[i+16:i+16+count*elem]
    return [(raw[4*k+1], raw[4*k+2], raw[4*k+3]) for k in range(count)]
pal = load_pal(D + '/REG/PALETTES/DRRENDER.PAL')
texs = {}
for f in sorted(glob.glob(D + '/PIXELMAP/*.PIX') + glob.glob(D + '/PIXELMAP/*.pix')):
    try:
        for n, w, h, t, rb, px in read_pix(f):
            if t == 3 and px is not None and w >= 4 and h >= 4:
                key = n.upper()
                if key not in texs:
                    texs[key] = (os.path.basename(f), w, h, rb, px)
    except Exception as e:
        print('ERR', f, e)
pickle.dump((pal, texs), open('texs.pkl', 'wb'))
print(len(texs))
