import pickle, colorsys
pal, texs = pickle.load(open('texs.pkl', 'rb'))
hsv = [colorsys.rgb_to_hsv(*(c / 255 for c in p)) for p in pal]
def is_cand(i):
    h, s, v = hsv[i]
    if i == 0: return False
    return (v >= 0.85 and s >= 0.45) or (v >= 0.93 and 0.12 <= s < 0.45)
CAND = [is_cand(i) for i in range(256)]
if __name__ == '__main__':
    print('candidate indices', [i for i in range(256) if CAND[i]])
    stats = []
    for n, (f, w, h, rb, px) in texs.items():
        c = 0
        for y in range(h):
            row = px[y*rb:y*rb+w]
            c += sum(CAND[b] for b in row)
        stats.append((c / (w*h), c, n, f, w, h))
    stats.sort(reverse=True)
    nz = [s for s in stats if s[1] >= 4]
    print('textures', len(stats), 'with >=4 candidate texels', len(nz))
    pickle.dump(nz, open('cands.pkl', 'wb'))
    import collections
    print(collections.Counter(s[3][:4] for s in nz).most_common(30))
