#pragma once
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "boons.h"

constexpr int kMaxTachos = 8;
constexpr int kPresetCount = 7;
extern const char* const kPresetNames[kPresetCount];

// Everything that belongs to ONE tacho (its own window, theme, boons, colours, fire, needle and skin).
struct TachoCfg {
    std::string name = "Tacho";
    bool enabled = true;
    // boons
    int boonMask = 1;     // bit i = kBoons[i] is tracked (bit 0 = Stability)
    int boonMode = 0;     // 0 = one dial per boon (max 4), 1 = one combined dial (average)
    // per-player colours
    bool gradient = false;
    float playerCol[5][3] = {{.29f,.87f,.50f},{.22f,.74f,.97f},{.98f,.75f,.14f},{.96f,.45f,.71f},{.65f,.55f,.98f}};
    // theme
    int themePreset = 0, tType = 0, tShape = 0, tNeedle = 0, tFill = 0, tTicks = 0;
    bool tRedline = false; float tBezelW = 4.f;
    float cFace[3] = {.07f,.07f,.086f}, cFace2[3] = {.11f,.11f,.13f}, cBezel[3] = {.23f,.23f,.26f}, cText[3] = {.90f,.90f,.92f},
          cTick[3] = {.55f,.54f,.58f}, cTrack[3] = {.15f,.15f,.17f}, cHub[3] = {.17f,.17f,.19f}, cReadout[3] = {.95f,.95f,.96f},
          cMax[3] = {1.f,.82f,.48f}, cRedline[3] = {.90f,.15f,.10f};
    // fire
    bool fireOn = true; float fireCol[3] = {1.f,.48f,.10f}; float fireAmt = 9.f;
    // needle
    float needleCol[3] = {1.f,.23f,.19f}; float needleLen = 1.0f, needleThick = 3.f, needleSmooth = 14.f;
    // skin image (file name inside the skins folder, or a full path)
    bool imgOn = true, imgAbove = true; float imgSize = 150, imgOpacity = 1.f, imgYOff = 0.f; std::string skinFile = "skin.png";
    // window
    float winX = 100, winY = 100, winW = 480, winH = 420;
    bool locked = false, clickThrough = false; float opacity = 1.0f;

    // everything that makes up the "look" (shared by theme export/import); p = key prefix
    template <class V> void visitStyle(V& v, const std::string& p) {
        auto k = [&](const char* s) { return p + s; };
        v(k("boonMask").c_str(), boonMask); v(k("boonMode").c_str(), boonMode); v(k("gradient").c_str(), gradient);
        for (int i = 0; i < 5; i++) v(k(("pc" + std::to_string(i)).c_str()).c_str(), playerCol[i]);
        v(k("themePreset").c_str(), themePreset); v(k("tType").c_str(), tType); v(k("tShape").c_str(), tShape); v(k("tNeedle").c_str(), tNeedle);
        v(k("tFill").c_str(), tFill); v(k("tTicks").c_str(), tTicks); v(k("tRedline").c_str(), tRedline); v(k("tBezelW").c_str(), tBezelW);
        v(k("cFace").c_str(), cFace); v(k("cFace2").c_str(), cFace2); v(k("cBezel").c_str(), cBezel); v(k("cText").c_str(), cText);
        v(k("cTick").c_str(), cTick); v(k("cTrack").c_str(), cTrack); v(k("cHub").c_str(), cHub); v(k("cReadout").c_str(), cReadout);
        v(k("cMax").c_str(), cMax); v(k("cRedline").c_str(), cRedline);
        v(k("fireOn").c_str(), fireOn); v(k("fireCol").c_str(), fireCol); v(k("fireAmt").c_str(), fireAmt);
        v(k("needleCol").c_str(), needleCol); v(k("needleLen").c_str(), needleLen); v(k("needleThick").c_str(), needleThick); v(k("needleSmooth").c_str(), needleSmooth);
        v(k("imgOn").c_str(), imgOn); v(k("imgAbove").c_str(), imgAbove); v(k("imgSize").c_str(), imgSize); v(k("imgOpacity").c_str(), imgOpacity); v(k("imgYOff").c_str(), imgYOff);
    }
    template <class V> void visit(V& v, const std::string& p) {
        auto k = [&](const char* s) { return p + s; };
        visitStyle(v, p);
        v(k("name").c_str(), name); v(k("enabled").c_str(), enabled); v(k("skinFile").c_str(), skinFile);
        v(k("winX").c_str(), winX); v(k("winY").c_str(), winY); v(k("winW").c_str(), winW); v(k("winH").c_str(), winH);
        v(k("locked").c_str(), locked); v(k("clickThrough").c_str(), clickThrough); v(k("opacity").c_str(), opacity);
    }
    void applyPreset(int preset);      // theme + needle only; keeps boons, window, skin
    void sanitize();                   // clamp loaded/imported values into valid ranges
    std::string exportStyle() const;   // shareable text (look only: no window position, name or skin path)
    bool importStyle(const std::string& text);
};

struct Settings {
    bool visible = true, showSolo = false, demo = false;
    int rosterMode = 0;                 // 0 = my subgroup, 1 = first 5 players seen
    int timerMode = 0;                  // 0 = events only (no timers), 1 = delay-corrected timers
    float eventDelayMs = 2650.f;        // ArcDPS delays its combat feed ~2.65s; subtracted from intensity-boon timers
    bool debugLog = false, debugVerbose = false;
    std::vector<TachoCfg> tachos = std::vector<TachoCfg>(1);

    template <class V> void visitGlobal(V& v) {
        v("visible", visible); v("showSolo", showSolo); v("demo", demo); v("rosterMode", rosterMode); v("timerMode", timerMode);
        v("eventDelayMs", eventDelayMs); v("debugLog", debugLog); v("debugVerbose", debugVerbose);
    }
    void load();
    void save();
    void reset() { *this = Settings(); }
};
extern Settings cfg;
