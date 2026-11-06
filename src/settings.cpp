#include "settings.h"
#include <algorithm>
#include <sstream>

Settings cfg;
static const char* kPath = "addons/arcdps/arcdps_stability_tach.ini";

namespace {
struct Writer {
    std::ostream& o;
    void operator()(const char* k, bool& v) { o << k << '=' << (v ? 1 : 0) << '\n'; }
    void operator()(const char* k, int& v) { o << k << '=' << v << '\n'; }
    void operator()(const char* k, float& v) { o << k << '=' << v << '\n'; }
    void operator()(const char* k, float (&v)[3]) { o << k << '=' << v[0] << ',' << v[1] << ',' << v[2] << '\n'; }
    void operator()(const char* k, std::string& v) { o << k << '=' << v << '\n'; }
};
struct Reader {
    std::map<std::string, std::string>& m;
    const std::string* get(const char* k) { auto it = m.find(k); return it == m.end() ? nullptr : &it->second; }
    void operator()(const char* k, bool& v) { if (auto s = get(k)) v = (*s == "1"); }
    void operator()(const char* k, int& v) { if (auto s = get(k)) try { v = std::stoi(*s); } catch (...) {} }
    void operator()(const char* k, float& v) { if (auto s = get(k)) try { v = std::stof(*s); } catch (...) {} }
    void operator()(const char* k, float (&v)[3]) {
        if (auto s = get(k)) {
            float t[3]; char c1, c2;
            std::istringstream is(*s);
            if (is >> t[0] >> c1 >> t[1] >> c2 >> t[2]) for (int i = 0; i < 3; i++) v[i] = t[i];
        }
    }
    void operator()(const char* k, std::string& v) { if (auto s = get(k)) v = *s; }
};
void parseLines(std::istream& in, std::map<std::string, std::string>& m) {
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto eq = line.find('=');
        if (eq != std::string::npos) m[line.substr(0, eq)] = line.substr(eq + 1);
    }
}
}

void TachoCfg::sanitize() {
    auto clampi = [](int& v, int lo, int hi) { v = std::min(hi, std::max(lo, v)); };
    auto clampf = [](float& v, float lo, float hi) { v = std::min(hi, std::max(lo, v)); };
    clampi(tType, 0, 2); clampi(tShape, 0, 1); clampi(tNeedle, 0, 3); clampi(tFill, 0, 1); clampi(tTicks, 0, 2);
    clampi(themePreset, 0, kPresetCount - 1); clampi(boonMode, 0, 1);
    boonMask &= (1 << BOON_COUNT) - 1; if (boonMask == 0) boonMask = 1;
    clampf(tBezelW, 0.f, 14.f); clampf(needleLen, 0.6f, 1.25f); clampf(needleThick, 1.f, 10.f); clampf(needleSmooth, 3.f, 40.f);
    clampf(fireAmt, 2.f, 20.f); clampf(imgSize, 60.f, 280.f); clampf(imgOpacity, 0.1f, 1.f); clampf(imgYOff, -60.f, 120.f);
    clampf(winW, 160.f, 1600.f); clampf(opacity, 0.1f, 1.f);
    if (name.empty()) name = "Tacho";
}

std::string TachoCfg::exportStyle() const {
    TachoCfg c = *this; std::ostringstream o; o << "STT1\n"; Writer w{o}; c.visitStyle(w, ""); return o.str();
}
bool TachoCfg::importStyle(const std::string& text) {
    if (text.find("STT1") == std::string::npos) return false;
    std::string t = text; std::replace(t.begin(), t.end(), ';', '\n');
    std::istringstream in(t); std::map<std::string, std::string> m; parseLines(in, m);
    if (m.empty()) return false;
    Reader r{m}; visitStyle(r, ""); sanitize(); return true;
}

void Settings::load() {
    std::ifstream f(kPath);
    if (!f) return;
    std::map<std::string, std::string> m; parseLines(f, m);
    Reader r{m}; visitGlobal(r);
    int n = 0;
    if (auto it = m.find("tachoCount"); it != m.end()) try { n = std::stoi(it->second); } catch (...) {}
    if (n <= 0) { tachos.assign(1, TachoCfg()); tachos[0].visit(r, ""); }   // pre-1.2 ini: one tacho stored under flat keys
    else {
        n = std::min(n, kMaxTachos); tachos.assign(n, TachoCfg());
        for (int i = 0; i < n; i++) tachos[i].visit(r, "t" + std::to_string(i) + ".");
    }
    for (auto& t : tachos) t.sanitize();
}

void Settings::save() {
    std::ofstream f(kPath, std::ios::trunc);
    if (!f) return;
    Writer w{f};
    visitGlobal(w);
    f << "tachoCount=" << tachos.size() << '\n';
    for (size_t i = 0; i < tachos.size(); i++) tachos[i].visit(w, "t" + std::to_string(i) + ".");
}

const char* const kPresetNames[kPresetCount] = {
    "Default (dark)", "Toyota Altezza (inspired)", "Toyota Starlet (inspired)", "Lexus LFA (inspired)", "Half dial", "Shift bar",
    "GM Squarebody pickup (inspired)"};

// Presets are original stylings *inspired by* each cluster (face colour, needle, ticks, redline) - not copies of any artwork.
void TachoCfg::applyPreset(int p) {
    auto set = [](float (&d)[3], float r, float g, float b) { d[0] = r; d[1] = g; d[2] = b; };
    themePreset = p;
    // baseline = the default look
    tType = 0; tShape = 0; tNeedle = 0; tFill = 0; tTicks = 0; tRedline = false; tBezelW = 4.f;
    set(cFace, .07f, .07f, .086f); set(cFace2, .11f, .11f, .13f); set(cBezel, .23f, .23f, .26f); set(cText, .90f, .90f, .92f);
    set(cTick, .55f, .54f, .58f); set(cTrack, .15f, .15f, .17f); set(cHub, .17f, .17f, .19f); set(cReadout, .95f, .95f, .96f);
    set(cMax, 1.f, .82f, .48f); set(cRedline, .90f, .15f, .10f);
    set(needleCol, 1.f, .23f, .19f); needleThick = 3.f; needleLen = 1.f;
    switch (p) {
    case 1:   // white-faced cluster, chrome bezel, red needle
        tTicks = 2; tRedline = true; tBezelW = 9.f; needleThick = 2.5f;
        set(cFace, .95f, .95f, .93f); set(cFace2, 1.f, 1.f, 1.f); set(cBezel, .72f, .74f, .78f); set(cText, .08f, .08f, .09f);
        set(cTick, .15f, .15f, .17f); set(cTrack, .82f, .83f, .85f); set(cHub, .70f, .72f, .76f); set(cReadout, .08f, .08f, .09f);
        set(cMax, .80f, .10f, .10f); set(needleCol, .85f, .08f, .08f); set(cRedline, .85f, .10f, .10f); break;
    case 2:   // black face, white markings, amber readout, orange wedge needle
        tTicks = 2; tRedline = true; tNeedle = 2; tBezelW = 7.f; needleThick = 3.f;
        set(cFace, .03f, .03f, .035f); set(cFace2, .09f, .09f, .10f); set(cBezel, .28f, .28f, .30f); set(cText, .95f, .95f, .95f);
        set(cTick, .95f, .95f, .95f); set(cTrack, .10f, .10f, .11f); set(cHub, .45f, .45f, .48f); set(cReadout, 1.f, .60f, .15f);
        set(cMax, 1.f, .62f, .20f); set(needleCol, 1.f, .35f, .08f); break;
    case 3:   // near-black digital look: thin bezel, LED segments, thin amber needle
        tTicks = 2; tRedline = true; tNeedle = 1; tFill = 1; tBezelW = 2.f; needleThick = 2.f;
        set(cFace, .02f, .02f, .025f); set(cFace2, .06f, .06f, .07f); set(cBezel, .45f, .45f, .50f); set(cText, .92f, .92f, .95f);
        set(cTick, .60f, .60f, .65f); set(cTrack, .07f, .07f, .08f); set(cHub, .12f, .12f, .14f); set(cReadout, 1.f, 1.f, 1.f);
        set(cMax, 1.f, .55f, .10f); set(needleCol, 1.f, .60f, .10f); set(cRedline, .95f, .10f, .05f); break;
    case 4: tType = 1; tTicks = 1; break;   // half dial
    case 5:   // horizontal shift-light bar
        tType = 2; tShape = 1; tFill = 1; tTicks = 1; tRedline = true; tNeedle = 1; set(needleCol, 1.f, 1.f, 1.f); break;
    case 6:   // 1970s-80s GM truck cluster: black face in a rounded-square housing, cream markings, orange-red needle
        tShape = 1; tTicks = 2; tRedline = true; tBezelW = 6.f; needleThick = 3.f; needleLen = 0.95f;
        set(cFace, .04f, .04f, .045f); set(cFace2, .10f, .10f, .11f); set(cBezel, .55f, .55f, .58f); set(cText, .93f, .90f, .80f);
        set(cTick, .93f, .90f, .80f); set(cTrack, .12f, .12f, .13f); set(cHub, .35f, .35f, .37f); set(cReadout, .95f, .90f, .75f);
        set(cMax, 1.f, .55f, .15f); set(needleCol, 1.f, .28f, .08f); set(cRedline, .85f, .12f, .08f); break;
    default: break;
    }
    sanitize();
}
