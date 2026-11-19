#pragma once
// Closing Ring phases. See GAME_DESIGN.md section 8.
#include <array>

namespace tonk {

struct RingPhase {
  float startSec;     // when the phase begins
  float endRadiusPct; // ring radius at end of shrink, % of full
  float shrinkSec;
  float dmgPerSec;    // damage outside the ring
};

constexpr std::array<RingPhase, 6> kRingPhases = {{
    {90, 80, 30, 2},
    {180, 55, 30, 4},
    {270, 35, 25, 7},
    {360, 20, 25, 10},
    {450, 8, 20, 15},
    {510, 0, 40, 25},
}};

}  // namespace tonk
