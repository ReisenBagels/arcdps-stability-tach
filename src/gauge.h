#pragma once
#include <imgui.h>
#include <vector>
#include "settings.h"
#include "tracker.h"

// Size of the whole gauge area in "design" units for the current theme type and boon selection
// (one 640-wide cell per dial; up to 4 dials in a 2-column grid). The window keeps this aspect ratio.
ImVec2 GaugeDesignSize(const Settings& s);
// Draws all dials with the top-left at `origin`, scaled by `scale` (window width / design width).
void DrawGauge(const std::vector<PartySlot>& slots, const Settings& s, ImDrawList* dl, ImVec2 origin, float scale, float dt);
