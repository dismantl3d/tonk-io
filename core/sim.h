#pragma once
// Minimal deterministic simulation shared by client (prediction) and server (authority).
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "core/powerups.h"
#include "core/vec2.h"
#include "core/weapons.h"

namespace tonk {

constexpr float kTickHz = 30.0f;
constexpr float kDt = 1.0f / kTickHz;
constexpr float kMapSize = 14000.0f;
constexpr float kTankRadius = 28.0f;
constexpr float kTankSpeed = 340.0f;  // units/sec (~40s to cross the map)
constexpr float kMaxHealth = 100.0f;
constexpr float kMaxHealthInit = 100.0f;  // for default member initializers declared before use
constexpr float kTurnRate = 10.0f;    // max aim rotation, rad/sec
constexpr float kPi = 3.14159265f;
constexpr float kViewHalfW = 880.0f, kViewHalfH = 495.0f;  // what a player sees (client kViewW x kViewH / 2)
// Movement has momentum: the tank speeds up quickly and, with no keys held, slides a little before it stops.
constexpr float kAccel = 11.0f;    // per second, how fast velocity reaches the wanted velocity while steering
constexpr float kFriction = 6.0f;  // per second, how fast it bleeds off with no input (lower = longer skid)

// ---- Stat upgrades ----
// Each stat has a tier 0..5. Picking up an upgrade of rarity r sets that stat to tier r+1 (Common 1 ... Legendary 5);
// upgrades at or below your current tier are useless to you and are left on the ground.
// Field of View only exists in Rocket Rampage (where only Move Speed and Field of View upgrades drop).
enum Stat { StatSpeed, StatBulletSpeed, StatRange, StatHealth, StatFireRate, StatDamage, StatFov, kStatCount };
constexpr int kRoyaleStats = 6;  // Battle Royale drops the first six
constexpr int kMaxPips = 5;
constexpr float statSpeedMul(int p) { return 1.0f + 0.06f * p; }        // +6% move speed per tier
constexpr float statBulletSpeedMul(int p) { return 1.0f + 0.08f * p; }  // +8% bullet speed per tier
constexpr float statRangeMul(int p) { return 1.0f + 0.08f * p; }        // +8% bullet range per tier
constexpr float statHealthBonus(int p) { return 12.0f * p; }            // +12 max health per tier
constexpr float statReloadMul(int p) { return 1.0f / (1.0f + 0.08f * p); }  // +8% fire rate per tier
constexpr float statDamageMul(int p) { return 1.0f + 0.07f * p; }           // +7% bullet damage per tier
constexpr float statFovMul(int p) { return 1.0f + 0.08f * p; }              // +8% view size per tier

// What a client sends each tick. The server never trusts anything else.
struct Input {
  int8_t moveX = 0;  // -1..1
  int8_t moveY = 0;  // -1..1
  float aim = 0;     // radians
  bool shoot = false;
  bool usePowerUp = false;  // E / Shift: use the superpower (or dodge, with Instinct)
  float speed = 1.0f;       // 0..1 throttle (humans are always 1; bots ease off so they are not twitchy)
  float turn = 1.0f;        // 0..1 barrel turn-rate scale
  bool swap = false;        // F: swap the superpower / oldest relic with a pickup next to us
  float tx = 0, ty = 0;     // world point under the cursor (rockets, air strikes, teleport, singularity, loot drone)
  bool relicDown[2] = {false, false};  // R (oldest relic) / T held
};

// Bot brain state (see core/bot.h). Bots produce Inputs only; the world decides outcomes.
enum class BotState : uint8_t { Roam, Loot, Farm, Engage, Retreat, Zone };
// How good a bot is: aim, reactions, dodging, and how well it judges a situation (GAME_DESIGN.md section 7.8).
// The first six play in matches (rarer the better they are); Unreal, Omniscient and Omnipotent are test bots that only
// the dev panel makes.
enum class BotTier : uint8_t { Newbie, Rookie, Intermediate, Skilled, Pro, Elite, Unreal, Omniscient, Omnipotent, Count };
constexpr int kMatchTiers = 6;
// What a bot is trying to do in a match.
enum class BotStyle : uint8_t {
  Looter,     // gears up first, fights once it has enough
  Brawler,    // fights whatever gets in its way
  Tactician,  // fights only with a clear advantage, backs off early
  Survivor,   // avoids enemies, plays the zone
  Hunter,     // picks a target and chases it down
  Raider,     // goes for the Arena Temples
  Count
};
// What a bot is doing right now (its objective), shown by the dev panel's Bot Info Overlay.
enum class BotGoal : uint8_t { Roam, Loot, Crate, Attack, Contest, Hunt, Retreat, Zone, Raid, Recover, Guard, Follow, Count };

// Preset bots (core/bot_presets.h): which hand-designed brain drives a bot (0 = the normal tiered brain).
enum class Brain : uint8_t { Standard, Aegis, Ghost, Carrion, Count };
// What a preset brain is doing right now (each preset maps its own named states onto these).
enum class PState : uint8_t { Roam, Loot, Fight, Shift, Finish, Hold, Stalk, Evade, Zone };
// What a preset bot remembers about one tank it has seen: observed positions only (never the true velocity).
struct PTrack {
  uint32_t id = 0;
  Vec2 h[8];                 // last observed positions (ring buffer)
  float ts[8] = {};          // ...and when
  int head = 0, n = 0;
  Vec2 last, vel;            // newest observed position; velocity estimated from the observations
  float conf = 1.0f;         // how far to trust that estimate
  float seenT = -99, firstT = -99;
  float firedT = -99;        // when a bullet of theirs was last seen appear
  Vec2 firedDir;             // ...and its direction
};
// A bullet or rocket a preset bot has noticed: judged once, after a reaction delay.
struct PBul {
  uint32_t id = 0;
  float seenT = 0, react = 0;
  bool judged = false, live = false;
};

struct BotMind {
  bool seeded = false;
  uint8_t brain = 0;         // Brain: 0 normal; 1 Aegis-Vulture, 2 Ghost Current, 3 Carrion Clock
  PState ps = PState::Roam;
  PTrack trk[10];
  PBul pbul[24];
  float psTime = 0, shiftT = 0, engageT = 0, noFoeT = 99, noBulletT = 99, holdT = 0, acquireT = 0, flipT = 0, prevCd = 0;
  float hitT[4] = {-99, -99, -99, -99};  // when it was last hurt (ring buffer)
  int hitN = 0, burstN = 0, bounceShots = 0, unstuckN = 0, powerTick = 0;
  float bounceT = 0, lastThreatT = -99, threatT2 = -99, bulSpd = 0, loseT = 0, hpPrev = 0;
  Vec2 holdSpot, coverPos, roamGoal;
  float coverT = 0, roamT = 0;
  bool coverOk = false;
  uint32_t finishId = 0;
  BotTier tier = BotTier::Rookie;
  BotStyle style = BotStyle::Brawler;
  float aggression = 1.0f;   // the aggression meter: 0 peaceful .. 1 neutral .. 2 picks fights for no reason (shown x50 %)
  BotGoal objective = BotGoal::Roam;
  // Affinities: how much it likes looting, fighting and raiding temples (from its style, aggression and a little luck).
  float affLoot = 1.0f, affFight = 1.0f, affRaid = 0.0f;
  bool custom = false;       // made in the dev panel
  float skill = 0.5f;        // 0..1 summary of the tier (shown in the dev panel)
  // Per-tier abilities (set when seeded).
  float aimNoise = 0.1f;     // radians of drifting aim error
  float lead = 0.5f;         // how much of the target's motion it leads (0 none .. 1 perfect)
  float trackTau = 0.35f;    // seconds to notice a change in the target's direction
  float reactMin = 0.4f, reactMax = 0.7f;  // delay before shooting a newly seen target
  float dodgeChance = 0.3f;  // chance to notice and dodge a bullet heading at it
  float iq = 0.5f;           // decision quality: 0 crude thresholds .. 1 weighs everything
  float thinkEvery = 0.3f;   // seconds between decisions
  float turn = 0.4f;         // barrel turn speed (0..1)
  // Decision state.
  BotState state = BotState::Roam;
  uint32_t targetId = 0;    // enemy tank we chose to fight
  uint32_t lastFireId = 0;  // enemy we were last aiming at (to restart the reaction delay on a new one)
  Vec2 goal;               // where the bot is heading (roam/loot/farm/zone)
  float decideTimer = 0;    // time until the next decision
  float stateTime = 0;
  float commitTimer = 0;    // minimum time to stay in Engage/Retreat before flipping (no jitter)
  Vec2 estVel;              // what it thinks the target's velocity is (lags behind the real one)
  uint32_t estFor = 0;
  Vec2 lastSeen;            // where the target was last seen (Hunters chase there)
  float lostTimer = 0;      // seconds since the target was last seen
  Vec2 grabPos;             // loot worth grabbing on the way (during a fight or retreat)
  bool grab = false;
  // Combat feel.
  float strafeSign = 1, strafeTimer = 0;
  float aimErr = 0, aimErrTarget = 0;  // slowly drifting aim error (radians)
  float react = 0;                      // reaction delay before shooting a newly spotted target
  float burst = 1.0f;                   // > 0 firing, counts down past 0 into a short pause
  float dodgeTimer = 0, dodgeCooldown = 0;
  Vec2 dodgeDir;
  float lastHealth = kMaxHealthInit;
  float hurtTimer = 0;      // seconds since last damage
  uint32_t hurtBy = 0;      // who hit us last (if we could tell)
  float instinctWait = -1;  // Instinct: reaction delay before pressing dodge
  // Anti-stuck.
  Vec2 stuckPos;
  float stuckTimer = 0;
  float unstickTimer = 0;
  Vec2 unstickDir;
  // Crates, temples, Sentry Robots.
  uint32_t crateId = 0;     // the loot crate it is going to shoot open
  bool templeFan = false;   // likes to raid Arena Temples
  int wp = 0;               // inside a temple: the next waypoint along the spiral
  float age = 0;            // Sentry Robot: seconds alive (it gets better with time)
  // Smooth steering (no twitching): the move direction eases toward what it wants, and a key stays down until the
  // direction has clearly changed. Against a wall it slides along it, keeping the same side.
  Vec2 steer;
  int8_t keyX = 0, keyY = 0;
  float slideSign = 0, slideT = 0;
  Vec2 skipPos;             // loot it couldn't reach: ignored for a while
  float skipT = 0;
  Vec2 detour;              // a wall between it and where it's going: a clear spot to go round by first
  float detourT = 0;
  Vec2 lootGoal;            // the loot it's going for, and how long since it last got closer to it
  float lootBest = 1e9f, lootT = 0;
  // Dodging like a person: a bullet it notices is judged once; if it decides to dodge, it starts after its reaction
  // time (too late if the bullet is already there), and lower tiers sometimes step the wrong way.
  uint32_t judged[6] = {};
  int judgedN = 0;
  float dodgePlanT = -1;
  Vec2 dodgePlanDir;
  float dodgeReact = 0.4f;
  Vec2 contestPos;          // Contest: the loot it is fighting for, and who it is fighting for it
  uint32_t contestId = 0;
  bool recovering = false;  // low on health with nobody around: patching up
  float teleT = 0, blindCd = 0, pullCd = 0;  // Omnipotent: teleport, blind and pull cooldowns
  uint32_t forcedTarget = 0;  // the red-lined tank (dev panel) it's going after right now, whatever else is going on
  float basePower = 1.0f;   // Omnipotent / dev bots: stat multiplier when not fighting
};

struct Tank {
  uint32_t id = 0;
  Vec2 pos;
  Vec2 vel;                    // velocity (has momentum); public information
  float radius = kTankRadius;
  uint32_t place = 0;          // finishing place once eliminated (1 = winner)
  uint32_t killedBy = 0;       // tank id that finished us (0 = the storm)
  float diedAt = 0;            // match time of death
  BotMind mind;
  float aim = 0;
  float health = kMaxHealth;
  Archetype weapon = Archetype::Standard;
  float cooldown = 0;           // seconds until the next shot
  bool alive = true;
  uint32_t xp = 0;              // in-match XP
  uint32_t kills = 0;
  uint32_t shapesDestroyed = 0; // feeds the post-match XP/Credit formula (GAME_DESIGN.md section 10)
  uint8_t stat[kStatCount] = {};  // upgrade tiers
  float maxHealth = kMaxHealth;
  float unharmed = 99;          // seconds since we last took damage
  Vec2 cursor;                  // last cursor point (homing rockets follow it)

  // Superpower: one slot.
  Power held = Power::None;
  Power active = Power::None;
  float activeLeft = 0;
  float recharge = 0;           // cooldown left (0 = ready)
  bool swapReq = false;
  float swapCd = 0;
  Vec2 dashDir;
  float revealT = 0;            // Invisibility: seconds the position is given away after shooting
  Vec2 lastShotPos;             // ...and where that shot came from
  float slowT = 0;              // inside a Singularity's pull
  float silenceT = 0;           // touched a Singularity: can't shoot or use superpowers
  float stunT = 0;              // hit by Lightning: can't move, shoot or use superpowers
  uint32_t lastHitBy = 0;       // who damaged us last
  // Instinct.
  float dodges = 0, dodgeWindow = 0;
  float pendingDmg = 0, pendingT = 0;  // a hit waiting for a dodge
  uint32_t pendingBy = 0;

  // Relics: relic[0] is the oldest (R), relic[1] the newer (T).
  Relic relic[2] = {Relic::None, Relic::None};
  float relicHold[2] = {0, 0};
  bool relicPrev[2] = {false, false};
  bool sentryOn = true;
  bool sentryOver = false;      // holding the Sentry key: overloaded
  float sentryRebuild = 0;
  float repairAcc = 0;
  bool repairing = false;

  // Arena Guardians (the big ones in the middle, and the Arena Mage at the heart of a temple).
  bool guardian = false;
  bool mob = false;             // a small Temple Guardian
  uint32_t sentryOf = 0;        // a Sentry Robot: the tank that deployed it
  // Arena Temples: which one we are in (-1 = the main map), how long we've been inside, the health we came in with, and
  // how many times a temple has thrown us out (twice and they stay closed to us).
  int8_t temple = -1;
  float templeT = 0;
  float templeHp = 0;
  uint8_t templeKicks = 0;
  float deniedT = 0;            // cooldown for the "the temples won't let you in" message
  float portalT = 0;            // seconds stood in a temple's exit portal (2 s to go through)
  float cdScale = 1.0f;         // superpower cooldowns are multiplied by this (Rocket Rampage)
  int32_t score = 0;            // Rocket Rampage score
  Vec2 home;
  uint32_t aggroId = 0;
  float aggroT = 0;

  // Respawn and bot state (bots are the lobby fillers).
  float deathTimer = 0;
  bool isBot = false;
  bool gone = false;            // removed from the match (dev panel): never respawns
  float blindT = 0;             // blinded (by an Omnipotent test bot): can barely see
  float powerMul = 1.0f;        // dev/test stat multiplier: damage, fire rate, speed, range and bullet speed
  bool ghost = false;           // dev panel: bots, guardians and turrets don't notice this tank
  uint8_t ringShots = 0;        // admin Bullet Storm: every shot fires this many bullets all around (0 = off)
  bool superBounce = false;     // admin Bouncy Bullets: bullets bounce off walls 25 times
};

inline float angleDiff(float from, float to) {  // shortest signed difference, in (-pi, pi]
  float d = std::fmod(to - from + kPi, 2.0f * kPi);
  if (d < 0) d += 2.0f * kPi;
  return d - kPi;
}

inline bool hasRelic(const Tank& t, Relic r) { return t.relic[0] == r || t.relic[1] == r; }

// Moves a tank with momentum. `speedMul` folds in stat tiers, Speed Boost, Repair, Singularity and Guardian slowness.
// lo/hi: the area it can't leave (the map, or an Arena Temple); defaults to the map.
inline void stepTank(Tank& t, const Input& in, float speedMul = -1.0f, Vec2 lo = {-1, -1}, Vec2 hi = {-1, -1}) {
  if (speedMul < 0) speedMul = statSpeedMul(t.stat[StatSpeed]);
  const Vec2 dir = Vec2{(float)in.moveX, (float)in.moveY}.norm();  // normalized: no faster diagonals
  if (t.active == Power::Dash && t.activeLeft > 0) {
    t.vel = t.dashDir * kDashSpeed;
  } else {
    const Vec2 want = dir * (kTankSpeed * speedMul * in.speed);
    const float k = 1.0f - std::exp(-(dir.len() > 0 ? kAccel : kFriction) * kDt);
    t.vel += (want - t.vel) * k;
    if (dir.len() == 0 && t.vel.len() < 4.0f) t.vel = {};
  }
  t.pos += t.vel * kDt;
  if (lo.x < 0) { lo = {0, 0}; hi = {kMapSize, kMapSize}; }
  const float x0 = lo.x + t.radius, x1 = hi.x - t.radius, y0 = lo.y + t.radius, y1 = hi.y - t.radius;
  if (t.pos.x < x0 || t.pos.x > x1) { t.pos.x = std::clamp(t.pos.x, x0, x1); t.vel.x = 0; }
  if (t.pos.y < y0 || t.pos.y > y1) { t.pos.y = std::clamp(t.pos.y, y0, y1); t.vel.y = 0; }

  // Barrel turns toward the aim point at a limited rate.
  const float maxTurn = kTurnRate * in.turn * kDt;
  t.aim += std::clamp(angleDiff(t.aim, in.aim), -maxTurn, maxTurn);
}

}  // namespace tonk
