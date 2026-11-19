// Tonk.io browser client (Emscripten + WebGL2).
// Runs the shared core world locally with bots. The visible world area is the same on every window size, shapes have
// a darker outline, and things fade out when they die. Palette: warm paper arena, coral enemies, violet storm zone.
// The sim ticks at 30 Hz; the renderer interpolates between ticks so motion is smooth at any frame rate.
// TODO next: network (WebSocket), prediction/reconciliation.
#include <GLES3/gl3.h>
#include <emscripten.h>
#include <emscripten/html5.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/world.h"

using namespace tonk;

// ---------------- View ----------------
// Everyone sees the same world area: the view is scaled to cover kViewW x kViewH world units.
constexpr float kViewW = 1760.0f, kViewH = 990.0f;
static float g_zoom = 1.0f;  // CSS px per world unit
static float g_ui = 1.0f;    // HUD scale, same formula as the page (web/ui.js)

static EMSCRIPTEN_WEBGL_CONTEXT_HANDLE g_ctx;
static GLuint g_bgProg, g_instProg, g_instVbo, g_instVao;
static GLint uRes, uCam, uDpr, uZoom, uRing, uNext, uRingOn, uTemple, uStyle, uRings, uRingN, uThick, uBounds, iRes, iCam, iTime;
static GLuint g_fogProg;
static GLint fRes, fCam, fDpr, fZoom, fTemple, fLight, fRingA, fRingN, fThick, fBlock, fBlockN;
static float g_dpr = 1.0f;
static bool g_menu = true;          // home screen: the match runs behind it with nobody at the controls
static bool g_godOpt = false;       // test-panel god mode (re-applied when a match starts)
static bool keyW, keyA, keyS, keyD, keyR, keyT, mouseDown;
static bool shootQueued;  // latched on press so a tap shorter than one frame still fires
static bool useQueued;    // same for the power-up keys (E / Shift)
static bool swapQueued;   // F
static float mouseX, mouseY;
static int canvasW = 1280, canvasH = 720;
static World g_world;
static uint32_t g_spectateId = 0;   // 0 = follow the player
static bool g_debugBots = false;    // draw each bot's state + target (test panel)
static bool g_botInfo = false;      // dev panel: Bot Info Overlay (objective, target, loadout, tier, affinities)
static bool g_admin = false;        // the admin account is signed in (the page checks the passcode)
static bool g_disco = false;        // admin Disco: every other tank goes rainbow
static bool g_ghostOpt = false;     // dev panel: bots don't notice you
static Vec2 g_cursor;               // the world point under the mouse
// Dev panel lines, drawn with the mouse: 0 off, 1 red (targets), 2 blue (teams). g_linkSel is the tank new lines start
// from; g_linkPress the tank the mouse went down on (a drag from it to another tank draws a line).
static int g_linkMode = 0;
static bool g_linkShow = true, g_linkDown = false;
static uint32_t g_linkSel = 0, g_linkPress = 0;
static void linkClick(bool down);
static void linkNote(int code);
// Sandbox settings (the page's Sandbox setup). Used by the next match when g_sandOn.
struct SandboxCfg { int mode = 0, map = 0, size = 14000, fog = 1, fogR0 = 5000, fogR1 = 1200, fogTime = 120, bots = 15, tier = -1;
                    bool respawn = true, temples = true; float loot = 1.0f; };
static SandboxCfg g_sand;
static bool g_sandOn = false;
static uint32_t g_nextDevBot = 5001;  // bots made in the dev panel get ids from 5001
static float g_timeScale = 1.0f;    // test panel: run the sim faster
static int g_botCount = 39;
static bool g_respawn = false;
static float g_alpha = 0.0f;        // interpolation between the previous and current tick
static float g_clock = 0.0f;        // real seconds, for animated paints

// Player match counters for the post-match rewards.
static int g_myUpgrades = 0, g_myPowerUses = 0, g_myCrates = 0, g_myLegend = 0, g_myMobs = 0, g_myMages = 0;
static bool g_resultSent = false;
static int g_mode = 0;              // 0 Battle Royale, 1 Rocket Rampage (sandbox is Battle Royale with respawns)
static bool g_legendMark = false;   // a Legendary Crate is coming / waiting: red X on the minimap
static Vec2 g_legendPos;
struct BoxSeen { float hp = -1, flash = 0; };            // crates flash when hit
static std::unordered_map<uint32_t, BoxSeen> g_boxSeen;

static float viewW() { return canvasW / g_zoom; }
static float viewH() { return canvasH / g_zoom; }

// ---------------- Colors and cosmetics ----------------
struct Col { float r, g, b; };
static Col hexCol(int h) { return {((h >> 16) & 255) / 255.0f, ((h >> 8) & 255) / 255.0f, (h & 255) / 255.0f}; }
static Col mixC(Col a, Col b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
static Col hueC(float h) {
  h = h - std::floor(h);
  auto f = [&](float o) { float k = std::fmod(h * 6.0f + o, 6.0f); return std::clamp(std::fabs(k - 3.0f) - 1.0f, 0.0f, 1.0f); };
  return {f(0), f(4), f(2)};
}

// What a tank looks like. Patterns are drawn by the instanced shader (see paint() in kInstFrag).
// 0 solid, 1 stripes, 2 checker, 3 camo, 4 gradient, 5 chrome, 6 rainbow, 7 swirl, 8 pulse, 9 dots, 10 rings, 11 split,
// 12 galaxy. Must match fillPattern() in web/meta.js.
static const Col kEnemyCol = hexCol(0xFF7A5C);   // tanks without a wrap are coral
static const Col kBarrelCol = hexCol(0xA3A7B8);  // default slate barrel
struct Look {
  int pat = 0;
  Col c1 = hexCol(0x2EC4B6), c2{1, 1, 1};
  int bpat = 0;
  Col b1 = kBarrelCol, b2{1, 1, 1};
  int trail = 0, death = 0, muzzle = 0, kill = 0;
};
static Look g_myLook;                          // set by the page from the equipped Locker items
static std::vector<Look> g_lookPool;           // cosmetics some bots wear, so the shop items show up in matches
static std::unordered_map<uint32_t, int> g_lookSlot;  // bot id -> index into g_lookPool (the page mirrors this)

static Look guardianLook() {
  Look l;
  l.pat = 10; l.c1 = hexCol(0x4B2E83); l.c2 = hexCol(0x5F3DA3);
  l.bpat = 5; l.b1 = hexCol(0xF5B82E);
  return l;
}
static const Col kGuardBullet = hexCol(0x3A2466);
constexpr float kBulletGrow = 0.35f;  // at the end of its range a bullet is 35% bigger
// A bullet stays solid for most of its flight, then fades out completely as it flies the last 35% of its range.
static float bulletAlpha(float k) { const float t = std::clamp((k - 0.65f) / 0.35f, 0.0f, 1.0f); return 1.0f - t * t * (3.0f - 2.0f * t); }

static Look mageLook() {
  Look l;
  l.pat = 7; l.c1 = hexCol(0x2A1650); l.c2 = hexCol(0x8A5CFF);
  l.bpat = 5; l.b1 = hexCol(0xFFD23F);
  return l;
}
static Col greyC(Col c) { const float g = 0.3f * c.r + 0.59f * c.g + 0.11f * c.b; return mixC(c, {g, g, g}, 0.85f); }
static Look greyLook(Look l) {  // a Sentry Robot: its owner's look, mostly drained of color
  l.c1 = greyC(l.c1); l.c2 = greyC(l.c2); l.b1 = greyC(l.b1); l.b2 = greyC(l.b2);
  if (l.pat == 6 || l.pat == 13) l.pat = 0;
  l.trail = l.death = l.muzzle = l.kill = 0;
  return l;
}

static Look lookOf(const Tank& t) {
  if (t.id == 1) return g_myLook;
  if (g_disco && !t.guardian) {  // admin Disco: everyone else in rainbow
    Look l;
    l.pat = 6; l.c1 = hexCol(0xFF8FC7); l.bpat = 13; l.b1 = hexCol(0xFF5A5F); l.trail = 3; l.muzzle = 8;
    return l;
  }
  if (t.sentryOf) {
    const Tank* o = nullptr;
    for (const Tank& k : g_world.tanks) if (k.id == t.sentryOf) o = &k;
    return greyLook(o && !o->sentryOf ? lookOf(*o) : Look{});
  }
  if (t.guardian && t.temple >= 0) return mageLook();
  if (t.guardian || t.mob) return guardianLook();
  auto it = g_lookSlot.find(t.id);
  if (it != g_lookSlot.end() && it->second < (int)g_lookPool.size()) return g_lookPool[it->second];
  Look l;
  l.c1 = kEnemyCol;
  return l;
}

// Superpower and relic colors (same as web/ui.js PU / RELICS).
static Col powerColor(Power p) {
  static const int k[] = {0xFFFFFF, 0xFFA833, 0x59C7FA, 0xFF5A5F, 0xFF4FD8, 0xFF6B82, 0x5B7CFF, 0x2EC4B6,
                          0xFFD23F, 0x8FE66E, 0x8C8FA3, 0xA45CFF, 0x7A86B8, 0x7C54F2, 0xFFB31F, 0x9AA3B5, 0xC68B4E, 0x3FAE5A};
  return hexCol(k[std::clamp((int)p, 0, (int)Power::Count - 1)]);
}
static Col relicColor(Relic r) {
  static const int k[] = {0xFFFFFF, 0x5CD166, 0xFF8A3D, 0xFFC93C, 0x3FA9F5};
  return hexCol(k[std::clamp((int)r, 0, (int)Relic::Count - 1)]);
}
// Upgrade colors by rarity: a thin rim in the rarity color, a light fill and the icon in a deeper shade.
struct UpCols { Col main, dark, fill; };
static UpCols upgradeCols(int r) {
  static const int k[5][2] = {{0x9EA4B3, 0x6B7080}, {0x4CC94C, 0x23852A}, {0x3FA9F5, 0x1B62B5}, {0xA45CFF, 0x6A2FC2}, {0xFFB31F, 0xC07800}};
  const int i = std::clamp(r, 0, 4);
  const Col m = hexCol(k[i][0]);
  return {m, hexCol(k[i][1]), mixC(m, {1, 1, 1}, 0.8f)};
}
static const Col kPowRed = hexCol(0xF9C5C1), kPowDark = hexCol(0xE6322C);  // superpowers on the map: light red, red (as the icon art)

// Rarity colors (same as web/meta.js RCOL): Common, Uncommon, Rare, Epic, Legendary.
static Col rarityCol(int r) {
  static const int k[5] = {0xB4B9C6, 0x5CD65C, 0x3FA9F5, 0xA45CFF, 0xFFB31F};
  return hexCol(k[std::clamp(r, 0, 4)]);
}

static int colHex(Col c) {
  return ((int)(c.r * 255.0f + 0.5f) << 16) | ((int)(c.g * 255.0f + 0.5f) << 8) | (int)(c.b * 255.0f + 0.5f);
}

// ---------------- Instances ----------------
// One instanced primitive. sides: -5 rounded rectangle, -4 five-point star, -3 ring (half-thickness radius*aspect),
// -2 capsule, -1 rectangle (half-size radius x radius*aspect), 0 circle, >=3 polygon.
struct Inst {
  float x, y;
  float radius, rot, sides, flash;
  float aspect, alpha, pat, ol;  // ol = outline width in world units (0 = none)
  float r, g, b;
  float r2, g2, b2;
};
static std::vector<Inst> g_insts;

static Inst& add(float x, float y, float R, float rot, float sides, Col c) {
  g_insts.push_back(Inst{x, y, R, rot, sides, 0, 1, 1, 0, 3.5f, c.r, c.g, c.b, 1, 1, 1});
  return g_insts.back();
}
static Inst& circle(float x, float y, float R, Col c) { return add(x, y, R, 0, 0, c); }
static Inst& blob(float x, float y, float R, Col c, float alpha) {  // translucent, no outline
  Inst& i = add(x, y, R, 0, 0, c);
  i.alpha = alpha; i.ol = 0;
  return i;
}
static Inst& rectI(float cx, float cy, float hw, float hh, float rot, Col c) {
  Inst& i = add(cx, cy, hw, rot, -1, c);
  i.aspect = hh / hw;
  return i;
}
static Inst& capsule(float cx, float cy, float hw, float hh, float rot, Col c) {
  Inst& i = add(cx, cy, std::max(hw, hh), rot, -2, c);
  i.aspect = hh / std::max(hw, hh);
  return i;
}
static Inst& poly(float x, float y, float R, float rot, int n, Col c) { return add(x, y, R, rot, (float)n, c); }
static Inst& rrect(float cx, float cy, float hw, float hh, float rot, Col c) {  // corners rounded by 30% of the short side
  Inst& i = add(cx, cy, hw, rot, -5, c);
  i.aspect = hh / hw;
  return i;
}
static Inst& ringI(float x, float y, float R, float halfThick, Col c) {
  Inst& i = add(x, y, R, 0, -3, c);
  i.aspect = halfThick / R; i.ol = 0;
  return i;
}
static void paint(Inst& i, int pat, Col c2) { i.pat = (float)pat; i.r2 = c2.r; i.g2 = c2.g; i.b2 = c2.b; }
// Icons drawn from art (web/icons/atlas.png, built by tools/make_icons.py), once it has loaded. Stat cells are masks
// colored here: rim c, light fill c2, the icon itself a deeper shade of c. Superpower cells are in their own colors.
static bool g_iconsReady = false;
static int statIconCell(int stat) { static const int k[7] = {0, 1, 2, -1, 3, 4, 16}; return stat >= 0 && stat < 7 ? k[stat] : -1; }
static int powerIconCell(int p) {
  static const int k[13] = {-1, 5, 6, 7, 8, 9, 10, 11, 12, 13, -1, 14, 15};
  return p >= 0 && p < 13 ? k[p] : -1;
}
static Inst& sprite(float x, float y, float R, int cell, bool tinted, Col c, Col c2) {  // R: half the icon's size
  Inst& i = add(x, y, R * 128.0f / 120.0f, 0, -6, c);  // (each 128 px cell has 4 px of padding around the art)
  i.ol = 0;
  paint(i, tinted ? 100 + cell : cell, c2);
  return i;
}

static bool inView(Vec2 p, Vec2 cam, float margin) {
  return std::fabs(p.x - cam.x) <= viewW() * 0.5f + margin && std::fabs(p.y - cam.y) <= viewH() * 0.5f + margin;
}

// ---------------- Shaders ----------------
static const char* kBgVert = R"(#version 300 es
void main() {
  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);  // fullscreen triangle
  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
})";

// Arena colors follow GAME_DESIGN.md section 3: warm paper ground, soft grid, violet storm. Line widths are in px.
static const char* kBgFrag = R"(#version 300 es
precision highp float;
uniform vec2 uRes; uniform vec2 uCam; uniform float uDpr; uniform float uZoom;
uniform vec3 uRing; uniform vec3 uNext; uniform float uRingOn;   // safe circle (x,y,r) and where it is heading
uniform vec3 uTemple; uniform float uStyle;                      // inside an Arena Temple: its middle and outer radius
uniform vec4 uRings; uniform float uRingN; uniform float uThick; // its wall radii (outer to inner) and wall thickness
uniform vec4 uBounds;                                            // the play area: lo.x, lo.y, hi.x, hi.y
out vec4 o;
void main() {
  vec2 world = uCam + (gl_FragCoord.xy / uDpr - 0.5 * uRes) * vec2(1.0, -1.0) / uZoom;
  if (uTemple.z > 0.5) {  // dark stone floor, a rune circle in the middle, torch-lit around you
    vec3 base = uStyle < 0.5 ? vec3(0.17, 0.14, 0.2) : (uStyle < 1.5 ? vec3(0.12, 0.17, 0.16) : vec3(0.19, 0.12, 0.14));
    vec3 accent = uStyle < 0.5 ? vec3(0.95, 0.72, 0.3) : (uStyle < 1.5 ? vec3(0.35, 0.9, 0.7) : vec3(1.0, 0.42, 0.35));
    vec2 rel = world - uTemple.xy;
    float dr = length(rel);
    vec2 t = world / 110.0 + vec2(floor(world.y / 110.0) * 0.5, 0.0);
    vec2 f = abs(fract(t) - 0.5) * 110.0 * uZoom;
    float h = fract(sin(dot(floor(t), vec2(12.9898, 78.233))) * 43758.5453);
    vec3 col = base * (0.85 + 0.3 * h);
    col = mix(col, base * 0.55, 1.0 - smoothstep(0.8, 2.2, min(f.x, f.y)));
    float rune = 1.0 - smoothstep(1.0, 2.6, abs(dr - 420.0) * uZoom);
    rune = max(rune, (1.0 - smoothstep(1.0, 2.2, abs(dr - 340.0) * uZoom)) * step(0.5, fract(atan(rel.y, rel.x) * 3.8197)));
    col = mix(col, accent, rune * 0.55);
    for (int i = 0; i < 4; i++) {
      if (float(i) >= uRingN) break;
      float ri = uRings[i];
      col *= mix(0.55, 1.0, smoothstep(uThick * 0.5, uThick * 0.5 + 70.0, abs(dr - ri)));  // shade next to the walls
      if (float(i) + 1.0 < uRingN) {  // a runner carpet down the middle of each corridor
        float rm = 0.5 * (ri + uRings[min(i + 1, 3)]), dc = abs(dr - rm);
        vec3 rug = mix(base * 0.7, accent * 0.42, 0.65);
        col = mix(col, rug, 1.0 - smoothstep(52.0, 56.0, dc));
        col = mix(col, accent * 0.75, (1.0 - smoothstep(1.0, 2.4, abs(dc - 46.0) * uZoom)) * 0.6);
      }
    }
    if (dr > uTemple.z + uThick * 0.5) col = vec3(0.04, 0.035, 0.06);
    float light = mix(0.5, 1.0, 1.0 - smoothstep(250.0, 950.0, length(world - uCam)));
    o = vec4(col * light, 1.0);
    return;
  }
  bool outside = world.x < uBounds.x || world.y < uBounds.y || world.x > uBounds.z || world.y > uBounds.w;
  vec3 col = outside ? vec3(0.835, 0.800, 0.745) : vec3(0.953, 0.937, 0.902);  // arena #F3EFE6, out of bounds darker
  vec2 gd = abs(fract(world / 48.0 + 0.5) - 0.5) * 48.0 * uZoom;                // px to the nearest grid line
  col = mix(col, col * 0.94, 1.0 - smoothstep(0.3, 1.1, min(gd.x, gd.y)));
  vec2 dots = abs(fract(world / 240.0 + 0.5) - 0.5) * 240.0 * uZoom;           // small dots where major lines cross
  col = mix(col, col * 0.86, 1.0 - smoothstep(1.5, 2.6, length(dots)));
  if (uRingOn > 0.5) {
    const vec3 storm = vec3(0.486, 0.329, 0.949);
    float dc = length(world - uRing.xy);
    if (dc > uRing.z) col = mix(col, storm, 0.24);
    col = mix(col, storm, 1.0 - smoothstep(2.0, 3.5, abs(dc - uRing.z) * uZoom));       // the storm edge
    if (uNext.z < uRing.z - 5.0) {                                                        // next zone: dashed line
      vec2 dn = world - uNext.xy;
      float dash = step(0.5, fract(atan(dn.y, dn.x) * uNext.z / 60.0));
      col = mix(col, vec3(0.22, 0.17, 0.36), dash * (1.0 - smoothstep(1.0, 2.2, abs(length(dn) - uNext.z) * uZoom)) * 0.7);
    }
  }
  o = vec4(col, 1.0);
})";

static const char* kInstVert = R"(#version 300 es
layout(location = 0) in vec2 iPos;
layout(location = 1) in vec4 iA;   // radius, rot, sides, flash
layout(location = 2) in vec4 iB;   // aspect, alpha, pattern, outline
layout(location = 3) in vec3 iCol;
layout(location = 4) in vec3 iCol2;
uniform vec2 iRes; uniform vec2 iCam;
out vec2 vLocal; flat out vec4 vA; flat out vec4 vB; flat out vec3 vCol; flat out vec3 vCol2;
void main() {
  vec2 corner = vec2(float(gl_VertexID & 1) * 2.0 - 1.0, float(gl_VertexID >> 1) * 2.0 - 1.0);
  float ext = iA.x * max(1.0, iB.x) * 1.42 + 3.0;   // ('half' is a reserved word in GLSL ES 3.00)
  vLocal = corner * ext; vA = iA; vB = iB; vCol = iCol; vCol2 = iCol2;
  vec2 rel = iPos + vLocal - iCam;
  gl_Position = vec4(rel.x / (iRes.x * 0.5), -rel.y / (iRes.y * 0.5), 0.0, 1.0);
})";

static const char* kInstFrag = R"(#version 300 es
precision highp float;
in vec2 vLocal; flat in vec4 vA; flat in vec4 vB; flat in vec3 vCol; flat in vec3 vCol2;
uniform float uTime;
uniform mediump sampler2DArray uIcons;
out vec4 o;
float sdBox(vec2 p, vec2 b) { vec2 d = abs(p) - b; return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0); }
vec3 hue(float h) { return clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0); }
float sdStar5(vec2 p, float r, float rf) {
  const vec2 k1 = vec2(0.809016994375, -0.587785252292);
  const vec2 k2 = vec2(-k1.x, k1.y);
  p.x = abs(p.x);
  p -= 2.0 * max(dot(k1, p), 0.0) * k1;
  p -= 2.0 * max(dot(k2, p), 0.0) * k2;
  p.x = abs(p.x);
  p.y -= r;
  vec2 ba = rf * vec2(-k1.y, k1.x) - vec2(0.0, 1.0);
  float h = clamp(dot(p, ba) / dot(ba, ba), 0.0, r);
  return length(p - ba * h) * sign(p.y * ba.x - p.x * ba.y);
}
vec3 paint(int pat, vec2 u, vec3 a, vec3 b) {
  if (pat == 1) return mod(floor((u.x + u.y) * 2.2 + 20.0), 2.0) < 0.5 ? a : b;                        // stripes
  if (pat == 2) return mod(floor(u.x * 2.4 + 20.0) + floor(u.y * 2.4 + 20.0), 2.0) < 0.5 ? a : b;       // checker
  if (pat == 3) { float n = sin(u.x * 5.3 + 1.3) * sin(u.y * 4.1 - 0.7) + 0.6 * sin((u.x - u.y) * 6.7 + 2.0);
                  return n > 0.35 ? b : (n < -0.45 ? mix(a, b, 0.5) * 0.85 : a); }                        // camo
  if (pat == 4) return mix(a, b, smoothstep(-1.0, 1.0, u.y));                                            // gradient
  if (pat == 5) { float h = clamp(1.0 - length(u - vec2(-0.35, -0.45)) * 0.9, 0.0, 1.0);
                  return mix(a * 0.8, vec3(1.0), h * h * 0.9); }                                          // chrome
  if (pat == 6) return mix(hue(atan(u.y, u.x) / 6.2831853 + uTime * 0.2), vec3(1.0), 0.18);             // rainbow
  if (pat == 7) return mix(a, b, 0.5 + 0.5 * sin(atan(u.y, u.x) * 2.0 + length(u) * 5.0 - uTime * 3.0)); // swirl
  if (pat == 8) return mix(a, b, 0.5 + 0.5 * sin(uTime * 5.0));                                         // pulse
  if (pat == 9) { vec2 g = fract(u * 2.0 + 0.25) - 0.5; return length(g) < 0.22 ? b : a; }              // dots
  if (pat == 10) return mod(floor(length(u) * 3.2), 2.0) < 0.5 ? a : b;                                 // rings
  if (pat == 11) return u.y < 0.0 ? a : b;                                                                // split
  if (pat == 13) return mix(hue(mod(floor((u.x + u.y) * 2.2) + 60.0, 6.0) / 6.0), vec3(1.0), 0.12);    // rainbow stripes
  if (pat == 12) {                                                                                       // galaxy
    vec2 g = floor(u * 4.0 + 8.0); float h = fract(sin(dot(g, vec2(12.9898, 78.233))) * 43758.5453);
    vec2 f = fract(u * 4.0) - 0.5; float tw = 0.5 + 0.5 * sin(uTime * 3.0 + h * 20.0);
    vec3 base = mix(a, a * 0.55, clamp(length(u), 0.0, 1.0));
    return (h > 0.55 && length(f) < 0.06 + 0.1 * tw) ? b : base;
  }
  return a;
}
void main() {
  float R = vA.x, rot = vA.y, n = vA.z, flash = vA.w, aspect = vB.x, alpha = vB.y, ol = vB.w;
  int pat = int(vB.z + 0.5);
  float c = cos(rot), s = sin(rot);
  vec2 q = vec2(c * vLocal.x + s * vLocal.y, -s * vLocal.x + c * vLocal.y);
  if (n < -5.5) {  // an icon from the atlas (pattern = cell; +100: a stat mask, colored rim / fill / icon)
    vec2 uv = q / R * 0.5 + 0.5;
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) discard;
    bool tinted = pat >= 100;
    vec4 t = texture(uIcons, vec3(uv, float(tinted ? pat - 100 : pat)));
    float ia = t.a * alpha;
    if (ia <= 0.003) discard;
    vec3 ic = tinted ? mix(mix(vCol2, vCol, t.g), vCol * 0.75, t.r) : t.rgb;
    o = vec4(mix(ic, vec3(1.0), flash), ia);
    return;
  }
  float d, ot; vec2 u;
  if (n < -4.5) { float h = R * aspect, cr = min(R, h) * 0.3; d = sdBox(q, vec2(R - cr, h - cr)) - cr; ot = min(ol, min(R, h) * 0.4); u = vec2(q.x / R, q.y / h); }
  else if (n < -3.5) { d = sdStar5(vec2(q.x, -q.y), R, 0.45); ot = min(ol, R * 0.25); u = q / R; }
  else if (n < -2.5) { float h = R * aspect; d = abs(length(q) - R) - h; ot = min(ol, h * 0.6); u = q / R; }
  else if (n < -1.5) { float h = R * aspect; d = sdBox(q, vec2(max(R - h, 0.0), 0.0)) - h; ot = min(ol, h * 0.6); u = vec2(q.x / R, q.y / h); }
  else if (n < -0.5) { float h = R * aspect; d = sdBox(q, vec2(R, h)); ot = min(ol, h * 0.6); u = vec2(q.x / R, q.y / h); }
  else if (n < 2.5) { d = length(q) - R; ot = min(ol, R * 0.5); u = q / R; }
  else {
    float an = 6.2831853 / n;
    float bn = mod(atan(q.x, q.y), an) - 0.5 * an;
    d = length(q) * cos(bn) - R * cos(0.5 * an);
    ot = min(ol, R * 0.4); u = q / R;
  }
  float aa = max(fwidth(d), 1e-4);
  float a = clamp(0.5 - d / aa, 0.0, 1.0) * alpha;
  if (a <= 0.0) discard;
  vec3 fill = paint(pat, u, vCol, vCol2);
  vec3 edge = (pat == 1 || pat == 2 || pat == 3 || pat == 9 || pat == 10 || pat == 11) ? mix(vCol, vCol2, 0.5) * 0.75
            : (pat == 6 || pat == 13 ? vec3(0.45) : (pat == 12 ? vCol * 0.6 : fill * 0.75));
  fill = mix(fill, vec3(1.0), flash);
  edge = mix(edge, vec3(1.0), flash * 0.6);
  vec3 col = ol > 0.0 ? mix(fill, edge, smoothstep(-ot - 0.5 * aa, -ot + 0.5 * aa, d)) : fill;
  o = vec4(col, a);
})";

// Inside a temple you are the light: it falls off smoothly around you, and the stone walls cast shadows away from you
// (you only see what has a clear line to you). The same test runs on the CPU for name tags (templeSight below).
static const char* kFogFrag = R"(#version 300 es
precision highp float;
uniform vec2 uRes; uniform vec2 uCam; uniform float uDpr; uniform float uZoom;
uniform vec3 uTemple;     // middle of the temple, outer radius
uniform vec2 uLight;      // where the light is (your tank), in world units
uniform vec4 uRingA[5];   // each ring: radius, the angle of its way in, half the way in (radians, 0 = none)
uniform float uRingN;
uniform float uThick;
uniform vec4 uBlock[4];   // dead-end walls: from .xy to .zw (relative to the middle)
uniform float uBlockN;
out vec4 o;
float angDist(float a, float b) { return abs(mod(a - b + 3.14159265, 6.2831853) - 3.14159265); }
// Where the line L->F crosses a circle of radius rc: up to two t values in (0, 1) (or -1 for none).
vec2 cross2(vec2 L, vec2 d, float rc) {
  float a = dot(d, d), b = 2.0 * dot(L, d), c = dot(L, L) - rc * rc;
  float disc = b * b - 4.0 * a * c;
  if (disc <= 0.0 || a < 1e-3) return vec2(-1.0);
  float sq = sqrt(disc);
  vec2 t = vec2(-b - sq, -b + sq) / (2.0 * a);
  return vec2(t.x > 0.0 && t.x < 1.0 ? t.x : -1.0, t.y > 0.0 && t.y < 1.0 ? t.y : -1.0);
}
// How open a crossing is: 1 in the ring's way in, 0 in solid stone, soft at the edges (wider further from the wall).
float gapOpen(vec2 P, float rc, vec4 ring, float far) {
  if (ring.z <= 0.0) return 0.0;
  float edge = (ring.z - angDist(atan(P.y, P.x), ring.y)) * rc - uThick * 0.3;
  float w = 4.0 + 30.0 * far;
  return smoothstep(-w, w, edge);
}
float sight(vec2 L, vec2 F) {
  vec2 d = F - L;
  float rF = length(F), seen = 1.0;
  for (int i = 0; i < 5; i++) {
    if (float(i) >= uRingN) break;
    vec4 ring = uRingA[i];
    float ri = ring.x - uThick * 0.5, ro = ring.x + uThick * 0.5;
    bool inBody = rF > ri && rF < ro;  // F is on this wall's top: the crossing that got it there doesn't hide it
    vec4 t = vec4(cross2(L, d, ri), cross2(L, d, ro));
    float last = max(max(t.x, t.y), max(t.z, t.w));
    for (int k = 0; k < 4; k++) {
      float tk = t[k];
      if (tk < 0.0 || (inBody && tk == last)) continue;
      float rc = k < 2 ? ri : ro;
      vec2 P = L + d * tk;
      seen *= gapOpen(P, rc, ring, (1.0 - tk) / max(tk, 0.05));
    }
  }
  for (int i = 0; i < 4; i++) {
    if (float(i) >= uBlockN) break;
    vec2 a = uBlock[i].xy, b = uBlock[i].zw, ab = b - a;
    vec2 n = normalize(vec2(-ab.y, ab.x)) * uThick * 0.5;
    vec2 q = F - a;
    float h = clamp(dot(q, ab) / dot(ab, ab), 0.0, 1.0);
    if (length(q - ab * h) < uThick * 0.5) continue;  // on top of it
    for (int s = 0; s < 2; s++) {  // its two faces
      vec2 a2 = a + (s == 0 ? n : -n), e = ab;
      float den = d.x * e.y - d.y * e.x;
      if (abs(den) < 1e-4) continue;
      vec2 w0 = a2 - L;
      float tt = (w0.x * e.y - w0.y * e.x) / den, uu = (w0.x * d.y - w0.y * d.x) / den;
      if (tt > 0.0 && tt < 1.0 && uu > 0.0 && uu < 1.0) seen = 0.0;
    }
  }
  return seen;
}
void main() {
  vec2 sp = gl_FragCoord.xy / uDpr - 0.5 * uRes;
  vec2 world = uCam + sp * vec2(1.0, -1.0) / uZoom;
  vec2 L = uLight - uTemple.xy, F = world - uTemple.xy;
  float fall = 1.0 - smoothstep(420.0, 1250.0, length(world - uLight));
  float lit = fall > 0.001 ? fall * sight(L, F) : 0.0;
  if (length(F) > uTemple.z + uThick * 0.5) lit = 0.0;
  vec2 q = sp / (0.5 * uRes);
  float edge = smoothstep(0.6, 1.2, length(q * vec2(1.0, 0.82)));
  o = vec4(0.015, 0.01, 0.03, max((1.0 - lit) * 0.97, edge * 0.85));
})";

static GLuint compile(GLenum type, const char* src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, nullptr);
  glCompileShader(s);
  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) { char log[1024]; glGetShaderInfoLog(s, 1024, nullptr, log); std::printf("shader error: %s\n", log); }
  return s;
}

static GLuint link(const char* vs, const char* fs) {
  GLuint p = glCreateProgram();
  glAttachShader(p, compile(GL_VERTEX_SHADER, vs));
  glAttachShader(p, compile(GL_FRAGMENT_SHADER, fs));
  glLinkProgram(p);
  return p;
}

// ---------------- Input ----------------
static bool typingInPage() {  // don't steer the tank while the player types their name
  return EM_ASM_INT({ const a = document.activeElement; return a && (a.tagName === 'INPUT' || a.tagName === 'TEXTAREA') ? 1 : 0; }) != 0;
}

static EM_BOOL onKey(int type, const EmscriptenKeyboardEvent* e, void*) {
  const bool down = type == EMSCRIPTEN_EVENT_KEYDOWN;
  if (down && typingInPage()) return EM_FALSE;
  if (down && std::strcmp(e->code, "Escape") == 0 && (g_linkSel || g_linkDown)) {
    g_linkSel = g_linkPress = 0; g_linkDown = false; linkNote(0);
    return EM_FALSE;
  }
  if (std::strcmp(e->code, "ShiftLeft") == 0 || std::strcmp(e->code, "ShiftRight") == 0 || std::strcmp(e->code, "KeyE") == 0) {
    if (down && !e->repeat) useQueued = true;
    return g_menu ? EM_FALSE : EM_TRUE;
  }
  if (std::strcmp(e->code, "KeyF") == 0) {
    if (down && !e->repeat) swapQueued = true;
    return g_menu ? EM_FALSE : EM_TRUE;
  }
  if (std::strlen(e->code) != 4 || std::strncmp(e->code, "Key", 3) != 0) return EM_FALSE;
  switch (e->code[3]) {  // "KeyW" -> 'W'
    case 'W': keyW = down; break;
    case 'A': keyA = down; break;
    case 'S': keyS = down; break;
    case 'D': keyD = down; break;
    case 'R': keyR = down; break;  // oldest relic
    case 'T': keyT = down; break;  // newer relic
    default: return EM_FALSE;
  }
  return g_menu ? EM_FALSE : EM_TRUE;
}

static EM_BOOL onMouseMove(int, const EmscriptenMouseEvent* e, void*) {
  mouseX = (float)e->targetX;
  mouseY = (float)e->targetY;
  return EM_TRUE;
}

static EM_BOOL onMouseButton(int type, const EmscriptenMouseEvent* e, void*) {
  mouseX = (float)e->targetX;  // keep aim current even if no move event preceded the click
  mouseY = (float)e->targetY;
  if (e->button == 0 && g_linkMode && !g_menu) {  // drawing lines: the mouse doesn't shoot
    linkClick(type == EMSCRIPTEN_EVENT_MOUSEDOWN);
    mouseDown = false;
    return EM_TRUE;
  }
  if (e->button == 0) {
    mouseDown = (type == EMSCRIPTEN_EVENT_MOUSEDOWN);
    if (mouseDown) shootQueued = true;
  }
  return EM_TRUE;
}

// ---------------- Client-side effects ----------------
// Snapshots of the previous tick: used to interpolate motion and to notice events (deaths, shots, pickups).
struct TankSnap {
  Vec2 pos; float aim, health, cooldown, recharge; bool alive; uint8_t stat[kStatCount]; Power held, active; Relic relic[2];
};
struct PickSnap { uint32_t id; Vec2 pos; int kind, rarity; };
struct BulletSnap { uint32_t id, owner; Vec2 pos; float r, left, step; int kind; float full; };
static std::vector<TankSnap> g_tsnap;
static std::vector<BulletSnap> g_bsnap;
static std::vector<PickSnap> g_usnap, g_csnap, g_rsnap;
static std::unordered_map<uint32_t, Vec2> g_bprev, g_mprev;  // bullets / rockets and drones on the previous tick

struct TankFx { float flash = 0, recoil = 0, trailT = 0, shownHp = -1; };
static std::vector<TankFx> g_tfx;

// A fading primitive. Ghosts grow and fade when a tank dies; particles drift and shrink. A negative age is a delay.
// Mirrored by the preview particle system in web/meta.js (same fields, same numbers).
struct Fx {
  Vec2 pos, vel, off;
  float age = 0, life = 0.25f, r = 10, rot = 0, spin = 0, sides = 0, aspect = 1, a0 = 1, drag = 0, ol = 0, grow = 0, grav = 0;
  bool shrink = false;
  bool fade2 = false;  // the color goes from c to c2 over its life (fire turning to smoke)
  Col c{1, 1, 1}, c2{1, 1, 1};
  int pat = 0;
};
static std::vector<Fx> g_fxLow, g_fxHigh;  // under tanks (trails, pickups) / over everything (deaths, shots)
static uint32_t g_fxSeed = 0x9e3779b9u;
static float frand() { g_fxSeed ^= g_fxSeed << 13; g_fxSeed ^= g_fxSeed >> 17; g_fxSeed ^= g_fxSeed << 5; return (g_fxSeed & 0xffffff) / 16777216.0f; }
static float frange(float a, float b) { return a + (b - a) * frand(); }
static Vec2 frandDir() { const float a = frange(0, 2 * kPi); return {std::cos(a), std::sin(a)}; }

static void fxPush(std::vector<Fx>& v, const Fx& f) { if (v.size() < 4000) v.push_back(f); }

static Vec2 g_camPos;  // camera of the last rendered frame (effects off-screen are skipped)
static bool nearView(Vec2 p) { return inView(p, g_camPos, 300.0f); }

static void tankGhost(Vec2 p, float aim, const Look& lk, float sc = 1.0f) {
  Fx barrel; barrel.pos = p; barrel.off = Vec2{std::cos(aim), std::sin(aim)} * (30.0f * sc);
  barrel.r = 30 * sc; barrel.aspect = 11.0f / 30.0f; barrel.sides = -1; barrel.rot = aim; barrel.ol = 3.5f; barrel.grow = 0.5f;
  barrel.life = 0.28f; barrel.c = lk.b1; barrel.c2 = lk.b2; barrel.pat = lk.bpat;
  Fx body = barrel; body.off = {}; body.r = kTankRadius * sc; body.aspect = 1; body.sides = 0; body.c = lk.c1; body.c2 = lk.c2; body.pat = lk.pat;
  fxPush(g_fxHigh, barrel);
  fxPush(g_fxHigh, body);
}

static void deathBurst(int kind, Vec2 p, const Look& lk) {
  switch (kind) {
    case 1:  // Confetti Pop
      for (int i = 0; i < 30; ++i) {
        Fx f; f.pos = p; f.vel = frandDir() * frange(160, 520); f.drag = 2.4f; f.sides = -1; f.r = 5; f.aspect = 0.6f;
        f.spin = frange(-12, 12); f.rot = frange(0, 6.28f); f.life = frange(0.8f, 1.3f); f.c = hueC(frand()); f.ol = 1.5f;
        fxPush(g_fxHigh, f);
      }
      break;
    case 2:  // Shatter: shards of the body
      for (int i = 0; i < 10; ++i) {
        Fx f; f.pos = p + frandDir() * 10.0f; f.vel = frandDir() * frange(120, 380); f.drag = 2.0f; f.sides = 3; f.r = frange(9, 14);
        f.spin = frange(-8, 8); f.rot = frange(0, 6.28f); f.life = frange(0.6f, 0.9f); f.c = lk.c1; f.c2 = lk.c2; f.pat = lk.pat; f.ol = 3;
        fxPush(g_fxHigh, f);
      }
      break;
    case 3:  // Fireworks
      for (int b = 0; b < 3; ++b) {
        const Vec2 c = p + frandDir() * frange(20, 70);
        const Col col = hueC(frand());
        for (int i = 0; i < 16; ++i) {
          const float a = i * (2 * kPi / 16);
          Fx f; f.pos = c; f.vel = Vec2{std::cos(a), std::sin(a)} * frange(240, 320); f.drag = 2.6f; f.r = 3.5f; f.life = frange(0.7f, 1.0f);
          f.c = mixC(col, {1, 1, 1}, 0.2f); f.shrink = true;
          fxPush(g_fxHigh, f);
        }
      }
      break;
    case 4: {  // Legend: gold shockwave + gold/purple diamonds
      Fx ring; ring.pos = p; ring.r = 30; ring.grow = 3.0f; ring.a0 = 0.45f; ring.life = 0.5f; ring.c = {1.0f, 0.8f, 0.25f};
      fxPush(g_fxHigh, ring);
      for (int i = 0; i < 22; ++i) {
        Fx f; f.pos = p; f.vel = frandDir() * frange(200, 460); f.drag = 2.2f; f.sides = 4; f.r = frange(5, 8); f.spin = frange(-9, 9);
        f.life = frange(0.7f, 1.1f); f.c = (i & 1) ? Col{1.0f, 0.8f, 0.25f} : Col{0.69f, 0.38f, 1.0f}; f.ol = 2; f.shrink = true;
        fxPush(g_fxHigh, f);
      }
      break;
    }
    case 5:  // Streamers
      for (int i = 0; i < 14; ++i) {
        const Vec2 d = frandDir();
        Fx f; f.pos = p; f.vel = d * frange(200, 420); f.drag = 2.0f; f.sides = -1; f.r = 11; f.aspect = 0.2f; f.rot = std::atan2(d.y, d.x);
        f.spin = frange(-3, 3); f.life = frange(0.9f, 1.2f); f.c = hueC(frand()); f.ol = 1;
        fxPush(g_fxHigh, f);
      }
      break;
    case 6:  // Pixel Burst
      for (int i = 0; i < 20; ++i) {
        Fx f; f.pos = p; f.vel = frandDir() * frange(150, 400); f.drag = 2.2f; f.sides = -1; f.r = 5.5f; f.life = frange(0.6f, 0.9f);
        f.c = (i & 1) ? lk.c1 : lk.c2; f.ol = 2;
        fxPush(g_fxHigh, f);
      }
      break;
    case 7: {  // Over the Rainbow: six rings in rainbow order, then a ring of colored dots
      Fx fl; fl.pos = p; fl.r = 30; fl.grow = 1.2f; fl.a0 = 0.7f; fl.life = 0.3f; fl.c = {1, 1, 1};
      fxPush(g_fxHigh, fl);
      for (int i = 0; i < 6; ++i) {
        Fx f; f.pos = p; f.sides = -3; f.r = 22; f.aspect = 0.14f; f.grow = 3.4f; f.life = 0.65f; f.age = -0.07f * i; f.c = hueC(i / 6.0f);
        fxPush(g_fxHigh, f);
      }
      for (int i = 0; i < 24; ++i) {
        const float a = i * (2 * kPi / 24);
        Fx f; f.pos = p; f.vel = Vec2{std::cos(a), std::sin(a)} * frange(260, 330); f.drag = 2.4f; f.r = 4; f.life = frange(0.8f, 1.0f);
        f.c = hueC(i / 24.0f); f.shrink = true;
        fxPush(g_fxHigh, f);
      }
      break;
    }
    default: break;
  }
}

static void muzzleFx(int kind, Vec2 tip, float aim) {
  const Vec2 d{std::cos(aim), std::sin(aim)};
  switch (kind) {
    case 1:  // Spark
      for (int i = 0; i < 5; ++i) {
        const float a = aim + frange(-0.5f, 0.5f);
        Fx f; f.pos = tip; f.vel = Vec2{std::cos(a), std::sin(a)} * frange(300, 520); f.drag = 6; f.r = 2.6f; f.life = 0.18f;
        f.c = {1.0f, 0.88f, 0.4f}; f.shrink = true;
        fxPush(g_fxHigh, f);
      }
      break;
    case 2:  // Star Burst
      for (int i = 0; i < 2; ++i) {
        Fx f; f.pos = tip + d * 6.0f; f.sides = 4; f.r = 13; f.rot = aim + i * 0.785f; f.life = 0.13f; f.grow = 0.7f;
        f.c = i ? Col{1.0f, 0.95f, 0.6f} : Col{1, 1, 1};
        fxPush(g_fxHigh, f);
      }
      break;
    case 3:  // Lightning
      for (int i = 0; i < 4; ++i) {
        const float a = aim + frange(-1.0f, 1.0f);
        Fx f; f.pos = tip + Vec2{std::cos(a), std::sin(a)} * 10.0f; f.sides = -1; f.r = 9; f.aspect = 0.16f; f.rot = a + frange(-0.6f, 0.6f);
        f.life = 0.1f; f.c = {0.55f, 0.95f, 1.0f};
        fxPush(g_fxHigh, f);
      }
      break;
    case 4: {  // Flare
      Fx f; f.pos = tip + d * 4.0f; f.r = 13; f.grow = 1.0f; f.a0 = 0.8f; f.life = 0.12f; f.c = {1.0f, 0.6f, 0.2f};
      fxPush(g_fxHigh, f);
      break;
    }
    case 5: {  // Plasma
      Fx f; f.pos = tip + d * 4.0f; f.r = 10; f.grow = 1.6f; f.a0 = 0.6f; f.life = 0.18f; f.c = {0.7f, 0.4f, 1.0f};
      fxPush(g_fxHigh, f);
      break;
    }
    case 6:  // Confetti Cannon
      for (int i = 0; i < 7; ++i) {
        const float a = aim + frange(-0.6f, 0.6f);
        Fx f; f.pos = tip; f.vel = Vec2{std::cos(a), std::sin(a)} * frange(260, 480); f.drag = 4; f.grav = 260; f.sides = -1;
        f.r = 3.5f; f.aspect = 0.55f; f.spin = frange(-14, 14); f.life = 0.45f; f.c = hueC(frand());
        fxPush(g_fxHigh, f);
      }
      break;
    case 7: {  // Ring Pop: a thin ring that snaps outward
      Fx f; f.pos = tip + d * 4.0f; f.sides = -3; f.r = 9; f.aspect = 0.2f; f.grow = 1.8f; f.life = 0.16f; f.c = {1, 1, 1};
      fxPush(g_fxHigh, f);
      break;
    }
    case 8:  // Rainbow Burst: a fan of six dots in rainbow order
      for (int i = 0; i < 6; ++i) {
        const float a = aim + (i - 2.5f) * 0.2f;
        Fx f; f.pos = tip; f.vel = Vec2{std::cos(a), std::sin(a)} * 400.0f; f.drag = 6; f.r = 3.4f; f.life = 0.24f;
        f.c = mixC(hueC(i / 6.0f), {1, 1, 1}, 0.1f); f.shrink = true;
        fxPush(g_fxHigh, f);
      }
      break;
    default: break;
  }
}

// Kill effects play where your victim was, on top of their own death effect. Numbers match web/meta.js killFx().
static void killFx(int kind, Vec2 p, const Look& lk) {
  switch (kind) {
    case 1: {  // Shockwave: two rings
      for (int i = 0; i < 2; ++i) {
        Fx f; f.pos = p; f.sides = -3; f.r = 30; f.aspect = 0.1f; f.grow = 3.2f; f.life = 0.5f; f.age = -0.12f * i;
        f.c = i ? lk.c1 : Col{1, 1, 1}; f.a0 = 0.9f;
        fxPush(g_fxHigh, f);
      }
      break;
    }
    case 2:  // Star Pop
      for (int i = 0; i < 10; ++i) {
        Fx f; f.pos = p; f.vel = frandDir() * frange(220, 420); f.drag = 2.6f; f.sides = -4; f.r = frange(9, 13); f.spin = frange(-7, 7);
        f.life = frange(0.6f, 0.85f); f.c = (i & 1) ? Col{1.0f, 0.85f, 0.25f} : Col{1, 1, 1}; f.ol = 2; f.shrink = true;
        fxPush(g_fxHigh, f);
      }
      break;
    case 3:  // Coin Shower
      for (int i = 0; i < 14; ++i) {
        Fx f; f.pos = p; f.vel = Vec2{frange(-220, 220), frange(-520, -260)}; f.grav = 1100; f.r = frange(7, 9); f.life = frange(0.8f, 1.0f);
        f.c = {1.0f, 0.79f, 0.22f}; f.ol = 2.5f; f.spin = 4;
        fxPush(g_fxHigh, f);
      }
      break;
    case 4: {  // Ghost: a little ghost floats up
      Fx body; body.pos = p; body.vel = {0, -110}; body.r = 24; body.a0 = 0.75f; body.life = 1.1f; body.grow = 0.2f; body.c = {1, 1, 1};
      body.ol = 2.5f;
      fxPush(g_fxHigh, body);
      for (int e = -1; e <= 1; e += 2) {
        Fx eye = body; eye.off = {e * 8.0f, -3.0f}; eye.r = 3.6f; eye.ol = 0; eye.c = {0.17f, 0.13f, 0.25f}; eye.a0 = 0.9f; eye.grow = 0;
        fxPush(g_fxHigh, eye);
      }
      break;
    }
    case 5: {  // Lightning Strike: bolts from above and a flash
      for (int i = 0; i < 3; ++i) {
        Fx b; b.pos = p + Vec2{frange(-14, 14), -90.0f}; b.sides = -1; b.r = 90; b.aspect = 0.05f; b.rot = kPi * 0.5f + frange(-0.25f, 0.25f);
        b.life = 0.22f; b.c = {0.62f, 0.93f, 1.0f}; b.age = -0.05f * i;
        fxPush(g_fxHigh, b);
      }
      Fx fl; fl.pos = p; fl.r = 40; fl.grow = 1.2f; fl.a0 = 0.6f; fl.life = 0.3f; fl.c = {0.85f, 0.97f, 1.0f};
      fxPush(g_fxHigh, fl);
      break;
    }
    case 6: {  // Supernova (Legendary): rainbow ring, white flash, rainbow stars
      Fx fl; fl.pos = p; fl.r = 34; fl.grow = 1.5f; fl.a0 = 0.7f; fl.life = 0.35f; fl.c = {1, 1, 1};
      fxPush(g_fxHigh, fl);
      for (int i = 0; i < 3; ++i) {
        Fx rg; rg.pos = p; rg.sides = -3; rg.r = 26; rg.aspect = 0.12f; rg.grow = 4.0f; rg.life = 0.6f; rg.age = -0.1f * i; rg.c = hueC(i / 3.0f);
        fxPush(g_fxHigh, rg);
      }
      for (int i = 0; i < 20; ++i) {
        Fx f; f.pos = p; f.vel = frandDir() * frange(250, 520); f.drag = 2.2f; f.sides = -4; f.r = frange(6, 10); f.spin = frange(-8, 8);
        f.life = frange(0.7f, 1.1f); f.c = hueC(frand()); f.ol = 2; f.shrink = true;
        fxPush(g_fxHigh, f);
      }
      break;
    }
    case 7:  // Vortex: sparks spiral in
      for (int i = 0; i < 18; ++i) {
        const float a = i * (2 * kPi / 18);
        const Vec2 rd{std::cos(a), std::sin(a)};
        Fx f; f.pos = p + rd * 95.0f; f.vel = Vec2{-rd.y, rd.x} * 330.0f - rd * 230.0f; f.drag = 1.6f; f.r = 5; f.life = 0.55f;
        f.c = (i & 1) ? Col{0.49f, 0.33f, 0.95f} : Col{0.18f, 0.77f, 0.71f}; f.shrink = true;
        fxPush(g_fxHigh, f);
      }
      break;
    default: break;
  }
}

static void trailFx(int kind, Vec2 p, Vec2 back, const Look& lk) {
  Fx f; f.pos = p + back * 26.0f + frandDir() * 5.0f;
  switch (kind) {
    case 1: f.r = frange(7, 10); f.c = {0.62f, 0.6f, 0.56f}; f.a0 = 0.45f; f.life = 0.55f; f.grow = 0.8f; break;            // Dust Puff
    case 2: f.sides = 4; f.r = frange(4, 6); f.c = frand() < 0.5f ? Col{1, 1, 1} : Col{1.0f, 0.92f, 0.5f}; f.life = 0.45f;   // Sparkle Line
            f.spin = 6; f.shrink = true; break;
    case 3: f.r = 11; f.c = hueC(g_clock * 0.8f); f.a0 = 0.6f; f.life = 0.45f; f.shrink = true; break;                    // Rainbow Ribbon
    case 4: f.r = frange(4, 8); f.c = {0.62f, 0.86f, 1.0f}; f.a0 = 0.55f; f.life = 0.9f; f.grow = 0.3f;                    // Bubbles
            f.vel = frandDir() * 20.0f; f.ol = 1.5f; break;
    case 5: f.r = frange(5, 7); f.c = mixC({1.0f, 0.55f, 0.15f}, {1.0f, 0.25f, 0.15f}, frand()); f.a0 = 0.85f;            // Ember
            f.life = 0.5f; f.vel = frandDir() * 40.0f; f.shrink = true; break;
    case 6: f.sides = 4; f.r = 6; f.c = frand() < 0.5f ? Col{1.0f, 0.8f, 0.25f} : Col{0.69f, 0.38f, 1.0f}; f.life = 0.6f;    // Legend
            f.shrink = true; f.spin = 5; f.ol = 2; break;
    case 7: f.r = 13; f.c = {1, 1, 1}; f.a0 = 0.3f; f.life = 0.7f; f.grow = 1.0f; break;                                   // Mist
    case 8: f.r = 9; f.c = {0.7f, 0.4f, 1.0f}; f.a0 = 0.5f; f.life = 0.4f; f.shrink = true; break;                          // Plasma
    case 9: f.r = 16; f.c = lk.c1; f.a0 = 0.5f; f.life = 0.3f; f.shrink = true; break;                                     // Comet
    case 10: f.sides = -1; f.r = 4; f.aspect = 0.55f; f.c = hueC(frand()); f.life = 0.6f; f.spin = frange(-10, 10);       // Party
             f.rot = frange(0, 6.28f); f.vel = frandDir() * 30.0f; f.ol = 1; break;
    case 11: f.r = frange(3, 5); f.c = {1, 1, 1}; f.life = 1.0f; f.vel = frandDir() * 18.0f; f.ol = 1.2f; break;          // Snowfall
    default: return;
  }
  fxPush(g_fxLow, f);
}

static void updateFx(std::vector<Fx>& v, float dt) {
  size_t n = 0;
  for (Fx& f : v) {
    f.age += dt;
    if (f.age >= f.life) continue;
    if (f.age < 0) { v[n++] = f; continue; }
    f.vel.y += f.grav * dt;
    f.pos += f.vel * dt;
    if (f.drag > 0) f.vel = f.vel * std::exp(-f.drag * dt);
    f.rot += f.spin * dt;
    v[n++] = f;
  }
  v.resize(n);
}

static void drawFx(const std::vector<Fx>& v, Vec2 cam) {
  for (const Fx& f : v) {
    if (f.age < 0) continue;
    const float k = f.age / f.life;
    float s = 1.0f + f.grow * k;
    if (f.shrink) s *= 1.0f - k;
    const Vec2 p = f.pos + f.off * s;
    if (!inView(p, cam, f.r * s + 20)) continue;
    Inst& i = add(p.x, p.y, f.r * s, f.rot, f.sides, f.fade2 ? mixC(f.c, f.c2, std::min(1.0f, k * 1.6f)) : f.c);
    i.aspect = f.aspect; i.alpha = f.a0 * (1.0f - k); i.ol = f.ol;
    if (!f.fade2) paint(i, f.pat, f.c2);
  }
}

// ---------------- Match setup ----------------
static const Tank& camTank() {
  if (g_spectateId)
    for (const Tank& t : g_world.tanks)
      if (t.id == g_spectateId) return t;
  return g_world.tanks[0];
}

// Follow the next/previous living tank (the test panel and the death screen use this).
static void cycleSpectate(int dir) {
  std::vector<uint32_t> ids;
  for (const Tank& t : g_world.tanks) if (t.alive && t.id != 1 && !npc(t)) ids.push_back(t.id);
  if (ids.empty()) { g_spectateId = 0; return; }
  int cur = -1;
  for (size_t i = 0; i < ids.size(); ++i) if (ids[i] == g_spectateId) cur = (int)i;
  int n = (int)ids.size();
  int next = cur < 0 ? (dir > 0 ? 0 : n - 1) : ((cur + dir) % n + n) % n;
  g_spectateId = ids[next];
}

static void resetClientState() {
  g_tsnap.clear(); g_bsnap.clear(); g_usnap.clear(); g_csnap.clear(); g_bprev.clear();
  g_fxLow.clear(); g_fxHigh.clear();
  g_tfx.assign(g_world.tanks.size(), TankFx{});
  g_myUpgrades = g_myPowerUses = g_myCrates = g_myLegend = g_myMobs = g_myMages = 0;
  g_legendMark = false;
  g_resultSent = false;
}

// What a bot wears: better players have played longer, so they wear rarer things. The page sends the pool in four bands
// of equal size, from basic (pastel and base colors, no effects) to the rarest; this picks whether the bot wears
// anything from it, and from which band, by its tier.
static void pickLook(const Tank& t, uint32_t h) {
  if (g_lookPool.empty()) return;
  const int n = (int)g_lookPool.size(), band = std::max(1, n / 4);
  const int tier = (int)t.mind.tier;
  //                           wear%  band weights (basic, common, rare, rarest)
  static const int odds[9][5] = {{55, 100, 0, 0, 0}, {60, 60, 40, 0, 0}, {70, 20, 60, 20, 0}, {80, 0, 30, 55, 15},
                                 {90, 0, 0, 50, 50}, {100, 0, 0, 25, 75}, {100, 0, 0, 20, 80}, {100, 0, 0, 20, 80}, {100, 0, 0, 0, 100}};
  const int* o = odds[std::clamp(tier, 0, 8)];
  if (t.sentryOf || (int)((h >> 8) % 100) >= o[0]) return;
  int r = (int)((h >> 16) % 100), b = 0;
  while (b < 3 && r >= o[1 + b]) { r -= o[1 + b]; ++b; }
  g_lookSlot[t.id] = std::min(n - 1, b * band + (int)((h >> 4) % (uint32_t)band));
}

static void setupMatch(int bots, bool respawn, uint32_t seed, int mode = 0) {
  g_world = World{};
  g_linkSel = g_linkPress = 0; g_linkDown = false;
  linkNote(0);
  g_mode = mode;
  g_world.allowRespawn = respawn;
  int map = -1;
  if (g_sandOn && !g_menu) {  // an editable sandbox: its play area, fog, loot and temples
    World& w = g_world;
    const SandboxCfg& c = g_sand;
    w.sandbox = true;
    const float half = std::clamp(c.size, 2000, (int)kMapSize) * 0.5f;
    w.lo = Vec2{kMapSize / 2 - half, kMapSize / 2 - half};
    w.hi = Vec2{kMapSize / 2 + half, kMapSize / 2 + half};
    w.fogMode = c.fog;
    w.fogR0 = (float)c.fogR0; w.fogR1 = std::min((float)c.fogR1, w.fogR0);
    w.fogT0 = 20.0f; w.fogT1 = 20.0f + std::max(10, c.fogTime);
    w.lootMul = c.loot;
    w.ringScale = half * 2.0f / kMapSize;
    w.spotScale = std::min(1.0f, (half - 700.0f) / 4650.0f);
    w.emptyMap = c.map == 3;
    w.noTemples = !c.temples || half < 1500.0f;
    map = c.map == 3 ? 0 : c.map;
  }
  g_world.init(seed, mode == 1 ? Mode::Rampage : Mode::Royale, map);
  const bool rampage = mode == 1;
  // Spawn everyone well apart so nobody is shot in the first seconds.
  auto spawnPos = [&]() {
    if (rampage) {  // Rocket Rampage: the spot farthest from everyone already placed (tanks start spread out)
      Vec2 best = g_world.lootPos(400.0f);
      float bestD = -1;
      for (int tries = 0; tries < 60; ++tries) {
        const Vec2 p = g_world.lootPos(400.0f);
        if (g_world.inWall(p, 60.0f)) continue;
        float nearest = 1e9f;
        for (const Tank& t : g_world.tanks) nearest = std::min(nearest, (t.pos - p).len());
        if (nearest > bestD) { bestD = nearest; best = p; }
      }
      return best;
    }
    const float apart = std::min(1500.0f, (g_world.hi.x - g_world.lo.x) * 0.2f);
    Vec2 best = rampage ? g_world.lootPos(400.0f) : g_world.randomPos(800.0f);
    for (int tries = 0; tries < 40; ++tries) {
      const Vec2 p = rampage ? g_world.lootPos(400.0f) : g_world.randomPos(800.0f);
      if (!g_world.freeSpot(p, 60.0f) && !rampage) continue;
      if (rampage && g_world.inWall(p, 60.0f)) continue;
      float nearest = 1e9f;
      for (const Tank& t : g_world.tanks) nearest = std::min(nearest, (t.pos - p).len());
      if (nearest > apart) return p;
      best = p;
    }
    return best;
  };
  g_world.addTank(1, spawnPos(), false);
  for (int i = 0; i < bots; ++i) g_world.addTank(2 + i, spawnPos(), true);
  if (g_sandOn && !g_menu && g_sand.tier >= 0)  // sandbox: every bot the same tier
    for (Tank& t : g_world.tanks) { if (t.isBot) botSeed(g_world, t, g_sand.tier); }
  assignRoster(g_world);  // skill tiers: better ones are rarer
  g_world.tanks[0].ghost = g_ghostOpt && !g_menu;
  if (g_admin && !g_menu && !rampage) g_world.tanks[0].held = Power::CocoBarrage;  // the admin's own superpower
  g_nextDevBot = 5001;
  if (rampage)
    for (Tank& t : g_world.tanks) { t.held = Power::Rocket; t.cdScale = kRampageCd; }
  g_world.spawnGuardians();
  g_spectateId = 0;
  g_world.godId = g_godOpt ? 1 : 0;
  // Some bots wear shop cosmetics (render only; the sim never sees looks). The page asks for the slot of each bot
  // (tonk_look_slot) so names, titles and the kill feed match what is drawn.
  g_lookSlot.clear();
  uint32_t h = seed * 2654435761u + 12345u;
  for (const Tank& t : g_world.tanks) {
    if (t.id == 1 || t.guardian || t.mob) continue;
    h = h * 1664525u + 1013904223u;
    pickLook(t, h);
  }
  EM_ASM({ if (window.tonkMatchStart) window.tonkMatchStart($0, $1); }, g_world.mapId, mode);
  resetClientState();
  g_boxSeen.clear();
}

// Home screen background: a bots-only match; our own tank is out of play and the camera follows a bot.
static void setupMenuMatch() {
  setupMatch(30, false, (uint32_t)emscripten_get_now() | 1u);
  g_world.tanks[0].alive = false;
  g_world.tanks[0].place = 0;
  cycleSpectate(1);
  resetClientState();
}

// ---------------- Tick bookkeeping ----------------
static void snapshot() {
  const auto& T = g_world.tanks;
  g_tsnap.resize(T.size());
  for (size_t i = 0; i < T.size(); ++i) {
    TankSnap& s = g_tsnap[i];
    s.pos = T[i].pos; s.aim = T[i].aim; s.health = T[i].health; s.cooldown = T[i].cooldown; s.alive = T[i].alive;
    std::memcpy(s.stat, T[i].stat, sizeof(s.stat)); s.held = T[i].held; s.active = T[i].active; s.recharge = T[i].recharge;
    s.relic[0] = T[i].relic[0]; s.relic[1] = T[i].relic[1];
  }
  g_bsnap.clear(); g_bprev.clear();
  for (const Bullet& b : g_world.bullets) {
    g_bsnap.push_back({b.id, b.owner, b.pos, b.radius, b.rangeLeft, b.vel.len() * kDt, (int)b.kind, b.range0});
    g_bprev[b.id] = b.pos;
  }
  g_mprev.clear();
  for (const Rocket& r : g_world.rockets) g_mprev[r.id] = r.pos;
  for (const Drone& d : g_world.drones) g_mprev[d.id] = d.pos;
  g_usnap.clear();
  for (const Upgrade& u : g_world.upgrades) g_usnap.push_back({u.id, u.pos, (int)u.stat, (int)u.rarity});
  g_csnap.clear();
  for (const Crate& c : g_world.crates) g_csnap.push_back({c.id, c.pos, (int)c.type, 0});
  g_rsnap.clear();
  for (const RelicPickup& r : g_world.relics) g_rsnap.push_back({r.id, r.pos, (int)r.kind, 0});
}

static void notify(int kind, int a, int b) { EM_ASM({ if (window.tonkNotify) window.tonkNotify($0, $1, $2); }, kind, a, b); }

// Sound effects (web/sfx.js). kind: 0 superpower (a = power), 1 explosion (a = explosion kind), 2 hit, 3 kill,
// 4 upgrade pickup (a = rarity), 5 superpower pickup (a = power), 6 relic pickup (a = relic), 7 wall breaks.
// Your own sounds play at full volume; other tanks' get quieter with distance from the camera and pan left/right.
static void sfx(int kind, int a, Vec2 at, bool mine, float minVol = 0.0f) {
  if (g_menu) return;
  float vol = 1.0f, pan = 0.0f;
  if (!mine) {
    const Vec2 d = at - g_camPos;
    const float k = std::clamp(1.0f - d.len() / 1500.0f, 0.0f, 1.0f);
    vol = std::max(minVol, 0.75f * k * k);
    pan = std::clamp(d.x / 1000.0f, -1.0f, 1.0f) * 0.6f;
  }
  if (vol < 0.03f) return;
  EM_ASM({ if (window.tonkSfx) window.tonkSfx($0, $1, $2, $3); }, kind, a, vol, pan);
}

// A rocket blew a hole in a wall: slate chunks fly out and a puff of dust hangs in the gap.
static void wallBreakFx(Vec2 p, float len, float angDeg) {
  const float a = angDeg * kPi / 180.0f;
  const Vec2 u{std::cos(a), std::sin(a)}, n{-u.y, u.x};
  const int chunks = std::clamp((int)(len / 14.0f), 8, 26);
  for (int i = 0; i < chunks; ++i) {
    Fx f; f.pos = p + u * frange(-len * 0.5f, len * 0.5f);
    f.vel = (n * (frand() < 0.5f ? -1.0f : 1.0f) * frange(150, 420) + u * frange(-120, 120));
    f.drag = 3.5f; f.r = frange(6, 12); f.sides = 4 + (i % 3); f.rot = frange(0, 6.3f); f.spin = frange(-8, 8);
    f.life = frange(0.5f, 0.9f); f.ol = 3.0f; f.shrink = true; f.c = {0.48f, 0.46f, 0.60f};
    fxPush(g_fxHigh, f);
  }
  for (int i = 0; i < 7; ++i) {
    Fx s; s.pos = p + u * frange(-len * 0.45f, len * 0.45f); s.vel = n * frange(-40, 40); s.r = frange(22, 38); s.grow = 1.0f;
    s.life = frange(0.7f, 1.1f); s.a0 = 0.3f; s.c = {0.62f, 0.6f, 0.7f};
    fxPush(g_fxLow, s);
  }
}

// Explosion: a white-hot flash, a fireball of overlapping puffs that cool from yellow to orange to dark smoke and drift
// apart, a faint shock ring, fast sparks and chunks, and a scorch mark left on the ground.
// kind 1 rocket, 2 loot drone, 3 bomb, 4 singularity, 5 Sentry Robot.
static void explosionFx(Vec2 p, float R, int kind) {
  const bool violet = kind == 4;
  if (kind == 6)  // a coconut: shell pieces fly out
    for (int i = 0; i < 8; ++i) {
      const Vec2 d = frandDir();
      Fx f; f.pos = p; f.vel = d * frange(160, 320); f.drag = 2.5f; f.r = frange(4, 7); f.life = frange(0.5f, 0.8f);
      f.sides = 3; f.rot = frand() * 6.28f; f.c = {0.5f, 0.32f, 0.16f}; f.shrink = true;
      fxPush(g_fxHigh, f);
    }
  if (violet) {  // a singularity collapsing: a violet implosion, not fire
    Fx fl; fl.pos = p; fl.r = R * 0.6f; fl.grow = -0.6f; fl.life = 0.3f; fl.a0 = 0.8f; fl.c = {0.75f, 0.55f, 1.0f};
    fxPush(g_fxHigh, fl);
    for (int i = 0; i < 14; ++i) {
      const Vec2 d = frandDir();
      Fx f; f.pos = p + d * R * 1.4f; f.vel = d * (-R * 4.0f); f.drag = 3; f.r = 4; f.life = 0.35f; f.c = hueC(0.72f + 0.06f * frand()); f.shrink = true;
      fxPush(g_fxHigh, f);
    }
    return;
  }
  const float s = R / 140.0f;
  Fx scorch; scorch.pos = p; scorch.r = R * 0.55f; scorch.life = 3.5f; scorch.a0 = 0.22f; scorch.c = {0.16f, 0.13f, 0.12f};
  fxPush(g_fxLow, scorch);
  Fx fl; fl.pos = p; fl.r = R * 0.5f; fl.grow = 0.6f; fl.life = 0.12f; fl.a0 = 0.95f; fl.c = {1.0f, 0.98f, 0.9f};
  fxPush(g_fxHigh, fl);
  Fx ring; ring.pos = p; ring.sides = -3; ring.r = R * 0.5f; ring.aspect = 0.035f; ring.grow = 1.3f; ring.life = 0.28f; ring.a0 = 0.55f;
  ring.c = {1.0f, 0.93f, 0.8f};
  fxPush(g_fxHigh, ring);
  Fx core; core.pos = p; core.r = R * 0.38f; core.grow = 0.5f; core.life = 0.3f; core.a0 = 1.0f; core.shrink = true;
  core.c = {1.0f, 0.97f, 0.75f}; core.c2 = {1.0f, 0.5f, 0.12f}; core.fade2 = true;
  const int puffs = 9 + (int)(5 * s);
  for (int i = 0; i < puffs; ++i) {  // fireball: hot in the middle, cooling into smoke at the edge
    const Vec2 d = frandDir();
    const float k = frand();
    Fx f; f.pos = p + d * (R * 0.25f * k); f.vel = d * frange(R * 0.5f, R * 1.4f) + Vec2{0, -frange(10, 40)}; f.drag = 3.2f;
    f.r = frange(R * 0.2f, R * 0.34f); f.grow = 0.9f; f.life = frange(0.45f, 0.75f); f.a0 = 0.9f; f.age = -0.03f * k;
    f.c = mixC({1.0f, 0.85f, 0.4f}, {0.95f, 0.35f, 0.1f}, k); f.c2 = {0.3f, 0.27f, 0.28f}; f.fade2 = true; f.a0 = 0.8f;
    fxPush(g_fxHigh, f);
  }
  fxPush(g_fxHigh, core);
  for (int i = 0; i < 7; ++i) {  // smoke that lingers and rises
    Fx sm; sm.pos = p + frandDir() * frange(0, R * 0.45f); sm.vel = frandDir() * 25.0f + Vec2{0, -35.0f}; sm.r = frange(R * 0.22f, R * 0.32f);
    sm.grow = 1.0f; sm.life = frange(1.1f, 1.7f); sm.a0 = 0.32f; sm.age = -0.15f; sm.c = {0.42f, 0.39f, 0.4f};
    fxPush(g_fxHigh, sm);
  }
  for (int i = 0; i < 14; ++i) {  // sparks
    const Vec2 d = frandDir();
    Fx f; f.pos = p; f.vel = d * frange(R * 3.0f, R * 6.0f); f.drag = 4.0f; f.sides = -2; f.r = frange(5, 9); f.aspect = 0.3f;
    f.rot = std::atan2(d.y, d.x); f.life = frange(0.25f, 0.45f); f.c = {1.0f, frange(0.7f, 0.9f), 0.35f}; f.shrink = true;
    fxPush(g_fxHigh, f);
  }
  for (int i = 0; i < 6; ++i) {  // dark chunks
    Fx f; f.pos = p; f.vel = frandDir() * frange(R * 1.6f, R * 3.2f); f.drag = 2.5f; f.sides = 3 + (i % 2); f.r = frange(3, 5.5f);
    f.spin = frange(-10, 10); f.life = frange(0.5f, 0.8f); f.c = {0.22f, 0.19f, 0.22f}; f.shrink = true;
    fxPush(g_fxHigh, f);
  }
}

// Teleport: where you left, the tank's light is sucked into a point; where you arrive, a column of light, two rings
// and a burst of sparkles.
static void teleportFx(Vec2 p, bool arrive, Col c) {
  const Col c2 = mixC(c, {1, 1, 1}, 0.55f);
  if (!arrive) {
    for (int i = 0; i < 16; ++i) {
      const Vec2 d = frandDir();
      Fx f; f.pos = p + d * frange(55, 90); f.vel = d * -frange(260, 380); f.drag = 1.5f; f.sides = -2; f.r = 6; f.aspect = 0.35f;
      f.rot = std::atan2(d.y, d.x); f.life = 0.22f; f.c = c2; f.shrink = true;
      fxPush(g_fxHigh, f);
    }
    Fx im; im.pos = p; im.sides = -3; im.r = 46; im.aspect = 0.07f; im.grow = -0.85f; im.life = 0.25f; im.c = c;
    fxPush(g_fxHigh, im);
    Fx core; core.pos = p; core.r = 22; core.grow = -0.9f; core.life = 0.25f; core.a0 = 0.8f; core.c = c2;
    fxPush(g_fxHigh, core);
    return;
  }
  Fx col; col.pos = p; col.sides = -2; col.r = 80; col.aspect = 0.2f; col.rot = kPi * 0.5f; col.life = 0.3f; col.a0 = 0.7f; col.c = c2; col.off = {0, -40};
  fxPush(g_fxHigh, col);
  Fx fl; fl.pos = p; fl.r = 34; fl.grow = 0.8f; fl.life = 0.25f; fl.a0 = 0.8f; fl.c = {1, 1, 1};
  fxPush(g_fxHigh, fl);
  for (int k = 0; k < 2; ++k) {
    Fx r; r.pos = p; r.sides = -3; r.r = 22; r.aspect = 0.12f; r.grow = 2.6f; r.life = 0.4f; r.age = -0.08f * k; r.c = k ? c2 : c;
    fxPush(g_fxHigh, r);
  }
  for (int i = 0; i < 14; ++i) {
    Fx s; s.pos = p; s.vel = frandDir() * frange(140, 300); s.drag = 3.5f; s.sides = -4; s.r = frange(4, 7); s.spin = frange(-8, 8);
    s.life = frange(0.4f, 0.6f); s.c = i % 2 ? c : c2; s.shrink = true;
    fxPush(g_fxHigh, s);
  }
}

// A loot crate breaks: planks and splinters fly, dust hangs. A Legendary Crate also throws gold sparkles and a ring.
static void boxBreakFx(Vec2 p, int kind) {
  const bool legend = kind == (int)BoxKind::Legendary;
  const Col wood = legend ? Col{0.95f, 0.72f, 0.18f} : Col{0.62f, 0.4f, 0.22f};
  const float R = boxRadius((BoxKind)kind);
  for (int i = 0; i < 12; ++i) {
    const Vec2 d = frandDir();
    Fx f; f.pos = p + d * frange(0, R * 0.5f); f.vel = d * frange(160, 380); f.drag = 3.2f; f.sides = -1; f.r = frange(7, 12) * R / 30.0f;
    f.aspect = 0.32f; f.rot = frange(0, 6.3f); f.spin = frange(-9, 9); f.life = frange(0.5f, 0.8f); f.ol = 2.5f; f.c = wood; f.shrink = true;
    fxPush(g_fxHigh, f);
  }
  for (int i = 0; i < 5; ++i) {
    Fx s; s.pos = p + frandDir() * frange(0, R * 0.6f); s.vel = frandDir() * 30.0f; s.r = frange(R * 0.5f, R * 0.8f); s.grow = 0.8f;
    s.life = frange(0.6f, 0.9f); s.a0 = 0.3f; s.c = {0.7f, 0.64f, 0.56f};
    fxPush(g_fxLow, s);
  }
  if (legend) {
    Fx r; r.pos = p; r.sides = -3; r.r = 40; r.aspect = 0.1f; r.grow = 3.0f; r.life = 0.55f; r.c = {1.0f, 0.82f, 0.25f};
    fxPush(g_fxHigh, r);
    for (int i = 0; i < 18; ++i) {
      Fx s; s.pos = p; s.vel = frandDir() * frange(220, 480); s.drag = 2.4f; s.sides = -4; s.r = frange(6, 10); s.spin = frange(-8, 8);
      s.life = frange(0.7f, 1.1f); s.c = i % 3 ? Col{1.0f, 0.85f, 0.3f} : Col{1, 1, 1}; s.ol = 2; s.shrink = true;
      fxPush(g_fxHigh, s);
    }
  }
}

static Col bulletCol(const Tank* o) {
  if (!o) return kEnemyCol;
  return lookOf(*o).c1;
}

static void detectEvents() {
  auto& T = g_world.tanks;
  if (g_tfx.size() < T.size()) g_tfx.resize(T.size(), TankFx{});  // a Sentry Robot joined
  for (size_t i = 0; i < T.size() && i < g_tsnap.size(); ++i) {
    const TankSnap& s = g_tsnap[i];
    const Tank& t = T[i];
    TankFx& f = g_tfx[i];
    if (s.alive && !t.alive) {
      if (nearView(s.pos)) {
        const Look lk = lookOf(t);
        tankGhost(s.pos, s.aim, lk, t.radius / kTankRadius);
        deathBurst(lk.death, s.pos, lk);
        for (const Tank& k : T)
          if (k.id == t.killedBy && k.id != t.id) { const Look kl = lookOf(k); if (kl.kill) killFx(kl.kill, s.pos, kl); }
      }
      const Tank* kt = g_world.find(t.killedBy);
      const bool byMe = t.killedBy == 1 || (kt && kt->sentryOf == 1);
      if (!g_menu) EM_ASM({ if (window.tonkKill) window.tonkKill($0, $1, $2, $3); }, (int)t.killedBy, (int)t.id, (int)t.sentryOf,
                          kt ? (int)kt->sentryOf : 0);
      if (t.mob && byMe && !g_menu) g_myMobs++;
      if (byMe && t.id != 1 && !npc(t) && !g_menu) { notify(0, (int)t.id, 0); sfx(3, 0, s.pos, true); }
      else if (byMe && t.id != 1 && t.killedBy == 1 && !g_menu) sfx(3, 0, s.pos, true);
    }
    if (!s.alive && t.alive) f = TankFx{};  // respawned
    if (s.alive && t.alive) {
      if (t.health < s.health - 0.01f) {
        f.flash = 0.11f;
        if (t.lastHitBy == 1 && t.id != 1) sfx(2, 0, t.pos, true);  // you hit someone
      }
      if (t.cooldown > s.cooldown + 0.02f) {  // fired this tick
        f.recoil = 1.0f;
        const int mz = lookOf(t).muzzle;
        const float sc = t.radius / kTankRadius;
        if (mz && nearView(t.pos) && canSpot(t)) muzzleFx(mz, t.pos + Vec2{std::cos(t.aim), std::sin(t.aim)} * (62.0f * sc), t.aim);
      }
    }
    if (t.id == 1 && t.alive && s.alive && !g_menu) {
      for (int k = 0; k < kStatCount; ++k)
        if (t.stat[k] > s.stat[k]) { g_myUpgrades++; notify(1, k, t.stat[k]); sfx(4, t.stat[k] - 1, t.pos, true); }
      if (t.held != s.held && t.held != Power::None) { notify(2, (int)t.held, 0); sfx(5, (int)t.held, t.pos, true); }
      if ((s.active == Power::None && t.active != Power::None) || (s.active == Power::None && t.recharge > s.recharge + 0.5f)) g_myPowerUses++;
      for (int k = 0; k < 2; ++k)
        if (t.relic[k] != Relic::None && t.relic[k] != s.relic[0] && t.relic[k] != s.relic[1]) {
          notify(3, (int)t.relic[k], k);
          sfx(6, (int)t.relic[k], t.pos, true);
        }
    }
  }
  // Bullets that vanished: at the end of their range they swell and fade out; on a hit they pop quickly.
  {
    std::unordered_set<uint32_t> live;
    live.reserve(g_world.bullets.size() * 2 + 1);
    for (const Bullet& b : g_world.bullets) live.insert(b.id);
    for (const BulletSnap& b : g_bsnap) {
      if (live.count(b.id) || !nearView(b.pos)) continue;
      if (b.left <= b.step * 1.05f) continue;  // flew its whole range: it has already faded out
      const float k = b.full > 0 ? std::clamp(1.0f - b.left / b.full, 0.0f, 1.0f) : 0.0f;
      Fx f; f.pos = b.pos; f.r = b.r * (1.0f + kBulletGrow * k); f.ol = 3.5f;
      f.c = b.kind == (int)BulletKind::Bolt ? Col{0.75f, 0.95f, 1.0f} : b.kind == (int)BulletKind::Guardian ? kGuardBullet : bulletCol(g_world.find(b.owner));
      f.grow = 0.5f; f.life = 0.12f; f.a0 = bulletAlpha(k);
      fxPush(g_fxHigh, f);
    }
  }
  // Pickups that were taken.
  {
    std::unordered_set<uint32_t> live;
    for (const Upgrade& u : g_world.upgrades) live.insert(u.id);
    for (const PickSnap& u : g_usnap) {
      if (live.count(u.id) || !nearView(u.pos)) continue;
      bool carried = false;  // a Loot Drone picked it up: it just flies off, no pop
      for (const Drone& d : g_world.drones) carried |= d.carryStat >= 0 && (d.pos - u.pos).len() < 12.0f;
      if (carried) continue;
      const Col c = upgradeCols(u.rarity).main;
      Fx f; f.pos = u.pos; f.sides = -3; f.r = 24; f.aspect = 0.12f; f.grow = 1.0f; f.life = 0.25f; f.c = c;
      fxPush(g_fxLow, f);
      for (int k = 0; k < 6; ++k) {
        Fx sp; sp.pos = u.pos; sp.vel = frandDir() * frange(120, 220); sp.drag = 4; sp.r = 3.5f; sp.life = 0.3f; sp.shrink = true; sp.c = c;
        fxPush(g_fxLow, sp);
      }
    }
    live.clear();
    for (const Crate& c : g_world.crates) live.insert(c.id);
    for (const PickSnap& c : g_csnap) {
      if (live.count(c.id) || !nearView(c.pos)) continue;
      Fx f; f.pos = c.pos; f.r = 28; f.sides = 4; f.grow = 0.6f; f.life = 0.22f; f.ol = 3.5f; f.c = powerColor((Power)c.kind);
      fxPush(g_fxLow, f);
    }
    live.clear();
    for (const RelicPickup& r : g_world.relics) live.insert(r.id);
    for (const PickSnap& r : g_rsnap) {
      if (live.count(r.id) || !nearView(r.pos)) continue;
      Fx f; f.pos = r.pos; f.r = 28; f.sides = 6; f.grow = 0.7f; f.life = 0.25f; f.ol = 3.5f; f.c = relicColor((Relic)r.kind);
      fxPush(g_fxLow, f);
    }
  }
  // Things the world reported this tick.
  for (const Event& e : g_world.events) {
    switch (e.type) {
      case EvType::Boom:
        if (nearView(e.pos)) explosionFx(e.pos, e.r, (int)e.b);
        if (e.b == 1 || e.b == 3 || e.b == 5 || e.b == 6) sfx(1, e.b == 6 ? 3 : (int)e.b, e.pos, false, e.a == 1 ? 0.55f : 0.0f);  // rockets, bombs, robots, coconuts
        break;
      case EvType::BoxBroken:
        if (nearView(e.pos)) boxBreakFx(e.pos, (int)e.b);
        if (!g_menu) {
          const Tank* k = g_world.find(e.a);
          if (e.a == 1 || (k && k->sentryOf == 1)) { g_myCrates++; if (e.b == (int)BoxKind::Legendary) g_myLegend++; }
        }
        if (e.b == (int)BoxKind::Legendary) { g_legendMark = false; if (!g_menu) notify(10, e.a == 1 ? 1 : 0, 0); }
        break;
      case EvType::LegendWarn:
        g_legendMark = true; g_legendPos = e.pos;
        if (!g_menu) notify(8, 0, 0);
        break;
      case EvType::LegendDrop:
        if (nearView(e.pos)) {
          Fx r; r.pos = e.pos; r.sides = -3; r.r = 50; r.aspect = 0.08f; r.grow = 2.5f; r.life = 0.6f; r.c = {1.0f, 0.82f, 0.25f};
          fxPush(g_fxHigh, r);
        }
        if (!g_menu) notify(9, 0, 0);
        break;
      case EvType::TempleEnter:
        if (e.a == 1 && !g_menu) notify(11, (int)e.b, 0);
        if (nearView(e.pos)) teleportFx(e.pos, false, {0.95f, 0.72f, 0.3f});
        break;
      case EvType::TempleExit: {
        if (e.a == 1 && !g_menu) notify(12, (int)e.b, (int)e.r);
        if (nearView(e.pos)) teleportFx(e.pos, true, {0.95f, 0.72f, 0.3f});
        break;
      }
      case EvType::TempleDenied:
        if (e.a == 1 && !g_menu) notify(14, 0, 0);
        break;
      case EvType::TempleCleared: {
        const Tank* k = g_world.find(e.a);
        if (!g_menu && (e.a == 1 || (k && k->sentryOf == 1))) { g_myMages++; notify(13, 0, 0); }
        break;
      }
      case EvType::PowerUsed: {
        const Tank* u = g_world.find(e.a);
        if (u && (e.a == 1 || canSpot(*u))) sfx(0, (int)e.b, e.pos, e.a == 1);
        break;
      }
      case EvType::WallBreak:
        if (nearView(e.pos)) wallBreakFx(e.pos, e.r, (float)e.a - 360.0f);
        sfx(7, 0, e.pos, false);
        break;
      case EvType::DroneLost:
        if (nearView(e.pos)) explosionFx(e.pos, 40.0f, 2);
        if (e.a == 1 && !g_menu) notify(4, (int)e.b, 0);
        break;
      case EvType::Dodged: {
        if (e.a == 1 && !g_menu) notify(5, 0, 0);
        Fx f; f.pos = e.pos; f.sides = -3; f.r = 34; f.aspect = 0.1f; f.grow = 1.2f; f.life = 0.3f; f.c = {1.0f, 0.8f, 0.25f};
        fxPush(g_fxHigh, f);
        break;
      }
      case EvType::DodgeMissed: if (e.a == 1 && !g_menu) notify(6, 0, 0); break;
      case EvType::GuardianDrop: if (e.a == 1 && !g_menu) notify(7, (int)e.b, 0); break;
      case EvType::Teleport:
        if (nearView(e.pos)) teleportFx(e.pos, e.r > 0.5f, powerColor(Power::Teleport));
        break;
      case EvType::Snare:
        if (nearView(e.pos)) {
          Fx r; r.pos = e.pos; r.sides = -3; r.r = e.r * 0.3f; r.aspect = 0.06f; r.grow = 2.4f; r.life = 0.5f; r.c = {0.25f, 0.75f, 0.35f};
          fxPush(g_fxHigh, r);
          for (int i = 0; i < 18; ++i) {
            const Vec2 d = frandDir();
            Fx f; f.pos = e.pos + d * (e.r * frand()); f.vel = Vec2{0, -40}; f.r = frange(5, 9); f.life = frange(0.8f, 1.4f);
            f.sides = -2; f.aspect = 0.45f; f.rot = frand() * 6.28f; f.c = hueC(0.3f + 0.05f * frand()); f.shrink = true;
            fxPush(g_fxHigh, f);
          }
        }
        break;
      case EvType::Blinded:
        if (nearView(e.pos)) {
          Fx f; f.pos = e.pos; f.sides = -3; f.r = 40; f.aspect = 0.12f; f.grow = 3.0f; f.life = 0.5f; f.c = {0.15f, 0.1f, 0.2f};
          fxPush(g_fxHigh, f);
        }
        break;
    }
  }
}

// Interpolated pose of tank i between the previous and current tick.
static Vec2 tankPos(size_t i) {
  const Tank& t = g_world.tanks[i];
  if (i >= g_tsnap.size()) return t.pos;
  const Vec2 p = g_tsnap[i].pos;
  if ((t.pos - p).len() > 250.0f) return t.pos;  // respawn / teleport: snap
  return p + (t.pos - p) * g_alpha;
}
static float tankAim(size_t i) {
  const Tank& t = g_world.tanks[i];
  if (i >= g_tsnap.size()) return t.aim;
  return g_tsnap[i].aim + angleDiff(g_tsnap[i].aim, t.aim) * g_alpha;
}
static Vec2 lerpPrev(const std::unordered_map<uint32_t, Vec2>& m, uint32_t id, Vec2 now) {
  auto it = m.find(id);
  if (it == m.end() || (now - it->second).len() > 300.0f) return now;
  return it->second + (now - it->second) * g_alpha;
}

// ---------------- World rendering ----------------
// ---- Icon glyphs (generated from one spec, the same as the SVG icons in web/ui.js) ----
// Coordinates are in a 16x16 design box around (x, y), scaled by s. Everything is flat (no outline).
struct Glyph {
  float x, y, s;
  static void flat(Inst& i) { i.ol = 0; }
  void seg(float x1, float y1, float x2, float y2, float w, Col c) const {
    const float dx = (x2 - x1) * s, dy = (y2 - y1) * s, l = std::sqrt(dx * dx + dy * dy);
    flat(capsule(x + (x1 + x2) * 0.5f * s, y + (y1 + y2) * 0.5f * s, l * 0.5f + w * s, w * s, std::atan2(dy, dx), c));
  }
  void cir(float cx, float cy, float r, Col c) const { flat(circle(x + cx * s, y + cy * s, r * s, c)); }
  void ring(float cx, float cy, float r, float w, Col c) const { flat(ringI(x + cx * s, y + cy * s, r * s, w * s, c)); }
  void rect(float cx, float cy, float hw, float hh, float rot, Col c) const { flat(rectI(x + cx * s, y + cy * s, hw * s, hh * s, rot, c)); }
  void cap(float cx, float cy, float hw, float hh, float rot, Col c) const { flat(capsule(x + cx * s, y + cy * s, hw * s, hh * s, rot, c)); }
  void poly(float cx, float cy, float r, float rot, int n, Col c) const { flat(::poly(x + cx * s, y + cy * s, r * s, rot, n, c)); }
};
// set 1 = superpowers, 2 = relics, 3 = stat upgrades
static void drawGlyph(int set, int k, float x, float y, float s, Col c, Col b) {
  const Glyph g{x, y, s};
  switch (set * 100 + k) {
    case 101:
      g.seg(-6, -5, -1, 0, 1.7f, c);
      g.seg(-1, 0, -6, 5, 1.7f, c);
      g.seg(0, -5, 5, 0, 1.7f, c);
      g.seg(5, 0, 0, 5, 1.7f, c);
      break;
    case 102:
      g.rect(0, -2.2f, 5.2f, 3.6f, 0, c);
      g.poly(0, 1.6f, 6, 0, 3, c);
      g.rect(0, -2, 3, 1.9f, 0, b);
      g.poly(0, 1, 3.5f, 0, 3, b);
      break;
    case 103:  // a rocket, side on: body, two swept fins, a window and a band
      g.cap(0.8f, 0, 6.0f, 2.8f, 0, c);
      g.poly(-4.3f, -3.0f, 2.7f, kPi, 3, c);
      g.poly(-4.3f, 3.0f, 2.7f, 0, 3, c);
      g.cir(2.6f, 0, 1.25f, b);
      g.rect(-1.6f, 0, 0.55f, 2.8f, 0, b);
      break;
    case 104:
      g.cir(-6.2f, 6, 1.1f, c);
      g.cir(-6.4f, 2.4f, 1.2f, c);
      g.cir(-5, -0.9f, 1.3f, c);
      g.cir(-2.4f, -3.2f, 1.4f, c);
      g.cap(3.2f, -4.4f, 4.2f, 2.2f, -0.2f, c);
      g.rect(0.6f, -3.9f, 2.6f, 1, 1.3708f, c);
      g.cir(4.3f, -4.6f, 0.9f, b);
      break;
    case 105:
      g.cap(-1.5f, -4.6f, 4.8f, 1.8f, 0, c);
      g.cap(1.5f, 0, 4.8f, 1.8f, 0, c);
      g.cap(-1.5f, 4.6f, 4.8f, 1.8f, 0, c);
      break;
    case 106:
      g.cap(0, 0, 7.2f, 1.9f, 1.5708f, c);
      g.rect(0, -0.8f, 7.4f, 1.6f, 0, c);
      g.rect(0, 5.4f, 3.4f, 1.1f, 0, c);
      break;
    case 107:
      g.seg(0, 6, -5.5f, -3.6f, 1.2f, c);
      g.seg(0, 6, 0, -5.2f, 1.2f, c);
      g.seg(0, 6, 5.5f, -3.6f, 1.2f, c);
      g.cir(-5.5f, -3.6f, 2.2f, c);
      g.cir(0, -5.2f, 2.2f, c);
      g.cir(5.5f, -3.6f, 2.2f, c);
      break;
    case 108:
      g.seg(2.8f, -7, -2.4f, -0.6f, 1.8f, c);
      g.seg(-2.4f, -0.6f, 2.4f, 0.6f, 1.8f, c);
      g.seg(2.4f, 0.6f, -2.8f, 7, 1.8f, c);
      break;
    case 109:
      g.poly(3.2f, 0, 5, -1.5708f, 3, c);
      g.seg(-7, -3.6f, -2.6f, -3.6f, 1.1f, c);
      g.seg(-8, 0, -2, 0, 1.1f, c);
      g.seg(-7, 3.6f, -2.6f, 3.6f, 1.1f, c);
      break;
    case 110:
      g.rect(0, 4, 6.2f, 2.6f, 0, c);
      g.rect(0, -4.2f, 1.5f, 3.6f, 0, c);
      g.cir(0, 1, 4, c);
      g.cir(0, 1, 1.6f, b);
      break;
    case 111:
      g.ring(-3.6f, 3.6f, 3.3f, 1, c);
      g.cir(3.6f, -3.6f, 3.3f, c);
      g.cir(0, 0, 0.9f, c);
      break;
    case 112:
      g.cir(0, -1.6f, 5.6f, c);
      g.rect(0, 2.4f, 5.6f, 3.8f, 0, c);
      g.cir(-2.8f, 6.4f, 1.4f, b);
      g.cir(2.8f, 6.4f, 1.4f, b);
      g.cir(-2.1f, -1.8f, 1.3f, b);
      g.cir(2.1f, -1.8f, 1.3f, b);
      break;
    case 113:
      g.ring(0, 0, 6.2f, 0.9f, c);
      g.cir(0, 0, 2.8f, c);
      g.cir(5.12f, -3.5f, 1.6f, c);
      g.cir(0.44f, 6.18f, 1.6f, c);
      g.cir(-5.56f, -2.74f, 1.6f, c);
      break;
    case 114:
      g.cap(0, 0, 7.2f, 3.9f, 0, c);
      g.cir(0, 0, 2.7f, b);
      g.cir(0, 0, 1.3f, c);
      break;
    case 115:  // Sentry Robot: a robot head with an antenna
      g.rect(0, 1.6f, 5.6f, 4.2f, 0, c);
      g.rect(-6.6f, 1.6f, 1.0f, 2.2f, 0, c);
      g.rect(6.6f, 1.6f, 1.0f, 2.2f, 0, c);
      g.seg(0, -2.6f, 0, -5.4f, 0.8f, c);
      g.cir(0, -6.3f, 1.5f, c);
      g.cir(-2.3f, 0.8f, 1.35f, b);
      g.cir(2.3f, 0.8f, 1.35f, b);
      g.rect(0, 4.0f, 2.6f, 0.6f, 0, b);
      break;
    case 116:  // Coconut Barrage: a coconut with its three eyes
      g.cir(0, 0.6f, 6.4f, c);
      g.cir(-2.1f, -0.9f, 1.1f, b);
      g.cir(2.1f, -0.9f, 1.1f, b);
      g.cir(0, 1.9f, 1.1f, b);
      break;
    case 117:  // Ivy Snare: a leaf on a curling vine
      g.cap(1.2f, -1.6f, 4.6f, 2.6f, -0.7854f, c);
      g.seg(-5.5f, 5.5f, -1.2f, 1.2f, 0.9f, c);
      g.seg(-1.2f, 1.2f, 3.6f, -3.6f, 0.45f, b);
      g.ring(-4.2f, 2.0f, 1.8f, 0.6f, c);
      break;
    case 201:
      g.seg(-5, 5, 0.8f, -0.8f, 1.6f, c);
      g.ring(3, -3, 3.1f, 1.3f, c);
      g.rect(5, -5, 1.6f, 1.6f, 0.7854f, b);
      break;
    case 202:
      g.seg(-4.6f, -4.6f, 4.6f, 4.6f, 1, c);
      g.seg(-4.6f, 4.6f, 4.6f, -4.6f, 1, c);
      g.ring(-4.6f, -4.6f, 2.4f, 0.8f, c);
      g.ring(4.6f, -4.6f, 2.4f, 0.8f, c);
      g.ring(-4.6f, 4.6f, 2.4f, 0.8f, c);
      g.ring(4.6f, 4.6f, 2.4f, 0.8f, c);
      g.cir(0, 0, 2.6f, c);
      break;
    case 203:
      g.rect(0, 1.2f, 6.2f, 5.2f, 0, c);
      g.rect(0, 1.2f, 1.1f, 5.2f, 0, b);
      g.rect(0, -1.6f, 6.2f, 0.6f, 0, b);
      break;
    case 204:
      g.ring(0, 0, 6.4f, 0.9f, c);
      g.ring(0, 0, 3.3f, 0.8f, c);
      g.seg(0, 0, 4.5f, -4.5f, 0.9f, c);
      g.cir(-2.4f, 2.6f, 1.3f, c);
      break;
    case 300:  // Move Speed: three rising bars and an arrow
      g.rect(-6.3f, 0, 0.95f, 2.2f, 0, c);
      g.rect(-3.6f, 0, 0.95f, 3.2f, 0, c);
      g.rect(-0.9f, 0, 0.95f, 4.2f, 0, c);
      g.poly(3.3f, 0, 4.7f, -1.5708f, 3, c);
      break;
    case 301:  // Bullet Speed: a bullet with speed lines
      g.cir(2.8f, 0, 3.7f, c);
      g.seg(-7.4f, -3.2f, -3.2f, -3.2f, 0.9f, c);
      g.seg(-8, 0, -2.2f, 0, 0.9f, c);
      g.seg(-7.4f, 3.2f, -3.2f, 3.2f, 0.9f, c);
      break;
    case 302:
      g.ring(0, 0, 5, 1.2f, c);
      g.cir(0, 0, 1.4f, c);
      g.rect(0, -7.4f, 1, 2, 0, c);
      g.rect(0, 7.4f, 1, 2, 0, c);
      g.rect(-7.4f, 0, 2, 1, 0, c);
      g.rect(7.4f, 0, 2, 1, 0, c);
      break;
    case 303:
      g.cir(-2.7f, -1.3f, 3.1f, c);
      g.cir(2.7f, -1.3f, 3.1f, c);
      g.poly(0, 1.1f, 5.5f, 0, 4, c);
      break;
    case 304:  // Fire Rate: three bullets in a volley
      g.cir(-1.0f, -4.3f, 2.5f, c);
      g.cir(4.2f, 0, 2.5f, c);
      g.cir(-1.0f, 4.3f, 2.5f, c);
      g.seg(-7.2f, -4.3f, -4.4f, -4.3f, 0.75f, c);
      g.seg(-2.2f, 0, 0.8f, 0, 0.75f, c);
      g.seg(-7.2f, 4.3f, -4.4f, 4.3f, 0.75f, c);
      break;
    case 305:
      g.poly(0, 0, 7.4f, 0, 4, c);
      g.poly(0, 0, 7.4f, 0.7854f, 4, c);
      break;
    case 306:  // Field of View: an eye with three rays
      g.cap(0, 1.2f, 6.6f, 3.6f, 0, c);
      g.cir(0, 1.2f, 2.7f, b);
      g.cir(0, 1.2f, 1.4f, c);
      g.seg(0, -6.6f, 0, -4.4f, 0.7f, c);
      g.seg(-5.0f, -5.4f, -3.8f, -3.9f, 0.7f, c);
      g.seg(5.0f, -5.4f, 3.8f, -3.9f, 0.7f, c);
      break;
    default: break;
  }
}
static void powerGlyph(Power p, float x, float y, float s, Col c, Col bg) { drawGlyph(1, (int)p, x, y, s, c, bg); }
static void relicGlyph(Relic r, float x, float y, float s, Col c, Col bg) { drawGlyph(2, (int)r, x, y, s, c, bg); }

static void fadeFrom(size_t start, float a) { for (size_t i = start; i < g_insts.size(); ++i) g_insts[i].alpha *= a; }

// Rocket Rampage: the rocket is the barrel. Drawn in the barrel's colors, 1.25x the size of a normal rocket, centered at c.
static void barrelRocket(Vec2 c, float a, float sc, const Look& lk, float flash) {
  const Vec2 d{std::cos(a), std::sin(a)}, n{-d.y, d.x};
  auto at = [&](float f, float s) { return c + d * (f * sc) + n * (s * sc); };
  const Col dark = mixC(lk.b1, {0, 0, 0}, 0.32f);
  for (float s : {-1.0f, 1.0f}) { const Vec2 q = at(-11, 7.5f * s); rectI(q.x, q.y, 7.5f * sc, 2.6f * sc, a - 0.6f * s, dark).ol = 2.2f; }
  Inst& b = capsule(c.x, c.y, 18 * sc, 6.2f * sc, a, lk.b1);
  b.ol = 2.4f; b.flash = flash;
  paint(b, lk.bpat, lk.b2);
  { const Vec2 q = at(12.5f, 0); circle(q.x, q.y, 5.6f * sc, dark).ol = 0; }
  { const Vec2 q = at(-3, 0); rectI(q.x, q.y, 1.6f * sc, 6.2f * sc, a, dark).ol = 0; }
}

// ---------------- Dev panel lines (red: targets, blue: teams) ----------------
static void linkNote(int code) {  // tell the page: how many lines, which tank is picked, what just happened
  int red = 0, blue = 0;
  for (const World::Link& l : g_world.links) (l.team ? blue : red)++;
  EM_ASM({ if (window.tonkLinks) tonkLinks($0, $1, $2, $3); }, red, blue, (int)g_linkSel, code);
}
static Vec2 mouseWorld() { return g_camPos + Vec2{(mouseX - canvasW * 0.5f) / g_zoom, (mouseY - canvasH * 0.5f) / g_zoom}; }
static uint32_t linkTankAt() {  // the living tank under the mouse (a little forgiving)
  const Vec2 p = mouseWorld();
  uint32_t best = 0;
  float bd = std::max(16.0f, 12.0f / g_zoom);
  for (size_t i = 0; i < g_world.tanks.size(); ++i) {
    const Tank& t = g_world.tanks[i];
    if (!t.alive) continue;
    const float d = (tankPos(i) - p).len() - t.radius;
    if (d < bd) { bd = d; best = t.id; }
  }
  return best;
}
// codes: 1/2 a red line added/removed, 3/4 a blue line added/removed, 5 red lines start at a bot, 6 not Arena
// Guardians, 7 a tank picked.
static void linkMake(uint32_t a, uint32_t b) {
  const bool team = g_linkMode == 2;
  Tank* A = g_world.find(a);
  Tank* B = g_world.find(b);
  if (!A || !B) return;
  if (A->guardian || B->guardian) { linkNote(6); return; }
  if (!team && !A->isBot) { linkNote(5); return; }
  const bool added = g_world.toggleLink(a, b, team);
  A->mind.decideTimer = 0;  // think again right away
  if (team) B->mind.decideTimer = 0;
  linkNote(team ? (added ? 3 : 4) : (added ? 1 : 2));
}
static void linkClick(bool down) {
  if (down) { g_linkDown = true; g_linkPress = linkTankAt(); return; }
  if (!g_linkDown) return;  // (the button went down somewhere else, like the dev panel)
  g_linkDown = false;
  const uint32_t at = linkTankAt(), press = g_linkPress;
  g_linkPress = 0;
  if (!press) {  // clicked the ground: let go of the picked tank
    if (!at && g_linkSel) { g_linkSel = 0; linkNote(0); }
    return;
  }
  if (at && at != press) { linkMake(press, at); g_linkSel = press; linkNote(-1); return; }  // dragged a line
  if (!at) return;  // dragged off into nothing
  if (g_linkSel && g_linkSel != press) { linkMake(g_linkSel, press); return; }  // picked one, then clicked another
  g_linkSel = g_linkSel == press ? 0 : press;
  const Tank* s = g_linkSel ? g_world.find(g_linkSel) : nullptr;
  if (s && s->guardian) { g_linkSel = 0; linkNote(6); return; }
  if (s && g_linkMode == 1 && !s->isBot) { g_linkSel = 0; linkNote(5); return; }
  linkNote(g_linkSel ? 7 : 0);
}
static int tankIndex(uint32_t id) {
  for (size_t i = 0; i < g_world.tanks.size(); ++i) if (g_world.tanks[i].id == id) return (int)i;
  return -1;
}
static void chevron(Vec2 tip, float ang, float s, float th, Col c, float alpha) {  // an arrowhead pointing along ang
  for (float k : {-1.0f, 1.0f}) {
    const float a = ang + kPi + 0.6f * k;
    const Vec2 m = tip + Vec2{std::cos(a), std::sin(a)} * s;
    Inst& i = rectI(m.x, m.y, s, th, a, c);
    i.ol = 0; i.alpha = alpha;
  }
}
// A red line: solid, with arrows marching toward the target and a big one at its end. A blue line: dashed, with a dot
// at both ends. side shifts it sideways (two red lines between the same tanks, one each way).
static void linkLine(Vec2 a, Vec2 b, float ra, float rb, bool team, float T, float side, Vec2 cam, float alpha = 1.0f) {
  Vec2 d = b - a;
  float L = d.len();
  if (L < 1) return;
  d = d * (1.0f / L);
  const Vec2 n{-d.y, d.x};
  a = a + d * (ra + 8.0f) + n * side;
  b = b - d * (rb + (team ? 8.0f : 14.0f)) + n * side;
  L = (b - a).len();
  if (L < 6 || dot2(b - a, d) <= 0) return;
  const float ang = std::atan2(d.y, d.x);
  const Col red{0.93f, 0.2f, 0.25f}, blue{0.25f, 0.56f, 1.0f};
  const float vw = kViewW / std::max(0.2f, g_zoom) + 400.0f;  // skip pieces far off screen
  auto near = [&](Vec2 p) { return std::fabs(p.x - cam.x) < vw && std::fabs(p.y - cam.y) < vw; };
  if (!team) {
    // The solid line, in pieces so a very long one stays cheap off screen.
    const float piece = 400.0f;
    for (float s = 0; s < L; s += piece) {
      const float e = std::min(L, s + piece);
      const Vec2 m = a + d * ((s + e) * 0.5f);
      if (!near(m)) continue;
      Inst& i = rectI(m.x, m.y, (e - s) * 0.5f + 0.5f, 3.6f, ang, red);
      i.ol = 0; i.alpha = alpha;
    }
    const float step = 120.0f, off = std::fmod(T * 140.0f, step);
    for (float s = off + 30.0f; s < L - 30.0f; s += step) {
      const Vec2 p = a + d * s;
      if (near(p)) chevron(p, ang, 11.0f, 3.4f, red, alpha);
    }
    chevron(b, ang, 20.0f, 5.0f, red, alpha);
  } else {
    const float dash = 24.0f, gap = 14.0f, off = std::fmod(T * 30.0f, dash + gap);
    for (float s = off - gap; s < L; s += dash + gap) {
      const float s0 = std::max(0.0f, s), s1 = std::min(L, s + dash);
      if (s1 <= s0) continue;
      const Vec2 m = a + d * ((s0 + s1) * 0.5f);
      if (!near(m)) continue;
      Inst& i = rectI(m.x, m.y, (s1 - s0) * 0.5f, 4.0f, ang, blue);
      i.ol = 0; i.alpha = alpha;
    }
    for (Vec2 p : {a, b}) { Inst& i = circle(p.x, p.y, 8.0f, blue); i.ol = 0; i.alpha = alpha; }
  }
}
static void drawLinks(Vec2 cam, float T) {
  const bool tool = g_linkMode && !g_menu;
  if (g_menu || (!g_linkShow && !tool) || (g_world.links.empty() && !tool)) return;
  if (g_linkSel) { const Tank* s = g_world.find(g_linkSel); if (!s || !s->alive) { g_linkSel = 0; linkNote(0); } }
  for (const World::Link& l : g_world.links) {
    const int ia = tankIndex(l.from), ib = tankIndex(l.to);
    if (ia < 0 || ib < 0) continue;
    const Tank& A = g_world.tanks[ia];
    const Tank& B = g_world.tanks[ib];
    if (!A.alive || !B.alive) continue;
    const bool both = !l.team && g_world.targets(l.to, l.from);
    // Lines touching the picked tank stand out; the rest fade back a little while the tool is on.
    const float alpha = !tool || !g_linkSel || l.from == g_linkSel || l.to == g_linkSel ? 1.0f : 0.55f;
    linkLine(tankPos(ia), tankPos(ib), A.radius, B.radius, l.team, T, both ? 9.0f : 0.0f, cam, alpha);
  }
  if (!tool) return;
  const Col mc = g_linkMode == 2 ? Col{0.25f, 0.56f, 1.0f} : Col{0.93f, 0.2f, 0.25f};
  // The tank under the mouse, and the picked one.
  const uint32_t hov = linkTankAt();
  if (hov) {
    const int i = tankIndex(hov);
    const Vec2 p = tankPos(i);
    Inst& r = ringI(p.x, p.y, g_world.tanks[i].radius + 10.0f, 1.6f, {1, 1, 1});
    r.alpha = 0.7f;
  }
  if (g_linkSel) {
    const int i = tankIndex(g_linkSel);
    if (i >= 0) {
      const Vec2 p = tankPos(i);
      const float R = g_world.tanks[i].radius + 14.0f + std::sin(T * 6.0f) * 3.0f;
      Inst& r = ringI(p.x, p.y, R, 3.0f, mc);
      r.alpha = 0.95f;
    }
  }
  // Dragging a new line from a tank: it follows the mouse.
  const uint32_t from = g_linkDown && g_linkPress ? g_linkPress : g_linkSel;
  if (from) {
    const int i = tankIndex(from);
    if (i >= 0) {
      Vec2 to = mouseWorld();
      float rb = 0;
      if (hov && hov != from) { const int j = tankIndex(hov); to = tankPos(j); rb = g_world.tanks[j].radius; }
      if ((to - tankPos(i)).len() > g_world.tanks[i].radius + 30.0f)
        linkLine(tankPos(i), to, g_world.tanks[i].radius, rb, g_linkMode == 2, T, 0, cam, 0.5f);
    }
  }
}

static void drawTankInner(size_t idx, Vec2 p, float aim, const Look& lk);
static void drawTank(size_t idx, Vec2 p, float aim, const Look& lk) {
  const size_t start = g_insts.size();
  drawTankInner(idx, p, aim, lk);
  if (g_world.tanks[idx].ghost) fadeFrom(start, 0.4f);  // dev panel: a ghost
}
static void drawTankInner(size_t idx, Vec2 p, float aim, const Look& lk) {
  const Tank& t = g_world.tanks[idx];
  TankFx& f = g_tfx[idx];
  const float pulse = 0.5f + 0.5f * std::sin(g_clock * 6.0f);
  const float sc = t.radius / kTankRadius;

  // Invisible: you only see a faded dot where you are; everyone else sees nothing.
  if (isHidden(t)) {
    if (t.id == 1) { blob(p.x, p.y, 12, lk.c1, 0.45f); ringI(p.x, p.y, 20, 1.5f, lk.c1).alpha = 0.3f; }
    return;
  }

  if (t.guardian && t.temple >= 0) blob(p.x, p.y, 62 + 5 * pulse, {0.55f, 0.36f, 1.0f}, 0.2f);  // the Arena Mage
  // Under the tank: auras for active superpowers and relic states.
  if (t.active == Power::RapidShot || t.active == Power::Spread) blob(p.x, p.y, 40 * sc + 3 * pulse, powerColor(t.active), 0.2f);
  if (t.repairing) blob(p.x, p.y, 40 * sc + 4 * pulse, relicColor(Relic::Repair), 0.22f);
  if (t.slowT > 0) blob(p.x, p.y, 40 * sc, powerColor(Power::Singularity), 0.15f);

  const float flash = f.flash > 0 ? 0.45f : 0.0f;
  const float kick = 3.5f * f.recoil;
  const float cx = std::cos(aim), cy = std::sin(aim);
  if (g_world.mode == Mode::Rampage && !npc(t)) {
    // The Rocket slides out of the body as it recharges (all the way out: ready), and is gone while one is flying.
    float k = 0;
    if (t.active == Power::None) {
      const float full = powerInfo(Power::Rocket).cooldown * t.cdScale;
      k = full > 0 ? std::clamp(1.0f - t.recharge / full, 0.0f, 1.0f) : 1.0f;
    }
    if (k > 0.01f) barrelRocket(p + Vec2{cx, cy} * ((-10.0f + 40.0f * k) * sc), aim, 1.25f * sc, lk, flash);
  } else {
    Inst& b = rectI(p.x + cx * (30 - kick) * sc, p.y + cy * (30 - kick) * sc, (30 - kick) * sc, 11 * sc, aim, lk.b1);
    b.flash = flash; b.ol = 3.5f * sc;
    paint(b, lk.bpat, lk.b2);
  }
  Inst& body = add(p.x, p.y, t.radius, aim, 0, lk.c1);
  body.flash = flash; body.ol = 3.5f * sc;
  paint(body, lk.pat, lk.c2);
  if (t.guardian) {  // a gold core and three studs: obviously not a normal tank
    circle(p.x, p.y, 15, {1.0f, 0.75f, 0.22f}).ol = 3;
    for (int k = 0; k < 3; ++k) {
      const float a = aim + kPi + (k - 1) * 0.8f;
      circle(p.x + std::cos(a) * 32, p.y + std::sin(a) * 32, 5, {1.0f, 0.75f, 0.22f}).ol = 2;
    }
  }

  if (t.active == Power::Shield) {
    blob(p.x, p.y, 46 * sc, powerColor(Power::Shield), 0.2f + 0.08f * pulse);
    ringI(p.x, p.y, 46 * sc, 2.5f, powerColor(Power::Shield)).alpha = 0.9f;
  }
  if (t.active == Power::Instinct) {  // gold ring with one pip per dodge; a shrinking ring while a hit waits
    ringI(p.x, p.y, 44 * sc, 1.6f, powerColor(Power::Instinct)).alpha = 0.7f;
    const int pips = (int)std::ceil(t.dodges - 0.01f);
    for (int k = 0; k < pips; ++k) {
      const float a = -kPi * 0.5f + (k - (pips - 1) * 0.5f) * 0.35f;
      circle(p.x + std::cos(a) * 44 * sc, p.y + std::sin(a) * 44 * sc, 4, powerColor(Power::Instinct)).ol = 1.5f;
    }
    if (t.pendingT > 0) ringI(p.x, p.y, 30 * sc + 40 * (t.pendingT / std::max(0.1f, t.dodgeWindow)), 2.5f, {1.0f, 0.3f, 0.3f}).alpha = 0.85f;
  }

  // Stunned (Lightning): little stars circling the tank. Silenced (Singularity): a crossed-out violet badge.
  if (t.stunT > 0)
    for (int k = 0; k < 3; ++k) {
      const float a = g_clock * 5.0f + k * 2.0944f;
      Inst& st = add(p.x + std::cos(a) * t.radius * 1.05f, p.y - t.radius * 0.35f + std::sin(a) * t.radius * 0.4f, 7, g_clock * 4, -4,
                     powerColor(Power::Lightning));
      st.ol = 2;
    }
  if (t.silenceT > 0) {
    const float bx = p.x + t.radius * 0.9f, by = p.y - t.radius * 0.9f;
    circle(bx, by, 9, powerColor(Power::Singularity)).ol = 2.5f;
    rectI(bx, by, 6.5f, 1.4f, -kPi * 0.25f, {1, 1, 1}).ol = 0;
  }

  if (g_debugBots && t.isBot && !t.guardian) {
    static const Col sc6[6] = {{0.55f, 0.55f, 0.55f}, {1.0f, 0.85f, 0.2f}, {0.75f, 0.9f, 0.4f},
                               {0.95f, 0.2f, 0.2f},   {0.2f, 0.5f, 1.0f},  {0.7f, 0.3f, 0.9f}};
    circle(p.x, p.y - 62, 8, sc6[(int)t.mind.state]);
    if (t.mind.state == BotState::Engage)
      for (const Tank& o : g_world.tanks)
        if (o.id == t.mind.targetId && o.alive) {
          const Vec2 d = o.pos - p;
          rectI(p.x + d.x * 0.5f, p.y + d.y * 0.5f, d.len() * 0.5f, 1.5f, std::atan2(d.y, d.x), {0.95f, 0.2f, 0.2f}).ol = 0;
        }
  }

  // Health bar under the tank when damaged (always for guardians). The fill eases and turns amber, then red.
  if (f.shownHp < 0) f.shownHp = t.health;
  if (t.guardian || t.health < t.maxHealth - 0.01f || f.shownHp < t.maxHealth - 0.5f) {
    const float frac = std::clamp(f.shownHp / t.maxHealth, 0.0f, 1.0f);
    const float W = 32 * sc, y = p.y + t.radius + 20;
    capsule(p.x, y, W, 5.0f, 0, {0.2f, 0.16f, 0.28f}).ol = 0;
    const Col hc = t.guardian ? Col{1.0f, 0.72f, 0.2f}
                 : frac > 0.5f ? Col{0.45f, 0.86f, 0.40f} : (frac > 0.25f ? Col{1.0f, 0.78f, 0.25f} : Col{1.0f, 0.38f, 0.33f});
    if (frac > 0.01f) capsule(p.x - W + 2.5f + (W - 2.5f) * frac, y, std::max(3.0f, (W - 2.5f) * frac), 3.0f, 0, hc).ol = 0;
  }
}

// Temple colors by style (one per map): stone, its trim, and the glowing accent (portals, braziers, runes).
struct TempleCols { Col stone, trim, accent, sand; };
static TempleCols templeCols(int style) {
  switch (style) {
    case 1:  return {hexCol(0x2F4A45), hexCol(0x5FE0B8), hexCol(0x59E6B5), hexCol(0xBFD6C4)};
    case 2:  return {hexCol(0x4F2D36), hexCol(0xFF8A66), hexCol(0xFF6B4A), hexCol(0xDDB7A8)};
    default: return {hexCol(0x4A3F5C), hexCol(0xE8B24A), hexCol(0xFFC44D), hexCol(0xE2CFA4)};
  }
}
static int templeStyle(int k) { return k >= 0 && k < (int)g_world.temples.size() ? g_world.temples[k].style : 0; }

// Can a light at L see F inside a temple? The same test as the lighting shader (kFogFrag), without the soft edges:
// name tags behind a wall stay hidden.
static bool templeSight(const Temple& T, Vec2 Lw, Vec2 Fw) {
  const Vec2 L = Lw - T.c, F = Fw - T.c, d = F - L;
  if ((Fw - Lw).len() > 1250.0f) return false;
  const float rF = F.len(), h = T.thick * 0.5f;
  auto angDist = [](float a, float b) { float x = std::fmod(a - b + kPi, 2 * kPi); if (x < 0) x += 2 * kPi; return std::fabs(x - kPi); };
  for (size_t i = 0; i < T.rings.size(); ++i) {
    const float ri = T.rings[i] - h, ro = T.rings[i] + h;
    const bool inBody = rF > ri && rF < ro;
    float ts[4] = {-1, -1, -1, -1};
    const float rcs[2] = {ri, ro};
    for (int c = 0; c < 2; ++c) {
      const float a = d.x * d.x + d.y * d.y, b = 2 * (L.x * d.x + L.y * d.y), cc = L.x * L.x + L.y * L.y - rcs[c] * rcs[c];
      const float disc = b * b - 4 * a * cc;
      if (disc <= 0 || a < 1e-3f) continue;
      const float sq = std::sqrt(disc), t0 = (-b - sq) / (2 * a), t1 = (-b + sq) / (2 * a);
      if (t0 > 0 && t0 < 1) ts[c * 2] = t0;
      if (t1 > 0 && t1 < 1) ts[c * 2 + 1] = t1;
    }
    const float last = std::max(std::max(ts[0], ts[1]), std::max(ts[2], ts[3]));
    for (int k = 0; k < 4; ++k) {
      if (ts[k] < 0 || (inBody && ts[k] == last)) continue;
      const float rc = rcs[k / 2], half = T.gapHalf[i];
      if (half <= 0) return false;
      const Vec2 P = L + d * ts[k];
      if ((half - angDist(std::atan2(P.y, P.x), T.gapA[i])) * rc - T.thick * 0.3f < 0) return false;
    }
  }
  for (size_t i = 0; i + 1 < T.blocks.size(); i += 2) {
    const Vec2 a = T.blocks[i] - T.c, ab = T.blocks[i + 1] - T.blocks[i];
    if ((closestOnSeg(a, a + ab, F) - F).len() < h) continue;
    const Vec2 n = Vec2{-ab.y, ab.x}.norm() * h;
    for (int sgn = -1; sgn <= 1; sgn += 2) {
      const Vec2 a2 = a + n * (float)sgn;
      const float den = d.x * ab.y - d.y * ab.x;
      if (std::fabs(den) < 1e-4f) continue;
      const Vec2 w0 = a2 - L;
      const float tt = (w0.x * ab.y - w0.y * ab.x) / den, uu = (w0.x * d.y - w0.y * d.x) / den;
      if (tt > 0 && tt < 1 && uu > 0 && uu < 1) return false;
    }
  }
  return true;
}

// A swirling portal (temple doors, the Mage's exit).
static void drawPortal(Vec2 p, float R, Col accent, float T) {
  blob(p.x, p.y, R * 1.6f + 4 * std::sin(T * 3), accent, 0.18f);
  circle(p.x, p.y, R, {0.1f, 0.07f, 0.16f}).ol = 4;
  for (int k = 0; k < 3; ++k) {
    const float rr = R * (0.85f - 0.22f * k) * (0.92f + 0.08f * std::sin(T * 4 + k));
    ringI(p.x, p.y, rr, 2.2f, mixC(accent, {1, 1, 1}, 0.25f * k)).alpha = 0.85f;
  }
  for (int k = 0; k < 6; ++k) {
    const float a = T * (2.2f + 0.3f * (k % 2)) + k * 1.0472f, rr = R * (0.3f + 0.12f * (k % 3));
    circle(p.x + std::cos(a) * rr, p.y + std::sin(a) * rr, 2.8f, mixC(accent, {1, 1, 1}, 0.5f)).ol = 0;
  }
}

// Loot crates: wooden (Normal, Small) with an up-arrow badge, or gold with a star (Legendary). They flash when hit.
static void drawBox(const LootBox& b, float T, float dt) {
  BoxSeen& s = g_boxSeen[b.id];
  if (s.hp >= 0 && b.hp < s.hp - 0.01f) s.flash = 0.1f;
  s.hp = b.hp;
  s.flash = std::max(0.0f, s.flash - dt);
  const bool legend = b.kind == BoxKind::Legendary;
  const float R = boxRadius(b.kind), x = b.pos.x, y = b.pos.y;
  const Col dark = legend ? hexCol(0x8A5A0E) : hexCol(0x5E3519), wood = legend ? hexCol(0xF2C230) : hexCol(0xB07040);
  const Col plank = legend ? hexCol(0xFFDB5C) : hexCol(0xC98A52), badge = legend ? hexCol(0xFFF0A0) : hexCol(0xF5A43A);
  const float fl = s.flash > 0 ? 0.5f : 0.0f;
  if (legend) blob(x, y, R * 1.9f + 4 * std::sin(T * 3), {1.0f, 0.82f, 0.25f}, 0.22f);
  const size_t st = g_insts.size();
  rrect(x, y, R, R, 0, dark).ol = 0;
  rrect(x, y, R * 0.84f, R * 0.84f, 0, wood).ol = 0;
  for (float yy : {-0.42f, 0.0f, 0.42f}) rectI(x, y + R * yy, R * 0.84f, R * 0.035f, 0, mixC(dark, wood, 0.35f)).ol = 0;
  rectI(x - R * 0.42f, y, R * 0.06f, R * 0.84f, 0, plank).ol = 0;
  for (int k = 0; k < 4; ++k) circle(x + (k & 1 ? 0.72f : -0.72f) * R, y + (k & 2 ? 0.72f : -0.72f) * R, R * 0.075f, dark).ol = 0;
  circle(x, y, R * 0.44f, dark).ol = 0;
  circle(x, y, R * 0.36f, badge).ol = 0;
  if (legend) add(x, y, R * 0.27f, 0, -4, dark).ol = 0;
  else { poly(x, y - R * 0.06f, R * 0.2f, kPi, 3, dark).ol = 0; rectI(x, y + R * 0.13f, R * 0.07f, R * 0.12f, 0, dark).ol = 0; }
  for (size_t i = st; i < g_insts.size(); ++i) g_insts[i].flash = fl;
  if (b.hp < boxHealth(b.kind) - 0.5f) {
    const float frac = std::clamp(b.hp / boxHealth(b.kind), 0.0f, 1.0f), W = R * 0.9f;
    capsule(x, y + R + 12, W, 4.0f, 0, {0.2f, 0.16f, 0.28f}).ol = 0;
    capsule(x - W + 2 + (W - 2) * frac, y + R + 12, std::max(2.5f, (W - 2) * frac), 2.4f, 0, legend ? Col{1.0f, 0.8f, 0.25f} : Col{0.95f, 0.65f, 0.3f}).ol = 0;
  }
}

// A stat upgrade lying on the map (also hanging under a Loot Drone, at the same size).
static void drawUpgrade(float x, float y, int stat, int r, float T, uint32_t id) {
  const UpCols uc = upgradeCols(r);
  if (r == 4) blob(x, y, 27.0f + 2.0f * std::sin(T * 4 + (float)id), uc.main, 0.2f);
  const int cell = statIconCell(stat);
  if (g_iconsReady && cell >= 0) { sprite(x, y, 19, cell, true, uc.main, uc.fill); return; }
  circle(x, y, 19, uc.main).ol = 0;
  circle(x, y, 16.6f, uc.fill).ol = 0;
  drawGlyph(3, stat, x, y, 1.0f, uc.dark, uc.fill);
}

static void buildInstances(Vec2 cam, const Tank& me) {
  g_insts.clear();
  const float margin = 80;
  const float T = g_clock;
  const Col white{1, 1, 1}, shadow{0.25f, 0.2f, 0.3f};
  const Tank& player = g_world.tanks[0];

  for (const Shape& s : g_world.shapes) {
    if (!inView(s.pos, cam, margin)) continue;
    const ShapeInfo info = shapeInfo(s.kind);
    poly(s.pos.x, s.pos.y, info.radius, s.rot, info.sides, {1.0f, 0.85f, 0.4f}).flash = s.hitFlash > 0 ? 0.5f : 0.0f;
  }

  static double lastBuild = emscripten_get_now();
  const float dt = (float)std::min(0.1, (emscripten_get_now() - lastBuild) / 1000.0);
  lastBuild = emscripten_get_now();

  // Arena Temples on the map: a stepped octagonal platform with stairs on four sides, four pillars, braziers, an inlaid
  // ring and the portal on a raised dais. A cleared temple leaves a cracked patch behind.
  for (const Temple& tp : g_world.temples) {
    if (!inView(tp.door, cam, 400)) continue;
    const TempleCols tc = templeCols(tp.style);
    const float x = tp.door.x, y = tp.door.y, o8 = kPi / 8;
    if (tp.cleared) {
      poly(x, y, 300, o8, 8, mixC(tc.sand, {0.45f, 0.42f, 0.4f}, 0.55f)).alpha = 0.5f;
      for (int k = 0; k < 7; ++k) {
        const float a = k * 0.9f + 0.3f, l = 70.0f + 25.0f * (k % 3);
        rectI(x + std::cos(a) * l, y + std::sin(a) * l, l, 2.5f, a, {0.3f, 0.27f, 0.3f}).alpha = 0.5f;
        poly(x + std::cos(a * 1.7f) * 200, y + std::sin(a * 1.7f) * 200, 14.0f + 5 * (k % 2), a, 5, mixC(tc.sand, {0.4f, 0.38f, 0.4f}, 0.5f)).ol = 3;
      }
      continue;
    }
    const Col sandD = mixC(tc.sand, tc.stone, 0.32f), sandL = mixC(tc.sand, {1, 1, 1}, 0.25f);
    poly(x, y + 12, 312, o8, 8, shadow).alpha = 0.16f;
    poly(x, y, 310, o8, 8, sandD).ol = 4.5f;
    poly(x, y, 276, o8, 8, tc.sand).ol = 3.0f;
    poly(x, y, 240, o8, 8, sandL).ol = 3.0f;
    for (int s = 0; s < 4; ++s) {  // stairs up each side
      const float a = s * 1.5708f;
      for (int k = 0; k < 4; ++k) {
        const float r = 246.0f + k * 16.0f;
        rectI(x + std::cos(a) * r, y + std::sin(a) * r, 7.0f, 64.0f - k * 4.0f, a, k % 2 ? tc.sand : sandL).ol = 2.0f;
      }
    }
    ringI(x, y, 190, 2.5f, tc.trim).alpha = 0.75f;
    ringI(x, y, 176, 1.2f, tc.trim).alpha = 0.5f;
    for (int k = 0; k < 8; ++k) {
      const float a = k * 0.7854f + 0.3927f;
      add(x + std::cos(a) * 183, y + std::sin(a) * 183, 8, a, 4, tc.trim).ol = 0;
    }
    for (int s = 0; s < 4; ++s) {  // braziers on the top step, between the stairs and the pillars
      const float a = s * 1.5708f + 0.3927f;
      const float bx = x + std::cos(a) * 215, by = y + std::sin(a) * 215, fl = std::sin(T * 9 + s * 2.1f);
      blob(bx, by, 26 + 3 * fl, {1.0f, 0.6f, 0.2f}, 0.18f);
      circle(bx, by, 10, {0.25f, 0.21f, 0.24f}).ol = 2.5f;
      blob(bx, by - 2, 6.5f + fl, {1.0f, 0.55f, 0.15f}, 0.95f);
      blob(bx, by - 3, 3.5f + 0.5f * fl, {1.0f, 0.92f, 0.5f}, 0.95f);
    }
    poly(x, y, 112, o8, 8, mixC(tc.sand, tc.stone, 0.5f)).ol = 3.5f;
    poly(x, y, 96, o8, 8, mixC(tc.sand, tc.stone, 0.25f)).ol = 0;
    drawPortal(tp.door, kPortalR + 10, tc.accent, T);
  }

  // Walls. Map walls: slate capsules with a soft shadow. Temple stone: thick blocks with a lit top and a trim line.
  // Pillars: square columns outside, round ones inside.
  struct StoneTop { Vec2 c, a, b; float hl, h, ang; Col top, trim, dark; bool joinA, joinB; };
  static std::vector<StoneTop> tops;
  tops.clear();
  for (const Wall& w : g_world.walls) {
    const Vec2 mid = (w.a + w.b) * 0.5f, d = w.b - w.a;
    const float len = d.len();
    if (!inView(mid, cam, len * 0.5f + 80.0f)) continue;
    const float ang = std::atan2(d.y, d.x);
    if (w.kind == 0) {
      capsule(mid.x, mid.y + 5, len * 0.5f + w.thick * 0.5f, w.thick * 0.5f, ang, shadow).alpha = 0.12f;
      capsule(mid.x, mid.y, len * 0.5f + w.thick * 0.5f, w.thick * 0.5f, ang, {0.48f, 0.46f, 0.60f}).ol = 4.0f;
      continue;
    }
    const TempleCols tc = templeCols(templeStyle(w.temple));
    if (w.kind == 2) {  // a pillar outside: a square sandstone column with a capital
      const float h = w.thick * 0.5f;
      rrect(mid.x + 4, mid.y + 9, h, h, 0, shadow).alpha = 0.2f;
      rrect(mid.x, mid.y, h, h, 0, mixC(tc.sand, tc.stone, 0.3f)).ol = 4;
      rrect(mid.x, mid.y - 3, h * 0.72f, h * 0.72f, 0, mixC(tc.sand, {1, 1, 1}, 0.3f)).ol = 0;
      circle(mid.x, mid.y - 3, h * 0.28f, tc.trim).ol = 0;
      continue;
    }
    const Col dark = mixC(tc.stone, {0, 0, 0}, 0.35f), top = mixC(tc.stone, {1, 1, 1}, 0.12f);
    if (len < 1.0f) {  // a pillar inside
      circle(mid.x, mid.y, w.thick * 0.5f, dark).ol = 4;
      circle(mid.x, mid.y - 4, w.thick * 0.38f, top).ol = 0;
      ringI(mid.x, mid.y - 4, w.thick * 0.24f, 2.0f, tc.trim).alpha = 0.85f;
      continue;
    }
    // Temple stone: square-ended blocks that run on into the next one (so a ring reads as one wall); only a free end (at
    // a way in, or a dead end) is rounded.
    const float h = w.thick * 0.5f;
    bool joinA = false, joinB = false;
    for (const Wall& o : g_world.walls) {
      if (&o == &w || o.kind != 1 || o.temple != w.temple) continue;
      joinA = joinA || (o.a - w.a).len() < 1.0f || (o.b - w.a).len() < 1.0f;
      joinB = joinB || (o.a - w.b).len() < 1.0f || (o.b - w.b).len() < 1.0f;
    }
    const Vec2 dir = d * (1.0f / len);
    const float ea = joinA ? 20.0f : 0.0f, eb = joinB ? 20.0f : 0.0f;  // overlap the next block at the joins
    const Vec2 c = mid + dir * ((eb - ea) * 0.5f);
    const float hl = len * 0.5f + (ea + eb) * 0.5f;
    for (int e = 0; e < 2; ++e) if (!(e == 0 ? joinA : joinB)) { const Vec2 q = e == 0 ? w.a : w.b; circle(q.x, q.y, h, dark).ol = 4.0f; }
    rectI(c.x, c.y, hl, h, ang, dark).ol = 4.0f;
    tops.push_back({c, w.a, w.b, hl, h, ang, top, tc.trim, dark, joinA, joinB});
  }
  // The lit tops go on after every block, so they run on unbroken across the joins.
  for (const StoneTop& s : tops) {
    for (int e = 0; e < 2; ++e) if (!(e == 0 ? s.joinA : s.joinB)) { const Vec2 q = e == 0 ? s.a : s.b; circle(q.x, q.y - 5, s.h * 0.7f, s.top).ol = 0; }
    rectI(s.c.x, s.c.y - 5, s.hl, s.h * 0.7f, s.ang, s.top).ol = 0;
  }
  for (const StoneTop& s : tops) {
    Inst& tr = rectI(s.c.x, s.c.y - 5, s.hl, 2.2f, s.ang, s.trim);
    tr.ol = 0; tr.alpha = 0.6f;
    if (s.joinA) { Inst& jt = rectI(s.a.x, s.a.y - 5, 2.0f, s.h * 0.7f, s.ang, s.dark); jt.ol = 0; jt.alpha = 0.6f; }  // joint between blocks
  }

  // Inside a temple: braziers along the walls, and the Mage's exit portal once it is beaten (stand in it for 2 s; dots
  // around you fill up).
  for (const Temple& tp : g_world.temples) {
    if (!inView(tp.c, cam, tp.R + 200)) continue;
    for (size_t k = 0; k < tp.deco.size(); ++k) {
      const Vec2 p = tp.deco[k];
      if (!inView(p, cam, 80)) continue;
      const float fl = std::sin(T * 9 + k * 1.7f) * 0.5f + std::sin(T * 13 + k) * 0.5f;
      blob(p.x, p.y, 40 + 4 * fl, {1.0f, 0.6f, 0.2f}, 0.16f);
      circle(p.x, p.y, 14, {0.2f, 0.17f, 0.22f}).ol = 3;
      blob(p.x, p.y - 2, 9 + 1.5f * fl, {1.0f, 0.55f, 0.15f}, 0.95f);
      blob(p.x, p.y - 4, 5 + fl, {1.0f, 0.92f, 0.5f}, 0.95f);
    }
    if (tp.cleared) {
      drawPortal(tp.c, kExitR, {0.45f, 1.0f, 0.6f}, T);
      const float k = std::clamp(player.portalT / kExitHold, 0.0f, 1.0f);
      if (player.alive && k > 0 && (player.pos - tp.c).len() < kExitR + 10) {
        for (int i = 0; i < 24; ++i) {
          const float a = -kPi * 0.5f + i * (2 * kPi / 24);
          circle(player.pos.x + std::cos(a) * 52, player.pos.y + std::sin(a) * 52, 4.0f,
                 i < (int)(k * 24 + 0.5f) ? Col{0.6f, 1.0f, 0.75f} : Col{0.25f, 0.3f, 0.3f}).ol = 0;
        }
      }
    }
  }

  // Loot crates.
  for (const LootBox& b : g_world.boxes) if (inView(b.pos, cam, margin)) drawBox(b, T, dt);

  // Stat upgrades: a thin rim in the rarity color, a light fill and the icon; they sit flat on the ground. Upgrades that
  // wouldn't raise your tier are faded out.
  for (const Upgrade& u : g_world.upgrades) {
    if (!inView(u.pos, cam, margin)) continue;
    const size_t start = g_insts.size();
    drawUpgrade(u.pos.x, u.pos.y, u.stat, (int)u.rarity, T, u.id);
    if (player.alive && !g_menu && (int)u.rarity + 1 <= player.stat[u.stat]) fadeFrom(start, 0.32f);
  }

  // Superpowers: a rounded square the size of an upgrade, light tomato red with a red rim and icon. Mythics: dark, with a
  // rainbow rim and glow.
  for (const Crate& c : g_world.crates) {
    if (!inView(c.pos, cam, margin)) continue;
    const float x = c.pos.x, y = c.pos.y;
    if (isMythic(c.type)) {
      blob(x, y, 29 + 2 * std::sin(T * 4), hueC(T * 0.3f), 0.25f);
      rrect(x, y, 19, 19, 0, hueC(T * 0.4f)).ol = 0;
      rrect(x, y, 16.2f, 16.2f, 0, {0.16f, 0.1f, 0.3f}).ol = 0;
      powerGlyph(c.type, x, y, 0.9f, white, {0.16f, 0.1f, 0.3f});
    } else if (g_iconsReady && powerIconCell((int)c.type) >= 0) {
      sprite(x, y, 19, powerIconCell((int)c.type), false, white, white);
    } else {
      rrect(x, y, 19, 19, 0, kPowDark).ol = 0;
      rrect(x, y, 16.6f, 16.6f, 0, kPowRed).ol = 0;
      powerGlyph(c.type, x, y, 0.9f, kPowDark, kPowRed);
    }
  }

  // Relics: a hexagon (pointy top) in the relic's color with a slow halo.
  for (const RelicPickup& r : g_world.relics) {
    if (!inView(r.pos, cam, margin)) continue;
    const float x = r.pos.x, y = r.pos.y;
    const Col rc = relicColor(r.kind);
    blob(x, y, 38 + 3 * std::sin(T * 3 + (float)r.id), rc, 0.18f);
    poly(x, y, 27, 0, 6, rc).ol = 2.5f;
    poly(x, y, 20.5f, 0, 6, white).ol = 0;
    relicGlyph(r.kind, x, y, 1.2f, rc, white);
  }

  // Singularities: a dark core, turning rings, and a faint circle showing how far the pull reaches.
  for (const Singularity& s : g_world.sings) {
    if (!inView(s.pos, cam, kSingRadius)) continue;
    const Col v = powerColor(Power::Singularity);
    ringI(s.pos.x, s.pos.y, kSingRadius, 2.0f, v).alpha = 0.25f;
    blob(s.pos.x, s.pos.y, 70 + 6 * std::sin(T * 6), v, 0.2f);
    for (int k = 0; k < 3; ++k) ringI(s.pos.x, s.pos.y, 26 + k * 11 + 4 * std::sin(T * 4 + k), 1.6f, hueC(0.72f + 0.05f * k)).alpha = 0.8f;
    circle(s.pos.x, s.pos.y, 22, {0.08f, 0.04f, 0.16f}).ol = 3;
  }

  drawFx(g_fxLow, cam);

  // Bombs: a dark ball with a blinking fuse and a warning ring that fills in toward the blast size.
  for (const Bomb& b : g_world.bombs) {
    if (!inView(b.pos, cam, margin)) continue;
    if (b.kind == 1) {  // a coconut falling: its shadow grows, then it lands
      const float k = std::clamp(1.0f - b.fuse / 0.9f, 0.0f, 1.0f);
      blob(b.pos.x, b.pos.y, 16 + 10 * k, {0.15f, 0.1f, 0.05f}, 0.25f * k + 0.05f);
      const float h = (1.0f - k) * 160.0f;
      circle(b.pos.x, b.pos.y - h, 13, {0.55f, 0.36f, 0.2f}).ol = 3;
      circle(b.pos.x - 3, b.pos.y - h - 3, 2.2f, {0.25f, 0.15f, 0.08f}).ol = 0;
      circle(b.pos.x + 3, b.pos.y - h - 3, 2.2f, {0.25f, 0.15f, 0.08f}).ol = 0;
      continue;
    }
    const float k = 1.0f - b.fuse / kBombFuse;
    ringI(b.pos.x, b.pos.y, kBombBlast * k, 2.0f, {1.0f, 0.3f, 0.25f}).alpha = 0.55f;
    blob(b.pos.x, b.pos.y, kBombBlast, {1.0f, 0.3f, 0.25f}, 0.08f);
    circle(b.pos.x, b.pos.y, 11, {0.2f, 0.17f, 0.27f}).ol = 3;
    if (std::fmod(T * 8, 1.0f) < 0.5f) circle(b.pos.x + 5, b.pos.y - 6, 3.5f, {1.0f, 0.4f, 0.3f}).ol = 0;
  }

  // Turrets: a square base with the owner's color and a barrel.
  for (const Turret& tu : g_world.turrets) {
    if (!inView(tu.pos, cam, margin)) continue;
    const Tank* o = g_world.find(tu.owner);
    const Col oc = o ? lookOf(*o).c1 : kEnemyCol;
    ringI(tu.pos.x, tu.pos.y, 27, 1.8f, oc).alpha = 0.35f + 0.65f * std::clamp(tu.life / kTurretLife, 0.0f, 1.0f);
    rectI(tu.pos.x + std::cos(tu.aim) * 15, tu.pos.y + std::sin(tu.aim) * 15, 15, 6, tu.aim, kBarrelCol).ol = 3;
    poly(tu.pos.x, tu.pos.y, 20, kPi * 0.25f, 4, {0.48f, 0.46f, 0.60f}).ol = 3;
    circle(tu.pos.x, tu.pos.y, 8, oc).ol = 2;
  }

  // Bullets use their owner's color; they swell and fade as they use up their range. Lightning bolts are jagged
  // zig-zags; guardian shells are dark and heavy.
  for (const Bullet& b : g_world.bullets) {
    const Vec2 p = lerpPrev(g_bprev, b.id, b.pos);
    if (!inView(p, cam, margin)) continue;
    const float k = b.range0 > 0 ? std::clamp(1.0f - b.rangeLeft / b.range0, 0.0f, 1.0f) : 0.0f;
    if (b.kind == BulletKind::Bolt) {
      const Vec2 d = b.vel.norm(), n{-d.y, d.x};
      const float fade = 1.0f - 0.5f * k;
      const int flick = (int)(T * 24.0f) + (int)b.id * 7;  // the zig-zag re-rolls a few times a second
      Vec2 prev = p;
      for (int j = 1; j <= 5; ++j) {
        const float side = ((flick + j * 5) % 3 - 1) * 7.0f + (j & 1 ? 5.0f : -5.0f);
        const Vec2 q = p - d * (13.0f * j) + n * (j == 5 ? 0.0f : side);
        const Vec2 m = (prev + q) * 0.5f, e = q - prev;
        Inst& glow = capsule(m.x, m.y, e.len() * 0.5f + 5.0f, 5.0f, std::atan2(e.y, e.x), {0.55f, 0.85f, 1.0f});
        glow.ol = 0; glow.alpha = 0.45f * fade;
        Inst& core = capsule(m.x, m.y, e.len() * 0.5f + 2.0f, 2.0f, std::atan2(e.y, e.x), white);
        core.ol = 0; core.alpha = fade;
        prev = q;
      }
      continue;
    }
    const float br = b.radius * (1.0f + kBulletGrow * k), ba = bulletAlpha(k);
    if (ba <= 0.01f) continue;
    if (b.kind == BulletKind::Guardian) { Inst& g = circle(p.x, p.y, br, kGuardBullet); g.ol = 4; g.alpha = ba; continue; }
    circle(p.x, p.y, br, bulletCol(g_world.find(b.owner))).alpha = ba;
  }

  // Rockets: a white missile with a colored nose cone and band, swept fins, a nozzle and a flame. Homing rockets are
  // magenta, Rockets red.
  for (const Rocket& r : g_world.rockets) {
    const Vec2 p = lerpPrev(g_mprev, r.id, r.pos);
    if (!inView(p, cam, margin)) continue;
    const float a = std::atan2(r.vel.y, r.vel.x);
    if (g_world.mode == Mode::Rampage) {  // the owner's barrel-colored rocket, with its flame
      const Tank* o = g_world.find(r.owner);
      const Vec2 d{std::cos(a), std::sin(a)};
      const float fl = std::sin(T * 47) * 2.5f;
      { const Vec2 q = p - d * 36.0f; blob(q.x, q.y, 12 + fl, {1.0f, 0.55f, 0.2f}, 0.85f); }
      { const Vec2 q = p - d * 31.0f; blob(q.x, q.y, 6.5f, {1.0f, 0.93f, 0.6f}, 0.95f); }
      barrelRocket(p, a, 1.25f, o ? lookOf(*o) : lookOf(g_world.tanks[0]), 0);
      continue;
    }
    const Col nose = powerColor(r.homing ? Power::Homing : Power::Rocket), fin{0.55f, 0.53f, 0.66f}, body{0.95f, 0.95f, 0.97f};
    const Vec2 d{std::cos(a), std::sin(a)}, n{-d.y, d.x};
    auto at = [&](float f, float s) { return p + d * f + n * s; };
    const float fl = std::sin(T * 47) * 2;
    { const Vec2 q = at(-29, 0); blob(q.x, q.y, 10 + fl, {1.0f, 0.55f, 0.2f}, 0.85f); }
    { const Vec2 q = at(-25, 0); blob(q.x, q.y, 5.5f, {1.0f, 0.93f, 0.6f}, 0.95f); }
    for (float s : {-1.0f, 1.0f}) { const Vec2 q = at(-11, 7.5f * s); rectI(q.x, q.y, 7.5f, 2.6f, a - 0.6f * s, fin).ol = 2.2f; }
    { const Vec2 q = at(-18.5f, 0); rectI(q.x, q.y, 3.2f, 4.6f, a, {0.35f, 0.33f, 0.42f}).ol = 2.2f; }
    capsule(p.x, p.y, 18, 6.2f, a, body).ol = 2.4f;
    { const Vec2 q = at(12.5f, 0); circle(q.x, q.y, 5.6f, nose).ol = 0; }
    { const Vec2 q = at(-3, 0); rectI(q.x, q.y, 1.6f, 6.2f, a, nose).ol = 0; }
  }

  // Tanks (invisible ones are skipped, except your own faded dot).
  for (size_t i = 0; i < g_world.tanks.size(); ++i) {
    const Tank& t = g_world.tanks[i];
    if (!t.alive) continue;
    const Vec2 p = tankPos(i);
    if (inView(p, cam, 120)) drawTank(i, p, tankAim(i), lookOf(t));
  }

  // Drones are quadcopters: four rotors on crossed arms. The Sentry Drone has its owner's color and a gun (a red ring
  // and its hits left while overloaded). The Loot Drone is a yellow cargo drone; what it carries hangs under it at full
  // size.
  for (const Drone& d : g_world.drones) {
    const Vec2 p = lerpPrev(g_mprev, d.id, d.pos);
    if (!inView(p, cam, margin)) continue;
    const Tank* o = g_world.find(d.owner);
    if (o && isHidden(*o) && o->id != 1) continue;
    const Col oc = o ? lookOf(*o).c1 : kEnemyCol;
    const bool sentry = d.kind == Relic::Sentry;
    if (!sentry && d.carryStat >= 0) {
      rectI(p.x, p.y + 13, 1.0f, 9, 0, {0.3f, 0.27f, 0.35f}).ol = 0;
      drawUpgrade(p.x, p.y + 30, d.carryStat, (int)d.carryRarity, T, d.id);
    }
    if (sentry && d.vulnerable()) {
      ringI(p.x, p.y, 24, 1.6f, {1.0f, 0.35f, 0.3f}).alpha = 0.6f + 0.4f * std::sin(T * 10);
      for (int k = 0; k < d.hp; ++k) circle(p.x - (d.hp - 1) * 4.0f + k * 8.0f, p.y + 26, 2.6f, {1.0f, 0.35f, 0.3f}).ol = 0;
    }
    const float base = sentry ? d.aim + 0.7854f : 0.7854f;
    const Col arm{0.36f, 0.34f, 0.44f};
    for (int k = 0; k < 2; ++k) rectI(p.x, p.y, 15, 1.9f, base + k * 1.5708f, arm).ol = 0;
    for (int k = 0; k < 4; ++k) {
      const float a = base + k * 1.5708f;
      const float rx = p.x + std::cos(a) * 14, ry = p.y + std::sin(a) * 14;
      blob(rx, ry, 7.5f, {0.85f, 0.85f, 0.92f}, 0.45f);
      ringI(rx, ry, 7.5f, 0.9f, arm).alpha = 0.85f;
      rectI(rx, ry, 6.5f, 1.0f, T * 31 + k, arm).ol = 0;
    }
    if (sentry) {
      rectI(p.x + std::cos(d.aim) * 9, p.y + std::sin(d.aim) * 9, 8, 3.0f, d.aim, kBarrelCol).ol = 2;
      circle(p.x, p.y, 8.5f, (o && !o->sentryOn && !o->sentryOver) ? Col{0.6f, 0.6f, 0.65f} : oc).ol = 2.5f;
    } else {
      rrect(p.x, p.y, 9.5f, 7.5f, 0, relicColor(Relic::Loot)).ol = 2.5f;
      rectI(p.x, p.y, 9.5f, 1.4f, 0, {0.55f, 0.4f, 0.1f}).ol = 0;
    }
  }

  drawLinks(cam, T);
  drawFx(g_fxHigh, cam);

  // Air strikes: the plane and its shadow pass over everything.
  for (const Strike& s : g_world.strikes) {
    const float dist = (s.t + g_alpha * kDt) * kPlaneSpeed;
    const Vec2 p = s.start + s.dir * dist;
    if (!inView(p, cam, 200)) continue;
    const float a = std::atan2(s.dir.y, s.dir.x);
    const Tank* o = g_world.find(s.owner);
    const Col oc = o ? lookOf(*o).c1 : kEnemyCol;
    for (int pass = 0; pass < 2; ++pass) {
      const bool sh = pass == 0;
      const Vec2 q = sh ? p + Vec2{50, 70} : p;
      const Col c = sh ? shadow : Col{0.93f, 0.93f, 0.96f};
      const size_t st = g_insts.size();
      rectI(q.x - s.dir.x * 4, q.y - s.dir.y * 4, 12, 44, a, c).ol = sh ? 0 : 3;
      rectI(q.x - s.dir.x * 30, q.y - s.dir.y * 30, 6, 17, a, c).ol = sh ? 0 : 3;
      capsule(q.x, q.y, 40, 9, a, c).ol = sh ? 0 : 3;
      if (!sh) capsule(q.x + s.dir.x * 6, q.y + s.dir.y * 6, 18, 4, a, oc).ol = 0;
      if (sh) fadeFrom(st, 0.18f);
    }
  }
}

// Emits cosmetic trails, rocket smoke, Speed Boost streaks and singularity sparks (real time, so they look the same at
// any sim speed).
static void emitTrails(float dt, Vec2 cam) {
  static float rocketT = 0, singT = 0;
  for (size_t i = 0; i < g_world.tanks.size() && i < g_tfx.size(); ++i) {
    const Tank& t = g_world.tanks[i];
    TankFx& f = g_tfx[i];
    f.flash = std::max(0.0f, f.flash - dt);
    f.recoil = std::max(0.0f, f.recoil - dt * 9.0f);
    if (f.shownHp >= 0) f.shownHp += (t.health - f.shownHp) * std::min(1.0f, dt * 10.0f);
    if (!t.alive || isHidden(t)) continue;
    const Look lk = lookOf(t);
    const float sp = t.vel.len();
    if (sp < 60.0f) continue;
    const Vec2 p = tankPos(i);
    if (!inView(p, cam, 200)) continue;
    f.trailT -= dt;
    if (f.trailT > 0) continue;
    f.trailT = 0.035f;
    const Vec2 back = t.vel * (-1.0f / sp);
    if (lk.trail) trailFx(lk.trail, p, back, lk);
    if (t.active == Power::Dash) {
      Fx s; s.pos = p + back * 20.0f + Vec2{-back.y, back.x} * frange(-18, 18); s.sides = -2; s.r = 14; s.aspect = 0.18f;
      s.rot = std::atan2(back.y, back.x); s.life = 0.25f; s.a0 = 0.6f; s.c = powerColor(Power::Speed); s.shrink = true;
      fxPush(g_fxLow, s);
    }
  }
  rocketT -= dt;
  if (rocketT <= 0) {
    rocketT = 0.02f;
    for (const Rocket& r : g_world.rockets) {
      if (!inView(r.pos, cam, 100)) continue;
      const Vec2 back = r.vel.norm() * -1.0f;
      Fx s; s.pos = r.pos + back * 22.0f + frandDir() * 3.0f; s.r = frange(5, 8); s.grow = 1.4f; s.life = 0.5f; s.a0 = 0.4f;
      s.c = {0.62f, 0.6f, 0.66f};
      fxPush(g_fxLow, s);
    }
  }
  singT -= dt;
  if (singT <= 0) {
    singT = 0.04f;
    for (const Singularity& s : g_world.sings) {
      if (!inView(s.pos, cam, kSingRadius)) continue;
      const Vec2 d = frandDir();
      Fx f; f.pos = s.pos + d * frange(150, kSingRadius); f.vel = (d * -1.0f) * 260.0f + Vec2{-d.y, d.x} * 140.0f; f.r = 3.5f;
      f.life = 0.7f; f.c = hueC(0.72f + frange(0, 0.1f)); f.shrink = true;
      fxPush(g_fxLow, f);
    }
  }
}

static void flushMinimap(float S, float pad, Vec2 c) {
  const int px = (int)(150 * g_ui), margin = (int)(16 * g_ui);
  glViewport((int)((canvasW - px - margin) * g_dpr), (int)(margin * g_dpr), (int)(px * g_dpr), (int)(px * g_dpr));
  glUseProgram(g_instProg);
  glUniform2f(iRes, S + 2 * pad, S + 2 * pad);
  glUniform2f(iCam, c.x, c.y);
  glBindVertexArray(g_instVao);
  glBindBuffer(GL_ARRAY_BUFFER, g_instVbo);
  glBufferData(GL_ARRAY_BUFFER, g_insts.size() * sizeof(Inst), g_insts.data(), GL_STREAM_DRAW);
  glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)g_insts.size());
  glViewport(0, 0, (int)(canvasW * g_dpr), (int)(canvasH * g_dpr));
}

// Inside a temple the minimap only shows the walls in a radius around you (the page writes "Arena Temple" above it).
static void drawTempleMap(const Tank& me, Vec2 mePos, float meAim) {
  const float S = 2400.0f, pad = 60.0f, k = S / 10000.0f;
  g_insts.clear();
  const TempleCols tc = templeCols(templeStyle(me.temple));
  const Col frame{0.165f, 0.13f, 0.25f};
  rectI(mePos.x, mePos.y, S * 0.5f + pad, S * 0.5f + pad, 0, {0.07f, 0.06f, 0.1f}).ol = 0;
  for (const Wall& w : g_world.walls) {
    if (w.temple != me.temple || w.kind != 1) continue;
    const Vec2 m = (w.a + w.b) * 0.5f, d = w.b - w.a;
    const float dist = (m - mePos).len();
    if (dist > S * 0.5f) continue;
    const float h = std::max(34.0f, w.thick * 0.6f);
    Inst& i = capsule(m.x, m.y, d.len() * 0.5f + h, h, std::atan2(d.y, d.x), mixC(tc.stone, tc.trim, 0.45f));
    i.ol = 0; i.alpha = std::clamp((S * 0.5f - dist) / 250.0f, 0.0f, 1.0f);
  }
  const Temple& T = g_world.temples[me.temple];
  if (T.cleared && (T.c - mePos).len() < S * 0.5f) circle(T.c.x, T.c.y, 90, {0.45f, 1.0f, 0.6f}).ol = 0;
  poly(mePos.x, mePos.y, 320.0f * k, meAim - kPi * 0.5f, 3, g_myLook.c1).ol = 90.0f * k;
  const float e = S * 0.5f + pad * 0.5f;
  rectI(mePos.x, mePos.y - e, e + pad * 0.5f, pad * 0.5f, 0, frame).ol = 0;
  rectI(mePos.x, mePos.y + e, e + pad * 0.5f, pad * 0.5f, 0, frame).ol = 0;
  rectI(mePos.x - e, mePos.y, pad * 0.5f, e + pad * 0.5f, 0, frame).ol = 0;
  rectI(mePos.x + e, mePos.y, pad * 0.5f, e + pad * 0.5f, 0, frame).ol = 0;
  flushMinimap(S, pad, mePos);
}

// Minimap (bottom-right): the arena, the storm, the temples and you. With the Radar relic it also shows walls and
// enemies. A red X marks where the next Legendary Crate lands.
static void drawMinimap(const Tank& me, Vec2 mePos, float meAim) {
  if (me.alive && me.temple >= 0 && me.temple < (int)g_world.temples.size()) { drawTempleMap(me, mePos, meAim); return; }
  const float S = kMapSize, mid = kMapSize * 0.5f, pad = S * 0.035f, k = S / 10000.0f;  // k: dot sizes scale with the map
  g_insts.clear();
  const Col frame{0.165f, 0.13f, 0.25f}, ground{0.93f, 0.91f, 0.86f};
  rectI(mid, mid, kMapSize * 0.5f, kMapSize * 0.5f, 0, ground).ol = 0;
  const RingState& r = g_world.ring;
  if (g_world.ringEnabled && r.cur.r < 1e8f) {
    rectI(mid, mid, kMapSize * 0.5f, kMapSize * 0.5f, 0, {0.74f, 0.66f, 0.95f}).ol = 0;
    circle(r.cur.c.x, r.cur.c.y, r.cur.r, ground).ol = 0;
    if (r.next.r < r.cur.r - 5.0f) {
      circle(r.next.c.x, r.next.c.y, r.next.r + 60.0f, {0.35f, 0.27f, 0.6f}).ol = 0;
      circle(r.next.c.x, r.next.c.y, r.next.r, ground).ol = 0;
    }
  }
  if (g_world.sandbox) {  // outside the sandbox's play area
    const Col off{0.62f, 0.59f, 0.55f};
    const Vec2 a = g_world.lo, b = g_world.hi;
    rectI(a.x * 0.5f, mid, a.x * 0.5f, mid, 0, off).ol = 0;
    rectI((b.x + S) * 0.5f, mid, (S - b.x) * 0.5f, mid, 0, off).ol = 0;
    rectI(mid, a.y * 0.5f, mid, a.y * 0.5f, 0, off).ol = 0;
    rectI(mid, (b.y + S) * 0.5f, mid, (S - b.y) * 0.5f, 0, off).ol = 0;
  }
  const Tank& player = g_world.tanks[0];
  const bool radar = player.alive && hasRelic(player, Relic::Radar);
  if (radar) {
    for (const Wall& w : g_world.walls) {
      const Vec2 m = (w.a + w.b) * 0.5f, d = w.b - w.a;
      capsule(m.x, m.y, d.len() * 0.5f + 40.0f * k, 40.0f * k, std::atan2(d.y, d.x), {0.6f, 0.58f, 0.7f}).ol = 0;
    }
    for (const Tank& t : g_world.tanks) {
      if (!t.alive || t.id == 1 || isHidden(t)) continue;
      if (t.temple >= 0) continue;
      circle(t.pos.x, t.pos.y, (t.guardian ? 190.0f : 130.0f) * k, t.guardian ? Col{1.0f, 0.72f, 0.2f} : t.sentryOf ? Col{0.6f, 0.6f, 0.65f} : kEnemyCol).ol = 40.0f * k;
    }
  }
  for (const Temple& T : g_world.temples) {  // temples: a little gold pyramid; a grey dot once cleared
    if (T.cleared) { circle(T.door.x, T.door.y, 140.0f * k, {0.6f, 0.58f, 0.62f}).ol = 0; continue; }
    const TempleCols tc = templeCols(T.style);
    poly(T.door.x, T.door.y + 40.0f * k, 270.0f * k, kPi, 3, tc.trim).ol = 50.0f * k;
  }
  if (g_legendMark) {
    for (float a : {0.7854f, -0.7854f}) rectI(g_legendPos.x, g_legendPos.y, 360.0f * k, 80.0f * k, a, {0.9f, 0.12f, 0.15f}).ol = 30.0f * k;
  }
  if (me.alive) poly(mePos.x, mePos.y, 320.0f * k, meAim - kPi * 0.5f, 3, g_myLook.c1).ol = 90.0f * k;
  const float e = S * 0.5f + pad * 0.5f;
  rectI(mid, mid - e, e + pad * 0.5f, pad * 0.5f, 0, frame).ol = 0;
  rectI(mid, mid + e, e + pad * 0.5f, pad * 0.5f, 0, frame).ol = 0;
  rectI(mid - e, mid, pad * 0.5f, e + pad * 0.5f, 0, frame).ol = 0;
  rectI(mid + e, mid, pad * 0.5f, e + pad * 0.5f, 0, frame).ol = 0;
  flushMinimap(S, pad, {mid, mid});
}

// ---------------- HUD feed ----------------
static void updateHud() {
  const Tank& cam = camTank();
  const Tank& t = g_world.tanks[0];
  {
    const RingState& r = g_world.ring;
    const bool outside = g_world.ringEnabled && cam.alive && !insideZone(r.cur, cam.pos);
    EM_ASM({ if (window.tonkMatch) window.tonkMatch($0, $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13); },
           g_world.aliveCount(), g_world.playerCount(), r.phase, r.shrinking ? 1 : 0, (int)(r.secsLeft * 10),
           (int)r.dmgPerSec, outside ? 1 : 0, g_world.matchOver ? 1 : 0, (int)t.place,
           g_world.winnerId == 1 ? 1 : 0, (int)g_world.time, (int)(r.cur.r), (int)g_world.winnerId, (int)t.kills);
    EM_ASM({ if (window.tonkBot) window.tonkBot($0, $1, $2, $3, $4, $5, $6, $7, $8, $9); },
           (int)cam.id, (int)cam.mind.objective, (int)cam.mind.targetId, (int)cam.health, (int)std::lround(cam.mind.aggression * 50),
           (int)cam.mind.tier, (int)cam.held, (int)cam.kills, g_spectateId ? 1 : 0, (int)cam.mind.style | (int)cam.mind.brain << 4);
    EM_ASM({ if (window.tonkBlind) window.tonkBlind($0); }, cam.alive ? (int)(cam.blindT * 100) : 0);
  }
  {  // Arena Temple: in one? time left, and is its portal open
    const bool in = t.alive && t.temple >= 0 && t.temple < (int)g_world.temples.size();
    EM_ASM({ if (window.tonkTemple) window.tonkTemple($0, $1, $2, $3, $4); }, in ? 1 : 0, in ? (int)((kTempleTime - t.templeT) * 10) : 0,
           in && g_world.temples[t.temple].cleared ? 1 : 0, in ? g_world.temples[t.temple].style : 0, (int)(100 * t.portalT / kExitHold));
  }
  if (g_mode == 1) {  // Rocket Rampage: time left, your score and rank, the top five
    std::vector<const Tank*> rank;
    for (const Tank& k : g_world.tanks) if (!npc(k)) rank.push_back(&k);
    std::stable_sort(rank.begin(), rank.end(), [](const Tank* a, const Tank* b) { return a->score > b->score || (a->score == b->score && a->kills > b->kills); });
    int myRank = 1, ids[5] = {}, sc[5] = {};
    for (size_t i = 0; i < rank.size(); ++i) {
      if (rank[i]->id == 1) myRank = (int)i + 1;
      if (i < 5) { ids[i] = (int)rank[i]->id; sc[i] = rank[i]->score; }
    }
    EM_ASM({ if (window.tonkRampage) window.tonkRampage($0, $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13); },
           (int)(std::max(0.0f, kRampageLen - g_world.time) * 10), (int)t.score, myRank, (int)t.kills, ids[0], sc[0], ids[1], sc[1], ids[2], sc[2],
           ids[3], sc[3], ids[4], sc[4]);
  }
  const bool rampageOver = g_mode == 1 && g_world.matchOver;
  if (!g_menu && !g_resultSent && (rampageOver || (!g_world.allowRespawn && (!t.alive || (g_world.matchOver && g_world.winnerId == 1))))) {
    g_resultSent = true;
    const bool won = rampageOver ? t.place == 1 : t.alive;
    EM_ASM({ if (window.tonkExtra) window.tonkExtra($0, $1, $2, $3, $4); }, g_myCrates, g_myLegend, g_myMobs, g_myMages, (int)t.score);
    EM_ASM({ if (window.tonkResult) window.tonkResult($0, $1, $2, $3, $4, $5, $6, $7, $8); }, won ? 1 : 0, rampageOver ? 0 : (int)t.killedBy,
           (int)t.xp, won || rampageOver ? (int)g_world.time : (int)t.diedAt, won ? 1 : (int)t.place, g_world.playerCount(), (int)t.kills,
           g_myUpgrades, g_myPowerUses);
  }
  EM_ASM({ if (window.tonkHud) window.tonkHud($0, $1, $2, $3, $4); },
         (int)t.xp, (int)t.kills, (int)t.health, (int)t.maxHealth, t.alive ? 1 : 0);
  EM_ASM({ if (window.tonkStats) window.tonkStats($0, $1, $2, $3, $4, $5, $6); },
         (int)t.stat[0], (int)t.stat[1], (int)t.stat[2], (int)t.stat[3], (int)t.stat[4], (int)t.stat[5], (int)t.stat[6]);
  {  // superpower bar: 0 ready, 1 active, 2 cooldown; plus pickups we could swap to with F
    int state = 0, frac = 1000, nearKind = 0, nearRelic = 0;
    if (t.active != Power::None) {
      state = 1;
      const float dur = powerInfo(t.active).duration;
      frac = t.active == Power::Instinct ? (int)(1000 * t.dodges / kInstinctDodges) : (int)(1000 * std::max(0.0f, t.activeLeft) / std::max(0.01f, dur));
    } else if (t.recharge > 0 && t.held != Power::None) {
      state = 2;
      frac = (int)(1000 * (1.0f - t.recharge / powerInfo(t.held).cooldown));
    }
    if (t.alive) {
      const float reach = t.radius + kCrateRadius + 60.0f;
      float best = reach;
      if (t.held != Power::None)
        for (const Crate& c : g_world.crates) {
          const float d = (c.pos - t.pos).len();
          if (d < best && c.type != t.held) { best = d; nearKind = (int)c.type; }
        }
      float bestR = reach;
      if (t.relic[1] != Relic::None)
        for (const RelicPickup& r : g_world.relics) {
          const float d = (r.pos - t.pos).len();
          if (d < bestR && !hasRelic(t, r.kind)) { bestR = d; nearRelic = (int)r.kind; }
        }
      if (nearKind && nearRelic) { if (best <= bestR) nearRelic = 0; else nearKind = 0; }
    }
    const float secs = state == 1 ? t.activeLeft : t.recharge;
    EM_ASM({ if (window.tonkPower) window.tonkPower($0, $1, $2, $3, $4, $5, $6, $7); }, (int)t.held, state, frac, nearKind,
           (int)(secs * 10), nearRelic, (int)(t.pendingT > 0 ? 1000 * t.pendingT / std::max(0.1f, t.dodgeWindow) : 0), (int)t.active);
  }
  {  // relic slots: kind, a state number and an extra value per slot
    int st[2] = {0, 0}, ex[2] = {0, 0};
    for (int s = 0; s < 2; ++s) {
      switch (t.relic[s]) {
        case Relic::Repair: st[s] = t.repairing ? 2 : (t.unharmed > kRepairDelay ? 1 : 0); break;
        case Relic::Sentry:
          st[s] = t.sentryRebuild > 0 ? 3 : t.sentryOver ? 2 : t.sentryOn ? 1 : 0;
          ex[s] = (int)(std::max(0.0f, t.sentryRebuild) * 10);
          break;
        case Relic::Loot:
          for (const Drone& d : g_world.drones)
            if (d.owner == 1 && d.kind == Relic::Loot) { st[s] = d.mode == DroneMode::Kamikaze ? 2 : d.vulnerable() ? 1 : 0; ex[s] = d.hp; }
          if (st[s] < 2 && t.relicHold[s] > 0.05f) ex[s] = 100 + (int)(100 * std::min(1.0f, t.relicHold[s] / kLootHold));
          break;
        case Relic::Radar: st[s] = 1; break;
        default: break;
      }
    }
    EM_ASM({ if (window.tonkRelics) window.tonkRelics($0, $1, $2, $3, $4, $5); }, (int)t.relic[0], (int)t.relic[1], st[0], st[1], ex[0], ex[1]);
  }
  EM_ASM({ window.tonkDebug = Array.of($0, $1, $2, $3); }, t.pos.x, t.pos.y, t.aim,
         (keyW ? 1 : 0) | (keyA ? 2 : 0) | (keyS ? 4 : 0) | (keyD ? 8 : 0));
}

// ---------------- Sim clock ----------------
// The fixed 30 Hz sim runs off real time. The render loop (requestAnimationFrame) drives it while the tab is visible;
// in a background tab the page calls tonk_bg_tick() from a worker timer instead, so the match keeps going.
static double g_simLast = -1, g_simAcc = 0, g_hudAcc = 0, g_lastFrame = 0;
static void advanceSim(Input in) {
  const double now = emscripten_get_now();
  if (g_simLast < 0) g_simLast = now;
  const double dt = std::min(0.25, (now - g_simLast) / 1000.0);
  g_simLast = now;
  g_simAcc += dt * g_timeScale;
  g_hudAcc += dt;
  if (g_spectateId || g_menu) in = Input{};  // watching a bot: our own tank just sits there (turn on God mode to keep it alive)
  std::vector<Input> inputs{in};
  // Queued taps are consumed by the first sim tick that runs; held buttons keep firing.
  while (g_simAcc >= kDt) {
    snapshot();
    stepWorld(g_world, inputs);
    detectEvents();
    g_simAcc -= kDt;
    inputs[0].shoot = in.shoot && mouseDown;
    inputs[0].usePowerUp = false;
    inputs[0].swap = false;
    shootQueued = useQueued = swapQueued = false;
  }
  g_alpha = (float)std::clamp(g_simAcc / kDt, 0.0, 1.0);
  if (g_menu) useQueued = swapQueued = false;

  // After dying in a real match, follow someone still alive; if the one we follow dies, move to the next.
  if (g_menu && g_world.matchOver) setupMenuMatch();  // keep the background alive
  if (!g_world.allowRespawn && !g_world.matchOver) {
    if (!g_world.tanks[0].alive && !g_spectateId) cycleSpectate(1);
    else if (g_spectateId && !camTank().alive) cycleSpectate(1);
  }
  if (g_hudAcc > 0.1) { updateHud(); g_hudAcc = 0; }
}

// ---------------- Frame ----------------
static void frame() {
  double cw, ch;
  emscripten_get_element_css_size("#canvas", &cw, &ch);
  canvasW = std::max(1, (int)cw); canvasH = std::max(1, (int)ch);
  g_dpr = std::clamp((float)emscripten_get_device_pixel_ratio(), 1.0f, 2.0f);
  const int bufW = (int)(canvasW * g_dpr), bufH = (int)(canvasH * g_dpr);
  static int lastW = 0, lastH = 0;
  if (bufW != lastW || bufH != lastH) { emscripten_set_canvas_element_size("#canvas", bufW, bufH); lastW = bufW; lastH = bufH; }
  glViewport(0, 0, bufW, bufH);
  g_zoom = std::max(canvasW / kViewW, canvasH / kViewH);
  if (!g_world.tanks.empty()) g_zoom /= statFovMul(camTank().stat[StatFov]);  // Field of View upgrades show more
  g_ui = std::clamp(std::min(canvasW / 1600.0f, canvasH / 900.0f), 0.55f, 1.5f);

  // Visual clock (real time, for effects).
  static double last = emscripten_get_now();
  const double now = emscripten_get_now();
  g_lastFrame = now;
  // Slow motion and pause slow (or stop) the effects and animations too; fast-forward leaves them at normal speed.
  const double dt = std::min(0.25, (now - last) / 1000.0) * std::min(1.0, (double)g_timeScale);
  last = now;
  g_clock += (float)dt;

  Input in;
  in.moveX = (int8_t)(keyD - keyA);
  in.moveY = (int8_t)(keyS - keyW);
  in.aim = std::atan2(mouseY - canvasH * 0.5f, mouseX - canvasW * 0.5f);
  in.shoot = mouseDown || shootQueued;
  in.usePowerUp = useQueued;
  in.swap = swapQueued;
  in.relicDown[0] = keyR;
  in.relicDown[1] = keyT;
  if (!g_world.tanks.empty()) {  // the world point under the cursor
    const Vec2 me = g_world.tanks[0].pos;
    in.tx = me.x + (mouseX - canvasW * 0.5f) / g_zoom;
    in.ty = me.y + (mouseY - canvasH * 0.5f) / g_zoom;
    g_cursor = {in.tx, in.ty};
  }
  advanceSim(in);

  const Tank& me = camTank();
  const size_t meIdx = (size_t)(&me - &g_world.tanks[0]);
  const Vec2 cam = tankPos(meIdx);
  g_camPos = cam;
  emitTrails((float)dt, cam);
  updateFx(g_fxLow, (float)dt);
  updateFx(g_fxHigh, (float)dt);

  // 1) Arena grid
  glDisable(GL_BLEND);
  glUseProgram(g_bgProg);
  glUniform2f(uRes, (float)canvasW, (float)canvasH);
  glUniform1f(uDpr, g_dpr);
  glUniform1f(uZoom, g_zoom);
  glUniform2f(uCam, cam.x, cam.y);
  const bool inTemple = me.temple >= 0 && me.temple < (int)g_world.temples.size();
  glUniform1f(uRingOn, g_world.ringEnabled && !inTemple ? 1.0f : 0.0f);
  if (inTemple) {
    const Temple& tp = g_world.temples[me.temple];
    glUniform3f(uTemple, tp.c.x, tp.c.y, tp.R);
    glUniform1f(uStyle, (float)tp.style);
    float rr[4] = {0, 0, 0, 0};
    for (size_t k = 0; k < tp.rings.size() && k < 4; ++k) rr[k] = tp.rings[k];
    glUniform4f(uRings, rr[0], rr[1], rr[2], rr[3]);
    glUniform1f(uRingN, (float)std::min<size_t>(4, tp.rings.size()));
    glUniform1f(uThick, tp.thick);
  } else {
    glUniform3f(uTemple, 0, 0, 0);
  }
  glUniform4f(uBounds, g_world.lo.x, g_world.lo.y, g_world.hi.x, g_world.hi.y);
  glUniform3f(uRing, g_world.ring.cur.c.x, g_world.ring.cur.c.y, g_world.ring.cur.r);
  glUniform3f(uNext, g_world.ring.next.c.x, g_world.ring.next.c.y, g_world.ring.next.r);
  glBindVertexArray(0);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  // 2) Everything else (instanced)
  buildInstances(cam, me);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);  // keep the canvas opaque
  glUseProgram(g_instProg);
  glUniform1f(iTime, g_clock);
  if (!g_insts.empty()) {
    glUniform2f(iRes, viewW(), viewH());
    glUniform2f(iCam, cam.x, cam.y);
    glBindVertexArray(g_instVao);
    glBindBuffer(GL_ARRAY_BUFFER, g_instVbo);
    glBufferData(GL_ARRAY_BUFFER, g_insts.size() * sizeof(Inst), g_insts.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)g_insts.size());
  }

  // 2b) Inside a temple: you are the light; the walls cast shadows, and the edges of the screen are dark
  if (inTemple) {
    const Temple& tp = g_world.temples[me.temple];
    glUseProgram(g_fogProg);
    glUniform2f(fRes, (float)canvasW, (float)canvasH);
    glUniform2f(fCam, cam.x, cam.y);
    glUniform1f(fDpr, g_dpr);
    glUniform1f(fZoom, g_zoom);
    glUniform3f(fTemple, tp.c.x, tp.c.y, tp.R);
    glUniform2f(fLight, cam.x, cam.y);
    float ra[20] = {}, bl[16] = {};
    const int rn = (int)std::min<size_t>(5, tp.rings.size()), bn = (int)std::min<size_t>(4, tp.blocks.size() / 2);
    for (int k = 0; k < rn; ++k) { ra[k * 4] = tp.rings[k]; ra[k * 4 + 1] = tp.gapA[k]; ra[k * 4 + 2] = tp.gapHalf[k]; }
    for (int k = 0; k < bn; ++k) {
      const Vec2 a = tp.blocks[k * 2] - tp.c, b = tp.blocks[k * 2 + 1] - tp.c;
      bl[k * 4] = a.x; bl[k * 4 + 1] = a.y; bl[k * 4 + 2] = b.x; bl[k * 4 + 3] = b.y;
    }
    glUniform4fv(fRingA, 5, ra);
    glUniform1f(fRingN, (float)rn);
    glUniform1f(fThick, tp.thick);
    glUniform4fv(fBlock, 4, bl);
    glUniform1f(fBlockN, (float)bn);
    glBindVertexArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
  }

  // 3) Minimap, hidden on the home screen
  if (!g_menu) drawMinimap(me, cam, tankAim(meIdx));

  // 4) Names (and titles) above every tank are drawn by the page on a 2D overlay.
  EM_ASM({ if (window.tonkLabelsBegin) window.tonkLabelsBegin($0, $1, $2); }, canvasW, canvasH, g_zoom);
  for (size_t i = 0; i < g_world.tanks.size(); ++i) {
    const Tank& t = g_world.tanks[i];
    if (!t.alive || isHidden(t)) continue;
    const Vec2 p = tankPos(i);
    if (!inView(p, cam, 0)) continue;
    if (inTemple && (t.temple != me.temple || (t.id != me.id && !templeSight(g_world.temples[me.temple], cam, p))))
      continue;  // names in the dark (behind a wall, or too far from your light) stay hidden too
    EM_ASM({ window.tonkLabel($0, $1, $2, $3); }, (int)t.id, canvasW * 0.5f + (p.x - cam.x) * g_zoom,
           canvasH * 0.5f + (p.y - cam.y - t.radius - 22.0f) * g_zoom, (int)t.sentryOf);
    if (g_botInfo && t.isBot && !t.guardian) {  // the Bot Info Overlay, under the tank
      const BotMind& m = t.mind;
      int stats = 0;
      for (int k = kStatCount - 1; k >= 0; --k) stats = stats * 6 + std::min<int>(5, t.stat[k]);
      // (EM_ASM takes at most 16 arguments, so small values are packed: who, feel, gear, health, likes.)
      const int kind = t.mob ? 1 : t.sentryOf ? 2 : m.custom ? 3 : 0;
      const int who = (int)m.tier | (int)m.style << 4 | (int)m.objective << 8 | kind << 13 | (int)m.brain << 15;  // brain: a preset bot
      const int feel = std::clamp((int)std::lround(m.aggression * 50), 0, 255) | std::clamp((int)std::lround(m.dodgeChance * 100), 0, 255) << 8;
      const int gear = (int)t.held | (int)t.relic[0] << 5 | (int)t.relic[1] << 9;
      const int hp = std::clamp((int)t.health, 0, 4095) | std::clamp((int)t.maxHealth, 0, 4095) << 12;
      const int likes = std::clamp((int)std::lround(m.affLoot * 100), 0, 1023) | std::clamp((int)std::lround(m.affFight * 100), 0, 1023) << 10 |
                        std::clamp((int)std::lround(m.affRaid * 100), 0, 1023) << 20;
      EM_ASM({ if (window.tonkBotInfo) window.tonkBotInfo($0, $1, $2, $3, $4, $5, $6, $7, $8, $9); },
             (int)t.id, canvasW * 0.5f + (p.x - cam.x) * g_zoom, canvasH * 0.5f + (p.y - cam.y + t.radius + 34.0f) * g_zoom,
             who, (int)m.targetId, feel, gear, stats, hp, likes);
    }
  }
  EM_ASM({ if (window.tonkLabelsEnd) window.tonkLabelsEnd(); });
}

// ---- Hooks called by the page (web/ui.js, web/meta.js) and the dev panel ----
extern "C" {
// Cosmetics. slot -1 = the player, anything else adds a look to the pool some bots wear. Colors are 0xRRGGBB.
EMSCRIPTEN_KEEPALIVE void tonk_set_look(int slot, int pat, int c1, int c2, int bpat, int b1, int b2, int trail, int death, int muzzle,
                                        int kill) {
  Look l;
  l.pat = pat; l.c1 = hexCol(c1); l.c2 = hexCol(c2);
  l.bpat = bpat; l.b1 = hexCol(b1); l.b2 = hexCol(b2);
  l.trail = trail; l.death = death; l.muzzle = muzzle; l.kill = kill;
  if (slot < 0) g_myLook = l; else g_lookPool.push_back(l);
}
EMSCRIPTEN_KEEPALIVE void tonk_clear_bot_looks() { g_lookPool.clear(); }
// Background tab: requestAnimationFrame stops, so the page pings this from a worker timer ~20x a second. It only does
// something when no frame has been drawn for a while. Nobody is watching, so effects are dropped and the player's tank
// gets no input (keys can't be read while the tab is in the background).
EMSCRIPTEN_KEEPALIVE void tonk_bg_tick() {
  const double now = emscripten_get_now();
  if (now - g_lastFrame < 150.0) return;           // frames are running: the render loop drives the sim
  if (g_menu) { g_simLast = now; return; }         // the home-screen match can just pause
  keyW = keyA = keyS = keyD = keyR = keyT = mouseDown = false;
  advanceSim(Input{});
  g_fxLow.clear();
  g_fxHigh.clear();
}
// Which pool look a tank wears: -2 the player, -1 none (plain coral), otherwise the index the page sent.
EMSCRIPTEN_KEEPALIVE int tonk_look_slot(int id) {
  if (id == 1) return -2;
  auto it = g_lookSlot.find((uint32_t)id);
  return it == g_lookSlot.end() ? -1 : it->second;
}

EMSCRIPTEN_KEEPALIVE void tonk_debug_give(int kind, int) {  // give the player a superpower (1..14)
  Tank& me = g_world.tanks[0];
  me.held = (Power)std::clamp(kind, 0, kPowerKinds);
  if (isExclusive(me.held) && !g_admin) me.held = Power::None;  // the admin's own
  me.active = Power::None;
  me.recharge = 0;
}
EMSCRIPTEN_KEEPALIVE void tonk_debug_relic(int slot, int kind) {  // put a relic (1..4) in slot 0 or 1
  Tank& me = g_world.tanks[0];
  me.relic[std::clamp(slot, 0, 1)] = (Relic)std::clamp(kind, 0, kRelicKinds);
  if (me.relic[0] == Relic::None) { me.relic[0] = me.relic[1]; me.relic[1] = Relic::None; }
}
// Run N sim ticks with the given input, independent of requestAnimationFrame (which pauses in background tabs).
EMSCRIPTEN_KEEPALIVE void tonk_debug_step(int ticks, int mx, int my, float aim, int shoot, int use) {
  for (int i = 0; i < ticks; ++i) {
    Input in;
    in.moveX = (int8_t)mx; in.moveY = (int8_t)my; in.aim = aim; in.shoot = shoot != 0; in.usePowerUp = use != 0 && i == 0;
    in.tx = g_world.tanks[0].pos.x + std::cos(aim) * 400; in.ty = g_world.tanks[0].pos.y + std::sin(aim) * 400;
    snapshot();
    stepWorld(g_world, std::vector<Input>{in});
    detectEvents();
  }
}
// 0 x, 1 y, 2 held, 3 active, 4 cooldown, 5 health, 6 xp, 7 crates, 8 rockets, 9 bullets, 10 aim
EMSCRIPTEN_KEEPALIVE double tonk_debug_info(int what) {
  const Tank& t = g_world.tanks[0];
  switch (what) {
    case 0: return t.pos.x; case 1: return t.pos.y; case 2: return (int)t.held; case 3: return (int)t.active;
    case 4: return t.recharge; case 5: return t.health; case 6: return t.xp; case 7: return (double)g_world.crates.size();
    case 8: return (double)g_world.rockets.size(); case 9: return (double)g_world.bullets.size(); default: return t.aim;
  }
}
EMSCRIPTEN_KEEPALIVE void tonk_restart(int bots, int respawn, int seed) {
  g_botCount = bots; g_respawn = respawn != 0; g_menu = false;
  setupMatch(g_mode == 1 ? std::min(bots, kRampageTanks - 1) : bots, g_respawn, seed ? (uint32_t)seed : (uint32_t)emscripten_get_now(), g_mode);
}
EMSCRIPTEN_KEEPALIVE void tonk_play(int respawn) {  // from the home screen: fresh match, you in it
  g_menu = false;
  g_respawn = respawn != 0;
  setupMatch(g_botCount, g_respawn, (uint32_t)emscripten_get_now() | 1u);
}
// mode: 0 Battle Royale, 1 Rocket Rampage.
EMSCRIPTEN_KEEPALIVE void tonk_play_mode(int mode, int respawn) {
  g_menu = false;
  g_respawn = respawn != 0;
  setupMatch(mode == 1 ? kRampageTanks - 1 : g_botCount, g_respawn, (uint32_t)emscripten_get_now() | 1u, mode);  // Rampage: 20 tanks
}
EMSCRIPTEN_KEEPALIVE int tonk_owner_of(int id) {  // a Sentry Robot's owner (0 for everyone else)
  const Tank* t = g_world.find((uint32_t)id);
  return t ? (int)t->sentryOf : 0;
}
EMSCRIPTEN_KEEPALIVE int tonk_map() { return g_world.mapId; }
EMSCRIPTEN_KEEPALIVE void tonk_debug_legend() {  // dev panel: the next Legendary Crate is announced now and lands in 10 s
  if (!g_world.ringEnabled || g_world.mode != Mode::Royale) return;
  g_world.legendAt = g_world.time + kLegendWarn + 0.1f;
  g_world.legendWarned = false;
}
EMSCRIPTEN_KEEPALIVE void tonk_debug_temple() {  // dev panel: stand next to the nearest open temple (again: go in)
  Tank& me = g_world.tanks[0];
  if (me.temple >= 0) return;
  const Temple* best = nullptr;
  for (const Temple& t : g_world.temples) if (!t.cleared && (!best || (t.door - me.pos).len() < (best->door - me.pos).len())) best = &t;
  if (!best) return;
  me.pos = (best->door - me.pos).len() < 500.0f ? best->door : best->door + Vec2{0, 330};  // a second click: step in
  me.vel = {};
}
EMSCRIPTEN_KEEPALIVE void tonk_debug_crate() {  // dev panel: stand next to the nearest loot crate
  Tank& me = g_world.tanks[0];
  const LootBox* best = nullptr;
  for (const LootBox& b : g_world.boxes) if (b.kind != BoxKind::Small && (!best || (b.pos - me.pos).len() < (best->pos - me.pos).len())) best = &b;
  if (best && me.temple < 0) { me.pos = best->pos + Vec2{-220, 0}; me.vel = {}; }
}
EMSCRIPTEN_KEEPALIVE void tonk_to_menu() { g_menu = true; g_timeScale = 1.0f; setupMenuMatch(); }
EMSCRIPTEN_KEEPALIVE void tonk_set_timescale(float s) { g_timeScale = s <= 0.0f ? 0.0f : (s > 120.0f ? 120.0f : std::max(s, 0.05f)); }
EMSCRIPTEN_KEEPALIVE void tonk_set_god(int on) { g_godOpt = on != 0; g_world.godId = on ? 1 : 0; }
EMSCRIPTEN_KEEPALIVE void tonk_set_debug_bots(int on) { g_debugBots = on != 0; }
EMSCRIPTEN_KEEPALIVE void tonk_set_bot_info(int on) { g_botInfo = on != 0; }
// Dev panel [Bots]: make a bot next to you. aim / dodge: 0..100, or -1 for the tier's own; aggr 0..100 (50 = neutral);
// power: stat multiplier x100 (Omnipotent bots also boost it in a fight); stats packed base 6 (stat k = digit k).
// Returns the new bot's id (5001 and up).
EMSCRIPTEN_KEEPALIVE int tonk_debug_spawn_bot(int tier, int style, int aggr, int aim, int dodge, int power, int held,
                                             int relic0, int relic1, int stats) {
  if (g_world.tanks.empty()) return 0;
  const Tank me = g_world.tanks[0];
  const Vec2 fwd{std::cos(me.aim), std::sin(me.aim)};
  Vec2 pos = me.pos + fwd * 420.0f;
  if (me.temple < 0) { pos.x = std::clamp(pos.x, 100.0f, kMapSize - 100.0f); pos.y = std::clamp(pos.y, 100.0f, kMapSize - 100.0f); }
  pos = g_world.clearOf(pos, 40.0f);
  const uint32_t id = g_nextDevBot++;
  Tank& b = g_world.addTank(id, pos, true);
  b.temple = me.temple;
  botSeed(g_world, b, std::clamp(tier, 0, (int)BotTier::Count - 1));
  BotMind& m = b.mind;
  m.custom = true;
  m.style = (BotStyle)std::clamp(style, 0, (int)BotStyle::Count - 1);
  m.aggression = std::clamp(aggr, 0, 100) / 50.0f;
  m.templeFan = m.style == BotStyle::Raider || m.templeFan;
  botAffinities(g_world, b);
  if (aim >= 0) {  // 0 hopeless .. 100 perfect
    const float a = std::clamp(aim, 0, 100) / 100.0f;
    m.aimNoise = 0.3f * std::pow(1.0f - a, 1.5f) + 0.006f;
    m.lead = a; m.trackTau = 0.9f - 0.87f * a;
    m.reactMin = 1.2f - 1.12f * a; m.reactMax = m.reactMin * 1.5f;
  }
  if (dodge >= 0) {
    const float d = std::clamp(dodge, 0, 100) / 100.0f;
    m.dodgeChance = d; m.dodgeReact = 0.6f - 0.54f * d;
  }
  m.basePower = std::clamp(power, 25, 500) / 100.0f;
  b.powerMul = m.basePower;
  for (int k = 0; k < kStatCount; ++k) { b.stat[k] = (uint8_t)std::min(5, stats % 6); stats /= 6; }
  b.maxHealth = kMaxHealth + statHealthBonus(b.stat[StatHealth]);
  b.health = b.maxHealth;
  b.held = (Power)std::clamp(held, 0, kPowerKinds);
  if (isExclusive(b.held)) b.held = Power::None;
  b.relic[0] = (Relic)std::clamp(relic0, 0, kRelicKinds);
  b.relic[1] = (Relic)std::clamp(relic1, 0, kRelicKinds);
  if (b.relic[0] == Relic::None) { b.relic[0] = b.relic[1]; b.relic[1] = Relic::None; }
  if (g_world.mode == Mode::Rampage) { b.held = Power::Rocket; b.cdScale = kRampageCd; }
  pickLook(b, id * 2654435761u + 977u);
  g_tfx.resize(g_world.tanks.size(), TankFx{});
  return (int)id;
}
// Dev panel [Bots > Presets]: one of the hand-made bots (1 Aegis-Vulture, 2 Ghost Current, 3 Carrion Clock; core/bot_presets.h).
// Same power as everyone: no stat multiplier, no upgrades, no superpower or relics. spread: appear at a random spot in the
// arena (so several presets don't start on top of each other) instead of next to you. Battle Royale rules only (they fall
// back to the normal brain in Rocket Rampage and inside temples). Returns the new bot's id (5001 and up), or 0.
EMSCRIPTEN_KEEPALIVE int tonk_debug_spawn_preset(int brain, int spread) {
  if (g_world.tanks.empty() || g_world.mode != Mode::Royale) return 0;
  const Tank me = g_world.tanks[0];
  Vec2 pos;
  int temple = -1;
  if (spread || me.temple >= 0) {
    pos = g_world.spawnSpot();
  } else {
    pos = me.pos + Vec2{std::cos(me.aim), std::sin(me.aim)} * 420.0f;
    pos.x = std::clamp(pos.x, 100.0f, kMapSize - 100.0f);
    pos.y = std::clamp(pos.y, 100.0f, kMapSize - 100.0f);
    pos = g_world.clearOf(pos, 40.0f);
  }
  const uint32_t id = g_nextDevBot++;
  Tank& b = g_world.addTank(id, pos, true);
  b.temple = (int8_t)temple;
  botSeed(g_world, b, (int)BotTier::Skilled);
  BotMind& m = b.mind;
  m.custom = true;
  m.brain = (uint8_t)std::clamp(brain, 1, (int)Brain::Count - 1);
  m.style = BotStyle::Tactician;
  m.aggression = 1.0f;
  m.templeFan = false;
  botAffinities(g_world, b);
  m.basePower = 1.0f;
  b.powerMul = 1.0f;
  pickLook(b, id * 2654435761u + 977u);
  g_tfx.resize(g_world.tanks.size(), TankFx{});
  return (int)id;
}
// Dev panel: bots, guardians and turrets don't notice you (you fade out a little). Kept for the next match too.
EMSCRIPTEN_KEEPALIVE void tonk_set_ghost(int on) {
  g_ghostOpt = on != 0;
  if (!g_world.tanks.empty()) g_world.tanks[0].ghost = g_ghostOpt && !g_menu;
}
// Dev panel lines: 0 off, 1 draw red lines (targets), 2 draw blue lines (teams).
EMSCRIPTEN_KEEPALIVE void tonk_set_link_mode(int m) {
  g_linkMode = std::clamp(m, 0, 2);
  g_linkSel = g_linkPress = 0; g_linkDown = false;
  mouseDown = false;
  linkNote(0);
}
EMSCRIPTEN_KEEPALIVE void tonk_show_links(int on) { g_linkShow = on != 0; }
EMSCRIPTEN_KEEPALIVE void tonk_icons_ready() { g_iconsReady = true; }  // the icon atlas has loaded
// which: 0 every line, 1 red, 2 blue; picked: only the picked tank's lines.
EMSCRIPTEN_KEEPALIVE void tonk_clear_links(int which, int picked) {
  if (picked && !g_linkSel) return;
  g_world.clearLinks(which, picked ? g_linkSel : 0);
  for (Tank& t : g_world.tanks) if (t.isBot) t.mind.decideTimer = 0;
  linkNote(0);
}
// Dev panel, quick lines: red lines from some bots to one tank. who: 0 the bot you're watching, 1 every bot, 2 the bots
// you made. target: 0 none (their red lines go), 1 you, 2 the bot you're watching, 3 the bot nearest to you.
// Returns how many bots changed.
EMSCRIPTEN_KEEPALIVE int tonk_debug_set_target(int who, int target) {
  if (g_world.tanks.empty()) return 0;
  const Tank& me = g_world.tanks[0];
  uint32_t tid = 0;
  if (target == 1) tid = 1;
  else if (target == 2) tid = g_spectateId;
  else if (target == 3) {
    float best = 1e18f;
    for (const Tank& t : g_world.tanks)
      if (t.alive && t.id != 1 && !t.guardian && t.temple == me.temple && (t.pos - me.pos).len() < best) { best = (t.pos - me.pos).len(); tid = t.id; }
  }
  if (target != 0 && !tid) return 0;
  int n = 0;
  for (Tank& t : g_world.tanks) {
    if (!t.alive || !t.isBot || t.guardian || t.id == tid) continue;
    const bool pick = who == 1 || (who == 2 && t.mind.custom) || (who == 0 && t.id == g_spectateId);
    if (!pick) continue;
    auto& L = g_world.links;
    for (size_t i = L.size(); i-- > 0;) if (!L[i].team && L[i].from == t.id) L.erase(L.begin() + i);
    if (tid) g_world.toggleLink(t.id, tid, false);
    t.mind.decideTimer = 0;
    ++n;
  }
  linkNote(0);
  return n;
}
// The page's Sandbox setup. mode 0 Battle Royale rules, 1 Rocket Rampage rules; map 0-2, 3 empty; size: the play
// area's side (2,000-14,000); fog: 0 none, 1 the mode's own, 2 a circle that closes from r0 to r1 over fogTime seconds
// (starting 20 s in), 3 a fixed circle r0; loot x100 (0 none, 100 normal, 250 lots); tier -1 a normal mix.
EMSCRIPTEN_KEEPALIVE void tonk_play_sandbox(int mode, int map, int size, int fog, int r0, int r1, int fogTime, int bots,
                                            int respawn, int temples, int loot, int tier) {
  g_sand.mode = mode; g_sand.map = std::clamp(map, 0, 3); g_sand.size = size; g_sand.fog = std::clamp(fog, 0, 3);
  g_sand.fogR0 = std::max(400, r0); g_sand.fogR1 = std::max(200, r1); g_sand.fogTime = fogTime;
  g_sand.bots = std::clamp(bots, 0, 39); g_sand.respawn = respawn != 0; g_sand.temples = temples != 0;
  g_sand.loot = std::clamp(loot, 0, 400) / 100.0f; g_sand.tier = std::clamp(tier, -1, (int)BotTier::Count - 1);
  g_menu = false;
  g_sandOn = true;
  g_respawn = g_sand.respawn;
  setupMatch(g_sand.bots, g_sand.respawn, (uint32_t)emscripten_get_now() | 1u, mode == 1 ? 1 : 0);
  if (mode == 1) g_world.allowRespawn = g_sand.respawn;
  g_sandOn = false;
}
EMSCRIPTEN_KEEPALIVE int tonk_is_sandbox() { return g_world.sandbox ? 1 : 0; }
// The admin account (FitCocoIvy, after its passcode): its panel's commands. Returns 1 on, 2 off (for toggles), 0 nothing.
EMSCRIPTEN_KEEPALIVE void tonk_set_admin(int on) { g_admin = on != 0; if (!g_admin) g_disco = false; }
EMSCRIPTEN_KEEPALIVE int tonk_admin(int cmd) {
  if (!g_admin || g_menu || g_world.tanks.empty()) return 0;
  World& w = g_world;
  Tank& me = w.tanks[0];
  auto others = [&](auto f) { for (Tank& t : w.tanks) if (t.alive && t.id != 1 && t.temple == me.temple && !t.sentryOf) f(t); };
  auto around = [&](float r0, float r1) { const Vec2 d = frandDir(); return me.pos + d * frange(r0, r1); };
  switch (cmd) {
    case 0: me.radius = kTankRadius * 2.2f; return 1;   // Giant
    case 1: me.radius = kTankRadius * 0.55f; return 1;  // Tiny
    case 2: me.radius = kTankRadius; return 1;          // normal size
    case 3: me.ringShots = me.ringShots ? 0 : 12; return me.ringShots ? 1 : 2;  // Bullet Storm
    case 4: me.superBounce = !me.superBounce; return me.superBounce ? 1 : 2;   // Bouncy Bullets
    case 5:  // Meteor Shower: 36 meteors around you over 6 seconds
      for (int i = 0; i < 36; ++i) {
        Bomb b; b.id = w.newId(); b.owner = 1; b.pos = around(230.0f, 1100.0f); b.fuse = 0.4f + 5.5f * i / 36.0f;
        b.blast = 125.0f; b.dmg = 35.0f;
        w.bombs.push_back(b);
      }
      return 1;
    case 6: {  // Black Hole: a huge Singularity under the cursor
      Singularity s; s.id = w.newId(); s.owner = 1; s.pos = g_cursor; s.life = kSingLife * 1.5f;
      w.sings.push_back(s);
      return 1;
    }
    case 7:  // Shockwave: everyone near you is flung away
      others([&](Tank& t) {
        const Vec2 d = t.pos - me.pos;
        const float l = d.len();
        if (l > 1.0f && l < 1100.0f) { t.vel = d * (1.0f / l) * (1900.0f * (1.0f - l / 1200.0f)); damageTank(w, t, 15.0f, 1); }
      });
      w.emit(EvType::Boom, me.pos, 420.0f, 1, 1);
      return 1;
    case 8: others([&](Tank& t) { if (!t.guardian) t.stunT = std::max(t.stunT, 5.0f); }); return 1;  // Freeze
    case 9:  // Loot Rain
      for (int i = 0; i < 24; ++i) w.spawnUpgrade(around(80.0f, 650.0f), w.rng.f01() < 0.4f ? 4 : 3);
      for (int i = 0; i < 4; ++i) w.spawnCrate(around(120.0f, 650.0f));
      for (int i = 0; i < 2; ++i) w.spawnRelic(around(120.0f, 650.0f));
      return 1;
    case 10:  // Minion Army: four Sentry Robots on your side (60 s)
      for (int i = 0; i < 4; ++i) {
        Tank s;
        s.id = w.nextSentry++; s.isBot = true; s.sentryOf = 1; s.temple = me.temple;
        for (int k = 0; k < kStatCount; ++k) s.stat[k] = me.stat[k];
        s.maxHealth = s.health = me.maxHealth;
        const float a = i * 1.5708f + 0.785f;
        s.pos = w.clearOf(me.pos + Vec2{std::cos(a), std::sin(a)} * 110.0f, 40.0f);
        w.tanks.push_back(s);
      }
      g_tfx.resize(w.tanks.size(), TankFx{});
      return 1;
    case 11: {  // Swap places with a random bot
      std::vector<Tank*> c;
      others([&](Tank& t) { if (!t.guardian && !t.mob) c.push_back(&t); });
      if (c.empty()) return 0;
      Tank& o = *c[(size_t)(frand() * c.size()) % c.size()];
      w.emit(EvType::Teleport, me.pos, 0, 1);
      std::swap(me.pos, o.pos);
      me.vel = o.vel = {};
      w.emit(EvType::Teleport, me.pos, 1, 1);
      return 1;
    }
    case 12: {  // Come Here: every bot appears in a ring around you
      int k = 0;
      others([&](Tank& t) {
        if (t.guardian || t.mob) return;
        const float a = k++ * 0.7f;
        t.pos = w.clearOf(me.pos + Vec2{std::cos(a), std::sin(a)} * (520.0f + 60.0f * (k % 5)), t.radius + 10.0f);
        t.vel = {};
        w.emit(EvType::Teleport, t.pos, 1, t.id);
      });
      return 1;
    }
    case 13: g_disco = !g_disco; return g_disco ? 1 : 2;  // Disco
    case 14: me.powerMul = me.powerMul > 1.01f ? 1.0f : 2.0f; return me.powerMul > 1.01f ? 1 : 2;  // Overdrive
    case 15:  // Nuke: a huge blast around you (it can't hurt you)
      explode(w, me.pos, 1300.0f, 1, 120.0f, 0, 0, 0, 1);
      for (int i = 0; i < 6; ++i) w.emit(EvType::Boom, around(0.0f, 700.0f), 300.0f, 1, 1);
      return 1;
    case 16: others([&](Tank& t) { if (t.isBot && !npc(t)) setBotTier(t, BotTier::Newbie); }); return 1;  // Everyone's a Newbie
    case 17: others([&](Tank& t) { if (t.isBot && !npc(t)) { setBotTier(t, BotTier::Omnipotent); t.mind.basePower = 1.6f; } }); return 1;  // Chaos
    case 18: me.held = Power::CocoBarrage; me.active = Power::None; me.recharge = 0; return 1;
    case 19: me.held = Power::IvySnare; me.active = Power::None; me.recharge = 0; return 1;
    case 20:  // Max me: every stat Legendary, full health
      for (int k = 0; k < kRoyaleStats; ++k) me.stat[k] = 5;
      me.maxHealth = kMaxHealth + statHealthBonus(5);
      me.health = me.maxHealth;
      return 1;
    default: return 0;
  }
}
// Dev panel: take every dev-made bot out of the match.
EMSCRIPTEN_KEEPALIVE void tonk_debug_clear_bots() {
  for (Tank& t : g_world.tanks)
    if (t.mind.custom) { t.alive = false; t.gone = true; t.health = 0; }
}
EMSCRIPTEN_KEEPALIVE void tonk_set_respawn(int on) { g_respawn = on != 0; g_world.allowRespawn = g_respawn; }
EMSCRIPTEN_KEEPALIVE void tonk_add_time(float sec) { g_world.time += sec; }
EMSCRIPTEN_KEEPALIVE void tonk_skip_ring() {  // jump to just after the next ring event (shrink start, or shrink end)
  const RingState r = ringAt(g_world);
  if (r.secsLeft < 1e8f) g_world.time += r.secsLeft + 0.05f;
}
EMSCRIPTEN_KEEPALIVE void tonk_spectate(int dir) {  // 0 = back to the player, +1/-1 = next/previous living bot
  if (dir == 0) g_spectateId = 0; else cycleSpectate(dir);
}
EMSCRIPTEN_KEEPALIVE void tonk_teleport_edge() {  // drop the player outside the ring to watch the damage
  Tank& me = g_world.tanks[0];
  const Zone& z = g_world.ring.cur;
  Vec2 d = (me.pos - z.c).norm();
  if (d.len() < 0.5f) d = {1, 0};
  Vec2 p = z.c + d * (z.r + 200.0f);
  me.pos = {std::clamp(p.x, 50.0f, kMapSize - 50.0f), std::clamp(p.y, 50.0f, kMapSize - 50.0f)};
}
EMSCRIPTEN_KEEPALIVE void tonk_debug_goto(float x, float y) {  // move the player (dev panel: "Go to middle")
  Tank& me = g_world.tanks[0];
  if (me.temple >= 0) return;
  me.pos = {std::clamp(x, 40.0f, kMapSize - 40.0f), std::clamp(y, 40.0f, kMapSize - 40.0f)};
  me.vel = {};
}
EMSCRIPTEN_KEEPALIVE void tonk_debug_goto_crate() {  // teleport next to the nearest crate
  Tank& me = g_world.tanks[0];
  float best = 1e18f; const Crate* pick = nullptr;
  for (const Crate& c : g_world.crates) {
    float d = (c.pos - me.pos).len();
    if (d < best) { best = d; pick = &c; }
  }
  if (pick) me.pos = pick->pos + Vec2{70, 0};
}
}

int main() {
  EmscriptenWebGLContextAttributes attr;
  emscripten_webgl_init_context_attributes(&attr);
  attr.majorVersion = 2;
  attr.antialias = true;
  g_ctx = emscripten_webgl_create_context("#canvas", &attr);
  if (g_ctx <= 0) { std::printf("WebGL2 not available\n"); return 1; }
  emscripten_webgl_make_context_current(g_ctx);

  g_bgProg = link(kBgVert, kBgFrag);
  uRes = glGetUniformLocation(g_bgProg, "uRes");
  uCam = glGetUniformLocation(g_bgProg, "uCam");
  uDpr = glGetUniformLocation(g_bgProg, "uDpr");
  uZoom = glGetUniformLocation(g_bgProg, "uZoom");
  uRing = glGetUniformLocation(g_bgProg, "uRing");
  uNext = glGetUniformLocation(g_bgProg, "uNext");
  uRingOn = glGetUniformLocation(g_bgProg, "uRingOn");
  uBounds = glGetUniformLocation(g_bgProg, "uBounds");
  uTemple = glGetUniformLocation(g_bgProg, "uTemple");
  uStyle = glGetUniformLocation(g_bgProg, "uStyle");
  uRings = glGetUniformLocation(g_bgProg, "uRings");
  uRingN = glGetUniformLocation(g_bgProg, "uRingN");
  uThick = glGetUniformLocation(g_bgProg, "uThick");
  g_fogProg = link(kBgVert, kFogFrag);
  fRes = glGetUniformLocation(g_fogProg, "uRes");
  fCam = glGetUniformLocation(g_fogProg, "uCam");
  fDpr = glGetUniformLocation(g_fogProg, "uDpr");
  fZoom = glGetUniformLocation(g_fogProg, "uZoom");
  fTemple = glGetUniformLocation(g_fogProg, "uTemple");
  fLight = glGetUniformLocation(g_fogProg, "uLight");
  fRingA = glGetUniformLocation(g_fogProg, "uRingA");
  fRingN = glGetUniformLocation(g_fogProg, "uRingN");
  fThick = glGetUniformLocation(g_fogProg, "uThick");
  fBlock = glGetUniformLocation(g_fogProg, "uBlock");
  fBlockN = glGetUniformLocation(g_fogProg, "uBlockN");

  g_instProg = link(kInstVert, kInstFrag);
  iRes = glGetUniformLocation(g_instProg, "iRes");
  iCam = glGetUniformLocation(g_instProg, "iCam");
  iTime = glGetUniformLocation(g_instProg, "uTime");

  // Instance layout: pos(2) | radius,rot,sides,flash(4) | aspect,alpha,pattern,outline(4) | color(3) | color2(3)
  glGenVertexArrays(1, &g_instVao);
  glGenBuffers(1, &g_instVbo);
  glBindVertexArray(g_instVao);
  glBindBuffer(GL_ARRAY_BUFFER, g_instVbo);
  const GLsizei stride = sizeof(Inst);
  glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
  glEnableVertexAttribArray(1); glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
  glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
  glEnableVertexAttribArray(3); glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(10 * sizeof(float)));
  glEnableVertexAttribArray(4); glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, (void*)(13 * sizeof(float)));
  for (int i = 0; i < 5; ++i) glVertexAttribDivisor(i, 1);
  glBindVertexArray(0);

  // The icon atlas: 128 px cells stacked top to bottom, uploaded as a texture array (one layer per icon). Until it has
  // loaded (or if it can't be), the hand-drawn glyphs are used.
  {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    glUseProgram(g_instProg);
    glUniform1i(glGetUniformLocation(g_instProg, "uIcons"), 0);
    EM_ASM({
      var img = new Image();
      img.onload = function() {
        var gl = GLctx;
        var A = gl.TEXTURE_2D_ARRAY;
        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(A, GL.textures[$0]);
        gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, false);
        gl.pixelStorei(gl.UNPACK_COLORSPACE_CONVERSION_WEBGL, gl.NONE);
        gl.texImage3D(A, 0, gl.RGBA8, img.width, img.width, Math.round(img.height / img.width), 0, gl.RGBA, gl.UNSIGNED_BYTE, img);
        gl.generateMipmap(A);
        gl.texParameteri(A, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
        gl.texParameteri(A, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
        gl.texParameteri(A, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
        gl.texParameteri(A, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
        Module._tonk_icons_ready();
      };
      img.src = 'icons/atlas.png';
    }, tex);
  }
  EM_ASM({ if (window.tonkReady) window.tonkReady(); });  // page sends the equipped look + bot looks first
  setupMenuMatch();

  emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, onKey);
  emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, onKey);
  emscripten_set_mousemove_callback("#canvas", nullptr, true, onMouseMove);
  emscripten_set_mousedown_callback("#canvas", nullptr, true, onMouseButton);
  emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, onMouseButton);
  emscripten_set_main_loop(frame, 0, true);
  return 0;
}
