import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pickle, sys, json
from PIL import Image, ImageDraw
from cand import CAND
pal, texs = pickle.load(open('texs.pkl','rb'))
def render(name, mask_fn=None, size=140):
    f, w, h, rb, px = texs[name]
    im = Image.new('RGB', (w, h)); hi = Image.new('RGB', (w, h))
    a = im.load(); b = hi.load()
    for y in range(h):
        for x in range(w):
            i = px[y*rb+x]; c = pal[i]
            a[x, y] = c if i else (255, 0, 255)
            m = mask_fn(name, x, y, i) if mask_fn else CAND[i]
            b[x, y] = c if m else (c[0]//4, c[1]//4, c[2]//4)
    s = min(size / w, size / h)
    W, H = max(1, int(w*s)), max(1, int(h*s))
    return im.resize((W, H), Image.NEAREST), hi.resize((W, H), Image.NEAREST)
def sheet(names, out, mask_fn=None, cols=6, cell=140):
    rows = (len(names) + cols - 1) // cols
    S = Image.new('RGB', (cols*(2*cell+16), rows*(cell+16)), (40, 40, 40)); d = ImageDraw.Draw(S)
    for k, n in enumerate(names):
        x0 = (k % cols) * (2*cell+16); y0 = (k // cols) * (cell+16)
        a, b = render(n, mask_fn, cell)
        S.paste(a, (x0, y0)); S.paste(b, (x0+cell+4, y0))
        d.text((x0, y0+cell+2), f'{k}:{n} ({texs[n][1]}x{texs[n][2]})', fill=(255,255,0))
    S.save(out)
if __name__ == '__main__':
    names = json.load(open(sys.argv[1]))
    per = 36
    for i in range(0, len(names), per):
        sheet(names[i:i+per], f'{sys.argv[2]}{i//per:02d}.png')
