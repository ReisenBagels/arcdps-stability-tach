// Stability Tach - ArcDPS plugin. Party boons as a tachometer (1000 rpm per player, max 5000).
#include <imgui.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include "arcdps.h"
#include "gauge.h"
#include "log.h"
#include "settings.h"
#include "skin.h"
#include "tracker.h"

static const char* kSkinPath = "addons/arcdps/stability_tach/skin.png";
static bool g_dirty = false, g_forceSize = false, g_forcePos = false;   // force* set by the options UI, applied next frame
static float g_aspect = 500.f / 640.f;                                    // window height / width for the current layout
static void* g_d3d = nullptr; static unsigned g_d3dver = 0;
static std::atomic<unsigned long long> g_evTotal{0}, g_evBuff{0}, g_evBoon{0};
static std::atomic<unsigned long long> g_scCount[256];
typedef unsigned long long ull;
static const char* S(const char* s) { return s ? s : "(null)"; }
static bool selected(int boon) { return boon >= 0 && ((cfg.boonMask >> boon) & 1); }
static int firstSelected() { for (int i = 0; i < BOON_COUNT; i++) if (selected(i)) return i; return 0; }

static void LogHeader() {
    Log::write("==== Stability Tach 1.1.0 debug log ==== imgui=%d d3dver=%u rosterMode=%d timerMode=%d boonMask=%d boonMode=%d showSolo=%d demo=%d verbose=%d delay=%.0f",
               IMGUI_VERSION_NUM, g_d3dver, cfg.rosterMode, cfg.timerMode, cfg.boonMask, cfg.boonMode, cfg.showSolo, cfg.demo, cfg.debugVerbose, cfg.eventDelayMs);
}
static void LogDump() {   // roster as the gauge sees it (first selected boon) + counters
    auto slots = tracker.snapshot(cfg.rosterMode, cfg.timerMode == 0);
    int b = firstSelected();
    std::string r;
    for (auto& x : slots) r += x.name + ":" + std::to_string(x.stacks[b]) + " ";
    Log::write("ROSTER boon=%s mode=%d slots=%zu [ %s] events=%llu buffEvents=%llu boonEvents=%llu", kBoons[b].name, cfg.rosterMode, slots.size(), r.c_str(),
               (ull)g_evTotal.load(), (ull)g_evBuff.load(), (ull)g_evBoon.load());
    std::string sc;   // how many of each statechange type ArcDPS has sent us (spots encoding changes)
    for (int i = 1; i < 256; i++) if (g_scCount[i]) sc += std::to_string(i) + "=" + std::to_string(g_scCount[i].load()) + " ";
    Log::write("STATECHANGES %s", sc.c_str());
}

// ---------------------------------------------------------------- combat events (arcdps thread)
static uintptr_t mod_combat(cbtevent* ev, ag* src, ag* dst, const char* skillname, uint64_t, uint64_t) {
    const bool dbg = Log::enabled();
    if (!ev) {
        // Agent added/removed: src->prof != 0 => added, == 0 => removed; dst->name is the account name (':' prefix),
        // dst->team is the subgroup (src->team is the team).
        const bool player = src && dst && src->elite == 0 && dst->name && dst->name[0] == ':';
        if (dbg && src && dst) {
            static int other = 0;   // non-player agents: only the first 100, they are mostly enemies/gadgets
            if (player || other++ < 100)
                Log::write("AGENT %s src{name=%s id=%llu prof=%u elite=%u self=%u team=%u} dst{name=%s id=%llu prof=%u elite=%u self=%u team=%u}",
                    player ? "player" : "other", S(src->name), (ull)src->id, src->prof, src->elite, src->self, src->team,
                    S(dst->name), (ull)dst->id, dst->prof, dst->elite, dst->self, dst->team);
        }
        if (player) {
            if (src->prof != 0) tracker.agentAdded(src->id, src->name, dst->name, (uint8_t)dst->team, dst->self != 0);
            else tracker.agentRemoved(src->id);
        }
        return 0;
    }
    g_evTotal++;
    const int bi = boonIndex(ev->skillid);
    // Intensity boons honour timer mode; duration-stacking boons are always events-only (their queued stacks do not tick).
    const int32_t delay = (bi >= 0 && kBoons[bi].intensity && cfg.timerMode == 1) ? (int32_t)cfg.eventDelayMs : 0;
    if (ev->is_statechange) {
        g_scCount[ev->is_statechange]++;
        if (dbg) {
            if (bi >= 0 && selected(bi))   // every statechange that carries a tracked boon's skill id, whatever its number
                Log::write("BOON-SC %s sc=%u src=%llu dst=%llu value=%d buff_dmg=%d overstack=%u stack=%u remove=%u srcKnown=%d dstKnown=%d",
                    kBoons[bi].name, ev->is_statechange, (ull)ev->src_agent, (ull)ev->dst_agent, ev->value, ev->buff_dmg, ev->overstack_value, stackIdOf(ev),
                    ev->is_buffremove, tracker.known(ev->src_agent) ? 1 : 0, tracker.known(ev->dst_agent) ? 1 : 0);
            else if ((ev->is_statechange == CBTS_ENTERCOMBAT || ev->is_statechange == CBTS_EXITCOMBAT || ev->is_statechange == CBTS_CHANGEDEAD) &&
                     tracker.known(ev->src_agent))
                Log::write("STATE sc=%u src=%llu dst=%llu value=%d", ev->is_statechange, (ull)ev->src_agent, (ull)ev->dst_agent, ev->value);
        }
        switch (ev->is_statechange) {
            case CBTS_ENTERCOMBAT: tracker.setSubgroup(ev->src_agent, (uint8_t)ev->dst_agent); tracker.dropIdless(ev->src_agent); break;   // initial snapshots follow
            case CBTS_CHANGEDEAD: tracker.reset(ev->src_agent); break;
            case CBTS_BUFFINITIAL:    // snapshot of stacks already present; dst has the buff
            case CBTS_BUFFAPPLY:      // new stack: dst has the buff
                if (bi >= 0) { g_evBoon++; tracker.applyStack(ev->dst_agent, bi, ev->value, stackIdOf(ev), delay); }
                break;
            case CBTS_BUFFCHANGE:     // stack duration changed: dst = agent, overstack_value = new duration
                if (bi >= 0) { g_evBoon++; tracker.changeStack(ev->dst_agent, bi, stackIdOf(ev), (int32_t)ev->overstack_value, delay); }
                break;
            case CBTS_BUFFREMOVE_SINGLE:   // src = agent that lost the stack
                if (bi >= 0) { g_evBoon++; tracker.removeStackId(ev->src_agent, bi, stackIdOf(ev)); }
                break;
            case CBTS_BUFFREMOVE_ALL:
                if (bi >= 0) { g_evBoon++; tracker.removeAll(ev->src_agent, bi); }
                break;
        }
        return 0;
    }
    const bool isBuff = ev->buff || ev->is_buffremove;
    if (isBuff) g_evBuff++;
    if (dbg) {
        // Log events of tracked boons, plus any skill named like Stability (to catch a changed id). Verbose: every buff event
        // that touches a tracked player.
        const bool relevant = selected(bi) || (skillname && std::strstr(skillname, "tability"));
        const bool verbose = cfg.debugVerbose && isBuff && (tracker.known(ev->dst_agent) || tracker.known(ev->src_agent));
        if (relevant || verbose)
            Log::write("BUFF skill=%u '%s' time=%llu src=%llu dst=%llu value=%d buff=%u buff_dmg=%d remove=%u overstack=%u result=%u offcycle=%u srcKnown=%d dstKnown=%d",
                ev->skillid, S(skillname), (ull)ev->time, (ull)ev->src_agent, (ull)ev->dst_agent, ev->value, ev->buff, ev->buff_dmg,
                ev->is_buffremove, ev->overstack_value, ev->result, ev->is_offcycle,
                tracker.known(ev->src_agent) ? 1 : 0, tracker.known(ev->dst_agent) ? 1 : 0);
    }
    if (bi < 0) return 0;
    g_evBoon++;
    if (ev->is_buffremove) tracker.removeStack(ev->src_agent, ev->dst_agent, bi, ev->value, ev->is_buffremove);
    else if (ev->buff && ev->buff_dmg == 0 && ev->value > 0) tracker.applyStack(ev->dst_agent, bi, ev->value, 0, delay);
    return 0;
}

// ---------------------------------------------------------------- settings UI
static void DrawSettings() {
    bool c = false;
    ImGui::TextUnformatted("Stability Tach");
    c |= ImGui::Checkbox("Show window", &cfg.visible);
    c |= ImGui::Checkbox("Show when solo", &cfg.showSolo);
    c |= ImGui::Checkbox("Demo mode (fake data)", &cfg.demo);
    const char* modes[] = {"My subgroup", "First 5 players seen"};
    c |= ImGui::Combo("Roster", &cfg.rosterMode, modes, 2);
    const char* timing[] = {"Events only (no timers)", "Delay-corrected timers"};
    c |= ImGui::Combo("Stack timing", &cfg.timerMode, timing, 2);

    if (ImGui::CollapsingHeader("Boons", ImGuiTreeNodeFlags_DefaultOpen)) {
        std::string sum; int cnt = 0;
        for (int i = 0; i < BOON_COUNT; i++) if (selected(i)) { if (cnt++ < 3) sum += (sum.empty() ? "" : ", ") + std::string(kBoons[i].name); }
        if (cnt > 3) sum += " +" + std::to_string(cnt - 3);
        if (ImGui::BeginCombo("Track", sum.c_str())) {
            for (int i = 0; i < BOON_COUNT; i++) {
                bool on = selected(i);
                if (ImGui::Checkbox(kBoons[i].name, &on)) {
                    cfg.boonMask = on ? (cfg.boonMask | (1 << i)) : (cfg.boonMask & ~(1 << i));
                    if (cfg.boonMask == 0) cfg.boonMask = 1 << i;   // always keep at least one
                    c = true;
                }
            }
            ImGui::EndCombo();
        }
        const char* bm[] = {"Separate dials (max 4)", "Combined dial (average)"};
        c |= ImGui::Combo("Multiple boons", &cfg.boonMode, bm, 2);
        ImGui::TextDisabled("Might/Stability show stacks; other boons show on/off");
    }
    if (ImGui::CollapsingHeader("Theme")) {
        int pre = cfg.themePreset;
        if (ImGui::Combo("Preset", &pre, kPresetNames, kPresetCount)) { cfg.applyPreset(pre); c = true; }
        ImGui::TextDisabled("Car presets are original styles inspired by each cluster");
        const char* types[] = {"Round dial (270)", "Half dial (180)", "Horizontal bar"};
        const char* shapes[] = {"Circle", "Rounded square"};
        const char* needles[] = {"Blade", "Thin line", "Wedge", "None"};
        const char* fills[] = {"Solid", "LED segments"};
        const char* ticks[] = {"None", "Major", "Major + minor"};
        c |= ImGui::Combo("Type", &cfg.tType, types, 3);
        c |= ImGui::Combo("Shape", &cfg.tShape, shapes, 2);
        c |= ImGui::Combo("Needle style", &cfg.tNeedle, needles, 4);
        c |= ImGui::Combo("Fill style", &cfg.tFill, fills, 2);
        c |= ImGui::Combo("Tick marks", &cfg.tTicks, ticks, 3);
        c |= ImGui::Checkbox("Redline zone", &cfg.tRedline);
        c |= ImGui::SliderFloat("Bezel width", &cfg.tBezelW, 0.f, 14.f, "%.1f");
        const ImGuiColorEditFlags ni = ImGuiColorEditFlags_NoInputs;
        c |= ImGui::ColorEdit3("Face", cfg.cFace, ni);        c |= ImGui::ColorEdit3("Face centre", cfg.cFace2, ni);
        c |= ImGui::ColorEdit3("Bezel", cfg.cBezel, ni);      c |= ImGui::ColorEdit3("Text", cfg.cText, ni);
        c |= ImGui::ColorEdit3("Ticks / numbers", cfg.cTick, ni); c |= ImGui::ColorEdit3("Empty track", cfg.cTrack, ni);
        c |= ImGui::ColorEdit3("Hub", cfg.cHub, ni);          c |= ImGui::ColorEdit3("Readout", cfg.cReadout, ni);
        c |= ImGui::ColorEdit3("Maxed highlight", cfg.cMax, ni); c |= ImGui::ColorEdit3("Redline colour", cfg.cRedline, ni);
    }
    if (ImGui::CollapsingHeader("Stability colours")) {
        c |= ImGui::Checkbox("Red-to-green by stacks", &cfg.gradient);
        for (int i = 0; i < 5; i++) { char l[16]; snprintf(l, sizeof l, "Player %d", i + 1); c |= ImGui::ColorEdit3(l, cfg.playerCol[i], ImGuiColorEditFlags_NoInputs); }
    }
    if (ImGui::CollapsingHeader("Window", ImGuiTreeNodeFlags_DefaultOpen)) {
        c |= ImGui::Checkbox("Lock position and size", &cfg.locked);
        c |= ImGui::Checkbox("Click-through (use ArcDPS options to change settings)", &cfg.clickThrough);
        if (ImGui::SliderFloat("Width (px)", &cfg.winW, 160.f, 1600.f, "%.0f")) { cfg.winH = cfg.winW * g_aspect; g_forceSize = true; c = true; }
        c |= ImGui::SliderFloat("Opacity", &cfg.opacity, 0.1f, 1.f, "%.2f");
        if (ImGui::Button("Reset position")) { cfg.winX = 100.f; cfg.winY = 100.f; g_forcePos = true; c = true; }
    }
    if (ImGui::CollapsingHeader("Fire")) {
        c |= ImGui::Checkbox("Enable fire", &cfg.fireOn);
        c |= ImGui::ColorEdit3("Fire colour", cfg.fireCol, ImGuiColorEditFlags_NoInputs);
        c |= ImGui::SliderFloat("Intensity", &cfg.fireAmt, 2.f, 20.f, "%.0f");
    }
    if (ImGui::CollapsingHeader("Needle")) {
        c |= ImGui::ColorEdit3("Needle colour", cfg.needleCol, ImGuiColorEditFlags_NoInputs);
        c |= ImGui::SliderFloat("Length", &cfg.needleLen, 0.6f, 1.25f, "%.2f");
        c |= ImGui::SliderFloat("Thickness", &cfg.needleThick, 1.f, 10.f, "%.1f");
        c |= ImGui::SliderFloat("Response", &cfg.needleSmooth, 3.f, 40.f, "%.0f");
    }
    if (ImGui::CollapsingHeader("Skin image")) {
        c |= ImGui::Checkbox("Show image", &cfg.imgOn);
        c |= ImGui::Checkbox("Draw above needle", &cfg.imgAbove);
        c |= ImGui::SliderFloat("Image size", &cfg.imgSize, 60.f, 280.f, "%.0f");
        c |= ImGui::SliderFloat("Opacity##img", &cfg.imgOpacity, 0.1f, 1.f, "%.2f");
        c |= ImGui::SliderFloat("Y offset", &cfg.imgYOff, -60.f, 120.f, "%.0f");
        if (ImGui::Button("Reload skin file")) Skin::load(kSkinPath);
        ImGui::TextDisabled("%s", kSkinPath);
    }
    if (ImGui::CollapsingHeader("Debug")) {
        if (ImGui::Checkbox("Write debug log", &cfg.debugLog)) { Log::setEnabled(cfg.debugLog); if (cfg.debugLog) LogHeader(); c = true; }
        c |= ImGui::Checkbox("Verbose (all buff events on tracked players)", &cfg.debugVerbose);
        c |= ImGui::SliderFloat("Feed delay (ms)", &cfg.eventDelayMs, 0.f, 5000.f, "%.0f");
        ImGui::TextDisabled("Only used by \"Delay-corrected timers\". ArcDPS feed delay is ~2.65s");
        ImGui::TextDisabled("addons/arcdps/stability_tach/debug.log");
        ImGui::TextDisabled("Rolls at 5 MB to debug.log.1 (max ~10 MB total)");
    }
    if (ImGui::Button("Restore defaults")) { cfg.reset(); Log::setEnabled(cfg.debugLog); g_forceSize = g_forcePos = true; c = true; }
    g_dirty |= c;
}

// ---------------------------------------------------------------- window (render thread)
static void keepAspect(ImGuiSizeCallbackData* d) { d->DesiredSize.y = d->DesiredSize.x * g_aspect; }

// Scales the alpha of every vertex the gauge drew (ImGui draw-list colours are absolute, so style alpha does not apply).
static void ApplyOpacity(ImDrawList* dl, int from, float a) {
    if (a >= 0.999f) return;
    for (int i = from; i < dl->VtxBuffer.Size; i++) {
        ImU32 c = dl->VtxBuffer[i].col;
        ImU32 al = (ImU32)(((c >> IM_COL32_A_SHIFT) & 0xFF) * a);
        dl->VtxBuffer[i].col = (c & ~IM_COL32_A_MASK) | (al << IM_COL32_A_SHIFT);
    }
}

static uintptr_t mod_imgui(uint32_t not_charsel_or_loading, uint32_t hide_if_combat_or_ooc) {
    if (!not_charsel_or_loading) return 0;
    if (Log::enabled()) {   // every 2s: roster + counters; every 10s: full player table
        static double last = 0; static int n = 0;
        double t = ImGui::GetTime();
        if (t - last >= 2.0) { last = t; LogDump(); if (n++ % 5 == 0) tracker.debugDump(); }
    }
    if (hide_if_combat_or_ooc || !cfg.visible) return 0;

    std::vector<PartySlot> slots;
    if (cfg.demo) {
        static const char* nm[5] = {"You", "Aria", "Bolt", "Cinder", "Dax"};
        float t = (float)ImGui::GetTime();
        for (int i = 0; i < 5; i++) {
            PartySlot s; s.name = nm[i];
            float f = 0.5f + 0.5f * sinf(t * .6f + i * 1.1f) + (sinf(t * .15f) > .8f ? 1.f : 0.f);
            for (int b = 0; b < BOON_COUNT; b++) s.stacks[b] = (int)std::lround(std::min(1.f, f * (0.7f + 0.05f * b)) * kBoons[b].maxShown);
            slots.push_back(s);
        }
    } else {
        slots = tracker.snapshot(cfg.rosterMode, cfg.timerMode == 0);
        if (slots.size() < 2 && !cfg.showSolo) return 0;      // only usable in a party / squad
    }

    const ImVec2 design = GaugeDesignSize(cfg);               // layout depends on theme type + number of dials
    const float aspect = design.y / design.x;
    if (std::fabs(aspect - g_aspect) > 1e-3f) { g_aspect = aspect; g_forceSize = true; }   // refit the window to the new layout

    ImGui::SetNextWindowPos(ImVec2(cfg.winX, cfg.winY), g_forcePos ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(cfg.winW, cfg.winW * g_aspect), g_forceSize ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
    g_forcePos = g_forceSize = false;   // applied this frame; if a slider changes them again below it re-arms for the next
    ImGui::SetNextWindowSizeConstraints(ImVec2(160, 80), ImVec2(1600, 1600), keepAspect);
    ImGuiWindowFlags fl = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings;
    if (cfg.locked) fl |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
    if (cfg.clickThrough) fl |= ImGuiWindowFlags_NoMouseInputs;
    if (ImGui::Begin("##StabilityTach", nullptr, fl)) {
        ImVec2 p = ImGui::GetWindowPos(), sz = ImGui::GetWindowSize();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const int vtx0 = dl->VtxBuffer.Size;
        DrawGauge(slots, cfg, dl, p, sz.x / design.x, ImGui::GetIO().DeltaTime);
        ApplyOpacity(dl, vtx0, cfg.opacity);
        if (ImGui::BeginPopupContextWindow("stab_ctx")) { DrawSettings(); ImGui::EndPopup(); }   // right-click for options
        if (!g_forceSize && !g_forcePos && (p.x != cfg.winX || p.y != cfg.winY || sz.x != cfg.winW || sz.y != cfg.winH)) {   // skip while a slider is driving them
            cfg.winX = p.x; cfg.winY = p.y; cfg.winW = sz.x; cfg.winH = sz.y; g_dirty = true;
        }
    }
    ImGui::End();
    if (g_dirty && !ImGui::IsMouseDown(0)) { cfg.save(); g_dirty = false; }
    return 0;
}

static uintptr_t mod_options_end() { DrawSettings(); return 0; }
static uintptr_t mod_options_windows(const char* name) {
    if (!name) { if (ImGui::Checkbox("Stability Tach", &cfg.visible)) g_dirty = true; }
    return 0;
}

// ---------------------------------------------------------------- arcdps plugin plumbing
static arcdps_exports g_exports;

static arcdps_exports* mod_init() {
    cfg.load();
    Log::setEnabled(cfg.debugLog);
    if (cfg.debugLog) LogHeader();
    Skin::init(g_d3d, g_d3dver);
    Skin::load(kSkinPath);       // optional: drop your PNG here to get the skin
    g_exports.size = sizeof(arcdps_exports);
    g_exports.sig = 0x57AB1117;  // unique plugin id
    g_exports.imguivers = IMGUI_VERSION_NUM;
    g_exports.out_name = "Stability Tach";
    g_exports.out_build = "1.1.0";
    g_exports.combat = (void*)mod_combat;
    g_exports.imgui = (void*)mod_imgui;
    g_exports.options_end = (void*)mod_options_end;
    g_exports.options_windows = (void*)mod_options_windows;
    return &g_exports;
}

static uintptr_t mod_release() { Log::write("==== shutdown ===="); cfg.save(); Skin::shutdown(); Log::setEnabled(false); return 0; }

extern "C" __declspec(dllexport) void* get_init_addr(char*, ImGuiContext* ctx, void* id3d, HANDLE, void* mallocfn, void* freefn, uint32_t d3dversion) {
    ImGui::SetCurrentContext(ctx);
    ImGui::SetAllocatorFunctions((void* (*)(size_t, void*))mallocfn, (void (*)(void*, void*))freefn);
    g_d3d = id3d; g_d3dver = d3dversion;
    return (void*)mod_init;
}
extern "C" __declspec(dllexport) void* get_release_addr() { return (void*)mod_release; }
