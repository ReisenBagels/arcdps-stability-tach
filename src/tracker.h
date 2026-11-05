#pragma once
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include "boons.h"

struct PartySlot { std::string name; int stacks[BOON_COUNT] = {}; };

class Tracker {
public:
    void agentAdded(uint64_t id, const char* character, const char* account, uint8_t subgroup, bool self);
    void agentRemoved(uint64_t id);
    void setSubgroup(uint64_t id, uint8_t subgroup);
    void reset(uint64_t id);   // all boons (e.g. on death)
    // Stack-id based (statechange buff encoding). delayMs = ArcDPS feed delay, removed from the duration (intensity boons, timer mode).
    void applyStack(uint64_t target, int boon, int32_t durationMs, uint32_t stackId = 0, int32_t delayMs = 0);
    void changeStack(uint64_t target, int boon, uint32_t stackId, int32_t newDurationMs, int32_t delayMs);
    void removeStackId(uint64_t target, int boon, uint32_t stackId);
    void removeAll(uint64_t target, int boon);
    void dropIdless(uint64_t target);   // forget stacks that came without a stack id (initial snapshots re-sent on combat entry)
    // Legacy encoding (is_buffremove on a normal event); no stack id, so the closest-expiry stack is dropped.
    void removeStack(uint64_t src, uint64_t dst, int boon, int32_t remainingMs, uint8_t type);
    // Party members (self first), max 5. rosterMode: 0 = my subgroup, 1 = first 5 seen.
    std::vector<PartySlot> snapshot(int rosterMode, bool eventsOnly = false);
    bool known(uint64_t id);
    void debugDump();
private:
    struct Stack { uint32_t id; uint64_t expiry; };
    struct Player { uint64_t id; std::string name, account; uint8_t subgroup; bool self; std::vector<Stack> stacks[BOON_COUNT]; };
    Player* find(uint64_t id);
    struct Stats { uint64_t applies = 0, changes = 0, removes = 0, unmatchedApply = 0, unmatchedRemove = 0; } stats_;
    std::mutex m_;
    std::vector<Player> players_;
};
extern Tracker tracker;
