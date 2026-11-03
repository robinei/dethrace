import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pickle, json, colorsys
from cand import CAND
from decisions import D
pal, texs = pickle.load(open('texs.pkl','rb'))
hsv = [colorsys.rgb_to_hsv(*(c / 255 for c in p)) for p in pal]
def rule_ok(rule, i):
    if i == 0: return False
    h, s, v = hsv[i]
    if rule == 'cand': return CAND[i]
    if rule == 'bright': return v >= 0.75
    if rule == 'fire': return v >= 0.45
    if rule == 'rects': return v >= 0.55
def inrects(rects, x, y):
    return any(rx <= x < rx+rw and ry <= y < ry+rh for rx, ry, rw, rh in rects)
def rect_indices(name, rects):
    # lamp colours: reasonably bright, and much more common inside the light rects than on the rest of the
    # texture (which removes the body paint around the lights)
    f, w, h, rb, px = texs[name]
    cin = {}; cout = {}; ain = aout = 0
    for y in range(h):
        for x in range(w):
            i = px[y*rb+x]; r = inrects(rects, x, y)
            if r: ain += 1
            else: aout += 1
            if i == 0 or hsv[i][2] < 0.35: continue
            (cin if r else cout)[i] = (cin if r else cout).get(i, 0) + 1
    keep = set()
    for i, c in cin.items():
        if c / ain >= 3 * cout.get(i, 0) / max(aout, 1):
            keep.add(i)
    return keep
def mask(name):
    kind, inten, rule, rects, note = D[name]
    f, w, h, rb, px = texs[name]
    m = set(); idx = set()
    if rule == 'rects':
        keep = rect_indices(name, rects)
    for y in range(h):
        for x in range(w):
            i = px[y*rb+x]
            if rule == 'rects':
                if i not in keep: continue
            elif not rule_ok(rule, i): continue
            if rects and not any(rx <= x < rx+rw and ry <= y < ry+rh for rx, ry, rw, rh in rects): continue
            m.add((x, y)); idx.add(i)
    return m, idx
if __name__ == '__main__':
    table = {}; masks = {}; missing = []
    for name in sorted(D):
        if name not in texs: missing.append(name); continue
        kind, inten, rule, rects, note = D[name]
        m, idx = mask(name)
        if not m: missing.append(name + '(empty)'); continue
        f, w, h, rb, px = texs[name]
        e = {'kind': kind, 'indices': sorted(idx), 'intensity': inten, 'note': note, 'size': [w, h], 'file': f}
        if rects: e['rects'] = rects
        table[name] = e; masks[name] = m
    json.dump(table, open('glow_table.json', 'w'), indent=1)
    pickle.dump(masks, open('masks.pkl', 'wb'))
    print(len(table), 'entries; missing', missing)
    import collections; print(collections.Counter(e['kind'] for e in table.values()))
