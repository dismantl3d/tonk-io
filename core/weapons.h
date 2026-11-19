#pragma once
// Weapon archetypes and variants. See GAME_DESIGN.md section 6.
// RULE: every weapon and variant has exactly kStatBudget points. No weapon is a strict upgrade.
#include <array>
#include <cstdint>

namespace tonk {

constexpr int kStatBudget = 25;

struct WeaponStats {
  int dmg, rof, rng, pspd, hdl;  // each 1..10
  constexpr int total() const { return dmg + rof + rng + pspd + hdl; }
};

enum class Archetype : uint8_t { Standard, Sniper, Twin, Cannon, Scatter, DroneCaller, Trapper, Rocket, Count };

constexpr std::array<WeaponStats, (size_t)Archetype::Count> kBaseWeapons = {{
    {5, 5, 5, 5, 5},  // Standard
    {8, 2, 8, 5, 2},  // Sniper
    {3, 9, 4, 5, 4},  // Twin
    {8, 3, 5, 3, 6},  // Cannon
    {6, 5, 2, 5, 7},  // Scatter
    {4, 4, 7, 3, 7},  // Drone Caller
    {5, 3, 4, 4, 9},  // Trapper
    {7, 2, 7, 3, 6},  // Rocket
}};

// Moves `points` from one stat to another (a sidegrade). Total is preserved.
constexpr WeaponStats sidegrade(WeaponStats w, int WeaponStats::*up, int WeaponStats::*down, int points = 2) {
  w.*up += points;
  w.*down -= points;
  return w;
}

constexpr bool valid(const WeaponStats& w) {
  for (int v : {w.dmg, w.rof, w.rng, w.pspd, w.hdl})
    if (v < 1 || v > 10) return false;
  return w.total() == kStatBudget;
}

constexpr bool allBaseWeaponsValid() {
  for (const auto& w : kBaseWeapons)
    if (!valid(w)) return false;
  return true;
}
static_assert(allBaseWeaponsValid(), "Every base weapon must total exactly 25 points with stats in 1..10");

// Example variant: Standard "Longshot" = RNG +2, ROF -2
constexpr WeaponStats kStandardLongshot =
    sidegrade(kBaseWeapons[(size_t)Archetype::Standard], &WeaponStats::rng, &WeaponStats::rof);
static_assert(valid(kStandardLongshot), "Variants must keep the 25-point budget");

}  // namespace tonk
