#pragma once
// Bot brain. Bots only ever produce an Input (move / aim / shoot / superpower / relic keys); the authoritative world
// decides what actually happens, exactly as it does for human players.
//
// Every bot has a SKILL TIER, a PLAY STYLE (Looter, Brawler, Tactician, Survivor, Hunter, Raider), an aggression meter
// and affinities (how much it likes looting, fighting and raiding). See GAME_DESIGN.md section 7.8.
//   - Tiers, like real players: Newbie (no game sense: random objectives, wastes superpowers), Rookie (sticks to its
//     style's goal), Intermediate (the first to change objective with the situation, e.g. fight for loot), Skilled, Pro
//     and Elite (better aim, dodging, prediction and judgment, each a bit more). Better tiers are rarer (assignRoster).
//   - Test tiers, made only from the dev panel: Unreal (computer-perfect aim and reactions), Omniscient (Unreal, plus it
//     knows where everything on the map is) and Omnipotent (Omniscient, plus boosted stats, teleports and blinds).
//   - Aggression: higher means more willing to fight, and to start fights for no reason; lower means it prefers loot,
//     the zone and staying alive.
//
// Fairness (the six match tiers): a bot only "sees" what a player would see on screen (the same view rectangle), and in a
// temple's dark only what has a clear line to it. Invisible tanks are hidden from it unless they just fired. It knows
// the Closing Ring (public on the HUD) but not other tanks' upgrades. It dodges like a person: each bullet it notices is
// judged once, and a dodge only starts after its reaction time, so fast or close shots land. Its aim leads with a
// lagging idea of your velocity, so sharp turns shake it off.
//
// Layers, from slow to fast:
//   1. decide()   every 0.18-0.45 s (by tier)  pick a state: Zone / Retreat / Engage / Loot / Roam, and superpower use
//   2. steer()    every tick  turn the state into a move vector, plus wall/ring/guardian/turret avoidance and dodging
//   3. aim/fire   every tick  lead the target with a lagging velocity estimate, drifting aim error, reaction delay
//
// The big guardians (the Arena Mage) have their own, much simpler brain at the bottom (guardianInput).
#include "core/world.h"

namespace tonk {

namespace botcfg {
constexpr float kNearFoe = 900.0f;     // foes this close count toward danger
constexpr float kWallMargin = 450.0f;  // start steering away from the map edge
constexpr float kHitRate = 0.55f;      // the share of shots a bot expects to land when it adds up a fight
}  // namespace botcfg

struct BotTierInfo {
  float aimNoise, lead, trackTau, reactMin, reactMax;
  float dodge, dodgeReact;  // chance to dodge a bullet; the fastest it can react (s)
  float iq, think, turn;
  float usePower;           // chance to use a superpower when the moment is right
  float guardSense;         // how far from a calm guardian it keeps (Newbies wander too close)
  float skill;              // 0..1 summary for the dev panel
};
//                       aimNoise lead  track  react      dodge react  iq    think turn  power guard  skill
inline BotTierInfo botTierInfo(BotTier t) {
  switch (t) {
    case BotTier::Newbie:       return {0.21f, 0.00f, 0.90f, 0.80f, 1.30f, 0.00f, 0.60f, 0.00f, 0.55f, 0.28f, 0.20f, 300.0f, 0.08f};
    case BotTier::Rookie:       return {0.15f, 0.25f, 0.60f, 0.60f, 1.00f, 0.03f, 0.50f, 0.25f, 0.42f, 0.40f, 0.40f, 450.0f, 0.25f};
    case BotTier::Intermediate: return {0.11f, 0.48f, 0.42f, 0.45f, 0.75f, 0.07f, 0.42f, 0.50f, 0.32f, 0.55f, 0.55f, 600.0f, 0.45f};
    case BotTier::Skilled:      return {0.085f, 0.65f, 0.32f, 0.34f, 0.56f, 0.11f, 0.36f, 0.72f, 0.26f, 0.75f, 0.65f, 700.0f, 0.62f};
    case BotTier::Pro:          return {0.07f, 0.75f, 0.26f, 0.29f, 0.47f, 0.14f, 0.32f, 0.85f, 0.22f, 0.90f, 0.72f, 760.0f, 0.74f};
    case BotTier::Elite:        return {0.06f, 0.82f, 0.22f, 0.25f, 0.41f, 0.17f, 0.29f, 0.94f, 0.19f, 1.00f, 0.80f, 800.0f, 0.84f};
    case BotTier::Unreal:       return {0.012f, 1.00f, 0.03f, 0.07f, 0.11f, 0.85f, 0.07f, 1.00f, 0.12f, 1.00f, 0.95f, 820.0f, 0.97f};
    case BotTier::Omniscient:   return {0.012f, 1.00f, 0.03f, 0.07f, 0.11f, 0.85f, 0.07f, 1.00f, 0.12f, 1.00f, 0.95f, 820.0f, 0.99f};
    default:                    return {0.010f, 1.00f, 0.03f, 0.06f, 0.10f, 0.88f, 0.06f, 1.00f, 0.12f, 1.00f, 0.95f, 820.0f, 1.00f};
  }
}
inline const char* botTierName(BotTier t) {
  static const char* k[] = {"Newbie", "Rookie", "Intermediate", "Skilled", "Pro", "Elite", "Unreal", "Omniscient", "Omnipotent"};
  return k[std::min((int)t, (int)BotTier::Count - 1)];
}
inline const char* botGoalName(BotGoal g) {
  static const char* k[] = {"Roam", "Loot", "Crate", "Attack", "Contest", "Hunt", "Retreat", "Zone", "Raid", "Recover", "Guard", "Follow"};
  return k[std::min((int)g, (int)BotGoal::Count - 1)];
}

struct BotStyleInfo {
  float fight;    // added to a fight's score: > 0 eager, < 0 picky
  float loot;     // how much loot is worth to it
  float retreat;  // it backs off when its best fight scores below this
  float chase;    // seconds it keeps chasing a target it can't see
  float avoid;    // 1: steers away from enemies it doesn't want to fight
  float range;    // preferred fighting distance, as a share of its bullet range
};
inline BotStyleInfo botStyleInfo(BotStyle s) {
  switch (s) {
    case BotStyle::Looter:    return {-0.45f, 1.6f, -0.25f, 1.5f, 0.0f, 0.75f};
    case BotStyle::Brawler:   return {0.45f, 0.7f, -0.90f, 3.0f, 0.0f, 0.55f};
    case BotStyle::Tactician: return {-0.20f, 1.0f, -0.10f, 2.0f, 0.0f, 0.85f};
    case BotStyle::Survivor:  return {-0.85f, 1.1f, 0.25f, 0.8f, 1.0f, 0.85f};
    case BotStyle::Raider:    return {-0.10f, 1.0f, -0.30f, 1.5f, 0.0f, 0.75f};
    default:                  return {0.20f, 0.8f, -0.60f, 8.0f, 0.0f, 0.65f};  // Hunter
  }
}
inline const char* botStyleName(BotStyle s) {
  static const char* k[] = {"Looter", "Brawler", "Tactician", "Survivor", "Hunter", "Raider"};
  return k[std::min((int)s, (int)BotStyle::Count - 1)];
}

inline float dot2(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float cross2(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
inline Vec2 perp2(Vec2 a) { return {-a.y, a.x}; }

inline bool botSafePoint(const World& w, Vec2 p, float margin = 150.0f) {
  if (p.x > kMapSize + 500.0f) {  // inside a temple: anywhere within its walls
    for (const Temple& T : w.temples) if ((p - T.c).len() < T.R - margin * 0.5f) return true;
    return false;
  }
  if (p.x < w.lo.x + margin || p.y < w.lo.y + margin || p.x > w.hi.x - margin || p.y > w.hi.y - margin) return false;
  return !w.ringEnabled || (p - w.ring.cur.c).len() < w.ring.cur.r - margin * 0.5f;
}

// What a player sees: the screen rectangle around their tank.
inline bool botSees(Vec2 me, Vec2 p, float pad = 40.0f) {
  return std::fabs(p.x - me.x) <= kViewHalfW + pad && std::fabs(p.y - me.y) <= kViewHalfH + pad;
}

// Damage per second a tank puts out. Its own stats a bot knows; for others it can only see active superpowers.
inline float botDps(const Tank& t, bool own) {
  if (t.guardian) return kGuardWeapon.damage / kGuardWeapon.reloadSec;
  const WeaponParams p = weaponParams(kBaseWeapons[(size_t)t.weapon]);
  float dmg = p.damage * statDamageMul(own ? t.stat[StatDamage] : 1);
  float reload = p.reloadSec * statReloadMul(own ? t.stat[StatFireRate] : 1);
  if (t.active == Power::RapidShot) reload /= kRapidShotMul;
  if (t.active == Power::Spread) dmg *= kSpreadDamageMul * 2.5f;  // a few of the five usually land
  if (t.stunT > 0 || t.silenceT > 0) dmg *= 0.3f;
  return dmg / reload;
}

// How much stuff a bot carries (Looters fight once they have enough).
inline int botGear(const Tank& t) {
  int g = 0;
  for (uint8_t s : t.stat) g += s;
  for (Relic r : t.relic) g += r != Relic::None ? 3 : 0;
  return g + (t.held != Power::None ? 3 : 0);
}

// Affinities from the style and the aggression meter, with a little luck: loot, fight, raid.
inline void botAffinities(World& w, Tank& me) {
  BotMind& m = me.mind;
  const BotStyleInfo si = botStyleInfo(m.style);
  m.affLoot = std::clamp(si.loot * w.rng.range(0.85f, 1.15f) * (1.0f - 0.25f * (m.aggression - 1.0f)), 0.2f, 2.5f);
  m.affFight = std::clamp((1.0f + si.fight + (m.aggression - 1.0f) * 0.8f) * w.rng.range(0.9f, 1.1f), 0.1f, 2.5f);
  m.affRaid = m.style == BotStyle::Raider ? 1.0f : m.templeFan ? 0.5f : 0.0f;
}

inline Vec2 botPickRoamGoal(World& w, const Zone& z, float inner) {
  const float a = w.rng.range(0, 6.2831853f);
  const float half = (w.hi.x - w.lo.x) * 0.5f, pad = std::min(300.0f, half * 0.3f);
  const bool on = w.ringEnabled && z.r < 1e8f;
  const Vec2 c = on ? z.c : w.middle();
  const float r = std::sqrt(w.rng.f01()) * std::min(on ? z.r : half, half) * inner;
  Vec2 p = c + Vec2{std::cos(a), std::sin(a)} * r;
  p.x = std::clamp(p.x, w.lo.x + pad, w.hi.x - pad);
  p.y = std::clamp(p.y, w.lo.y + pad, w.hi.y - pad);
  return p;
}

// Rocket Rampage: somewhere inside the fog that is far from everyone else (so the arena stays spread out).
inline Vec2 botSpreadGoal(World& w, const Tank& me) {
  const bool on = w.ringEnabled && w.ring.cur.r < 1e8f;
  const Vec2 c = on ? w.ring.cur.c : w.middle();
  const float R = std::min(on ? w.ring.cur.r : 1e9f, (w.hi.x - w.lo.x) * 0.5f);
  Vec2 best = c;
  float bestS = -1e9f;
  for (int k = 0; k < 10; ++k) {
    const float a = w.rng.range(0, 6.2831853f), d = std::sqrt(w.rng.f01()) * std::max(300.0f, R - 450.0f);
    const Vec2 p = c + Vec2{std::cos(a), std::sin(a)} * d;
    if (w.inWall(p, 60.0f) || !w.inAnyArea(p)) continue;
    float nearest = 1e9f;
    for (const Tank& o : w.tanks) if (o.alive && o.id != me.id && !npc(o)) nearest = std::min(nearest, (o.pos - p).len());
    const float sc = std::min(nearest, 2500.0f) - (p - me.pos).len() * 0.15f;
    if (sc > bestS) { bestS = sc; best = p; }
  }
  return best;
}

// Sets a bot's skill tier and everything that comes with it.
inline void setBotTier(Tank& me, BotTier tier) {
  BotMind& m = me.mind;
  m.tier = tier;
  const BotTierInfo ti = botTierInfo(tier);
  m.aimNoise = ti.aimNoise;
  m.lead = ti.lead;
  m.trackTau = ti.trackTau;
  m.reactMin = ti.reactMin; m.reactMax = ti.reactMax;
  m.dodgeChance = ti.dodge;
  m.dodgeReact = ti.dodgeReact;
  m.iq = ti.iq;
  m.thinkEvery = ti.think;
  m.turn = ti.turn;
  m.skill = ti.skill;
}

// A tier for a bot that isn't part of a roster (the odds of a match): Newbie 15%, Rookie 30%, Intermediate 27%,
// Skilled 17%, Pro 7%, Elite 4%.
inline BotTier botRandomTier(World& w) {
  const float r = w.rng.f01();
  return r < 0.15f ? BotTier::Newbie : r < 0.45f ? BotTier::Rookie : r < 0.72f ? BotTier::Intermediate
       : r < 0.89f ? BotTier::Skilled : r < 0.96f ? BotTier::Pro : BotTier::Elite;
}

// tier < 0: a random match tier.
inline void botSeed(World& w, Tank& me, int tier = -1) {
  BotMind& m = me.mind;
  m.seeded = true;
  m.tier = tier >= 0 ? (BotTier)std::min(tier, (int)BotTier::Count - 1) : botRandomTier(w);
  const float s = w.rng.f01();  // 20% Looter, 22% Brawler, 15% Tactician, 13% Survivor, 16% Hunter, 14% Raider
  m.style = s < 0.20f ? BotStyle::Looter : s < 0.42f ? BotStyle::Brawler : s < 0.57f ? BotStyle::Tactician
          : s < 0.70f ? BotStyle::Survivor : s < 0.86f ? BotStyle::Hunter : BotStyle::Raider;
  m.aggression = w.rng.range(0.4f, 1.6f);
  if (m.tier >= BotTier::Unreal) m.aggression = std::min(2.0f, m.aggression + 0.3f);  // the test bots are pushier
  // Temple Guardians: Newbie Brawlers, and clumsy ones (see below). Sentry Robots start as Newbie Brawlers and learn.
  if (me.mob) { m.tier = BotTier::Newbie; m.style = BotStyle::Brawler; m.aggression = 1.5f; }
  if (me.sentryOf) { m.tier = BotTier::Newbie; m.style = BotStyle::Brawler; m.aggression = 1.3f; }
  m.templeFan = !npc(me) && (m.style == BotStyle::Raider || (m.style != BotStyle::Survivor && w.rng.f01() < 0.3f));
  botAffinities(w, me);
  setBotTier(me, m.tier);
  m.aimNoise *= w.rng.range(0.85f, 1.15f);
  if (m.tier == BotTier::Omnipotent) m.basePower = 1.6f;
  me.powerMul = m.basePower;
  if (me.mob) {  // much worse than a Newbie: wild aim, no leading, slow to react, slow to turn, never dodges
    m.aimNoise *= 2.0f; m.lead = 0; m.trackTau = 1.0f; m.reactMin = 1.0f; m.reactMax = 1.6f;
    m.dodgeChance = 0; m.turn = 0.25f; m.thinkEvery = 0.6f;
  }
  m.decideTimer = w.rng.range(0.0f, 0.25f);  // stagger thinking across bots
  m.strafeSign = w.rng.f01() < 0.5f ? 1.0f : -1.0f;
  m.goal = me.pos;
  m.stuckPos = me.pos;
  m.lastHealth = me.health;
}

// The bots of a match: better tiers are rarer. Per 39 bots: Elite 0-1, Pro 0-2, Skilled 5-8, Intermediate 9-12,
// Newbie 4-7, and the rest Rookies (scaled down for fewer bots, e.g. Rocket Rampage's 19). Seeds every unseeded bot.
inline void assignRoster(World& w) {
  std::vector<Tank*> bots;
  for (Tank& t : w.tanks) if (t.isBot && !npc(t) && !t.mind.seeded) bots.push_back(&t);
  const int n = (int)bots.size();
  if (!n) return;
  const float k = n / 39.0f;
  auto pick = [&](int lo, int hi) { return (int)std::lround((lo + (int)(w.rng.next() % (uint32_t)(hi - lo + 1))) * k); };
  int cnt[kMatchTiers] = {};
  cnt[(int)BotTier::Elite] = pick(0, 1);
  cnt[(int)BotTier::Pro] = pick(0, 2);
  cnt[(int)BotTier::Skilled] = pick(5, 8);
  cnt[(int)BotTier::Intermediate] = pick(9, 12);
  cnt[(int)BotTier::Newbie] = pick(4, 7);
  int used = 0;
  for (int c : cnt) used += c;
  for (int t = kMatchTiers - 1; used > n && t >= 0; --t)  // (tiny lobbies) drop the best first
    while (cnt[t] > 0 && used > n) { cnt[t]--; used--; }
  cnt[(int)BotTier::Rookie] += n - used;
  std::vector<int> tiers;
  for (int t = 0; t < kMatchTiers; ++t) for (int i = 0; i < cnt[t]; ++i) tiers.push_back(t);
  for (int i = n - 1; i > 0; --i) std::swap(tiers[i], tiers[w.rng.next() % (uint32_t)(i + 1)]);
  for (int i = 0; i < n; ++i) botSeed(w, *bots[i], tiers[i]);
}

inline Input presetInput(World& w, Tank& me);  // core/bot_presets.h

inline Input botInput(World& w, Tank& me) {
  using namespace botcfg;
  // Preset bots (dev panel, BOTS > Presets) have their own brains. In a temple or Rocket Rampage they fall back to this one.
  if (me.mind.brain && me.mind.seeded && !npc(me) && me.temple < 0 && w.mode == Mode::Royale) return presetInput(w, me);
  BotMind& m = me.mind;
  Input in;
  if (!m.seeded) botSeed(w, me);

  const BotTierInfo ti = botTierInfo(m.tier);
  const BotStyleInfo si = botStyleInfo(m.style);
  const WeaponParams wp = weaponParams(kBaseWeapons[(size_t)me.weapon]);
  const float myRange = wp.range * statRangeMul(me.stat[StatRange]) * me.powerMul;
  const float myBulletSpeed = wp.bulletSpeed * statBulletSpeedMul(me.stat[StatBulletSpeed]) * me.powerMul;
  // What its tier lets it do.
  const bool newbie = m.tier == BotTier::Newbie, rookie = m.tier == BotTier::Rookie;
  const bool varied = m.tier >= BotTier::Intermediate;  // changes objective with the situation
  const bool cheat = m.tier >= BotTier::Unreal;         // computer-perfect aim and reactions (test bots only)
  const bool omni = m.tier >= BotTier::Omniscient;      // knows where everything on the map is (test bots only)
  const RingState& ring = w.ring;
  const float caution = 2.0f - m.aggression;

  m.stateTime += kDt;
  m.commitTimer -= kDt;
  m.decideTimer -= kDt;
  m.react -= kDt;
  m.strafeTimer -= kDt;
  m.dodgeTimer -= kDt;
  m.dodgeCooldown -= kDt;
  m.skipT -= kDt;
  m.slideT -= kDt;
  m.detourT -= kDt;
  m.hurtTimer += kDt;
  m.lostTimer += kDt;
  if (me.health < m.lastHealth - 0.01f) { m.hurtTimer = 0; m.hurtBy = me.lastHitBy; }
  m.lastHealth = me.health;
  // A Sentry Robot gets better the longer it survives.
  if (me.sentryOf) {
    const BotTier want = m.age >= kBotSkillAt[2] ? BotTier::Skilled : m.age >= kBotSkillAt[1] ? BotTier::Intermediate
                       : m.age >= kBotSkillAt[0] ? BotTier::Rookie : BotTier::Newbie;
    if (want != m.tier) setBotTier(me, want);
  }
  const Tank* boss = me.sentryOf ? w.find(me.sentryOf) : nullptr;
  const Temple* temple = me.temple >= 0 && me.temple < (int)w.temples.size() ? &w.temples[me.temple] : nullptr;
  const bool rampage = w.mode == Mode::Rampage;

  // ---- Perception: what's on its screen (where it *looks* like it is, for invisible tanks) ----
  // (Omniscient bots know about everyone; only those on screen count for fights. Blinded: only what's right next to it.)
  struct Foe { const Tank* t; Vec2 p; float d; float score; bool vis; bool forced = false; };
  Foe foes[64];
  int nf = 0;
  // Dev panel lines: red ones it must go after (always known, wherever they are), blue ones (teammates) it never attacks.
  uint32_t must[64], mates[64];
  int nMust = 0, nMates = 0;
  for (const World::Link& l : w.links) {
    if (l.team && (l.from == me.id || l.to == me.id) && nMates < 64) mates[nMates++] = l.from == me.id ? l.to : l.from;
    else if (!l.team && l.from == me.id && nMust < 64) must[nMust++] = l.to;
  }
  auto listed = [](const uint32_t* a, int n, uint32_t id) { for (int i = 0; i < n; ++i) if (a[i] == id) return true; return false; };
  if (m.targetId && listed(mates, nMates, m.targetId)) {  // just teamed up with the one it was fighting
    m.targetId = 0;
    m.decideTimer = 0;
  }
  const Foe* forcedFoe = nullptr;
  for (const Tank& o : w.tanks) {
    if (!o.alive || o.id == me.id || nf >= 64) continue;
    if (nMates && listed(mates, nMates, o.id)) continue;
    if (nMust && listed(must, nMust, o.id)) {
      if (o.temple != me.temple) continue;
      foes[nf] = {&o, o.pos, (o.pos - me.pos).len(), -1e9f, true, true};
      // The nearest one (sticking with the one it's already after, unless another is much closer).
      if (!forcedFoe || foes[nf].d * (o.id == m.forcedTarget ? 0.6f : 1.0f) <
                            forcedFoe->d * (forcedFoe->t->id == m.forcedTarget ? 0.6f : 1.0f))
        forcedFoe = &foes[nf];
      ++nf;
      continue;
    }
    if (!hostile(me, o) || o.ghost) continue;
    if (!omni && !canSpot(o)) continue;
    if (o.guardian && o.aggroId != me.id && !me.mob) continue;  // calm guardians are scenery to steer around
    if (o.temple != me.temple) continue;
    const Vec2 p = omni ? o.pos : seenPos(o);
    const float d = (p - me.pos).len();
    if (me.blindT > 0 && d > 220.0f) continue;
    bool vis = botSees(me.pos, p);
    if (vis && (me.mob || temple) && w.lineBlocked(me.pos, p)) vis = false;  // in a temple's dark, only what it can see
    if (!vis && !omni) continue;
    foes[nf++] = {&o, p, d, -1e9f, vis};
  }
  // Enemy turrets are threats too (they can't be destroyed: stay out of their reach).
  float turretDps = 0;
  for (const Turret& tu : w.turrets)
    if (tu.owner != me.id && (tu.pos - me.pos).len() < kTurretRange + 80.0f) turretDps += 19.0f / kTurretReload;

  const float distC = temple ? 0.0f : (me.pos - ring.cur.c).len();
  const float outBy = temple ? -1e9f : distC - ring.cur.r;  // > 0: standing in the storm
  const float outNext = temple ? -1e9f : (me.pos - ring.next.c).len() - ring.next.r;
  const float margin = 4.0f + 14.0f * m.iq;  // smarter bots rotate early
  const bool zoneSoon = !temple && (ring.shrinking || ring.secsLeft < outNext / (kTankSpeed * 0.8f) + margin);
  // (Rocket Rampage: only the fog it's in matters, not where the fog ends up, or everyone piles into the middle.)
  // Newbies only notice the storm once it's on them.
  const bool needZone = !temple && !me.sentryOf &&
                        (outBy > (rampage ? -250.0f : newbie ? -40.0f : -120.0f) || (!rampage && !newbie && zoneSoon && outNext > -150.0f));
  // Inside a temple: the next waypoint along the spiral (or the exit portal once the Mage is beaten).
  auto templeGoal = [&]() {
    if (temple->cleared) return temple->c;
    const auto& P = temple->path;
    int best = m.wp;
    for (int k = m.wp; k < (int)P.size() && k < m.wp + 6; ++k)  // skip ahead if pushed along
      if ((P[k] - me.pos).len() < (P[best] - me.pos).len()) best = k;
    m.wp = best;
    if (m.wp < (int)P.size() - 1 && (P[m.wp] - me.pos).len() < 120.0f) m.wp++;
    return P[std::min(m.wp, (int)P.size() - 1)];
  };
  const Zone& Z = (ring.shrinking || zoneSoon) && !rampage ? ring.next : ring.cur;

  const float myStr = me.health / me.maxHealth;
  const float myDps = botDps(me, true) * kHitRate;
  bool thought = false;
  const Foe* best = nullptr;
  int nNear = 0;

  auto nearGuardian = [&](Vec2 p, float r) {
    for (const Tank& g : w.tanks) if (g.alive && g.guardian && g.aggroId != me.id && (g.pos - p).len() < r) return true;
    return false;
  };
  // Is there loot worth taking right now, close by? (during a fight or while running away)
  auto findGrab = [&](Vec2 awayFrom, bool fleeing) {
    m.grab = false;
    if (m.iq < 0.2f) return;  // Newbies never think of it
    const float reach = 120.0f + 180.0f * m.iq;
    float bestV = 0;
    auto consider = [&](Vec2 pos, float val) {
      const Vec2 d = pos - me.pos;
      const float len = d.len();
      if (len > reach || val <= 0) return;
      if (fleeing && len > 90.0f && dot2(d.norm(), (awayFrom - me.pos).norm()) > 0.35f) return;  // not back toward them
      const float v = val - len / reach * 0.4f;
      if (v > bestV) { bestV = v; m.grabPos = pos; m.grab = true; }
    };
    for (const Upgrade& u : w.upgrades) {
      const int gain = (int)u.rarity + 1 - me.stat[u.stat];
      if (gain >= (fleeing ? 2 : 1)) consider(u.pos, 0.3f + 0.2f * gain + (u.stat == StatHealth && fleeing ? 0.3f : 0.0f));
    }
    if (me.held == Power::None) for (const Crate& c : w.crates) consider(c.pos, 1.0f);
    if (me.relic[1] == Relic::None) for (const RelicPickup& r : w.relics) if (!hasRelic(me, r.kind)) consider(r.pos, 1.0f);
  };

  // ======================= 1) DECIDE =======================
  if (m.decideTimer <= 0) {
    thought = true;
    m.decideTimer = m.thinkEvery * w.rng.range(0.8f, 1.2f);

    // Everyone close enough to shoot at us soon.
    float nearDps = turretDps;
    Vec2 centroid;
    for (int i = 0; i < nf; ++i) {
      if (foes[i].d >= kNearFoe || !foes[i].vis) continue;
      nearDps += botDps(*foes[i].t, false) * kHitRate;
      centroid += foes[i].p;
      nNear++;
    }
    if (nNear) centroid = centroid * (1.0f / nNear);

    const int gear = botGear(me);
    // The aggression meter: how willing it is to fight at all...
    float fightBias = si.fight + (m.aggression - 1.0f) * 0.9f;
    // ...and, for hotheads, now and then picking a fight for no reason.
    if (m.aggression > 1.15f && w.rng.f01() < (m.aggression - 1.15f) * 0.6f) fightBias += 0.7f;
    if (m.style == BotStyle::Looter && gear >= 12) fightBias += 0.5f;  // geared up: now it fights
    if (rampage) fightBias += 0.8f;                                      // Rocket Rampage: everyone fights
    if (me.mob || me.sentryOf) fightBias += 1.0f;
    // Rookies stick to their style's goal: Brawlers and Hunters fight, Looters and Survivors don't.
    if (rookie && !npc(me))
      fightBias += m.style == BotStyle::Brawler || m.style == BotStyle::Hunter ? 0.8f
                 : m.style == BotStyle::Looter || m.style == BotStyle::Survivor ? -0.6f : m.style == BotStyle::Raider ? -0.3f : 0.0f;
    const float noise = newbie ? 1.2f : (1.0f - m.iq) * 0.6f;  // Newbies' choices are close to random

    float bestScore = -1e9f;
    for (int i = 0; i < nf; ++i) {
      Foe& f = foes[i];
      const Tank& t = *f.t;
      if (!f.vis) continue;
      // Smart estimate: how long each of us needs to win (third parties shoot at us too).
      const float theirDps = botDps(t, false) * kHitRate;
      const float onMe = theirDps + std::max(0.0f, nearDps - (f.d < kNearFoe ? theirDps : 0.0f)) * 0.5f;
      const float myTime = (t.health + (t.active == Power::Shield ? 30.0f : 0.0f)) / std::max(1.0f, myDps);
      const float theirTime = (me.health + (me.active == Power::Shield ? 30.0f : 0.0f)) / std::max(1.0f, onMe);
      const float smart = std::log(std::max(0.05f, theirTime / myTime)) - (f.d / kNearFoe) * 0.25f;
      // Crude estimate (Newbies, Rookies): compare health bars, count heads.
      const int crowd = nNear - (f.d < kNearFoe ? 1 : 0);
      const float crude = (myStr - t.health / t.maxHealth) * 1.5f - crowd * 0.4f - (t.guardian ? 1.5f : 0.0f);
      float score = m.iq * smart + (1.0f - m.iq) * crude + fightBias + w.rng.range(-noise, noise);
      if (t.health < 25.0f) score += 0.3f;              // one or two shots from a kill
      if (t.mob && !me.mob) score += 0.6f;              // Temple Guardians are weak: clear them out of the way
      if (t.id == m.targetId) score += 0.25f;           // stick with the fight we're in
      if (t.id == m.hurtBy && m.hurtTimer < 2.0f) score += 0.2f;
      if (isHidden(t)) score -= 0.3f;                   // only a rough idea where it is
      f.score = score;
      if (score > bestScore) { bestScore = score; best = &f; }
    }
    // Newbies (40%) and Rookies (33%) run below a fixed health line, whatever is going on around them.
    const float panicAt = newbie ? 0.4f : rookie ? 0.33f : -1.0f;
    const bool noobPanic = myStr < panicAt && nNear > 0 && !me.mob && !me.sentryOf;
    const bool danger = nNear > 0 || turretDps > 0;
    const bool wantRetreat = danger && (noobPanic || bestScore < si.retreat || (!best && turretDps > 0));
    const Vec2 esc = (me.pos - (nNear ? centroid : me.pos + Vec2{1, 0})).norm();
    const bool cornered = nNear > 0 && !botSafePoint(w, me.pos + esc * 600.0f) &&
                          !botSafePoint(w, me.pos + perp2(esc) * 500.0f) && !botSafePoint(w, me.pos + perp2(esc) * -500.0f);
    const bool fightWhileZoning = outBy < -300.0f && best && bestScore > 0.6f && best->d < 500.0f;
    // Survivors (and peaceful bots) steer clear of anyone they don't want to fight.
    const bool avoidFoes = (si.avoid > 0 || m.aggression < 0.65f) && best && bestScore < 0.0f && best->d < 800.0f;
    // Hunters keep going after a target they lost sight of.
    const bool chasing = m.state == BotState::Engage && m.targetId && m.lostTimer < si.chase && !best;

    BotState ns = m.state;
    if (!forcedFoe) m.forcedTarget = 0;
    m.crateId = 0;
    m.recovering = false;
    if (m.contestId && (m.state != BotState::Engage || m.targetId != m.contestId)) m.contestId = 0;
    if (forcedFoe) {  // a red line (dev panel): go after it, no matter what
      ns = BotState::Engage;
      m.targetId = m.forcedTarget = forcedFoe->t->id;
      m.lostTimer = 0;
    } else if (me.mob || me.sentryOf) {  // temple guardians and Sentry Robots: fight, or go back to their spot / their owner
      if (best && bestScore > -3.0f && (!me.mob || (best->p - me.home).len() < 900.0f)) {
        ns = BotState::Engage;
        m.targetId = best->t->id;
      } else {
        ns = BotState::Roam;
        const float a = (float)(me.id % 7) * 0.9f;
        m.goal = boss ? boss->pos + Vec2{std::cos(a), std::sin(a)} * 110.0f : me.home;
      }
    } else if (needZone && !fightWhileZoning) {
      ns = BotState::Zone;
    } else if (m.state == BotState::Retreat && m.commitTimer > 0 && !cornered && danger) {
      ns = BotState::Retreat;
    } else if ((wantRetreat || avoidFoes) && !cornered) {
      ns = BotState::Retreat;
      if (m.state != BotState::Retreat) m.commitTimer = 1.0f + 1.0f * m.iq;
    } else if (best && (bestScore > (m.state == BotState::Engage ? -0.25f : 0.0f) || (wantRetreat && cornered))) {
      ns = BotState::Engage;
      if (best->t->id != m.targetId) m.lostTimer = 0;
      if (best->t->id != m.contestId) m.contestId = 0;
      m.targetId = best->t->id;
    } else if (chasing) {
      ns = BotState::Engage;
    } else {
      ns = BotState::Roam;
      // Loot: worth the trip? Weigh value against distance, enemies parked near it, and guardians.
      // Intermediate and up weigh fighting for loot that has an enemy next to it (Contest): the loot's worth, how the
      // fight looks, and the aggression meter. Newbies don't judge loot at all (anything will do, even useless upgrades)
      // and ignore who's near it. Hurt Skilled+ bots go for health first (Recover).
      bool found = false;
      float bcs = 0.2f, bestContest = 0.5f;
      Vec2 lootPos, contestPos;
      const Foe* contestFoe = nullptr;
      const bool recover = m.tier >= BotTier::Skilled && myStr < 0.45f && nNear == 0 && !rampage;
      bool lootIsHealth = false, healthBest = false;
      auto consider = [&](Vec2 pos, float val) {
        const float d = (pos - me.pos).len();
        if (val <= 0 || (omni ? d > 6000.0f : !botSees(me.pos, pos, 220.0f))) return;
        if (m.skipT > 0 && (pos - m.skipPos).len() < 80.0f) return;  // couldn't get to it a moment ago
        int risk = 0;
        const Foe* guard = nullptr;
        for (int i = 0; i < nf; ++i)
          if (foes[i].vis && (foes[i].p - pos).len() < 500.0f) { risk++; if (!guard || foes[i].d < guard->d) guard = &foes[i]; }
        const float worth = val * si.loot * m.affLoot * (1.0f - d / (omni ? 6000.0f : 1300.0f));
        float s = worth - risk * (newbie ? 0.1f : 0.5f * caution) + (d < 250.0f ? 0.4f : 0.0f);
        if (nearGuardian(pos, ti.guardSense)) s -= 1.2f * caution;
        if (varied && guard && !temple) {
          const float c = worth + guard->score * m.iq * 0.8f + (m.aggression - 1.0f) * 1.0f - (risk - 1) * 0.6f;
          if (c > bestContest) { bestContest = c; contestFoe = guard; contestPos = pos; }
        }
        if (s > bcs && w.lineBlocked(me.pos, pos)) {
          if (temple) return;  // in a temple, loot behind a wall is in another corridor: a long way round
          s -= 0.35f;          // behind a wall: a longer way round
        }
        if (s > bcs) { bcs = s; found = true; lootPos = pos; healthBest = lootIsHealth; }
      };
      if (me.held == Power::None) for (const Crate& c : w.crates) consider(c.pos, 1.0f);
      if (me.relic[1] == Relic::None)
        for (const RelicPickup& r : w.relics) if (!hasRelic(me, r.kind)) consider(r.pos, 1.1f);
      for (const Upgrade& u : w.upgrades) {  // only upgrades that raise a tier; bigger jumps are worth more
        const int gain = (int)u.rarity + 1 - me.stat[u.stat];
        lootIsHealth = u.stat == StatHealth && gain > 0;
        if (newbie) consider(u.pos, 0.6f);
        else if (gain > 0) consider(u.pos, 0.4f + 0.2f * gain + (recover && lootIsHealth ? 1.0f : 0.0f));
      }
      lootIsHealth = false;
      // Loot crates: worth shooting open (a Legendary Crate very much so).
      uint32_t crate = 0;
      for (const LootBox& b : w.boxes) {
        const float before = bcs;
        consider(b.pos, b.kind == BoxKind::Legendary ? 3.0f : b.kind == BoxKind::Small ? 0.8f : 1.3f);
        if (bcs > before) crate = b.id;  // crates are looked at last, so the best crate is the best loot overall
      }
      if (contestFoe && bestContest > bcs) {  // worth a fight: take the loot from them
        ns = BotState::Engage;
        m.targetId = m.contestId = contestFoe->t->id;
        m.contestPos = contestPos;
        m.lostTimer = 0;
        found = true;
      } else if (found) {
        ns = BotState::Loot;
        m.goal = lootPos;
        m.crateId = crate;
        m.recovering = recover && healthBest;
      }
      // Raiders and temple fans head into an Arena Temple when they are healthy and geared (Raiders go further for one).
      if (!found && !temple && m.templeFan && me.templeKicks < kTempleKicks && myStr > 0.7f &&
          gear >= (m.style == BotStyle::Raider ? 3 : 5) && !rampage && ring.phase < 4)
        for (const Temple& T : w.temples) {
          const float d = (T.door - me.pos).len(), reach = m.style == BotStyle::Raider ? 4500.0f : 1700.0f;
          if (!T.cleared && d < reach && botSafePoint(w, T.door, 200.0f)) { ns = BotState::Loot; m.goal = T.door; found = true; break; }
        }
      // Omniscient: with nothing better to do, hunt someone worth it anywhere on the map (a relic it lacks, low health).
      if (!found && omni && !temple) {
        const Foe* prey = nullptr;
        float ps = -1e9f;
        for (int i = 0; i < nf; ++i) {
          const Tank& t = *foes[i].t;
          if (t.mob || t.guardian) continue;
          bool relic = false;
          for (Relic r : t.relic) relic |= r != Relic::None && !hasRelic(me, r);
          const float sc = (relic ? 1.0f : 0.0f) + (1.0f - t.health / t.maxHealth) - foes[i].d / 6000.0f;
          if (sc > ps) { ps = sc; prey = &foes[i]; }
        }
        if (prey) { ns = BotState::Engage; m.targetId = prey->t->id; m.lostTimer = 0; found = true; }
      }
      // Recovering with nothing to pick up: keep to the quiet middle of the zone.
      if (!found && recover) { m.recovering = true; }
      if (temple && !found) { ns = BotState::Roam; m.goal = templeGoal(); }
      if (ns == BotState::Roam && !temple) {
        // Hunters and Brawlers roam toward the busy middle of the zone; Survivors keep to its quiet edge.
        const float inner = m.recovering ? 0.3f : m.style == BotStyle::Hunter || m.style == BotStyle::Brawler ? 0.5f
                          : m.style == BotStyle::Survivor ? 0.9f : 0.75f;
        const bool arrived = (m.goal - me.pos).len() < 200.0f;
        if (m.state != BotState::Roam || arrived || m.stateTime > 14.0f || !botSafePoint(w, m.goal, 100.0f) ||
            nearGuardian(m.goal, ti.guardSense)) {
          m.goal = rampage ? botSpreadGoal(w, me) : botPickRoamGoal(w, Z, inner);
          if (m.state == BotState::Roam) m.stateTime = 0;
        }
      }
    }
    if (ns != m.state) { m.state = ns; m.stateTime = 0; }
    if (m.state != BotState::Engage) m.targetId = 0;

    // Loot on the way: smart bots grab close upgrades even mid-fight or while running away.
    m.grab = false;
    if (npc(me)) {}
    else if (m.state == BotState::Retreat) findGrab(nNear ? centroid : me.pos, true);
    else if (m.state == BotState::Engage && best && best->d > 250.0f) findGrab(best->p, false);

    // ---- Superpower use: each one has a moment where it actually helps (lower tiers often miss it) ----
    // Newbies have no sense of timing: they fire it off whenever, often wasting it.
    if (newbie && !npc(me) && me.held != Power::None && me.active == Power::None && me.recharge <= 0)
      in.usePowerUp = w.rng.f01() < (nNear > 0 ? 0.3f : 0.05f);
    else if (me.held != Power::None && me.active == Power::None && me.recharge <= 0 && w.rng.f01() < ti.usePower) {
      const bool inFight = m.state == BotState::Engage && best;
      const float bd = best ? best->d : 1e9f;
      const bool clear = best && !w.lineBlocked(me.pos, best->p);
      bool use = false;
      switch (me.held) {
        case Power::Speed:       use = m.state == BotState::Retreat || (m.state == BotState::Zone && outBy > -50.0f) ||
                                       (inFight && m.style == BotStyle::Hunter && bd > myRange); break;
        case Power::Shield:      use = nNear > 0 && m.hurtTimer < 0.4f; break;
        case Power::Rocket:      use = best && bd < 1300.0f && clear && best->t->health > 30.0f; break;
        case Power::Homing:      use = best && bd < 800.0f; break;
        case Power::RapidShot:
        case Power::Spread:      use = inFight && bd < myRange * 0.85f; break;
        case Power::AirStrike:   use = best && bd < 800.0f && bd > 200.0f; break;
        case Power::Lightning:   use = best && bd < 380.0f; break;
        case Power::Dash:        use = m.state == BotState::Retreat && nNear > 0; break;
        case Power::Turret:      use = inFight && bd < kTurretRange; break;
        case Power::Teleport:    use = (m.state == BotState::Retreat && myStr < 0.35f) || (m.state == BotState::Zone && outBy > 0); break;
        case Power::Invisible:   use = m.state == BotState::Retreat && nNear > 0; break;
        case Power::Singularity: use = inFight && bd < 700.0f; break;
        case Power::Instinct:    use = inFight; break;
        default: break;
      }
      in.usePowerUp = use;
    }
  }

  // Instinct: after being hit, try to press dodge in time (a reaction delay; Newbies are often too late).
  if (me.active == Power::Instinct && me.pendingT > 0) {
    if (m.instinctWait < 0) m.instinctWait = w.rng.range(0.25f, 0.9f) * (1.4f - m.skill);
    m.instinctWait -= kDt;
    if (m.instinctWait <= 0) { in.usePowerUp = true; m.instinctWait = -1; }
  } else {
    m.instinctWait = -1;
  }

  // ======================= 2) STEER =======================
  const Foe* tgt = nullptr;
  if (m.state == BotState::Engage)
    for (int i = 0; i < nf; ++i) if (foes[i].t->id == m.targetId) tgt = &foes[i];
  if (tgt) { m.lastSeen = tgt->p; m.lostTimer = 0; }

  // Omnipotent (a test bot): much stronger in a fight, teleports to whatever it's after, and blinds whoever it fights.
  if (m.tier == BotTier::Omnipotent) {
    m.teleT -= kDt;
    m.blindCd -= kDt;
    me.powerMul = m.basePower * (m.state == BotState::Engage ? 1.4f : 1.0f);
    Vec2 dest;
    bool go = false;
    if (m.state == BotState::Engage && tgt && tgt->d > 800.0f) { dest = tgt->p + (me.pos - tgt->p).norm() * 420.0f; go = true; }
    else if (m.state != BotState::Engage && m.state != BotState::Retreat && (m.goal - me.pos).len() > 1500.0f) { dest = m.goal; go = true; }
    if (go && m.teleT <= 0 && !temple) {
      dest.x = std::clamp(dest.x, w.lo.x + 200.0f, w.hi.x - 200.0f);
      dest.y = std::clamp(dest.y, w.lo.y + 200.0f, w.hi.y - 200.0f);
      dest = w.clearOf(dest, me.radius + 10.0f);
      w.emit(EvType::Teleport, me.pos, 0, me.id);
      me.pos = dest; me.vel = {};
      w.emit(EvType::Teleport, me.pos, 1, me.id);
      m.teleT = 3.0f;
    }
    m.pullCd -= kDt;
    if (tgt && tgt->d > 350.0f && m.pullCd <= 0 && !temple) {  // yank whoever it's fighting right next to it
      if (Tank* v = w.find(tgt->t->id)) {
        w.emit(EvType::Teleport, v->pos, 0, v->id);
        v->pos = w.clearOf(me.pos + (v->pos - me.pos).norm() * 230.0f, v->radius + 10.0f);
        v->vel = {};
        w.emit(EvType::Teleport, v->pos, 1, v->id);
      }
      m.pullCd = 6.0f;
    }
    if (tgt && tgt->d < 900.0f && m.blindCd <= 0) {
      if (Tank* v = w.find(tgt->t->id)) { v->blindT = 2.5f; w.emit(EvType::Blinded, v->pos, 0, v->id, me.id); }
      m.blindCd = 8.0f;
    }
  } else if (!m.custom) {
    me.powerMul = 1.0f;
  }
  if (m.state == BotState::Engage && !tgt && (m.lostTimer > si.chase || (m.lastSeen - me.pos).len() < 80.0f)) {
    m.state = BotState::Roam; m.stateTime = 0; m.decideTimer = 0; m.targetId = 0;
  }

  if (m.strafeTimer <= 0) {
    // Smart bots juke unpredictably; Newbies strafe one way for ages (or stand still).
    const bool still = m.iq < 0.2f && w.rng.f01() < 0.12f;
    m.strafeSign = still ? 0.0f : (w.rng.f01() < 0.5f ? 1.0f : -1.0f);
    m.strafeTimer = w.rng.range(0.4f, 1.1f) + (1.0f - m.iq) * w.rng.range(0.8f, 2.0f);
  }

  // Going somewhere with a wall in the way: head for a clear spot round it first (on the map; temples have waypoints).
  auto route = [&](Vec2 goal) -> Vec2 {
    if (temple) return goal;
    if (m.detourT > 0) {
      if ((m.detour - me.pos).len() > 60.0f) return m.detour;
      m.detourT = 0;
    }
    if (!w.lineBlocked(me.pos, goal, me.radius * 0.6f)) return goal;
    float bestS = 1e9f;
    Vec2 bestP = goal;
    for (int i = 0; i < 16; ++i) {
      const float a = i * 0.3927f;
      for (float r : {240.0f, 480.0f}) {
        const Vec2 p = me.pos + Vec2{std::cos(a), std::sin(a)} * r;
        if (!w.inAnyArea(p) || w.inWall(p, me.radius + 20.0f) || w.lineBlocked(me.pos, p, me.radius * 0.8f)) continue;
        const float sc = (goal - p).len() + (w.lineBlocked(p, goal, me.radius * 0.6f) ? 350.0f : 0.0f) +
                         ((p - m.detour).len() < 50.0f ? 200.0f : 0.0f);  // not straight back to the last one
        if (sc < bestS) { bestS = sc; bestP = p; }
      }
    }
    if (bestS < 1e8f) { m.detour = bestP; m.detourT = 1.5f; return bestP; }
    return goal;
  };

  Vec2 want;
  switch (m.state) {
    case BotState::Zone: {
      Vec2 g = Z.c;
      if (Z.r > 1.0f) g = Z.c + (me.pos - Z.c).norm() * std::min((me.pos - Z.c).len(), Z.r * 0.55f);
      want = (route(g) - me.pos).norm();
      if (me.held == Power::Teleport) { in.tx = g.x; in.ty = g.y; }
      break;
    }
    case BotState::Roam:
    case BotState::Loot:
    case BotState::Farm: {
      if (temple && m.state == BotState::Roam && !me.mob && !me.sentryOf) m.goal = templeGoal();
      if (me.sentryOf && boss) { const float a = (float)(me.id % 7) * 0.9f; m.goal = boss->pos + Vec2{std::cos(a), std::sin(a)} * 110.0f; }
      // Shoot crates from a distance (once nothing is in the way).
      const bool clearShot = m.crateId && !w.lineBlocked(me.pos, m.goal);
      const float stop = clearShot ? 260.0f : me.sentryOf ? 60.0f : 40.0f;
      const Vec2 d = m.goal - me.pos;
      want = d.len() > stop ? ((clearShot ? m.goal : route(m.goal)) - me.pos).norm() : Vec2{};
      break;
    }
    case BotState::Engage: {
      if (!tgt) {  // chasing: head to where it was last seen
        want = (route(m.lastSeen) - me.pos).norm();
        break;
      }
      const Vec2 d = tgt->p - me.pos;
      const float len = d.len();
      const float pref = myRange * std::clamp(si.range - 0.1f * (m.aggression - 1.0f), 0.4f, 0.9f);
      want = d.norm() * std::clamp((len - pref) / 150.0f, -1.0f, 1.0f) + perp2(d.norm()) * (0.85f * m.strafeSign);
      break;
    }
    case BotState::Retreat: {
      Vec2 away;
      for (int i = 0; i < nf; ++i) away += (me.pos - foes[i].p).norm() * (1.0f + (kNearFoe - std::min(foes[i].d, kNearFoe)) / kNearFoe);
      for (const Turret& tu : w.turrets)
        if (tu.owner != me.id && (tu.pos - me.pos).len() < kTurretRange + 80.0f) away += (me.pos - tu.pos).norm() * 1.5f;
      away = away.len() > 0.01f ? away.norm() : (ring.cur.c - me.pos).norm();
      if (!temple) away = (away + (ring.next.c - me.pos).norm() * 0.25f * m.iq).norm();  // smart bots run toward the safe zone
      want = away + perp2(away) * (0.35f * m.strafeSign);
      const Vec2 tp = me.pos + away * 700.0f;
      in.tx = tp.x; in.ty = tp.y;  // Teleport escapes this way
      break;
    }
  }
  if (m.grab) {
    const Vec2 d = m.grabPos - me.pos;
    if (d.len() < 30.0f) m.grab = false;
    else want = d.norm() * 1.6f + want * 0.4f;
  }

  // Environment: map edge, the storm edge, calm guardians, walls, enemy turrets, singularities, bombs about to go off.
  Vec2 env;
  if (!temple) {
    const Vec2 a = w.lo + Vec2{kWallMargin, kWallMargin}, b = w.hi - Vec2{kWallMargin, kWallMargin};
    if (me.pos.x < a.x) env.x += (a.x - me.pos.x) / kWallMargin;
    if (me.pos.x > b.x) env.x -= (me.pos.x - b.x) / kWallMargin;
    if (me.pos.y < a.y) env.y += (a.y - me.pos.y) / kWallMargin;
    if (me.pos.y > b.y) env.y -= (me.pos.y - b.y) / kWallMargin;
  }
  if (!temple && m.state != BotState::Zone && ring.cur.r < 1e8f && distC > ring.cur.r - 300.0f)
    env += (ring.cur.c - me.pos).norm() * (std::clamp((distC - (ring.cur.r - 300.0f)) / 300.0f, 0.0f, 1.0f) * 1.3f);
  for (const Tank& g : w.tanks) {
    if (!g.alive || !g.guardian || (m.state == BotState::Engage && m.targetId == g.id)) continue;
    const float d = (g.pos - me.pos).len();
    if (d < ti.guardSense) env += (me.pos - g.pos).norm() * ((ti.guardSense - d) / ti.guardSense * 2.0f);
  }
  // Walls: a gentle push away from the surface (temple corridors are narrow), and remember the closest one to slide on.
  const float wallR = temple ? 45.0f : 110.0f;
  Vec2 wallN;
  float wallGap = 1e9f;
  for (const Wall& wl : w.walls) {
    if (std::fabs(wl.a.x - me.pos.x) > 1600.0f && std::fabs(wl.b.x - me.pos.x) > 1600.0f) continue;
    const Vec2 cp = closestOnSeg(wl.a, wl.b, me.pos);
    const float d = (me.pos - cp).len(), gap = d - wl.thick * 0.5f - me.radius;  // from our edge to its surface
    if (d < 0.01f) continue;
    if (gap < wallR) env += (me.pos - cp) * (1.0f / d) * (std::max(0.0f, wallR - gap) / wallR * 1.2f);
    if (gap < wallGap) { wallGap = gap; wallN = (me.pos - cp) * (1.0f / d); }
  }
  for (const LootBox& b : w.boxes) {
    const float d = (b.pos - me.pos).len(), R = boxRadius(b.kind) + me.radius + 60.0f;
    if (d < R && d > 1.0f) env += (me.pos - b.pos).norm() * ((R - d) / R * 1.2f);
  }
  if (m.style != BotStyle::Brawler || myStr < 0.5f)  // turrets: get out of reach (Brawlers only when hurt)
    for (const Turret& tu : w.turrets) {
      if (tu.owner == me.id) continue;
      const float d = (tu.pos - me.pos).len(), R = kTurretRange + 60.0f;
      if (d < R && w.rng.f01() < 0.3f + 0.7f * m.iq) env += (me.pos - tu.pos).norm() * ((R - d) / R * 1.8f);
    }
  for (const Singularity& s : w.sings) {
    if (s.owner == me.id) continue;
    const float d = (s.pos - me.pos).len(), R = kSingRadius + 150.0f;
    if (d < R) env += (me.pos - s.pos).norm() * ((R - d) / R * 2.5f);
  }
  for (const Bomb& b : w.bombs) {
    if (b.owner == me.id) continue;
    const float d = (b.pos - me.pos).len(), R = kBombBlast + me.radius + 40.0f;
    if (d < R && ((b.id * 2654435761u) >> 24) < 255.0f * (0.3f + 0.7f * m.iq)) env += (me.pos - b.pos).norm() * 3.0f;
  }
  want += env * 1.5f;

  // Dodging, like a person: it only notices shots on its screen, judges each one once, and if it decides to dodge it
  // only starts after its reaction time (too late if the shot is already there). Lower tiers sometimes step the wrong
  // way. Only the Unreal-and-up test bots react at computer speed.
  if (m.dodgePlanT >= 0) {
    m.dodgePlanT -= kDt;
    if (m.dodgePlanT < 0) { m.dodgeDir = m.dodgePlanDir; m.dodgeTimer = 0.3f; m.dodgePlanT = -1; }
  } else if (m.dodgeTimer <= 0 && m.dodgeCooldown <= 0 && m.dodgeChance > 0) {
    Vec2 threatVel, threatClosest;
    float threatT = 1e9f;
    bool rocket = false;
    uint32_t threatId = 0;
    auto check = [&](uint32_t id, Vec2 pos, Vec2 vel, float reachR, float left, bool isRocket) {
      const Vec2 p = pos - me.pos;
      if (p.len() > 700.0f || !botSees(me.pos, pos, 0.0f)) return;
      for (uint32_t j : m.judged) if (j == id) return;  // already made up its mind about this one
      const float vv = dot2(vel, vel);
      if (vv < 1.0f) return;
      const float ts = -dot2(p, vel) / vv;
      if (ts < 0.02f || ts > 0.9f || ts * std::sqrt(vv) > left) return;
      const Vec2 closest = p + vel * ts;
      if (closest.len() < me.radius + reachR + 14.0f && ts < threatT) {
        threatT = ts; threatVel = vel; threatClosest = closest; rocket = isRocket; threatId = id;
      }
    };
    for (const Bullet& b : w.bullets) if (b.owner != me.id) check(b.id, b.pos, b.vel, b.radius, b.rangeLeft, false);
    for (const Rocket& r : w.rockets) if (r.owner != me.id) check(r.id, r.pos, r.vel, kRocketBlast * 0.7f, r.rangeLeft, true);
    if (threatId) {
      m.judged[m.judgedN] = threatId;
      m.judgedN = (m.judgedN + 1) % 6;
      m.dodgeCooldown = 0.2f;
      const float react = cheat ? m.dodgeReact : m.dodgeReact * w.rng.range(0.85f, 1.35f);
      if (w.rng.f01() < m.dodgeChance * (rocket ? 1.2f : 1.0f) && react < threatT - 0.08f) {
        const Vec2 n = perp2(threatVel.norm());
        Vec2 dir = dot2(n, threatClosest * -1.0f) >= 0 ? n : n * -1.0f;
        if (threatClosest.len() < 4.0f) dir = n * (m.strafeSign != 0 ? m.strafeSign : 1.0f);
        if (!cheat && w.rng.f01() < (1.0f - m.skill) * 0.35f) dir = dir * -1.0f;  // guessed the wrong way
        if (!botSafePoint(w, me.pos + dir * 200.0f, 60.0f)) dir = dir * -1.0f;
        m.dodgePlanDir = dir;
        m.dodgePlanT = react;
        m.dodgeCooldown = 0.9f;
        // A smart bot pops its Shield (or Dash) when a rocket is about to land.
        if (rocket && m.iq >= 0.45f && me.active == Power::None && me.recharge <= 0 &&
            (me.held == Power::Shield || me.held == Power::Dash) && threatT < 0.6f)
          in.usePowerUp = true;
      }
    }
  }
  if (m.dodgeTimer > 0) want = m.dodgeDir * 2.0f + want * 0.25f;

  // Going for loot but not getting any closer (round and round a wall): give up on it for a while.
  if (m.state == BotState::Loot) {
    const float dg = (m.goal - me.pos).len();
    if ((m.goal - m.lootGoal).len() > 30.0f) { m.lootGoal = m.goal; m.lootBest = dg; m.lootT = 0; }
    if (dg < m.lootBest - 40.0f) { m.lootBest = dg; m.lootT = 0; }
    else if (!(m.crateId && !w.lineBlocked(me.pos, m.goal))) m.lootT += kDt;
    if (m.lootT > 3.0f) {
      m.skipPos = m.goal; m.skipT = 10.0f; m.lootT = 0; m.crateId = 0;
      m.state = BotState::Roam; m.stateTime = 0; m.decideTimer = 0;
      m.goal = temple ? m.goal : botPickRoamGoal(w, Z, 0.75f);
    }
  }

  // Up against a wall and pushing into it: slide along it instead (and keep sliding the same way for a moment, so it
  // doesn't flip back and forth).
  if (wallGap < 30.0f && want.len() > 0.1f && dot2(want, wallN) < -0.05f) {
    const Vec2 tg = perp2(wallN);
    if (m.slideT <= 0) { m.slideSign = dot2(want, tg) >= 0 ? 1.0f : -1.0f; }
    m.slideT = 0.8f;
    want = tg * (m.slideSign * want.len()) + wallN * 0.15f;
  }

  // Anti-stuck.
  if (m.unstickTimer > 0) {
    m.unstickTimer -= kDt;
    want = m.unstickDir;
  } else if ((me.pos - m.stuckPos).len() > 60.0f || m.state == BotState::Engage || m.state == BotState::Retreat ||
             (m.goal - me.pos).len() < 90.0f || (m.crateId && !w.lineBlocked(me.pos, m.goal)) ||
             ((me.mob || me.sentryOf) && m.state == BotState::Roam)) {
    // Moving, fighting, there, or happily shooting a crate open: not stuck.
    m.stuckPos = me.pos;
    m.stuckTimer = 0;
  } else {
    m.stuckTimer += kDt;
    if (m.stuckTimer > 1.2f) {
      const float a = w.rng.range(0, 6.2831853f);
      const Vec2 mid = temple ? (temple->path.empty() ? temple->c : temple->path[std::min(m.wp, (int)temple->path.size() - 1)])
                              : w.middle();
      m.unstickDir = (mid - me.pos).norm() * 0.6f + Vec2{std::cos(a), std::sin(a)};
      m.unstickDir = m.unstickDir.norm();
      m.unstickTimer = temple ? 0.4f : 0.8f;
      m.stuckTimer = 0;
      m.slideSign = -m.slideSign;  // try the wall's other way
      m.detourT = 0;
      if (m.state == BotState::Loot) { m.skipPos = m.goal; m.skipT = 8.0f; m.crateId = 0; }
      if (!temple && !me.mob && !me.sentryOf) m.goal = botPickRoamGoal(w, Z, 0.75f);
      m.decideTimer = 0;
    }
  }

  const bool hurry = m.state == BotState::Retreat || (m.state == BotState::Zone && outBy > -50.0f) || m.grab ||
                     (m.state == BotState::Engage && !tgt);
  in.speed = m.dodgeTimer > 0 ? 1.0f : (hurry ? 0.9f : m.state == BotState::Engage ? 0.75f : 0.62f);
  in.turn = m.turn;
  // Ease the move direction toward what it wants (quickly when dodging), then press keys with a little hysteresis: a key
  // goes down past 0.42 and only comes up below 0.2, so a bot near a 45-degree line doesn't flicker.
  m.steer += (want - m.steer) * std::min(1.0f, kDt * (m.dodgeTimer > 0 ? 16.0f : 7.0f));
  const Vec2 n = m.steer.len() > 0.12f ? m.steer.norm() : Vec2{};
  auto key = [](float v, int8_t prev) -> int8_t {
    if (prev > 0 && v > 0.2f) return 1;
    if (prev < 0 && v < -0.2f) return -1;
    return v > 0.42f ? 1 : (v < -0.42f ? -1 : 0);
  };
  m.keyX = key(n.x, m.keyX);
  m.keyY = key(n.y, m.keyY);
  in.moveX = m.keyX;
  in.moveY = m.keyY;

  // Relics: lean on Repair when hurt and nobody is around; smart bots overload their Sentry in a fight.
  for (int s = 0; s < 2; ++s) {
    if (me.relic[s] == Relic::Repair) in.relicDown[s] = myStr < 0.5f && nf == 0 && turretDps == 0;
    if (me.relic[s] == Relic::Sentry) in.relicDown[s] = m.iq >= 0.7f && m.state == BotState::Engage && tgt && myStr > 0.45f;
  }

  // ======================= 3) AIM & FIRE =======================
  const Foe* ft = nullptr;
  float fd = 1e9f;
  if (m.state == BotState::Engage && tgt) {
    ft = tgt;
    fd = tgt->d;
  } else {
    for (int i = 0; i < nf; ++i)
      if (foes[i].d < myRange * 0.92f && foes[i].d < fd) { fd = foes[i].d; ft = &foes[i]; }
  }

  Vec2 aimPoint;
  bool haveAim = false;
  if (ft) {
    // The bot's idea of where the target is going lags behind reality, so sharp turns (and Speed Boost) shake it off.
    const Vec2 realVel = isHidden(*ft->t) ? Vec2{} : ft->t->vel;
    if (m.estFor != ft->t->id) { m.estFor = ft->t->id; m.estVel = {}; }
    m.estVel += (realVel - m.estVel) * (1.0f - std::exp(-kDt / m.trackTau));
    aimPoint = ft->p + m.estVel * ((fd / myBulletSpeed) * m.lead);
    if (cheat) {  // test bots: solve the intercept exactly, with the real velocity
      float tt = fd / myBulletSpeed;
      for (int k = 0; k < 3; ++k) tt = ((ft->p + realVel * tt) - me.pos).len() / myBulletSpeed;
      aimPoint = ft->p + realVel * tt;
    }
    haveAim = true;
    if (in.tx == 0 && in.ty == 0) {  // rockets, strikes, singularity aim here (smart bots lead them too)
      const Vec2 ahead = ft->p + m.estVel * (0.4f * m.lead);
      in.tx = ahead.x; in.ty = ahead.y;
    }
    if (ft->t->id != m.lastFireId) {
      m.lastFireId = ft->t->id;
      m.react = w.rng.range(m.reactMin, m.reactMax);
    }
  } else {
    m.lastFireId = 0;
  }

  if (thought) {
    float amp = m.aimNoise + (haveAim ? fd / myRange * 0.04f : 0.0f);
    if (me.vel.len() > 250.0f) amp *= 1.3f;  // shooting on the move is harder
    m.aimErrTarget = w.rng.range(-1.0f, 1.0f) * amp;
  }
  const float errNow = m.aimErrTarget * (m.dodgeTimer > 0 ? 1.8f : 1.0f);  // dodging wrecks the aim
  m.aimErr += (errNow - m.aimErr) * std::min(1.0f, 6.0f * kDt);

  if (haveAim) {
    const Vec2 d = aimPoint - me.pos;
    in.aim = std::atan2(d.y, d.x) + m.aimErr;
    m.burst -= kDt;
    if (m.burst < -w.rng.range(0.3f, 0.7f) * (1.6f - m.iq)) m.burst = w.rng.range(0.9f, 1.8f);
    // Invisible bots hold fire while sneaking away (shooting gives them away).
    const bool sneaking = isHidden(me) && m.state == BotState::Retreat;
    in.shoot = !sneaking && m.react <= 0 && m.burst > 0 && fd < myRange * 0.95f &&
               std::fabs(angleDiff(me.aim, in.aim)) < 0.12f + m.aimNoise * 0.5f && !w.lineBlocked(me.pos, aimPoint);
  } else {
    in.aim = n.len() > 0.1f ? std::atan2(n.y, n.x) : me.aim;
    if (m.crateId)  // nobody around: shoot the crate open
      for (const LootBox& b : w.boxes)
        if (b.id == m.crateId && (b.pos - me.pos).len() < myRange * 0.9f && !w.lineBlocked(me.pos, b.pos)) {
          const Vec2 d = b.pos - me.pos;
          in.aim = std::atan2(d.y, d.x) + m.aimErr * 0.5f;
          in.shoot = std::fabs(angleDiff(me.aim, in.aim)) < 0.15f;
        }
  }
  // What it's doing, for the dev panel's Bot Info Overlay (and the spectator panel).
  {
    BotGoal g = BotGoal::Roam;
    if (me.mob) g = m.state == BotState::Engage ? BotGoal::Attack : BotGoal::Guard;
    else if (me.sentryOf) g = m.state == BotState::Engage ? BotGoal::Attack : BotGoal::Follow;
    else switch (m.state) {
      case BotState::Zone: g = BotGoal::Zone; break;
      case BotState::Retreat: g = BotGoal::Retreat; break;
      case BotState::Engage:
        g = m.contestId && m.contestId == m.targetId ? BotGoal::Contest : (tgt && tgt->vis ? BotGoal::Attack : BotGoal::Hunt);
        break;
      case BotState::Loot: {
        bool door = false;
        for (const Temple& T : w.temples) door |= (T.door - m.goal).len() < 5.0f;
        g = door ? BotGoal::Raid : m.crateId ? BotGoal::Crate : m.recovering ? BotGoal::Recover : BotGoal::Loot;
        break;
      }
      default: g = temple ? BotGoal::Raid : m.recovering ? BotGoal::Recover : BotGoal::Roam;
    }
    m.objective = g;
  }
  return in;
}

}  // namespace tonk
#include "core/bot_presets.h"  // presetInput() (needs everything above)
namespace tonk {

// Arena Mage (a big guardian): stays near its post in the middle and does nothing until someone hurts it or walks into its (small)
// field of view. Then it aims with near-perfect prediction and shoots heavy, slow bullets. It never leaves its post for
// long, never fights the other guardians, and the storm doesn't hurt it.
inline Input guardianInput(World& w, Tank& g) {
  Input in;
  in.turn = 1.0f;
  const Tank* tgt = nullptr;
  if (g.aggroT > 0) {
    const Tank* a = w.find(g.aggroId);
    if (a && a->alive && hostile(g, *a) && canSpot(*a) && (seenPos(*a) - g.pos).len() < kGuardWeapon.range + 300.0f) tgt = a;
  }
  if (!tgt) {
    float bd = kGuardFov;
    for (const Tank& o : w.tanks) {
      if (!o.alive || !hostile(g, o) || !canSpot(o)) continue;
      const float d = (seenPos(o) - g.pos).len();
      if (d < bd && !w.lineBlocked(g.pos, seenPos(o))) { bd = d; tgt = &o; }
    }
    if (tgt) { g.aggroId = tgt->id; g.aggroT = 6.0f; }
  }
  if (!tgt && g.aggroT <= 0) g.aggroId = 0;

  // Circle-strafe the target while staying close to its post; walk home when calm.
  const Vec2 toHome = g.home - g.pos;
  Vec2 want;
  if (tgt) {
    const float pull = std::clamp(toHome.len() / kGuardLeash, 0.0f, 1.5f);
    want = perp2((seenPos(*tgt) - g.pos).norm()) * 0.6f + toHome.norm() * pull;
  } else if (toHome.len() > 40.0f) {
    want = toHome.norm();
  }
  const Vec2 n = want.norm();
  in.moveX = n.x > 0.38f ? 1 : (n.x < -0.38f ? -1 : 0);
  in.moveY = n.y > 0.38f ? 1 : (n.y < -0.38f ? -1 : 0);

  if (tgt) {
    const Vec2 p = seenPos(*tgt);
    const Vec2 v = isHidden(*tgt) ? Vec2{} : tgt->vel;
    // Solve the intercept: where the target will be when a bullet gets there.
    float t = (p - g.pos).len() / kGuardWeapon.bulletSpeed;
    for (int k = 0; k < 3; ++k) t = ((p + v * t) - g.pos).len() / kGuardWeapon.bulletSpeed;
    const Vec2 aimAt = p + v * t;
    in.aim = std::atan2(aimAt.y - g.pos.y, aimAt.x - g.pos.x) + w.rng.range(-0.025f, 0.025f);
    // Don't fire through another guardian.
    bool friendInWay = false;
    for (const Tank& o : w.tanks)
      if (o.alive && arenaSide(o) && o.id != g.id) {
        const Vec2 c = closestOnSeg(g.pos, aimAt, o.pos);
        friendInWay |= (c - o.pos).len() < o.radius + kGuardWeapon.bulletRadius;
      }
    in.shoot = !friendInWay && std::fabs(angleDiff(g.aim, in.aim)) < 0.06f && (aimAt - g.pos).len() < kGuardWeapon.range &&
               !w.lineBlocked(g.pos, aimAt);
  } else {
    in.aim = g.aim + 0.3f * kDt;  // slowly scanning
  }
  return in;
}

}  // namespace tonk
