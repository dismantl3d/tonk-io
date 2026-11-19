#pragma once
// Match world: tanks, bullets, pickups, loot crates, superpowers, relics, Arena Guardians, Arena Temples and the maps.
// Deterministic; shared by client and server.
// Weapon stats (1..10, see weapons.h) are converted to real numbers by weaponParams().
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "core/powerups.h"
#include "core/ring.h"
#include "core/sim.h"
#include "core/weapons.h"

namespace tonk {

// ---- Deterministic RNG (xorshift32) ----
struct Rng {
  uint32_t s = 2463534242u;
  uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
  float f01() { return (next() >> 8) * (1.0f / 16777216.0f); }
  float range(float a, float b) { return a + (b - a) * f01(); }
};

// ---- Weapon stats -> gameplay numbers ----
struct WeaponParams {
  float damage;       // per bullet
  float reloadSec;
  float bulletSpeed;  // units/sec
  float range;        // units before the bullet fades
  float bulletRadius;
};

constexpr WeaponParams weaponParams(const WeaponStats& w) {
  return {
      4.0f + 3.0f * w.dmg,    // Standard (5): 19
      1.1f - 0.09f * w.rof,   // Standard: 0.65 s
      300.0f + 100.0f * w.pspd,
      220.0f + 110.0f * w.rng,  // Standard: 770
      6.0f + 1.0f * w.dmg,
  };
}
// Arena Guardians: heavy, slow bullets and a slightly slower fire rate than a player.
constexpr WeaponParams kGuardWeapon = {42.0f, 0.85f, 470.0f, 950.0f, 15.0f};

constexpr float kBarrelLength = 58.0f;  // muzzle distance from tank center (scaled by tank size)

// ---- Bullets ----
enum class BulletKind : uint8_t { Normal, Bolt, Guardian, Turret, Drone };
struct Bullet {
  uint32_t id = 0, owner = 0;
  Vec2 pos, vel;
  float damage = 0, radius = 8, rangeLeft = 0, range0 = 0;  // range0: the full range (bullets grow and fade as they fly)
  BulletKind kind = BulletKind::Normal;
  uint8_t bounces = 0;  // bullets bounce off walls (a few times)
  uint8_t maxBounce = 4;
};
constexpr int kMaxBounces = 4;

// ---- Shapes (dormant: kept for modes that farm polygons) ----
enum class ShapeKind : uint8_t { Square, Triangle, Pentagon };
struct ShapeInfo { float radius, health; uint32_t xp; int sides; };
constexpr ShapeInfo shapeInfo(ShapeKind k) {
  switch (k) {
    case ShapeKind::Square:   return {24, 30, 10, 4};
    case ShapeKind::Triangle: return {28, 60, 25, 3};
    default:                  return {40, 150, 100, 5};
  }
}
struct Shape {
  uint32_t id = 0;
  ShapeKind kind = ShapeKind::Square;
  Vec2 pos, vel;
  float rot = 0, spin = 0;
  float health = 0;
  float hitFlash = 0;
};

// ---- Pickups ----
struct Crate { uint32_t id = 0; Vec2 pos; Power type = Power::Speed; };  // a superpower lying on the ground
struct Upgrade { uint32_t id = 0; Vec2 pos; uint8_t stat = StatSpeed; Rarity rarity = Rarity::Common; };
struct RelicPickup { uint32_t id = 0; Vec2 pos; Relic kind = Relic::Repair; };

// Obstacles: thick line segments with round ends. They block tanks; bullets bounce off them. kind 0 can be blown open by
// rockets; 1 is temple stone and 2 a pillar outside a temple (neither breaks; the pillars go when the temple is cleared).
struct Wall { Vec2 a, b; float thick = 18.0f; uint8_t kind = 0; int8_t temple = -1; };
inline Vec2 closestOnSeg(Vec2 a, Vec2 b, Vec2 p) {
  const Vec2 ab = b - a;
  const float l2 = ab.x * ab.x + ab.y * ab.y;
  const float t = l2 > 0 ? std::clamp(((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / l2, 0.0f, 1.0f) : 0.0f;
  return a + ab * t;
}

// ---- Superpower entities ----
struct Rocket { uint32_t id = 0, owner = 0; Vec2 pos, vel; bool homing = false; float life = 0, rangeLeft = 0; };
struct Bomb { uint32_t id = 0, owner = 0; Vec2 pos; float fuse = kBombFuse, blast = kBombBlast, dmg = kBombDamage; uint8_t kind = 0; };  // kind 1: a coconut
struct Strike {  // a plane pass; the bombs land scattered around the target, not in a line
  uint32_t id = 0, owner = 0;
  Vec2 start, dir, target;
  float t = 0;
  int dropped = 0;
  float along[kStrikeBombs] = {};  // how far along the pass each bomb is released
  Vec2 drop[kStrikeBombs];         // where each bomb lands
};
struct Turret { uint32_t id = 0, owner = 0; Vec2 pos; float life = kTurretLife, cd = 0.3f, aim = 0; };
struct Singularity { uint32_t id = 0, owner = 0; Vec2 pos; float life = kSingLife; };
enum class DroneMode : uint8_t { Orbit, Chase, Fetch, Return, Kamikaze };
struct Drone {
  uint32_t id = 0, owner = 0;
  Relic kind = Relic::Sentry;
  Vec2 pos, goal;
  int hp = kSentryHp;
  DroneMode mode = DroneMode::Orbit;
  float angle = 0, cd = 0.5f, timer = 0, aim = 0;
  uint32_t fetchId = 0;
  int carryStat = -1;
  Rarity carryRarity = Rarity::Common;
  bool dead = false;
  bool vulnerable() const { return mode != DroneMode::Orbit; }
};

// ---- Loot crates: shoot them (or blow them up) to break them open ----
// Normal: 3 items. Legendary (one at a time, announced 10 s ahead): the same rules, much better odds. Small (inside
// temples): 1-2 upgrades.
enum class BoxKind : uint8_t { Normal, Legendary, Small };
struct LootBox { uint32_t id = 0; Vec2 pos; BoxKind kind = BoxKind::Normal; float hp = 0; uint32_t lastHit = 0; };
constexpr float boxRadius(BoxKind k) { return k == BoxKind::Legendary ? 52.0f : k == BoxKind::Small ? 32.0f : 40.0f; }  // all bigger than a tank
constexpr float boxHealth(BoxKind k) { return k == BoxKind::Legendary ? 260.0f : k == BoxKind::Small ? 24.0f : 70.0f; }
constexpr int kTargetBoxes = 18, kBoxEvery = 300;  // a new crate every 10 s while there are fewer than 18
constexpr float kLegendWarn = 10.0f;               // seconds of warning before a Legendary Crate lands

// ---- Arena Temples (GAME_DESIGN.md section 7.5) ----
// A small building on the map with a portal inside. The inside is its own place, far off the map: a dark spiral of
// corridors with small Temple Guardians and crates, and the Arena Mage in the middle. Beat the Mage and a portal takes
// you to the edge of the safe zone; the temple then closes and leaves a patch behind.
struct Temple {
  Vec2 door;               // the portal on the map
  Vec2 c;                  // middle of the inside
  float R = 2000.0f;       // outer wall (its middle line); set by buildTemple
  float chamber = 1000.0f; // the Mage's room (its wall's middle line)
  float thick = 200.0f;    // the spiral's walls are very thick stone (about 3.5 tanks across)
  std::vector<float> rings;  // wall radii, outer to inner (the last one is the Mage's room)
  std::vector<float> gapA, gapHalf;  // each ring's way in: the angle of its middle and half its width (0: no gap)
  std::vector<Vec2> blocks;  // the dead-end walls between rings (pairs of points), for light and shadow
  Vec2 entry;              // where you arrive inside
  std::vector<Vec2> path;  // the way through: waypoints from the entry to the middle (bots follow it)
  std::vector<Vec2> mobs;  // where the small guardians stand
  std::vector<Vec2> deco;  // braziers (drawing only)
  int style = 0;           // look and shape, per map
  uint32_t mageId = 0;
  bool cleared = false;    // the Mage is beaten: the exit portal is open and the temple is closing
  float clearT = 0;
};
constexpr float kTempleTime = 210.0f;   // 3.5 minutes inside, then you lose half your health and are thrown out
constexpr float kTempleClose = 25.0f;   // after the Mage falls, everyone still inside is sent out after this long
constexpr float kPortalR = 46.0f, kTempleSpacing = 6000.0f;
constexpr float kTempleStep = 500.0f;  // ring to ring: a 300-wide corridor between 200-thick walls
constexpr int kTempleKicks = 2;        // knocked out in temples twice: they won't let you in again
constexpr float kExitR = 75.0f, kExitHold = 2.0f;  // the Mage's exit portal: stand in it for 2 s to go through
constexpr float kMobRadius = 24.0f, kMobHealth = 55.0f, kMobDamageMul = 0.55f, kMageSpeedMul = 0.55f;
constexpr uint32_t kMobIdBase = 3001, kMageIdBase = 3901;
constexpr int kMobXp = 30;

// ---- Maps: set wall layouts. Each has its own temple spots and temple shape ----
constexpr int kMapCount = 3;
inline const char* mapName(int m) {
  static const char* k[kMapCount] = {"Crossroads", "Ruins", "Canyon"};
  return k[((m % kMapCount) + kMapCount) % kMapCount];
}

// ---- Game modes ----
enum class Mode : uint8_t { Royale, Rampage };
// Rocket Rampage: 3 minutes, everyone has a Rocket with a short cooldown, respawns, highest score wins.
// Rockets are the only weapon there (a hit takes 50% of max health), and only Move Speed and Field of View upgrades drop.
// A fog closes in: the arena is 6,300 wide (radius, nearly the whole map) for 45 s, then shrinks to 1,500 by 2:45.
constexpr float kRampageLen = 180.0f, kRampageR = 6300.0f, kRampageR1 = 1500.0f, kRampageCd = 0.35f, kRampageStorm = 20.0f;
constexpr float kRampageFogStart = 45.0f, kRampageFogEnd = 165.0f, kRampageFrac = 0.5f;
constexpr int kRampageKill = 50, kRampageTanks = 20;  // score: 1 per point of damage, +50 per knockout

// Things that happened this tick, for the client (explosions, lost drones, dodges, guardian rewards).
// PowerUsed: a = tank, b = power. WallBreak: pos = middle of the hole, r = hole length, a = wall angle in degrees.
// BoxBroken: r = crate kind, a = who broke it. LegendWarn / LegendDrop: where. TempleEnter: a = tank, b = temple.
// TempleExit: a = tank, b = 0 portal, 1 knocked out, 2 out of time, 3 the temple closed; r = times knocked out so far.
// TempleCleared: a = killer, b = temple. TempleDenied: a = tank (knocked out twice, it can't go in again).
// Blinded: a = the blinded tank, b = who did it (an Omnipotent test bot). Snare: Ivy Snare at pos, r = radius.
enum class EvType : uint8_t { Boom, DroneLost, Dodged, DodgeMissed, GuardianDrop, Teleport, PowerUsed, WallBreak, BoxBroken,
                              LegendWarn, LegendDrop, TempleEnter, TempleExit, TempleCleared, TempleDenied, Blinded, Snare };
struct Event { EvType type; Vec2 pos; float r = 0; uint32_t a = 0, b = 0; };

constexpr int kTargetShapes = 0;
// Loot on the map (the map is 14000 x 14000). Kept sparse so a full loadout takes a whole match.
constexpr int kTargetCrates = 24;
constexpr int kTargetUpgrades = 130;
constexpr int kTargetRelics = 11;
constexpr int kCrateEvery = 270, kUpgradeEvery = 75, kRelicEvery = 900;  // ticks between respawns (9 s, 2.5 s, 30 s)
constexpr float kUpgradeRadius = 20.0f, kCrateRadius = 22.0f, kRelicRadius = 22.0f;
constexpr float kKillHeal = 25.0f;
// Arena Guardians: big, slow, sharp-shooting bots. The Arena Mage at the heart of every temple is one.
constexpr int kGuardians = 3;
constexpr uint32_t kGuardIdBase = 1001;
constexpr float kGuardRadius = 46.0f, kGuardHealth = 650.0f, kGuardSpeedMul = 0.42f, kGuardFov = 560.0f, kGuardLeash = 420.0f;

// Who is on whose side. Guardians, temple guardians and the Mage are one side; a Sentry Robot is on its owner's side;
// everyone else is on their own.
inline bool npc(const Tank& t) { return t.guardian || t.mob || t.sentryOf != 0; }
inline bool arenaSide(const Tank& t) { return t.guardian || t.mob; }
inline uint32_t sideOf(const Tank& t) { return t.sentryOf ? t.sentryOf : t.id; }
inline bool hostile(const Tank& a, const Tank& b) {
  if (a.id == b.id) return false;
  if (arenaSide(a) || arenaSide(b)) return arenaSide(a) != arenaSide(b);
  return sideOf(a) != sideOf(b);
}

// ---- Closing Ring: zones are pre-rolled so the ring is a pure function of match time ----
struct Zone { Vec2 c; float r = 0; };
struct RingState {
  Zone cur, next;
  int phase = -1;
  bool shrinking = false;
  float secsLeft = 0;
  float dmgPerSec = 0;
};
constexpr float kRingFullRadius = kMapSize * 0.70711f;  // circumscribes the map, so the ring starts "off"

struct World {
  std::vector<Tank> tanks;
  std::vector<Bullet> bullets;
  std::vector<Shape> shapes;
  std::vector<Crate> crates;
  std::vector<Upgrade> upgrades;
  std::vector<RelicPickup> relics;
  std::vector<Wall> walls;
  std::vector<Rocket> rockets;
  std::vector<Bomb> bombs;
  std::vector<Strike> strikes;
  std::vector<Turret> turrets;
  std::vector<Drone> drones;
  std::vector<Singularity> sings;
  std::vector<LootBox> boxes;
  std::vector<Temple> temples;
  std::vector<Tank> pending;  // tanks made this tick (Sentry Robots), added once the tank loop is done
  std::vector<Event> events;
  Rng rng;
  uint32_t nextId = 1;
  uint32_t tickCount = 0;
  float time = 0;
  bool allowRespawn = false;  // sandbox and Rocket Rampage; Battle Royale is last-tank-standing
  Mode mode = Mode::Royale;
  int mapId = 0;
  uint32_t nextSentry = kSentryIdBase, nextMob = kMobIdBase;
  Vec2 heart{kMapSize / 2, kMapSize / 2};  // where the storm ends: at one of the temples
  // Sandbox (set before init): the play area (a square around the middle of the map), the fog, how much loot, and
  // where the temples stand (scaled toward the middle for small areas).
  bool sandbox = false;
  Vec2 lo{0, 0}, hi{kMapSize, kMapSize};
  int fogMode = -1;           // -1 the mode's own, 0 none, 1 the Battle Royale storm, 2 a circle that closes, 3 a fixed circle
  float fogR0 = 5000, fogR1 = 1200, fogT0 = 30, fogT1 = 150, fogDmg = 15;
  float lootMul = 1.0f, ringScale = 1.0f, spotScale = 1.0f;
  bool emptyMap = false, noTemples = false;
  Vec2 middle() const { return (lo + hi) * 0.5f; }
  float legendAt = -1;        // when the next Legendary Crate lands (-1 = none planned)
  bool legendWarned = false;
  Vec2 legendPos;

  bool ringEnabled = false;
  std::array<Zone, kRingPhases.size() + 1> zones{};
  RingState ring;
  bool matchOver = false;
  uint32_t winnerId = 0;
  uint32_t godId = 0;            // test hook: this tank takes no damage
  uint32_t specialGuardian = 0;  // the guardian that drops the big reward
  // Dev panel lines. A red one (from -> to) sends a bot after that tank; a bot can have several. A blue one teams two
  // tanks up, both ways: they never go after, or hurt, each other.
  struct Link { uint32_t from, to; bool team; };
  std::vector<Link> links;
  bool teamed(uint32_t a, uint32_t b) const {
    for (const Link& l : links) if (l.team && ((l.from == a && l.to == b) || (l.from == b && l.to == a))) return true;
    return false;
  }
  bool targets(uint32_t a, uint32_t b) const {
    for (const Link& l : links) if (!l.team && l.from == a && l.to == b) return true;
    return false;
  }
  // Adds the line, or takes it away if it's already there (returns true if it was added). A red line replaces a blue
  // one between the same two tanks, and the other way round.
  bool toggleLink(uint32_t a, uint32_t b, bool team) {
    if (a == b || !a || !b) return false;
    auto same = [&](const Link& l) {
      return l.team == team && (team ? ((l.from == a && l.to == b) || (l.from == b && l.to == a)) : (l.from == a && l.to == b));
    };
    for (size_t i = 0; i < links.size(); ++i)
      if (same(links[i])) { links.erase(links.begin() + i); return false; }
    for (size_t i = links.size(); i-- > 0;) {
      const Link& l = links[i];
      const bool pair = (l.from == a && l.to == b) || (l.from == b && l.to == a);
      if (pair && l.team != team) links.erase(links.begin() + i);
    }
    links.push_back({a, b, team});
    return true;
  }
  // which: 0 every line, 1 the red ones, 2 the blue ones; from: only lines touching this tank (0: everyone's).
  void clearLinks(int which, uint32_t from = 0) {
    for (size_t i = links.size(); i-- > 0;) {
      const Link& l = links[i];
      if (which == 1 && l.team) continue;
      if (which == 2 && !l.team) continue;
      if (from && l.from != from && l.to != from) continue;
      links.erase(links.begin() + i);
    }
  }

  uint32_t newId() { return nextId++; }
  void emit(EvType t, Vec2 p, float r = 0, uint32_t a = 0, uint32_t b = 0) { events.push_back({t, p, r, a, b}); }

  Tank* find(uint32_t id) {
    for (Tank& t : tanks) if (t.id == id) return &t;
    return nullptr;
  }

  // The zones are built backwards from the map's heart, one of its temples: the last two (the eye of the storm)
  // are centered on it, and every zone before holds the one after it.
  void setupRing() {
    ringEnabled = true;
    const int n = (int)kRingPhases.size();
    zones[0] = {middle(), kRingFullRadius * ringScale};
    for (int k = n; k >= 1; --k) {
      Zone z;
      z.r = kRingFullRadius * ringScale * kRingPhases[k - 1].endRadiusPct / 100.0f;
      if (k >= n - 1) { z.c = heart; zones[k] = z; continue; }
      const Zone& next = zones[k + 1];
      const float slack = std::max(0.0f, z.r - next.r) * 0.85f;
      const float a = rng.range(0, 6.2831853f), d = slack * std::sqrt(rng.f01());
      z.c = next.c + Vec2{std::cos(a) * d, std::sin(a) * d};
      z.c.x = std::clamp(z.c.x, lo.x, hi.x);
      z.c.y = std::clamp(z.c.y, lo.y, hi.y);
      zones[k] = z;
    }
  }
  Vec2 randomPos(float margin = 200.0f) {
    margin = std::min(margin, (hi.x - lo.x) * 0.3f);
    return {rng.range(lo.x + margin, hi.x - margin), rng.range(lo.y + margin, hi.y - margin)};
  }
  // 0 at the map edge .. 1 in the middle. Loot gets better toward the middle.
  static float closeness(Vec2 p) { return std::clamp(1.0f - (p - Vec2{kMapSize / 2, kMapSize / 2}).len() / kRingFullRadius, 0.0f, 1.0f); }

  void spawnShape() {
    Shape s;
    s.id = newId();
    float r = rng.f01();
    s.kind = r < 0.70f ? ShapeKind::Square : (r < 0.92f ? ShapeKind::Triangle : ShapeKind::Pentagon);
    s.health = shapeInfo(s.kind).health;
    s.pos = randomPos();
    s.vel = {rng.range(-12, 12), rng.range(-12, 12)};
    s.rot = rng.range(0, 6.2831853f);
    s.spin = rng.range(-0.6f, 0.6f);
    shapes.push_back(s);
  }

  void spawnCrate(Vec2 pos, Power type = Power::None) {
    Crate c;
    c.id = newId();
    c.pos = clearOf(pos);
    c.type = type != Power::None ? type : (Power)(1 + rng.next() % kNormalPowers);
    crates.push_back(c);
  }

  // Rarer upgrades toward the middle. At the edge: Common 60%, Uncommon 26%, Rare 10%, Epic 3.5%, Legendary 0.5%.
  // In the middle: Common 22%, Uncommon 28%, Rare 26%, Epic 17%, Legendary 7% (as weights).
  Rarity rollUpgradeRarity(Vec2 pos) {
    const float c = closeness(pos);
    const float wts[5] = {60 - 38 * c, 26 + 2 * c, 10 + 16 * c, 3.5f + 13.5f * c, 0.5f + 6.5f * c};
    float tot = 0;
    for (float x : wts) tot += x;
    float v = rng.f01() * tot;
    for (int i = 0; i < 5; ++i) { v -= wts[i]; if (v <= 0) return (Rarity)i; }
    return Rarity::Legendary;
  }
  // Loot never sits in a wall (where nobody could reach it): nudge it to the nearest clear spot.
  Vec2 clearOf(Vec2 p, float r = 26.0f) const {
    if (!inWall(p, r)) return p;
    for (int k = 1; k <= 16; ++k)
      for (int i = 0; i < 12; ++i) {
        const float a = i * 0.5236f + k * 0.3f;
        const Vec2 q = p + Vec2{std::cos(a), std::sin(a)} * (25.0f * k);
        if (!inWall(q, r) && inAnyArea(q)) return q;
      }
    return p;
  }
  void spawnUpgrade(Vec2 pos, int rarity = -1, int stat = -1) {
    Upgrade u;
    u.id = newId();
    u.pos = clearOf(pos);
    // Rocket Rampage only drops Move Speed and Field of View; Battle Royale drops the other stats (never Field of View).
    if (stat < 0) stat = mode == Mode::Rampage ? (rng.f01() < 0.5f ? StatSpeed : StatFov) : (int)(rng.next() % kRoyaleStats);
    u.stat = (uint8_t)stat;
    u.rarity = rarity >= 0 ? (Rarity)rarity : rollUpgradeRarity(pos);
    upgrades.push_back(u);
  }
  // Relics are rare, and more common toward the middle.
  Vec2 relicPos() {
    for (int i = 0; i < 30; ++i) {
      const Vec2 p = randomPos(300.0f);
      if (rng.f01() < 0.15f + 0.85f * closeness(p)) return p;
    }
    return randomPos(300.0f);
  }
  void spawnRelic(Vec2 pos, Relic kind = Relic::None) {
    RelicPickup r;
    r.id = newId();
    r.pos = clearOf(pos);
    r.kind = kind != Relic::None ? kind : (Relic)(1 + rng.next() % kRelicKinds);
    relics.push_back(r);
  }

  // ---- Maps: set wall layouts. Each map has three temples on different sides; the storm always ends at one of them
  // (picked per match), so that one stays in the eye of the storm until the very end. ----
  void buildMap(int id) {
    walls.clear();
    temples.clear();
    mapId = ((id % kMapCount) + kMapCount) % kMapCount;
    const Vec2 C{kMapSize / 2, kMapSize / 2};
    auto seg = [&](Vec2 a, Vec2 b) { walls.push_back({C + a, C + b, 18.0f}); };
    auto mirror4 = [&](Vec2 a, Vec2 b) {  // one quarter of the map, mirrored into all four
      for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2) seg({a.x * sx, a.y * sy}, {b.x * sx, b.y * sy});
    };
    auto pol = [](float r, float deg) { return Vec2{std::cos(deg * 0.0174533f) * r, std::sin(deg * 0.0174533f) * r}; };
    auto arc = [&](float r, float d0, float d1, int n) {
      for (int i = 0; i < n; ++i) seg(pol(r, d0 + (d1 - d0) * i / n), pol(r, d0 + (d1 - d0) * (i + 1) / n));
    };
    Vec2 spots[3];  // the three temples, on different sides of the map (relative to the middle)
    switch (mapId) {
      case 0:  // Crossroads: bunkers and avenues, the same in every quarter
        mirror4({1500, 1500}, {2500, 1500}); mirror4({1500, 1500}, {1500, 2500});
        mirror4({3300, 500}, {3300, 1900}); mirror4({500, 3300}, {1900, 3300});
        mirror4({3700, 3700}, {4500, 4500});
        mirror4({5300, 1700}, {5300, 2900}); mirror4({5300, 2900}, {4400, 2900});
        mirror4({1700, 5300}, {2900, 5300}); mirror4({2900, 5300}, {2900, 4400});
        mirror4({6000, 3900}, {6000, 5100}); mirror4({3900, 6000}, {5100, 6000});
        mirror4({2200, 2900}, {2900, 2200});
        mirror4({600, 900}, {1100, 400}); mirror4({4300, 500}, {5200, 500}); mirror4({500, 4300}, {500, 5200});
        mirror4({4000, 1100}, {4000, 1800}); mirror4({1100, 4000}, {1800, 4000}); mirror4({4700, 3500}, {5600, 3500});
        mirror4({3500, 4700}, {3500, 5600}); mirror4({6100, 6100}, {5400, 6100}); mirror4({2400, 600}, {2400, 1100});
        spots[0] = pol(4300, -90); spots[1] = pol(4300, 30); spots[2] = pol(4300, 150);
        break;
      case 1:  // Ruins: broken rings of wall around the middle, spokes between them, rubble in the corners
        for (int i = 0; i < 4; ++i) arc(1150, 20.0f + 90 * i, 70.0f + 90 * i, 2);
        for (int i = 0; i < 6; ++i) arc(2300, 10.0f + 60 * i, 50.0f + 60 * i, 3);
        for (int i = 0; i < 8; ++i) arc(3900, 25.0f + 45 * i, 55.0f + 45 * i, 3);
        for (int i = 0; i < 10; ++i) arc(5400, 5.0f + 36 * i, 27.0f + 36 * i, 3);
        for (int i = 0; i < 6; ++i) seg(pol(2600, 40.0f + 60 * i), pol(3500, 40.0f + 60 * i));
        for (int i = 0; i < 8; ++i) seg(pol(4300, 5.0f + 45 * i), pol(5000, 5.0f + 45 * i));
        mirror4({5600, 5600}, {6300, 4900}); mirror4({4700, 6300}, {5600, 6300}); mirror4({6300, 3300}, {6300, 4300});
        mirror4({6200, 900}, {6200, 1900}); mirror4({900, 6200}, {1900, 6200});
        spots[0] = pol(4650, 90); spots[1] = pol(4650, 210); spots[2] = pol(4650, 330);  // between the outer rings
        break;
      default: {  // Canyon: long diagonal ridges with breaks, bunkers and cross walls in the valleys
        for (float o : {-4600.0f, -2300.0f, 2300.0f, 4600.0f}) {
          const Vec2 n{0.7071f * o, -0.7071f * o};
          for (float t = -6600; t < 6600; t += 1900) seg(n + Vec2{0.7071f * t, 0.7071f * t}, n + Vec2{0.7071f * (t + 1300), 0.7071f * (t + 1300)});
        }
        for (float k : {-1.0f, 1.0f}) {
          const Vec2 b = Vec2{2050, 2050} * k;
          seg(b + Vec2{-560, 0}, b + Vec2{0, -560}); seg(b + Vec2{0, -560}, b + Vec2{560, 0}); seg(b + Vec2{-560, 0}, b + Vec2{0, 560});
        }
        for (float o : {-3450.0f, 3450.0f})  // short walls across the side valleys
          for (float t : {-4200.0f, -1400.0f, 1400.0f, 4200.0f}) {
            const Vec2 m = Vec2{0.7071f * o, -0.7071f * o} + Vec2{0.7071f * t, 0.7071f * t};
            seg(m + Vec2{-250, 250}, m + Vec2{250, -250});
          }
        for (float o : {-5800.0f, 5800.0f})
          for (float t : {-1200.0f, 1200.0f}) {
            const Vec2 m = Vec2{0.7071f * o, -0.7071f * o} + Vec2{0.7071f * t, 0.7071f * t};
            seg(m + Vec2{-400, -400}, m + Vec2{400, 400});
          }
        mirror4({6200, 900}, {6200, 2000}); mirror4({900, 6200}, {2000, 6200}); mirror4({3400, 300}, {3400, 1000});
        spots[0] = {2828, 2828}; spots[1] = {460, -4420}; spots[2] = {-4420, 460};  // main valley and both side valleys
        break;
      }
    }
    Vec2 doors[3];
    for (int k = 0; k < 3; ++k) doors[k] = C + spots[k] * spotScale;
    heart = doors[rng.next() % 3];  // the storm ends at one of the temples (picked per match)
    if (emptyMap) walls.clear();
    if (sandbox && noTemples) heart = middle();
    // Keep the map edge and the temples clear.
    std::vector<Wall> keep;
    for (const Wall& wl : walls) {
      auto inside = [](Vec2 p) { return p.x > 250 && p.y > 250 && p.x < kMapSize - 250 && p.y < kMapSize - 250; };
      bool ok = inside(wl.a) && inside(wl.b);
      for (const Vec2& d : doors) ok = ok && (closestOnSeg(wl.a, wl.b, d) - d).len() > 600.0f;
      if (ok) keep.push_back(wl);
    }
    walls.swap(keep);
    if (!noTemples) for (int k = 0; k < 3; ++k) buildTemple(k, doors[k], mapId);
  }

  // Rocket Rampage: extra cover inside the arena (two rings of short walls and a few spokes).
  void rampageCover() {
    const Vec2 C{kMapSize / 2, kMapSize / 2};
    auto pol = [](float r, float deg) { return Vec2{std::cos(deg * 0.0174533f) * r, std::sin(deg * 0.0174533f) * r}; };
    auto tang = [&](float r, float deg, float len) {  // a wall across the radius, len long
      const Vec2 m = C + pol(r, deg), t = pol(len * 0.5f, deg + 90);
      walls.push_back({m - t, m + t, 18.0f});
    };
    for (int i = 0; i < 8; ++i) tang(1500, 22.5f + 45 * i, 520);
    for (int i = 0; i < 12; ++i) tang(2900, 30.0f * i, 640);
    for (int i = 0; i < 16; ++i) tang(4000, 11.0f + 22.5f * i, 560);
    for (int i = 0; i < 20; ++i) tang(5300, 4.0f + 18.0f * i, 600);
    for (int i = 0; i < 10; ++i) walls.push_back({C + pol(4500, 18.0f + 36.0f * i), C + pol(4900, 18.0f + 36.0f * i), 18.0f});
    for (int i = 0; i < 6; ++i) walls.push_back({C + pol(2100, 60.0f * i), C + pol(2600, 60.0f * i), 18.0f});
  }

  // A temple: a stepped platform with four pillars around a portal on the map, and the inside: thick rings of stone
  // with one gap each, plus a dead end next to every way in, so the only way to the middle is round and round.
  void buildTemple(int k, Vec2 door, int style) {
    Temple T;
    T.door = door;
    T.style = style;
    const int n = style == 2 ? 4 : 3;
    T.R = T.chamber + (n - 1) * kTempleStep;
    T.c = {kMapSize + 3300.0f + k * kTempleSpacing, 3300.0f};
    for (int i = 0; i < 4; ++i) {
      const float a = 0.7854f + i * 1.5708f;
      const Vec2 p = door + Vec2{std::cos(a), std::sin(a)} * 215.0f;
      walls.push_back({p, p + Vec2{0.01f, 0}, 50.0f, 2, (int8_t)k});
    }
    const float dir = style == 1 ? -1.0f : 1.0f;
    const float a0 = style == 0 ? kPi * 0.5f : style == 1 ? -kPi * 0.5f : kPi;
    const float step = kTempleStep, lap = 2 * kPi - 0.6f, th = T.thick;
    auto P = [&](float r, float a) { return T.c + Vec2{std::cos(a) * r, std::sin(a) * r}; };
    auto stone = [&](Vec2 a, Vec2 b, float w) { walls.push_back({a, b, w, 1, (int8_t)k}); };
    float start = a0;
    T.entry = P(T.R - step * 0.5f, a0 + dir * 0.2f);
    T.path.push_back(T.entry);
    for (int i = 0; i < n; ++i) {
      const float r = T.R - i * step;
      T.rings.push_back(r);
      const float half = i > 0 ? (150.0f + th * 0.5f) / r : 0.0f;  // the gap where the corridor outside comes in (300 clear)
      T.gapA.push_back(start);
      T.gapHalf.push_back(half);
      const float from = start + half, to = start + 2 * kPi - half;
      const int segs = 36;
      for (int q = 0; q < segs; ++q) stone(P(r, from + (to - from) * q / segs), P(r, from + (to - from) * (q + 1) / segs), th);
      const float inner = r - th * 0.5f - (i == n - 1 ? 50.0f : 26.0f);
      for (float a = 0; a < 6.28f; a += i == n - 1 ? 1.5708f : 0.7854f) T.deco.push_back(P(inner, start + kPi / 8 + a));
      if (i == n - 1) break;
      const float rn = r - step, rm = (r + rn) * 0.5f, end = start + dir * lap;
      const Vec2 b0 = P(rn + th * 0.5f, start - dir * 0.3f), b1 = P(r - th * 0.5f, start - dir * 0.3f);
      stone(b0, b1, th);  // dead end behind the way in
      T.blocks.push_back(b0); T.blocks.push_back(b1);
      for (float a = 0.3f; a < lap; a += 0.25f) T.path.push_back(P(rm, start + dir * a));
      T.path.push_back(P(rm, end));
      T.path.push_back(P(rn - step * 0.45f, end));
      const int mobs = 2 + (rng.f01() < 0.5f ? 1 : 0);
      for (int m = 0; m < mobs; ++m) T.mobs.push_back(P(rm, start + dir * lap * (m + 1.0f) / (mobs + 1.0f)));
      for (float f : {0.16f, 0.62f}) {
        LootBox b;
        b.id = newId(); b.kind = BoxKind::Small; b.hp = boxHealth(b.kind);
        b.pos = P(rm + (step - th) * (f < 0.5f ? 0.22f : -0.22f), start + dir * lap * f);
        boxes.push_back(b);
      }
      start = end;
    }
    for (int i = 0; i < 4; ++i) {  // four pillars in the Mage's room
      const Vec2 p = P(T.chamber * 0.6f, start + 0.7854f + i * 1.5708f);
      stone(p, p + Vec2{0.01f, 0}, 90.0f);
    }
    T.path.push_back(T.c);
    temples.push_back(T);
  }

  // The area a tank can move in: the map, or the inside of its temple.
  void areaOf(int temple, Vec2& a, Vec2& b) const {
    if (temple < 0 || temple >= (int)temples.size()) { a = lo; b = hi; return; }
    const Temple& T = temples[temple];
    const float e = T.R + 120.0f;
    a = T.c - Vec2{e, e};
    b = T.c + Vec2{e, e};
  }
  bool inAnyArea(Vec2 p) const {
    if (p.x >= lo.x && p.y >= lo.y && p.x <= hi.x && p.y <= hi.y) return true;
    for (const Temple& T : temples) if ((p - T.c).len() < T.R + 120.0f) return true;
    return false;
  }
  // Somewhere free on the map (not in a wall, a crate, a temple or the guardians' arena).
  bool freeSpot(Vec2 p, float r) const {
    if (inWall(p, r)) return false;
    for (const LootBox& b : boxes) if ((b.pos - p).len() < boxRadius(b.kind) + r + 20.0f) return false;
    for (const Temple& T : temples) if ((T.door - p).len() < 340.0f + r) return false;
    return true;
  }
  void spawnBox(Vec2 pos, BoxKind kind) {
    LootBox b;
    b.id = newId(); b.pos = pos; b.kind = kind; b.hp = boxHealth(kind);
    boxes.push_back(b);
  }
  // A random spot for loot: anywhere on the map, or inside the arena in Rocket Rampage.
  Vec2 lootPos(float margin = 200.0f) {
    float R = 0;
    if (sandbox) R = fogMode == 2 || fogMode == 3 ? fogR0 : 0;
    else if (mode == Mode::Rampage) R = kRampageR;
    if (R <= 0) return randomPos(margin);
    for (int k = 0; k < 20; ++k) {  // inside the arena (and the play area)
      const float a = rng.range(0, 6.2831853f), d = std::sqrt(rng.f01()) * std::max(100.0f, R - 250.0f);
      const Vec2 p = middle() + Vec2{std::cos(a), std::sin(a)} * d;
      if (p.x > lo.x + margin && p.y > lo.y + margin && p.x < hi.x - margin && p.y < hi.y - margin) return p;
    }
    return randomPos(margin);
  }
  int boxCount(BoxKind k) const {
    int n = 0;
    for (const LootBox& b : boxes) n += b.kind == k ? 1 : 0;
    return n;
  }

  // Is any wall between p and q? (exact segment-to-segment distance, so thin walls can't be missed)
  bool lineBlocked(Vec2 p, Vec2 q, float pad = 0.0f) const {
    auto cross = [](Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; };
    for (const Wall& w : walls) {
      const Vec2 r = q - p, sv = w.b - w.a, qp = w.a - p;
      const float den = cross(r, sv);
      if (std::fabs(den) > 1e-6f) {
        const float t = cross(qp, sv) / den, u = cross(qp, r) / den;
        if (t >= 0 && t <= 1 && u >= 0 && u <= 1) return true;
      }
      const float lim = w.thick * 0.5f + pad;
      if ((closestOnSeg(p, q, w.a) - w.a).len() < lim || (closestOnSeg(p, q, w.b) - w.b).len() < lim ||
          (closestOnSeg(w.a, w.b, p) - p).len() < lim || (closestOnSeg(w.a, w.b, q) - q).len() < lim)
        return true;
    }
    return false;
  }
  bool inWall(Vec2 p, float r) const {
    for (const Wall& w : walls) if ((p - closestOnSeg(w.a, w.b, p)).len() < w.thick * 0.5f + r) return true;
    return false;
  }

  Tank& addTank(uint32_t id, Vec2 pos, bool bot) {
    Tank t;
    t.id = id; t.pos = pos; t.isBot = bot;
    tanks.push_back(t);
    return tanks.back();
  }

  // A big Arena Guardian (the Arena Mage is one).
  Tank& addGuardian(uint32_t id, Vec2 pos) {
    Tank& g = addTank(id, pos, true);
    g.guardian = true;
    g.radius = kGuardRadius;
    g.maxHealth = g.health = kGuardHealth;
    g.home = pos;
    return g;
  }
  // Every temple's small guardians and its Arena Mage. Call after the players are added (tanks[0] is the local player).
  void spawnGuardians() {
    if (mode == Mode::Rampage) return;
    for (size_t k = 0; k < temples.size(); ++k) {
      Temple& T = temples[k];
      for (const Vec2& p : T.mobs) {
        Tank& m = addTank(nextMob++, p, true);
        m.mob = true; m.temple = (int8_t)k; m.radius = kMobRadius; m.maxHealth = m.health = kMobHealth; m.home = p;
      }
      Tank& g = addGuardian(kMageIdBase + (uint32_t)k, T.c);
      g.temple = (int8_t)k;
      T.mageId = g.id;
    }
  }

  // map < 0: pick one from the seed. Rocket Rampage has no temples, guardians, superpowers or relics on the ground.
  void init(uint32_t seed = 1, Mode m = Mode::Royale, int map = -1) {
    rng.s = seed ? seed : 1;
    mode = m;
    buildMap(map >= 0 ? map : (int)(rng.next() % kMapCount));
    if (mode == Mode::Rampage) {
      temples.clear();
      boxes.clear();
      walls.erase(std::remove_if(walls.begin(), walls.end(), [](const Wall& wl) { return wl.kind != 0; }), walls.end());
      rampageCover();
      allowRespawn = true;
      ringEnabled = true;
      while ((int)upgrades.size() < target(kTargetUpgrades / 3)) spawnUpgrade(lootPos());
      if (sandbox && fogMode == 0) ringEnabled = false;
      return;
    }
    if (fogMode != 0) setupRing();
    while ((int)shapes.size() < kTargetShapes) spawnShape();
    while ((int)crates.size() < target(kTargetCrates)) spawnCrate(lootPos());
    while ((int)upgrades.size() < target(kTargetUpgrades)) spawnUpgrade(lootPos());
    while ((int)relics.size() < target(kTargetRelics)) spawnRelic(relicPos());
    for (int i = 0; i < 300 && boxCount(BoxKind::Normal) < target(kTargetBoxes); ++i) {
      const Vec2 p = randomPos(400.0f);
      if (freeSpot(p, 40.0f)) spawnBox(p, BoxKind::Normal);
    }
    legendAt = rng.range(120.0f, 180.0f);  // the first Legendary Crate lands 2-3 minutes in
  }

  // How much loot to keep on the map: the normal amount, times the sandbox's loot setting and its share of the map.
  int target(int n) const {
    if (!sandbox) return n;
    const float area = std::max(0.08f, (hi.x - lo.x) * (hi.y - lo.y) / (kMapSize * kMapSize));
    return (int)std::lround(n * lootMul * area);
  }
  // Somewhere to (re)spawn: inside the fog and the play area, away from walls.
  Vec2 spawnSpot() {
    const Vec2 c = ring.cur.r < 1e8f && ringEnabled ? ring.cur.c : middle();
    const float r = ringEnabled && ring.cur.r < 1e8f ? std::max(300.0f, ring.cur.r - 300.0f) : 1e9f;
    for (int k = 0; k < 40; ++k) {
      const Vec2 p = randomPos(400.0f);
      if ((p - c).len() < r && !inWall(p, 60.0f)) return p;
    }
    return middle();
  }
  int aliveCount() const {  // players and bots still in the match (guardians and Sentry Robots don't count)
    int n = 0;
    for (const Tank& t : tanks) n += t.alive && !npc(t) ? 1 : 0;
    return n;
  }
  int playerCount() const {
    int n = 0;
    for (const Tank& t : tanks) n += npc(t) ? 0 : 1;
    return n;
  }
};

inline Vec2 lerpV(Vec2 a, Vec2 b, float t) { return a + (b - a) * t; }

inline RingState ringAt(const World& w) {
  RingState s;
  if (w.sandbox && (w.fogMode == 2 || w.fogMode == 3)) {  // sandbox: a circle around the middle, closing (2) or fixed (3)
    const float k = w.fogMode == 3 ? 0.0f : std::clamp((w.time - w.fogT0) / std::max(1.0f, w.fogT1 - w.fogT0), 0.0f, 1.0f);
    s.cur = {w.middle(), w.fogR0 + (w.fogR1 - w.fogR0) * k};
    s.next = {w.middle(), w.fogMode == 3 ? w.fogR0 : w.fogR1};
    s.phase = 0;
    s.shrinking = w.fogMode == 2 && k > 0 && k < 1;
    s.dmgPerSec = w.fogDmg;
    s.secsLeft = w.fogMode == 3 ? 1e9f : w.time < w.fogT0 ? w.fogT0 - w.time : k < 1 ? w.fogT1 - w.time : 1e9f;
    return s;
  }
  if (w.mode == Mode::Rampage) {  // an arena in the middle; the fog closes in from 0:45 to 2:45
    const Vec2 C{kMapSize / 2, kMapSize / 2};
    const float k = std::clamp((w.time - kRampageFogStart) / (kRampageFogEnd - kRampageFogStart), 0.0f, 1.0f);
    s.cur = {C, kRampageR + (kRampageR1 - kRampageR) * k};
    s.next = {C, kRampageR1};
    s.phase = 0;
    s.shrinking = k > 0 && k < 1;
    s.dmgPerSec = kRampageStorm;
    s.secsLeft = w.time < kRampageFogStart ? kRampageFogStart - w.time : k < 1 ? kRampageFogEnd - w.time : 1e9f;
    return s;
  }
  if (!w.ringEnabled) {
    s.cur = {{kMapSize / 2, kMapSize / 2}, 1e9f};
    s.next = s.cur;
    s.secsLeft = 1e9f;
    return s;
  }
  const int n = (int)kRingPhases.size();
  int i = -1;
  for (int k = 0; k < n; ++k)
    if (w.time >= kRingPhases[k].startSec) i = k;
  if (i < 0) {
    s.cur = w.zones[0];
    s.next = w.zones[1];
    s.secsLeft = kRingPhases[0].startSec - w.time;
    return s;
  }
  s.phase = i;
  s.dmgPerSec = kRingPhases[i].dmgPerSec;
  const float t = (w.time - kRingPhases[i].startSec) / kRingPhases[i].shrinkSec;
  if (t < 1.0f) {
    s.shrinking = true;
    s.cur = {lerpV(w.zones[i].c, w.zones[i + 1].c, t), w.zones[i].r + (w.zones[i + 1].r - w.zones[i].r) * t};
    s.next = w.zones[i + 1];
    s.secsLeft = kRingPhases[i].startSec + kRingPhases[i].shrinkSec - w.time;
  } else {
    s.cur = w.zones[i + 1];
    s.next = i + 1 < n ? w.zones[i + 2] : w.zones[i + 1];
    s.secsLeft = i + 1 < n ? kRingPhases[i + 1].startSec - w.time : 0.0f;
  }
  return s;
}

inline bool insideZone(const Zone& z, Vec2 p) { return (p - z.c).len() <= z.r; }

// ---- Visibility (Invisibility) ----
// An invisible tank can't be seen, except right after it fires: then everyone knows roughly where the shot came from.
inline bool isHidden(const Tank& t) { return t.active == Power::Invisible; }
inline bool canSpot(const Tank& t) { return !t.ghost && (!isHidden(t) || t.revealT > 0); }
inline Vec2 seenPos(const Tank& t) { return isHidden(t) ? t.lastShotPos : t.pos; }

// ---- Superpower helpers ----
inline void endActive(Tank& t, bool startCooldown = true) {
  const Power p = t.active;
  if (p == Power::None) return;
  t.active = Power::None;
  t.activeLeft = 0;
  if (p == Power::Instinct) { t.dodges = 0; t.pendingDmg = 0; t.pendingT = 0; }
  if (startCooldown) t.recharge = powerInfo(p).cooldown * t.cdScale;
}

inline void killTank(World& w, Tank& v, uint32_t killer);

// ---- Arena Temples: going in and out ----
// Moves a tank (and its drones and Sentry Robots) to `to` in area `temple` (-1 = the map).
inline void moveArea(World& w, Tank& t, int temple, Vec2 to) {
  t.temple = (int8_t)temple;
  t.pos = to;
  t.vel = {};
  t.mind.wp = 0;
  t.mind.goal = to;
  t.mind.stuckPos = to;
  for (Drone& d : w.drones) if (d.owner == t.id) { d.pos = to; d.goal = to; }
  int n = 0;
  for (Tank& s : w.tanks)
    if (s.alive && s.sentryOf == t.id) {
      const float a = 1.0f + 1.3f * n++;
      s.temple = (int8_t)temple; s.pos = to + Vec2{std::cos(a), std::sin(a)} * 70.0f; s.vel = {};
      s.mind.goal = s.pos; s.mind.stuckPos = s.pos;
    }
}
// Just outside a temple's pillars, on the side facing the middle of the map.
inline Vec2 templeOutside(World& w, const Temple& T) {
  Vec2 d = (Vec2{kMapSize / 2, kMapSize / 2} - T.door).norm();
  for (int k = 0; k < 12; ++k) {
    const float a = std::atan2(d.y, d.x) + (k / 2) * 0.5f * (k % 2 ? 1.0f : -1.0f);
    const Vec2 p = T.door + Vec2{std::cos(a), std::sin(a)} * 360.0f;
    if (!w.inWall(p, 40.0f)) return p;
  }
  return T.door + d * 360.0f;
}
// The Mage's portal: a random spot just inside the edge of the safe zone.
inline Vec2 portalTarget(World& w, const Temple& T) {
  const Zone z = w.ringEnabled ? w.ring.cur : Zone{{kMapSize / 2, kMapSize / 2}, 5000.0f};
  const float r = std::max(0.0f, std::min(z.r, kMapSize) - 260.0f);
  for (int k = 0; k < 24; ++k) {
    const float a = w.rng.range(0, 6.2831853f);
    Vec2 p = z.c + Vec2{std::cos(a), std::sin(a)} * r;
    p.x = std::clamp(p.x, 300.0f, kMapSize - 300.0f);
    p.y = std::clamp(p.y, 300.0f, kMapSize - 300.0f);
    if (!w.inWall(p, 50.0f) && (p - z.c).len() <= z.r) return p;
  }
  return templeOutside(w, T);
}
inline void enterTemple(World& w, Tank& t, int k) {
  const Temple& T = w.temples[k];
  int inside = 0;
  for (const Tank& o : w.tanks) inside += o.alive && o.temple == k && !npc(o) ? 1 : 0;
  const float a = inside * 2.4f;
  w.emit(EvType::TempleEnter, T.door, 0, t.id, (uint32_t)k);
  moveArea(w, t, k, T.entry + Vec2{std::cos(a), std::sin(a)} * (inside ? 40.0f : 0.0f));
  t.templeT = 0;
  t.templeHp = t.health;
}
// reason: 0 the Mage's portal, 1 knocked out, 2 out of time, 3 the temple closed.
inline void leaveTemple(World& w, Tank& t, int reason) {
  const int k = t.temple;
  if (k < 0 || k >= (int)w.temples.size()) return;
  const Temple& T = w.temples[k];
  const Vec2 to = reason == 0 || reason == 3 ? portalTarget(w, T) : templeOutside(w, T);
  moveArea(w, t, -1, to);
  w.emit(EvType::TempleExit, to, (float)t.templeKicks, t.id, (uint32_t)reason);
}

inline void applyDamage(World& w, Tank& v, float dmg, uint32_t killer) {
  v.health -= dmg;
  v.unharmed = 0;
  if (v.health > 0) return;
  // Knocked out inside a temple: thrown out with half the health you came in with. After the second time, the temples
  // won't let you in again.
  if (v.temple >= 0 && !npc(v)) {
    v.templeKicks++;
    v.health = std::max(1.0f, v.templeHp * 0.5f);
    v.stunT = v.silenceT = v.slowT = 0;
    v.pendingDmg = v.pendingT = 0;
    leaveTemple(w, v, 1);
    return;
  }
  killTank(w, v, killer);
}

// All damage goes through here: Shield blocks everything, Instinct holds bullet/explosion hits for a dodge, Repair
// (held) makes you take more, guardians get angry at whoever hurt them.
inline void damageTank(World& w, Tank& v, float dmg, uint32_t killer, bool storm = false) {
  if (!v.alive || dmg <= 0 || v.id == w.godId) return;
  if (v.active == Power::Shield) return;
  Tank* k = killer && killer != v.id ? w.find(killer) : nullptr;
  if (k && !hostile(*k, v)) return;  // no friendly fire (guardians, a Sentry Robot and its owner)
  if (k && !w.links.empty() && w.teamed(k->id, v.id)) return;  // teamed up with a blue line in the dev panel
  if (v.guardian) {
    if (storm) return;  // guardians hold the middle; the storm doesn't wear them down
    if (killer && killer != v.id) { v.aggroId = killer; v.aggroT = 10.0f; }
  }
  if (killer && killer != v.id) v.lastHitBy = killer;
  if (w.mode == Mode::Rampage && k && !storm) {  // score: damage dealt (to the attacker's side)
    Tank* s = k->sentryOf ? w.find(k->sentryOf) : k;
    if (s && v.active != Power::Instinct) s->score += (int32_t)std::lround(std::min(dmg * (v.repairing ? kRepairHoldDamage : 1.0f), v.health));
  }
  if (!storm && v.active == Power::Instinct && v.dodges > 0.001f) {
    v.pendingDmg += dmg;
    if (v.pendingT <= 0) v.pendingT = v.dodgeWindow;
    v.pendingBy = killer;
    return;
  }
  if (v.repairing) dmg *= kRepairHoldDamage;
  applyDamage(w, v, dmg, killer);
}

inline void destroyDrone(World& w, Drone& d, bool loseRelic);

// Explosion: tanks inside the blast take damage that falls off toward the edge. Rocket hits take `frac` of the victim's
// max health (up to `cap`); otherwise `dmg` is fixed. The owner is never hurt by their own explosions. Rockets also
// destroy exposed drones.
inline void explode(World& w, Vec2 pos, float R, uint32_t owner, float dmg, float frac = 0, float cap = 0, uint32_t directId = 0,
                    uint32_t kind = 0) {
  const bool rocket = frac > 0;
  const Tank* o = w.find(owner);
  for (Tank& t : w.tanks) {
    if (!t.alive || t.id == owner || (o && !hostile(*o, t))) continue;
    float d = (t.pos - pos).len() - t.radius;
    if (t.id == directId) d = 0;
    if (d > R) continue;
    const float fall = 1.0f - 0.6f * std::clamp(d / R, 0.0f, 1.0f);
    const float base = rocket ? std::min(frac * t.maxHealth, cap) : dmg;
    damageTank(w, t, base * fall, owner);
  }
  for (Drone& d : w.drones)
    if (!d.dead && d.owner != owner && d.vulnerable() && (d.pos - pos).len() < R) destroyDrone(w, d, d.kind == Relic::Loot);
  for (LootBox& b : w.boxes)  // explosions break crates too
    if (b.hp > 0 && (b.pos - pos).len() < R + boxRadius(b.kind)) { b.hp -= rocket ? 90.0f : dmg * 1.5f; b.lastHit = owner; }
  w.emit(EvType::Boom, pos, R, owner, kind);
}

// Rockets blow a hole in any wall they hit or explode next to. Pieces too short to matter crumble too.
constexpr float kWallCarve = 130.0f, kWallMinPiece = 70.0f;
inline void carveWalls(World& w, Vec2 c, float R) {
  std::vector<Wall> keep;
  keep.reserve(w.walls.size() + 2);
  for (const Wall& wl : w.walls) {
    if (wl.kind != 0) { keep.push_back(wl); continue; }  // temple stone doesn't break
    const Vec2 d = wl.b - wl.a;
    const float L = d.len();
    if (L < 1e-3f) continue;
    const Vec2 u = d * (1.0f / L);
    const Vec2 rel = c - wl.a;
    const float s0 = rel.x * u.x + rel.y * u.y, h = std::fabs(rel.x * u.y - rel.y * u.x);
    const float rr = R + wl.thick * 0.5f;
    if (h >= rr) { keep.push_back(wl); continue; }
    const float half = std::sqrt(rr * rr - h * h);
    const float lo = std::max(0.0f, s0 - half), hi = std::min(L, s0 + half);
    if (hi <= lo) { keep.push_back(wl); continue; }
    if (lo >= kWallMinPiece) keep.push_back({wl.a, wl.a + u * lo, wl.thick});
    if (L - hi >= kWallMinPiece) keep.push_back({wl.a + u * hi, wl.b, wl.thick});
    const float gapLo = lo >= kWallMinPiece ? lo : 0.0f, gapHi = L - hi >= kWallMinPiece ? hi : L;
    w.emit(EvType::WallBreak, wl.a + u * ((gapLo + gapHi) * 0.5f), gapHi - gapLo, (uint32_t)(int)(std::atan2(u.y, u.x) * 57.2958f + 360.0f));
  }
  w.walls.swap(keep);
}

inline void removeRelic(Tank& t, int slot) {
  if (slot == 0) { t.relic[0] = t.relic[1]; t.relicHold[0] = t.relicHold[1]; t.relicPrev[0] = t.relicPrev[1]; }
  t.relic[1] = Relic::None;
  t.relicHold[1] = 0;
  t.relicPrev[1] = false;
}

inline void destroyDrone(World& w, Drone& d, bool loseRelic) {
  if (d.dead) return;
  d.dead = true;
  if (d.carryStat >= 0) w.spawnUpgrade(d.pos, (int)d.carryRarity, d.carryStat);
  Tank* o = w.find(d.owner);
  if (o) {
    if (d.kind == Relic::Sentry) o->sentryRebuild = kSentryRebuild;
    if (loseRelic) for (int s = 1; s >= 0; --s) if (o->relic[s] == d.kind) { removeRelic(*o, s); break; }
  }
  w.emit(EvType::DroneLost, d.pos, 0, d.owner, (uint32_t)d.kind);
}

// One of the three guardians drops a big reward for the tank that beat it (GAME_DESIGN.md section 7). The Arena Mage
// always does, with better odds of a Mythic.
inline void guardianDrop(World& w, const Tank& g, uint32_t killer) {
  Tank* k = w.find(killer);
  if (k && k->sentryOf) k = w.find(k->sentryOf);
  const Vec2 p = g.pos;
  const bool mage = g.temple >= 0;
  if ((g.id != w.specialGuardian && !mage) || !k || arenaSide(*k)) {
    w.spawnUpgrade(p, (int)Rarity::Epic);
    return;
  }
  killer = k->id;
  int missing[kStatCount], nm = 0;
  for (int s = 0; s < kRoyaleStats; ++s) if (k->stat[s] < kMaxPips) missing[nm++] = s;
  const bool slotFree = k->relic[1] == Relic::None;
  uint32_t what = 0;  // 1 legendary upgrade, 2 relic, 3 mythic
  if ((nm == 0 && !slotFree) || w.rng.f01() < (mage ? 0.35f : 0.2f)) {
    w.spawnCrate(p, kMythics[w.rng.next() % 3]);
    what = 3;
  } else if (nm > 0 && (!slotFree || w.rng.f01() < 0.65f)) {
    w.spawnUpgrade(p, (int)Rarity::Legendary, missing[w.rng.next() % nm]);
    if (mage && nm > 1 && w.rng.f01() < 0.5f) {  // the Mage often adds a second one
      int other = missing[w.rng.next() % nm];
      w.spawnUpgrade(p + Vec2{40, 0}, (int)Rarity::Legendary, other);
    }
    what = 1;
  } else {
    Relic r = Relic::None;
    for (int tries = 0; tries < 10 && (r == Relic::None || hasRelic(*k, r)); ++tries) r = (Relic)(1 + w.rng.next() % kRelicKinds);
    w.spawnRelic(p, r);
    what = 2;
  }
  w.emit(EvType::GuardianDrop, p, 0, killer, what);
}

// A tank is out: it drops every upgrade tier it had, its superpower, and its relics (one relic is destroyed).
inline void killTank(World& w, Tank& v, uint32_t killer) {
  v.alive = false;
  v.health = 0;
  v.deathTimer = 0;
  v.killedBy = killer;
  v.diedAt = w.time;
  if (!npc(v)) v.place = (uint32_t)w.aliveCount() + 1;  // everyone still alive finishes ahead of you
  endActive(v, false);
  v.recharge = 0;
  v.stunT = v.silenceT = 0;
  const Vec2 p = v.pos;
  if (v.sentryOf || v.mob) {  // a Sentry Robot self-destructs; neither drops anything
    if (v.sentryOf) explode(w, p, kBotBlast, v.sentryOf, kBotBlastDamage, 0, 0, 0, 5);
    if (v.mob)
      for (Tank& k : w.tanks)
        if (k.id == killer && k.alive) {
          Tank* c = k.sentryOf ? w.find(k.sentryOf) : &k;
          if (c) c->xp += kMobXp;
        }
    return;
  }
  for (Tank& s : w.tanks)  // your Sentry Robots go with you
    if (s.alive && s.sentryOf == v.id) killTank(w, s, 0);
  int n = 0;
  auto around = [&]() { const float a = n++ * 1.3f; return p + Vec2{std::cos(a), std::sin(a)} * (30.0f + 9.0f * n); };
  for (int s = 0; s < kStatCount; ++s)
    if (v.stat[s] > 0) w.spawnUpgrade(around(), v.stat[s] - 1, s);
  if (v.held != Power::None && !isExclusive(v.held) && w.mode != Mode::Rampage) w.spawnCrate(around(), v.held);
  Relic rs[2];
  int nr = 0;
  for (Relic r : v.relic) if (r != Relic::None) rs[nr++] = r;
  if (nr > 0) {
    const int lost = (int)(w.rng.next() % nr);
    for (int i = 0; i < nr; ++i) if (i != lost) w.spawnRelic(around(), rs[i]);
  }
  v.held = Power::None;
  v.relic[0] = v.relic[1] = Relic::None;
  for (uint8_t& s : v.stat) s = 0;
  v.maxHealth = v.guardian ? kGuardHealth : kMaxHealth;
  for (Drone& d : w.drones) if (d.owner == v.id) { d.dead = true; if (d.carryStat >= 0) { w.spawnUpgrade(d.pos, (int)d.carryRarity, d.carryStat); d.carryStat = -1; } }
  for (Turret& t : w.turrets) if (t.owner == v.id) t.life = 0;
  for (Singularity& s : w.sings) if (s.owner == v.id) s.life = 0;
  if (v.guardian) {
    guardianDrop(w, v, killer);
    if (v.temple >= 0 && v.temple < (int)w.temples.size()) {  // the Mage is beaten: the portal opens, the temple closes
      Temple& T = w.temples[v.temple];
      T.cleared = true;
      T.clearT = 0;
      w.walls.erase(std::remove_if(w.walls.begin(), w.walls.end(), [&](const Wall& wl) { return wl.kind == 2 && wl.temple == v.temple; }),
                    w.walls.end());
      w.emit(EvType::TempleCleared, T.c, 0, killer, (uint32_t)v.temple);
    }
  }
  const Tank* kt = w.find(killer);
  const uint32_t credit = kt && kt->sentryOf ? kt->sentryOf : killer;  // a Sentry Robot's knockouts count for its owner
  for (Tank& k : w.tanks) {
    if (k.id == credit && k.id != v.id && k.alive) {
      k.kills++;
      k.xp += 50;
      if (w.mode == Mode::Rampage) k.score += kRampageKill;
      if (!npc(k)) k.health = std::min(k.maxHealth, k.health + kKillHeal);
    }
  }
}

// ---- Loot crates ----
// Rarer upgrades are less likely. A crate holds 3 things: at least 2 upgrades of different stats, and at most one
// superpower or one relic (never both). Small temple crates hold 1-2 upgrades.
inline Rarity boxRarity(World& w, BoxKind k) {
  static const float wN[5] = {45, 30, 16, 7, 2}, wL[5] = {4, 14, 30, 32, 20}, wS[5] = {50, 32, 14, 4, 0};
  const float* wt = k == BoxKind::Legendary ? wL : k == BoxKind::Small ? wS : wN;
  float tot = 0;
  for (int i = 0; i < 5; ++i) tot += wt[i];
  float v = w.rng.f01() * tot;
  for (int i = 0; i < 5; ++i) { v -= wt[i]; if (v <= 0) return (Rarity)i; }
  return Rarity::Common;
}
inline void openBox(World& w, const LootBox& b) {
  const bool legend = b.kind == BoxKind::Legendary;
  int nUp = 2, extra = 0;  // extra: 0 a third upgrade, 1 a superpower, 2 a relic
  if (b.kind == BoxKind::Small) {
    nUp = w.rng.f01() < 0.5f ? 1 : 2;
    extra = -1;
  } else {
    const float r = w.rng.f01();
    const float up = legend ? 0.25f : 0.58f, pw = legend ? 0.75f : 0.88f;  // Normal: 58% / 30% / 12%. Legendary: 25% / 50% / 25%
    extra = r < up ? 0 : r < pw ? 1 : 2;
    if (extra == 0) nUp = 3;
  }
  const int total = nUp + (extra > 0 ? 1 : 0);
  int n = 0;
  auto spot = [&]() { const float a = 0.6f + n++ * 6.2831853f / total; return b.pos + Vec2{std::cos(a), std::sin(a)} * (boxRadius(b.kind) + 18.0f); };
  int used[kStatCount] = {}, nUsed = 0;
  for (int i = 0; i < nUp; ++i) {
    int s = (int)(w.rng.next() % kRoyaleStats);
    for (int t = 0; t < kRoyaleStats && used[s]; ++t) s = (s + 1) % kRoyaleStats;  // every upgrade a different stat
    used[s] = 1; nUsed++;
    w.spawnUpgrade(spot(), (int)boxRarity(w, b.kind), s);
  }
  if (extra == 1) w.spawnCrate(spot(), legend && w.rng.f01() < 0.2f ? kMythics[w.rng.next() % 3] : Power::None);
  if (extra == 2) w.spawnRelic(spot());
  w.emit(EvType::BoxBroken, b.pos, (float)b.kind, b.lastHit, (uint32_t)b.kind);
}

// ---- Sentry Robot ----
// The bot gets better the longer it survives: Newbie, then Rookie, Intermediate and Skilled.
inline void setBotTier(Tank& me, BotTier tier);

// Fires the tank's gun (or Spread Shot), honoring fire-rate tiers and Rapid Shot.
inline void fireWeapon(World& w, Tank& t) {
  const WeaponParams p = t.guardian ? kGuardWeapon : weaponParams(kBaseWeapons[(size_t)t.weapon]);
  const float sc = t.radius / kTankRadius;
  const bool spread = t.active == Power::Spread;
  const bool storm = t.ringShots > 1;  // admin Bullet Storm: all the way around
  const int n = storm ? t.ringShots : spread ? kSpreadCount : 1;
  for (int i = 0; i < n; ++i) {
    const float a = storm ? t.aim + 6.2831853f * i / n : t.aim + (n > 1 ? (-kSpreadHalfAngle + 2.0f * kSpreadHalfAngle * i / (n - 1)) : 0.0f);
    const Vec2 dir{std::cos(a), std::sin(a)};
    Bullet b;
    b.id = w.newId();
    b.owner = t.id;
    b.pos = t.pos + dir * (kBarrelLength * sc);
    const bool rapid = t.active == Power::RapidShot;
    b.vel = dir * (p.bulletSpeed * statBulletSpeedMul(t.stat[StatBulletSpeed]) * (rapid ? kRapidShotBulletMul : 1.0f) * t.powerMul);
    b.damage = p.damage * statDamageMul(t.stat[StatDamage]) * (spread ? kSpreadDamageMul : 1.0f) * t.powerMul;
    b.radius = p.bulletRadius * (spread ? kSpreadRadiusMul : 1.0f);
    b.rangeLeft = b.range0 = p.range * statRangeMul(t.stat[StatRange]) * t.powerMul;
    b.kind = t.guardian ? BulletKind::Guardian : BulletKind::Normal;
    if (t.mob) b.damage *= kMobDamageMul;
    if (t.superBounce) b.maxBounce = 25;
    w.bullets.push_back(b);
  }
  t.cooldown = p.reloadSec * statReloadMul(t.stat[StatFireRate]) / (t.active == Power::RapidShot ? kRapidShotMul : 1.0f) / t.powerMul;
  if (isHidden(t)) { t.revealT = 1.2f; t.lastShotPos = t.pos; }
}

// Clamps a target point to what the tank can see on screen and to its area (the map, or inside its temple).
inline Vec2 inView(const World& w, const Tank& t, Vec2 p) {
  p.x = std::clamp(p.x, t.pos.x - kViewHalfW, t.pos.x + kViewHalfW);
  p.y = std::clamp(p.y, t.pos.y - kViewHalfH, t.pos.y + kViewHalfH);
  if (t.temple >= 0 && t.temple < (int)w.temples.size()) {
    const Temple& T = w.temples[t.temple];
    const Vec2 d = p - T.c;
    if (d.len() > T.R - 60.0f) p = T.c + d.norm() * (T.R - 60.0f);
    return p;
  }
  p.x = std::clamp(p.x, 40.0f, kMapSize - 40.0f);
  p.y = std::clamp(p.y, 40.0f, kMapSize - 40.0f);
  return p;
}

// E / Shift: use the superpower (or, during Instinct, dodge the hit that just landed).
inline void usePower(World& w, Tank& t, const Input& in) {
  if (t.active == Power::Instinct) {
    if (t.pendingT > 0) {  // dodged: the hit never happens
      t.pendingDmg = 0;
      t.pendingT = 0;
      t.dodges -= 1.0f - kInstinctRefund;
      t.dodgeWindow = std::max(kInstinctMinWindow, t.dodgeWindow - kInstinctShrink);
      w.emit(EvType::Dodged, t.pos, 0, t.id);
      if (t.dodges <= 0.001f) endActive(t);
    }
    return;
  }
  if (t.held == Power::None || t.active != Power::None || t.recharge > 0) return;
  const Power p = t.held;
  const PowerInfo info = powerInfo(p);
  const Vec2 aimDir{std::cos(t.aim), std::sin(t.aim)};
  const Vec2 cur = t.cursor;
  switch (p) {
    case Power::Speed: case Power::Shield: case Power::RapidShot: case Power::Spread: case Power::Invisible:
      t.active = p;
      t.activeLeft = info.duration;
      if (p == Power::Invisible) t.revealT = 0;
      return;
    case Power::Dash: {
      const Vec2 mv = Vec2{(float)in.moveX, (float)in.moveY}.norm();
      t.dashDir = mv.len() > 0 ? mv : aimDir;
      t.active = p;
      t.activeLeft = info.duration;
      return;
    }
    case Power::Rocket: case Power::Homing: {
      Rocket r;
      r.id = w.newId();
      r.owner = t.id;
      // Rocket Rampage: it leaves from where it sits on the tank (it is the barrel there).
      r.pos = t.pos + aimDir * ((w.mode == Mode::Rampage ? 30.0f : kBarrelLength) * t.radius / kTankRadius);
      r.homing = p == Power::Homing;
      r.vel = aimDir * (r.homing ? kHomingSpeed : kRocketSpeed);
      r.life = kHomingLife;
      r.rangeLeft = kRocketRange;
      w.rockets.push_back(r);
      if (w.mode == Mode::Rampage) {  // Rocket Rampage: the cooldown starts the moment it's fired
        t.recharge = info.cooldown * t.cdScale;
        return;
      }
      t.active = p;  // active until the rocket explodes; then the cooldown starts
      t.activeLeft = info.duration;
      return;
    }
    case Power::AirStrike: {
      Strike s;
      s.id = w.newId();
      s.owner = t.id;
      s.target = inView(w, t, cur);
      Vec2 d = (s.target - t.pos).norm();
      if (d.len() < 0.5f) d = aimDir;
      s.dir = d;
      s.start = s.target - d * kPlaneLead;
      const Vec2 side{-d.y, d.x};
      for (int k = 0; k < kStrikeBombs; ++k) {  // scattered around the target, released as the plane passes over
        s.along[k] = kPlaneLead - 150.0f + 100.0f * k + w.rng.range(-35.0f, 35.0f);
        s.drop[k] = s.start + d * s.along[k] + side * w.rng.range(-kBombScatter, kBombScatter);
      }
      w.strikes.push_back(s);
      t.active = p;
      t.activeLeft = info.duration;
      return;
    }
    case Power::Lightning:
      for (int i = 0; i < kBolts; ++i) {
        const float a = t.aim + i * (2 * kPi / kBolts);
        const Vec2 d{std::cos(a), std::sin(a)};
        Bullet b;
        b.id = w.newId(); b.owner = t.id; b.pos = t.pos + d * t.radius; b.vel = d * kBoltSpeed;
        b.damage = kBoltDamage; b.radius = 10; b.rangeLeft = b.range0 = kBoltRange; b.kind = BulletKind::Bolt;
        w.bullets.push_back(b);
      }
      t.recharge = info.cooldown;
      return;
    case Power::Turret: {
      for (Turret& old : w.turrets) if (old.owner == t.id) old.life = 0;  // only one turret each
      Turret tu;
      tu.id = w.newId(); tu.owner = t.id; tu.pos = t.pos; tu.aim = t.aim;
      w.turrets.push_back(tu);
      t.active = p;
      t.activeLeft = info.duration;
      return;
    }
    case Power::Teleport: {
      Vec2 to = inView(w, t, cur);
      for (int k = 0; k < 8 && w.inWall(to, t.radius); ++k) to = to + (t.pos - to).norm() * 30.0f;
      w.emit(EvType::Teleport, t.pos, 0, t.id);
      t.pos = to;
      t.vel = {};
      w.emit(EvType::Teleport, t.pos, 1, t.id);
      t.recharge = info.cooldown;
      return;
    }
    case Power::Singularity: {
      Singularity s;
      s.id = w.newId(); s.owner = t.id; s.pos = inView(w, t, cur);
      w.sings.push_back(s);
      t.active = p;
      t.activeLeft = info.duration;
      return;
    }
    case Power::Instinct:
      t.active = p;
      t.dodges = kInstinctDodges;
      t.dodgeWindow = kInstinctWindow;
      return;
    case Power::CocoBarrage: {  // admin: coconuts rain down in a fan toward the cursor
      const Vec2 to = inView(w, t, cur);
      Vec2 d = (to - t.pos).norm();
      if (d.len() < 0.5f) d = aimDir;
      const Vec2 side{-d.y, d.x};
      const float reach = std::max(250.0f, (to - t.pos).len());
      for (int k = 0; k < kCocoCount; ++k) {
        Bomb b;
        b.id = w.newId(); b.owner = t.id; b.kind = 1;
        const float f = (float)k / (kCocoCount - 1);
        b.pos = t.pos + d * (reach * (0.55f + 0.5f * w.rng.f01())) + side * ((f - 0.5f) * 2.0f * kCocoSpread);
        b.fuse = 0.25f + 0.07f * k;
        b.blast = kCocoBlast; b.dmg = kCocoDamage;
        w.bombs.push_back(b);
      }
      t.recharge = info.cooldown * t.cdScale;
      return;
    }
    case Power::IvySnare: {  // admin: vines root everyone around the cursor
      const Vec2 at = inView(w, t, cur);
      for (Tank& o : w.tanks)
        if (o.alive && hostile(t, o) && o.temple == t.temple && (o.pos - at).len() < kIvyRadius + o.radius) {
          o.stunT = std::max(o.stunT, kIvyRoot);
          o.slowT = std::max(o.slowT, kIvySlow);
          o.vel = {};
        }
      w.emit(EvType::Snare, at, kIvyRadius, t.id);
      t.recharge = info.cooldown * t.cdScale;
      return;
    }
    case Power::SentryBot: {  // a grey copy of you: your upgrades right now, 25% less health, no relics or superpowers
      Tank s;
      s.id = w.nextSentry++;
      s.isBot = true;
      s.sentryOf = t.id;
      s.temple = t.temple;
      s.weapon = t.weapon;
      for (int k = 0; k < kStatCount; ++k) s.stat[k] = t.stat[k];
      s.maxHealth = s.health = t.maxHealth * kBotHealthMul;
      s.aim = t.aim;
      const Vec2 side{-aimDir.y, aimDir.x};
      s.pos = t.pos - aimDir * 75.0f;
      for (Vec2 o : {side * 75.0f, side * -75.0f, aimDir * 75.0f})
        if (w.inWall(s.pos, s.radius)) s.pos = t.pos + o;
      w.pending.push_back(s);
      t.active = p;
      t.activeLeft = info.duration;
      return;
    }
    default: return;
  }
}

// ---- Relics: R/T keys (tap vs hold), Repair healing, drones ----
inline void relicInput(World& w, Tank& t, const Input& in) {
  t.repairing = false;
  t.sentryOver = false;
  for (int s = 0; s < 2; ++s) {
    const bool down = in.relicDown[s];
    const Relic r = t.relic[s];
    if (down) t.relicHold[s] += kDt;
    const bool released = t.relicPrev[s] && !down;
    const float held = t.relicHold[s];
    if (r == Relic::Repair && down) t.repairing = true;
    if (r == Relic::Sentry) {
      if (released && held < kTapTime) t.sentryOn = !t.sentryOn;
      if (down && held >= kTapTime) t.sentryOver = true;
    }
    if (r == Relic::Loot && down && held >= kLootHold && held - kDt < kLootHold) {
      for (Drone& d : w.drones)
        if (!d.dead && d.owner == t.id && d.kind == Relic::Loot && d.mode != DroneMode::Kamikaze) {
          d.mode = DroneMode::Kamikaze;
          d.goal = inView(w, t, t.cursor);
          d.timer = 3.0f;
        }
    }
    if (!down) t.relicHold[s] = 0;
    t.relicPrev[s] = down;
  }
  if (hasRelic(t, Relic::Repair)) {
    if (t.unharmed > kRepairDelay || t.repairing) t.health = std::min(t.maxHealth, t.health + kRepairRate * kDt);
    if (t.repairing) {
      t.repairAcc += kDt;
      if (t.repairAcc >= kRepairHoldEvery) { t.repairAcc -= kRepairHoldEvery; t.health = std::min(t.maxHealth, t.health + kRepairHoldHeal); }
    } else {
      t.repairAcc = 0;
    }
  }
}

// Nearest enemy a turret / drone can see (guardians only if they are angry at the owner).
inline Tank* nearestEnemy(World& w, uint32_t owner, Vec2 from, float range) {
  Tank* best = nullptr;
  float bd = range;
  const Tank* me = w.find(owner);
  for (Tank& o : w.tanks) {
    if (!o.alive || o.id == owner || !canSpot(o) || (me && !hostile(*me, o))) continue;
    if (o.guardian && o.aggroId != owner) continue;
    const float d = (seenPos(o) - from).len();
    if (d < bd && !w.lineBlocked(from, seenPos(o))) { bd = d; best = &o; }
  }
  return best;
}

inline void autoShot(World& w, uint32_t owner, Vec2 from, Vec2 at, Vec2 atVel, float speed, float damage, float radius, float range,
                     BulletKind kind, float err, float& aimOut) {
  const float tt = (at - from).len() / speed;
  const Vec2 p = at + atVel * (tt * 0.6f);
  const float a = std::atan2(p.y - from.y, p.x - from.x) + w.rng.range(-err, err);
  aimOut = a;
  Bullet b;
  b.id = w.newId(); b.owner = owner; b.vel = Vec2{std::cos(a), std::sin(a)} * speed; b.pos = from + b.vel.norm() * 18.0f;
  b.damage = damage; b.radius = radius; b.rangeLeft = b.range0 = range; b.kind = kind;
  w.bullets.push_back(b);
}

// Keeps one Sentry and one Loot drone per tank that has those relics.
inline void syncDrones(World& w) {
  for (Drone& d : w.drones) {
    const Tank* o = w.find(d.owner);
    if (!o || !o->alive || !hasRelic(*o, d.kind)) {
      if (!d.dead && d.carryStat >= 0 && o) { w.spawnUpgrade(d.pos, (int)d.carryRarity, d.carryStat); d.carryStat = -1; }
      d.dead = true;
    }
  }
  for (Tank& t : w.tanks) {
    if (!t.alive) continue;
    if (t.sentryRebuild > 0) t.sentryRebuild -= kDt;
    for (Relic k : {Relic::Sentry, Relic::Loot}) {
      if (!hasRelic(t, k) || (k == Relic::Sentry && t.sentryRebuild > 0)) continue;
      bool have = false;
      for (const Drone& d : w.drones) if (!d.dead && d.owner == t.id && d.kind == k) have = true;
      if (have) continue;
      Drone d;
      d.id = w.newId(); d.owner = t.id; d.kind = k; d.pos = t.pos;
      d.hp = k == Relic::Sentry ? kSentryHp : kLootHp;
      d.angle = k == Relic::Sentry ? 0.0f : kPi;
      w.drones.push_back(d);
    }
  }
}

inline void moveTo(Vec2& p, Vec2 goal, float speed) {
  const Vec2 d = goal - p;
  const float l = d.len(), step = speed * kDt;
  p = l <= step ? goal : p + d * (step / l);
}

inline void stepDrones(World& w) {
  for (size_t i = 0; i < w.drones.size(); ++i) {
    Drone& d = w.drones[i];
    if (d.dead) continue;
    Tank* o = w.find(d.owner);
    if (!o) { d.dead = true; continue; }
    d.angle += 2.0f * kDt;
    const Vec2 orbit = o->pos + Vec2{std::cos(d.angle), std::sin(d.angle)} * kSentryOrbit;
    d.cd -= kDt;
    if (d.kind == Relic::Sentry) {
      Tank* tg = o->sentryOn || o->sentryOver ? nearestEnemy(w, o->id, d.pos, kSentryRange + (o->sentryOver ? 300.0f : 0.0f)) : nullptr;
      d.mode = o->sentryOver ? DroneMode::Chase : DroneMode::Orbit;
      if (d.mode == DroneMode::Chase && tg) {
        const Vec2 to = seenPos(*tg) - d.pos;
        moveTo(d.pos, to.len() > 220.0f ? seenPos(*tg) - to.norm() * 220.0f : d.pos, 520.0f);
      } else {
        moveTo(d.pos, orbit, 900.0f);
      }
      if (tg && d.cd <= 0) {
        autoShot(w, o->id, d.pos, seenPos(*tg), tg->vel, 750.0f, kSentryDamage, 6.0f, 650.0f, BulletKind::Drone, 0.07f, d.aim);
        d.cd = o->sentryOver ? kSentryOverReload : kSentryReload;
      }
      continue;
    }
    // Loot drone.
    switch (d.mode) {
      case DroneMode::Orbit:
      case DroneMode::Chase: {
        moveTo(d.pos, orbit, 900.0f);
        if (d.cd > 0) break;
        d.cd = 0.5f;
        const Upgrade* best = nullptr;
        float bd = kLootRange;
        for (const Upgrade& u : w.upgrades) {
          if ((int)u.rarity + 1 <= o->stat[u.stat]) continue;  // only upgrades the owner can use
          if ((u.pos - o->pos).len() < 160.0f) continue;      // already next to the owner (e.g. one it just delivered)
          bool taken = false;
          for (const Drone& e : w.drones) if (!e.dead && e.fetchId == u.id) taken = true;
          const float dd = (u.pos - o->pos).len();
          if (!taken && dd < bd) { bd = dd; best = &u; }
        }
        if (best) { d.mode = DroneMode::Fetch; d.fetchId = best->id; d.goal = best->pos; }
        break;
      }
      case DroneMode::Fetch: {
        const Upgrade* u = nullptr;
        size_t ui = 0;
        for (size_t k = 0; k < w.upgrades.size(); ++k) if (w.upgrades[k].id == d.fetchId) { u = &w.upgrades[k]; ui = k; }
        if (!u) { d.mode = DroneMode::Return; d.fetchId = 0; break; }
        moveTo(d.pos, u->pos, kLootSpeed);
        if ((d.pos - u->pos).len() < 4.0f) {
          d.carryStat = u->stat; d.carryRarity = u->rarity; d.fetchId = 0;
          w.upgrades[ui] = w.upgrades.back();
          w.upgrades.pop_back();
          d.mode = DroneMode::Return;
        }
        break;
      }
      case DroneMode::Return: {
        const Vec2 drop = o->pos + Vec2{std::cos(d.angle), std::sin(d.angle)} * 55.0f;
        moveTo(d.pos, drop, kLootSpeed);
        if ((d.pos - drop).len() < 4.0f) {
          if (d.carryStat >= 0) w.spawnUpgrade(d.pos, (int)d.carryRarity, d.carryStat);
          d.carryStat = -1;
          d.mode = DroneMode::Orbit;
        }
        break;
      }
      case DroneMode::Kamikaze: {
        moveTo(d.pos, d.goal, 900.0f);
        d.timer -= kDt;
        if ((d.pos - d.goal).len() < 4.0f || d.timer <= 0) {
          const Vec2 at = d.pos;
          const uint32_t owner = d.owner;
          d.dead = true;
          if (d.carryStat >= 0) { w.spawnUpgrade(at, (int)d.carryRarity, d.carryStat); d.carryStat = -1; }
          for (int s = 1; s >= 0; --s) if (o->relic[s] == Relic::Loot) { removeRelic(*o, s); break; }
          explode(w, at, kLootBlast, owner, kLootDamage, 0, 0, 0, 2);
        }
        break;
      }
    }
  }
}

// Smart bot brain and guardian brain live in core/bot.h (included at the bottom of this file).
inline Input botInput(World& w, Tank& me);
inline Input guardianInput(World& w, Tank& g);

// Advance the world by one tick. inputs[i] drives tanks[i] (bots and guardians ignore it).
inline void stepWorld(World& w, const std::vector<Input>& inputs) {
  w.time += kDt;
  w.tickCount++;
  w.ring = ringAt(w);
  w.events.clear();

  // Temples with a visitor in them (nobody inside: their guardians stand still and don't think).
  bool busy[8] = {};
  for (const Tank& t : w.tanks)
    if (t.alive && !arenaSide(t) && t.temple >= 0 && t.temple < 8) busy[t.temple] = true;

  // ---- Tanks: relics, superpower, move, shoot ----
  for (size_t i = 0; i < w.tanks.size(); ++i) {
    Tank& t = w.tanks[i];
    if (!t.alive) continue;
    if (t.sentryOf) {  // a Sentry Robot self-destructs after its time, or when its owner is out
      const Tank* o = w.find(t.sentryOf);
      t.mind.age += kDt;
      if (!o || !o->alive || t.mind.age >= powerInfo(Power::SentryBot).duration) { killTank(w, t, 0); continue; }
    }
    const bool idle = arenaSide(t) && t.temple >= 0 && !busy[std::min((int)t.temple, 7)];
    Input in;
    if (idle) in.aim = t.aim;
    else in = t.guardian ? guardianInput(w, t) : t.isBot ? botInput(w, t) : (i < inputs.size() ? inputs[i] : Input{});
    t.cursor = (in.tx != 0 || in.ty != 0) ? Vec2{in.tx, in.ty} : t.pos + Vec2{std::cos(in.aim), std::sin(in.aim)} * 400.0f;
    t.unharmed += kDt;
    if (t.stunT > 0) {  // Lightning: frozen in place
      t.stunT -= kDt;
      in.moveX = in.moveY = 0;
      in.shoot = in.usePowerUp = in.swap = false;
      in.relicDown[0] = in.relicDown[1] = false;
      in.aim = t.aim;
    }
    if (t.silenceT > 0) {  // Singularity: no shooting, no superpower
      t.silenceT -= kDt;
      in.shoot = in.usePowerUp = false;
    }
    relicInput(w, t, in);
    if (t.swapCd > 0) t.swapCd -= kDt;
    t.swapReq = in.swap && t.swapCd <= 0;
    if (t.active == Power::None && t.recharge > 0) t.recharge = std::max(0.0f, t.recharge - kDt);
    if (in.usePowerUp) {
      const Power was = t.held;
      const float rc = t.recharge;
      const bool idle = t.active == Power::None;
      usePower(w, t, in);
      if (was != Power::None && idle && (t.active != Power::None || t.recharge > rc + 0.01f))
        w.emit(EvType::PowerUsed, t.pos, 0, t.id, (uint32_t)was);
    }

    float speedMul = statSpeedMul(t.stat[StatSpeed]) * t.powerMul;
    if (t.active == Power::Speed) speedMul *= kSpeedBoostMul;
    if (t.repairing) speedMul *= kRepairHoldSlow;
    if (t.slowT > 0) speedMul *= kSingSlow;
    if (t.guardian) speedMul = t.temple >= 0 ? kMageSpeedMul : kGuardSpeedMul;
    Vec2 lo, hi;
    w.areaOf(t.temple, lo, hi);
    stepTank(t, in, speedMul, lo, hi);
    if (t.cooldown > 0) t.cooldown -= kDt;
    if (t.revealT > 0) t.revealT -= kDt;
    if (t.blindT > 0) t.blindT -= kDt;
    if (t.slowT > 0) t.slowT -= kDt;
    if (t.guardian && t.aggroT > 0) t.aggroT -= kDt;

    // Active superpower upkeep.
    if (t.active != Power::None && t.active != Power::Instinct) {
      t.activeLeft -= kDt;
      if (powerMakesThing(t.active)) {  // over once the rocket / strike / turret / singularity is gone
        bool alive = false;
        switch (t.active) {
          case Power::Rocket: case Power::Homing:
            for (const Rocket& r : w.rockets) alive |= r.owner == t.id;
            break;
          case Power::AirStrike:
            for (const Strike& s : w.strikes) alive |= s.owner == t.id;
            for (const Bomb& b : w.bombs) alive |= b.owner == t.id;
            break;
          case Power::Turret:
            for (const Turret& u : w.turrets) alive |= u.owner == t.id && u.life > 0;
            break;
          case Power::SentryBot:
            for (const Tank& s : w.tanks) alive |= s.sentryOf == t.id && s.alive;
            for (const Tank& s : w.pending) alive |= s.sentryOf == t.id;
            break;
          default:
            for (const Singularity& s : w.sings) alive |= s.owner == t.id && s.life > 0;
            break;
        }
        if (!alive || t.activeLeft < -10.0f) endActive(t);
        else t.activeLeft = std::max(t.activeLeft, 0.05f);
      } else if (t.activeLeft <= 0) {
        endActive(t);
      }
    }
    if (t.active == Power::Instinct && t.pendingT > 0) {
      t.pendingT -= kDt;
      if (t.pendingT <= 0) {  // missed the dodge: the hit lands
        const float dmg = t.pendingDmg;
        const uint32_t by = t.pendingBy;
        t.pendingDmg = 0;
        t.dodges -= 1.0f;
        t.dodgeWindow = std::max(kInstinctMinWindow, t.dodgeWindow - kInstinctShrink);
        w.emit(EvType::DodgeMissed, t.pos, 0, t.id);
        if (t.dodges <= 0.001f) endActive(t);
        applyDamage(w, t, dmg, by);
        if (!t.alive) continue;
      }
    }
    if (in.shoot && t.cooldown <= 0 && w.mode != Mode::Rampage) fireWeapon(w, t);  // Rocket Rampage: rockets only
  }
  for (Tank& s : w.pending) w.tanks.push_back(s);  // Sentry Robots deployed this tick
  w.pending.clear();

  // Closing Ring: standing outside the safe circle burns health (not inside a temple).
  if (w.ringEnabled && w.ring.dmgPerSec > 0) {
    for (Tank& t : w.tanks)
      if (t.alive && t.temple < 0 && !insideZone(w.ring.cur, t.pos)) damageTank(w, t, w.ring.dmgPerSec * kDt, 0, true);
  }

  // Walls and crates push tanks out (a couple of passes handles corners) and stop their momentum into them.
  for (int pass = 0; pass < 2; ++pass)
    for (Tank& t : w.tanks) {
      if (!t.alive) continue;
      auto push = [&](Vec2 cp, float minD) {
        const Vec2 d = t.pos - cp;
        const float len = d.len();
        if (len >= minD) return;
        const Vec2 n = len > 1e-3f ? d * (1.0f / len) : Vec2{1, 0};
        t.pos += n * (minD - len);
        const float into = t.vel.x * n.x + t.vel.y * n.y;
        if (into < 0) t.vel = t.vel - n * into;
      };
      for (const Wall& wl : w.walls) {
        if (std::fabs(wl.a.x - t.pos.x) > 2000.0f && std::fabs(wl.b.x - t.pos.x) > 2000.0f) continue;  // far away (quick skip)
        push(closestOnSeg(wl.a, wl.b, t.pos), t.radius + wl.thick * 0.5f);
      }
      for (const LootBox& b : w.boxes) push(b.pos, t.radius + boxRadius(b.kind));
      Vec2 lo, hi;
      w.areaOf(t.temple, lo, hi);
      t.pos.x = std::clamp(t.pos.x, lo.x + t.radius, hi.x - t.radius);
      t.pos.y = std::clamp(t.pos.y, lo.y + t.radius, hi.y - t.radius);
    }

  // ---- Pickups (not for guardians or Sentry Robots) ----
  for (Tank& t : w.tanks) {
    if (!t.alive || npc(t)) continue;
    // Stat upgrades: only worth taking if they raise the stat's tier.
    for (size_t ui = 0; ui < w.upgrades.size();) {
      const Upgrade& u = w.upgrades[ui];
      const float rr = t.radius + kUpgradeRadius;
      const Vec2 d = u.pos - t.pos;
      const int tier = (int)u.rarity + 1;
      if (tier > t.stat[u.stat] && d.x * d.x + d.y * d.y <= rr * rr) {
        const int before = t.stat[u.stat];
        t.stat[u.stat] = (uint8_t)tier;
        t.xp += (uint32_t)(5 * tier);
        if (u.stat == StatHealth) {
          t.maxHealth = kMaxHealth + statHealthBonus(tier);
          t.health = std::min(t.maxHealth, t.health + statHealthBonus(tier) - statHealthBonus(before));
        }
        w.upgrades[ui] = w.upgrades.back();
        w.upgrades.pop_back();
      } else {
        ++ui;
      }
    }
    // Superpowers: drive over one with an empty slot.
    const float reach = t.radius + kCrateRadius;
    for (size_t ci = 0; ci < w.crates.size(); ++ci)
      if (t.held == Power::None && (w.crates[ci].pos - t.pos).len() <= reach) {
        t.held = w.crates[ci].type;
        t.recharge = 0;
        w.crates[ci] = w.crates.back();
        w.crates.pop_back();
        break;
      }
    // Relics: drive over one with a free slot (not one you already have).
    for (size_t ri = 0; ri < w.relics.size(); ++ri) {
      const RelicPickup& r = w.relics[ri];
      if ((r.pos - t.pos).len() > t.radius + kRelicRadius || hasRelic(t, r.kind)) continue;
      const int slot = t.relic[0] == Relic::None ? 0 : (t.relic[1] == Relic::None ? 1 : -1);
      if (slot < 0) continue;
      t.relic[slot] = r.kind;
      if (r.kind == Relic::Sentry) { t.sentryOn = true; t.sentryRebuild = 0; }
      w.relics[ri] = w.relics.back();
      w.relics.pop_back();
      break;
    }
    // F: swap with the closest pickup in reach: a superpower for your superpower, or a relic for your OLDEST relic.
    if (t.swapReq) {
      const float swapReach = t.radius + kCrateRadius + 60.0f;
      int ci = -1, ri = -1;
      float cd = swapReach, rd = swapReach;
      for (size_t k = 0; k < w.crates.size(); ++k) {
        const float d = (w.crates[k].pos - t.pos).len();
        if (t.held != Power::None && w.crates[k].type != t.held && d < cd) { cd = d; ci = (int)k; }
      }
      for (size_t k = 0; k < w.relics.size(); ++k) {
        const float d = (w.relics[k].pos - t.pos).len();
        if (t.relic[1] != Relic::None && !hasRelic(t, w.relics[k].kind) && d < rd) { rd = d; ri = (int)k; }
      }
      if (ci >= 0 && (ri < 0 || cd <= rd)) {
        const bool busy = t.active != Power::None || t.recharge > 0;  // no free refresh by swapping
        endActive(t, false);
        const Power old = t.held;
        t.held = w.crates[ci].type;
        t.recharge = busy ? powerInfo(t.held).cooldown * 0.5f : 0.0f;
        w.crates[ci].type = old;
        t.swapCd = 1.0f;
      } else if (ri >= 0) {
        const Relic old = t.relic[0];
        removeRelic(t, 0);
        t.relic[1] = w.relics[ri].kind;
        if (t.relic[1] == Relic::Sentry) { t.sentryOn = true; t.sentryRebuild = 0; }
        w.relics[ri].kind = old;
        t.swapCd = 1.0f;
      }
      t.swapReq = false;
    }
  }

  // ---- Arena Temples: step on a portal to go in; the Mage's portal takes you out; the clock runs while you're inside ----
  for (size_t ti = 0; ti < w.tanks.size(); ++ti) {
    Tank& t = w.tanks[ti];
    if (!t.alive || npc(t) || w.mode != Mode::Royale) continue;
    if (t.temple < 0) {
      t.deniedT = std::max(0.0f, t.deniedT - kDt);
      for (size_t k = 0; k < w.temples.size(); ++k)
        if (!w.temples[k].cleared && (t.pos - w.temples[k].door).len() < kPortalR + t.radius * 0.5f) {
          if (t.templeKicks < kTempleKicks) enterTemple(w, t, (int)k);
          else if (t.deniedT <= 0) { w.emit(EvType::TempleDenied, t.pos, 0, t.id); t.deniedT = 4.0f; }
          break;
        }
      continue;
    }
    if (t.temple >= (int)w.temples.size()) continue;
    const Temple& T = w.temples[t.temple];
    t.templeT += kDt;
    const bool onPortal = T.cleared && (t.pos - T.c).len() < kExitR;
    t.portalT = onPortal ? t.portalT + kDt : 0.0f;
    if (t.portalT >= kExitHold) { t.portalT = 0; leaveTemple(w, t, 0); }
    else if (t.templeT >= kTempleTime) {  // too slow: half your health, and out you go
      t.health = std::max(1.0f, t.health * 0.5f);
      leaveTemple(w, t, 2);
    }
  }
  for (size_t k = 0; k < w.temples.size(); ++k) {
    Temple& T = w.temples[k];
    if (!T.cleared || T.clearT > kTempleClose) continue;
    T.clearT += kDt;
    if (T.clearT <= kTempleClose) continue;
    for (Tank& t : w.tanks) {  // the temple closes: everyone still inside is sent out, its guardians are gone
      if (!t.alive || t.temple != (int)k) continue;
      if (arenaSide(t)) { t.alive = false; t.health = 0; continue; }
      if (!t.sentryOf) leaveTemple(w, t, 3);
    }
  }

  // Shapes drift, spin, bounce off the map edge, and fade their hit flash.
  for (Shape& s : w.shapes) {
    s.pos += s.vel * kDt;
    s.rot += s.spin * kDt;
    if (s.hitFlash > 0) s.hitFlash -= kDt;
    float r = shapeInfo(s.kind).radius;
    if (s.pos.x < r || s.pos.x > kMapSize - r) s.vel.x = -s.vel.x;
    if (s.pos.y < r || s.pos.y > kMapSize - r) s.vel.y = -s.vel.y;
  }

  // ---- Rockets ----
  for (size_t i = 0; i < w.rockets.size();) {
    Rocket& r = w.rockets[i];
    const Tank* o = w.find(r.owner);
    if (r.homing && o) {  // steer toward the owner's cursor
      const Vec2 want = (o->cursor - r.pos).norm();
      const float a = std::atan2(r.vel.y, r.vel.x), b = std::atan2(want.y, want.x);
      const float na = a + std::clamp(angleDiff(a, b), -kHomingTurn * kDt, kHomingTurn * kDt);
      if (want.len() > 0.5f) r.vel = Vec2{std::cos(na), std::sin(na)} * kHomingSpeed;
    }
    const Vec2 step = r.vel * kDt;
    r.pos += step;
    r.rangeLeft -= step.len();
    r.life -= kDt;
    bool boom = false;
    uint32_t direct = 0;
    if (!w.inAnyArea(r.pos) || w.inWall(r.pos, 6.0f)) boom = true;
    if (!boom)
      for (const Tank& t : w.tanks)
        if (t.alive && t.id != r.owner && (!o || hostile(*o, t)) && (t.pos - r.pos).len() < t.radius + 10.0f) { boom = true; direct = t.id; break; }
    if (!boom)
      for (const LootBox& b : w.boxes)
        if ((b.pos - r.pos).len() < boxRadius(b.kind) + 8.0f) { boom = true; break; }
    if (!boom)
      for (Drone& d : w.drones)
        if (!d.dead && d.owner != r.owner && (d.pos - r.pos).len() < 22.0f) { boom = true; break; }
    if (!boom && r.homing && (r.life <= 0 || !o || std::fabs(r.pos.x - o->pos.x) > kViewHalfW || std::fabs(r.pos.y - o->pos.y) > kViewHalfH)) boom = true;
    if (!boom && !r.homing && r.rangeLeft <= 0) boom = true;
    if (boom) {
      const Rocket copy = r;
      w.rockets[i] = w.rockets.back();
      w.rockets.pop_back();
      const bool ramp = w.mode == Mode::Rampage;
      explode(w, copy.pos, kRocketBlast, copy.owner, 0, ramp ? kRampageFrac : copy.homing ? kHomingFrac : kRocketFrac,
              ramp ? 1000.0f : copy.homing ? kHomingCap : kRocketCap,
              direct, 1);
      carveWalls(w, copy.pos, kWallCarve);
    } else {
      ++i;
    }
  }

  // ---- Air strikes: a plane flies over the target and drops 4 bombs around it; each goes off after a short fuse ----
  for (size_t i = 0; i < w.strikes.size();) {
    Strike& s = w.strikes[i];
    s.t += kDt;
    const float dist = s.t * kPlaneSpeed;
    while (s.dropped < kStrikeBombs && dist >= s.along[s.dropped]) {
      Bomb b;
      b.id = w.newId(); b.owner = s.owner; b.pos = s.drop[s.dropped];
      w.bombs.push_back(b);
      s.dropped++;
    }
    if (dist > kPlaneLead + 1300.0f) { w.strikes[i] = w.strikes.back(); w.strikes.pop_back(); }
    else ++i;
  }
  for (size_t i = 0; i < w.bombs.size();) {
    Bomb& b = w.bombs[i];
    b.fuse -= kDt;
    if (b.fuse <= 0) {
      const Bomb copy = b;
      w.bombs[i] = w.bombs.back();
      w.bombs.pop_back();
      explode(w, copy.pos, copy.blast, copy.owner, copy.dmg, 0, 0, 0, copy.kind == 1 ? 6 : 3);
    } else {
      ++i;
    }
  }

  // ---- Turrets ----
  for (Turret& tu : w.turrets) {
    tu.life -= kDt;
    tu.cd -= kDt;
    if (tu.life <= 0) continue;
    Tank* tg = nearestEnemy(w, tu.owner, tu.pos, kTurretRange);
    if (tg && tu.cd <= 0) {
      autoShot(w, tu.owner, tu.pos, seenPos(*tg), tg->vel, 700.0f, 19.0f, 9.0f, 820.0f, BulletKind::Turret, 0.08f, tu.aim);
      tu.cd = kTurretReload;
    }
  }

  // ---- Drones ----
  syncDrones(w);
  stepDrones(w);

  // ---- Singularities: pull enemies in and slow them; touching the core stops them shooting and using superpowers ----
  for (Singularity& s : w.sings) {
    s.life -= kDt;
    if (s.life <= 0) { w.emit(EvType::Boom, s.pos, 90.0f, s.owner, 4); continue; }  // collapses when it runs out
    for (Tank& t : w.tanks) {
      if (!t.alive || t.id == s.owner || t.id == w.godId) continue;
      const Vec2 d = s.pos - t.pos;
      const float len = d.len();
      if (len > kSingRadius) continue;
      t.slowT = 0.2f;
      if (len > 1.0f) t.pos += d * (std::min(len, kSingPull * (1.0f - 0.5f * len / kSingRadius) * kDt) / len);
      if (len < kSingTouch + t.radius) t.silenceT = std::max(t.silenceT, kSingSilence);
    }
  }

  // ---- Bullets: move (in small steps, bouncing off walls), then hit crates, shapes, exposed drones and tanks. Each hits
  // at most one thing. A bullet keeps flying to the very end of its range (it fades out on the way). ----
  struct Box2 { float x0, y0, x1, y1; };
  std::vector<Box2> wallBox(w.walls.size());
  for (size_t k = 0; k < w.walls.size(); ++k) {
    const Wall& wl = w.walls[k];
    const float e = wl.thick * 0.5f + 16.0f;
    wallBox[k] = {std::min(wl.a.x, wl.b.x) - e, std::min(wl.a.y, wl.b.y) - e, std::max(wl.a.x, wl.b.x) + e, std::max(wl.a.y, wl.b.y) + e};
  }
  for (size_t bi = 0; bi < w.bullets.size();) {
    Bullet& b = w.bullets[bi];
    const float speed = b.vel.len();
    const int sub = std::max(1, (int)std::ceil(speed * kDt / 14.0f));
    bool dead = false;
    for (int k = 0; k < sub && !dead; ++k) {
      b.pos += b.vel * (kDt / sub);
      for (size_t wi = 0; wi < w.walls.size(); ++wi) {
        const Box2& bx = wallBox[wi];
        if (b.pos.x < bx.x0 || b.pos.x > bx.x1 || b.pos.y < bx.y0 || b.pos.y > bx.y1) continue;
        const Wall& wl = w.walls[wi];
        const Vec2 cp = closestOnSeg(wl.a, wl.b, b.pos);
        const Vec2 d = b.pos - cp;
        const float len = d.len(), lim = wl.thick * 0.5f + b.radius * 0.5f;
        if (len >= lim) continue;
        if (b.bounces >= b.maxBounce) { dead = true; break; }
        const Vec2 n = len > 1e-3f ? d * (1.0f / len) : b.vel.norm() * -1.0f;
        const float vn = b.vel.x * n.x + b.vel.y * n.y;
        if (vn < 0) b.vel = b.vel - n * (2.0f * vn);
        b.pos = cp + n * (lim + 0.5f);
        b.bounces++;
        break;
      }
    }
    b.rangeLeft -= speed * kDt;
    if (b.rangeLeft <= 0 || !w.inAnyArea(b.pos)) dead = true;
    const Tank* bo = dead ? nullptr : w.find(b.owner);

    if (!dead) {
      for (LootBox& x : w.boxes) {
        if (x.hp <= 0) continue;
        const float rr = boxRadius(x.kind) + b.radius;
        const Vec2 d = x.pos - b.pos;
        if (d.x * d.x + d.y * d.y <= rr * rr) { x.hp -= b.damage; x.lastHit = b.owner; dead = true; break; }
      }
    }
    if (!dead) {
      for (Shape& s : w.shapes) {
        if (s.health <= 0) continue;
        float rr = shapeInfo(s.kind).radius + b.radius;
        Vec2 d = s.pos - b.pos;
        if (d.x * d.x + d.y * d.y <= rr * rr) {
          s.health -= b.damage;
          s.hitFlash = 0.08f;
          dead = true;
          if (s.health <= 0)
            for (Tank& t : w.tanks) if (t.id == b.owner) { t.xp += shapeInfo(s.kind).xp; t.shapesDestroyed++; }
          break;
        }
      }
    }
    if (!dead) {
      for (Drone& d : w.drones) {
        if (d.dead || d.owner == b.owner || !d.vulnerable()) continue;
        if ((d.pos - b.pos).len() < 14.0f + b.radius) {
          dead = true;
          if (--d.hp <= 0) destroyDrone(w, d, d.kind == Relic::Loot);
          break;
        }
      }
    }
    if (!dead) {
      for (Tank& t : w.tanks) {
        if (!t.alive || t.id == b.owner || (bo && !hostile(*bo, t))) continue;
        float rr = t.radius + b.radius;
        Vec2 d = t.pos - b.pos;
        if (d.x * d.x + d.y * d.y <= rr * rr) {
          dead = true;
          const bool blocked = t.active == Power::Shield || t.id == w.godId;
          damageTank(w, t, b.damage, b.owner);
          if (b.kind == BulletKind::Bolt && t.alive && !blocked) t.stunT = std::max(t.stunT, kBoltStun);
          break;
        }
      }
    }

    if (dead) { w.bullets[bi] = w.bullets.back(); w.bullets.pop_back(); }
    else ++bi;
  }

  // Remove spent entities.
  w.drones.erase(std::remove_if(w.drones.begin(), w.drones.end(), [](const Drone& d) { return d.dead; }), w.drones.end());
  w.turrets.erase(std::remove_if(w.turrets.begin(), w.turrets.end(), [](const Turret& t) { return t.life <= 0; }), w.turrets.end());
  w.sings.erase(std::remove_if(w.sings.begin(), w.sings.end(), [](const Singularity& s) { return s.life <= 0; }), w.sings.end());
  for (size_t i = 0; i < w.shapes.size();) {
    if (w.shapes[i].health <= 0) { w.shapes[i] = w.shapes.back(); w.shapes.pop_back(); }
    else ++i;
  }
  for (int n = 0; n < 2 && (int)w.shapes.size() < kTargetShapes; ++n) w.spawnShape();
  // Broken crates spill their loot.
  for (size_t k = 0; k < w.boxes.size();) {
    if (w.boxes[k].hp > 0) { ++k; continue; }
    const LootBox b = w.boxes[k];
    w.boxes[k] = w.boxes.back();
    w.boxes.pop_back();
    openBox(w, b);
  }
  if (w.mode == Mode::Rampage) {
    if (w.tickCount % kUpgradeEvery == 0 && (int)w.upgrades.size() < w.target(kTargetUpgrades / 3)) w.spawnUpgrade(w.lootPos());
  } else {
    if (w.tickCount % kCrateEvery == 0 && (int)w.crates.size() < w.target(kTargetCrates)) w.spawnCrate(w.lootPos());
    if (w.tickCount % kUpgradeEvery == 0 && (int)w.upgrades.size() < w.target(kTargetUpgrades)) w.spawnUpgrade(w.lootPos());
    if (w.tickCount % kRelicEvery == 0 && (int)w.relics.size() < w.target(kTargetRelics)) w.spawnRelic(w.relicPos());
    if (w.tickCount % kBoxEvery == 0 && w.boxCount(BoxKind::Normal) < w.target(kTargetBoxes))
      for (int k = 0; k < 10; ++k) {
        const Vec2 p = w.randomPos(400.0f);
        if (w.freeSpot(p, 40.0f)) { w.spawnBox(p, BoxKind::Normal); break; }
      }
  }
  // Legendary Crate: one at a time, announced 10 s ahead, always inside the safe zone. The first lands 2-3 minutes in,
  // then another every 2-3 storm phases.
  if (w.legendAt > 0 && w.ringEnabled && w.mode == Mode::Royale) {
    if (!w.legendWarned && w.time >= w.legendAt - kLegendWarn) {
      const Zone z = w.ring.next;
      Vec2 p = z.c;
      for (int k = 0; k < 60; ++k) {
        const float a = w.rng.range(0, 6.2831853f), d = std::sqrt(w.rng.f01()) * std::min(z.r * 0.7f, kMapSize * 0.45f);
        const Vec2 q = z.c + Vec2{std::cos(a), std::sin(a)} * d;
        if (q.x > 500 && q.y > 500 && q.x < kMapSize - 500 && q.y < kMapSize - 500 && w.freeSpot(q, 60.0f)) { p = q; break; }
      }
      w.legendPos = p;
      w.legendWarned = true;
      w.emit(EvType::LegendWarn, p);
    }
    if (w.legendWarned && w.time >= w.legendAt) {
      w.spawnBox(w.legendPos, BoxKind::Legendary);
      w.emit(EvType::LegendDrop, w.legendPos);
      w.legendWarned = false;
      w.legendAt = w.ring.phase >= (int)kRingPhases.size() - 2 ? -1.0f : w.time + w.rng.range(180.0f, 270.0f);
    }
  }

  // Last tank standing wins (guardians and Sentry Robots don't count).
  if (!w.allowRespawn && !w.matchOver && w.playerCount() > 1 && w.aliveCount() <= 1) {
    w.matchOver = true;
    for (Tank& t : w.tanks)
      if (t.alive && !npc(t)) { w.winnerId = t.id; t.place = 1; }
  }
  // Rocket Rampage: after 3 minutes the highest score wins; everyone is ranked by score.
  if (w.mode == Mode::Rampage && !w.sandbox && !w.matchOver && w.time >= kRampageLen) {
    w.matchOver = true;
    std::vector<Tank*> rank;
    for (Tank& t : w.tanks) if (!npc(t)) rank.push_back(&t);
    std::stable_sort(rank.begin(), rank.end(), [](const Tank* a, const Tank* b) { return a->score > b->score || (a->score == b->score && a->kills > b->kills); });
    for (size_t k = 0; k < rank.size(); ++k) rank[k]->place = (uint32_t)k + 1;
    if (!rank.empty()) w.winnerId = rank[0]->id;
  }

  // Respawns (sandbox and Rocket Rampage: bots after 6 s, the player after 3 s). Guardians and Sentry Robots don't come back.
  if (w.allowRespawn && !w.matchOver) {
    for (Tank& t : w.tanks) {
      if (t.alive || npc(t) || t.gone) continue;
      t.deathTimer += kDt;
      if (t.deathTimer >= (t.isBot ? (w.mode == Mode::Rampage ? 4.0f : 6.0f) : 3.0f)) {
        t.alive = true;
        for (uint8_t& st : t.stat) st = 0;
        t.maxHealth = kMaxHealth;
        t.health = kMaxHealth;
        t.cooldown = 0;
        t.vel = {};
        t.temple = -1;
        t.pos = w.spawnSpot();
        if (w.mode == Mode::Rampage && !w.sandbox) {  // inside the fog, as far as possible from everyone else
          float bestD = -1;
          Vec2 best = t.pos;
          for (int k = 0; k < 30; ++k) {
            const float a = w.rng.range(0, 6.2831853f), d = std::sqrt(w.rng.f01()) * std::max(300.0f, w.ring.cur.r - 300.0f);
            const Vec2 p = w.ring.cur.c + Vec2{std::cos(a), std::sin(a)} * d;
            if (w.inWall(p, 60.0f)) continue;
            float nearest = 1e9f;
            for (const Tank& o : w.tanks) if (o.id != t.id && o.alive) nearest = std::min(nearest, (o.pos - p).len());
            if (nearest > bestD) { bestD = nearest; best = p; }
          }
          t.pos = best;
        }
        t.held = w.mode == Mode::Rampage ? Power::Rocket : Power::None;
        t.recharge = 0;
        t.unharmed = 99;
        t.stunT = t.silenceT = t.slowT = 0;
        endActive(t, false);
        t.recharge = 0;
      }
    }
  }
}

}  // namespace tonk

#include "core/bot.h"
