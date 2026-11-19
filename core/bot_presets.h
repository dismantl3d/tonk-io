#pragma once
// Preset bots: three hand-designed brains that can be spawned from the dev panel (BOTS > Presets).
//
//   1  Aegis-Vulture : third-party opportunist. Loots along an inward spiral, refuses even fights when hurt, collapses on
//                      wounded tanks and tanks that are busy shooting each other.
//   2  Ghost Current : the adaptive predator. Fights from the sides, keeps escape routes open, "Current Shift" (a tangential
//                      move that breaks the enemy's firing angle) instead of running straight away.
//   3  Carrion Clock : treats the storm as a clock and other tanks as food. Loots early, lives on the shoulder of the ring and
//                      only takes fights with a positive Fight Value Score (FVS).
//
// They obey the same rules as every other bot (GAME_DESIGN.md section 7.8):
//   - same body, same Standard weapon, same damage: no power multiplier, no free upgrades;
//   - they only act on what a player could see (the screen rectangle, minimap ring, visible tanks and bullets), keep their
//     own short memory of observed positions to estimate velocity (never the true velocity), dodge only after a reaction
//     delay and judge each bullet once;
//   - they only produce an Input (WASD, aim, shoot, superpower, relics, F).
// They stay out of the Arena Temples; inside one (or in Rocket Rampage) botInput() falls back to the normal brain.
#include "core/bot.h"

namespace tonk {

inline const char* brainName(int b) {
  static const char* k[] = {"Standard", "Aegis-Vulture", "Ghost Current", "Carrion Clock"};
  return k[std::clamp(b, 0, (int)Brain::Count - 1)];
}

namespace pre {

// Every tunable number of the three brains lives here.
struct Cfg {
  float think;                 // s between strategic decisions
  float reactMin, reactMax;    // dodge reaction delay (s)
  float dodgeR;                // a bullet passing closer than tank radius + this is a threat
  float dodgeHorizon;          // ...if it arrives within this many seconds
  float dodgeHold;             // how long a dodge lasts (s)
  int dodgeBounce;             // wall bounces projected when judging a bullet
  float aimSigma;              // aim error (radians, ~1 sigma)
  float acquire;               // delay before shooting a newly acquired target (s)
  float leadMul;               // damping on the lead
  float horizon;               // cap on the prediction horizon (s)
  int iters;                   // intercept iterations
  float fireRange;             // shoot up to this share of the weapon's range
  float aimTol;                // max aim error (radians) to pull the trigger
  float pref;                  // preferred fight distance, share of weapon range
  float flipMin, flipMax;      // strafe direction flip interval (s)
  int rays; float rayLen, wallW;  // wall feelers
  float ringPush, ringW;       // push inward when closer than this to the storm edge
  float stuckWin, stuckDisp, stuckDur;  // stuck: moved < stuckDisp in stuckWin seconds -> unstick for stuckDur
  float turn;                  // barrel turn rate scale
  float statW[6];              // loot value per stat (Speed, BulletSpeed, Range, Health, FireRate, Damage)
};
// aim sigma: Aegis 1.8 deg, Ghost ~1.5 deg (1-3% of the distance), Carrion 1.5 deg
inline const Cfg& cfgOf(int b) {
  //                  think  rMin   rMax   dR   horiz hold  bnc sigma   acq   lead  hor  it  fire  tol   pref  flip       rays len  wW   ringP ringW stkW  stkD stkT  turn  stats
  static const Cfg A{0.20f, 0.167f, 0.233f, 18.0f, 1.20f, 0.35f, 1, 0.0314f, 0.12f, 0.88f, 2.0f, 1, 1.05f, 0.075f, 0.75f, 1.1f, 1.9f, 7, 180.0f, 2.2f, 250.0f, 1.6f, 1.5f, 45.0f, 0.8f, 0.8f, {1.0f, 0.5f, 0.5f, 0.8f, 0.8f, 1.0f}};
  static const Cfg G{0.30f, 0.180f, 0.320f, 72.0f, 0.70f, 0.40f, 0, 0.0262f, 0.15f, 1.00f, 0.8f, 1, 0.95f, 0.080f, 0.72f, 1.2f, 2.0f, 5, 200.0f, 1.8f, 350.0f, 1.8f, 1.0f, 40.0f, 0.6f, 0.8f, {0.7f, 0.5f, 0.4f, 1.0f, 0.9f, 0.8f}};
  static const Cfg R{0.20f, 0.133f, 0.233f, 25.0f, 0.80f, 0.267f, 0, 0.0262f, 0.133f, 1.00f, 1.5f, 3, 1.10f, 0.070f, 0.70f, 0.5f, 1.3f, 8, 250.0f, 2.0f, 400.0f, 2.5f, 1.5f, 40.0f, 0.83f, 0.6f, {0.7f, 0.4f, 0.5f, 0.8f, 1.0f, 0.9f}};
  return b == 1 ? A : b == 2 ? G : R;
}

inline float rankPower(int b, Power p) {
  if (b == 1) {  // Aegis: three tiers
    switch (p) {
      case Power::Singularity: case Power::Instinct: case Power::Homing: case Power::Shield: return 3;
      case Power::Teleport: case Power::Dash: case Power::Invisible: case Power::Lightning: case Power::RapidShot: return 2;
      case Power::None: return 0;
      default: return 1;
    }
  }
  if (b == 3) {  // Carrion: its own order, but a Mythic always wins
    static const Power ord[] = {Power::Shield, Power::Dash, Power::Teleport, Power::Rocket, Power::Homing, Power::Lightning, Power::Speed,
                                Power::RapidShot, Power::Spread, Power::Turret, Power::SentryBot, Power::Invisible, Power::AirStrike,
                                Power::Singularity, Power::Instinct};
    if (isMythic(p)) return 100.0f - (p == Power::Singularity ? 0 : p == Power::Instinct ? 1 : 2);
    for (int i = 0; i < 15; ++i) if (ord[i] == p) return 30.0f - i;
    return 0;
  }
  switch (p) {  // Ghost: a rough usefulness score; it only swaps for something clearly better
    case Power::Shield: case Power::Dash: case Power::Teleport: case Power::Singularity: return 8;
    case Power::Homing: case Power::Rocket: case Power::Lightning: return 7;
    case Power::Speed: case Power::Invisible: case Power::RapidShot: case Power::SentryBot: return 6;
    case Power::Spread: case Power::Turret: case Power::Instinct: return 5;
    case Power::AirStrike: return 4;
    default: return 0;
  }
}
inline float rankRelic(int b, Relic r) {
  switch (r) {
    case Relic::Repair: return 4;
    case Relic::Radar: return b == 2 ? 3 : 3;
    case Relic::Sentry: return b == 2 ? 3.5f : 2;
    case Relic::Loot: return 1.5f;
    default: return 0;
  }
}

inline float pAng(Vec2 v) { return std::atan2(v.y, v.x); }
inline Vec2 pDir(float a) { return {std::cos(a), std::sin(a)}; }
inline float pGauss(Rng& r) { return (r.f01() + r.f01() + r.f01() + r.f01() - 2.0f) * 1.7320508f; }  // ~N(0,1)

// ---- Nearby walls (cached once per tick) ----
struct Wl { Vec2 a, b; float th; };
struct Near { Wl w[80]; int n = 0; };
inline void gatherWalls(const World& w, Vec2 p, float R, Near& nr) {
  nr.n = 0;
  for (const Wall& wl : w.walls) {
    if (wl.temple >= 0) continue;
    if ((closestOnSeg(wl.a, wl.b, p) - p).len() > R + wl.thick * 0.5f) continue;
    if (nr.n < 80) nr.w[nr.n++] = {wl.a, wl.b, wl.thick};
  }
}
// Distance along the unit direction d to the first wall face (1e9: none within maxLen). nrm: the face's normal.
inline float rayHit(const Near& nr, Vec2 p, Vec2 d, float maxLen, Vec2* nrm = nullptr) {
  float best = 1e9f;
  for (int i = 0; i < nr.n; ++i) {
    const Wl& L = nr.w[i];
    const Vec2 s = L.b - L.a, q = L.a - p;
    const float den = cross2(d, s);
    if (std::fabs(den) < 1e-6f) continue;
    const float t = cross2(q, s) / den, u = cross2(q, d) / den;
    if (u < 0 || u > 1 || t < 0 || t > maxLen) continue;
    Vec2 n = perp2(s).norm();
    if (dot2(n, d) > 0) n = n * -1.0f;
    const float tt = std::max(0.0f, t - L.th * 0.5f / std::max(0.2f, -dot2(n, d)));
    if (tt < best) { best = tt; if (nrm) *nrm = n; }
  }
  return best;
}
// A one-bounce shot: aim at the mirror image of the target across a wall face. Both legs must be clear.
inline bool bounceAim(const World& w, const Near& nr, Vec2 from, Vec2 target, float maxPath, Vec2& aimOut) {
  float bestPath = 1e9f;
  bool found = false;
  for (int i = 0; i < nr.n; ++i) {
    const Wl& L = nr.w[i];
    const Vec2 s = L.b - L.a;
    const float ss = dot2(s, s);
    if (ss < 1.0f) continue;
    const Vec2 n0 = perp2(s).norm();
    const float sf = dot2(from - L.a, n0), st = dot2(target - L.a, n0);
    if (sf * st <= 0) continue;  // both on the same side of the wall
    const Vec2 n = n0 * (sf > 0 ? 1.0f : -1.0f);
    const Vec2 face = L.a + n * (L.th * 0.5f);
    const float dt = dot2(target - face, n);
    if (dt < 4.0f) continue;
    const Vec2 img = target - n * (2.0f * dt);
    const Vec2 dir = img - from;
    const float len = dir.len();
    if (len < 1.0f) continue;
    const Vec2 d = dir * (1.0f / len);
    const float den = dot2(d, n);
    if (std::fabs(den) < 1e-4f) continue;
    const float t = dot2(face - from, n) / den;
    if (t <= 0 || t >= len) continue;
    const Vec2 h = from + d * t;
    const float u = dot2(h - face, s) / ss;
    if (u < 0.02f || u > 0.98f) continue;
    const float path = t + (target - h).len();
    if (path > maxPath || path >= bestPath) continue;
    const Vec2 hOff = h + n * 3.0f;
    if (w.lineBlocked(from, hOff) || w.lineBlocked(hOff, target)) continue;
    bestPath = path;
    aimOut = img;
    found = true;
  }
  return found;
}
// A spot near `me` where a wall breaks the line to `threat` (preferring the ring's interior).
inline bool findCover(const World& w, Vec2 me, Vec2 threat, Vec2 inward, Vec2& out) {
  float best = 1e9f;
  bool found = false;
  for (int i = 0; i < 16; ++i) {
    const float a = i * 0.3927f;
    for (float r : {200.0f, 380.0f, 560.0f}) {
      const Vec2 p = me + pDir(a) * r;
      if (!w.inAnyArea(p) || w.inWall(p, 45.0f) || !botSafePoint(w, p, 120.0f) || w.lineBlocked(me, p, 30.0f)) continue;
      if (!w.lineBlocked(threat, p)) continue;
      const float s = r + (p - inward).len() * 0.15f;
      if (s < best) { best = s; out = p; found = true; }
    }
  }
  return found;
}

inline PTrack* findTrack(BotMind& m, uint32_t id) {
  for (PTrack& t : m.trk) if (t.id == id) return &t;
  return nullptr;
}
inline PTrack& ptrack(BotMind& m, uint32_t id) {
  PTrack *free = nullptr, *old = &m.trk[0];
  for (PTrack& t : m.trk) {
    if (t.id == id) return t;
    if (!t.id && !free) free = &t;
    if (t.seenT < old->seenT) old = &t;
  }
  PTrack& r = free ? *free : *old;
  r = PTrack{};
  r.id = id;
  return r;
}
// Learn from one observed position. Velocity comes from the observations only; each brain smooths it its own way.
inline void observe(PTrack& k, Vec2 p, float now, int brain) {
  if (k.n > 0 && now - k.seenT > 0.5f) { k.n = 0; k.vel = {}; k.conf = 0.4f; }   // lost it for a while: start over
  if (k.n > 0 && (p - k.last).len() > 300.0f) { k.n = 0; k.vel = {}; k.conf = 0.3f; }  // teleport / dash: reseed
  k.head = (k.head + 1) & 7;
  k.h[k.head] = p;
  k.ts[k.head] = now;
  if (k.n < 8) k.n++;
  if (k.n == 1) k.firstT = now;
  k.last = p;
  k.seenT = now;
  if (k.n < 2) return;
  const int pi = (k.head + 7) & 7;
  const float dt1 = std::max(1e-3f, now - k.ts[pi]);
  const Vec2 nv = (p - k.h[pi]) * (1.0f / dt1);
  if (brain == 1) {  // 5-observation span: (p_t - p_{t-4}) / (4 dt)
    const int back = std::min(k.n - 1, 4), j = (k.head - back) & 7;
    const float dt = std::max(1e-3f, now - k.ts[j]);
    k.vel = (p - k.h[j]) * (1.0f / dt);
  } else if (brain == 2) {  // EMA 0.65 / 0.35, and less trust after a change of direction
    const bool turned = k.vel.len() > 30.0f && nv.len() > 30.0f && dot2(k.vel.norm(), nv.norm()) < 0.5f;
    k.vel = k.vel * 0.65f + nv * 0.35f;
    k.conf = turned ? 0.5f : std::min(1.0f, k.conf + dt1 * 0.8f);
  } else {  // alpha 0.35
    k.vel += (nv - k.vel) * 0.35f;
  }
}

struct PFoe { const Tank* t; Vec2 p; float d; bool vis; bool forced; float hp; float fvs; };
struct LootPick { bool found = false; Vec2 pos; uint32_t box = 0; float score = 0; };

}  // namespace pre

inline Input presetInput(World& w, Tank& me) {
  using namespace pre;
  using namespace botcfg;
  BotMind& m = me.mind;
  const int B = m.brain;
  const Cfg& C = cfgOf(B);
  Input in;
  const float now = w.time;

  // ---- Weapon, from what a player knows about their own gun ----
  const WeaponParams wp = weaponParams(kBaseWeapons[(size_t)me.weapon]);
  const float myRange = wp.range * statRangeMul(me.stat[StatRange]);
  float bs = wp.bulletSpeed * statBulletSpeedMul(me.stat[StatBulletSpeed]);
  if (me.active == Power::RapidShot) bs *= kRapidShotBulletMul;
  if (B == 3)  // Carrion calibrates the bullet speed from its own shots in flight
    for (const Bullet& b : w.bullets) if (b.owner == me.id && b.kind == BulletKind::Normal) { m.bulSpd = b.vel.len(); break; }
  if (B == 3 && m.bulSpd > 50.0f) bs = m.bulSpd;

  // ---- Timers ----
  m.psTime += kDt; m.shiftT -= kDt; m.decideTimer -= kDt; m.strafeTimer -= kDt; m.react -= kDt; m.noFoeT += kDt;
  m.noBulletT += kDt; m.hurtTimer += kDt; m.dodgeTimer -= kDt; m.slideT -= kDt; m.skipT -= kDt; m.coverT -= kDt; m.roamT -= kDt;
  m.holdT -= kDt; m.lostTimer += kDt; m.loseT += kDt; m.burstN++;
  if (me.health < m.hpPrev - 0.01f) {
    m.hurtTimer = 0;
    m.hitT[m.hitN++ & 3] = now;
  }
  m.hpPrev = me.health;
  const bool fired = me.cooldown > m.prevCd + 0.001f;
  m.prevCd = me.cooldown;
  const float hp = me.health, hpF = me.health / me.maxHealth;

  Near nr;
  gatherWalls(w, me.pos, 1100.0f, nr);

  // ---- The ring (public on the HUD) ----
  const RingState& ring = w.ring;
  const bool ringOn = ring.cur.r < 1e8f;
  const float distC = (me.pos - ring.cur.c).len();
  const float curEdge = ringOn ? ring.cur.r - distC : 1e9f;      // > 0: inside the safe zone
  const float outBy = ringOn ? -curEdge : -1e9f;
  const float outNext = ringOn ? (me.pos - ring.next.c).len() - ring.next.r : -1e9f;
  const Zone Zn = ringOn ? ring.next : Zone{w.middle(), (w.hi.x - w.lo.x) * 0.5f};
  const Vec2 inward = ringOn ? ring.next.c : w.middle();

  // ================= PERCEPTION: only what is on screen =================
  PFoe foes[48];
  int nf = 0;
  for (const Tank& o : w.tanks) {
    if (!o.alive || o.id == me.id || nf >= 48) continue;
    if (w.teamed(me.id, o.id) || !hostile(me, o) || o.ghost) continue;
    const bool forced = w.targets(me.id, o.id);  // a red line from the dev panel: always known
    if (!forced && !canSpot(o)) continue;
    if (o.guardian && o.aggroId != me.id) continue;
    if (o.temple != me.temple) continue;
    const Vec2 p = seenPos(o);
    const float d = (p - me.pos).len();
    if (me.blindT > 0 && d > 220.0f && !forced) continue;
    const bool vis = botSees(me.pos, p) || forced;
    if (!vis) continue;
    foes[nf++] = {&o, p, d, true, forced, o.health / o.maxHealth, 0.0f};
    if (botSees(me.pos, p)) observe(ptrack(m, o.id), p, now, B);
  }
  float turretDps = 0;
  for (const Turret& tu : w.turrets)
    if (tu.owner != me.id && !w.teamed(me.id, tu.owner) && (tu.pos - me.pos).len() < kTurretRange + 80.0f) turretDps += 19.0f / kTurretReload;
  for (int i = 0; i < nf; ++i) if (foes[i].d < 900.0f) { m.noFoeT = 0; break; }

  // ---- Bullets: each one is judged once, after a human-like reaction delay ----
  auto seeShot = [&](uint32_t id, uint32_t owner, Vec2 pos, Vec2 vel, float reachR, float left) {
    if (owner == me.id || !botSees(me.pos, pos, 0.0f)) return;
    const Tank* o = w.find(owner);
    if (o && (!hostile(me, *o) || w.teamed(me.id, owner))) return;
    PBul* s = nullptr;
    for (PBul& b : m.pbul) if (b.id == id) { s = &b; break; }
    if (!s) {
      for (PBul& b : m.pbul) if (!b.id) { s = &b; break; }
      if (!s) return;
      *s = PBul{};
      s->id = id; s->seenT = now; s->react = w.rng.range(C.reactMin, C.reactMax);
      if (PTrack* k = findTrack(m, owner)) { k->firedT = now; k->firedDir = vel.norm(); }
    }
    s->live = true;
    m.noBulletT = std::min(m.noBulletT, 0.0f);
    if (s->judged || now < s->seenT + s->react) return;
    s->judged = true;
    // Project the shot (a few wall bounces for the brains that do) and see how close it comes.
    Vec2 p = pos - me.pos, v = vel;
    float travelled = 0, tBase = 0;
    const float speed = vel.len();
    if (speed < 1.0f) return;
    for (int bounce = 0; bounce <= C.dodgeBounce; ++bounce) {
      const float vv = dot2(v, v);
      float ts = -dot2(p, v) / vv;
      float wallT = 1e9f;
      Vec2 wn;
      if (bounce < C.dodgeBounce) {
        const float h = rayHit(nr, me.pos + p, v.norm(), 400.0f, &wn);
        if (h < 1e8f) wallT = h / speed;
      }
      if (ts > wallT) {  // it hits a wall first: reflect and keep going
        p = p + v * wallT;
        v = v - wn * (2.0f * dot2(v, wn));
        tBase += wallT;
        travelled += wallT * speed;
        continue;
      }
      if (ts < 0.02f || tBase + ts > C.dodgeHorizon || travelled + ts * speed > left) return;
      const Vec2 closest = p + v * ts;
      if (closest.len() >= me.radius + reachR + C.dodgeR) return;
      // A threat: step across its path, to the side with more room.
      const Vec2 n = perp2(v.norm());
      Vec2 dir = dot2(n, closest * -1.0f) >= 0 ? n : n * -1.0f;
      if (closest.len() < 4.0f) dir = n * (m.strafeSign != 0 ? m.strafeSign : 1.0f);
      const float roomA = std::min(rayHit(nr, me.pos, n, 260.0f), 260.0f), roomB = std::min(rayHit(nr, me.pos, n * -1.0f, 260.0f), 260.0f);
      if (std::fabs(roomA - roomB) > 60.0f) dir = roomA > roomB ? n : n * -1.0f;
      if (!botSafePoint(w, me.pos + dir * 200.0f, 60.0f)) dir = dir * -1.0f;
      m.dodgeDir = dir;
      m.dodgeTimer = C.dodgeHold;
      m.threatT2 = m.lastThreatT;
      m.lastThreatT = now;
      return;
    }
  };
  for (const Bullet& b : w.bullets) seeShot(b.id, b.owner, b.pos, b.vel, b.radius, b.rangeLeft);
  for (const Rocket& r : w.rockets) seeShot(r.id, r.owner, r.pos, r.vel, kRocketBlast * 0.7f, r.rangeLeft);
  for (PBul& b : m.pbul) { if (!b.live) b.id = 0; b.live = false; }  // forget shots that are gone

  // ---- Summary of who is around ----
  const PFoe* nearest = nullptr;
  int n750 = 0, n900 = 0;
  float nearDps = turretDps;
  for (int i = 0; i < nf; ++i) {
    const PFoe& f = foes[i];
    if (f.forced && !botSees(me.pos, f.p)) continue;
    if (!nearest || f.d < nearest->d) nearest = &f;
    if (f.d < 750.0f) n750++;
    if (f.d < 900.0f) { n900++; nearDps += botDps(*f.t, false) * kHitRate; }
  }
  auto foeById = [&](uint32_t id) -> PFoe* {
    if (!id) return nullptr;
    for (int i = 0; i < nf; ++i) if (foes[i].t->id == id) return &foes[i];
    return nullptr;
  };
  // Did this tank recently shoot toward `at`? (From the bullets we saw leave it.)
  auto firedAt = [&](const PFoe& f, Vec2 at) {
    const PTrack* k = findTrack(m, f.t->id);
    return k && now - k->firedT < 1.5f && std::fabs(angleDiff(pAng(k->firedDir), pAng(at - f.p))) < 0.3f;
  };
  auto hitsIn = [&](float secs) { int c = 0; for (float t : m.hitT) if (now - t < secs) c++; return c; };
  const float myDps = botDps(me, true) * kHitRate;
  // Loot worth a detour (each brain weighs the stats its own way).
  auto pickLoot = [&](float detour) -> LootPick {
    LootPick best;
    best.score = 0.15f;
    auto consider = [&](Vec2 pos, float val, uint32_t box) {
      const float d = (pos - me.pos).len();
      if (val <= 0 || d > detour || !botSees(me.pos, pos, 220.0f)) return;
      if (m.skipT > 0 && (pos - m.skipPos).len() < 80.0f) return;
      if (ringOn && (pos - ring.cur.c).len() > ring.cur.r - 80.0f) return;  // never out in the storm
      int risk = 0;
      for (int i = 0; i < nf; ++i) if ((foes[i].p - pos).len() < 500.0f) risk++;
      if (box && (risk > 0 || n900 > 0)) return;  // crates only when it is quiet
      float s = val * (1.0f - d / (detour * 1.25f)) - risk * 0.5f + (d < 250.0f ? 0.3f : 0.0f);
      if (w.lineBlocked(me.pos, pos, 20.0f)) s -= 0.3f;
      if (s > best.score) { best.score = s; best.found = true; best.pos = pos; best.box = box; }
    };
    for (const Upgrade& u : w.upgrades) {
      const int gain = (int)u.rarity + 1 - me.stat[u.stat];
      if (gain <= 0) continue;
      float v = C.statW[u.stat] * (0.5f + 0.25f * gain);
      if (B == 2 && u.stat == StatHealth && hp < 70.0f) v *= 1.5f;
      consider(u.pos, v, 0);
    }
    for (const Crate& c : w.crates) {
      if (me.held == Power::None) consider(c.pos, 1.1f, 0);
      else if (rankPower(B, c.type) >= rankPower(B, me.held) + (B == 2 ? 2.0f : 1.0f)) consider(c.pos, 0.9f, 0);
    }
    for (const RelicPickup& r : w.relics) {
      if (hasRelic(me, r.kind)) continue;
      if (me.relic[1] == Relic::None) consider(r.pos, 1.0f, 0);
      else if (rankRelic(B, r.kind) > rankRelic(B, me.relic[0])) consider(r.pos, 0.8f, 0);
    }
    for (const LootBox& b : w.boxes)
      consider(b.pos, b.kind == BoxKind::Legendary ? 3.0f : b.kind == BoxKind::Small ? 0.8f : 1.3f, b.id);
    return best;
  };
  // A spot to be at while nothing else is going on (each brain has its own idea).
  auto roamGoal = [&]() {
    if (m.roamT > 0 && (m.goal - me.pos).len() > 200.0f && botSafePoint(w, m.goal, 100.0f)) return;
    Vec2 g = m.goal;
    if (B == 1) {  // an inward spiral around the next ring's centre, offset so it isn't dead-centre
      const float a = now * 0.07f + (float)me.id;
      g = Zn.c + pDir(a) * (Zn.r * 0.3f);
      m.roamT = 10.0f;
    } else if (B == 2) {  // 65% of the way from the centre to the edge, along the most open direction
      float bestS = -1e9f;
      for (int i = 0; i < 12; ++i) {
        const Vec2 p = Zn.c + pDir(i * 0.5236f + w.rng.range(-0.2f, 0.2f)) * (Zn.r * 0.65f);
        if (!w.inAnyArea(p) || w.inWall(p, 60.0f)) continue;
        int walls = 0;
        for (const Wall& wl : w.walls) if (wl.temple < 0 && (closestOnSeg(wl.a, wl.b, p) - p).len() < 450.0f) walls++;
        const float s = -walls * 30.0f - (p - me.pos).len() * 0.1f;
        if (s > bestS) { bestS = s; g = p; }
      }
      m.roamT = 12.0f;
    } else if (!ringOn || ring.phase < 0 || now < 90.0f) {  // before the first storm: toward the middle, where the loot is
      g = w.middle() + pDir(w.rng.range(0, 6.2831853f)) * w.rng.range(0, 2200.0f);
      m.roamT = 10.0f;
    } else {  // the shoulder: 500-1200 inside the ring edge, next to a wall if possible
      float bestS = -1e9f;
      for (int i = 0; i < 14; ++i) {
        const Vec2 p = Zn.c + pDir(w.rng.range(0, 6.2831853f)) * std::max(0.0f, Zn.r - w.rng.range(500.0f, 1200.0f));
        if (!w.inAnyArea(p) || w.inWall(p, 60.0f)) continue;
        float gap = 1e9f;
        for (const Wall& wl : w.walls) if (wl.temple < 0) gap = std::min(gap, (closestOnSeg(wl.a, wl.b, p) - p).len() - wl.thick * 0.5f);
        const float s = (gap < 150.0f ? 1.0f : 0.0f) - (p - me.pos).len() * 0.0004f + w.rng.range(0, 0.3f);
        if (s > bestS) { bestS = s; g = p; }
      }
      m.roamT = 8.0f;
    }
    m.goal = g;
  };

  // ================= 1) DECIDE =================
  bool thought = false;
  if (m.decideTimer <= 0) {
    thought = true;
    m.decideTimer = C.think * w.rng.range(0.9f, 1.1f);
    // Has some cover (a wall) within 400 units? (Carrion's FVS asks)
    m.coverOk = false;
    for (int i = 0; i < nr.n; ++i) if ((closestOnSeg(nr.w[i].a, nr.w[i].b, me.pos) - me.pos).len() - nr.w[i].th * 0.5f < 400.0f) m.coverOk = true;
    const bool offensive = me.held == Power::Rocket || me.held == Power::Homing || me.held == Power::Lightning || me.held == Power::AirStrike ||
                           me.held == Power::Spread || me.held == Power::RapidShot || me.held == Power::Singularity;
    // Fight value of each tank we can see.
    const bool underFire = m.hurtTimer < 1.5f || now - m.lastThreatT < 1.5f;
    for (int i = 0; i < nf; ++i) {
      PFoe& f = foes[i];
      if (B == 3) {  // Carrion Clock's FVS
        float s = 0;
        if (f.hp < 0.4f) s += 3;
        const PTrack* k = findTrack(m, f.t->id);
        const bool shot = k && now - k->firedT < 2.0f;
        if (shot && !firedAt(f, me.pos)) s += 2;
        if (std::fabs(angleDiff(f.t->aim, pAng(me.pos - f.p))) > 1.0472f) s += 1;
        if (offensive && me.active == Power::None && me.recharge <= 0) s += 1;
        if (hp < 50.0f) s -= 3;
        for (int j = 0; j < nf; ++j) if (j != i && (foes[j].p - f.p).len() < 700.0f) s -= 2;
        if (underFire && hp <= f.t->health) s -= 2;
        if (!m.coverOk) s -= 1;
        f.fvs = s;
      } else {  // Aegis / Ghost: how long each of us needs to win (log of the ratio; > 0 favours us)
        const float their = botDps(*f.t, false) * kHitRate;
        const float onMe = their + std::max(0.0f, nearDps - (f.d < 900.0f ? their : 0.0f)) * 0.5f;
        f.fvs = std::log(std::max(0.05f, (hp / std::max(1.0f, onMe)) / std::max(0.05f, f.t->health / std::max(1.0f, myDps))));
      }
    }
    PFoe* forcedFoe = nullptr;
    for (int i = 0; i < nf; ++i) if (foes[i].forced && (!forcedFoe || foes[i].d < forcedFoe->d)) forcedFoe = &foes[i];
    PState ns = m.ps;
    const int phase = ring.phase;

    if (forcedFoe) {  // a red line (dev panel): go after it, whatever else is going on
      ns = PState::Fight;
      m.targetId = forcedFoe->t->id;
    } else if (B == 1) {
      // ---- Aegis-Vulture: STORM_EVAC > DISENGAGE > VULTURE_EXECUTE > SKIRMISH > HARVEST > SPIRAL_ROAM ----
      const bool storm = ringOn && (outBy > 0 || (ring.shrinking && curEdge < C.ringPush) || (ring.secsLeft < 12.0f && outNext > 0));
      const bool stay = m.ps == PState::Zone && ringOn && !(curEdge > 400.0f && outNext <= 0);
      const PFoe *weak = nullptr, *brawl = nullptr;
      for (int i = 0; i < nf; ++i) {
        const PFoe& f = foes[i];
        if (f.d > 1200.0f) continue;
        if (f.hp <= 0.45f && f.d < 1100.0f && (!weak || f.hp < weak->hp)) weak = &f;
        for (int j = 0; j < nf; ++j)  // two tanks shooting at each other: take the weaker one
          if (j != i && foes[j].d < 1200.0f && firedAt(f, foes[j].p)) {
            const PFoe* lo = f.hp <= foes[j].hp ? &f : &foes[j];
            if (!brawl || lo->hp < brawl->hp) brawl = lo;
          }
      }
      const bool evadeNow = (hpF < 0.42f && n900 > 0) || (n750 >= 2 && hpF < 0.70f) || (hpF < 0.55f && nearest && nearest->d < 950.0f);
      const bool evadeStay = m.ps == PState::Evade && !(hpF >= 0.65f && m.noFoeT > 2.5f) && !(m.noFoeT > 7.0f);
      if (storm || stay) ns = PState::Zone;
      else if (evadeNow || evadeStay) ns = PState::Evade;
      else if (hpF >= 0.45f && (weak || brawl)) { ns = PState::Finish; m.targetId = (weak ? weak : brawl)->t->id; }
      else if (m.ps == PState::Finish && m.loseT < 1.5f && hpF >= 0.42f && !foeById(m.targetId) && m.targetId) ns = PState::Finish;
      else if (nearest && nearest->d < 950.0f && hpF >= 0.55f) { ns = PState::Fight; m.targetId = nearest->t->id; }
      else {
        const LootPick lp = pickLoot(900.0f);
        if (lp.found) { ns = PState::Loot; m.goal = lp.pos; m.crateId = lp.box; }
        else { ns = PState::Roam; m.crateId = 0; roamGoal(); }
      }
    } else if (B == 2) {
      // ---- Ghost Current: STORM ROTATION > DISENGAGE > CURRENT SHIFT > ENGAGE > SCAVENGE > NAVIGATE (+ FINAL HUNT) ----
      const bool finalHunt = w.aliveCount() <= 4;
      const float toNext = std::max(0.0f, outNext + 350.0f) / (kTankSpeed * 0.85f);
      const bool storm = ringOn && (outBy > 0 || (ring.shrinking && curEdge < 350.0f) || (outNext > -350.0f && ring.secsLeft < toNext + 6.0f));
      const bool stay = m.ps == PState::Zone && ringOn && !(curEdge > 350.0f && outNext <= 0);
      // Pick the primary target: weak, busy with someone else, near danger, close, easy.
      PFoe* best = nullptr;
      float bestS = -1e9f;
      for (int i = 0; i < nf; ++i) {
        PFoe& f = foes[i];
        if (f.d > 1100.0f && !(finalHunt && f.d < 1300.0f)) continue;
        float s = (1.0f - f.hp) * 2.0f + (1.0f - f.d / 1100.0f) + std::clamp(f.fvs, -1.0f, 1.0f) * 0.8f;
        const PTrack* k = findTrack(m, f.t->id);
        if (k && now - k->firedT < 2.0f && !firedAt(f, me.pos)) s += 1.0f;                       // busy with someone else
        if (ringOn && (f.p - ring.cur.c).len() > ring.cur.r - 150.0f) s += 1.0f;                // standing in danger
        if (finalHunt) { int others = 0; for (int j = 0; j < nf; ++j) if (j != i && (foes[j].p - f.p).len() < 700.0f) others++; s -= others * 0.7f; }  // isolate one
        if (s > bestS) { bestS = s; best = &f; }
      }
      // Open directions: boxed in by walls?
      int open = 0;
      for (int i = 0; i < 8; ++i) if (rayHit(nr, me.pos, pDir(i * 0.7854f), 260.0f) >= 260.0f) open++;
      const bool trapped = n900 > 0 && open <= 2;
      const bool unfav = best && best->fvs < -0.1f;
      if (m.ps == PState::Fight && unfav) m.engageT += m.decideTimer; else m.engageT = std::max(0.0f, m.engageT - 0.3f);
      const bool disNow = (n900 > 0 && hp < 30.0f) || (hp < 45.0f && n750 >= 2) || (best && best->fvs < -0.9f && hp < 60.0f) || trapped || m.engageT > 4.0f;
      const bool disStay = m.ps == PState::Evade && !(hp > 65.0f) && !(m.noFoeT > 4.0f && m.noBulletT > 1.0f) && !(best && best->fvs > 0.8f && hp > 50.0f);
      const bool shiftNow = n900 > 0 && (hp < 55.0f || n750 >= 2 || now - m.threatT2 < 1.5f ||
                                         (m.ps == PState::Fight && rayHit(nr, me.pos, me.vel.len() > 20.0f ? me.vel.norm() : pDir(me.aim), 130.0f) < 130.0f));
      if (m.ps != PState::Shift && shiftNow && m.ps != PState::Evade) m.shiftT = w.rng.range(0.8f, 1.8f);
      if (storm || stay) ns = PState::Zone;
      else if (disNow || disStay) { ns = PState::Evade; if (nearest) m.targetId = nearest->t->id; if (m.ps != PState::Evade) m.engageT = 0; }
      else if (m.shiftT > 0 && (m.ps == PState::Shift || shiftNow) && best) { ns = PState::Shift; m.targetId = best->t->id; }
      else if (best && (hp >= 45.0f || best->hp < 0.35f) && best->fvs > -0.5f) { ns = PState::Fight; m.targetId = best->t->id; }
      else if (m.ps == PState::Fight && m.loseT < 1.2f && m.targetId && !foeById(m.targetId)) ns = PState::Fight;
      else {
        const float detour = finalHunt ? 300.0f : phase <= 1 ? 1500.0f : phase <= 3 ? 900.0f : 500.0f;
        const LootPick lp = pickLoot(detour);
        if (lp.found && m.ps == PState::Loot && m.psTime > 5.0f) {  // 5 s on the same loot is enough: give it up for a while
          m.skipPos = lp.pos; m.skipT = 6.0f; ns = PState::Roam; m.crateId = 0; m.roamT = 0; roamGoal();
        } else if (lp.found) { ns = PState::Loot; m.goal = lp.pos; m.crateId = lp.box; }
        else { ns = PState::Roam; m.crateId = 0; roamGoal(); }
      }
    } else {
      // ---- Carrion Clock: ZONE > EVADE > FINISH > DUEL > LOOT > STALK > HOLD ----
      const bool storm = ringOn && (outBy > 0 || (!ring.shrinking && ring.secsLeft < 8.0f && curEdge < 600.0f) || outNext > 0);
      const bool stay = m.ps == PState::Zone && ringOn && (curEdge < 500.0f || outNext > 0);
      PFoe* bestF = nullptr;
      for (int i = 0; i < nf; ++i) if (foes[i].d < myRange * 1.2f && (!bestF || foes[i].fvs > bestF->fvs)) bestF = &foes[i];
      const bool evadeNow = hp < 35.0f || (hitsIn(2.0f) >= 3 && bestF && bestF->fvs < 0) || (hitsIn(2.0f) >= 3 && !bestF);
      const bool evadeStay = m.ps == PState::Evade && !(m.noFoeT > 3.0f && m.noBulletT > 0.5f);
      const PFoe* fin = nullptr;
      for (int i = 0; i < nf; ++i)
        if (foes[i].hp < 0.30f && foes[i].d < myRange * 1.3f && (!fin || foes[i].d < fin->d)) fin = &foes[i];
      const bool finStay = m.ps == PState::Finish && m.targetId && m.loseT < 2.0f;
      if (storm || stay) ns = PState::Zone;
      else if (evadeNow || evadeStay) { ns = PState::Evade; if (nearest) m.targetId = nearest->t->id; }
      else if ((fin && hp > 25.0f) || (finStay && hp > 25.0f && (foeById(m.targetId) ? foeById(m.targetId)->hp < 0.45f : true))) {
        ns = PState::Finish;
        if (fin) m.targetId = fin->t->id;
      } else {
        // DUEL: only with FVS >= 2; leave after 15 ticks (0.5 s) of FVS < 0.
        PFoe* cur = foeById(m.targetId);
        if (m.ps == PState::Fight && cur) { if (cur->fvs < 0) m.engageT += m.decideTimer; else m.engageT = 0; }
        const bool duel = bestF && bestF->fvs >= 2.0f;
        const bool duelStay = m.ps == PState::Fight && cur && m.engageT < 0.5f;
        if (duel || duelStay) {
          ns = PState::Fight;
          if (duel && (!cur || bestF->fvs > cur->fvs + 0.5f || m.ps != PState::Fight)) { m.targetId = bestF->t->id; m.engageT = 0; }
        } else {
          const float detour = phase <= 1 ? 1500.0f : phase <= 3 ? 700.0f : 300.0f;
          bool guarded = false;  // an enemy we would happily fight is near: no looting
          for (int i = 0; i < nf; ++i) if (foes[i].d < 900.0f && foes[i].fvs >= 0) guarded = true;
          const LootPick lp = guarded ? LootPick{} : pickLoot(detour);
          // STALK: a tank or a gunfight at a distance: close in to weapon range and wait for a window.
          const PFoe* stalkF = nullptr;
          for (int i = 0; i < nf; ++i) {
            const PTrack* k = findTrack(m, foes[i].t->id);
            if ((foes[i].d >= 700.0f || (k && now - k->firedT < 2.0f && !firedAt(foes[i], me.pos))) && (!stalkF || foes[i].d < stalkF->d)) stalkF = &foes[i];
          }
          if (lp.found) { ns = PState::Loot; m.goal = lp.pos; m.crateId = lp.box; }
          else if (stalkF && hp >= 50.0f) { ns = PState::Stalk; m.targetId = stalkF->t->id; }
          else { ns = PState::Hold; m.crateId = 0; roamGoal(); }
        }
      }
    }
    if (ns != m.ps) {
      if (m.ps == PState::Evade || ns == PState::Evade) m.coverT = 0;
      m.ps = ns;
      m.psTime = 0;
      m.loseT = 0;
    }
    if (m.ps == PState::Roam || m.ps == PState::Hold || m.ps == PState::Zone) m.crateId = 0;
    if (m.ps != PState::Fight && m.ps != PState::Shift && m.ps != PState::Finish && m.ps != PState::Stalk && m.ps != PState::Evade) m.targetId = 0;
  }

  const bool fightLike = m.ps == PState::Fight || m.ps == PState::Shift || m.ps == PState::Finish;
  PFoe* tgt = foeById(m.targetId);
  if (tgt) { m.loseT = 0; m.lastSeen = tgt->p; m.lostTimer = 0; }
  if (m.targetId && !tgt && m.loseT > 2.5f && (fightLike || m.ps == PState::Stalk)) {  // lost it: back to normal business
    m.targetId = 0;
    m.decideTimer = 0;
  }
  if (!m.targetId && (m.ps == PState::Fight || m.ps == PState::Shift || m.ps == PState::Finish || m.ps == PState::Stalk)) m.decideTimer = 0;

  // ================= 2) STEER =================
  if (m.strafeTimer <= 0) {
    m.strafeSign = w.rng.f01() < 0.5f ? 1.0f : -1.0f;
    m.strafeTimer = w.rng.range(C.flipMin, C.flipMax);
  }
  if (B == 1 && fired && (fightLike)) m.strafeSign = -m.strafeSign;  // Aegis: flip as it fires, to break the enemy's lead
  auto route = [&](Vec2 goal) -> Vec2 {  // a wall in the way: head for a clear spot round it first
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
        const Vec2 p = me.pos + pDir(a) * r;
        if (!w.inAnyArea(p) || w.inWall(p, me.radius + 20.0f) || w.lineBlocked(me.pos, p, me.radius * 0.8f)) continue;
        const float sc = (goal - p).len() + (w.lineBlocked(p, goal, me.radius * 0.6f) ? 350.0f : 0.0f) + ((p - m.detour).len() < 50.0f ? 200.0f : 0.0f);
        if (sc < bestS) { bestS = sc; bestP = p; }
      }
    }
    if (bestS < 1e8f) { m.detour = bestP; m.detourT = 1.5f; return bestP; }
    return goal;
  };

  Vec2 want;
  const float pref = myRange * C.pref;
  switch (m.ps) {
    case PState::Zone: {
      Vec2 g = Zn.c;
      if (Zn.r > 1.0f) g = Zn.c + (me.pos - Zn.c).norm() * std::min((me.pos - Zn.c).len(), Zn.r * 0.55f);
      want = (route(g) - me.pos).norm();
      break;
    }
    case PState::Roam: case PState::Hold: case PState::Loot: {
      bool clearShot = false;
      if (m.ps == PState::Loot && m.crateId) clearShot = !w.lineBlocked(me.pos, m.goal);
      const float stop = clearShot ? 260.0f : m.ps == PState::Hold ? 60.0f : 40.0f;
      const Vec2 d = m.goal - me.pos;
      want = d.len() > stop ? ((clearShot ? m.goal : route(m.goal)) - me.pos).norm() : Vec2{};
      if (want.len() < 0.01f && nearest && !clearShot) want = perp2((nearest->p - me.pos).norm()) * (0.8f * m.strafeSign);  // never stand still in sight
      break;
    }
    case PState::Stalk: {
      if (!tgt) { want = (route(m.lastSeen) - me.pos).norm(); break; }
      const Vec2 d = tgt->p - me.pos;
      want = d.len() > myRange * 1.0f ? (route(tgt->p) - me.pos).norm() : perp2(d.norm()) * (0.7f * m.strafeSign);
      break;
    }
    case PState::Fight: case PState::Finish: case PState::Shift: {
      if (!tgt) { want = (route(m.lastSeen) - me.pos).norm(); break; }
      const Vec2 d = tgt->p - me.pos;
      const float len = d.len();
      const float r = m.ps == PState::Finish ? pref * 0.75f : pref;
      if (m.ps == PState::Shift) want = perp2(d.norm()) * (1.0f * m.strafeSign) + d.norm() * std::clamp((len - r) / 250.0f, -0.6f, 0.6f);  // around, not away
      else if (B == 2) want = d.norm() * std::clamp((len - r) / 150.0f, -0.7f, 1.0f) + perp2(d.norm()) * (0.75f * m.strafeSign);  // diagonal, never straight in
      else want = d.norm() * std::clamp((len - r) / 150.0f, -1.0f, 1.0f) + perp2(d.norm()) * (0.85f * m.strafeSign);
      // a side blocked within 200 units: flip now (Carrion and Ghost)
      if (B != 1 && rayHit(nr, me.pos, perp2(d.norm()) * m.strafeSign, 200.0f) < 200.0f) { m.strafeSign = -m.strafeSign; m.strafeTimer = w.rng.range(C.flipMin, C.flipMax); }
      break;
    }
    case PState::Evade: {
      Vec2 away;
      for (int i = 0; i < nf; ++i) away += (me.pos - foes[i].p).norm() * (1.0f + (900.0f - std::min(foes[i].d, 900.0f)) / 900.0f);
      for (const Turret& tu : w.turrets)
        if (tu.owner != me.id && (tu.pos - me.pos).len() < kTurretRange + 80.0f) away += (me.pos - tu.pos).norm() * 1.5f;
      away = away.len() > 0.01f ? away.norm() : (inward - me.pos).norm();
      // Cover that breaks the line to the nearest enemy, or at least a way that isn't a dead end.
      if (nearest && m.coverT <= 0) {
        m.coverT = 1.0f;
        if (!findCover(w, me.pos, nearest->p, inward, m.coverPos)) m.coverPos = {};
      }
      want = away + perp2(away) * (0.35f * m.strafeSign) + (inward - me.pos).norm() * 0.3f;
      if ((m.coverPos.x != 0 || m.coverPos.y != 0) && (m.coverPos - me.pos).len() < 700.0f && (m.coverPos - me.pos).len() > 40.0f)
        want = want * 0.5f + (m.coverPos - me.pos).norm() * 1.2f;
      break;
    }
  }

  // Environment: map edge, the storm edge, calm guardians, walls, crates, turrets, singularities, bombs.
  Vec2 env;
  const Vec2 lo = w.lo + Vec2{kWallMargin, kWallMargin}, hi = w.hi - Vec2{kWallMargin, kWallMargin};
  if (me.pos.x < lo.x) env.x += (lo.x - me.pos.x) / kWallMargin;
  if (me.pos.x > hi.x) env.x -= (me.pos.x - hi.x) / kWallMargin;
  if (me.pos.y < lo.y) env.y += (lo.y - me.pos.y) / kWallMargin;
  if (me.pos.y > hi.y) env.y -= (me.pos.y - hi.y) / kWallMargin;
  if (ringOn && m.ps != PState::Zone && curEdge < C.ringPush)
    env += (ring.cur.c - me.pos).norm() * (std::clamp((C.ringPush - curEdge) / C.ringPush, 0.0f, 1.0f) * C.ringW);
  for (const Tank& g : w.tanks) {
    if (!g.alive || !g.guardian || g.aggroId == me.id) continue;
    const float d = (g.pos - me.pos).len();
    if (d < 700.0f) env += (me.pos - g.pos).norm() * ((700.0f - d) / 700.0f * 2.0f);
  }
  Vec2 wallN;
  float wallGap = 1e9f;
  for (int i = 0; i < nr.n; ++i) {  // a gentle push away from every wall surface, and the closest one to slide on
    const Wl& L = nr.w[i];
    const Vec2 cp = closestOnSeg(L.a, L.b, me.pos);
    const float d = (me.pos - cp).len(), gap = d - L.th * 0.5f - me.radius;
    if (d < 0.01f) continue;
    if (gap < 110.0f) env += (me.pos - cp) * (1.0f / d) * (std::max(0.0f, 110.0f - gap) / 110.0f * 1.2f);
    if (gap < wallGap) { wallGap = gap; wallN = (me.pos - cp) * (1.0f / d); }
  }
  {  // feelers fanned out around the way it wants to go
    const Vec2 head = want.len() > 0.1f ? want.norm() : me.vel.len() > 20.0f ? me.vel.norm() : pDir(me.aim);
    const float base = pAng(head);
    for (int k = 0; k < C.rays; ++k) {
      const float a = base + (C.rays > 1 ? (k / (float)(C.rays - 1) - 0.5f) * 2.0944f : 0.0f);
      const Vec2 d = pDir(a);
      const float h = rayHit(nr, me.pos, d, C.rayLen);
      if (h < C.rayLen) { const float f = 1.0f - h / C.rayLen; env += d * (-f * f * C.wallW * 0.6f); }
    }
  }
  for (const LootBox& b : w.boxes) {
    const float d = (b.pos - me.pos).len(), R = boxRadius(b.kind) + me.radius + 60.0f;
    if (d < R && d > 1.0f) env += (me.pos - b.pos).norm() * ((R - d) / R * 1.2f);
  }
  for (const Turret& tu : w.turrets) {
    if (tu.owner == me.id || w.teamed(me.id, tu.owner)) continue;
    const float d = (tu.pos - me.pos).len(), R = kTurretRange + 60.0f;
    if (d < R) env += (me.pos - tu.pos).norm() * ((R - d) / R * 1.8f);
  }
  for (const Singularity& s : w.sings) {
    if (s.owner == me.id) continue;
    const float d = (s.pos - me.pos).len(), R = kSingRadius + 150.0f;
    if (d < R) env += (me.pos - s.pos).norm() * ((R - d) / R * 2.5f);
  }
  for (const Bomb& b : w.bombs) {
    if (b.owner == me.id) continue;
    const float d = (b.pos - me.pos).len(), R = kBombBlast + me.radius + 40.0f;
    if (d < R) env += (me.pos - b.pos).norm() * 3.0f;
  }
  want += env * 1.5f;
  if (m.dodgeTimer > 0) want = m.dodgeDir * 2.0f + want * 0.25f;  // the dodge (it started after its reaction delay)

  // Up against a wall and pushing into it: slide along it, keeping the same side for a moment.
  if (wallGap < 30.0f && want.len() > 0.1f && dot2(want, wallN) < -0.05f) {
    const Vec2 tg = perp2(wallN);
    if (m.slideT <= 0) m.slideSign = dot2(want, tg) >= 0 ? 1.0f : -1.0f;
    m.slideT = 0.8f;
    want = tg * (m.slideSign * want.len()) + wallN * 0.15f;
  }
  // Anti-stuck: it is trying to move but is not getting anywhere.
  if (m.unstickTimer > 0) {
    m.unstickTimer -= kDt;
    want = m.unstickDir;
  } else if ((me.pos - m.stuckPos).len() > C.stuckDisp || fightLike || m.ps == PState::Hold || (m.goal - me.pos).len() < 90.0f ||
             (m.crateId && !w.lineBlocked(me.pos, m.goal))) {
    m.stuckPos = me.pos;
    m.stuckTimer = 0;
  } else {
    m.stuckTimer += kDt;
    if (m.stuckTimer > C.stuckWin) {
      const Vec2 head = want.len() > 0.1f ? want.norm() : pDir(me.aim);
      const Vec2 side = wallGap < 90.0f ? perp2(wallN) : perp2(head);
      const float sgn = w.rng.f01() < 0.5f ? 1.0f : -1.0f;
      m.unstickDir = (B == 1 ? side * (sgn * 0.8f) - head * 0.6f : side * sgn + wallN * 0.3f + Vec2{w.rng.range(-0.3f, 0.3f), w.rng.range(-0.3f, 0.3f)}).norm();
      m.unstickTimer = C.stuckDur;
      m.stuckTimer = 0;
      m.slideSign = -m.slideSign;
      m.detourT = 0;
      m.skipPos = m.goal; m.skipT = 5.0f; m.crateId = 0;  // forget the loot it could not reach
      if (++m.unstuckN % 2 == 0 && ringOn) {              // still stuck: a different goal on the other side
        const Vec2 away = (me.pos - Zn.c).norm();
        m.goal = Zn.c + away * -std::min(Zn.r * 0.5f, 900.0f);
        m.roamT = 5.0f;
      } else m.roamT = 0;
      m.decideTimer = 0;
    }
  }

  const bool hurry = m.ps == PState::Zone || m.ps == PState::Evade || m.dodgeTimer > 0;
  const float goalD = (m.goal - me.pos).len();
  in.speed = hurry || fightLike ? 1.0f : std::clamp(goalD / 220.0f, 0.55f, 1.0f);
  in.turn = C.turn;
  m.steer += (want - m.steer) * std::min(1.0f, kDt * (m.dodgeTimer > 0 ? 16.0f : 7.0f));
  const Vec2 nrm = m.steer.len() > 0.12f ? m.steer.norm() : Vec2{};
  auto key = [](float v, int8_t prev) -> int8_t {  // a key goes down past 0.42 and only comes up below 0.2
    if (prev > 0 && v > 0.2f) return 1;
    if (prev < 0 && v < -0.2f) return -1;
    return v > 0.42f ? 1 : (v < -0.42f ? -1 : 0);
  };
  m.keyX = key(nrm.x, m.keyX);
  m.keyY = key(nrm.y, m.keyY);
  in.moveX = m.keyX;
  in.moveY = m.keyY;

  // ================= 3) AIM & FIRE =================
  const PFoe* ft = nullptr;
  if (tgt && (fightLike || m.ps == PState::Stalk || m.ps == PState::Evade)) ft = tgt;
  if (!ft)
    for (int i = 0; i < nf; ++i)
      if (foes[i].d < myRange * C.fireRange * 0.9f && (!ft || foes[i].d < ft->d)) ft = &foes[i];
  Vec2 aimPoint;
  bool haveAim = false;
  if (ft) {
    const PTrack* k = findTrack(m, ft->t->id);
    const Vec2 v = (k && !isHidden(*ft->t)) ? k->vel : Vec2{};  // what it has *seen* the target do
    const float conf = (B == 2 && k) ? k->conf : 1.0f;
    float tt = ft->d / bs;
    for (int i = 0; i < C.iters; ++i) tt = std::min(((ft->p + v * (tt * C.leadMul * conf)) - me.pos).len() / bs, C.horizon);
    aimPoint = ft->p + v * (tt * C.leadMul * conf);
    haveAim = true;
    if (ft->t->id != m.lastFireId) { m.lastFireId = ft->t->id; m.react = C.acquire + w.rng.range(0.0f, 0.04f); }
    in.tx = aimPoint.x; in.ty = aimPoint.y;  // rockets, strikes and the singularity go here
  } else {
    m.lastFireId = 0;
  }
  if (thought) m.aimErrTarget = pGauss(w.rng) * C.aimSigma * (me.vel.len() > 250.0f ? 1.3f : 1.0f);
  m.aimErr += (m.aimErrTarget * (m.dodgeTimer > 0 ? 1.8f : 1.0f) - m.aimErr) * std::min(1.0f, 6.0f * kDt);

  bool bouncing = false;
  if (haveAim) {
    Vec2 ap = aimPoint;
    // The line must be clear of walls, crates and other tanks.
    bool los = !w.lineBlocked(me.pos, ap);
    if (los)
      for (const LootBox& b : w.boxes) if ((closestOnSeg(me.pos, ap, b.pos) - b.pos).len() < boxRadius(b.kind) + 6.0f) { los = false; break; }
    if (los)
      for (const Tank& o : w.tanks)
        if (o.alive && o.id != me.id && o.id != ft->t->id && (closestOnSeg(me.pos, ap, o.pos) - o.pos).len() < o.radius + 4.0f) { los = false; break; }
    if (!los) {  // one bounce off a wall, if it is a good shot (never more than one)
      Vec2 ba;
      const float maxPath = (B == 3 ? 0.9f : B == 2 ? 0.8f : 1.0f) * myRange;
      const bool allow = B == 1 ? ft->d < 1100.0f : B == 2 ? (ft->d < 900.0f && (!findTrack(m, ft->t->id) || findTrack(m, ft->t->id)->vel.len() < 250.0f)) : true;
      if (allow && bounceAim(w, nr, me.pos, ap, maxPath, ba)) { ap = ba; los = true; bouncing = true; }
    }
    in.aim = pAng(ap - me.pos) + m.aimErr;
    bool ok = los && m.react <= 0.0f && ft->d <= myRange * C.fireRange && std::fabs(angleDiff(me.aim, in.aim)) < C.aimTol;
    if (bouncing) {  // Aegis fires a 2-round burst, then holds; the others are just as sparing
      if (now - m.bounceT > 1.5f) m.bounceShots = 0;
      if (m.bounceShots >= 2) ok = false;
      else if (ok && me.cooldown <= 0.0f) { m.bounceShots++; m.bounceT = now; }
    }
    if (B == 3 && ft->d > myRange * 0.8f && (m.burstN % 10) >= 6) ok = false;  // far away: 6 ticks on, 4 off
    if (isHidden(me) && m.ps == PState::Evade) ok = false;                      // sneaking away
    in.shoot = ok;
  } else {
    in.aim = nrm.len() > 0.1f ? pAng(nrm) : me.aim;
    if (m.ps == PState::Loot && m.crateId && n900 == 0)  // nobody around: shoot the crate open
      for (const LootBox& b : w.boxes)
        if (b.id == m.crateId && (b.pos - me.pos).len() < std::min(myRange * 0.9f, B == 1 ? 650.0f : 1e9f) && !w.lineBlocked(me.pos, b.pos)) {
          in.aim = pAng(b.pos - me.pos) + m.aimErr * 0.5f;
          in.shoot = std::fabs(angleDiff(me.aim, in.aim)) < 0.15f;
        }
  }

  // ================= 4) SUPERPOWER, RELICS, SWAPS =================
  const PFoe* pt = tgt ? tgt : nearest;
  const float bd = pt ? pt->d : 1e9f;
  const bool clear = pt && !w.lineBlocked(me.pos, pt->p);
  const bool incoming = now - m.lastThreatT < 0.8f || m.hurtTimer < 0.6f;
  const bool inFight = fightLike || m.ps == PState::Stalk;
  // Instinct: after being hit, press dodge in time.
  if (me.active == Power::Instinct && me.pendingT > 0) {
    if (m.instinctWait < 0) m.instinctWait = w.rng.range(0.2f, 0.45f);
    m.instinctWait -= kDt;
    if (m.instinctWait <= 0) { in.usePowerUp = true; m.instinctWait = -1; }
  } else {
    m.instinctWait = -1;
  }
  if (me.held != Power::None && me.active == Power::None && me.recharge <= 0 && me.stunT <= 0 && me.silenceT <= 0 && (++m.powerTick % 3) == 0) {
    bool use = false;
    Vec2 esc = me.pos + (inward - me.pos).norm() * 600.0f;  // where a Teleport / Dash escape heads: toward the safe zone
    if (nearest) esc = me.pos + (me.pos - nearest->p).norm() * 450.0f + (inward - me.pos).norm() * 250.0f;
    const bool wounded = pt && pt->hp < 0.5f;
    const bool still = pt && (findTrack(m, pt->t->id) ? findTrack(m, pt->t->id)->vel.len() < 120.0f : true);
    switch (me.held) {
      case Power::Shield:
        use = (incoming && hpF < 0.7f) || (m.ps == PState::Evade && incoming);
        break;
      case Power::Speed:
        use = m.ps == PState::Evade || (m.ps == PState::Zone && outBy > -60.0f) || (m.ps == PState::Finish && bd > myRange * 0.8f);
        break;
      case Power::Dash:
        use = (m.ps == PState::Evade && n900 > 0) || (B != 1 && m.ps == PState::Shift && incoming) || (m.ps == PState::Zone && outBy > 0) ||
              (m.ps == PState::Finish && pt && bd > 300.0f && pt->hp < 0.35f);
        break;
      case Power::Teleport:
        use = (m.ps == PState::Evade && hpF < 0.35f) || (B == 2 && n750 >= 3) || (m.ps == PState::Zone && outBy > 0);
        if (use && m.ps == PState::Zone) esc = me.pos + (Zn.c - me.pos).norm() * std::min(700.0f, (Zn.c - me.pos).len());
        break;
      case Power::Invisible:
        use = (m.ps == PState::Evade && n900 > 0) || (B == 3 && m.ps == PState::Stalk && m.psTime < 0.6f);
        break;
      case Power::Rocket:
        use = pt && ((clear && bd < (B == 1 ? 900.0f : 1300.0f) && (B == 1 || wounded || pt->fvs >= 2.0f)) || (!clear && bd < 650.0f && B != 3));
        break;
      case Power::Homing:
        use = pt && bd < 800.0f && (B == 1 || wounded || pt->fvs >= 2.0f || (B == 2 && bd > pref * 1.2f));
        break;
      case Power::RapidShot: case Power::Spread:
        use = inFight && pt && clear && bd < myRange * 0.8f && (B != 3 || m.psTime < 3.0f || m.ps == PState::Finish);
        break;
      case Power::AirStrike:
        use = pt && bd > 250.0f && bd < 850.0f && (B == 1 || still || n750 >= 2);
        break;
      case Power::Lightning:
        use = pt && bd < 380.0f && clear;
        break;
      case Power::Turret:
        use = (inFight && pt && bd < kTurretRange) || (B == 3 && m.ps == PState::Hold && nearest);
        break;
      case Power::Singularity:
        use = inFight && pt && bd < 650.0f && (B == 1 || n750 >= 2 || still);
        break;
      case Power::Instinct:
        use = inFight && pt && bd < 900.0f && (B != 3 || m.ps == PState::Stalk || ring.shrinking);
        break;
      case Power::SentryBot:
        use = (fightLike && pt && bd < 900.0f && hpF > 0.5f) || (B == 3 && m.ps == PState::Hold && ring.phase >= 3);
        break;
      default: break;
    }
    if (use) {
      in.usePowerUp = true;
      if (me.held == Power::Teleport || me.held == Power::Dash) { in.tx = esc.x; in.ty = esc.y; }
    }
  }
  // Relics: Repair only when nobody is around (it makes you slow and fragile); a Sentry Drone is overloaded in a fight.
  for (int s = 0; s < 2; ++s) {
    if (me.relic[s] == Relic::Repair) {
      const bool safe = nf == 0 && turretDps == 0 && m.noBulletT > 1.0f && m.hurtTimer > 1.5f && m.ps != PState::Zone;
      in.relicDown[s] = safe && (B == 2 ? me.maxHealth - hp >= 25.0f : hpF < (B == 1 ? 0.72f : 0.70f));
    }
    if (me.relic[s] == Relic::Sentry && B != 3) in.relicDown[s] = fightLike && tgt && hpF > 0.45f && (B == 1 || m.psTime > 2.0f);
  }
  // F: swap for a clearly better ground pickup (a superpower for the superpower, a relic for the oldest relic).
  if (me.swapCd <= 0 && !incoming) {
    const float reach = me.radius + kCrateRadius + 40.0f;
    float cd = reach, rd = reach;
    const Crate* bc = nullptr;
    const RelicPickup* br = nullptr;
    for (const Crate& c : w.crates) { const float d = (c.pos - me.pos).len(); if (me.held != Power::None && c.type != me.held && d < cd) { cd = d; bc = &c; } }
    for (const RelicPickup& r : w.relics) { const float d = (r.pos - me.pos).len(); if (me.relic[1] != Relic::None && !hasRelic(me, r.kind) && d < rd) { rd = d; br = &r; } }
    if (bc && (!br || cd <= rd) && rankPower(B, bc->type) >= rankPower(B, me.held) + (B == 2 ? 2.0f : 1.0f)) in.swap = true;
    else if (br && !(bc && cd <= rd) && rankRelic(B, br->kind) > rankRelic(B, me.relic[0])) in.swap = true;
  }

  // What it is doing, for the dev panel's Bot Info Overlay.
  BotGoal g = BotGoal::Roam;
  switch (m.ps) {
    case PState::Zone: g = BotGoal::Zone; m.state = BotState::Zone; break;
    case PState::Evade: g = BotGoal::Retreat; m.state = BotState::Retreat; break;
    case PState::Fight: case PState::Shift: case PState::Finish: g = tgt ? BotGoal::Attack : BotGoal::Hunt; m.state = BotState::Engage; break;
    case PState::Stalk: g = BotGoal::Hunt; m.state = BotState::Engage; break;
    case PState::Loot: g = m.crateId ? BotGoal::Crate : BotGoal::Loot; m.state = BotState::Loot; break;
    default: m.state = BotState::Roam; break;
  }
  m.objective = g;
  return in;
}

}  // namespace tonk
