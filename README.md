# Tonk.io

A free, browser-based multiplayer-style **tank battle royale**. Round cartoon tanks fight on a huge arena, grab upgrades,
superpowers and relics, and try to be the last tank standing while a storm closes in. Bots fill the lobby. The game is
C++17 compiled to WebAssembly (WebGL2) with a plain HTML/JS front end. Full design: [GAME_DESIGN.md](GAME_DESIGN.md).

## Quick start (just play)
The compiled game is already in `web/`, so you only need a static web server. You don't need to build anything.

1. Install [Python 3](https://www.python.org/downloads/) (or any static file server).
2. From this folder run:
   ```bash
   python -m http.server 8000 --directory web
   ```
3. Open **http://localhost:8000** in a desktop browser (Chrome, Edge or Firefox with WebGL2).

Opening `web/index.html` directly from disk will not work: browsers refuse to load WebAssembly from `file://`.

## How to play
1. Type a name and press **Play** on the home screen. Pick **Battle Royale** (about 8-10 minutes, up to 40 tanks),
   **Rocket Rampage** (3 minutes, rockets only, highest score wins) or **Sandbox** (your own rules, bots and map).
2. Drive, aim and shoot:

   | Input | Action |
   |---|---|
   | `W A S D` | Move (tanks have momentum and skid a little) |
   | Mouse | Aim (also the target for Teleport, Air Strike, Rocket steering and more) |
   | Left click (hold) | Shoot |
   | `E` / `Shift` | Use your superpower |
   | `R` / `T` | Use your oldest / newer relic |
   | `F` | Swap your superpower or oldest relic for one on the ground |
   | `M` | Mute sound |
   | `` ` `` (backtick) | Dev panel |

3. **Loot.** Drive over upgrade circles, superpower squares and relic hexagons, and shoot crates open. Loot is better near
   the middle of the map. Upgrades only help when they beat your current tier.
4. **Stay in the safe zone.** The storm closes in six phases and hurts more each time. The next safe zone is on the minimap.
5. **Fight smart.** No weapon is best at everything. Bullets bounce off walls (up to 4 times), rockets blow holes in walls,
   and the tank you knock out drops everything it carried and heals you for 25 HP.
6. **Arena Temples** (three per map) hold a dark spiral of guardians and the Arena Mage, who drops the best rewards.
7. Win matches to earn Credits and Legendary Path XP for cosmetics in the Shop, Locker and Path (all saved in your browser).

Superpowers, relics and temples are covered in GAME_DESIGN.md sections 7 and 8.

### Trying the preset bots
Press `` ` `` to open the dev panel, go to **BOTS**, and use **Presets** to spawn Aegis-Vulture, Ghost Current and Carrion
Clock, three hand-made bots with the same power as every other bot, then watch them fight (use a Sandbox with respawn off).

## Build from source (optional)
Only needed if you change the C++ in `core/` or `client/`.

**Prerequisites:** CMake 3.20+, Ninja, a C++17 compiler (MinGW, GCC, Clang or MSVC), and the
[Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) for the web client.

```bash
# Tests and the server skeleton (native)
cmake -B build -G Ninja && cmake --build build
ctest --test-dir build --output-on-failure

# Web client: writes web/tonk_client.js and web/tonk_client.wasm
emcmake cmake -B build-web -G Ninja && cmake --build build-web
python -m http.server 8000 --directory web
```
On Windows, `.\scriptsuild.ps1 [native|web|all]` does the same (set `$env:EMSDK` to your emsdk folder first).
The native tests run whole 20-bot matches, so they take a few minutes.

## Project layout
- `core/` shared C++ (movement, weapons, ring, bots; `bot_presets.h` holds the three preset bots). Used by client and server.
- `server/` authoritative game server (skeleton).
- `client/main.cpp` Emscripten + WebGL2 renderer: fixed world-area view scaling, interpolation, effects, cosmetics, kill FX.
- `web/` the page: `index.html` (markup), `style.css`, `ui.js` (lobby, HUD and kill feed, death screen, Shop/Locker/Path
  screens, tooltips, dev panel), `meta.js` (item catalog, profile, Credits, Legendary Path, shop rotation, achievements,
  and the preview engine that mirrors the game's effects). `web/icons/` the stat and superpower icon art (the HUD uses
  the PNGs; the game draws from `atlas.png`).
- `tools/make_icons.py` rebuilds `web/icons/` from the source art (1024x1024 PNGs on black): cuts out the background,
  crops and sizes every icon the same, colors the stat icons, and evens out Lightning (needs Pillow).
- `firebase/` rules and setup notes. `tests/` dependency-free core tests.

## Profile, Shop, Locker, Legendary Path
Until Firebase is connected, the profile (Credits, Path XP, owned and equipped items) is saved in the browser's
localStorage under `tonk.profile.v2` (Credits, XP, name and stats carry over from `v1`). Credits and Path XP come only from
**Battle Royale** and **Rocket Rampage** matches with no dev tools used; Sandbox gives no rewards. Battle Royale has no
respawns. Rocket Rampage is 3 minutes of Rockets-only, highest score wins. Each match picks one of three set maps
(Crossroads, Ruins, Canyon) with three Arena Temples each.

- Cosmetic types: Wraps, Barrels, Trails, Muzzle Flashes, Kill FX, Death FX, Titles, Name Tag Colors.
- Shop: **Featured** (3 exclusive items every 3 hours + 2 Deals at 5–40% off) and **All** (side tabs per type, price/rarity filter).
- Legendary Path: 60 levels, every reward exclusive to the Path.
- The catalog, Path, rotation rules and reward formula are in `web/meta.js` and GAME_DESIGN.md sections 9 to 11.

When Firebase is added, the server must write Credits/XP/inventory and purchases must go through a Cloud Function; the
page then only reads them.

## Controls
WASD move · mouse aim/shoot · E/Shift superpower · R/T relics (oldest/newer) · F swap superpower or oldest relic.
Superpowers, relics, upgrade tiers, crates, Arena Guardians and Arena Temples are described in GAME_DESIGN.md section 7;
maps and Rocket Rampage in section 8.

## Dev panel
Press the backtick key (`) or click **dev tools** on the home screen:
- **Time speed**: ❚❚ pauses, ¼× and ½× are slow motion (effects, animations, sounds and bots all slow down or stop),
  5×/20×/60× run the whole sim faster; **Next ring event** and **+60 s** jump the match clock.
- **Put me outside** drops you outside the safe circle; **Go to middle** puts you in the middle of the map; **God mode** stops you dying;
  **Ghost** makes bots, guardians and turrets ignore you (you fade out a little).
- **Legendary Crate in 10 s** announces one now; **Go to temple** / **Go to crate** put you next to the nearest one (click **Go to temple** again to step in).
- **Give** any superpower (including Mythics) or relic.
- **Bots** + **Restart** change the lobby size (0–39); **respawn** turns the match into a sandbox.
- **Profile**: +1000 Credits, +2500 Path XP, or reset the saved profile.
- The panel has two pages: **MAIN** (everything above) and **BOTS**:
  - **Bot Info Overlay** puts a card under every bot (tier and type, objective and target, health, aggression,
    dodging, loadout, upgrades, affinities); **State dots** draws a colored dot and a line to its target; **Watch bot ▶**
    follows a bot.
  - **Lines**: pick **Red: targets** or **Blue: teams**, then click a bot in the arena and click (or drag to) other
    tanks. A red line sends the bot after that tank (a bot can have several; it takes the nearest first). A blue line
    teams two tanks up, both ways: they never attack or hurt each other. Clicking the same pair again removes the line;
    Esc or clicking the ground lets go of the picked bot. While the tool is on, the mouse doesn't shoot. **❚❚** pauses so
    you can draw on still bots; **Clear** buttons remove the picked bot's lines, the red ones, the blue ones or all.
    **Quick red lines** draws them in bulk (the bot you're watching, every bot, or your bots → you, the bot nearest you
    or the watched bot).
  - **Presets** spawns one of three hand-made bots (Aegis-Vulture, Ghost Current, Carrion Clock) or all three; they have
    the same power as every other bot.
  - **Make a bot** spawns one in front of you with the skill tier (including the Unreal, Omniscient and Omnipotent test
    bots), type, aggression, aim, dodging, power, upgrades, superpower and relics you pick; **Remove my bots** takes
    them out.
- **Sandbox setup…** opens the Sandbox settings (rules, map, play-area size, fog, bots and their skill, loot, temples,
  respawn) and starts a new sandbox. Picking Sandbox on the home screen opens the same setup.
- The admin account has its own panel on the backtick key (Shift+` for this one); see GAME_DESIGN.md section 13a.

Using match tools (speed, ring skips, god mode, give, go to middle, restart) turns off rewards for that match.
The match keeps running in a background tab (a worker timer steps it when the page stops drawing frames).
Sound effects are synthesized in `web/sfx.js` (no audio files); browsers only start audio after your first click or key
press. **M** or the speaker button in the top-left corner mutes them, and the choice is saved in this browser.
Bot AI and the Arena Guardian brain live in `core/bot.h`; `tests/core_tests.cpp` covers movement, upgrade tiers, every
superpower, relics, drops, guardians, bouncing bullets, crates, the Legendary Crate, maps, Arena Temples, the Sentry
Robot, Rocket Rampage and a 20-bot match.
