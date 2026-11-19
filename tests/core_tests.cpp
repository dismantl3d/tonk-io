// Tiny dependency-free tests for the shared core.
#include <cstdio>

#include "core/ring.h"
#include "core/sim.h"
#include "core/world.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL: %s (line %d)\n", #c, __LINE__); ++failures; } } while (0)

using namespace tonk;

static Input mv(int x, int y) { Input i; i.moveX = (int8_t)x; i.moveY = (int8_t)y; return i; }
static Input fireAt(float aim) { Input i; i.aim = aim; i.shoot = true; return i; }
static Input useAt(float tx, float ty) { Input i; i.usePowerUp = true; i.tx = tx; i.ty = ty; return i; }
static void run(World& w, int ticks, std::vector<Input> in = {}) { for (int i = 0; i < ticks; ++i) stepWorld(w, in); }

int main() {
  // ---------------- Movement ----------------
  {
    // Diagonal movement is not faster than straight movement.
    Tank a, b;
    a.pos = b.pos = {5000, 5000};
    for (int i = 0; i < 60; ++i) { stepTank(a, mv(1, 0)); stepTank(b, mv(1, 1)); }
    CHECK(std::fabs((a.pos - Vec2{5000, 5000}).len() - (b.pos - Vec2{5000, 5000}).len()) < 0.5f);

    // Momentum: speed builds up (not instant), and the tank slides a little after the keys are released.
    Tank c;
    c.pos = {5000, 5000};
    stepTank(c, mv(1, 0));
    CHECK(c.vel.x > 0 && c.vel.x < kTankSpeed * 0.5f);
    for (int i = 0; i < 30; ++i) stepTank(c, mv(1, 0));
    CHECK(std::fabs(c.vel.x - kTankSpeed) < 5.0f);
    const float x0 = c.pos.x;
    for (int i = 0; i < 30; ++i) stepTank(c, Input{});
    CHECK(c.pos.x - x0 > 20.0f && c.pos.x - x0 < 90.0f);  // a short skid
    CHECK(c.vel.len() == 0);                              // ...that comes to a full stop

    // Tanks stay inside the map.
    Tank d;
    d.pos = {1, 1};
    for (int i = 0; i < 10; ++i) stepTank(d, mv(-1, -1));
    CHECK(d.pos.x >= kTankRadius && d.pos.y >= kTankRadius);

    for (size_t i = 1; i < kRingPhases.size(); ++i) {
      CHECK(kRingPhases[i].startSec > kRingPhases[i - 1].startSec);
      CHECK(kRingPhases[i].endRadiusPct < kRingPhases[i - 1].endRadiusPct);
    }
  }

  // ---------------- Bullets, kills, drops ----------------
  {
    World w2;
    w2.addTank(1, {5000, 5000}, false);
    stepWorld(w2, {fireAt(0)});
    CHECK(w2.bullets.size() == 1);
    run(w2, 200, {Input{}});
    CHECK(w2.bullets.empty() && w2.tanks[0].health == kMaxHealth);

    // A kill gives credit, and the loser drops every upgrade tier, its superpower and all but one relic.
    World w3;
    w3.addTank(1, {5000, 5000}, false);
    Tank& b = w3.addTank(2, {5300, 5000}, false);
    b.health = 10;
    b.stat[StatSpeed] = 3; b.stat[StatFireRate] = 5;
    b.held = Power::Rocket;
    b.relic[0] = Relic::Radar; b.relic[1] = Relic::Repair;
    for (int i = 0; i < 60 && w3.tanks[1].alive; ++i) stepWorld(w3, {fireAt(0), Input{}});
    CHECK(!w3.tanks[1].alive && w3.tanks[0].kills == 1);
    int ups = 0;
    for (const Upgrade& u : w3.upgrades)
      if ((u.stat == StatSpeed && u.rarity == Rarity::Rare) || (u.stat == StatFireRate && u.rarity == Rarity::Legendary)) ups++;
    CHECK(ups == 2);
    CHECK(w3.crates.size() == 1 && w3.crates[0].type == Power::Rocket);
    CHECK(w3.relics.size() == 1);  // one of the two relics was destroyed

    World d1, d2;
    d1.init(7); d2.init(7);
    CHECK(d1.upgrades[10].pos.x == d2.upgrades[10].pos.x && d1.walls.size() == d2.walls.size() && d1.relics.size() == d2.relics.size());
  }

  // ---------------- Upgrade tiers ----------------
  {
    World w;
    w.addTank(1, {5000, 5000}, false);
    Upgrade u; u.id = w.newId(); u.pos = {5000, 5030}; u.stat = StatSpeed; u.rarity = Rarity::Rare;  // tier 3
    w.upgrades.push_back(u);
    stepWorld(w, {Input{}});
    CHECK(w.tanks[0].stat[StatSpeed] == 3 && w.upgrades.empty());
    Upgrade lower = u; lower.id = w.newId(); lower.rarity = Rarity::Uncommon;  // tier 2 < 3: left on the ground
    w.upgrades.push_back(lower);
    stepWorld(w, {Input{}});
    CHECK(w.tanks[0].stat[StatSpeed] == 3 && w.upgrades.size() == 1);
    w.upgrades.clear();
    Upgrade h = u; h.id = w.newId(); h.stat = StatHealth; h.rarity = Rarity::Epic;  // tier 4
    w.upgrades.push_back(h);
    stepWorld(w, {Input{}});
    CHECK(w.tanks[0].stat[StatHealth] == 4 && w.tanks[0].maxHealth == kMaxHealth + 48.0f && w.tanks[0].health == w.tanks[0].maxHealth);

    // Fire Rate: higher tiers shoot faster.
    World f;
    f.addTank(1, {5000, 5000}, false).stat[StatFireRate] = 5;
    stepWorld(f, {fireAt(0)});
    CHECK(std::fabs(f.tanks[0].cooldown - 0.65f / 1.4f) < 0.02f);

    // Bullet Damage: +7% per tier.
    World dm;
    dm.addTank(1, {5000, 5000}, false).stat[StatDamage] = 5;
    stepWorld(dm, {fireAt(0)});
    CHECK(dm.bullets.size() == 1 && std::fabs(dm.bullets[0].damage - 19.0f * 1.35f) < 0.05f && dm.bullets[0].range0 > 0);

    // Loot is better toward the middle.
    World r;
    r.rng.s = 3;
    int edge = 0, mid = 0;
    for (int i = 0; i < 4000; ++i) {
      edge += (int)r.rollUpgradeRarity({300, 300}) >= 3;
      mid += (int)r.rollUpgradeRarity({5000, 5000}) >= 3;
    }
    CHECK(mid > edge * 2);
  }

  // ---------------- Superpowers ----------------
  {
    for (int k = 1; k <= kPowerKinds; ++k) CHECK(powerInfo((Power)k).cooldown > 0);
    CHECK(isMythic(Power::Singularity) && isMythic(Power::Instinct) && !isMythic(Power::Rocket));

    // Pickup with an empty slot; F swaps, leaving the old one behind.
    World w;
    w.addTank(1, {5000, 5000}, false);
    Crate c; c.id = w.newId(); c.pos = {5030, 5000}; c.type = Power::Shield;
    w.crates.push_back(c);
    stepWorld(w, {Input{}});
    CHECK(w.tanks[0].held == Power::Shield);
    Crate c2 = c; c2.id = w.newId(); c2.type = Power::Dash; c2.pos = {5000, 5030};
    w.crates.push_back(c2);
    stepWorld(w, {Input{}});
    CHECK(w.tanks[0].held == Power::Shield && w.crates.size() == 1);
    Input swap; swap.swap = true;
    stepWorld(w, {swap});
    CHECK(w.tanks[0].held == Power::Dash && w.crates[0].type == Power::Shield);

    // Speed Boost doubles speed for 8 s, then a 6.5 s cooldown.
    World ws;
    ws.addTank(1, {2000, 5000}, false).held = Power::Speed;
    Input go = mv(1, 0); go.usePowerUp = true;
    stepWorld(ws, {go});
    CHECK(ws.tanks[0].active == Power::Speed);
    run(ws, 60, {mv(1, 0)});
    CHECK(std::fabs(ws.tanks[0].vel.x - 2 * kTankSpeed) < 5.0f);
    run(ws, 8 * 30, {Input{}});
    CHECK(ws.tanks[0].active == Power::None && ws.tanks[0].recharge > 4.0f);

    // Shield blocks everything for 1.8 s.
    World wsh;
    wsh.addTank(1, {5000, 5000}, false).held = Power::Shield;
    stepWorld(wsh, {useAt(0, 0)});
    damageTank(wsh, wsh.tanks[0], 80, 9);
    CHECK(wsh.tanks[0].health == kMaxHealth);
    run(wsh, 60, {Input{}});
    damageTank(wsh, wsh.tanks[0], 30, 9);
    CHECK(wsh.tanks[0].health == kMaxHealth - 30);

    // Rocket: a direct hit takes 60% of the victim's max health; the owner is safe from the blast. The cooldown only
    // starts once the rocket has exploded.
    World wr;
    wr.addTank(1, {5000, 5000}, false).held = Power::Rocket;
    wr.addTank(2, {5600, 5000}, false);
    stepWorld(wr, {useAt(0, 0), Input{}});
    CHECK(wr.rockets.size() == 1 && wr.tanks[0].active == Power::Rocket && wr.tanks[0].recharge == 0);
    run(wr, 30, {Input{}, Input{}});
    CHECK(wr.rockets.empty() && std::fabs(wr.tanks[1].health - 40.0f) < 0.5f && wr.tanks[0].health == kMaxHealth);
    CHECK(wr.tanks[0].active == Power::None && wr.tanks[0].recharge > 5.0f);

    // Rockets blow a hole in the wall they hit: the wall splits in two and the gap is open.
    World ww;
    ww.walls.clear();
    ww.walls.push_back({{5400, 4400}, {5400, 5600}, 18.0f});
    ww.addTank(1, {5000, 5000}, false).held = Power::Rocket;
    stepWorld(ww, {useAt(0, 0)});
    bool used = false;
    for (const Event& e : ww.events) used |= e.type == EvType::PowerUsed && e.a == 1 && e.b == (uint32_t)Power::Rocket;
    CHECK(used);
    bool broke = false;
    for (int i = 0; i < 30 && !broke; ++i) {
      stepWorld(ww, {Input{}});
      for (const Event& e : ww.events) broke |= e.type == EvType::WallBreak;
    }
    CHECK(broke && ww.walls.size() == 2 && !ww.lineBlocked({5000, 5000}, {5800, 5000}));
    CHECK(ww.lineBlocked({5000, 4500}, {5800, 4500}) && ww.lineBlocked({5000, 5500}, {5800, 5500}));

    // Homing rocket follows the cursor.
    World wh;
    wh.addTank(1, {5000, 5000}, false).held = Power::Homing;
    Input hu = useAt(5000, 5700);  // cursor straight below; rocket starts going right (aim 0)
    stepWorld(wh, {hu});
    Input cur; cur.tx = 5000; cur.ty = 5700;
    run(wh, 12, {cur});
    CHECK(wh.rockets.size() == 1 && wh.rockets[0].vel.y > 300.0f);
    CHECK(wh.tanks[0].active == Power::Homing);

    // Air Strike drops 4 bombs scattered around the target (not in a line); they all go off, then the cooldown starts.
    World wa;
    wa.addTank(1, {5000, 5000}, false).held = Power::AirStrike;
    wa.addTank(2, {5400, 5000}, false);
    stepWorld(wa, {useAt(5400, 5000), Input{}});
    CHECK(wa.strikes.size() == 1 && wa.tanks[0].active == Power::AirStrike);
    float sideMin = 1e9f, sideMax = -1e9f;
    for (const Vec2& d : wa.strikes[0].drop) { sideMin = std::min(sideMin, d.y); sideMax = std::max(sideMax, d.y); }
    CHECK(sideMax - sideMin > 60.0f);
    int booms = 0;
    for (int i = 0; i < 75; ++i) { stepWorld(wa, {Input{}, Input{}}); for (const Event& e : wa.events) booms += e.type == EvType::Boom; }
    CHECK(wa.bombs.empty() && wa.strikes.empty() && booms == kStrikeBombs);
    CHECK(wa.tanks[0].active == Power::None && wa.tanks[0].recharge > 5.0f);

    // Lightning fires a ring of bolts that stun what they hit (no moving, no shooting).
    World wl;
    wl.addTank(1, {5000, 5000}, false).held = Power::Lightning;
    wl.addTank(2, {5250, 5000}, false);
    stepWorld(wl, {useAt(0, 0), Input{}});
    CHECK((int)wl.bullets.size() == kBolts);
    run(wl, 8, {Input{}, Input{}});
    CHECK(wl.tanks[1].health < kMaxHealth && wl.tanks[1].stunT > 0);
    const Vec2 stunPos = wl.tanks[1].pos;
    run(wl, 10, {Input{}, fireAt(0)});
    CHECK((wl.tanks[1].pos - stunPos).len() < 1.0f);

    // Spread Shot fires 5 bullets per shot.
    World wsp;
    wsp.addTank(1, {5000, 5000}, false).held = Power::Spread;
    Input su = fireAt(0); su.usePowerUp = true;
    stepWorld(wsp, {su});
    CHECK((int)wsp.bullets.size() == kSpreadCount);

    // Dash moves fast for a short distance.
    World wd;
    wd.addTank(1, {5000, 5000}, false).held = Power::Dash;
    Input du = mv(1, 0); du.usePowerUp = true;
    stepWorld(wd, {du});
    run(wd, 6, {Input{}});
    CHECK(wd.tanks[0].pos.x > 5250);

    // Turret: one at a time, shoots at enemies; the cooldown starts when it's gone.
    World wt;
    wt.addTank(1, {5000, 5000}, false).held = Power::Turret;
    wt.addTank(2, {5400, 5000}, false);
    stepWorld(wt, {useAt(0, 0), Input{}});
    CHECK(wt.turrets.size() == 1 && wt.tanks[0].active == Power::Turret);
    run(wt, 60, {Input{}, Input{}});
    CHECK(wt.tanks[1].health < kMaxHealth && wt.tanks[0].recharge == 0);
    run(wt, (int)(kTurretLife * 30), {Input{}, Input{}});
    CHECK(wt.turrets.empty() && wt.tanks[0].active == Power::None && wt.tanks[0].recharge > 2.0f);

    // Teleport jumps to the cursor (limited to what you can see).
    World wtp;
    wtp.addTank(1, {5000, 5000}, false).held = Power::Teleport;
    stepWorld(wtp, {useAt(5600, 5200)});
    CHECK((wtp.tanks[0].pos - Vec2{5600, 5200}).len() < 1.0f);

    // Invisibility: bots can't see you until you shoot.
    World wi;
    wi.rng.s = 5;
    wi.addTank(1, {5000, 5000}, false).held = Power::Invisible;
    wi.addTank(2, {5350, 5000}, true);
    stepWorld(wi, {useAt(0, 0), Input{}});
    run(wi, 45, {Input{}, Input{}});
    CHECK(wi.tanks[0].health == kMaxHealth && isHidden(wi.tanks[0]) && !canSpot(wi.tanks[0]));
    stepWorld(wi, {fireAt(3.14f), Input{}});
    CHECK(canSpot(wi.tanks[0]));

    // Singularity pulls an enemy in; touching it stops them shooting and using superpowers. It lasts its full time.
    World wg;
    wg.addTank(1, {5000, 5000}, false).held = Power::Singularity;
    wg.addTank(2, {5500, 5000}, false).held = Power::Shield;
    stepWorld(wg, {useAt(5300, 5000), Input{}});
    run(wg, 60, {Input{}, Input{}});
    CHECK(wg.tanks[1].pos.x < 5500 - 50 && wg.tanks[1].health == kMaxHealth);
    run(wg, 60, {Input{}, Input{}});
    CHECK(wg.tanks[1].silenceT > 0 && wg.sings.size() == 1);
    Input tryAll = fireAt(3.14f); tryAll.usePowerUp = true;
    const size_t nb = wg.bullets.size();
    stepWorld(wg, {Input{}, tryAll});
    CHECK(wg.bullets.size() == nb && wg.tanks[1].active == Power::None);
    run(wg, (int)(kSingLife * 30), {Input{}, Input{}});
    CHECK(wg.sings.empty() && wg.tanks[0].active == Power::None && wg.tanks[0].recharge > 3.0f);

    // Instinct: a hit is held; pressing E in time dodges it, otherwise it lands.
    World wn;
    wn.addTank(1, {5000, 5000}, false).held = Power::Instinct;
    stepWorld(wn, {useAt(0, 0)});
    CHECK(wn.tanks[0].active == Power::Instinct && wn.tanks[0].dodges == kInstinctDodges);
    damageTank(wn, wn.tanks[0], 40, 9);
    CHECK(wn.tanks[0].health == kMaxHealth && wn.tanks[0].pendingT > 0);
    stepWorld(wn, {useAt(0, 0)});
    CHECK(wn.tanks[0].health == kMaxHealth && std::fabs(wn.tanks[0].dodges - (3.0f - 2.0f / 3.0f)) < 0.01f);
    damageTank(wn, wn.tanks[0], 40, 9);
    run(wn, 60, {Input{}});
    CHECK(wn.tanks[0].health == kMaxHealth - 40);

    // Cooldown flow: can't use while recharging; ready again afterwards.
    World wc;
    wc.addTank(1, {5000, 5000}, false).held = Power::Lightning;
    stepWorld(wc, {useAt(0, 0)});
    wc.bullets.clear();
    stepWorld(wc, {useAt(0, 0)});
    CHECK(wc.bullets.empty());
    run(wc, 9 * 30, {Input{}});
    stepWorld(wc, {useAt(0, 0)});
    CHECK((int)wc.bullets.size() == kBolts);
  }

  // ---------------- Relics ----------------
  {
    // Two slots; the oldest is relic[0] (R). F swaps out the oldest.
    World w;
    w.addTank(1, {5000, 5000}, false);
    auto drop = [&](Relic k, Vec2 p) { RelicPickup r; r.id = w.newId(); r.pos = p; r.kind = k; w.relics.push_back(r); };
    drop(Relic::Radar, {5020, 5000});
    stepWorld(w, {Input{}});
    drop(Relic::Repair, {5020, 5000});
    stepWorld(w, {Input{}});
    CHECK(w.tanks[0].relic[0] == Relic::Radar && w.tanks[0].relic[1] == Relic::Repair);
    drop(Relic::Sentry, {5020, 5000});
    stepWorld(w, {Input{}});
    CHECK(w.relics.size() == 1);  // both slots full: not picked up
    Input sw; sw.swap = true;
    stepWorld(w, {sw});
    CHECK(w.tanks[0].relic[0] == Relic::Repair && w.tanks[0].relic[1] == Relic::Sentry && w.relics[0].kind == Relic::Radar);

    // Sentry Drone appears and shoots enemies; tapping its key turns it off.
    w.addTank(2, {5450, 5000}, false);
    run(w, 60, {Input{}, Input{}});
    CHECK(w.drones.size() == 1 && w.tanks[1].health < kMaxHealth);
    Input tap; tap.relicDown[1] = true;
    stepWorld(w, {tap, Input{}});
    stepWorld(w, {Input{}, Input{}});
    CHECK(!w.tanks[0].sentryOn);

    // Repair: +1 HP/s after 5 s unharmed; holding heals more but you take +50% damage.
    World wr;
    Tank& t = wr.addTank(1, {5000, 5000}, false);
    t.relic[0] = Relic::Repair; t.health = 50; t.unharmed = 0;
    run(wr, 30 * 4, {Input{}});
    CHECK(wr.tanks[0].health == 50);
    run(wr, 30 * 3, {Input{}});
    CHECK(wr.tanks[0].health > 50.5f);
    Input hold; hold.relicDown[0] = true;
    const float h0 = wr.tanks[0].health;
    run(wr, 30 * 3 + 2, {hold});
    CHECK(wr.tanks[0].health > h0 + 15.0f);
    stepWorld(wr, {hold});
    const float h1 = wr.tanks[0].health;
    damageTank(wr, wr.tanks[0], 10, 9);
    CHECK(std::fabs(h1 - wr.tanks[0].health - 15.0f) < 0.01f);

    // Loot Drone fetches an upgrade you can use and brings it next to you.
    World wl;
    wl.addTank(1, {5000, 5000}, false).relic[0] = Relic::Loot;
    Upgrade u; u.id = wl.newId(); u.pos = {5500, 5000}; u.stat = StatRange; u.rarity = Rarity::Rare;
    wl.upgrades.push_back(u);
    run(wl, 30 * 4, {Input{}});
    bool delivered = false;  // (the world also keeps spawning random upgrades elsewhere)
    for (const Upgrade& x : wl.upgrades) delivered |= x.stat == StatRange && x.rarity == Rarity::Rare && (x.pos - wl.tanks[0].pos).len() < 80.0f;
    CHECK(delivered);
    // Shot down on a trip: it's gone, with its relic.
    Upgrade u2 = u; u2.id = wl.newId(); u2.pos = {5600, 5000}; u2.stat = StatSpeed;
    wl.upgrades.push_back(u2);
    run(wl, 20, {Input{}});
    CHECK(!wl.drones.empty() && wl.drones[0].vulnerable());
    for (int i = 0; i < kLootHp; ++i) {
      Bullet b; b.id = wl.newId(); b.owner = 99; b.pos = wl.drones[0].pos; b.vel = {1, 0}; b.damage = 10; b.radius = 8; b.rangeLeft = 100;
      wl.bullets.push_back(b);
      stepWorld(wl, {Input{}});
    }
    CHECK(wl.drones.empty() && wl.tanks[0].relic[0] == Relic::None);
  }

  // ---------------- Walls ----------------
  {
    World ww;
    ww.walls.push_back({{5200, 4000}, {5200, 6000}, 18.0f});
    ww.addTank(1, {5000, 5000}, false);
    run(ww, 90, {mv(1, 0)});
    CHECK(ww.tanks[0].pos.x < 5200 - kTankRadius + 0.5f);
    World wb;
    wb.walls.push_back({{5300, 4000}, {5300, 6000}, 18.0f});
    wb.addTank(1, {5000, 5000}, false);
    wb.addTank(2, {5600, 5000}, false);
    run(wb, 90, {fireAt(0), Input{}});
    CHECK(wb.tanks[1].health == kMaxHealth);
    CHECK(wb.lineBlocked({5000, 5000}, {5600, 5000}) && !wb.lineBlocked({5000, 3000}, {5000, 3500}));
  }

  // ---------------- Closing Ring ----------------
  {
    World w;
    w.init(5);
    w.time = 10;
    RingState r0 = ringAt(w);
    CHECK(r0.phase == -1 && r0.dmgPerSec == 0 && r0.cur.r >= kMapSize * 0.7f);
    float prevR = 1e9f;
    for (float t = 0; t <= 600; t += 1.0f) {
      w.time = t;
      RingState r = ringAt(w);
      CHECK(r.cur.r <= prevR + 0.01f);
      prevR = r.cur.r;
    }
    w.time = 600;
    CHECK(ringAt(w).cur.r < 1.0f);

    World wd;
    wd.ringEnabled = true;
    wd.zones[0] = {{5000, 5000}, kRingFullRadius};
    for (size_t k = 1; k < wd.zones.size(); ++k) wd.zones[k] = {{5000, 5000}, 200};
    wd.addTank(1, {9000, 5000}, false);
    wd.addTank(2, {5000, 5000}, false);
    wd.time = 400;
    run(wd, 1, {Input{}, Input{}});
    CHECK(wd.tanks[0].health < kMaxHealth && wd.tanks[1].health == kMaxHealth);
    for (int i = 0; i < 600 && wd.tanks[0].alive; ++i) stepWorld(wd, {Input{}, Input{}});
    CHECK(!wd.tanks[0].alive);
    CHECK(wd.matchOver && wd.winnerId == 2 && wd.tanks[1].place == 1 && wd.tanks[0].place == 2);

    World wg;
    wg.addTank(1, {5000, 5000}, false);
    wg.godId = 1;
    damageTank(wg, wg.tanks[0], 500, 9);
    CHECK(wg.tanks[0].alive && wg.tanks[0].health == kMaxHealth);

    // Sandbox respawn brings tanks back.
    World wz;
    wz.allowRespawn = true;
    wz.addTank(1, {5000, 5000}, false);
    damageTank(wz, wz.tanks[0], 500, 99);
    CHECK(!wz.tanks[0].alive);
    run(wz, 100, {Input{}});
    CHECK(wz.tanks[0].alive && wz.tanks[0].health == kMaxHealth);
  }

  // ---------------- Arena Guardians ----------------
  {
    World w;
    w.rng.s = 21;
    w.addTank(1, {5000, 5700}, false);
    w.addGuardian(kGuardIdBase, {5000, 5000});
    w.addGuardian(kGuardIdBase + 1, {4400, 4600});
    w.addGuardian(kGuardIdBase + 2, {5600, 4600});
    CHECK(w.tanks.size() == 4 && w.tanks[1].guardian && w.tanks[1].radius > kTankRadius && w.aliveCount() == 1);
    // Calm while nobody is close.
    run(w, 60, {Input{}});
    CHECK(w.bullets.empty());
    // Hurt one: it gets angry and shoots back.
    damageTank(w, w.tanks[1], 5, 1);
    CHECK(w.tanks[1].aggroId == 1);
    bool shot = false;
    for (int i = 0; i < 120 && !shot; ++i) { stepWorld(w, {Input{}}); for (const Bullet& b : w.bullets) shot |= b.kind == BulletKind::Guardian; }
    CHECK(shot);
    // Guardians never hurt each other, and the storm leaves them alone.
    const float gh = w.tanks[2].health;
    damageTank(w, w.tanks[2], 50, w.tanks[1].id);
    damageTank(w, w.tanks[2], 50, 0, true);
    CHECK(w.tanks[2].health == gh && w.tanks[2].aggroId != w.tanks[1].id);
    // The special guardian drops a reward for its killer.
    World wk;
    wk.rng.s = 4;
    Tank& k = wk.addTank(1, {5000, 6500}, false);
    for (int s = 0; s < kStatCount; ++s) k.stat[s] = kMaxPips;
    k.relic[0] = Relic::Radar; k.relic[1] = Relic::Repair;
    wk.addGuardian(kGuardIdBase, {5000, 5000});
    wk.specialGuardian = kGuardIdBase;
    Tank* g = wk.find(wk.specialGuardian);
    damageTank(wk, *g, 10000, 1);
    bool mythic = false;
    for (const Crate& c : wk.crates) mythic |= isMythic(c.type);
    CHECK(!g->alive && mythic);  // nothing missing: always a Mythic superpower
  }

  // ---------------- Smart bots ----------------
  {
    World wb;
    wb.rng.s = 7;
    wb.addTank(1, {5000, 5000}, false);
    wb.addTank(2, {5300, 5000}, true);
    stepWorld(wb, {Input{}, Input{}});
    CHECK(wb.bullets.empty());

    World wr;
    wr.rng.s = 11;
    wr.addTank(2, {5000, 5000}, true).health = 20;
    wr.addTank(1, {5400, 5000}, false);
    wr.addTank(3, {5400, 5100}, false);
    float d0 = (wr.tanks[0].pos - wr.tanks[1].pos).len();
    run(wr, 60, {Input{}, Input{}, Input{}});
    CHECK(wr.tanks[0].mind.state == BotState::Retreat);
    CHECK((wr.tanks[0].pos - wr.tanks[1].pos).len() > d0);

    World wz;
    wz.rng.s = 13;
    wz.ringEnabled = true;
    wz.zones[0] = {{5000, 5000}, kRingFullRadius};
    for (size_t k = 1; k < wz.zones.size(); ++k) wz.zones[k] = {{5000, 5000}, 1500};
    wz.time = 120;
    wz.addTank(2, {8500, 5000}, true);
    wz.tanks[0].health = wz.tanks[0].maxHealth = 100000;
    run(wz, 90, {Input{}});
    CHECK(wz.tanks[0].mind.state == BotState::Zone);
    CHECK(wz.tanks[0].pos.x < 8500 - 300);

    int dodged = 0, trials = 40;
    for (int s = 1; s <= trials; ++s) {
      World wdg;
      wdg.rng.s = 1000 + s;
      wdg.addTank(2, {5000, 5000}, true);
      Bullet b; b.id = wdg.newId(); b.owner = 99; b.pos = {5300, 5000}; b.vel = {-900, 0};
      b.damage = 19; b.radius = 11; b.rangeLeft = 2000;
      wdg.bullets.push_back(b);
      run(wdg, 12, {Input{}});
      if (wdg.tanks[0].health == kMaxHealth) dodged++;
    }
    CHECK(dodged > trials / 10);
    CHECK(dodged < trials);

    // 20 bots + guardians in a ringed match: exactly one winner, and it's deterministic.
    auto runMatch = [](uint32_t seed) {
      World w;
      w.init(seed);
      for (uint32_t i = 0; i < 20; ++i) w.addTank(1 + i, w.randomPos(600.0f), true);
      w.spawnGuardians();
      for (int i = 0; i < 30 * 900 && !w.matchOver; ++i) stepWorld(w, {});
      return w;
    };
    World m1 = runMatch(2024), m2 = runMatch(2024);
    CHECK(m1.matchOver && m1.winnerId != 0 && m1.winnerId < kGuardIdBase);
    CHECK(m1.winnerId == m2.winnerId && m1.tickCount == m2.tickCount);
    int kills = 0;
    for (const Tank& t : m1.tanks) kills += (int)t.kills;
    CHECK(kills > 0);
    // Preset bots (dev panel, BOTS > Presets): three hand-made brains play whole matches among normal bots without
    // breaking anything, with the same stats as everyone else, and a match with them is deterministic too.
    {
      auto presetMatch = [](uint32_t seed) {
        World w;
        w.init(seed);
        for (uint32_t i = 0; i < 20; ++i) {
          Tank& b = w.addTank(1 + i, w.randomPos(600.0f), true);
          if (i < 3) { botSeed(w, b, (int)BotTier::Skilled); b.mind.brain = (uint8_t)(1 + i); b.mind.custom = true; }
        }
        w.spawnGuardians();
        for (int i = 0; i < 30 * 900 && !w.matchOver; ++i) stepWorld(w, {});
        return w;
      };
      int wins[4] = {}, kills[4] = {}, placeSum[4] = {}, matches = 0;
      for (uint32_t seed = 11; seed < 14; ++seed) {
        World a = presetMatch(seed);
        CHECK(a.matchOver && a.winnerId != 0);
        bool sane = true;
        for (const Tank& t : a.tanks) sane = sane && std::isfinite(t.pos.x) && std::isfinite(t.pos.y) && std::isfinite(t.health);
        CHECK(sane);
        for (int k = 0; k < 3; ++k) {
          const Tank& t = a.tanks[(size_t)k];
          CHECK(t.mind.brain == 1 + k && t.powerMul == 1.0f && t.mind.basePower == 1.0f);  // same power as everyone
          kills[1 + k] += (int)t.kills;
          placeSum[1 + k] += (int)t.place;
          if (a.winnerId == t.id) wins[1 + k]++;
        }
        matches++;
        if (seed == 11) { World b = presetMatch(seed); CHECK(a.winnerId == b.winnerId && a.tickCount == b.tickCount); }
      }
      for (int k = 1; k <= 3; ++k)
        std::printf("  preset %-14s wins %d/%d, kills %d, average place %.1f\n", brainName(k), wins[k], matches, kills[k], placeSum[k] / (float)matches);
    }
    // Bots come in all six match tiers (never the test tiers) and every play style.
    int tiers[(int)BotTier::Count] = {}, styles[(int)BotStyle::Count] = {};
    World mix;
    for (uint32_t i = 0; i < 300; ++i) { Tank& b = mix.addTank(1 + i, {5000, 5000}, true); botSeed(mix, b); tiers[(int)b.mind.tier]++; styles[(int)b.mind.style]++; }
    for (int t = 0; t < (int)BotTier::Count; ++t) CHECK(t < kMatchTiers ? tiers[t] > 0 : tiers[t] == 0);
    for (int t : styles) CHECK(t > 5);
    // A match's roster: the better the tier, the fewer of them (per 39 bots: Elite 0-1, Pro 0-2, Newbie 4-7).
    for (uint32_t seed = 1; seed <= 20; ++seed) {
      World r;
      r.rng.s = seed * 7919u;
      for (uint32_t i = 0; i < 39; ++i) r.addTank(2 + i, {5000, 5000}, true);
      assignRoster(r);
      int c[(int)BotTier::Count] = {};
      for (const Tank& t : r.tanks) { CHECK(t.mind.seeded); c[(int)t.mind.tier]++; }
      CHECK(c[(int)BotTier::Elite] <= 1 && c[(int)BotTier::Pro] <= 2 && c[(int)BotTier::Newbie] >= 4 && c[(int)BotTier::Newbie] <= 7);
      CHECK(c[(int)BotTier::Skilled] >= 5 && c[(int)BotTier::Intermediate] >= 9 && c[(int)BotTier::Unreal] == 0 &&
            c[(int)BotTier::Omniscient] == 0 && c[(int)BotTier::Omnipotent] == 0);
    }
    // Omnipotent (a test bot): blinds whoever it fights.
    {
      World o;
      o.addTank(1, {5000, 5000}, false);
      o.godId = 1;
      Tank& g = o.addTank(2, {5500, 5000}, true);
      botSeed(o, g, (int)BotTier::Omnipotent);
      run(o, 90, {Input{}});
      CHECK(o.tanks[0].blindT > 0 || o.tanks[0].health < kMaxHealth);
      CHECK(o.tanks[1].powerMul > 1.0f);
    }
  }

  // ---------------- Bullets bounce off walls ----------------
  {
    World w;
    w.walls.push_back({{5300, 4000}, {5300, 6000}, 18.0f});
    w.addTank(1, {5000, 5000}, false);
    run(w, 1, {fireAt(0)});
    bool back = false;
    for (int i = 0; i < 20; ++i) { stepWorld(w, {Input{}}); for (const Bullet& b : w.bullets) back |= b.vel.x < 0 && b.bounces == 1; }
    CHECK(back);
  }

  // ---------------- Loot crates ----------------
  {
    // Shot open: it spills its loot.
    World w;
    w.rng.s = 3;
    w.addTank(1, {5000, 5000}, false);
    w.spawnBox({5250, 5000}, BoxKind::Normal);
    for (int i = 0; i < 30 * 8 && !w.boxes.empty(); ++i) stepWorld(w, {fireAt(0)});
    CHECK(w.boxes.empty() && !w.upgrades.empty());
    // Contents: 3 things, at least 2 upgrades of different stats, never a superpower and a relic together.
    bool ok = true;
    int powers = 0, relics = 0, legendPowers = 0;
    for (int k = 0; k < 400; ++k) {
      World b;
      b.rng.s = 2654435761u * (uint32_t)(k + 1);
      LootBox x; x.pos = {5000, 5000}; x.kind = k % 2 ? BoxKind::Legendary : BoxKind::Normal;
      openBox(b, x);
      int seen[kStatCount] = {}, ups = 0;
      for (const Upgrade& u : b.upgrades) { ok = ok && !seen[u.stat]; seen[u.stat] = 1; ups++; }
      const int total = ups + (int)b.crates.size() + (int)b.relics.size();
      ok = ok && total == 3 && ups >= 2 && b.crates.size() + b.relics.size() <= 1;
      powers += (int)b.crates.size(); relics += (int)b.relics.size();
      if (k % 2) legendPowers += (int)b.crates.size();
    }
    CHECK(ok && powers > relics && relics > 0 && legendPowers > powers - legendPowers);
    // Small temple crates: 1 or 2 upgrades.
    World s;
    LootBox x; x.pos = {5000, 5000}; x.kind = BoxKind::Small;
    openBox(s, x);
    CHECK(s.upgrades.size() >= 1 && s.upgrades.size() <= 2 && s.crates.empty() && s.relics.empty());
  }

  // ---------------- Legendary Crate ----------------
  {
    World w;
    w.init(9);
    w.addTank(1, {5000, 5000}, false);
    CHECK(w.legendAt >= 120 && w.legendAt <= 180);
    w.time = w.legendAt - kLegendWarn - 0.5f;
    bool warned = false;
    for (int i = 0; i < 30 && !warned; ++i) { stepWorld(w, {Input{}}); for (const Event& e : w.events) warned |= e.type == EvType::LegendWarn; }
    CHECK(warned && w.boxCount(BoxKind::Legendary) == 0);
    w.time = w.legendAt - 0.01f;
    stepWorld(w, {Input{}});
    CHECK(w.boxCount(BoxKind::Legendary) == 1 && insideZone(w.ring.cur, w.legendPos));
  }

  // ---------------- Maps and Arena Temples ----------------
  {
    for (int m = 0; m < kMapCount; ++m) {
      World w;
      w.init(5, Mode::Royale, m);
      bool clear = w.temples.size() == 3;
      for (const Temple& T : w.temples)
        for (const Wall& wl : w.walls)
          if (wl.kind == 0) clear = clear && (closestOnSeg(wl.a, wl.b, T.door) - T.door).len() > 500.0f;
      CHECK(clear && w.mapId == m);
      // The temples are spread out on different sides of the map, and the storm ends at one of them.
      const Zone& eye = w.zones[kRingPhases.size() - 1];
      int inEye = 0;
      for (const Temple& T : w.temples) inEye += (T.door - eye.c).len() + 320.0f < eye.r ? 1 : 0;
      CHECK(inEye == 1);
      for (int i = 0; i < 3; ++i) CHECK((w.temples[i].door - w.temples[(i + 1) % 3].door).len() > 6000.0f);
    }
    World w;
    w.init(5, Mode::Royale, 0);
    w.addTank(1, {2000, 2000}, false);
    w.spawnGuardians();
    const Temple& T = w.temples[0];
    int mobs = 0;
    for (const Tank& t : w.tanks) mobs += t.mob && t.temple == 0 ? 1 : 0;
    CHECK(mobs >= 4 && w.find(T.mageId) && w.aliveCount() == 1);
    // Walk onto the portal: you're inside, far from the map.
    w.tanks[0].pos = T.door;
    stepWorld(w, {Input{}});
    CHECK(w.tanks[0].temple == 0 && (w.tanks[0].pos - T.entry).len() < 60.0f);
    CHECK(T.thick >= 3.0f * 2.0f * kTankRadius && T.chamber - T.thick * 0.5f >= 850.0f);  // very thick walls, a big room
    // Knocked out inside: thrown out with half the health you came in with. Twice, and the temples stay closed to you.
    w.tanks[0].health = 30;  // hurt inside, then knocked out: half of the full health you came in with
    damageTank(w, w.tanks[0], 500, T.mageId);
    CHECK(w.tanks[0].alive && w.tanks[0].temple == -1 && w.tanks[0].health == kMaxHealth * 0.5f && w.tanks[0].templeKicks == 1);
    w.tanks[0].health = 80;
    w.tanks[0].pos = T.door;
    stepWorld(w, {Input{}});
    CHECK(w.tanks[0].temple == 0 && std::fabs(w.tanks[0].templeHp - 80) < 1.0f);
    damageTank(w, w.tanks[0], 500, T.mageId);
    CHECK(w.tanks[0].alive && w.tanks[0].temple == -1 && std::fabs(w.tanks[0].health - 40) < 0.6f && w.tanks[0].templeKicks == 2);
    w.tanks[0].pos = T.door;
    w.events.clear();
    stepWorld(w, {Input{}});
    bool denied = false;
    for (const Event& e : w.events) denied |= e.type == EvType::TempleDenied && e.a == 1;
    CHECK(w.tanks[0].temple == -1 && denied);
    w.tanks[0].templeKicks = 0;  // (let it back in for the rest of the test)
    w.tanks[0].health = kMaxHealth;
    stepWorld(w, {Input{}});
    CHECK(w.tanks[0].temple == 0);
    // Out of time: half your health, and out you go.
    w.tanks[0].health = 80;
    w.tanks[0].templeT = kTempleTime - 0.01f;
    stepWorld(w, {Input{}});
    CHECK(w.tanks[0].temple == -1 && w.tanks[0].health <= 40.5f && w.tanks[0].alive);
    // Beat the Mage: the exit portal sends you to the edge of the safe zone and the temple closes.
    w.tanks[0].pos = T.door;
    stepWorld(w, {Input{}});
    damageTank(w, *w.find(T.mageId), 10000, 1);
    CHECK(w.temples[0].cleared && !w.find(T.mageId)->alive);
    w.tanks[0].pos = w.temples[0].c;
    run(w, 30, {Input{}});
    CHECK(w.tanks[0].temple == 0);  // you have to stand in the portal for 2 s
    run(w, 32, {Input{}});
    CHECK(w.tanks[0].temple == -1 && insideZone(w.ring.cur, w.tanks[0].pos));
    w.tanks[0].pos = w.temples[0].door;
    stepWorld(w, {Input{}});
    CHECK(w.tanks[0].temple == -1);  // a cleared temple can't be entered
    damageTank(w, w.tanks[0], 500, 99);
    CHECK(!w.tanks[0].alive || w.tanks[0].temple == -1);
  }

  // ---------------- Sentry Robot ----------------
  {
    World w;
    Tank& me = w.addTank(1, {5000, 5000}, false);
    me.held = Power::SentryBot;
    me.stat[StatDamage] = 3;
    w.addTank(2, {9000, 9000}, false);
    Input use; use.usePowerUp = true;
    stepWorld(w, {use, Input{}});
    CHECK(w.tanks.size() == 3 && w.tanks[2].sentryOf == 1 && w.tanks[2].stat[StatDamage] == 3);
    CHECK(w.tanks[2].maxHealth == kMaxHealth * kBotHealthMul && w.aliveCount() == 2 && w.tanks[0].active == Power::SentryBot);
    const float h = w.tanks[2].health;
    damageTank(w, w.tanks[2], 10, 1);  // no friendly fire
    CHECK(w.tanks[2].health == h);
    damageTank(w, w.tanks[2], 500, 2);  // beaten: it self-destructs
    bool boom = false;
    for (const Event& e : w.events) boom |= e.type == EvType::Boom && e.b == 5;
    CHECK(!w.tanks[2].alive && boom);
    run(w, 2, {Input{}, Input{}});
    CHECK(w.tanks[0].active == Power::None && w.tanks[0].recharge > 0);
  }

  // ---------------- Sandbox: a smaller play area, its own fog and loot ----------------
  {
    World w;
    w.sandbox = true;
    w.lo = {4500, 4500}; w.hi = {9500, 9500};
    w.fogMode = 2; w.fogR0 = 2400; w.fogR1 = 600; w.fogT0 = 5; w.fogT1 = 25;
    w.lootMul = 2.0f; w.ringScale = 5000.0f / kMapSize; w.spotScale = 0.4f; w.noTemples = true;
    w.init(9, Mode::Royale, 1);
    CHECK(w.temples.empty() && w.ringEnabled);
    bool lootIn = true;
    for (const Upgrade& u : w.upgrades) lootIn = lootIn && u.pos.x > w.lo.x && u.pos.y > w.lo.y && u.pos.x < w.hi.x && u.pos.y < w.hi.y;
    CHECK(lootIn && (int)w.upgrades.size() > 0 && (int)w.upgrades.size() < kTargetUpgrades);
    for (uint32_t i = 0; i < 8; ++i) w.addTank(1 + i, w.spawnSpot(), i > 0);
    w.allowRespawn = true;
    run(w, 30 * 30, {Input{}});
    bool inside = true;
    for (const Tank& t : w.tanks) if (t.alive) inside = inside && t.pos.x >= w.lo.x && t.pos.y >= w.lo.y && t.pos.x <= w.hi.x && t.pos.y <= w.hi.y;
    CHECK(inside && w.ring.cur.r < 2400.0f && w.ring.cur.r >= 600.0f && !w.matchOver);
    // No fog at all.
    World n;
    n.sandbox = true; n.fogMode = 0;
    n.init(3, Mode::Royale, 0);
    CHECK(!n.ringEnabled);
  }

  // ---------------- Admin exclusives, ghosts and dev-set targets ----------------
  {
    World w;
    w.addTank(1, {5000, 5000}, false);
    w.tanks[0].held = Power::CocoBarrage;
    w.addTank(2, {5000, 5650}, false);
    Input use; use.usePowerUp = true; use.tx = 5000; use.ty = 5600; use.aim = 1.5708f;
    stepWorld(w, {use, Input{}});
    int coconuts = 0;
    for (const Bomb& b : w.bombs) coconuts += b.kind == 1 ? 1 : 0;
    CHECK(coconuts == kCocoCount && w.tanks[0].recharge > 0);
    w.tanks[1].pos = w.bombs[4].pos;  // stand where one lands
    run(w, 60, {Input{}, Input{}});
    CHECK(w.tanks[1].health < kMaxHealth);
    // Ivy Snare roots everyone at the cursor.
    w.tanks[0].held = Power::IvySnare; w.tanks[0].recharge = 0;
    stepWorld(w, {use, Input{}});
    CHECK(w.tanks[1].stunT > 2.0f && w.tanks[1].slowT > 3.0f);
    // The admin's superpowers are never dropped.
    w.tanks[1].held = Power::CocoBarrage;
    const size_t before = w.crates.size();
    damageTank(w, w.tanks[1], 1000, 1);
    CHECK(!w.tanks[1].alive && w.crates.size() == before);
    // A ghost isn't noticed by bots; a target set by hand is chased even if the bot couldn't see it.
    World g;
    Tank& me = g.addTank(1, {5000, 5000}, false);
    me.ghost = true;
    g.godId = 1;
    g.addTank(2, {5400, 5000}, true);
    run(g, 60, {Input{}});
    CHECK(g.tanks[1].mind.state != BotState::Engage);
    g.tanks[0].ghost = false;
    g.tanks[0].pos = {9000, 9000};
    CHECK(g.toggleLink(2, 1, false) && g.targets(2, 1));
    g.tanks[1].mind.decideTimer = 0;  // (the dev panel does this too)
    run(g, 3, {Input{}});
    CHECK(g.tanks[1].mind.state == BotState::Engage && g.tanks[1].mind.targetId == 1);
  }

  // ---------------- Dev panel lines: several red targets, blue teams ----------------
  {
    World w;
    w.addTank(1, {2000, 2000}, false);
    w.godId = 1;
    w.addTank(2, {7000, 7000}, true);   // the hunter
    w.addTank(3, {7000, 9000}, false);  // its two targets, far off screen
    w.addTank(4, {10000, 7000}, false);
    CHECK(w.toggleLink(2, 3, false) && w.toggleLink(2, 4, false) && w.links.size() == 2);
    w.tanks[1].mind.decideTimer = 0;
    run(w, 3, {Input{}});
    CHECK(w.tanks[1].mind.state == BotState::Engage && w.tanks[1].mind.targetId == 3);  // the nearer one first
    damageTank(w, w.tanks[2], 1000, 0);
    w.tanks[1].mind.decideTimer = 0;
    run(w, 3, {Input{}});
    CHECK(w.tanks[1].mind.targetId == 4);  // then the next
    // Clicking a line again takes it away.
    CHECK(!w.toggleLink(2, 4, false) && !w.targets(2, 4));

    // Two bots side by side, teamed up with a blue line: they leave each other alone (and can't hurt each other).
    World t;
    t.addTank(1, {2000, 2000}, false);
    t.godId = 1;
    t.addTank(2, {7000, 7000}, true);
    t.addTank(3, {7300, 7000}, true);
    for (int k = 1; k < 3; ++k) {
      botSeed(t, t.tanks[k], (int)BotTier::Pro);
      t.tanks[k].mind.style = BotStyle::Brawler;
      t.tanks[k].mind.aggression = 1.6f;
    }
    {  // (without the line, these two do fight)
      World c = t;
      bool f = false;
      for (int k = 0; k < 300 && !f; ++k) {
        stepWorld(c, {Input{}});
        f = (c.tanks[1].mind.state == BotState::Engage && c.tanks[1].mind.targetId == 3) || c.tanks[1].health < c.tanks[1].maxHealth;
      }
      CHECK(f);
    }
    CHECK(t.toggleLink(3, 2, true) && t.teamed(2, 3) && t.teamed(3, 2));
    bool fought = false;
    for (int k = 0; k < 300; ++k) {
      stepWorld(t, {Input{}});
      for (int j = 1; j < 3; ++j) fought = fought || (t.tanks[j].mind.state == BotState::Engage && t.tanks[j].mind.targetId);
    }
    CHECK(!fought && t.tanks[1].health == t.tanks[1].maxHealth && t.tanks[2].health == t.tanks[2].maxHealth);
    damageTank(t, t.tanks[1], 30, 3);
    CHECK(t.tanks[1].health == t.tanks[1].maxHealth);
    // A red line between them replaces the blue one.
    CHECK(t.toggleLink(2, 3, false) && !t.teamed(2, 3) && t.links.size() == 1);
    t.clearLinks(1);
    CHECK(t.links.empty());
  }

  // ---------------- Rocket Rampage ----------------
  {
    World w;
    w.init(4, Mode::Rampage);
    for (uint32_t i = 0; i < 6; ++i) { Tank& t = w.addTank(1 + i, w.lootPos(300.0f), true); t.held = Power::Rocket; t.cdScale = kRampageCd; }
    w.spawnGuardians();
    CHECK(w.tanks.size() == 6 && w.temples.empty() && w.crates.empty());
    {  // the Rocket's cooldown starts the moment it's fired
      World q;
      q.init(4, Mode::Rampage);
      Tank& t = q.addTank(1, q.lootPos(300.0f), false);
      t.held = Power::Rocket; t.cdScale = kRampageCd;
      Input use; use.usePowerUp = true; use.tx = t.pos.x + 500; use.ty = t.pos.y;
      stepWorld(q, {use});
      CHECK(q.rockets.size() == 1 && q.tanks[0].active == Power::None && q.tanks[0].recharge > 0);
    }
    for (int i = 0; i < 30 * 185 && !w.matchOver; ++i) stepWorld(w, {});
    CHECK(w.matchOver && w.time >= kRampageLen && w.winnerId != 0);
    bool onlyRockets = true, statsOk = true;
    onlyRockets = w.bullets.empty();
    for (const Upgrade& u : w.upgrades) statsOk = statsOk && (u.stat == StatSpeed || u.stat == StatFov);
    CHECK(onlyRockets && statsOk && ringAt(w).cur.r < kRampageR);
    int best = -1, firsts = 0, scored = 0;
    for (const Tank& t : w.tanks) { best = std::max(best, (int)t.score); firsts += t.place == 1 ? 1 : 0; scored += t.score > 0 ? 1 : 0; }
    CHECK(w.find(w.winnerId)->score == best && firsts == 1 && scored > 0);
  }

  std::printf(failures ? "%d test(s) failed\n" : "all tests passed\n", failures);
  return failures ? 1 : 0;
}
