#pragma once
#include <fstream>
#include <map>
#include <string>

struct Settings {
    // general
    bool visible = true, showSolo = false, demo = false;
    bool locked = false, clickThrough = false; float opacity = 1.0f;   // window lock / opacity
    int timerMode = 0;                  // 0 = events only (no timers), 1 = delay-corrected timers
    int rosterMode = 0;                 // 0 = my subgroup, 1 = first 5 players seen
    float winX = 100, winY = 100, winW = 480, winH = 375;
    // stability colours
    bool gradient = false;
    float playerCol[5][3] = {{.29f,.87f,.50f},{.22f,.74f,.97f},{.98f,.75f,.14f},{.96f,.45f,.71f},{.65f,.55f,.98f}};
    // fire
    bool fireOn = true; float fireCol[3] = {1.f,.48f,.10f}; float fireAmt = 9.f;
    // needle
    float needleCol[3] = {1.f,.23f,.19f}; float needleLen = 1.0f; float needleThick = 3.f; float needleSmooth = 14.f;   // higher = snappier
    // boons
    int boonMask = 1;     // bit i = kBoons[i] is tracked (bit 0 = Stability)
    int boonMode = 0;     // 0 = one dial per boon (max 4), 1 = one combined dial (average)
    // theme
    int themePreset = 0, tType = 0, tShape = 0, tNeedle = 0, tFill = 0, tTicks = 0;   // see DrawSettings for the labels
    bool tRedline = false; float tBezelW = 4.f;
    float cFace[3] = {.07f,.07f,.086f}, cFace2[3] = {.11f,.11f,.13f}, cBezel[3] = {.23f,.23f,.26f}, cText[3] = {.90f,.90f,.92f},
          cTick[3] = {.55f,.54f,.58f}, cTrack[3] = {.15f,.15f,.17f}, cHub[3] = {.17f,.17f,.19f}, cReadout[3] = {.95f,.95f,.96f},
          cMax[3] = {1.f,.82f,.48f}, cRedline[3] = {.90f,.15f,.10f};
    void applyPreset(int preset);
    // debug
    bool debugLog = false, debugVerbose = false;
    float eventDelayMs = 2650.f;   // ArcDPS delays its combat feed ~2.65s; subtracted from stack durations
    // skin image
    bool imgOn = true, imgAbove = true; float imgSize = 150, imgOpacity = 1.f, imgYOff = 0.f;

    template <class V> void visit(V& v) {
        v("visible", visible); v("locked", locked); v("clickThrough", clickThrough); v("opacity", opacity); v("showSolo", showSolo); v("demo", demo); v("rosterMode", rosterMode); v("timerMode", timerMode);
        v("winX", winX); v("winY", winY); v("winW", winW); v("winH", winH);
        v("gradient", gradient);
        v("boonMask", boonMask); v("boonMode", boonMode);
        v("themePreset", themePreset); v("tType", tType); v("tShape", tShape); v("tNeedle", tNeedle); v("tFill", tFill); v("tTicks", tTicks);
        v("tRedline", tRedline); v("tBezelW", tBezelW);
        v("cFace", cFace); v("cFace2", cFace2); v("cBezel", cBezel); v("cText", cText); v("cTick", cTick); v("cTrack", cTrack);
        v("cHub", cHub); v("cReadout", cReadout); v("cMax", cMax); v("cRedline", cRedline);
        v("pc0", playerCol[0]); v("pc1", playerCol[1]); v("pc2", playerCol[2]); v("pc3", playerCol[3]); v("pc4", playerCol[4]);
        v("fireOn", fireOn); v("fireCol", fireCol); v("fireAmt", fireAmt);
        v("needleCol", needleCol); v("needleLen", needleLen); v("needleThick", needleThick); v("needleSmooth", needleSmooth);
        v("debugLog", debugLog); v("debugVerbose", debugVerbose); v("eventDelayMs", eventDelayMs);
        v("imgOn", imgOn); v("imgAbove", imgAbove); v("imgSize", imgSize); v("imgOpacity", imgOpacity); v("imgYOff", imgYOff);
    }
    void load();
    void save();
    void reset() { *this = Settings(); }
};
constexpr int kPresetCount = 6;
extern const char* const kPresetNames[kPresetCount];
extern Settings cfg;
