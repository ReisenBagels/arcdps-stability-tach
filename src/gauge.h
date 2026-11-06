#pragma once
#include <imgui.h>
#include <vector>
#include "settings.h"
#include "tracker.h"

// Animation state of one tacho (needle position + fire particles for each of its up-to-4 dials).
struct Particle { float x, y, vx, vy, l, s; };
struct DialState { float needle = 0.f, acc = 0.f; std::vector<Particle> parts; };
struct GaugeState { DialState dial[4]; };

// Size of the whole gauge area in "design" units for this tacho's theme type and boon selection
// (one 640-wide cell per dial; up to 4 dials in a 2-column grid). The window keeps this aspect ratio.
ImVec2 GaugeDesignSize(const TachoCfg& s);
// Draws all dials of one tacho with the top-left at `origin`, scaled by `scale` (window width / design width).
void DrawGauge(const std::vector<PartySlot>& slots, const TachoCfg& s, ImDrawList* dl, ImVec2 origin, float scale, float dt, GaugeState& gs);
