#pragma once
// Superpowers and Relics. See GAME_DESIGN.md section 7.
//
// Superpower: ONE slot. Press E / Shift to use it. It is active for `duration` seconds (0 = instant); the cooldown only
// starts once it is over. Swap it for one lying on the ground with F. Mythic superpowers drop from an Arena Guardian,
// the Arena Mage at the heart of a temple, or (rarely) a Legendary Crate.
//
// Relics: TWO slots, mostly passive. The oldest relic is on R, the newer one on T. F next to a relic pickup (with both
// slots full) swaps out the OLDEST relic.
#include <cstdint>

namespace tonk {

enum class Power : uint8_t {
  None, Speed, Shield, Rocket, Homing, RapidShot, AirStrike, Spread, Lightning, Dash, Turret, Teleport, Invisible,
  Singularity, Instinct, SentryBot,  // Mythic
  CocoBarrage, IvySnare,             // admin exclusives: never found on the map or dropped
  Count
};
constexpr int kNormalPowers = 12;                 // kinds 1..12 are found on the map
constexpr int kPowerKinds = (int)Power::Count - 1;
constexpr bool isMythic(Power p) { return p == Power::Singularity || p == Power::Instinct || p == Power::SentryBot; }
constexpr Power kMythics[3] = {Power::Singularity, Power::Instinct, Power::SentryBot};
constexpr bool isExclusive(Power p) { return p == Power::CocoBarrage || p == Power::IvySnare; }

// Rarity is used by stat upgrades (and the shop's cosmetics). Superpowers and relics have none.
enum class Rarity : uint8_t { Common, Uncommon, Rare, Epic, Legendary };

struct PowerInfo { const char* name; float duration; float cooldown; };
// Every cooldown starts only once the superpower is over: when its active time ends, or when the thing it made (rocket,
// air strike, turret, singularity) is gone. Instant powers (Lightning, Teleport) start their cooldown right away.
constexpr PowerInfo powerInfo(Power p) {
  switch (p) {
    case Power::Speed:       return {"Speed Boost", 8.0f, 6.5f};
    case Power::Shield:      return {"Shield", 1.8f, 5.0f};
    case Power::Rocket:      return {"Rocket", 3.4f, 6.0f};          // active while the rocket flies
    case Power::Homing:      return {"Homing Rocket", 5.0f, 7.5f};   // active while the rocket flies
    case Power::RapidShot:   return {"Rapid Shot", 5.5f, 4.5f};
    case Power::AirStrike:   return {"Air Strike", 1.6f, 7.0f};      // active until the last bomb goes off
    case Power::Spread:      return {"Spread Shot", 3.0f, 5.0f};
    case Power::Lightning:   return {"Lightning", 0.0f, 8.0f};
    case Power::Dash:        return {"Dash", 0.16f, 2.3f};
    case Power::Turret:      return {"Turret", 8.0f, 4.5f};          // active while the turret stands
    case Power::Teleport:    return {"Teleport", 0.0f, 7.0f};
    case Power::Invisible:   return {"Invisibility", 4.0f, 6.0f};
    case Power::Singularity: return {"Singularity", 9.0f, 8.0f};     // active while it stands
    case Power::Instinct:    return {"Instinct", 0.0f, 10.0f};       // active until its dodges are used up
    case Power::SentryBot:   return {"Sentry Robot", 60.0f, 12.0f};  // active while the robot is alive
    case Power::CocoBarrage: return {"Coconut Barrage", 0.0f, 6.0f};
    case Power::IvySnare:    return {"Ivy Snare", 0.0f, 7.0f};
    default:                 return {"None", 0.0f, 0.0f};
  }
}
// Superpowers that stay "active" while something they made is still in the world.
constexpr bool powerMakesThing(Power p) {
  return p == Power::Rocket || p == Power::Homing || p == Power::AirStrike || p == Power::Turret || p == Power::Singularity ||
         p == Power::SentryBot;
}

// ---- Superpower tuning ----
constexpr float kSpeedBoostMul = 2.0f;
constexpr float kRapidShotMul = 2.4f, kRapidShotBulletMul = 1.2f;  // 2.4x the fire rate, faster bullets
// Rockets: a hit takes a share of the victim's max health (capped); the splash falls off toward the edge of the blast.
constexpr float kRocketSpeed = 1050.0f, kRocketRange = 3520.0f;  // range = twice the width of the view
constexpr float kRocketBlast = 140.0f, kRocketFrac = 0.6f, kRocketCap = 80.0f;
constexpr float kHomingSpeed = 600.0f, kHomingTurn = 2.8f, kHomingLife = 5.0f, kHomingFrac = 0.65f, kHomingCap = 85.0f;
constexpr int kStrikeBombs = 4;
constexpr float kPlaneSpeed = 2100.0f, kPlaneLead = 700.0f;  // the plane starts this far before the target
constexpr float kBombFuse = 0.3f, kBombBlast = 105.0f, kBombDamage = 28.0f, kBombScatter = 170.0f;
constexpr int kSpreadCount = 5;
constexpr float kSpreadHalfAngle = 0.32f, kSpreadRadiusMul = 1.15f, kSpreadDamageMul = 0.95f;
constexpr int kBolts = 12;
constexpr float kBoltSpeed = 1150.0f, kBoltRange = 430.0f, kBoltDamage = 40.0f, kBoltStun = 1.0f;  // bolts stun what they hit
constexpr float kDashSpeed = 2200.0f;
constexpr float kTurretLife = 8.0f, kTurretReload = 0.6f, kTurretRange = 750.0f;
// Singularity: pulls enemies in and slows them. Touching its core stops them shooting and using superpowers.
constexpr float kSingLife = 9.0f, kSingRadius = 650.0f, kSingPull = 175.0f, kSingTouch = 60.0f;
constexpr float kSingSilence = 1.5f, kSingSlow = 0.55f;
constexpr float kInstinctDodges = 3.0f, kInstinctWindow = 1.5f, kInstinctShrink = 0.15f, kInstinctMinWindow = 0.6f;
constexpr float kInstinctRefund = 1.0f / 3.0f;  // a successful dodge gives back a third of a dodge
// Sentry Robot: a grey copy of you (your upgrades when you deployed it, 25% less health, no relics or superpowers). It
// starts with a Newbie's skill and gets better the longer it survives. It self-destructs when beaten or after 60 s.
constexpr float kBotHealthMul = 0.75f, kBotBlast = 130.0f, kBotBlastDamage = 40.0f;
// Admin exclusives. Coconut Barrage: 10 coconuts rain down in a fan toward the cursor, each a small blast.
// Ivy Snare: vines burst up around the cursor; every enemy caught can't move for 2.5 s and is slowed after.
constexpr int kCocoCount = 10;
constexpr float kCocoSpread = 260.0f, kCocoBlast = 95.0f, kCocoDamage = 24.0f;
constexpr float kIvyRadius = 300.0f, kIvyRoot = 2.5f, kIvySlow = 4.0f;
constexpr float kBotSkillAt[3] = {15.0f, 35.0f, 55.0f};  // seconds alive to become Normal, Intermediate, Pro
constexpr uint32_t kSentryIdBase = 2001;

// ---- Relics ----
enum class Relic : uint8_t { None, Repair, Sentry, Loot, Radar, Count };
constexpr int kRelicKinds = (int)Relic::Count - 1;
constexpr const char* relicName(Relic r) {
  switch (r) {
    case Relic::Repair: return "Repair";
    case Relic::Sentry: return "Sentry Drone";
    case Relic::Loot:   return "Loot Drone";
    case Relic::Radar:  return "Radar";
    default:            return "None";
  }
}
constexpr float kTapTime = 0.3f;  // a press shorter than this is a tap, longer is a hold
// Repair: +1 HP/s after 5 s unharmed. Hold: take +50% damage, move 50% slower, heal 15 HP every 3 s (+ the 1 HP/s).
constexpr float kRepairDelay = 5.0f, kRepairRate = 1.0f, kRepairHoldHeal = 15.0f, kRepairHoldEvery = 3.0f;
constexpr float kRepairHoldDamage = 1.5f, kRepairHoldSlow = 0.5f;
// Sentry Drone: orbits and shoots. Tap toggles it on/off. Hold overloads it: faster fire, chases enemies, but it can be
// shot down (4 hits, or any rocket). A destroyed sentry rebuilds after a while.
constexpr float kSentryOrbit = 75.0f, kSentryReload = 0.9f, kSentryOverReload = 0.35f, kSentryRange = 600.0f;
constexpr float kSentryDamage = 10.0f, kSentryRebuild = 15.0f;
constexpr int kSentryHp = 4;
// Loot Drone: fetches upgrades you can use and drops them next to you. One hit while it's out on a trip destroys it (and
// the relic). Hold 2 s to send it at your cursor to explode.
constexpr float kLootSpeed = 520.0f, kLootRange = 1100.0f, kLootHold = 2.0f, kLootBlast = 130.0f, kLootDamage = 45.0f;
constexpr int kLootHp = 1;

}  // namespace tonk
