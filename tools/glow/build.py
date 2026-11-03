import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pickle, json, colorsys
from cand import CAND
from decisions import D
try:
    from decisions import NO_GROW
except ImportError:
    NO_GROW = set()
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
# Kinds whose marked texels are only the brightest core of a lit pane (window, sign, tube): grow them over
# the rest of the pane, ie. connected texels of the same hue that are somewhat dimmer
GROW_KINDS = ('window', 'lamp', 'sign', 'neon', 'screen')
def hue_close(a, b):
    d = abs(hsv[a][0] - hsv[b][0])
    return min(d, 1 - d) <= 0.07
def pane_colour(core, q):
    hs, ss, vs = hsv[core]; hq, sq, vq = hsv[q]
    if q == 0 or vq < 0.5 or vq < 0.6 * vs:
        return False
    if ss < 0.2:
        # white / pale core: pale neighbours
        return sq < 0.3 and vq >= 0.55
    return sq >= 0.2 and hue_close(core, q)
def grow(name, m):
    f, w, h, rb, px = texs[name]
    seen = set(m); out = set(m)
    # each seed component may only grow within its bounding box, dilated a little: a pane is not much
    # bigger than its bright core, and this stops leaks into walls of the same colour
    comps = []
    rest = set(m)
    while rest:
        st = [rest.pop()]; comp = set(st)
        while st:
            x, y = st.pop()
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    q = (x + dx, y + dy)
                    if q in rest:
                        rest.discard(q); comp.add(q); st.append(q)
        comps.append(comp)
    for comp in comps:
        xs = [p[0] for p in comp]; ys = [p[1] for p in comp]
        bw = max(xs) - min(xs) + 1; bh = max(ys) - min(ys) + 1
        mx = max(2, int(0.75 * bw)); my = max(2, int(0.75 * bh))
        x0, x1, y0, y1 = min(xs) - mx, max(xs) + mx, min(ys) - my, max(ys) + my
        st = [(p, px[p[1]*rb+p[0]]) for p in comp]
        while st:
            (x, y), core = st.pop()
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                qx, qy = x + dx, y + dy
                if not (0 <= qx < w and 0 <= qy < h and x0 <= qx <= x1 and y0 <= qy <= y1) or (qx, qy) in seen:
                    continue
                qi = px[qy*rb+qx]
                if pane_colour(core, qi):
                    seen.add((qx, qy)); out.add((qx, qy)); st.append(((qx, qy), core))
    return out
def boxes(name, m):
    # bounding boxes of the 8-connected parts of the mask
    rest = set(m); out = []
    while rest:
        st = [rest.pop()]; comp = list(st)
        while st:
            x, y = st.pop()
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    q = (x + dx, y + dy)
                    if q in rest:
                        rest.discard(q); comp.append(q); st.append(q)
        xs = [p[0] for p in comp]; ys = [p[1] for p in comp]
        out.append([min(xs), min(ys), max(xs) - min(xs) + 1, max(ys) - min(ys) + 1])
    return sorted(out, key=lambda r: (r[1], r[0]))
def pane_mask(name, m):
    # the table stores indices (+ optional rects): when the pane colours also occur outside the panes,
    # limit them to the panes' bounding boxes
    f, w, h, rb, px = texs[name]
    g = grow(name, m)
    idx = set(px[y*rb+x] for x, y in g)
    leak = any(px[y*rb+x] in idx and (x, y) not in g for y in range(h) for x in range(w))
    rects = boxes(name, g) if leak else None
    if rects and len(rects) > 64:
        # too fragmented to be worth rects: keep the core only
        return m, set(px[y*rb+x] for x, y in m), None
    if rects:
        g = set((x, y) for y in range(h) for x in range(w) if px[y*rb+x] in idx and inrects(rects, x, y))
    return g, idx, rects
if __name__ == '__main__':
    table = {}; masks = {}; missing = []
    for name in sorted(D):
        if name not in texs: missing.append(name); continue
        kind, inten, rule, rects, note = D[name]
        m, idx = mask(name)
        if not m: missing.append(name + '(empty)'); continue
        if kind in GROW_KINDS and not rects and name not in NO_GROW:
            m, idx, rects = pane_mask(name, m)
        f, w, h, rb, px = texs[name]
        e = {'kind': kind, 'indices': sorted(idx), 'intensity': inten, 'note': note, 'size': [w, h], 'file': f}
        if rects: e['rects'] = rects
        table[name] = e; masks[name] = m
    json.dump(table, open('glow_table.json', 'w'), indent=1)
    pickle.dump(masks, open('masks.pkl', 'wb'))
    print(len(table), 'entries; missing', missing)
    import collections; print(collections.Counter(e['kind'] for e in table.values()))
