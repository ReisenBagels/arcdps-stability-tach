#include "settings.h"
#include <sstream>

Settings cfg;
static const char* kPath = "addons/arcdps/arcdps_stability_tach.ini";

namespace {
struct Writer {
    std::ofstream& o;
    void operator()(const char* k, bool& v) { o << k << '=' << (v ? 1 : 0) << '\n'; }
    void operator()(const char* k, int& v) { o << k << '=' << v << '\n'; }
    void operator()(const char* k, float& v) { o << k << '=' << v << '\n'; }
    void operator()(const char* k, float (&v)[3]) { o << k << '=' << v[0] << ',' << v[1] << ',' << v[2] << '\n'; }
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
};
}

void Settings::load() {
    std::ifstream f(kPath);
    if (!f) return;
    std::map<std::string, std::string> m; std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq != std::string::npos) m[line.substr(0, eq)] = line.substr(eq + 1);
    }
    Reader r{m}; visit(r);
}

void Settings::save() {
    std::ofstream f(kPath, std::ios::trunc);
    if (!f) return;
    Writer w{f}; visit(w);
}

const char* const kPresetNames[kPresetCount] = {
    "Default (dark)", "Toyota Altezza (inspired)", "Toyota Starlet (inspired)", "Lexus LFA (inspired)", "Half dial", "Shift bar"};

// Presets are original stylings *inspired by* each cluster (face colour, needle, ticks, redline) - not copies of any artwork.
void Settings::applyPreset(int p) {
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
    default: break;
    }
}
