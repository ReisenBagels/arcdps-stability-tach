// Stability Tach - ArcDPS plugin. Party boons as one or more tachometers (1000 rpm per player, max 5000).
#include <imgui.h>
#include <algorithm>
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

static bool g_dirty = false;
static void* g_d3d = nullptr; static unsigned g_d3dArg = 0;
static std::atomic<unsigned long long> g_evTotal{0}, g_evBuff{0}, g_evBoon{0};
static std::atomic<unsigned long long> g_scCount[256];
static std::atomic<int> g_boonUnion{1};     // boons tracked by at least one visible tacho (events for others are ignored cheaply)
typedef unsigned long long ull;
static const char* S(const char* s) { return s ? s : "(null)"; }
static bool selected(int boon) { return boon >= 0 && ((g_boonUnion.load() >> boon) & 1); }
static int firstSelected() { for (int i = 0; i < BOON_COUNT; i++) if (selected(i)) return i; return 0; }

// per-tacho runtime state (not saved)
struct TachoRt { float aspect = 560.f / 640.f; bool forceSize = false, forcePos = false; GaugeState gs; };
static std::vector<TachoRt> g_rt;
static int g_sel = 0;                       // tacho being edited in the options UI
static std::string g_note;                  // short feedback line in the options UI
static int g_action = 0;                    // structural edit requested by the UI, applied at a safe point: 1 add, 2 duplicate, 3 remove, 4 reset everything
static void syncRt() { if (g_rt.size() != cfg.tachos.size()) g_rt.resize(cfg.tachos.size()); g_sel = std::min(std::max(g_sel, 0), (int)cfg.tachos.size() - 1); }

static void ApplyPending() {   // never called while a reference into cfg.tachos is held
    if (!g_action) return;
    const int a = g_action; g_action = 0;
    syncRt();
    if (a == 1 || a == 2) {
        if ((int)cfg.tachos.size() >= kMaxTachos) return;
        TachoCfg t = (a == 2) ? cfg.tachos[g_sel] : TachoCfg();
        t.name = (a == 2) ? t.name + " copy" : "Tacho " + std::to_string(cfg.tachos.size() + 1);
        t.winX += 40.f * (float)cfg.tachos.size(); t.winY += 40.f; t.enabled = true;
        cfg.tachos.push_back(t); g_sel = (int)cfg.tachos.size() - 1; syncRt();
        g_rt[g_sel].forcePos = g_rt[g_sel].forceSize = true;
    } else if (a == 3 && cfg.tachos.size() > 1) {
        cfg.tachos.erase(cfg.tachos.begin() + g_sel);
        if (g_sel < (int)g_rt.size()) g_rt.erase(g_rt.begin() + g_sel);
        syncRt();
    } else if (a == 4) {
        cfg.reset(); Log::setEnabled(cfg.debugLog); g_rt.clear(); g_sel = 0; syncRt();
        for (auto& r : g_rt) r.forceSize = r.forcePos = true;
    }
    g_dirty = true;
}

static void LogHeader() {
    Log::write("==== Stability Tach 1.2.0 debug log ==== imgui=%d lastInitArg=%u tachos=%zu rosterMode=%d timerMode=%d showSolo=%d demo=%d verbose=%d delay=%.0f",
               IMGUI_VERSION_NUM, g_d3dArg, cfg.tachos.size(), cfg.rosterMode, cfg.timerMode, cfg.showSolo, cfg.demo, cfg.debugVerbose, cfg.eventDelayMs);
    Log::write("SKIN device: %s", Skin::deviceStatus().c_str());
    for (size_t i = 0; i < cfg.tachos.size(); i++) {
        auto& t = cfg.tachos[i];
        Log::write("TACHO %zu '%s' enabled=%d boonMask=%d type=%d preset=%d skin='%s' img=%d", i, t.name.c_str(), t.enabled, t.boonMask, t.tType, t.themePreset, t.skinFile.c_str(), t.imgOn);
    }
}
static void LogDump() {   // roster as the gauge sees it (first tracked boon) + counters
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
static std::vector<std::string> skinFiles() {   // directory listing, refreshed every 2s instead of every frame
    static std::vector<std::string> files; static double last = -10;
    double t = ImGui::GetTime();
    if (t - last > 2.0) { last = t; files = Skin::list(); }
    return files;
}

static void DrawTachoOptions(TachoCfg& tc, TachoRt& rt, bool& c) {
    if (ImGui::CollapsingHeader("Boons", ImGuiTreeNodeFlags_DefaultOpen)) {
        std::string sum; int cnt = 0;
        for (int i = 0; i < BOON_COUNT; i++) if ((tc.boonMask >> i) & 1) { if (cnt++ < 3) sum += (sum.empty() ? "" : ", ") + std::string(kBoons[i].name); }
        if (cnt > 3) sum += " +" + std::to_string(cnt - 3);
        if (ImGui::BeginCombo("Track", sum.c_str())) {
            for (int i = 0; i < BOON_COUNT; i++) {
                bool on = (tc.boonMask >> i) & 1;
                if (ImGui::Checkbox(kBoons[i].name, &on)) {
                    tc.boonMask = on ? (tc.boonMask | (1 << i)) : (tc.boonMask & ~(1 << i));
                    if (tc.boonMask == 0) tc.boonMask = 1 << i;   // always keep at least one
                    c = true;
                }
            }
            ImGui::EndCombo();
        }
        const char* bm[] = {"Separate dials (max 4)", "Combined dial (average)"};
        c |= ImGui::Combo("Multiple boons", &tc.boonMode, bm, 2);
        ImGui::TextDisabled("Might/Stability show stacks; other boons show on/off");
    }
    if (ImGui::CollapsingHeader("Theme")) {
        int pre = tc.themePreset;
        if (ImGui::Combo("Preset", &pre, kPresetNames, kPresetCount)) { tc.applyPreset(pre); c = true; }
        ImGui::TextDisabled("Car presets are original styles inspired by each cluster");
        const char* types[] = {"Round dial (270)", "Half dial (180)", "Horizontal bar"};
        const char* shapes[] = {"Circle", "Rounded square"};
        const char* needles[] = {"Blade", "Thin line", "Wedge", "None"};
        const char* fills[] = {"Solid", "LED segments"};
        const char* ticks[] = {"None", "Major", "Major + minor"};
        c |= ImGui::Combo("Type", &tc.tType, types, 3);
        c |= ImGui::Combo("Shape", &tc.tShape, shapes, 2);
        c |= ImGui::Combo("Needle style", &tc.tNeedle, needles, 4);
        c |= ImGui::Combo("Fill style", &tc.tFill, fills, 2);
        c |= ImGui::Combo("Tick marks", &tc.tTicks, ticks, 3);
        c |= ImGui::Checkbox("Redline zone", &tc.tRedline);
        c |= ImGui::SliderFloat("Bezel width", &tc.tBezelW, 0.f, 14.f, "%.1f");
        const ImGuiColorEditFlags ni = ImGuiColorEditFlags_NoInputs;
        c |= ImGui::ColorEdit3("Face", tc.cFace, ni);        c |= ImGui::ColorEdit3("Face centre", tc.cFace2, ni);
        c |= ImGui::ColorEdit3("Bezel", tc.cBezel, ni);      c |= ImGui::ColorEdit3("Text", tc.cText, ni);
        c |= ImGui::ColorEdit3("Ticks / numbers", tc.cTick, ni); c |= ImGui::ColorEdit3("Empty track", tc.cTrack, ni);
        c |= ImGui::ColorEdit3("Hub", tc.cHub, ni);          c |= ImGui::ColorEdit3("Readout", tc.cReadout, ni);
        c |= ImGui::ColorEdit3("Maxed highlight", tc.cMax, ni); c |= ImGui::ColorEdit3("Redline colour", tc.cRedline, ni);
    }
    if (ImGui::CollapsingHeader("Player colours")) {
        c |= ImGui::Checkbox("Red-to-green by stacks", &tc.gradient);
        for (int i = 0; i < 5; i++) { char l[16]; snprintf(l, sizeof l, "Player %d", i + 1); c |= ImGui::ColorEdit3(l, tc.playerCol[i], ImGuiColorEditFlags_NoInputs); }
    }
    if (ImGui::CollapsingHeader("Window", ImGuiTreeNodeFlags_DefaultOpen)) {
        c |= ImGui::Checkbox("Lock position and size", &tc.locked);
        c |= ImGui::Checkbox("Click-through (use ArcDPS options to change settings)", &tc.clickThrough);
        if (ImGui::SliderFloat("Width (px)", &tc.winW, 160.f, 1600.f, "%.0f")) { tc.winH = tc.winW * rt.aspect; rt.forceSize = true; c = true; }
        c |= ImGui::SliderFloat("Opacity", &tc.opacity, 0.1f, 1.f, "%.2f");
        if (ImGui::Button("Reset position")) { tc.winX = 100.f; tc.winY = 100.f; rt.forcePos = true; c = true; }
    }
    if (ImGui::CollapsingHeader("Fire")) {
        c |= ImGui::Checkbox("Enable fire", &tc.fireOn);
        c |= ImGui::ColorEdit3("Fire colour", tc.fireCol, ImGuiColorEditFlags_NoInputs);
        c |= ImGui::SliderFloat("Intensity", &tc.fireAmt, 2.f, 20.f, "%.0f");
    }
    if (ImGui::CollapsingHeader("Needle")) {
        c |= ImGui::ColorEdit3("Needle colour", tc.needleCol, ImGuiColorEditFlags_NoInputs);
        c |= ImGui::SliderFloat("Length", &tc.needleLen, 0.6f, 1.25f, "%.2f");
        c |= ImGui::SliderFloat("Thickness", &tc.needleThick, 1.f, 10.f, "%.1f");
        c |= ImGui::SliderFloat("Response", &tc.needleSmooth, 3.f, 40.f, "%.0f");
    }
    if (ImGui::CollapsingHeader("Skin image")) {
        c |= ImGui::Checkbox("Show image", &tc.imgOn);
        if (ImGui::BeginCombo("File", tc.skinFile.empty() ? "(none)" : tc.skinFile.c_str())) {
            if (ImGui::Selectable("(none)", tc.skinFile.empty())) { tc.skinFile.clear(); c = true; }
            for (auto& f : skinFiles()) if (ImGui::Selectable(f.c_str(), f == tc.skinFile)) { tc.skinFile = f; c = true; }
            ImGui::EndCombo();
        }
        char buf[260]; std::snprintf(buf, sizeof buf, "%s", tc.skinFile.c_str());
        if (ImGui::InputText("Name or path", buf, sizeof buf)) { tc.skinFile = buf; c = true; }
        const Skin::Info& si = Skin::get(tc.skinFile);
        ImGui::TextColored(si.ok ? ImVec4(.4f, 1.f, .5f, 1.f) : ImVec4(1.f, .55f, .45f, 1.f), "%s", si.status.c_str());
        if (ImGui::Button("Open skins folder")) { if (!Skin::openFolder()) g_note = "Could not open the folder"; }
        ImGui::SameLine();
        if (ImGui::Button("Reload images")) Skin::reload();
        c |= ImGui::Checkbox("Draw above needle", &tc.imgAbove);
        c |= ImGui::SliderFloat("Image size", &tc.imgSize, 60.f, 280.f, "%.0f");
        c |= ImGui::SliderFloat("Image opacity", &tc.imgOpacity, 0.1f, 1.f, "%.2f");
        c |= ImGui::SliderFloat("Y offset", &tc.imgYOff, -60.f, 120.f, "%.0f");
    }
}

static void DrawSettings() {
    syncRt();
    bool c = false;
    ImGui::TextUnformatted("Stability Tach");
    c |= ImGui::Checkbox("Show all tachos", &cfg.visible);
    c |= ImGui::Checkbox("Show when solo", &cfg.showSolo);
    c |= ImGui::Checkbox("Demo mode (fake data)", &cfg.demo);
    const char* modes[] = {"My subgroup", "First 5 players seen"};
    c |= ImGui::Combo("Roster", &cfg.rosterMode, modes, 2);
    const char* timing[] = {"Events only (no timers)", "Delay-corrected timers"};
    c |= ImGui::Combo("Stack timing", &cfg.timerMode, timing, 2);

    TachoCfg& tc = cfg.tachos[g_sel];
    TachoRt& rt = g_rt[g_sel];
    if (ImGui::CollapsingHeader("Tachos", ImGuiTreeNodeFlags_DefaultOpen)) {
        std::string cur = std::to_string(g_sel + 1) + ". " + tc.name + (tc.enabled ? "" : " (hidden)");
        if (ImGui::BeginCombo("Editing", cur.c_str())) {
            for (int i = 0; i < (int)cfg.tachos.size(); i++) {
                std::string l = std::to_string(i + 1) + ". " + cfg.tachos[i].name + (cfg.tachos[i].enabled ? "" : " (hidden)");
                if (ImGui::Selectable(l.c_str(), i == g_sel)) g_sel = i;
            }
            ImGui::EndCombo();
        }
        const bool room = (int)cfg.tachos.size() < kMaxTachos;
        if (ImGui::Button("Add tacho") && room) g_action = 1;
        ImGui::SameLine(); if (ImGui::Button("Duplicate") && room) g_action = 2;
        ImGui::SameLine(); if (ImGui::Button("Remove") && cfg.tachos.size() > 1) g_action = 3;
        char nb[48]; std::snprintf(nb, sizeof nb, "%s", tc.name.c_str());
        if (ImGui::InputText("Name", nb, sizeof nb)) { tc.name = nb; c = true; }
        c |= ImGui::Checkbox("Visible", &tc.enabled);
        if (ImGui::Button("Lock all")) { for (auto& t : cfg.tachos) t.locked = true; c = true; }
        ImGui::SameLine(); if (ImGui::Button("Unlock all")) { for (auto& t : cfg.tachos) t.locked = false; c = true; }
        ImGui::SameLine();
        if (ImGui::Button("Auto-arrange")) {   // lay the visible tachos out left to right
            float x = 100.f;
            for (size_t i = 0; i < cfg.tachos.size(); i++) if (cfg.tachos[i].enabled) { cfg.tachos[i].winX = x; cfg.tachos[i].winY = 100.f; g_rt[i].forcePos = true; x += cfg.tachos[i].winW + 12.f; }
            c = true;
        }
        if (ImGui::Button("Copy theme")) { ImGui::SetClipboardText(tc.exportStyle().c_str()); g_note = "Theme copied to the clipboard"; }
        ImGui::SameLine();
        if (ImGui::Button("Paste theme")) {
            const char* t = ImGui::GetClipboardText();
            if (t && tc.importStyle(t)) { rt.forceSize = true; c = true; g_note = "Theme pasted"; } else g_note = "Clipboard has no Stability Tach theme";
        }
        if (!g_note.empty()) ImGui::TextDisabled("%s", g_note.c_str());
        ImGui::TextDisabled("Right-click any tacho to edit it; drag to move; each has its own size, opacity and skin");
    }
    DrawTachoOptions(tc, rt, c);

    if (ImGui::CollapsingHeader("Debug")) {
        if (ImGui::Checkbox("Write debug log", &cfg.debugLog)) { Log::setEnabled(cfg.debugLog); if (cfg.debugLog) LogHeader(); c = true; }
        c |= ImGui::Checkbox("Verbose (all buff events on tracked players)", &cfg.debugVerbose);
        c |= ImGui::SliderFloat("Feed delay (ms)", &cfg.eventDelayMs, 0.f, 5000.f, "%.0f");
        ImGui::TextDisabled("Only used by \"Delay-corrected timers\". ArcDPS feed delay is ~2.65s");
        ImGui::TextDisabled("Skins: %s", Skin::deviceStatus().c_str());
        ImGui::TextDisabled("addons/arcdps/stability_tach/debug.log (rolls at 5 MB)");
    }
    if (ImGui::Button("Reset this tacho")) { std::string n = tc.name; float x = tc.winX, y = tc.winY; tc = TachoCfg(); tc.name = n; tc.winX = x; tc.winY = y; rt.forceSize = rt.forcePos = true; c = true; }
    ImGui::SameLine();
    if (ImGui::Button("Reset everything")) g_action = 4;

    g_dirty |= c;
}

// ---------------------------------------------------------------- windows (render thread)
static void keepAspect(ImGuiSizeCallbackData* d) { d->DesiredSize.y = d->DesiredSize.x * (*static_cast<float*>(d->UserData)); }

// Scales the alpha of every vertex the gauge drew (ImGui draw-list colours are absolute, so style alpha does not apply).
static void ApplyOpacity(ImDrawList* dl, int from, float a) {
    if (a >= 0.999f) return;
    for (int i = from; i < dl->VtxBuffer.Size; i++) {
        ImU32 c = dl->VtxBuffer[i].col;
        ImU32 al = (ImU32)(((c >> IM_COL32_A_SHIFT) & 0xFF) * a);
        dl->VtxBuffer[i].col = (c & ~IM_COL32_A_MASK) | (al << IM_COL32_A_SHIFT);
    }
}

static void DrawTacho(int i, const std::vector<PartySlot>& slots) {
    TachoCfg& tc = cfg.tachos[i];
    TachoRt& rt = g_rt[i];
    const ImVec2 design = GaugeDesignSize(tc);                 // layout depends on theme type + number of dials
    const float aspect = design.y / design.x;
    if (std::fabs(aspect - rt.aspect) > 1e-3f) { rt.aspect = aspect; rt.forceSize = true; }   // refit the window to the new layout

    ImGui::SetNextWindowPos(ImVec2(tc.winX, tc.winY), rt.forcePos ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(tc.winW, tc.winW * rt.aspect), rt.forceSize ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
    rt.forcePos = rt.forceSize = false;   // applied this frame; if a slider changes them again below it re-arms for the next
    ImGui::SetNextWindowSizeConstraints(ImVec2(160, 80), ImVec2(1600, 1600), keepAspect, &rt.aspect);
    ImGuiWindowFlags fl = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings;
    if (tc.locked) fl |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
    if (tc.clickThrough) fl |= ImGuiWindowFlags_NoMouseInputs;
    char id[40]; std::snprintf(id, sizeof id, "##StabTach%d", i);
    if (ImGui::Begin(id, nullptr, fl)) {
        ImVec2 p = ImGui::GetWindowPos(), sz = ImGui::GetWindowSize();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const int vtx0 = dl->VtxBuffer.Size;
        DrawGauge(slots, tc, dl, p, sz.x / design.x, ImGui::GetIO().DeltaTime, rt.gs);
        ApplyOpacity(dl, vtx0, tc.opacity);
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(1)) g_sel = i;            // right-click edits THIS tacho
        if (ImGui::BeginPopupContextWindow("stab_ctx")) { DrawSettings(); ImGui::EndPopup(); }
        if (!rt.forceSize && !rt.forcePos && (p.x != tc.winX || p.y != tc.winY || sz.x != tc.winW || sz.y != tc.winH)) {   // skip while a slider is driving them
            tc.winX = p.x; tc.winY = p.y; tc.winW = sz.x; tc.winH = sz.y; g_dirty = true;
        }
    }
    ImGui::End();
}

static uintptr_t mod_imgui(uint32_t not_charsel_or_loading, uint32_t hide_if_combat_or_ooc) {
    Skin::beginFrame();
    ApplyPending();                          // add / duplicate / remove / reset requested last frame
    if (!not_charsel_or_loading) return 0;
    syncRt();
    int u = 0; for (auto& t : cfg.tachos) if (t.enabled) u |= t.boonMask;
    g_boonUnion = u ? u : 1;
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
    for (int i = 0; i < (int)cfg.tachos.size(); i++) if (cfg.tachos[i].enabled) DrawTacho(i, slots);
    if (g_dirty && !ImGui::IsMouseDown(0)) { cfg.save(); g_dirty = false; }
    return 0;
}

static uintptr_t mod_options_end() { DrawSettings(); ApplyPending(); return 0; }
static uintptr_t mod_options_windows(const char* name) {
    if (!name) { if (ImGui::Checkbox("Stability Tach", &cfg.visible)) g_dirty = true; }
    return 0;
}

// ---------------------------------------------------------------- arcdps plugin plumbing
static arcdps_exports g_exports;

static arcdps_exports* mod_init() {
    cfg.load();
    syncRt();
    Log::setEnabled(cfg.debugLog);
    Skin::init(g_d3d);           // works out the D3D11 device itself; images are loaded lazily when first drawn
    if (cfg.debugLog) LogHeader();
    g_exports.size = sizeof(arcdps_exports);
    g_exports.sig = 0x57AB1117;  // unique plugin id
    g_exports.imguivers = IMGUI_VERSION_NUM;
    g_exports.out_name = "Stability Tach";
    g_exports.out_build = "1.2.0";
    g_exports.combat = (void*)mod_combat;
    g_exports.imgui = (void*)mod_imgui;
    g_exports.options_end = (void*)mod_options_end;
    g_exports.options_windows = (void*)mod_options_windows;
    return &g_exports;
}

static uintptr_t mod_release() { Log::write("==== shutdown ===="); cfg.save(); Skin::shutdown(); Log::setEnabled(false); return 0; }

extern "C" __declspec(dllexport) void* get_init_addr(char*, ImGuiContext* ctx, void* id3d, HANDLE, void* mallocfn, void* freefn, uint32_t lastArg) {
    ImGui::SetCurrentContext(ctx);
    ImGui::SetAllocatorFunctions((void* (*)(size_t, void*))mallocfn, (void (*)(void*, void*))freefn);
    g_d3d = id3d; g_d3dArg = lastArg;   // lastArg is logged only: current ArcDPS passes something other than the D3D version
    return (void*)mod_init;
}
extern "C" __declspec(dllexport) void* get_release_addr() { return (void*)mod_release; }
