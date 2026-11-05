#include "tracker.h"
#include "arcdps.h"
#include "log.h"
#include <algorithm>
#include <cstdlib>

Tracker tracker;
static uint64_t now() { return GetTickCount64(); }
static constexpr size_t kCap = 25;
static bool okBoon(int b) { return b >= 0 && b < BOON_COUNT; }

Tracker::Player* Tracker::find(uint64_t id) {
    for (auto& p : players_) if (p.id == id) return &p;
    return nullptr;
}

void Tracker::agentAdded(uint64_t id, const char* ch, const char* acc, uint8_t sub, bool self) {
    std::lock_guard<std::mutex> l(m_);
    std::string account = acc ? acc : "";
    for (auto& p : players_) if (p.id == id || (!account.empty() && p.account == account)) {
        p.id = id; p.name = ch ? ch : ""; p.subgroup = sub; p.self = self; return;   // re-added (e.g. map change)
    }
    Player np; np.id = id; np.name = ch ? ch : ""; np.account = account; np.subgroup = sub; np.self = self;
    players_.push_back(std::move(np));
}

void Tracker::agentRemoved(uint64_t id) {
    std::lock_guard<std::mutex> l(m_);
    players_.erase(std::remove_if(players_.begin(), players_.end(), [&](const Player& p) { return p.id == id; }), players_.end());
}

void Tracker::setSubgroup(uint64_t id, uint8_t sub) { std::lock_guard<std::mutex> l(m_); if (auto p = find(id)) p->subgroup = sub; }
void Tracker::reset(uint64_t id) { std::lock_guard<std::mutex> l(m_); if (auto p = find(id)) for (auto& v : p->stacks) v.clear(); }

void Tracker::applyStack(uint64_t target, int boon, int32_t dur, uint32_t sid, int32_t delay) {
    if (!okBoon(boon)) return;
    std::lock_guard<std::mutex> l(m_);
    auto p = find(target);
    if (!p) {
        if (++stats_.unmatchedApply <= 200)   // first 200 only, so one bad id cannot flood the log
            Log::write("APPLY unmatched: %s no tracked player with id=%llu (dur=%d stack=%u)", kBoons[boon].name, (unsigned long long)target, dur, sid);
        return;
    }
    int64_t left = (int64_t)dur - delay;
    if (dur <= 0 || left <= 0) return;            // already over by the time ArcDPS told us
    auto& v = p->stacks[boon];
    stats_.applies++;
    uint64_t exp = now() + (uint64_t)left;
    bool found = false;
    if (sid) for (auto& st : v) if (st.id == sid) { st.expiry = exp; found = true; break; }   // BuffInitial + Apply of same stack
    if (!found) {
        if (v.size() < kCap) v.push_back({sid, exp});
        else { auto mn = std::min_element(v.begin(), v.end(), [](const Stack& a, const Stack& b) { return a.expiry < b.expiry; }); if (exp > mn->expiry) *mn = {sid, exp}; }
    }
    Log::write("APPLY ok %s %s id=%llu stack=%u dur=%d(-%d) -> stacks=%zu", kBoons[boon].name, p->name.c_str(), (unsigned long long)target, sid, dur, delay, v.size());
}

void Tracker::changeStack(uint64_t target, int boon, uint32_t sid, int32_t newDur, int32_t delay) {
    if (!okBoon(boon)) return;
    std::lock_guard<std::mutex> l(m_);
    auto p = find(target);
    if (!p) { if (++stats_.unmatchedApply <= 200) Log::write("CHANGE unmatched: %s id=%llu stack=%u", kBoons[boon].name, (unsigned long long)target, sid); return; }
    auto& v = p->stacks[boon];
    int64_t left = (int64_t)newDur - delay;
    stats_.changes++;
    for (auto it = v.begin(); it != v.end(); ++it) if (it->id == sid) {
        if (left <= 0) v.erase(it); else it->expiry = now() + (uint64_t)left;
        Log::write("CHANGE ok %s %s stack=%u newDur=%d -> stacks=%zu", kBoons[boon].name, p->name.c_str(), sid, newDur, v.size());
        return;
    }
    if (left > 0 && v.size() < kCap) v.push_back({sid, now() + (uint64_t)left});   // missed the apply
    Log::write("CHANGE new %s %s stack=%u newDur=%d -> stacks=%zu", kBoons[boon].name, p->name.c_str(), sid, newDur, v.size());
}

void Tracker::removeStackId(uint64_t target, int boon, uint32_t sid) {
    if (!okBoon(boon)) return;
    std::lock_guard<std::mutex> l(m_);
    auto p = find(target);
    if (!p) { if (++stats_.unmatchedRemove <= 200) Log::write("REMOVE unmatched: %s id=%llu stack=%u", kBoons[boon].name, (unsigned long long)target, sid); return; }
    auto& v = p->stacks[boon];
    stats_.removes++;
    size_t before = v.size();
    v.erase(std::remove_if(v.begin(), v.end(), [&](const Stack& s) { return s.id == sid; }), v.end());
    Log::write("REMOVE ok %s %s stack=%u stacks %zu -> %zu", kBoons[boon].name, p->name.c_str(), sid, before, v.size());
}

void Tracker::removeAll(uint64_t target, int boon) {
    if (!okBoon(boon)) return;
    std::lock_guard<std::mutex> l(m_);
    auto p = find(target);
    if (!p) { if (++stats_.unmatchedRemove <= 200) Log::write("REMOVEALL unmatched: %s id=%llu", kBoons[boon].name, (unsigned long long)target); return; }
    stats_.removes++;
    Log::write("REMOVEALL ok %s %s stacks %zu -> 0", kBoons[boon].name, p->name.c_str(), p->stacks[boon].size());
    p->stacks[boon].clear();
}

void Tracker::dropIdless(uint64_t target) {
    std::lock_guard<std::mutex> l(m_);
    if (auto p = find(target)) for (auto& v : p->stacks)
        v.erase(std::remove_if(v.begin(), v.end(), [](const Stack& s) { return s.id == 0; }), v.end());
}

void Tracker::removeStack(uint64_t src, uint64_t dst, int boon, int32_t remaining, uint8_t type) {
    if (!okBoon(boon)) return;
    std::lock_guard<std::mutex> l(m_);
    auto p = find(dst); const char* via = "dst";
    if (!p) { p = find(src); via = "src"; }
    if (!p) {
        if (++stats_.unmatchedRemove <= 200)
            Log::write("REMOVE(legacy) unmatched: %s neither dst=%llu nor src=%llu is tracked (type=%u)", kBoons[boon].name, (unsigned long long)dst, (unsigned long long)src, type);
        return;
    }
    auto& v = p->stacks[boon];
    stats_.removes++;
    size_t before = v.size();
    if (type == CBTB_ALL) v.clear();
    else if (!v.empty()) {
        int64_t want = (int64_t)now() + std::max(remaining, 0);
        auto it = std::min_element(v.begin(), v.end(), [&](const Stack& a, const Stack& b) {
            return std::llabs((int64_t)a.expiry - want) < std::llabs((int64_t)b.expiry - want); });
        v.erase(it);
    }
    Log::write("REMOVE(legacy) ok %s %s via=%s type=%u remaining=%d stacks %zu -> %zu", kBoons[boon].name, p->name.c_str(), via, type, remaining, before, v.size());
}

bool Tracker::known(uint64_t id) { std::lock_guard<std::mutex> l(m_); return find(id) != nullptr; }

std::vector<PartySlot> Tracker::snapshot(int mode, bool eventsOnly) {
    std::lock_guard<std::mutex> l(m_);
    uint64_t t = now();
    // Intensity boons, timer mode: drop at the (delay-corrected) expiry. Events-only: stacks end ONLY on ArcDPS remove events;
    // the 10s net just clears stacks whose remove event never came. Duration-stacking boons queue stacks that do not tick
    // while waiting, so they are always events-only with a long (5 min) net.
    for (auto& p : players_) for (int b = 0; b < BOON_COUNT; b++) {
        const bool inten = kBoons[b].intensity;
        p.stacks[b].erase(std::remove_if(p.stacks[b].begin(), p.stacks[b].end(), [&](const Stack& s) {
            uint64_t grace = !inten ? 300000 : (eventsOnly ? 10000 : (s.id ? 0 : 1000));
            return s.expiry + grace < t; }), p.stacks[b].end());
    }
    const Player* self = nullptr;
    for (auto& p : players_) if (p.self) self = &p;
    std::vector<PartySlot> out;
    auto add = [&](const Player& p) {
        if (out.size() >= 5) return;
        PartySlot s; s.name = p.name;
        for (int b = 0; b < BOON_COUNT; b++) s.stacks[b] = (int)p.stacks[b].size();
        out.push_back(std::move(s));
    };
    if (self) add(*self);
    for (auto& p : players_) {
        if (&p == self) continue;
        if (mode == 0 && self && p.subgroup != self->subgroup) continue;
        add(p);
    }
    return out;
}

void Tracker::debugDump() {
    std::lock_guard<std::mutex> l(m_);
    Log::write("TRACKER players=%zu applies=%llu changes=%llu removes=%llu unmatchedApply=%llu unmatchedRemove=%llu", players_.size(),
        (unsigned long long)stats_.applies, (unsigned long long)stats_.changes, (unsigned long long)stats_.removes,
        (unsigned long long)stats_.unmatchedApply, (unsigned long long)stats_.unmatchedRemove);
    for (auto& p : players_) {
        std::string b;
        for (int i = 0; i < BOON_COUNT; i++) if (!p.stacks[i].empty()) b += std::string(kBoons[i].name) + "=" + std::to_string(p.stacks[i].size()) + " ";
        Log::write("  PLAYER id=%llu name=%s acct=%s subgroup=%u self=%d boons[ %s]", (unsigned long long)p.id, p.name.c_str(), p.account.c_str(), p.subgroup, p.self ? 1 : 0, b.c_str());
    }
}
