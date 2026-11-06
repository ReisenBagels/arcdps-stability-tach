#!/usr/bin/env python3
"""Renders the README demo GIFs (docs/gifs/*.gif).

This is a Python port of the drawing code in src/gauge.cpp and the presets in src/settings.cpp, driven by *synthetic*
party data. The GIFs are NOT in-game captures: in-game text uses ImGui's font and the real events come from ArcDPS.
Usage:  python3 tools/render_demos.py [name ...]      (needs Pillow)
"""
import math, os, random, sys
from PIL import Image, ImageDraw, ImageFont

K = 2                      # supersampling factor
CW = 640                   # design width of one dial cell
OUT = os.path.join(os.path.dirname(__file__), "..", "docs", "gifs")
NAMES = ["You", "Aria", "Bolt", "Cinder", "Dax"]
BOONS = [("Stability", 25), ("Might", 25), ("Fury", 1), ("Quickness", 1), ("Alacrity", 1), ("Protection", 1)]

_FONT = next(p for p in ["/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", "DejaVuSans-Bold.ttf"] if os.path.exists(p) or "/" not in p)
_fc = {}
def font(px):
    px = max(6, int(round(px)))
    if px not in _fc: _fc[px] = ImageFont.truetype(_FONT, px)
    return _fc[px]

# ------------------------------------------------------------------ settings / presets (mirror Settings::applyPreset)
def make_cfg(preset=0, **over):
    c = dict(tType=0, tShape=0, tNeedle=0, tFill=0, tTicks=0, tRedline=False, tBezelW=4.0,
             cFace=(.07, .07, .086), cFace2=(.11, .11, .13), cBezel=(.23, .23, .26), cText=(.90, .90, .92), cTick=(.55, .54, .58),
             cTrack=(.15, .15, .17), cHub=(.17, .17, .19), cReadout=(.95, .95, .96), cMax=(1, .82, .48), cRedline=(.90, .15, .10),
             needleCol=(1, .23, .19), needleThick=3.0, needleLen=1.0, needleSmooth=14.0,
             fireOn=True, fireCol=(1, .48, .10), fireAmt=9.0, gradient=False,
             playerCol=[(.29, .87, .50), (.22, .74, .97), (.98, .75, .14), (.96, .45, .71), (.65, .55, .98)],
             imgOn=False, imgAbove=True, imgSize=150.0, imgOpacity=1.0, imgYOff=0.0)
    if preset == 1:    # Altezza-inspired
        c.update(tTicks=2, tRedline=True, tBezelW=9.0, needleThick=2.5, cFace=(.95, .95, .93), cFace2=(1, 1, 1), cBezel=(.72, .74, .78),
                 cText=(.08, .08, .09), cTick=(.15, .15, .17), cTrack=(.82, .83, .85), cHub=(.70, .72, .76), cReadout=(.08, .08, .09),
                 cMax=(.80, .10, .10), needleCol=(.85, .08, .08), cRedline=(.85, .10, .10))
    elif preset == 2:  # Starlet-inspired
        c.update(tTicks=2, tRedline=True, tNeedle=2, tBezelW=7.0, needleThick=3.0, cFace=(.03, .03, .035), cFace2=(.09, .09, .10),
                 cBezel=(.28, .28, .30), cText=(.95, .95, .95), cTick=(.95, .95, .95), cTrack=(.10, .10, .11), cHub=(.45, .45, .48),
                 cReadout=(1, .60, .15), cMax=(1, .62, .20), needleCol=(1, .35, .08))
    elif preset == 3:  # LFA-inspired
        c.update(tTicks=2, tRedline=True, tNeedle=1, tFill=1, tBezelW=2.0, needleThick=2.0, cFace=(.02, .02, .025), cFace2=(.06, .06, .07),
                 cBezel=(.45, .45, .50), cText=(.92, .92, .95), cTick=(.60, .60, .65), cTrack=(.07, .07, .08), cHub=(.12, .12, .14),
                 cReadout=(1, 1, 1), cMax=(1, .55, .10), needleCol=(1, .60, .10), cRedline=(.95, .10, .05))
    elif preset == 4:
        c.update(tType=1, tTicks=1)
    elif preset == 5:
        c.update(tType=2, tShape=1, tFill=1, tTicks=1, tRedline=True, tNeedle=1, needleCol=(1, 1, 1))
    elif preset == 6:  # GM squarebody pickup-inspired
        c.update(tShape=1, tTicks=2, tRedline=True, tBezelW=6.0, needleThick=3.0, needleLen=0.95, cFace=(.04, .04, .045), cFace2=(.10, .10, .11),
                 cBezel=(.55, .55, .58), cText=(.93, .90, .80), cTick=(.93, .90, .80), cTrack=(.12, .12, .13), cHub=(.35, .35, .37),
                 cReadout=(.95, .90, .75), cMax=(1, .55, .15), needleCol=(1, .28, .08), cRedline=(.85, .12, .08))
    c.update(over)
    return c

def col(c, a=1.0): return tuple(int(round(v * 255)) for v in c) + (int(round(max(0, min(1, a)) * 255)),)
def rad(d): return d * math.pi / 180.0
def cell_h(t): return {0: 560, 1: 370, 2: 225}[t]
def geo(t):  # type, CX, CY, R, START, SWEEP, SEG
    return {1: (1, 320, 300, 240, 180, 180, 36), 2: (2, 320, 0, 0, 0, 0, 0)}.get(t, (0, 320, 290, 230, 135, 270, 54))

# ------------------------------------------------------------------ tiny canvas (design units -> supersampled pixels)
class Cv:
    def __init__(s, img, ox, oy, sc): s.im, s.d, s.ox, s.oy, s.sc = img, ImageDraw.Draw(img, "RGBA"), ox, oy, sc
    def P(s, x, y): return (s.ox + x * s.sc * K, s.oy + y * s.sc * K)
    def px(s, v): return v * s.sc * K
    def w(s, v): return max(1, int(round(s.px(v))))
    def circle(s, cx, cy, r, fill): x, y = s.P(cx, cy); r = s.px(r); s.d.ellipse([x - r, y - r, x + r, y + r], fill=fill)
    def ring(s, cx, cy, r, color, wd): x, y = s.P(cx, cy); r = s.px(r); s.d.ellipse([x - r, y - r, x + r, y + r], outline=color, width=s.w(wd))
    def sector(s, cx, cy, r, a0, a1, th, color):
        n = max(2, int(abs(a1 - a0) / 2) + 1)
        out = [s.P(cx + math.cos(rad(a0 + (a1 - a0) * i / n)) * (r + th / 2), cy + math.sin(rad(a0 + (a1 - a0) * i / n)) * (r + th / 2)) for i in range(n + 1)]
        inn = [s.P(cx + math.cos(rad(a0 + (a1 - a0) * i / n)) * (r - th / 2), cy + math.sin(rad(a0 + (a1 - a0) * i / n)) * (r - th / 2)) for i in range(n, -1, -1)]
        s.d.polygon(out + inn, fill=color)
    def line(s, p0, p1, color, wd): s.d.line([s.P(*p0), s.P(*p1)], fill=color, width=s.w(wd))
    def poly(s, pts, color): s.d.polygon([s.P(*p) for p in pts], fill=color)
    def rrect(s, x0, y0, x1, y1, r, fill=None, outline=None, wd=1):
        s.d.rounded_rectangle([s.P(x0, y0), s.P(x1, y1)], radius=s.px(r), fill=fill, outline=outline, width=s.w(wd))
    def text(s, t, x, y, size, color, shadow):
        f, p = font(size * s.sc * K), s.P(x, y)
        if shadow: s.d.text((p[0] + K, p[1] + K), t, font=f, fill=(0, 0, 0, 210), anchor="mm")
        s.d.text(p, t, font=f, fill=color, anchor="mm")

def seg_color(cfg, i, f):
    if not cfg["gradient"]: return col(cfg["playerCol"][i])
    import colorsys
    r, g, b = colorsys.hsv_to_rgb(f * 0.333, 0.85, 0.9)
    return col((r, g, b))

def short(n): return n if len(n) <= 12 else n[:11] + "."

# ------------------------------------------------------------------ dial data (mirror build())
def build(slots, boons, combined):
    d = dict(n=5, pct=combined, frac=[0.0] * 5, val=[0] * 5, name=list(NAMES))
    for i in range(5):
        sm, v = 0.0, 0
        for b in boons:
            mx = BOONS[b][1]; c = min(slots[b][i], mx); sm += c / mx; v = c
        d["frac"][i] = sm / len(boons)
        d["val"][i] = int(round(d["frac"][i] * 100)) if combined else v
    d["label"] = "COMBINED" if combined else BOONS[boons[0]][0].upper()
    return d

class State:
    def __init__(s): s.needle, s.acc, s.parts = 0.0, 0.0, []

def tip_pos(cfg, g, needle):
    if g[0] == 2: return (50 + min(needle, 5000) / 5000 * 540, 102)
    _, CX, CY, R, START, SWEEP, SEG = g
    ang = rad(START + needle / 5000 * SWEEP); ln = (R - 30) * cfg["needleLen"]
    return (CX + math.cos(ang) * ln, CY + math.sin(ang) * ln)

def update(st, d, cfg, g, dt, rnd):
    total = sum(d["frac"][i] * 1000 for i in range(d["n"]))
    allm = all(f >= 0.999 for f in d["frac"])
    st.needle += (total - st.needle) * min(1.0, dt * cfg["needleSmooth"])
    fire = allm and cfg["fireOn"]
    if not cfg["fireOn"]: st.parts.clear()
    if fire:
        st.acc += cfg["fireAmt"] * 60 * dt * (0.3 if g[0] == 2 else 1.0)
        tip = tip_pos(cfg, g, st.needle)
        while st.acc >= 1:
            st.acc -= 1
            if g[0] == 2: st.parts.append([50 + rnd.random() * 540, 112, (rnd.random() - .5) * 1.0, -1.2 - rnd.random() * 2.0, 1.0, 6 + rnd.random() * 8])
            else:
                a = rad(g[4] + rnd.random() * g[5]); rr = g[3] + 10 + rnd.random() * 16
                st.parts.append([g[1] + math.cos(a) * rr, g[2] + math.sin(a) * rr, (rnd.random() - .5) * 1.2, -1.5 - rnd.random() * 2.8, 1.0, 10 + rnd.random() * 16])
            if rnd.random() < .45: st.parts.append([tip[0], tip[1], (rnd.random() - .5) * 1.5, -1 - rnd.random() * 2.5, 1.0, 8 + rnd.random() * 10])
    else: st.acc = 0
    k = dt * 60
    for p in st.parts:
        p[0] += p[2] * k; p[1] += p[3] * k; p[2] += (rnd.random() - .5) * .25 * k; p[4] -= 1.32 * dt
    st.parts = [p for p in st.parts if p[4] > 0][-900:]
    return fire, allm

# ------------------------------------------------------------------ drawing (mirror drawRound / drawBar / fireStep)
def draw_round(cv, g, st, d, cfg, fire, allm, skin_cb, shadow):
    _, CX, CY, R, START, SWEEP, SEG = g
    half = g[0] == 1
    face, bez, bw = col(cfg["cFace"]), col(cfg["cBezel"]), cfg["tBezelW"]
    ctick, ctext, cmax = col(cfg["cTick"]), col(cfg["cText"]), col(cfg["cMax"])
    edge = col(cfg["fireCol"]) if fire else bez
    if cfg["tShape"] == 1:
        b = (CX + R + 28, CY + 44) if half else (CX + R + 28, CY + R + 28)
        cv.rrect(CX - R - 28, CY - R - 28, b[0], b[1], 64, face, bez, bw)
    elif not half:
        cv.circle(CX, CY, R + 28, face); cv.circle(CX, CY, R * .72, col(cfg["cFace2"], .65)); cv.ring(CX, CY, R + 28, edge, bw)
    else:
        arc = [(CX + math.cos(rad(a)) * (R + 28), CY + math.sin(rad(a)) * (R + 28)) for a in range(180, 361, 3)]
        cv.poly(arc, face); cv.poly([(CX - R - 28, CY), (CX + R + 28, CY), (CX + R + 28, CY + 44), (CX - R - 28, CY + 44)], face)
        cv.d.line([cv.P(*p) for p in arc], fill=edge, width=cv.w(bw), joint="curve")
        for a, b in [((CX - R - 28, CY), (CX - R - 28, CY + 44)), ((CX + R + 28, CY), (CX + R + 28, CY + 44)), ((CX - R - 28, CY + 44), (CX + R + 28, CY + 44))]:
            cv.line(a, b, bez, bw)
    LED = 8
    for i in range(5):
        a0, f = START + i * SEG, d["frac"][i]
        cv.sector(CX, CY, R - 14, a0 + 1, a0 + SEG - 1, 22, col(cfg["cTrack"]))
        if f <= 0: continue
        c = seg_color(cfg, i, f)
        if cfg["tFill"] == 0: cv.sector(CX, CY, R - 14, a0 + 1, a0 + 1 + (SEG - 2) * f, 22, c)
        else:
            bs = (SEG - 2) / LED
            for k in range(LED):
                if (k + .5) / LED <= f + .02:
                    b0 = a0 + 1 + k * bs; cv.sector(CX, CY, R - 14, b0 + .5, b0 + bs - .5, 22, c)
    for k in range(26):
        major = k % 5 == 0
        if cfg["tTicks"] == 0 or (not major and cfg["tTicks"] < 2): continue
        a = rad(START + k * SEG / 5); r1 = R + (10 if major else 5)
        cv.line((CX + math.cos(a) * (R + 2), CY + math.sin(a) * (R + 2)), (CX + math.cos(a) * r1, CY + math.sin(a) * r1), ctick, 2.5 if major else 1.5)
    for k in range(6):
        a = rad(START + k * SEG); cv.text(str(k), CX + math.cos(a) * (R + 22), CY + math.sin(a) * (R + 22), 14, ctick, shadow)
    if cfg["tRedline"]: cv.sector(CX, CY, R + 13, START + SWEEP * .9, START + SWEEP, 4, col(cfg["cRedline"]))
    ang, ln, th = rad(START + st.needle / 5000 * SWEEP), (R - 30) * cfg["needleLen"], cfg["needleThick"]
    dx, dy = math.cos(ang), math.sin(ang); px_, py_ = -dy, dx
    pt = lambda al, sd: (CX + dx * al + px_ * sd, CY + dy * al + py_ * sd)
    def needle():
        if cfg["tNeedle"] == 3: return
        nc = col(cfg["needleCol"])
        cv.line(pt(0, 0), pt(ln, 0), col(cfg["needleCol"], .35 if fire else .18), th * 3)
        if cfg["tNeedle"] == 0: cv.poly([pt(-18, -th), pt(ln, -th * .5), pt(ln, th * .5), pt(-18, th)], nc)
        elif cfg["tNeedle"] == 1: cv.line(pt(-18, 0), pt(ln, 0), nc, max(1.5, th * .6))
        else: cv.poly([pt(-12, -th * 1.8), pt(-12, th * 1.8), pt(ln, 0)], nc)
        cv.circle(CX, CY, 10 + th * 1.2, col(cfg["cHub"])); cv.ring(CX, CY, 10 + th * 1.2, bez, 3)
    imgY, imgK = (235 if half else 175) + cfg["imgYOff"], (.8 if half else 1.0)
    if skin_cb and not cfg["imgAbove"]: skin_cb(cv, imgY, imgK)
    needle()
    if skin_cb and cfg["imgAbove"]: skin_cb(cv, imgY, imgK)
    for i in range(5):
        m = rad(START + i * SEG + SEG / 2); x, y = CX + math.cos(m) * (R - 62), CY + math.sin(m) * (R - 62)
        f = d["frac"][i]
        cv.text(short(d["name"][i]), x, y - 9, 17, cmax if f >= .999 else ctext, shadow)
        cv.text(("%d%%" if d["pct"] else "%d") % d["val"][i], x, y + 12, 20, seg_color(cfg, i, f), shadow)
    ro = cmax if allm else col(cfg["cReadout"])
    if not half:
        cv.text("%d" % round(st.needle), CX, CY + 88, 34, ro, shadow); cv.text("x1000 rpm  -  " + d["label"], CX, CY + 112, 13, ctick, shadow)
    else:
        cv.text("%d" % round(st.needle), CX - 70, CY + 24, 24, ro, shadow); cv.text("x1000 rpm  -  " + d["label"], CX + 50, CY + 24, 12, ctick, shadow)

def draw_bar(cv, st, d, cfg, allm, shadow):
    BX0, BX1, BY0, BY1 = 50, 590, 112, 160; SW = (BX1 - BX0) / 5
    ctick, ctext, cmax, bez = col(cfg["cTick"]), col(cfg["cText"]), col(cfg["cMax"]), col(cfg["cBezel"])
    rr = 34 if cfg["tShape"] == 1 else 6
    cv.rrect(0, 0, CW, 225, rr, col(cfg["cFace"]), bez, cfg["tBezelW"])
    for i in range(5):
        x0, x1, f = BX0 + i * SW + 2, BX0 + (i + 1) * SW - 2, d["frac"][i]; mid = (x0 + x1) / 2
        cv.rrect(x0, BY0, x1, BY1, 4, col(cfg["cTrack"]))
        if f > 0:
            c = seg_color(cfg, i, f)
            if cfg["tFill"] == 0: cv.rrect(x0, BY0, x0 + (x1 - x0) * f, BY1, 4, c)
            else:
                bw = (x1 - x0) / 10
                for k in range(10):
                    if (k + .5) / 10 <= f + .02: cv.rrect(x0 + k * bw + 1, BY0, x0 + (k + 1) * bw - 1, BY1, 2, c)
        cv.text(short(d["name"][i]), mid, 40, 16, cmax if f >= .999 else ctext, shadow)
        cv.text(("%d%%" if d["pct"] else "%d") % d["val"][i], mid, 76, 22, seg_color(cfg, i, f), shadow)
    for k in range(26):
        major = k % 5 == 0
        if cfg["tTicks"] == 0 or (not major and cfg["tTicks"] < 2): continue
        x = BX0 + k * SW / 5; cv.line((x, BY1 + 4), (x, BY1 + (14 if major else 9)), ctick, 2.5 if major else 1.5)
    for k in range(6): cv.text(str(k), BX0 + k * SW, BY1 + 28, 14, ctick, shadow)
    if cfg["tRedline"]: cv.d.rectangle([cv.P(BX0 + 540 * .9, BY1 + 4), cv.P(BX1, BY1 + 8)], fill=col(cfg["cRedline"]))
    xN, th = BX0 + min(st.needle, 5000) / 5000 * 540, cfg["needleThick"]; nc = col(cfg["needleCol"])
    if cfg["tNeedle"] != 3:
        cv.line((xN, BY0 - 10), (xN, BY1 + 10), col(cfg["needleCol"], .25), th * 3)
        if cfg["tNeedle"] == 0: cv.d.rectangle([cv.P(xN - th / 2, BY0 - 10), cv.P(xN + th / 2, BY1 + 10)], fill=nc)
        elif cfg["tNeedle"] == 1: cv.line((xN, BY0 - 10), (xN, BY1 + 10), nc, max(1.5, th * .6))
        else: cv.poly([(xN - th * 1.8, BY0 - 16), (xN + th * 1.8, BY0 - 16), (xN, BY1 + 6)], nc)
    cv.text("%d   x1000 rpm  -  %s" % (round(st.needle), d["label"]), 320, 208, 14, cmax if allm else col(cfg["cReadout"]), shadow)

def draw_fire(cv, st, cfg):
    fc = cfg["fireCol"]
    for p in st.parts:
        w = max(0, p[4] - .4) * .9; r = p[5] * p[4] + 2
        c = tuple(fc[i] + (1 - fc[i]) * w for i in range(3))
        cv.circle(p[0], p[1], r, col(fc, p[4] * .25)); cv.circle(p[0], p[1], r * .55, col(c, p[4] * .75))

# ------------------------------------------------------------------ gif assembly
def backdrop(w, h):
    im = Image.new("RGB", (w, h)); d = ImageDraw.Draw(im)
    for y in range(h):
        t = y / max(1, h - 1); d.line([(0, y), (w, y)], fill=(int(58 - 26 * t), int(74 - 30 * t), int(98 - 40 * t)))
    return im

def emblem(size=300):   # placeholder skin art generated here (the real skin is any PNG the user supplies)
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    d.ellipse([6, 6, size - 6, size - 6], fill=(240, 190, 60, 255), outline=(255, 245, 200, 255), width=8)
    c = size / 2; pts = []
    for i in range(10):
        r = size * (0.34 if i % 2 == 0 else 0.15); a = -math.pi / 2 + i * math.pi / 5
        pts.append((c + math.cos(a) * r, c + math.sin(a) * r))
    d.polygon(pts, fill=(120, 40, 20, 255))
    return im

def make_gif(name, duration, data_fn, boons=(0,), combined=False, cfg=None, cfg_fn=None, width=520, fps=10,
             caption=None, scale_fn=None, opacity_fn=None, skin=False, seed=1):
    rnd = random.Random(seed)
    boons = list(boons); n = 1 if combined else min(4, len(boons))
    cfg0 = cfg or make_cfg()
    ty = cfg0["tType"] if cfg_fn is None else max(c["tType"] for c in [cfg_fn(0)])
    g = geo(ty); cols = 1 if n == 1 else 2; rows = (n + cols - 1) // cols
    dw, dh = cols * CW, rows * cell_h(ty); sc0 = width / dw
    W, H = width, int(round(dh * sc0)); CAP = 34
    states = [State() for _ in range(n)]
    emb = emblem() if skin else None
    frames, step, dt = [], 0, 1 / 60
    every = 60 // fps; total = int(duration * 60)
    for step in range(total):
        t = step * dt
        c = cfg_fn(t) if cfg_fn else cfg0
        slots = data_fn(t)
        sets = [build(slots, boons, True)] if combined else [build(slots, [b], False) for b in boons[:4]]
        fires = []
        for i, d in enumerate(sets): fires.append(update(states[i], d, c, g, dt, rnd))
        if step % every: continue
        sc = sc0 * (scale_fn(t) if scale_fn else 1.0)
        img = backdrop(W * K, H * K); base = img.copy()
        lum = .3 * c["cFace"][0] + .59 * c["cFace"][1] + .11 * c["cFace"][2]
        for i, d in enumerate(sets):
            cv = Cv(img, (i % cols) * CW * sc * K, (i // cols) * cell_h(ty) * sc * K, sc)
            allm, fire = fires[i][1], fires[i][0]
            if g[0] == 2: draw_bar(cv, states[i], d, c, allm, lum < .5)
            else:
                def skin_cb(cv_, y, k):
                    if i or not c["imgOn"] or emb is None: return
                    h = c["imgSize"] * k; w = h; e = emb.resize((max(1, int(cv_.px(w))), max(1, int(cv_.px(h)))), Image.LANCZOS)
                    a = e.getchannel("A").point(lambda v: int(v * c["imgOpacity"])); e.putalpha(a)
                    x, yy = cv_.P(320 - w / 2, y - h / 2); cv_.im.paste(e, (int(x), int(yy)), e)
                draw_round(cv, g, states[i], d, c, fire, allm, skin_cb, lum < .5)
            draw_fire(cv, states[i], c)
        if opacity_fn: img = Image.blend(base, img, opacity_fn(t))
        small = img.resize((W, H), Image.LANCZOS)
        out = Image.new("RGB", (W, H + CAP), (14, 18, 26)); out.paste(small, (0, 0))
        if caption:
            txt = caption(t) if callable(caption) else caption; dr = ImageDraw.Draw(out); fs = 15
            while fs > 10 and dr.textlength(txt, font=font(fs)) > W - 20: fs -= 1     # shrink to fit the frame width
            dr.text((12, H + CAP // 2), txt, font=font(fs), fill=(205, 212, 224), anchor="lm")
        frames.append(out)
    sample = Image.new("RGB", (W, (H + CAP) * min(8, len(frames))))
    for i, idx in enumerate(range(0, len(frames), max(1, len(frames) // 8))[:8]): sample.paste(frames[idx], (0, i * (H + CAP)))
    pal = sample.quantize(colors=224, method=Image.Quantize.MEDIANCUT)
    q = [f.quantize(palette=pal, dither=Image.Dither.NONE) for f in frames]
    os.makedirs(OUT, exist_ok=True); path = os.path.join(OUT, name + ".gif")
    q[0].save(path, save_all=True, append_images=q[1:], duration=int(1000 / fps), loop=0, optimize=True, disposal=1)
    print("%-22s %3d frames %5.2f MB  %s" % (name, len(q), os.path.getsize(path) / 1e6, os.path.basename(path)))

def make_multi_gif(name, duration, tachos, canvas, data_fn, caption, fps=10, seed=3):
    """Several independent tachos (own theme / boons / size / opacity / skin) on one backdrop, like several ImGui windows."""
    rnd = random.Random(seed); W, H = canvas; CAP = 34
    emb = emblem()
    ts = []
    for t in tachos:
        c = t["cfg"]; g = geo(c["tType"]); boons = list(t["boons"]); comb = t.get("combined", False)
        n = 1 if comb else min(4, len(boons)); cols = 1 if n == 1 else 2; rows = (n + cols - 1) // cols
        ts.append(dict(t=t, c=c, g=g, boons=boons, comb=comb, n=n, cols=cols, cell=cell_h(c["tType"]),
                       sc=t["width"] / (cols * CW), states=[State() for _ in range(n)]))
    frames, dt = [], 1 / 60; every = 60 // fps
    for step in range(int(duration * 60)):
        tm = step * dt; slots = data_fn(tm)
        for T in ts:
            T["sets"] = [build(slots, T["boons"], True)] if T["comb"] else [build(slots, [b], False) for b in T["boons"][:4]]
            T["fires"] = [update(T["states"][i], d, T["c"], T["g"], dt, rnd) for i, d in enumerate(T["sets"])]
        if step % every: continue
        img = backdrop(W * K, H * K)
        for T in ts:
            c, g, sc = T["c"], T["g"], T["sc"]; ox, oy = T["t"]["pos"]; op = T["t"].get("opacity", 1.0)
            layer = img.copy() if op < 0.999 else img
            lum = .3 * c["cFace"][0] + .59 * c["cFace"][1] + .11 * c["cFace"][2]
            for i, d in enumerate(T["sets"]):
                cv = Cv(layer, (ox + (i % T["cols"]) * CW * sc) * K, (oy + (i // T["cols"]) * T["cell"] * sc) * K, sc)
                fire, allm = T["fires"][i]
                if g[0] == 2: draw_bar(cv, T["states"][i], d, c, allm, lum < .5)
                else:
                    def skin_cb(cv_, y, k, c=c, i=i):
                        if i or not c["imgOn"]: return
                        h = c["imgSize"] * k; e = emb.resize((max(1, int(cv_.px(h))), max(1, int(cv_.px(h)))), Image.LANCZOS)
                        a = e.getchannel("A").point(lambda v: int(v * c["imgOpacity"])); e.putalpha(a)
                        x, yy = cv_.P(320 - h / 2, y - h / 2); cv_.im.paste(e, (int(x), int(yy)), e)
                    draw_round(cv, g, T["states"][i], d, c, fire, allm, skin_cb, lum < .5)
                draw_fire(cv, T["states"][i], c)
            if op < 0.999: img = Image.blend(img, layer, op)
        small = img.resize((W, H), Image.LANCZOS)
        out = Image.new("RGB", (W, H + CAP), (14, 18, 26)); out.paste(small, (0, 0))
        dr = ImageDraw.Draw(out); fs = 15
        while fs > 10 and dr.textlength(caption, font=font(fs)) > W - 20: fs -= 1
        dr.text((12, H + CAP // 2), caption, font=font(fs), fill=(205, 212, 224), anchor="lm")
        frames.append(out)
    sample = Image.new("RGB", (W, (H + CAP) * min(8, len(frames))))
    for i, idx in enumerate(range(0, len(frames), max(1, len(frames) // 8))[:8]): sample.paste(frames[idx], (0, i * (H + CAP)))
    pal = sample.quantize(colors=224, method=Image.Quantize.MEDIANCUT)
    q = [f.quantize(palette=pal, dither=Image.Dither.NONE) for f in frames]
    os.makedirs(OUT, exist_ok=True); path = os.path.join(OUT, name + ".gif")
    q[0].save(path, save_all=True, append_images=q[1:], duration=int(1000 / fps), loop=0, optimize=True, disposal=1)
    print("%-22s %3d frames %5.2f MB  %s" % (name, len(q), os.path.getsize(path) / 1e6, os.path.basename(path)))

# ------------------------------------------------------------------ synthetic party data
def ss(x): x = min(1.0, max(0.0, x)); return x * x * (3 - 2 * x)
def stab(t, i): return int(round(25 * ss((t - .4 - .3 * i) / 1.4) * (1 - ss((t - 4.5 - .15 * i) / .9))))
def might(t, i): return int(round(25 * ss((t - .8 - .25 * i) / 2.5) * (.7 + .06 * i) * (1 - ss((t - 5.0) / 1.0))))
def fury(t, i): return 1 if (.8 + .5 * i) < t < (5.0 + .2 * i) else 0
def quick(t, i): return 1 if (1.2 + .3 * i) < t < (4.6 + .4 * i) else 0
def slots_main(t):
    s = [[0] * 5 for _ in BOONS]
    for i in range(5): s[0][i], s[1][i], s[2][i], s[3][i] = stab(t, i), might(t, i), fury(t, i), quick(t, i)
    return s
def slots_full(t):   # every boon reaches 100% for everyone (so the combined dial can show fire)
    s = slots_main(t)
    for i in range(5): s[1][i] = int(round(25 * ss((t - .8 - .25 * i) / 2.2) * (1 - ss((t - 5.0) / 1.0))))
    return s
def breathing(t, full=False):
    base = [25, 18, 12, 25, 7]; s = [[0] * 5 for _ in BOONS]
    for i in range(5): s[0][i] = 25 if full else int(max(0, min(25, round(base[i] + 3 * math.sin(t * 1.3 + i)))))
    return s

T_MAIN = 6.4

def jobs():
    yield "default-dial", dict(duration=T_MAIN, data_fn=slots_main, cfg=make_cfg(0), caption="Default theme - party stability, fire when everyone is maxed")
    yield "toyota-altezza", dict(duration=T_MAIN, data_fn=slots_main, cfg=make_cfg(1), caption="Toyota Altezza preset (inspired): white face, chrome bezel, red needle")
    yield "toyota-starlet", dict(duration=T_MAIN, data_fn=slots_main, cfg=make_cfg(2), caption="Toyota Starlet preset (inspired): black face, amber readout, wedge needle")
    yield "lexus-lfa", dict(duration=T_MAIN, data_fn=slots_main, cfg=make_cfg(3), caption="Lexus LFA preset (inspired): digital look, LED segments, thin needle")
    yield "half-dial", dict(duration=T_MAIN, data_fn=slots_main, cfg=make_cfg(4), width=560, caption="Half dial (180 degrees)")
    yield "shift-bar", dict(duration=T_MAIN, data_fn=slots_main, cfg=make_cfg(5, fireCol=(.35, .65, 1.0)), width=620, caption="Shift-light bar with LED segments and a blue flame")
    yield "gm-squarebody", dict(duration=T_MAIN, data_fn=slots_main, cfg=make_cfg(6), caption="GM Squarebody pickup preset (inspired): rounded-square cluster, orange needle")
    yield "multi-boon-separate", dict(duration=T_MAIN, data_fn=slots_main, boons=(0, 1, 2, 3), cfg=make_cfg(0), width=760,
                                      caption="Separate dials: Stability, Might, Fury, Quickness")
    yield "multi-boon-combined", dict(duration=T_MAIN, data_fn=slots_full, boons=(0, 1, 2, 3), combined=True, cfg=make_cfg(0),
                                      caption="Combined dial: average of Stability, Might, Fury, Quickness (%)")
    segs = [("Solid fill, blade needle", dict()), ("LED segment fill", dict(tFill=1)),
            ("Wedge needle + minor ticks", dict(tFill=1, tNeedle=2, tTicks=2, needleCol=(1, .6, .1))),
            ("Thin needle, redline, rounded square", dict(tNeedle=1, tTicks=2, tRedline=True, tShape=1, needleCol=(.4, .9, 1.0))),
            ("Red-to-green colouring by stacks", dict(gradient=True, tTicks=1)),
            ("Custom colours + blue fire (all maxed)", dict(tFill=1, tTicks=2, cFace=(.10, .06, .14), cFace2=(.2, .1, .28), cBezel=(.6, .3, .9), cTrack=(.18, .1, .24),
                                                           needleCol=(.9, .5, 1.0), fireCol=(.35, .55, 1.0), cMax=(.7, .85, 1.0),
                                                           playerCol=[(.9, .4, .9), (.5, .8, 1), (1, .8, .3), (.5, 1, .6), (1, .5, .5)]))]
    seg_len = 1.7
    pick = lambda t: segs[min(len(segs) - 1, int(t / seg_len))]
    yield "customisation", dict(duration=seg_len * len(segs), data_fn=lambda t: breathing(t, full=t >= seg_len * (len(segs) - 1) + .2),
                                cfg_fn=lambda t: make_cfg(0, **pick(t)[1]), caption=lambda t: "Customise: " + pick(t)[0])
    yield "window-options", dict(duration=6.0, data_fn=breathing, cfg=make_cfg(0), width=560,
                                 scale_fn=lambda t: 1 - .45 * ss((t - .3) / 1.5) + .45 * ss((t - 4.4) / 1.2),
                                 opacity_fn=lambda t: 1 - .65 * ss((t - 2.2) / 1.4) + .65 * ss((t - 4.2) / 1.2),
                                 caption=lambda t: "Window: resize + opacity  (width %d%%, opacity %d%%)" % (
                                     100 * (1 - .45 * ss((t - .3) / 1.5) + .45 * ss((t - 4.4) / 1.2)), 100 * (1 - .65 * ss((t - 2.2) / 1.4) + .65 * ss((t - 4.2) / 1.2))))
    yield "skin-image", dict(duration=6.0, data_fn=breathing, skin=True,
                             cfg_fn=lambda t: make_cfg(0, imgOn=True, imgSize=130.0, imgYOff=32.0, imgAbove=(t < 2.0 or t >= 4.0), imgOpacity=(1.0 if t < 4.0 else .45)),
                             caption=lambda t: "Skin image (placeholder art): %s" % ("above needle" if t < 2 else "below needle" if t < 4 else "above needle, 45% opacity"))

def multi_jobs():
    yield "multi-tacho", dict(
        duration=T_MAIN, canvas=(900, 500), data_fn=slots_full,
        caption="Three tachos at once: each has its own theme, boons, size, opacity and skin",
        tachos=[dict(cfg=make_cfg(1, imgOn=True, imgSize=110.0, imgYOff=34.0, imgOpacity=1.0), boons=(0,), pos=(10, 10), width=400, opacity=1.0),
                dict(cfg=make_cfg(5, fireCol=(.35, .65, 1.0)), boons=(1,), pos=(430, 10), width=450, opacity=0.85),
                dict(cfg=make_cfg(6, imgOn=True, imgAbove=False, imgSize=120.0, imgYOff=34.0, imgOpacity=0.5), boons=(3, 2), combined=True, pos=(500, 190), width=330, opacity=0.8)])

if __name__ == "__main__":
    want = set(sys.argv[1:])
    for name, kw in jobs():
        if not want or name in want: make_gif(name, **kw)
    for name, kw in multi_jobs():
        if not want or name in want: make_multi_gif(name, **kw)
