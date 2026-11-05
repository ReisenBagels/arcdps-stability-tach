#include "gauge.h"
#include "skin.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
constexpr float kPi = 3.14159265f, CW = 640.f;
float rad(float d) { return d * kPi / 180.f; }
float frand() { return (float)rand() / (float)RAND_MAX; }
float cellH(int type) { return type == 1 ? 370.f : type == 2 ? 225.f : 500.f; }

struct Particle { float x, y, vx, vy, l, s; };
struct DialState { float needle = 0.f, acc = 0.f; std::vector<Particle> parts; };
DialState g_state[4];

struct DialData { std::string label, name[5]; float frac[5] = {0, 0, 0, 0, 0}; int val[5] = {0, 0, 0, 0, 0}; bool pct = false; int n = 0; };
struct Geo { int type; float CX, CY, R, START, SWEEP, SEG; };
Geo geoFor(int type) {
    if (type == 1) return {1, 320, 300, 240, 180, 180, 36};
    if (type == 2) return {2, 320, 0, 0, 0, 0, 0};
    return {0, 320, 290, 230, 135, 270, 54};
}

ImU32 col3(const float* c, float a = 1.f) { return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], a)); }
ImU32 segColor(const Settings& s, int i, float f) {
    if (!s.gradient) return col3(s.playerCol[i]);
    float r, g, b; ImGui::ColorConvertHSVtoRGB(f * 0.333f, 0.85f, 0.9f, r, g, b);
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, 1.f));
}
std::string shortName(const std::string& n) { return n.size() > 12 ? n.substr(0, 11) + "." : n; }

// text centred on (x,y) in design units; shadow only helps on dark faces
void text(ImDrawList* dl, ImVec2 o, float sc, const std::string& t, float x, float y, float size, ImU32 c, bool shadow) {
    ImFont* f = ImGui::GetFont();
    float fs = size * sc;
    ImVec2 ts = f->CalcTextSizeA(fs, FLT_MAX, 0.f, t.c_str());
    ImVec2 p(o.x + x * sc - ts.x * .5f, o.y + y * sc - ts.y * .5f);
    if (shadow) dl->AddText(f, fs, ImVec2(p.x + 1, p.y + 1), IM_COL32(0, 0, 0, 210), t.c_str());
    dl->AddText(f, fs, p, c, t.c_str());
}

std::vector<int> selectedBoons(const Settings& s) {
    std::vector<int> v;
    for (int i = 0; i < BOON_COUNT; i++) if ((s.boonMask >> i) & 1) v.push_back(i);
    if (v.empty()) v.push_back(0);
    return v;
}
int dialCount(const Settings& s) {
    auto sel = selectedBoons(s);
    return (s.boonMode == 1 && sel.size() > 1) ? 1 : (int)std::min<size_t>(4, sel.size());
}

DialData build(const std::vector<PartySlot>& slots, const std::vector<int>& boons) {
    DialData d; d.n = (int)std::min<size_t>(5, slots.size()); d.pct = boons.size() > 1;
    for (int i = 0; i < d.n; i++) {
        float sum = 0; int v = 0;
        for (int b : boons) { int mx = kBoons[b].maxShown, c = std::min(slots[i].stacks[b], mx); sum += c / (float)mx; v = c; }
        d.frac[i] = sum / (float)boons.size();
        d.val[i] = d.pct ? (int)std::lround(d.frac[i] * 100.f) : v;
        d.name[i] = slots[i].name;
    }
    if (boons.size() == 1) { d.label = kBoons[boons[0]].name; for (auto& ch : d.label) ch = (char)toupper((unsigned char)ch); }
    else d.label = "COMBINED";
    return d;
}

// ---- fire (shared by all layouts) ----
void fireStep(ImDrawList* dl, ImVec2 o, float sc, const Geo& g, DialState& st, const Settings& s, bool fire, ImVec2 tip, float dt) {
    auto P = [&](float x, float y) { return ImVec2(o.x + x * sc, o.y + y * sc); };
    if (!s.fireOn) st.parts.clear();
    if (fire) {
        st.acc += s.fireAmt * 60.f * dt;
        for (; st.acc >= 1.f; st.acc -= 1.f) {
            if (g.type == 2) st.parts.push_back({50.f + frand() * 540.f, 112.f, (frand() - .5f) * 1.2f, -1.5f - frand() * 2.8f, 1.f, 10 + frand() * 16});
            else {
                float a = rad(g.START + frand() * g.SWEEP), rr = g.R + 10 + frand() * 16;
                st.parts.push_back({g.CX + cosf(a) * rr, g.CY + sinf(a) * rr, (frand() - .5f) * 1.2f, -1.5f - frand() * 2.8f, 1.f, 10 + frand() * 16});
            }
            if (frand() < .45f) st.parts.push_back({tip.x, tip.y, (frand() - .5f) * 1.5f, -1.f - frand() * 2.5f, 1.f, 8 + frand() * 10});
        }
    } else st.acc = 0;
    const float k = dt * 60.f;
    for (auto& p : st.parts) { p.x += p.vx * k; p.y += p.vy * k; p.vx += (frand() - .5f) * .25f * k; p.l -= 1.32f * dt; }
    st.parts.erase(std::remove_if(st.parts.begin(), st.parts.end(), [](const Particle& p) { return p.l <= 0.f; }), st.parts.end());
    if (st.parts.size() > 900) st.parts.erase(st.parts.begin(), st.parts.end() - 900);
    for (auto& p : st.parts) {
        float w = std::max(0.f, p.l - .4f) * .9f, r = (p.s * p.l + 2) * sc;
        float c[3] = {s.fireCol[0] + (1 - s.fireCol[0]) * w, s.fireCol[1] + (1 - s.fireCol[1]) * w, s.fireCol[2] + (1 - s.fireCol[2]) * w};
        dl->AddCircleFilled(P(p.x, p.y), r, col3(s.fireCol, p.l * .25f), 12);
        dl->AddCircleFilled(P(p.x, p.y), r * .55f, col3(c, p.l * .75f), 10);
    }
}

void drawSkin(ImDrawList* dl, ImVec2 o, float sc, const Settings& s, float cy, float k) {
    if (!s.imgOn) return;
    ImTextureID tex = Skin::texture();
    if (!tex) return;
    float h = s.imgSize * k, w = h * Skin::aspect();
    auto P = [&](float x, float y) { return ImVec2(o.x + x * sc, o.y + y * sc); };
    dl->AddImage(tex, P(320 - w / 2, cy - h / 2), P(320 + w / 2, cy + h / 2), ImVec2(0, 0), ImVec2(1, 1),
                 IM_COL32(255, 255, 255, (int)(std::clamp(s.imgOpacity, 0.f, 1.f) * 255)));
}

// ---- round (270 deg) and half (180 deg) dials ----
ImVec2 drawRound(ImDrawList* dl, ImVec2 o, float sc, const Geo& g, DialState& st, const DialData& d, const Settings& s, bool fire, bool all, bool skin, bool shadow) {
    auto P = [&](float x, float y) { return ImVec2(o.x + x * sc, o.y + y * sc); };
    const float CX = g.CX, CY = g.CY, R = g.R, START = g.START, SWEEP = g.SWEEP, SEG = g.SEG;
    const bool half = g.type == 1;
    const ImU32 face = col3(s.cFace), bez = col3(s.cBezel), cText = col3(s.cText), cTick = col3(s.cTick), cMaxC = col3(s.cMax);
    const float bw = s.tBezelW * sc;

    // face + bezel
    if (s.tShape == 1) {
        ImVec2 a = P(CX - R - 28, CY - R - 28), b = half ? P(CX + R + 28, CY + 44) : P(CX + R + 28, CY + R + 28);
        dl->AddRectFilled(a, b, face, 64 * sc);
        dl->AddRect(a, b, bez, 64 * sc, 0, bw);
    } else if (!half) {
        dl->AddCircleFilled(P(CX, CY), (R + 28) * sc, face, 96);
        dl->AddCircleFilled(P(CX, CY), R * 0.72f * sc, col3(s.cFace2, 0.65f), 64);
        dl->AddCircle(P(CX, CY), (R + 28) * sc, (fire ? col3(s.fireCol) : bez), 96, bw);
    } else {
        dl->AddRectFilled(P(CX - R - 28, CY), P(CX + R + 28, CY + 44), face);
        dl->PathArcTo(P(CX, CY), (R + 28) * sc, rad(180), rad(360), 64); dl->PathFillConvex(face);
        dl->PathArcTo(P(CX, CY), (R + 28) * sc, rad(180), rad(360), 64); dl->PathStroke(fire ? col3(s.fireCol) : bez, false, bw);
        dl->AddLine(P(CX - R - 28, CY), P(CX - R - 28, CY + 44), bez, bw);
        dl->AddLine(P(CX + R + 28, CY), P(CX + R + 28, CY + 44), bez, bw);
        dl->AddLine(P(CX - R - 28, CY + 44), P(CX + R + 28, CY + 44), bez, bw);
    }

    // segments (one per person)
    constexpr int LED = 8;
    for (int i = 0; i < 5; i++) {
        float a0 = START + i * SEG, f = d.frac[i];
        dl->PathArcTo(P(CX, CY), (R - 14) * sc, rad(a0 + 1), rad(a0 + SEG - 1), 24);
        dl->PathStroke(col3(s.cTrack), false, 22 * sc);
        if (f <= 0.f) continue;
        ImU32 c = segColor(s, i, f);
        if (s.tFill == 0) {
            dl->PathArcTo(P(CX, CY), (R - 14) * sc, rad(a0 + 1), rad(a0 + 1 + (SEG - 2) * f), 24);
            dl->PathStroke(c, false, 22 * sc);
        } else {
            float bs = (SEG - 2) / LED;
            for (int k = 0; k < LED; k++) if ((k + 0.5f) / LED <= f + 0.02f) {
                float b0 = a0 + 1 + k * bs;
                dl->PathArcTo(P(CX, CY), (R - 14) * sc, rad(b0 + 0.5f), rad(b0 + bs - 0.5f), 4);
                dl->PathStroke(c, false, 22 * sc);
            }
        }
    }
    // ticks, numerals, redline
    for (int k = 0; k <= 25; k++) {
        const bool major = (k % 5 == 0);
        if (s.tTicks == 0 || (!major && s.tTicks < 2)) continue;
        float a = rad(START + k * SEG / 5.f), r0 = R + 2, r1 = R + (major ? 10 : 5);
        dl->AddLine(P(CX + cosf(a) * r0, CY + sinf(a) * r0), P(CX + cosf(a) * r1, CY + sinf(a) * r1), cTick, (major ? 2.5f : 1.5f) * sc);
    }
    for (int k = 0; k <= 5; k++) {
        float a = rad(START + k * SEG);
        text(dl, o, sc, std::to_string(k), CX + cosf(a) * (R + 22), CY + sinf(a) * (R + 22), 14, cTick, shadow);
    }
    if (s.tRedline) {
        dl->PathArcTo(P(CX, CY), (R + 13) * sc, rad(START + SWEEP * 0.9f), rad(START + SWEEP), 12);
        dl->PathStroke(col3(s.cRedline), false, 4 * sc);
    }

    // needle (+ optional skin image above or below it)
    const float ang = rad(START + st.needle / 5000.f * SWEEP), len = (R - 30) * s.needleLen, th = s.needleThick;
    const ImVec2 dir(cosf(ang), sinf(ang)), pp(-dir.y, dir.x);
    auto pt = [&](float along, float side) { return P(CX + dir.x * along + pp.x * side, CY + dir.y * along + pp.y * side); };
    const ImVec2 tip(CX + dir.x * len, CY + dir.y * len);
    auto needle = [&]() {
        if (s.tNeedle == 3) return;
        const ImU32 nc = col3(s.needleCol);
        dl->AddLine(pt(0, 0), pt(len, 0), col3(s.needleCol, fire ? 0.35f : 0.18f), th * 3.f * sc);   // glow
        if (s.tNeedle == 0) {
            ImVec2 q[4] = {pt(-18, -th), pt(len, -th * .5f), pt(len, th * .5f), pt(-18, th)};
            dl->AddConvexPolyFilled(q, 4, nc);
        } else if (s.tNeedle == 1) dl->AddLine(pt(-18, 0), pt(len, 0), nc, std::max(1.5f, th * 0.6f) * sc);
        else dl->AddTriangleFilled(pt(-12, -th * 1.8f), pt(-12, th * 1.8f), pt(len, 0), nc);
        dl->AddCircleFilled(P(CX, CY), (10 + th * 1.2f) * sc, col3(s.cHub), 24);
        dl->AddCircle(P(CX, CY), (10 + th * 1.2f) * sc, bez, 24, 3 * sc);
    };
    const float imgY = (half ? 235.f : 175.f) + s.imgYOff, imgK = half ? 0.8f : 1.f;
    if (skin && !s.imgAbove) drawSkin(dl, o, sc, s, imgY, imgK);
    needle();
    if (skin && s.imgAbove) drawSkin(dl, o, sc, s, imgY, imgK);

    // names + counts last, so a label stays above the skin image
    for (int i = 0; i < 5; i++) {
        float m = rad(START + i * SEG + SEG / 2), x = CX + cosf(m) * (R - 62), y = CY + sinf(m) * (R - 62);
        if (i < d.n) {
            char c[16]; snprintf(c, sizeof c, d.pct ? "%d%%" : "%d", d.val[i]);
            text(dl, o, sc, shortName(d.name[i]), x, y - 9, 17, d.frac[i] >= 0.999f ? cMaxC : cText, shadow);
            text(dl, o, sc, c, x, y + 12, 20, segColor(s, i, d.frac[i]), shadow);
        } else text(dl, o, sc, "-", x, y, 17, cTick, shadow);
    }
    // readout
    char r[16]; snprintf(r, sizeof r, "%d", (int)std::lround(st.needle));
    const ImU32 ro = all ? cMaxC : col3(s.cReadout);
    if (!half) {
        text(dl, o, sc, r, CX, CY + 88, 34, ro, shadow);
        text(dl, o, sc, "x1000 rpm  -  " + d.label, CX, CY + 112, 13, cTick, shadow);
    } else {
        text(dl, o, sc, r, CX - 70, CY + 24, 24, ro, shadow);
        text(dl, o, sc, "x1000 rpm  -  " + d.label, CX + 50, CY + 24, 12, cTick, shadow);
    }
    return tip;
}

// ---- horizontal bar ----
ImVec2 drawBar(ImDrawList* dl, ImVec2 o, float sc, DialState& st, const DialData& d, const Settings& s, bool all, bool shadow) {
    auto P = [&](float x, float y) { return ImVec2(o.x + x * sc, o.y + y * sc); };
    constexpr float BX0 = 50, BX1 = 590, BY0 = 112, BY1 = 160, SW = (BX1 - BX0) / 5.f;
    const ImU32 cText = col3(s.cText), cTick = col3(s.cTick), cMaxC = col3(s.cMax), bez = col3(s.cBezel);
    dl->AddRectFilled(P(0, 0), P(CW, 225), col3(s.cFace), (s.tShape == 1 ? 34.f : 6.f) * sc);
    dl->AddRect(P(0, 0), P(CW, 225), bez, (s.tShape == 1 ? 34.f : 6.f) * sc, 0, s.tBezelW * sc);
    constexpr int LED = 10;
    for (int i = 0; i < 5; i++) {
        float x0 = BX0 + i * SW + 2, x1 = BX0 + (i + 1) * SW - 2, f = d.frac[i], mid = (x0 + x1) / 2;
        dl->AddRectFilled(P(x0, BY0), P(x1, BY1), col3(s.cTrack), 4 * sc);
        if (f > 0.f) {
            ImU32 c = segColor(s, i, f);
            if (s.tFill == 0) dl->AddRectFilled(P(x0, BY0), P(x0 + (x1 - x0) * f, BY1), c, 4 * sc);
            else {
                float bw = (x1 - x0) / LED;
                for (int k = 0; k < LED; k++) if ((k + 0.5f) / LED <= f + 0.02f) dl->AddRectFilled(P(x0 + k * bw + 1, BY0), P(x0 + (k + 1) * bw - 1, BY1), c, 2 * sc);
            }
        }
        if (i < d.n) {
            char c[16]; snprintf(c, sizeof c, d.pct ? "%d%%" : "%d", d.val[i]);
            text(dl, o, sc, shortName(d.name[i]), mid, 40, 16, d.frac[i] >= 0.999f ? cMaxC : cText, shadow);
            text(dl, o, sc, c, mid, 76, 22, segColor(s, i, f), shadow);
        } else text(dl, o, sc, "-", mid, 58, 16, cTick, shadow);
    }
    for (int k = 0; k <= 25; k++) {
        const bool major = (k % 5 == 0);
        if (s.tTicks == 0 || (!major && s.tTicks < 2)) continue;
        float x = BX0 + k * SW / 5.f;
        dl->AddLine(P(x, BY1 + 4), P(x, BY1 + (major ? 14 : 9)), cTick, (major ? 2.5f : 1.5f) * sc);
    }
    for (int k = 0; k <= 5; k++) text(dl, o, sc, std::to_string(k), BX0 + k * SW, BY1 + 28, 14, cTick, shadow);
    if (s.tRedline) dl->AddRectFilled(P(BX0 + (BX1 - BX0) * 0.9f, BY1 + 4), P(BX1, BY1 + 8), col3(s.cRedline));
    const float xN = BX0 + std::min(st.needle, 5000.f) / 5000.f * (BX1 - BX0), th = s.needleThick;
    const ImU32 nc = col3(s.needleCol);
    if (s.tNeedle != 3) {
        dl->AddLine(P(xN, BY0 - 10), P(xN, BY1 + 10), col3(s.needleCol, 0.25f), th * 3.f * sc);
        if (s.tNeedle == 0) dl->AddRectFilled(P(xN - th * .5f, BY0 - 10), P(xN + th * .5f, BY1 + 10), nc);
        else if (s.tNeedle == 1) dl->AddLine(P(xN, BY0 - 10), P(xN, BY1 + 10), nc, std::max(1.5f, th * 0.6f) * sc);
        else dl->AddTriangleFilled(P(xN - th * 1.8f, BY0 - 16), P(xN + th * 1.8f, BY0 - 16), P(xN, BY1 + 6), nc);
    }
    char r[96]; snprintf(r, sizeof r, "%d   x1000 rpm  -  %s", (int)std::lround(st.needle), d.label.c_str());
    text(dl, o, sc, r, 320, 208, 14, all ? cMaxC : col3(s.cReadout), shadow);
    return ImVec2(xN, BY0 - 10);
}
}  // namespace

ImVec2 GaugeDesignSize(const Settings& s) {
    int n = dialCount(s), cols = n == 1 ? 1 : 2, rows = (n + cols - 1) / cols;
    return ImVec2(cols * CW, rows * cellH(s.tType));
}

void DrawGauge(const std::vector<PartySlot>& slots, const Settings& s, ImDrawList* dl, ImVec2 o, float sc, float dt) {
    auto sel = selectedBoons(s);
    std::vector<DialData> dials;
    if (s.boonMode == 1 && sel.size() > 1) dials.push_back(build(slots, sel));
    else for (size_t i = 0; i < sel.size() && i < 4; i++) dials.push_back(build(slots, {sel[i]}));

    const int n = (int)dials.size(), cols = n == 1 ? 1 : 2;
    const float ch = cellH(s.tType);
    const Geo g = geoFor(s.tType);
    const float lum = 0.3f * s.cFace[0] + 0.59f * s.cFace[1] + 0.11f * s.cFace[2];
    const bool shadow = lum < 0.5f;
    for (int di = 0; di < n; di++) {
        const DialData& d = dials[di];
        ImVec2 co(o.x + (di % cols) * CW * sc, o.y + (di / cols) * ch * sc);
        DialState& st = g_state[di];
        float total = 0.f; bool all = d.n > 0;
        for (int i = 0; i < d.n; i++) { total += d.frac[i] * 1000.f; if (d.frac[i] < 0.999f) all = false; }
        st.needle += (total - st.needle) * std::min(1.f, dt * s.needleSmooth);
        const bool fire = all && s.fireOn;
        ImVec2 tip = (g.type == 2) ? drawBar(dl, co, sc, st, d, s, all, shadow)
                                   : drawRound(dl, co, sc, g, st, d, s, fire, all, di == 0, shadow);
        fireStep(dl, co, sc, g, st, s, fire, tip, dt);
    }
}
