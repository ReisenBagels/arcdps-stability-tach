#pragma once
// Minimal ArcDPS plugin API definitions (see the ArcDPS "mod template" for the originals).
#include <cstdint>
#include <Windows.h>

struct ag {
    char* name; uintptr_t id; uint32_t prof; uint32_t elite; uint32_t self; uint16_t team;
};

struct cbtevent {
    uint64_t time, src_agent, dst_agent;
    int32_t value, buff_dmg;
    uint32_t overstack_value, skillid;
    uint16_t src_instid, dst_instid, src_master_instid, dst_master_instid;
    uint8_t iff, buff, result, is_activation, is_buffremove, is_ninety, is_fifty, is_moving,
            is_statechange, is_flanking, is_shields, is_offcycle, pad61, pad62, pad63, pad64;
};

struct arcdps_exports {
    uintptr_t size; uint32_t sig; uint32_t imguivers;
    const char* out_name; const char* out_build;
    void* wnd_nofilter; void* combat; void* imgui; void* options_end;
    void* combat_local; void* wnd_filter; void* options_windows;
};

enum { CBTS_ENTERCOMBAT = 1, CBTS_EXITCOMBAT = 2, CBTS_CHANGEDEAD = 4, CBTS_BUFFINITIAL = 18,
       CBTS_STACKACTIVE = 27, CBTS_STACKRESET = 28,
       // newer ArcDPS encodes buff events as statechanges:
       CBTS_BUFFAPPLY = 69,         // src=applier, dst=target, value=duration applied (ms), pad61..64 = stack id
       CBTS_BUFFCHANGE = 70,        // dst=agent, value=duration diff, overstack_value=new duration, pad61..64 = stack id
       CBTS_BUFFREMOVE_SINGLE = 71, // src=agent that LOST the stack, dst=remover, pad61..64 = stack id
       CBTS_BUFFREMOVE_ALL = 72 };  // src=agent that lost all stacks, dst=remover
enum { CBTB_ALL = 1, CBTB_SINGLE = 2, CBTB_MANUAL = 3 };
constexpr uint32_t STABILITY_ID = 1122;
constexpr int STABILITY_MAX = 25;

#include <cstring>
// pad61..pad64 hold the trackable buff stack (instance) id as a u32 in the statechange buff encoding
inline uint32_t stackIdOf(const cbtevent* ev) { uint32_t id; std::memcpy(&id, &ev->pad61, sizeof id); return id; }
