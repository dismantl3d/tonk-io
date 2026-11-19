# Tonk.io — Game Design Bible

> Living reference document. Version 0.1 (2026-11-13). Update this file whenever a design decision changes.

---

## 1. Vision & Pillars

**Tonk.io** is a free-to-play, browser-based multiplayer battle royale where round cartoon tanks roam a huge arena, grab upgrades, superpowers and relics, and fight until one tank is left standing.

**Pillars**
1. **Easy to learn, hard to master.** Move with WASD, aim with the mouse, click to shoot. Skill shows in positioning, aim, and decisions.
2. **No god weapon.** Every weapon is a trade-off. A player with a starter gun can beat a player with a "fancy" gun. Unlocks give *options*, never raw power.
3. **Readable and bright.** Clean, flat visuals in our own bright palette. You always know who is who, what is dangerous, and where the ring is.
4. **Fair and free.** Fully free-to-play. Credits and Legendary Path XP come from playing. Rewards are cosmetic or sidegrade options. No pay-to-win.
5. **Short, exciting matches.** About 8–10 minutes, with an obvious winner.

---

## 2. Tech Architecture

| Layer | Choice |
|---|---|
| Client | C++17 compiled to **WebAssembly** with **Emscripten**, rendering with **WebGL2**. |
| Game server | Authoritative **C++** server using WebSockets. Fixed tick **30 Hz**. Clients use prediction, reconciliation, and snapshot interpolation. |
| Shared code | A `core/` library (movement, collision, weapon stats, damage math) is compiled into both client and server so they agree. |
| Backend | **Firebase**: Authentication, Firestore (profiles, inventory, progress), Cloud Functions or trusted server writes for rewards, Hosting for the client. |
| Hosting | Game servers on a VPS/container host (decide later). Firebase Hosting serves the static client. |

**Security rules of thumb**
- The **server is the authority** on position, hits, kills, and match results. The client only sends inputs.
- Clients never write Credits, XP, or inventory. Only the trusted game server (through a service account) does.
- Players join with a Firebase **ID token**; the server verifies it before admitting them.

### Suggested repo layout
```
tonkio/
  GAME_DESIGN.md
  core/        shared C++ (math, physics, weapons, powerups)
  client/      Emscripten client (renderer, input, UI)
  server/      authoritative match server
  web/         index.html, JS glue, Firebase web SDK
  firebase/    firestore.rules, functions/, firebase.json
  assets/      sprites, audio, fonts
```

---

## 3. Art Direction

**Style: flat and playful, with its own identity.** Round tanks with a darker outline on a warm paper-colored grid,
bright accent colors, chunky rounded type (Fredoka). Tonk.io must not copy another game's palette, fonts or layout.
Polish comes from smooth animation, clean effects, and a good UI.

### Palette
| Use | Color |
|---|---|
| Arena floor | `#F3EFE6` (warm paper) with a soft 48-unit grid and small dots every 240 units |
| Out of bounds | `#D5CCBE` |
| The storm (outside the safe zone) | violet tint `#7C54F2`; the next zone is a dark dashed line |
| Default player wrap | Teal `#2EC4B6` |
| Tanks with no cosmetics | Coral `#FF7A5C` |
| Default barrel | Slate `#A3A7B8` |
| Walls | Slate violet `#7A7599` with a soft shadow |
| Health bar | Green `#73DB66`, amber under 50%, red under 25%, on a plum track |
| UI ink (outlines, text) | Plum `#2B2140` |
| UI cards | Cream `#FFFAF0` |
| UI accents | Orange `#FF8A3D`, teal `#2EC4B6`, violet `#7B5CFF`, pink `#FF5D8F`, sun yellow `#FFC93C` |
| Rarity | Common `#B4B9C6`, Uncommon `#5CD65C`, Rare `#3FA9F5`, Epic `#A45CFF`, Legendary `#FFB31F` |

Shape outlines are 25% darker than the fill. UI cards are "stickers": cream fill, 3 px plum border, a hard plum drop shadow.

### Look & feel rules
- **Camera:** top-down, with a slight zoom out as you get stronger so you can see more.
- **Animation:** barrel recoil on shoot, hit flash (white blink), smooth lerped health bar, a gentle death shrink and pop.
- **Particles:** simple, clean puffs and sparks. Never cover the player's view.
- **UI:** cream sticker cards in the lobby and menus; dark translucent plum chips in game so the HUD reads over the arena. Fredoka everywhere.
- **Audio:** punchy, satisfying shots and pops; a light, upbeat soundtrack in menus.
  Sound effects are synthesized in the browser (web/sfx.js, no audio files) and only play for: using a superpower (each
  has its own sound), rocket and air-strike explosions, walls breaking, hitting and knocking out a tank, and picking up an
  upgrade (higher and fuller with rarity), a superpower (a bigger fanfare for Mythics) or a relic. Your own sounds play
  at full volume; other tanks' get quieter with distance and pan left/right. The speaker button (top-left) or M mutes.
- **Performance target:** 60 FPS on mid-range laptops; draw batching for shapes and bullets.

---

## 4. Core Loop & Match Flow

**Meta loop:** Play match → earn Credits + Legendary Path XP → unlock cosmetics, Arsenal options → customize → play again.

**Match flow (about 8–10 min, up to 40 players)**
1. **Lobby (20 s):** the queue fills; bots fill empty slots if needed.
2. **Drop-in:** players spawn spread across the map with 5 s of spawn protection.
3. **Early (0:00–2:00):** farm shapes for in-match XP, grab crates, avoid big fights.
4. **Mid (2:00–6:00):** the Closing Ring shrinks and pushes players together. Crates get better.
5. **Late (6:00–end):** small ring, final fights; the last tank wins.
6. **Results:** placement, kills, XP, Credits, and Path progress, with bonuses itemized.

**In-match leveling (light):** shooting shapes and tanks gives match XP. Reaching match levels grants small *sidegrade* choices (see §7), not raw power increases. This keeps comebacks possible.

---

## 5. Controls & HUD

| Input | Action |
|---|---|
| `W A S D` | Move. Tanks have momentum: they speed up quickly and skid a little when you let go. |
| Mouse | Aim (and the target point for Rocket steering, Air Strike, Teleport, Singularity, Loot Drone dive) |
| Left click / hold | Shoot |
| `E` / `Shift` | Use your superpower (with Instinct: dodge the hit that just landed) |
| `R` | Your oldest relic: tap or hold, depending on the relic |
| `T` | Your newer relic |
| `F` | Swap: your superpower for one on the ground, or your OLDEST relic for one on the ground |

**HUD:** the kill feed top right (with the players-alive counter above it), the storm timer at the top center, your
title, name and health bar at the bottom center with the **superpower bar** under it (red; it fills while recharging,
drains while active, and shows the superpower's name when ready), and the stats panel bottom left with the 5 upgrade
rows and 2 hexagonal **relic slots** (hover for what they do and what R/T does). Minimap bottom right.

---

## 6. Weapons & Arsenal

### 6.1 Balance rules (non-negotiable)
1. Every weapon is rated 1–10 on **five stats**: **Damage (DMG), Fire Rate (ROF), Range (RNG), Projectile Speed (PSPD), Handling (HDL)**.
2. **Every weapon and every variant has the same total of 25 points.** A variant moves points from one stat to another; it never adds points.
3. Skill expression: some weapons need aim (Sniper), some need positioning (Trapper), some need close fighting (Scatter).
4. Every weapon has **a counter**. No weapon is good at every range.
5. Time-to-kill (TTK) targets: Any weapon against any other lands within a 0.5x–2x TTK window at its best range. This stops "unwinnable" fights.
6. Higher Arsenal levels unlock **more choices**, not more power. A level-1 Standard tank can and should beat an unlucky high-level player.
7. Tank body base stats (health 100, move speed, turn speed) are identical for everyone. Cosmetics never change hitboxes.

### 6.2 Archetypes (base stat distributions, 25 total)

| Archetype | DMG | ROF | RNG | PSPD | HDL | Role & weakness |
|---|---|---|---|---|---|---|
| **Standard** | 5 | 5 | 5 | 5 | 5 | Balanced all-rounder. No strengths, no holes. |
| **Sniper** | 8 | 2 | 8 | 5 | 2 | Long-range, one big hit. Slow to turn, punished up close. |
| **Twin** | 3 | 9 | 4 | 5 | 4 | Fast stream of weak bullets. Short range, loses to long guns. |
| **Cannon** | 8 | 3 | 5 | 3 | 6 | Heavy shells with small splash. Slow, easy to dodge at range. |
| **Scatter** | 6 | 5 | 2 | 5 | 7 | Shotgun spread. Deadly close up; useless far away. |
| **Drone Caller** | 4 | 4 | 7 | 3 | 7 | Controls 3 drones that chase enemies. Your tank is weak while drones live. |
| **Trapper** | 5 | 3 | 4 | 4 | 9 | Lays mines and walls. Low direct damage; strong when defending. |
| **Rocket** | 7 | 2 | 7 | 3 | 6 | Slow homing rockets. Dodgeable, small ammo count. |

### 6.3 Variants (3 sidegrades per archetype, unlocked with Arsenal Tokens or mastery)
Each variant shifts **2 points** between stats; totals remain 25.

| Archetype | Variant A | Variant B | Variant C |
|---|---|---|---|
| Standard | **Longshot:** RNG +2, ROF −2 | **Quickdraw:** ROF +2, DMG −2 | **Brick:** DMG +2, HDL −2 |
| Sniper | **Marksman:** ROF +2, DMG −2 | **Ghost:** PSPD +2, DMG −2 | **Railgun:** DMG +2, RNG −2 |
| Twin | **Triple:** DMG +2, ROF −2 | **Sprinkler:** HDL +2, RNG −2 | **Burst:** RNG +2, HDL −2 |
| Cannon | **Mortar:** RNG +2, PSPD −2 | **Bomber:** DMG +2, HDL −2 | **Lobber:** ROF +2, DMG −2 |
| Scatter | **Wide:** HDL +2, DMG −2 | **Choke:** RNG +2, ROF −2 | **Buckshot:** DMG +2, RNG −2 |
| Drone Caller | **Swarm:** ROF +2, DMG −2 | **Guard:** HDL +2, RNG −2 | **Hunter:** RNG +2, HDL −2 |
| Trapper | **Minefield:** ROF +2, HDL −2 | **Wallbuilder:** HDL +2, DMG −2 | **Spiker:** DMG +2, ROF −2 |
| Rocket | **Salvo:** ROF +2, PSPD −2 | **Seeker:** HDL +2, DMG −2 | **Stinger:** PSPD +2, RNG −2 |

That is **8 archetypes × 4 weapons = 32 weapons.**

### 6.4 Arsenal & mastery
**Status:** the lobby has an Arsenal screen as a placeholder: six locked weapon bays with silhouettes and your Arsenal
Token count. Weapons are not in the game yet; tokens carry over.
- Before each match, choose **one equipped weapon** from the Arsenal.
- Each weapon has **Mastery levels 1–10**, earned by using it (kills, damage, survival time).
- Mastery rewards are **cosmetic** (barrel skin, kill counter, title) and **Arsenal Tokens**, never stat increases.
- Starter weapons: **Standard (base)**. All eight archetypes' base weapons unlock quickly (levels 1–10 of the Path or via Credits). Variants unlock with tokens.
- **Weapon pickups in the arena:** a crate can hold a weapon that replaces your gun for the rest of that life (a "found" weapon uses base archetype stats). This is a risk/reward choice, not a power spike.

### 6.5 In-match sidegrade picks
Match levels (every ~2 shape-clears or kills) offer a pick of **2 random sidegrades**, each trading something, e.g.:
- *Light Plating:* +move speed, −max health.
- *Reinforced Barrel:* +damage, −reload speed.
- *Steady Aim:* +range, −turn speed.
- *Scavenger:* +crate pickup radius, −bullet damage.
Maximum 4 picks per match, so builds stay simple.

---

## 7. Upgrades, Superpowers, Relics & Arena Guardians

### 7.1 Stat upgrades
Six stats: Move Speed (+6%/tier), Bullet Speed (+8%), Bullet Range (+8%), Max Health (+12), **Fire Rate** (+8%) and
**Bullet Damage** (+7%). Each has a tier 0–5. Picking up an upgrade of rarity r sets that stat to tier r+1 (Common 1 …
Legendary 5). Upgrades at or below your current tier are faded out and you drive straight over them. Pickups are
**circles** in their rarity's two colors: Common grey/white, Uncommon green/dark green, Rare blue/dark blue, Epic
purple/dark purple, Legendary yellow/dark yellow with a slight pulse. The HUD's upgrade panel shows only icons and tier
bars; hovering one shows only its name, the rarity and tier you have, and what it gives you (for example "+18% move speed").
Pickups lie flat on the ground (no bobbing, no shadow): a thin rim in the rarity color, a light fill and a crisp icon in
a deeper shade.
**Loot gets better toward the middle of the map** (rarer upgrades, more relics). The map is 14,000 × 14,000 and loot is
kept sparse (about 24 superpowers, 130 upgrades and 11 relics on the map at once, respawning every 9 s / 2.5 s / 30 s),
so a full loadout takes most of a match.

### 7.2 Superpowers (one slot, E / Shift)
Pickups are **rounded squares the same size as an upgrade**: light tomato red with a red rim and icon (no particles,
no bobbing, no shadow); Mythics are dark rounded squares with a rainbow rim. With an empty slot you take one by driving over it; F swaps. In the match HUD the
superpower bar has no tooltip. **The cooldown only starts once the
superpower is over**: when its active time ends, or when what it made (rocket, air strike, turret, singularity) is gone.

| Superpower | Effect | Active | Cooldown |
|---|---|---|---|
| Speed Boost | Double speed (no aura or trail) | 8 s | 6.5 s |
| Shield | Blocks everything | 1.8 s | 5 s |
| Rocket | Rocket with a huge range (twice the view width), explodes on walls/tanks or at the end of its range. A hit takes 60% of max health (max 80). Blows a hole in walls | until it explodes | 6 s |
| Homing Rocket | Follows your cursor (slower and turns less sharply than before); explodes after 5 s, on a wall/tank, or when it leaves your screen. A hit takes 65% of max health (max 85). Blows a hole in walls | until it explodes | 7.5 s |
| Rapid Shot | 2.4 times the fire rate, bullets 20% faster | 5.5 s | 4.5 s |
| Air Strike | A fast plane passes over your cursor and drops 4 bombs scattered around it (not in a line); each goes off 0.3 s after it lands | until the last bomb | 7 s |
| Spread Shot | Each shot fires 5 bullets in a fan (slightly bigger than normal, 95% damage each) | 3 s | 5 s |
| Lightning | 12 jagged bolts burst out in a circle. 40 damage each, and a hit **stuns for 1 s** (no moving, shooting or superpowers) | instant | 8 s |
| Dash | A very fast, short burst in the direction you're moving | 0.16 s | 2.3 s |
| Turret | Places a turret for 8 s that shoots the nearest enemy (good but not perfect aim, normal damage). One at a time | 8 s | 4.5 s |
| Teleport | Jump to your cursor (within your view) | instant | 7 s |
| Invisibility | You see a faded dot; others see nothing. Firing gives away where the shot came from for 1.2 s, and bots shoot there | 4 s | 6 s |

**Mythic superpowers** come from the Arena Mage, or (20% of the superpowers in it) a Legendary Crate:

| Mythic | Effect | Cooldown |
|---|---|---|
| Singularity | Placed at your cursor for **9 s**: pulls enemies in and slows them. Touching its core **silences** them (no shooting, no superpowers) while they touch it and 1.5 s after. It is not destroyed by touching | 8 s |
| Instinct | Press E to start: 3 dodges. When a hit lands, press E within the window (1.5 s, a little shorter after each hit) to dodge it. A successful dodge gives back a third of a dodge; a missed one costs a whole dodge and the hit lands. Ends when the dodges run out | 10 s |
| Sentry Robot | Deploys a grey copy of you: your upgrades at that moment, 25% less health, no relics or superpowers, a small "BOT" name tag reading "<name>'s Sentry". It fights for you (your bullets and its bullets never hurt each other) and its knockouts count as yours. It starts with a Newbie's skill and becomes a Rookie at 15 s, Intermediate at 35 s and Skilled at 55 s. It self-destructs (an explosion) when beaten, when you are, or after 60 s | 12 s |

**Bullets bounce off walls** (up to 4 times) and fly the whole length of their range: they swell a little and fade out
over the last 35% instead of stopping. Base range is longer (Standard: 770, was 650). Your own bullets never hurt you.

**Rockets** look like a white missile with a colored nose and band (red Rocket, magenta Homing), swept fins and a
flame. **Explosions** have a white-hot flash and core, a fireball of puffs that cool from yellow to orange to smoke,
sparks, dark chunks, lingering smoke and a scorch mark.

**Rockets break walls:** when a Rocket or Homing Rocket explodes, every wall within 130 units loses that stretch (about
a 280-unit gap); leftover pieces shorter than 70 units crumble too. Broken walls don't come back during the match.

### 7.3 Relics (two slots, R / T)
Hexagon pickups, rarer than upgrades and more common toward the middle. You can carry two different relics. The oldest is
on R, the newer on T. With both slots full, F next to a relic swaps out the oldest.

| Relic | Passive | Key |
|---|---|---|
| Repair | +1 HP/s after 5 s without damage | Hold: heal 15 HP every 3 s (plus 1 HP/s), but move 50% slower and take 50% more damage |
| Sentry Drone | Circles you and shoots enemies; can't be hurt while circling | Tap: on/off. Hold: overload: faster fire, chases enemies, but can be shot down (4 hits or a rocket); rebuilds after 15 s |
| Loot Drone | A yellow cargo quadcopter. Fetches upgrades you can use and drops them next to you; what it carries hangs under it at full size. **One hit** on a trip destroys it and the relic (you get a notification) | Hold 2 s: dives at your cursor and explodes; uses up the relic. A destroyed drone drops what it carried |
| Radar | Your minimap shows enemies and walls. Without Radar you only see the storm, the temples and yourself | — |

Both drones are quadcopters (four rotors on crossed arms); the Sentry Drone has its owner's color and a gun. In the
HUD an empty relic slot's tooltip just says "Empty Relic Slot".

### 7.4 Drops
A beaten tank drops **every upgrade tier it had** (as pickups of matching rarity), its superpower, and its relics, **one
of which is destroyed**. The killer heals 25 HP.

### 7.5 Arena Guardians
Big, slow guardians with high health (650), heavy slow bullets (42 damage), a slightly slower fire rate, but
near-perfect aim and prediction. They stay calm until someone hurts them or walks into their small field of view, don't
count as players, never shoot through or hurt each other, and circle-strafe close to their post. There are no longer
any in the middle of the map: the only one is the **Arena Mage** at the heart of each temple (section 7.6), and it
always drops the big reward for its killer:
- If you're missing anything: 35% chance of a **Mythic superpower**; otherwise a **Legendary upgrade for a stat you
  haven't maxed** (more likely, sometimes two) or a **relic** if you have a free slot.
- If you have every stat at Legendary and both relic slots full: a Mythic superpower.

### 7.6 Arena Temples
Every Battle Royale map has **3 temples**, spread out on different sides of the map (about 4,300 to 4,650 from the
middle). The storm always ends at one of them (picked each match), so that one stays in **the eye of the storm** until
the final collapse; the other two are swallowed by the storm along the way. Outside, a temple is a stepped octagonal
platform with stairs on four sides, four square pillars, braziers, an inlaid ring and a glowing portal on a raised dais. Step into the portal to go inside; the screen
dims for a moment and comes back.
The inside is its own place, not part of the map: a dark stone floor with a carpet down every corridor, braziers, and
a **spiral** of massive stone walls (200 units thick, about 3.5 tanks across, drawn as one continuous run of blocks;
rings 500 apart with a 300-wide corridor between them, one gap each and a dead end next to every way in, so the only
way to the middle is round and round). The Mage's room in the middle is 1,800 across (with four pillars), so there is
room to fight. **You are the light:** it falls off smoothly around your tank, and the walls cast shadows away from you,
so you only see what has a clear line to you (along your corridor, through a gap, onto the tops of the walls facing
you). The shadows move smoothly with you. Everything in shadow and the edges of the screen are dark, and names there
are hidden. Each map's temples have their own shape and colors (Crossroads: 3 rings, gold; Ruins: 3 rings the other way
round, jade; Canyon: 4 rings, crimson). Several players and bots can be in one temple at once.
- **Each lap** of the spiral has 2-3 small **Temple Guardians** (slightly smaller than a tank, 55 health, 55% bullet
  damage). They are clumsy on purpose: wild aim, no leading, slow to react and turn, and they never dodge. Each lap also
  has 2 small crates (1-2 upgrades each). A temple has 6-9 Temple Guardians. Each one you beat gives **+30 Path XP**
  (it isn't a kill).
- In the middle is the **Arena Mage**: a big guardian that moves a little faster. Beating it always drops the big
  reward (the guardian reward with better odds: 35% Mythic, sometimes two Legendary upgrades).
- Then a portal opens in the middle of the Mage's room. **Stand in it for 2 seconds** (dots around you fill up) and it
  takes you to **a random spot just inside the edge of the safe zone**. The temple closes 25 s later (anyone still
  inside is sent out the same way), its pillars vanish and a cracked patch is left.
- **Knocked out inside:** you aren't eliminated. The temple throws you out next to it with **half the health you had
  when you went in** (not half your max). The first time a note warns you; the **second** time a big alert says the
  temples are closed to you, and stepping on any temple portal after that only shows a note.
- **3.5 minutes** inside and you lose half your health and are thrown out.
- Inside, the storm can't reach you, the banner shows the time left, and the minimap is titled "Arena Temple" and only
  shows the walls around you.
- Bots: about 45% of bots (never Survivors) go in when they're healthy and geared (and not knocked out twice), and
  follow the spiral. Inside they only react to what they can see, like you.

### 7.7 Crates
Wooden **crates** (18 on the map, a new one every 10 s), bigger than a tank (80 units across; Legendary 104, the
small temple ones 64), block tanks and bullets. Shoot them (70 HP) or blow them up and
they break into **3 items**: always at least 2 upgrades of different stats, and the third is another upgrade (58%), a
superpower (30%) or a relic (12%), never a superpower and a relic together. Better rarities are rarer
(Common 45%, Uncommon 30%, Rare 16%, Epic 7%, Legendary 2%).
**Legendary Crate:** gold, with a star. The first lands at a random time 2-3 minutes in, then another every 2-3 storm
phases (none in the last two). 10 s before it lands everyone gets a note and a **red X** on the minimap shows where. It
always lands inside the eye of the storm (the next safe zone), never in a wall or temple. 260 HP. Same rules, much
better odds: upgrade 25%, superpower 50% (1 in 5 of them Mythic), relic 25%; rarities Common 4%, Uncommon 14%, Rare 30%,
Epic 32%, Legendary 20%.

---

### 7.8 Bots
Tonk.io is meant to be multiplayer; until there are enough players, bots fill the lobby and should feel like people:
different skill, different goals, different cosmetics. They must not have the computer's advantages (knowing where
every bullet is and reacting instantly, or aiming perfectly), yet still be a challenge. Each bot has a **skill tier**,
a **play style** (type), an **aggression meter** and **affinities**.

**Fairness (every match tier).** A bot only sees what a player would see on screen (in a temple's dark, only what has
a clear line to it), doesn't know other tanks' upgrades, and loses invisible tanks unless they just fired. Its aim leads
with a lagging idea of your velocity, so sharp turns shake it off. It **dodges like a person**: it judges each shot it
notices once; if it decides to dodge, the dodge only starts after its reaction time (so close or fast shots land), and
lower tiers sometimes step the wrong way. After a dodge it can't try again for 0.9 s.

| Tier | Per 39 bots | Aim | Dodge (react) | Game sense |
|---|---|---|---|---|
| **Newbie** | 4-7 | Wild, no leading, very slow to react | never | None: near-random objectives, grabs any loot (even useless upgrades), ignores who's near it, fires superpowers off whenever, only notices the storm once it's on it, runs below 40% health |
| **Rookie** | the rest (most) | Rough, a little lead | 3% (0.5 s) | Has a set goal from its type and sticks to it (a Rookie Brawler fights, a Rookie Looter loots); runs below 33% health |
| **Intermediate** | 9-12 | Decent, half lead | 7% (0.42 s) | A casual player. The first tier with **varied objectives**: it changes objective with the situation (e.g. fights for loot) |
| **Skilled** | 5-8 | Good | 11% (0.36 s) | Better judgment of what to do and when; goes for health when hurt (Recover) |
| **Pro** | 0-2 | Very good | 14% (0.32 s) | Like Skilled, a slightly higher ceiling |
| **Elite** | 0-1 | Best of the match tiers | 17% (0.29 s) | A bit smarter and better at fighting than Pro |

Better tiers are rarer: each match's bots are dealt from that roster (scaled down for smaller lobbies, e.g. Rocket
Rampage's 19). Three more tiers are **test bots, only made from the dev panel** (never in a match):
- **Unreal:** the computer's advantage on purpose: exact intercept aim with the real velocity, 0.07 s reactions, 85%
  dodges, and a little more aggression.
- **Omniscient:** Unreal's fighting, plus it knows where everything on the map is: it heads for loot and crates
  anywhere, and hunts tanks anywhere (preferring ones carrying a relic it lacks, or low on health).
- **Omnipotent:** Omniscient, plus x1.6 damage, fire rate, move speed, range and bullet speed (x2.2 while fighting, it
  "changes its stats at will"), it **teleports** to whatever it's after (every 3 s), and it **blinds** whoever it fights
  for 2.5 s (every 8 s): their screen goes almost black except right around their tank, and a blinded bot can only see
  what's right next to it. Every 6 s it also **pulls** whoever it's fighting (if they're over 350 away) right up next to
  itself.

**Types (play styles):** **Looter** (gears up first, fights once it has enough), **Brawler** (fights whatever is in its
way), **Tactician** (fights only with an advantage, backs off early), **Survivor** (avoids enemies, plays the zone),
**Hunter** (picks a target and chases it, even out of sight for a while) and **Raider** (goes for the Arena Temples,
from further away and with less gear than others).

**Aggression meter** (0-100%, 50% neutral): higher means more willing to fight, and, past about 58%, now and then picking
a fight for no reason. Lower means a more peaceful bot that prefers loot, the zone and staying alive; below about 33% it
steers clear of fights it doesn't want, like a Survivor.

**Affinities:** how much it likes looting, fighting and raiding, from its type and aggression with a little luck. They
scale how much loot is worth to it and how willing it is to fight.

**Objectives** (what it's doing right now; shown by the dev panel's Bot Info Overlay):
- **Roam:** wander the zone until something triggers another objective (which one depends on the bot). In Rocket
  Rampage it roams to quiet spots far from everyone, so the arena stays spread out.
- **Loot** (and **Crate**): go for an upgrade, superpower or relic, or shoot a crate open (the Legendary Crate is worth a
  lot to everyone).
- **Attack:** fight a tank it can see. Better tiers judge the fight (both tanks' health and damage, who else is near).
- **Contest** (Intermediate and up): loot with an enemy next to it. It weighs the loot, how the fight looks and its
  aggression, and may fight for it. Example: a Legendary Damage upgrade next to a player on 50/100 HP, the bot on 40/100:
  an aggressive bot fights for it, a peaceful one leaves it.
- **Hunt:** chase a target it lost sight of (Hunters for a long while), or, for Omniscient bots, one anywhere.
- **Retreat:** run when the fight looks bad (Newbies and Rookies at a fixed health line), grabbing close loot on the way
  out (smarter bots) and heading for the safe zone.
- **Zone:** get inside the safe zone before the storm (smarter bots go earlier).
- **Raid:** head into an Arena Temple and follow its spiral.
- **Recover** (Skilled and up): hurt with nobody around, go for health upgrades and keep to the quiet middle of the zone.
- Temple Guardians **Guard** their spot; Sentry Robots **Follow** their owner.

Movement is smoothed so bots don't twitch: the direction eases toward what they want and a key stays down until the
direction has clearly changed. Against a wall they slide along it, a wall between them and their goal makes them head for
a clear spot round it first, and loot they can't get any closer to for 3 s (or that got them stuck) is ignored for a
while. Loot never spawns inside a wall. All bots get out of enemy turrets' reach, Singularities and bomb blasts, and aim
worse while dodging.

In a 40-bot simulation (four full matches), kills per bot rise with tier (Newbie 0.05, Rookie 0.6, Intermediate 0.9,
Skilled about 3), and bots spend their time roughly: Roam 29%, Retreat 17%, Attack 16%, Loot 15%, Raid 13%, Hunt 5%.

**Dev panel, BOTS page** (the **BOTS** button at the top of the dev panel): a **Bot Info Overlay** checkbox (a card
under every bot: tier and type, objective and target, health, aggression and dodge chance, superpower and relics,
upgrades, affinities), state dots, Watch bot, and **Make a bot**: skill tier (all nine), type, aggression, aim and dodging
(or the tier's own), a power multiplier, upgrade tiers for every stat, a superpower and two relics. **Remove my bots**
takes them out of the match.

**Presets** (dev panel, BOTS page, above Make a bot): three hand-made bots with their own brains (`core/bot_presets.h`),
spawned with **Spawn** (or **Spawn all three**; "spread across the arena" puts them at random spots instead of next to you).
They have exactly the same power as every other bot (100 HP, the Standard gun, no upgrades, no power multiplier) and the
same fairness rules: they only act on what is on their screen, estimate velocities from positions they have seen, judge each
bullet once after a human-like reaction delay, and only produce a normal Input. They stay out of the Arena Temples (inside
one, or in Rocket Rampage, they fall back to the normal brain). The Bot Info Overlay shows their name and "preset".
- **Aegis-Vulture:** states Storm Evac, Disengage, Vulture Execute, Skirmish, Harvest, Spiral Roam. Loots along an inward
  spiral, leaves fights when hurt (below 42% health, or 55% with an enemy close), and collapses on tanks below 45% health or
  tanks busy shooting each other.
- **Ghost Current:** states Storm Rotation, Disengage, Current Shift, Engage, Scavenge, Navigate, Final Hunt (three or fewer
  opponents left). Circles around the enemy (Current Shift) to break their firing angle instead of running straight away.
- **Carrion Clock:** states Zone, Evade, Finish, Duel, Loot, Stalk, Hold. Scores each visible enemy with a Fight Value Score
  (FVS) and only starts a Duel at FVS 2 or more; otherwise it loots, holds a wall-side spot just inside the ring, or finishes
  weak tanks.

**Lines** are drawn in the arena with the mouse: pick a color, click a bot, then click (or drag to) other tanks.
- **Red (targets):** an arrowed line from a bot to a tank it must go after. A bot can have any number; it always knows
  where they are and takes the nearest first (sticking with the one it's on unless another is much closer), and a red
  line overrides everything else it would do. Red lines start at a bot; they can end at any tank, you included.
- **Blue (teams):** a dashed line between two tanks, drawn from either one and applying both ways: they never go after
  each other, and their shots and blasts can't hurt each other. A blue line with you on one end means that bot leaves
  you alone.
- A red and a blue line can't join the same two tanks (the new one replaces the old). Clicking an existing line's two
  tanks again removes it. Arena Guardians can't take lines. Lines last for the match and disappear with dead tanks
  (they come back if the tank respawns).
- **Quick red lines** draws them in bulk: the bot you're watching, every bot, or the bots you made → you, the bot
  nearest you or the bot you're watching ("Nobody" removes their red lines).

**Bot cosmetics:** better bots have played longer, so they wear rarer things. The page sends 36 looks in four bands
(basic: pastel and base colors with no effects; common; rare; the rarest, with effects), and each bot picks by tier:
Newbie 55% wear anything, always basic; Rookie 60% (basic or common); Intermediate 70% (mostly common); Skilled 80%
(mostly rare); Pro 90% (rare or rarest); Elite always (mostly the rarest). Bots never wear the Legend set or admin
exclusives.

---

## 8. Maps and modes

### 8.1 Maps
Walls are set layouts, not random. Each match picks one map (the lobby says which):
- **Crossroads:** bunkers, avenues and short barriers, the same in every quarter.
- **Ruins:** four broken rings of wall around the middle with spokes between them, rubble in the corners.
- **Canyon:** long diagonal ridges with gaps, bunkers in the main valley and cross walls in the side valleys.
Each has its three temples on different sides (the storm ends at one of them) and its own temple shape. Rockets can still
blow holes in map walls (not temple stone).

### 8.2 Rocket Rampage
A 3-minute mode for **20 tanks**, spread out as far from each other as possible, in an arena covering nearly the whole
map, with four rings of extra cover walls. Bots roam to quiet spots far from everyone (and only mind the fog they're
in), so the fighting is spread out instead of piling into the middle. A **fog** closes in: the arena is 6,300 wide
(radius) until 0:45, then shrinks to 1,500 by 2:45 (20 damage per second outside). There is **no normal shooting**: everyone always has a **Rocket** with a 35%
cooldown (about 2 s), and a hit takes **50% of max health**. Tanks have **no barrel**: the Rocket itself sits on the
tank in the barrel's colors. Once fired it is gone and **its cooldown starts at once** (not when it lands); a new one
slides out of the body as it recharges until it is fully out (ready), and it launches from right there. The only upgrades are **Move Speed** and **Field of View**
(+8% view per tier, Rocket Rampage only). No superpowers, relics, crates, guardians or temples. Respawn after 3 s (bots
after 4 s), as far from everyone as possible inside the fog.
**Score:** 1 point per point of damage you deal, +50 per knockout. A scoreboard shows the top five. When time runs out
everyone is ranked by score. Rewards: Score/8 XP and Score/30 Credits, 30 XP + 10 Credits per kill, and placement
(1st 300 XP / 150 Cr, top 3 150 / 75, top 10 50 / 25). Rocket Rampage has its own stats; it doesn't count toward Battle
Royale achievements.

### 8.3 The Closing Ring (Battle Royale zone)

The arena starts as a huge square (14,000 × 14,000 units; crossing it takes about 40 s). A colored ring closes in phases. Standing outside it drains health.

| Phase | Starts at | Ring radius (end) | Shrink time | Damage/sec outside |
|---|---|---|---|---|
| 1 | 1:30 | 80% | 30 s | 2 |
| 2 | 3:00 | 55% | 30 s | 4 |
| 3 | 4:30 | 35% | 25 s | 7 |
| 4 | 6:00 | 20% | 25 s | 10 |
| 5 | 7:30 | 8% | 20 s | 15 |
| 6 | 8:30 | 0% | 40 s | 25 |

- The next ring is shown on the minimap before it starts.
- Ring damage is the same for everyone and can be survived with a Repair Kit or Shield.
- If two players are left at the end, a final tight ring forces the duel.

---

### 8.4 Sandbox
Picking **Sandbox** and pressing Play opens its setup (remembered in this browser):
- **Rules:** Battle Royale (guns, all loot, temples) or Rocket Rampage (Rockets only).
- **Map:** Crossroads, Ruins, Canyon, or Empty (no walls).
- **Play area:** a square around the middle, 2,000 to 14,000 across. Everything outside is out of bounds; loot, bots,
  respawns and the temples (moved in toward the middle) stay inside.
- **Fog:** none; the rules' own (the Battle Royale storm, scaled to the play area, or the Rampage fog); a circle that
  closes from a start radius to an end radius over a chosen time (starting 20 s in); or a fixed circle.
- **Bots** (0-39) and their **skill** (a normal mix, or all one tier, including the test tiers); **Loot** (none, a
  little, normal, lots; scaled to the play area); **Arena Temples** on or off; **Respawn** on or off.
A sandbox never ends on the clock (with respawn off, the last tank standing wins). In a sandbox, the dev panel's **Make a
bot** and the red and blue **lines** build your own fights, and **Sandbox setup…** changes the settings and starts again.

## 9. Legendary Path (Battle Pass)

**60 levels, fully free.** XP is earned from matches (see §10). Each level has a reward.

### XP curve
`XP from level L to L+1 = 200 + 15k + 0.8k²`, where k = L − 1
- Every level costs more than the one before, and the step itself grows a little each time.
- Level 1→2 needs 200 XP, 2→3 needs 216 XP, 59→60 needs 3,761 XP.
- Total to finish: **90,848 XP** (about 200 typical matches).

### Rewards
Everything on the Path is **exclusive to the Path**: it is never sold in the Shop or the Featured rotation.
Big cosmetics (wraps, trails, muzzle flashes, kill FX, death FX, barrels) are spaced out; the levels between them are
**filler**: Titles, Name Tag Colors, Credits and Arsenal Tokens.

**Cr** = Credits · **AT** = Arsenal Token · Rarity: C Common, U Uncommon, R Rare, E Epic, L Legendary

| Lvl | Reward | Lvl | Reward |
|---|---|---|---|
| 1 | Title: Newbie (C) | 31 | 70 Cr |
| 2 | 20 Cr | 32 | 2 AT |
| 3 | 1 AT | 33 | Wrap: Green (U) |
| 4 | 25 Cr | 34 | 75 Cr |
| 5 | Title: Rookie (C) | 35 | Trail: Sparkle Line (E) |
| 6 | Wrap: Red (U) | 36 | Title: Zone Runner (R) |
| 7 | 30 Cr | 37 | 80 Cr |
| 8 | Name Tag Color: Sky (C) | 38 | Name Tag Color: Lava (R) |
| 9 | 35 Cr | 39 | 2 AT |
| 10 | Trail: Dust Puff (U) | 40 | Wrap: Hazard Stripes (E) |
| 11 | 1 AT | 41 | 85 Cr |
| 12 | Title: Scrapper (C) | 42 | Title: Survivor (R) |
| 13 | 40 Cr | 43 | 90 Cr |
| 14 | Wrap: Orange (U) | 44 | Wrap: Blue (U) |
| 15 | Muzzle Flash: Spark (R) | 45 | Muzzle Flash: Lightning (E) |
| 16 | 45 Cr | 46 | 95 Cr |
| 17 | Name Tag Color: Mint (U) | 47 | 2 AT |
| 18 | 1 AT | 48 | Name Tag Color: Frost (E) |
| 19 | 50 Cr | 49 | 100 Cr |
| 20 | Barrel: Copper (R) | 50 | Death FX: Fireworks (E) |
| 21 | Title: Tank Ace (U) | 51 | Title: Veteran (E) |
| 22 | 55 Cr | 52 | 110 Cr |
| 23 | Wrap: Yellow (U) | 53 | Wrap: Purple (U) |
| 24 | 1 AT | 54 | 3 AT |
| 25 | Death FX: Confetti Pop (R) | 55 | Kill FX: Supernova (L) |
| 26 | 60 Cr | 56 | 120 Cr |
| 27 | Name Tag Color: Sunset (R) | 57 | Title: Warlord (E) |
| 28 | 65 Cr | 58 | Name Tag Color: Holo (L) |
| 29 | Title: Sharpshooter (R) | 59 | 150 Cr |
| 30 | Kill FX: Shockwave (E) | 60 | Wrap: Legend (L) + Trail: Legend Trail (L) + Title: Legend (L) |

**Totals:** 1,400 Credits and 13 Arsenal Tokens. Tooltips: Credits and Arsenal Tokens show the type "Currency" and no rarity; the Arsenal Token tooltip says "Use this in the Arsenal".

### Path rules
- Progress is account-wide and saved in Firebase.
- Completed levels' rewards are granted immediately and appear in the Locker.
- The Path is permanent. Seasons can be added later as separate Paths (optional).

---

## 10. Economy

### Match rewards
```
XP      = 20 × minutesSurvived
        + 50 × kills
        + 4  × shapesDestroyed
        + 10 × powerUpsUsed
        + placementBonus
Credits = 8  × minutesSurvived
        + 20 × kills
        + 2  × powerUpsUsed
        + placementBonus / 2
```
| Placement | XP bonus | Credit bonus |
|---|---|---|
| Win (1st) | +500 | +250 |
| Top 3 | +250 | +125 |
| Top 10 | +100 | +50 |
| 11th–20th | +40 | +20 |
| Other | +10 | +5 |

**Streak multiplier** (kills in one life without dying): 3 kills ×1.10, 5 kills ×1.20, 8 kills ×1.30 (cap), applied to the kill portion only. **Daily first-win bonus:** +200 XP, +100 Credits.

Typical match: about 300–600 XP, 100–200 Credits; a win about 1,000+ XP and 400+ Credits.
**Arena Temples:** +30 XP per temple guardian beaten, +150 XP and +75 Credits for the Arena Mage.
Rocket Rampage has its own formula (section 8.2).

### Credits shop
- Credits are the **only** currency. No real-money purchases at launch.
- **Featured tab:** 4 Featured items that refresh every 3 hours (UTC-aligned) and 2 Deals. The Featured pool has 33 items.
  - Featured items come from a pool that is **never** in the normal shop or on the Path. Lower rarities are more likely
    (weights: Uncommon 40, Rare 30, Epic 20, Legendary 10). They cost more than shop items of the same rarity:
    Uncommon 700, Rare 1,200, Epic 2,200, Legendary 4,500 Credits.
  - Deals put a discount on a normal shop item. Discounts are 5–40% in steps of 5, with 20% the most common; lower
    discounts get slowly rarer and big ones get rare fast (weights 5%:7, 10%:11, 15%:15, 20%:19, 25%:16, 30%:10, 35%:5, 40%:2).
- **All tab:** every item you can buy any time, with a side tab per category (Wraps, Barrels, Trails, Muzzle Flashes,
  Kill FX, Death FX, Name Tag Colors) and a filter for price or rarity, low to high or high to low.
- Clicking an item opens a detail panel inside the Shop/Locker: a live preview on the arena grid, rarity, name, type,
  collection, description, and Buy/Equip. Every item, in the Shop, Locker and Path, has a hover tooltip.
- **Only the Shop sells things.** The Locker never shows a Buy button.

| Rarity | Shop price |
|---|---|
| Common | 200–250 Cr |
| Uncommon | 400–450 Cr |
| Rare | 700–800 Cr |
| Epic | 1,400–1,800 Cr |
| Legendary | 3,000–3,500 Cr |

Weapon variants can be bought with **Credits (1,500 Cr)** or **Arsenal Tokens (1–3 AT)** once the Arsenal exists.

---

## 11. Cosmetics Locker

All cosmetics are **visual only**. They never change hitboxes, stats, or visibility fairness.

| Type | What it is | Examples |
|---|---|---|
| Wraps | The design of your round body | Base Color Palette (Path), Pastel Pack, stripes, checker, camo, gradients, metals, animated (Prism, Nebula, Neon Pulse, Starfield) |
| Barrels | Barrel designs | Slate, Ivory, Charcoal, Carbon Fiber, Caution Tape, Chrome, Gold, Prism |
| Trails | Left behind your tank as it moves | Dust Puff, Bubbles, Mist, Ember, Plasma, Rainbow Ribbon, Comet, Snowfall, Party Trail |
| Kill FX | Plays on a tank you eliminate | Shockwave, Star Pop, Coin Shower, Lightning Strike, Boo!, Vortex, Supernova |
| Titles | Small text above your name | From the Path (Newbie, Rookie, ... Legend) and achievements (First Blood, Champion, ...) |
| Muzzle Flashes | Plays when you shoot | Spark, Flare, Ring Pop, Star Burst, Lightning, Plasma Ring, Confetti Cannon, Rainbow Burst |
| Death FX | Plays when you are eliminated | Confetti Pop, Streamers, Pixel Burst, Shatter, Fireworks, Royal Burst, Over the Rainbow |
| Name Tag Colors | Color or effect on your name | Sky, Ruby, Emerald, Sunset, Lava (flowing), Neon Glow, Gold Leaf, Holo (animated) |

Every item has a rarity, a collection and a description. Titles are never sold: they come from the Path or achievements.

**Locker view:** "Hide locked" is on by default, so you only see what you own. With it off, locked items are shown
greyed out with a lock and **no hint of where they come from** (no price, Path level or source); that lives in the Shop.
A **Collections** tab lists every collection with its progress. Clicking a collection (there, or the collection name in
any item's details, in the Shop or the Locker) shows every item in it, the ones you own first.

**Rainbow collection:** Prism, Rainbow Road (wraps), Prism Barrel, Rainbow Barrel, Rainbow Ribbon (trail), Rainbow Burst
(muzzle flash), Supernova (kill FX), Over the Rainbow (death FX), Holo and Rainbow (name tags) and the Prismatic title.

### 11.1 Achievements
Each achievement is named after the **title** it gives you; some also give Credits or Arsenal Tokens. The Achievements
screen shows each one's description, reward and either "Unlocked" (with a button to wear the title) or a progress bar.
Only Battle Royale and Rocket Rampage matches without dev tools count.

| Title | How | Extra reward |
|---|---|---|
| First Blood | Get your first kill | 50 Cr |
| Regular | Play 25 matches | 200 Cr |
| Contender | Finish top 10 in 10 matches | 200 Cr |
| Champion | Win a Battle Royale | 300 Cr |
| Marathoner | Survive 8 minutes in one match | 150 Cr |
| Exterminator | 8 kills in one match | 250 Cr + 1 AT |
| Hoarder | Collect 500 upgrades | 250 Cr |
| Power Hungry | Use 150 superpowers | 250 Cr |
| Crate Cracker | Break open 50 crates | 200 Cr |
| Treasure Hunter | Break open a Legendary Crate | 300 Cr |
| Giant Slayer | Beat 3 Arena Mages | 400 Cr + 1 AT |
| Guardian Bane | Defeat 25 Temple Guardians | 250 Cr |
| Temple Raider | Beat the Arena Mage | 500 Cr + 1 AT |
| Rocketeer | Win a Rocket Rampage | 300 Cr |
| Demolition Expert | Score 1,500 in one Rocket Rampage | 200 Cr + 1 AT |
| Centurion | 100 kills in total | 500 Cr + 2 AT |
| Pathfinder | Reach Legendary Path level 30 | 300 Cr |
| Collector | Own 20 cosmetics | - |
| Big Spender | Spend 10,000 Credits in the Shop | 2 AT |
| Dynasty | Win 10 Battle Royales | 1,000 Cr + 3 AT |
| Prismatic | Own every item in the Rainbow collection | - |
Bots wear random cosmetics so players see items in matches; names, titles and the kill feed match what bots wear.

**Rules:** one item per slot equipped at a time; the Locker lets you equip/unequip freely, preview in 3D-style turntable, and save **3 loadout presets**. Enemy-readability rule: an *enemy's* paint is shown in a tone that never matches the ring or the arena color enough to hide the tank.

---

## 12. Progression Safeguards (anti-snowball)

- **No permanent power from XP.** Levels only unlock choices and cosmetics.
- **Bounty:** the player with the most kills gets a visible crown/ping on the minimap that updates every 30 s, so leaders are always hunted fairly.
- **Comeback crates:** low-placing players get a slightly better crate roll.
- **Streaks** raise rewards but never stats.
- **Fair matchmaking:** loose skill-based lobbies, with new accounts placed in lobbies with bots at first.
- **Anti-cheat:** server-authoritative, input rate limits, sanity checks on speed and fire rate, and a report system.

---

## 13. Firebase Data Model & Security

### Firestore collections
```
users/{uid}
  displayName, createdAt, level (Path), xp, credits, arsenalTokens,
  equipped { paint, barrel, muzzle, trail, deathFx, killFx, banner, emote, nameTag, avatar },
  equippedWeapon, stats { matches, wins, kills, timePlayed }

users/{uid}/inventory/{itemId}      owned cosmetics   { itemId, acquiredAt, source }
users/{uid}/arsenal/{weaponId}      { unlocked, masteryXp, masteryLevel }
users/{uid}/loadouts/{n}            saved presets

catalog/{itemId}                    { name, category, rarity, price, shopSlot }
legendaryPath/{level}               { xpRequired, reward }
shop/current                        { featured[], weekly[], rotatesAt }
matches/{matchId}                   { startedAt, results[] }   (server-written)
```

### Rules (summary)
- Users may **read** their own `users/{uid}` and subcollections, plus `catalog`, `legendaryPath`, and `shop`.
- Users may **write only** harmless fields (e.g., `displayName`, `equipped`, `equippedWeapon`, `loadouts`), validated against owned inventory.
- Users may **never write** `credits`, `xp`, `level`, `arsenalTokens`, inventory, or arsenal progress. Those are written only by the **trusted server** or **Cloud Functions** (service account).
- **Purchases** go through a Cloud Function that checks price and balance in a transaction.
- **Match results** are written by the game server once per match, idempotent by `matchId`, so rewards cannot double-apply.

### Join flow
1. Player signs in (Firebase Auth) → gets an ID token.
2. Client connects to a game server and sends the token.
3. The server verifies the token and loads the loadout (weapon + cosmetics, via a trusted read).
4. During the match the server tracks everything.
5. At the end the server computes XP/Credits and writes them to Firestore.

---

## 13a. Admin account
One name, **FitCocoIvy**, is the admin account. Every time it presses Play (or Enter) the game asks for its passcode;
a wrong one says so, and the player can try again or play without admin. With the right one:
- **Exclusives** are given the first time (and kept): ten Legendary cosmetics in the **Coco Ivy** set (two wraps, a
  barrel, a trail, a muzzle flash, kill and death FX, the titles **Head Admin** and **The Coconut**, and the **Ivy Glow**
  name tag), and two superpowers nobody else can get: **Coconut Barrage** (ten coconuts rain down in a fan toward the
  cursor, 24 damage each, 6 s cooldown) and **Ivy Snare** (vines root everyone near the cursor for 2.5 s, then slow them;
  7 s cooldown). The admin starts every Battle Royale holding Coconut Barrage. The exclusives are hidden from the Locker
  (and taken off) whenever anyone else plays, never drop when the admin is knocked out, and can't be given from the dev
  panel or worn by bots.
- **` opens the admin panel** (Shift+` or the dev tools link still opens the dev panel): Giant / Tiny / Normal size,
  Max me, Overdrive (x2 damage, fire rate, speed and range), Bullet Storm (12 bullets all around), Bouncy Bullets (25
  bounces), the two exclusive superpowers, Meteor Shower, Black Hole (a huge Singularity), Shockwave, Nuke (can't hurt the
  admin), Loot Rain, Disco (everyone else goes rainbow), Freeze everyone (5 s), Come here! (every bot appears around you),
  Swap with a bot, Minion Army (four Sentry Robots for 60 s), Everyone's a Newbie, and Total chaos (every bot becomes
  Omnipotent).
The passcode is checked in the browser (only a hash is in the page), so it only keeps out casual players. Once there's a
server, the admin flag must come from the account on the server instead.

## 14. Roadmap

| Phase | Goal |
|---|---|
| **0 — Prototype** | Native C++ window: one tank, shapes, shooting, collision. Shared `core/` library. |
| **1 — Web MVP** | Emscripten client in browser, C++ server, 10–20 players, one weapon, ring, basic HUD. |
| **2 — Alpha** | All 8 archetypes, power-ups, crates, full ring, bots, scoreboard, results screen. Firebase Auth. |
| **3 — Meta** | Credits, XP, Legendary Path (60 levels), Locker, Shop, Arsenal, saved progress. |
| **4 — Polish** | Art/audio pass, performance, matchmaking, anti-cheat, onboarding. |
| **5 — Launch** | Public servers, analytics, balance patching. |
| **6 — Later** | Duos, mobile touch controls, seasons, events. |

---

## 15. Open Questions

- Server hosting provider and region plan.
- Final tick rate (30 vs 40 Hz) after network testing.
- Exact numbers for all tuning values (damage, speeds, health). These will be tuned in playtests.
- Duos design: revive, shared kills.
- Mobile controls and aim assist.
- Whether to add optional supporter packs later (kept strictly cosmetic).
- Config: Firebase project config will be pasted when ready.
