// Tonk.io page UI: lobby, in-game HUD and kill feed, death screen, Shop / Locker / Path screens, tooltips, dev panel.
// The game (client/main.cpp) calls the window.tonk* functions; the page calls back through Module._tonk_*.
'use strict';
window.Module = { canvas: document.getElementById('canvas') };

(() => {
  const $ = (id) => document.getElementById(id);
  const el = (tag, cls, html) => { const e = document.createElement(tag); if (cls) e.className = cls; if (html != null) e.innerHTML = html; return e; };
  const call = (n, t, a) => Module.ccall('tonk_' + n, null, t, a);
  const hexInt = (h) => parseInt(h.slice(1), 16);
  const mmss = (s) => Math.floor(s / 60) + ':' + String(Math.floor(s % 60)).padStart(2, '0');
  const num = (n) => Math.round(n).toLocaleString('en-US');
  const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
  const I = Meta.ITEMS;
  const DPR = () => Math.min(2, window.devicePixelRatio || 1);

  // ---------------- Icons ----------------
  const svg = (b, vb = '-14 -14 28 28') => `<svg viewBox="${vb}">${b}</svg>`;
  const ngon = (n, r, rot = 0) => Array.from({ length: n }, (_, k) => { const a = rot + k * Math.PI * 2 / n; return (Math.sin(a) * r).toFixed(2) + ',' + (Math.cos(a) * r).toFixed(2); }).join(' ');
  const INK = '#2b2140';
  // A triangle the way the game draws one: vertex at angle 90deg + rot (rot 0 points down, -90deg points right).
  const tri = (cx, cy, r, rot) => [0, 1, 2].map((k) => { const a = Math.PI / 2 + rot + k * 2 * Math.PI / 3; return (cx + Math.cos(a) * r).toFixed(2) + ',' + (cy + Math.sin(a) * r).toFixed(2); }).join(' ');
  const mixHex = (hex, to, k) => { const n = hexInt(hex), m = (s) => Math.round(((n >> s) & 255) + (to - ((n >> s) & 255)) * k); return `rgb(${m(16)},${m(8)},${m(0)})`; };
  // Icon glyphs: generated from the same spec as the game's own drawing (drawGlyph in client/main.cpp), so the HUD
  // and the map always show the same picture. 16x16 design box; c = glyph color, b = background (for cut-outs).
  const GLYPH = {
    1: {
      1: (c, b) => `<line x1="-6" y1="-5" x2="-1" y2="0" stroke="${c}" stroke-width="3.4" stroke-linecap="round"/><line x1="-1" y1="0" x2="-6" y2="5" stroke="${c}" stroke-width="3.4" stroke-linecap="round"/><line x1="0" y1="-5" x2="5" y2="0" stroke="${c}" stroke-width="3.4" stroke-linecap="round"/><line x1="5" y1="0" x2="0" y2="5" stroke="${c}" stroke-width="3.4" stroke-linecap="round"/>`,
      2: (c, b) => `<rect x="-5.2" y="-3.6" width="10.4" height="7.2" fill="${c}" transform="translate(0 -2.2)"/><polygon points="0,7.6 -5.196,-1.4 5.196,-1.4" fill="${c}"/><rect x="-3" y="-1.9" width="6" height="3.8" fill="${b}" transform="translate(0 -2)"/><polygon points="0,4.5 -3.031,-0.75 3.031,-0.75" fill="${b}"/>`,
      3: (c, b) => `<rect x="-5.2" y="-2.8" width="12" height="5.6" rx="2.8" fill="${c}"/><polygon points="${tri(-4.3, -3, 2.7, Math.PI)}" fill="${c}"/><polygon points="${tri(-4.3, 3, 2.7, 0)}" fill="${c}"/><circle cx="2.6" cy="0" r="1.25" fill="${b}"/><rect x="-2.15" y="-2.8" width="1.1" height="5.6" fill="${b}"/>`,
      4: (c, b) => `<circle cx="-6.2" cy="6" r="1.1" fill="${c}"/><circle cx="-6.4" cy="2.4" r="1.2" fill="${c}"/><circle cx="-5" cy="-0.9" r="1.3" fill="${c}"/><circle cx="-2.4" cy="-3.2" r="1.4" fill="${c}"/><rect x="-4.2" y="-2.2" width="8.4" height="4.4" rx="2.2" fill="${c}" transform="translate(3.2 -4.4) rotate(-11.4592)"/><rect x="-2.6" y="-1" width="5.2" height="2" fill="${c}" transform="translate(0.6 -3.9) rotate(78.5408)"/><circle cx="4.3" cy="-4.6" r="0.9" fill="${b}"/>`,
      5: (c, b) => `<rect x="-4.8" y="-1.8" width="9.6" height="3.6" rx="1.8" fill="${c}" transform="translate(-1.5 -4.6)"/><rect x="-4.8" y="-1.8" width="9.6" height="3.6" rx="1.8" fill="${c}" transform="translate(1.5 0)"/><rect x="-4.8" y="-1.8" width="9.6" height="3.6" rx="1.8" fill="${c}" transform="translate(-1.5 4.6)"/>`,
      6: (c, b) => `<rect x="-7.2" y="-1.9" width="14.4" height="3.8" rx="1.9" fill="${c}" transform="translate(0 0) rotate(90)"/><rect x="-7.4" y="-1.6" width="14.8" height="3.2" fill="${c}" transform="translate(0 -0.8)"/><rect x="-3.4" y="-1.1" width="6.8" height="2.2" fill="${c}" transform="translate(0 5.4)"/>`,
      7: (c, b) => `<line x1="0" y1="6" x2="-5.5" y2="-3.6" stroke="${c}" stroke-width="2.4" stroke-linecap="round"/><line x1="0" y1="6" x2="0" y2="-5.2" stroke="${c}" stroke-width="2.4" stroke-linecap="round"/><line x1="0" y1="6" x2="5.5" y2="-3.6" stroke="${c}" stroke-width="2.4" stroke-linecap="round"/><circle cx="-5.5" cy="-3.6" r="2.2" fill="${c}"/><circle cx="0" cy="-5.2" r="2.2" fill="${c}"/><circle cx="5.5" cy="-3.6" r="2.2" fill="${c}"/>`,
      8: (c, b) => `<line x1="2.8" y1="-7" x2="-2.4" y2="-0.6" stroke="${c}" stroke-width="3.6" stroke-linecap="round"/><line x1="-2.4" y1="-0.6" x2="2.4" y2="0.6" stroke="${c}" stroke-width="3.6" stroke-linecap="round"/><line x1="2.4" y1="0.6" x2="-2.8" y2="7" stroke="${c}" stroke-width="3.6" stroke-linecap="round"/>`,
      9: (c, b) => `<polygon points="8.2,0 0.7,4.33 0.7,-4.33" fill="${c}"/><line x1="-7" y1="-3.6" x2="-2.6" y2="-3.6" stroke="${c}" stroke-width="2.2" stroke-linecap="round"/><line x1="-8" y1="0" x2="-2" y2="0" stroke="${c}" stroke-width="2.2" stroke-linecap="round"/><line x1="-7" y1="3.6" x2="-2.6" y2="3.6" stroke="${c}" stroke-width="2.2" stroke-linecap="round"/>`,
      10: (c, b) => `<rect x="-6.2" y="-2.6" width="12.4" height="5.2" fill="${c}" transform="translate(0 4)"/><rect x="-1.5" y="-3.6" width="3" height="7.2" fill="${c}" transform="translate(0 -4.2)"/><circle cx="0" cy="1" r="4" fill="${c}"/><circle cx="0" cy="1" r="1.6" fill="${b}"/>`,
      11: (c, b) => `<circle cx="-3.6" cy="3.6" r="3.3" fill="none" stroke="${c}" stroke-width="2"/><circle cx="3.6" cy="-3.6" r="3.3" fill="${c}"/><circle cx="0" cy="0" r="0.9" fill="${c}"/>`,
      12: (c, b) => `<circle cx="0" cy="-1.6" r="5.6" fill="${c}"/><rect x="-5.6" y="-3.8" width="11.2" height="7.6" fill="${c}" transform="translate(0 2.4)"/><circle cx="-2.8" cy="6.4" r="1.4" fill="${b}"/><circle cx="2.8" cy="6.4" r="1.4" fill="${b}"/><circle cx="-2.1" cy="-1.8" r="1.3" fill="${b}"/><circle cx="2.1" cy="-1.8" r="1.3" fill="${b}"/>`,
      13: (c, b) => `<circle cx="0" cy="0" r="6.2" fill="none" stroke="${c}" stroke-width="1.8"/><circle cx="0" cy="0" r="2.8" fill="${c}"/><circle cx="5.12" cy="-3.5" r="1.6" fill="${c}"/><circle cx="0.44" cy="6.18" r="1.6" fill="${c}"/><circle cx="-5.56" cy="-2.74" r="1.6" fill="${c}"/>`,
      14: (c, b) => `<rect x="-7.2" y="-3.9" width="14.4" height="7.8" rx="3.9" fill="${c}" transform="translate(0 0)"/><circle cx="0" cy="0" r="2.7" fill="${b}"/><circle cx="0" cy="0" r="1.3" fill="${c}"/>`,
      16: (c, b) => `<circle cx="0" cy="0.6" r="6.4" fill="${c}"/><circle cx="-2.1" cy="-0.9" r="1.1" fill="${b}"/><circle cx="2.1" cy="-0.9" r="1.1" fill="${b}"/><circle cx="0" cy="1.9" r="1.1" fill="${b}"/>`,
      17: (c, b) => `<ellipse cx="1.2" cy="-1.6" rx="7.2" ry="2.6" transform="rotate(-45 1.2 -1.6)" fill="${c}"/><line x1="-5.5" y1="5.5" x2="-1.2" y2="1.2" stroke="${c}" stroke-width="1.8" stroke-linecap="round"/><line x1="-1.2" y1="1.2" x2="3.6" y2="-3.6" stroke="${b}" stroke-width="0.9" stroke-linecap="round"/><circle cx="-4.2" cy="2" r="1.8" fill="none" stroke="${c}" stroke-width="1.2"/>`,
      15: (c, b) => `<rect x="-5.6" y="-2.6" width="11.2" height="8.4" fill="${c}"/><rect x="-7.6" y="-0.6" width="2" height="4.4" fill="${c}"/><rect x="5.6" y="-0.6" width="2" height="4.4" fill="${c}"/><line x1="0" y1="-2.6" x2="0" y2="-5.4" stroke="${c}" stroke-width="1.6" stroke-linecap="round"/><circle cx="0" cy="-6.3" r="1.5" fill="${c}"/><circle cx="-2.3" cy="0.8" r="1.35" fill="${b}"/><circle cx="2.3" cy="0.8" r="1.35" fill="${b}"/><rect x="-2.6" y="3.4" width="5.2" height="1.2" fill="${b}"/>`,
    },
    2: {
      1: (c, b) => `<line x1="-5" y1="5" x2="0.8" y2="-0.8" stroke="${c}" stroke-width="3.2" stroke-linecap="round"/><circle cx="3" cy="-3" r="3.1" fill="none" stroke="${c}" stroke-width="2.6"/><rect x="-1.6" y="-1.6" width="3.2" height="3.2" fill="${b}" transform="translate(5 -5) rotate(45)"/>`,
      2: (c, b) => `<line x1="-4.6" y1="-4.6" x2="4.6" y2="4.6" stroke="${c}" stroke-width="2" stroke-linecap="round"/><line x1="-4.6" y1="4.6" x2="4.6" y2="-4.6" stroke="${c}" stroke-width="2" stroke-linecap="round"/><circle cx="-4.6" cy="-4.6" r="2.4" fill="none" stroke="${c}" stroke-width="1.6"/><circle cx="4.6" cy="-4.6" r="2.4" fill="none" stroke="${c}" stroke-width="1.6"/><circle cx="-4.6" cy="4.6" r="2.4" fill="none" stroke="${c}" stroke-width="1.6"/><circle cx="4.6" cy="4.6" r="2.4" fill="none" stroke="${c}" stroke-width="1.6"/><circle cx="0" cy="0" r="2.6" fill="${c}"/>`,
      3: (c, b) => `<rect x="-6.2" y="-5.2" width="12.4" height="10.4" fill="${c}" transform="translate(0 1.2)"/><rect x="-1.1" y="-5.2" width="2.2" height="10.4" fill="${b}" transform="translate(0 1.2)"/><rect x="-6.2" y="-0.6" width="12.4" height="1.2" fill="${b}" transform="translate(0 -1.6)"/>`,
      4: (c, b) => `<circle cx="0" cy="0" r="6.4" fill="none" stroke="${c}" stroke-width="1.8"/><circle cx="0" cy="0" r="3.3" fill="none" stroke="${c}" stroke-width="1.6"/><line x1="0" y1="0" x2="4.5" y2="-4.5" stroke="${c}" stroke-width="1.8" stroke-linecap="round"/><circle cx="-2.4" cy="2.6" r="1.3" fill="${c}"/>`,
    },
    3: {
      0: (c, b) => `<rect x="-7.25" y="-2.2" width="1.9" height="4.4" fill="${c}"/><rect x="-4.55" y="-3.2" width="1.9" height="6.4" fill="${c}"/><rect x="-1.85" y="-4.2" width="1.9" height="8.4" fill="${c}"/><polygon points="${tri(3.3, 0, 4.7, -Math.PI / 2)}" fill="${c}"/>`,
      1: (c, b) => `<circle cx="2.8" cy="0" r="3.7" fill="${c}"/><line x1="-7.4" y1="-3.2" x2="-3.2" y2="-3.2" stroke="${c}" stroke-width="1.8" stroke-linecap="round"/><line x1="-8" y1="0" x2="-2.2" y2="0" stroke="${c}" stroke-width="1.8" stroke-linecap="round"/><line x1="-7.4" y1="3.2" x2="-3.2" y2="3.2" stroke="${c}" stroke-width="1.8" stroke-linecap="round"/>`,
      2: (c, b) => `<circle cx="0" cy="0" r="5" fill="none" stroke="${c}" stroke-width="2.4"/><circle cx="0" cy="0" r="1.4" fill="${c}"/><rect x="-1" y="-2" width="2" height="4" fill="${c}" transform="translate(0 -7.4)"/><rect x="-1" y="-2" width="2" height="4" fill="${c}" transform="translate(0 7.4)"/><rect x="-2" y="-1" width="4" height="2" fill="${c}" transform="translate(-7.4 0)"/><rect x="-2" y="-1" width="4" height="2" fill="${c}" transform="translate(7.4 0)"/>`,
      3: (c, b) => `<circle cx="-2.7" cy="-1.3" r="3.1" fill="${c}"/><circle cx="2.7" cy="-1.3" r="3.1" fill="${c}"/><polygon points="0,6.6 -5.5,1.1 0,-4.4 5.5,1.1" fill="${c}"/>`,
      4: (c, b) => `<circle cx="-1" cy="-4.3" r="2.5" fill="${c}"/><circle cx="4.2" cy="0" r="2.5" fill="${c}"/><circle cx="-1" cy="4.3" r="2.5" fill="${c}"/><line x1="-7.2" y1="-4.3" x2="-4.4" y2="-4.3" stroke="${c}" stroke-width="1.5" stroke-linecap="round"/><line x1="-2.2" y1="0" x2="0.8" y2="0" stroke="${c}" stroke-width="1.5" stroke-linecap="round"/><line x1="-7.2" y1="4.3" x2="-4.4" y2="4.3" stroke="${c}" stroke-width="1.5" stroke-linecap="round"/>`,
      5: (c, b) => `<polygon points="0,7.4 -7.4,0 0,-7.4 7.4,0" fill="${c}"/><polygon points="-5.233,5.233 -5.233,-5.233 5.233,-5.233 5.233,5.233" fill="${c}"/>`,
      6: (c, b) => `<rect x="-6.6" y="-2.4" width="13.2" height="7.2" rx="3.6" fill="${c}"/><circle cx="0" cy="1.2" r="2.7" fill="${b}"/><circle cx="0" cy="1.2" r="1.4" fill="${c}"/><line x1="0" y1="-6.6" x2="0" y2="-4.4" stroke="${c}" stroke-width="1.4" stroke-linecap="round"/><line x1="-5" y1="-5.4" x2="-3.8" y2="-3.9" stroke="${c}" stroke-width="1.4" stroke-linecap="round"/><line x1="5" y1="-5.4" x2="3.8" y2="-3.9" stroke="${c}" stroke-width="1.4" stroke-linecap="round"/>`,
    },
  };
  const glyph = (set, k, c, bg, scale) => `<g transform="scale(${scale})">${GLYPH[set][k](c, bg)}</g>`;
  // Superpowers: the red rounded square the game draws on the ground (dark-red rim and icon). Mythics: dark, with a
  // rainbow rim. Same order as Power in core/powerups.h; the colors are used for effects and notes.
  // name, color, description (for tooltips), mythic
  const PU = [
    ['None', '#a39cb4', ''],
    ['Speed Boost', '#ffa833', 'Doubles your speed for 8 seconds.'],
    ['Shield', '#59c7fa', 'A shield that blocks everything for 1.8 seconds.'],
    ['Rocket', '#ff5a5f', 'A rocket with a huge range that explodes on walls and tanks. A hit takes 60% of their health.'],
    ['Homing Rocket', '#ff4fd8', 'A rocket that follows your cursor for up to 5 seconds, then explodes. A hit takes 65% of their health.'],
    ['Rapid Shot', '#ff6b82', '2.4 times the fire rate (and faster bullets) for 5.5 seconds.'],
    ['Air Strike', '#5b7cff', 'A plane flies over your cursor and drops 4 bombs scattered around it. They go off almost at once.'],
    ['Spread Shot', '#2ec4b6', 'Every shot fires 5 bullets in a fan for 3 seconds.'],
    ['Lightning', '#ffd23f', 'Lightning bolts burst out in every direction. Each hit deals big damage and stuns for 1 second.'],
    ['Dash', '#8fe66e', 'A quick burst of speed in the direction you are moving.'],
    ['Turret', '#8c8fa3', 'Place a turret that shoots the nearest enemy for 8 seconds. One at a time.'],
    ['Teleport', '#a45cff', 'Jump to your cursor instantly.'],
    ['Invisibility', '#7a86b8', 'Vanish for 4 seconds. Shooting gives away where you are.'],
    ['Singularity', '#7c54f2', 'Mythic. For 9 seconds, pulls enemies toward it and slows them. Touching it stops them shooting and using superpowers.', true],
    ['Instinct', '#ffb31f', 'Mythic. 3 dodges: after you get hit, press E within the window to dodge it.', true],
    ['Coconut Barrage', '#c68b4e', 'Admin exclusive. Ten coconuts rain down in a fan toward your cursor, each a small blast.', false, 'admin'],
    ['Ivy Snare', '#3fae5a', 'Admin exclusive. Vines burst up around your cursor: everyone caught is rooted for 2.5 seconds, then slowed.', false, 'admin'],
    ['Sentry Robot', '#9aa3b5', 'Mythic. Deploys a grey copy of you with your upgrades and 25% less health. It starts as a Newbie and gets better the longer it survives, and self-destructs when beaten or after 60 seconds.', true],
  ];
  // (Sentry Robot is power 15; the admin exclusives 16 and 17 are listed before it above, so move them into place.)
  PU.splice(16, 0, ...PU.splice(15, 2));
  const POW_RED = '#f9c5c1', POW_DARK = '#e6322c';  // light red, red (same as the map and the icon art)
  // Superpowers with icon art (web/icons, built by tools/make_icons.py); the rest are drawn from glyphs.
  const POW_ART = new Set([1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12]);
  const puIcon = (k) => POW_ART.has(k) ? `<img class="ico" src="icons/pow-${k}.png" alt="">` : PU[k][4] === 'admin'
    ? svg(`<rect x="-13" y="-13" width="26" height="26" rx="7.5" fill="#ffb31f"/><rect x="-10.8" y="-10.8" width="21.6" height="21.6" rx="6" fill="${PU[k][1]}"/>${glyph(1, k, '#fff8e8', PU[k][1], 0.64)}`)
    : PU[k][3]
    ? svg(`<defs><linearGradient id="mg${k}" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#ff8fc7"/><stop offset=".5" stop-color="#8fd3ff"/><stop offset="1" stop-color="#ffd23f"/></linearGradient></defs><rect x="-13" y="-13" width="26" height="26" rx="7.5" fill="url(#mg${k})"/><rect x="-10.6" y="-10.6" width="21.2" height="21.2" rx="5.8" fill="#29194d"/>${glyph(1, k, '#fff', '#29194d', 0.64)}`)
    : svg(`<rect x="-13" y="-13" width="26" height="26" rx="7.5" fill="${POW_DARK}"/><rect x="-11.2" y="-11.2" width="22.4" height="22.4" rx="6.3" fill="${POW_RED}"/>${glyph(1, k, POW_DARK, POW_RED, 0.62)}`);
  // Relics: hexagons. name, color, what it does, what R/T does.
  const RELICS = [
    ['None', '#a39cb4', '', ''],
    ['Repair', '#5cd166', 'Heals 1 HP per second after 5 seconds without taking damage.',
      'Hold: heal 15 HP every 3 seconds (plus the 1 HP/s), but you move 50% slower and take 50% more damage.'],
    ['Sentry Drone', '#ff8a3d', 'A drone circles you and shoots nearby enemies. It can\'t be hurt while it circles you.',
      'Tap: turn the drone on or off. Hold: overload it: it fires faster and chases enemies, but can be shot down (4 hits or a rocket). It rebuilds after 15 s.'],
    ['Loot Drone', '#ffc93c', 'A drone fetches upgrades you can use and drops them next to you. One hit destroys it (and this relic) while it\'s out.',
      'Hold for 2 seconds: send the drone to your cursor to explode. This uses up the relic.'],
    ['Radar', '#3fa9f5', 'Shows enemies and walls on your minimap. Without it you only see the storm and yourself.', 'No key action.'],
  ];
  const hexPts = (rr) => ngon(6, rr, 0);
  const relicIcon = (k) => k
    ? svg(`<polygon points="${hexPts(13)}" fill="${RELICS[k][1]}" stroke="${INK}" stroke-width="2" stroke-linejoin="round"/><polygon points="${hexPts(9.5)}" fill="#fff"/>${glyph(2, k, RELICS[k][1], '#fff', 0.68)}`, '-14 -15 28 30')
    : svg(`<polygon points="${hexPts(12.5)}" fill="rgba(255,255,255,.06)" stroke="rgba(255,255,255,.35)" stroke-width="1.6" stroke-dasharray="3 2.4"/>`, '-14 -15 28 30');
  // Stat upgrades: circles. In the HUD each stat has its own color; on the map pickups are colored by rarity.
  // name, color, what one tier does
  const STATS = [
    ['Move Speed', '#29b5f5', 'Each tier: +6% move speed.', 6, '% move speed'],
    ['Bullet Speed', '#ff8c38', 'Each tier: +8% bullet speed.', 8, '% bullet speed'],
    ['Bullet Range', '#9e66ff', 'Each tier: +8% bullet range.', 8, '% range'],
    ['Max Health', '#4dcc5c', 'Each tier: +12 max health.', 12, ' max health'],
    ['Fire Rate', '#ff5d8f', 'Each tier: +8% fire rate.', 8, '% fire rate'],
    ['Bullet Damage', '#ff4d4d', 'Each tier: +7% bullet damage.', 7, '% damage'],
    ['Field of View', '#2ec4b6', 'Each tier: +8% view (Rocket Rampage only).', 8, '% view'],
  ];
  // Like the pickups on the map: a thin rim in the stat's color, a light fill, the icon in a deeper shade.
  const STAT_ART = new Set([0, 1, 2, 4, 5, 6]);  // stats with icon art (already in their HUD colors)
  const statIcon = (k) => { if (STAT_ART.has(k)) return `<img class="ico" src="icons/stat-${k}.png" alt="">`;
    const c = STATS[k][1], f = mixHex(c, 255, 0.8); return svg(`<circle r="12.8" fill="${c}"/><circle r="11" fill="${f}"/>${glyph(3, k, mixHex(c, 0, 0.22), f, 0.8)}`); };
  // Arena Guardians (ids from 1001): their own look, name and gold name color.
  const GUARD_LOOK = { pat: 10, c1: '#4b2e83', c2: '#5f3da3', bpat: 5, b1: '#f5b82e', b2: '#ffffff', trail: 0, death: 0, muzzle: 0, kill: 0 };
  const GUARD_EQ = { ...Meta.DEFAULTS, tag: 'tag.gold' };
  const MAGE_LOOK = { pat: 7, c1: '#2a1650', c2: '#8a5cff', bpat: 5, b1: '#ffd23f', b2: '#ffffff', trail: 0, death: 0, muzzle: 0, kill: 0 };
  // A Sentry Robot: its owner's look, mostly drained of color (the same as greyLook() in client/main.cpp).
  const greyHex = (h) => { const n = hexInt(h), r = (n >> 16) & 255, g = (n >> 8) & 255, b = n & 255, y = 0.3 * r + 0.59 * g + 0.11 * b;
    return '#' + [r, g, b].map((c) => Math.round(c + (y - c) * 0.85).toString(16).padStart(2, '0')).join(''); };
  const greyLook = (l) => ({ ...l, c1: greyHex(l.c1), c2: greyHex(l.c2), b1: greyHex(l.b1), b2: greyHex(l.b2), pat: l.pat === 6 || l.pat === 13 ? 0 : l.pat,
    trail: 0, death: 0, muzzle: 0, kill: 0 });
  const MAPS = ['Crossroads', 'Ruins', 'Canyon'];
  const LOCK = svg('<rect x="-6" y="-1.5" width="12" height="9" rx="2" fill="#fff"/><path d="M-3.6 -1.5 v-2.5 a3.6 3.6 0 0 1 7.2 0 v2.5" fill="none" stroke="#fff" stroke-width="2.2"/>', '-9 -9 18 18');
  const CHECK = svg('<path d="M-5 0 l3.5 3.5 l6.5 -7" fill="none" stroke="#fff" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"/>', '-8 -8 16 16');
  const FUNNEL = svg('<path d="M-8 -7 h16 l-6 7.5 v6 l-4 2 v-8z" fill="currentColor"/>', '-9 -9 18 18');
  const CROSS = svg('<circle r="5" fill="none" stroke="#ff8a7a" stroke-width="2"/><path d="M0 -8 v4 M0 4 v4 M-8 0 h4 M4 0 h4" stroke="#ff8a7a" stroke-width="2"/>', '-9 -9 18 18');
  const coin = '<i class="ico cr"></i>', gem = '<i class="ico at"></i>';
  const SPK_ON = svg('<path d="M-8 -3 h4 l5 -5 v16 l-5 -5 h-4z" fill="currentColor"/><path d="M4 -4 a5 5 0 0 1 0 8 M6.5 -7 a9 9 0 0 1 0 14" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"/>', '-10 -10 20 20');
  const SPK_OFF = svg('<path d="M-8 -3 h4 l5 -5 v16 l-5 -5 h-4z" fill="currentColor"/><path d="M4 -3.5 l7 7 M11 -3.5 l-7 7" stroke="currentColor" stroke-width="2.2" stroke-linecap="round"/>', '-10 -10 20 20');
  const TROPHY = (c = '#ffc63d') => svg(`<path d="M-7 -9 h14 v5 a7 7 0 0 1 -14 0z" fill="${c}" stroke="${INK}" stroke-width="2" stroke-linejoin="round"/><path d="M-7 -7 h-3 a3.5 3.5 0 0 0 4 5 M7 -7 h3 a3.5 3.5 0 0 1 -4 5" fill="none" stroke="${INK}" stroke-width="2"/><rect x="-1.6" y="2.5" width="3.2" height="3.5" fill="${c}" stroke="${INK}" stroke-width="1.6"/><rect x="-5.5" y="6" width="11" height="3.5" rx="1" fill="${c}" stroke="${INK}" stroke-width="2"/>`, '-12 -12 24 24');
  // Bots (core/bot.h): objectives, skill tiers (the last three are dev-panel test bots), play styles.
  const BOT_GOALS = ['Roam', 'Loot', 'Crate', 'Attack', 'Contest', 'Hunt', 'Retreat', 'Zone', 'Raid', 'Recover', 'Guard', 'Follow'];
  const BOT_TIERS = ['Newbie', 'Rookie', 'Intermediate', 'Skilled', 'Pro', 'Elite', 'Unreal', 'Omniscient', 'Omnipotent'];
  const TIER_COLS = ['#b9b9c6', '#9fd4ff', '#7fe07a', '#ffd25e', '#ff9f43', '#ff6b6b', '#d78bff', '#7ef0ff', '#ffffff'];
  const BOT_STYLES = ['Looter', 'Brawler', 'Tactician', 'Survivor', 'Hunter', 'Raider'];
  const STAT_SHORT = ['Spd', 'Bul', 'Rng', 'HP', 'Rate', 'Dmg', 'FOV'];
  // Preset bots (core/bot_presets.h, dev panel BOTS > Presets): index = BotMind.brain (0 = the normal tiered brain).
  const BOT_PRESETS = [null,
    { name: 'Aegis-Vulture', col: '#ffb347', desc: 'Third-party opportunist. Loots on an inward spiral, avoids even fights when hurt, and swoops on wounded tanks and tanks busy shooting each other.' },
    { name: 'Ghost Current', col: '#7ef0ff', desc: 'Adaptive predator. Fights from the sides, keeps escape routes open, and uses Current Shift (circling the enemy) to break their firing angle.' },
    { name: 'Carrion Clock', col: '#c9a7ff', desc: 'Treats the storm as a clock. Loots early, lives just inside the ring, and only fights when its Fight Value Score says it is worth it.' }];
  const devBotNames = new Map();  // bots made in the dev panel (ids from 5001)
  const NAMES = ['Kodiak', 'Pebble', 'Vex', 'Mango', 'Rook', 'Juno', 'Sprocket', 'Bolt', 'Fenwick', 'Tusk', 'Nova', 'Gizmo', 'Clank', 'Ember',
    'Wren', 'Boomer', 'Pixel', 'Ziggy', 'Moxie', 'Hex', 'Rusty', 'Beans', 'Orbit', 'Maple', 'Dozer', 'Quill', 'Fizz', 'Hank', 'Olive', 'Spud',
    'Marlo', 'Turbo', 'Biscuit', 'Atlas', 'Piper', 'Sarge', 'Lumen', 'Gus', 'Echo', 'Dot'];

  let mode = 'br', inMenu = true, ended = false, dead = false, respawnMode = false, spectating = false, devTouched = false;
  let myName = 'Tank';
  // Tank ids: 1 you, 2.. bots, 1001-1003 the big guardians, 2001.. Sentry Robots, 3001.. temple guardians, 3901.. Arena Mages,
  // 5001.. bots made in the dev panel.
  const ownerOf = (id) => (Module._tonk_owner_of ? Module._tonk_owner_of(id) : 0);
  const nameOf = (id) => id === 1 ? myName : id >= 5001 ? (devBotNames.get(id) || 'Test Bot') : id >= 3901 ? 'Arena Mage' : id >= 3001 ? 'Temple Guardian'
    : id >= 2001 ? (ownerOf(id) ? nameOf(ownerOf(id)) : 'A') + "'s Sentry" : id >= 1001 ? 'Arena Guardian' : NAMES[(id - 2) % NAMES.length];

  // ---------------- Scaling (same formula as g_ui in client/main.cpp) ----------------
  function rescale() {
    const s = Math.max(0.55, Math.min(1.5, Math.min(innerWidth / 1600, innerHeight / 900)));
    document.documentElement.style.setProperty('--s', s.toFixed(3));
  }
  addEventListener('resize', rescale);
  rescale();

  // ---------------- Cosmetics -> renderer ----------------
  const sendOne = (slot, l) => Module._tonk_set_look(slot, l.pat, hexInt(l.c1), hexInt(l.c2), l.bpat, hexInt(l.b1), hexInt(l.b2), l.trail, l.death, l.muzzle, l.kill);
  function sendLook() { if (Module._tonk_set_look) sendOne(-1, Meta.look()); }
  // Bots wear combinations of shop, featured and Path items (never the Legend set or admin exclusives), in four bands of
  // nine looks from basic to rarest. Better bots have played longer, so the game picks rarer bands for them (pickLook in
  // client/main.cpp): Newbies wear basics (pastels and base colors, no effects) if anything.
  const botPool = [];
  function sendBotLooks() {
    if (!Module._tonk_clear_bot_looks) return;
    Module._tonk_clear_bot_looks();
    botPool.length = 0;
    const ok = (i) => !i.id.includes('legend') && i.src !== 'admin' && i.src !== 'default';
    const by = (cat, lo, hi, srcs) => Object.values(I).filter((i) => i.cat === cat && ok(i) && i.rarity >= lo && i.rarity <= hi && (!srcs || srcs.includes(i.src)));
    const r = (a, def) => a.length ? a[Math.floor(Math.random() * a.length)].id : def;
    const basicWraps = Object.values(I).filter((i) => i.cat === 'wrap' && ['Pastel Pack', 'Base Color Palette', 'Starter Kit'].includes(i.coll));
    //            rarity    fx chance  title / tag chance
    const BANDS = [[0, 0, 0.0, 0.25], [0, 1, 0.3, 0.4], [2, 3, 0.6, 0.6], [3, 4, 0.9, 0.85]];
    let slot = 0;
    BANDS.forEach(([lo, hi, fx, deco], b) => {
      for (let n = 0; n < 9; n++) {
        const maybe = (p, cat, def) => Math.random() < p ? r(by(cat, lo, hi), def) : def;
        const eq = b === 0
          ? { wrap: r(basicWraps, 'wrap.teal'), barrel: Math.random() < 0.3 ? r(by('barrel', 0, 0), 'barrel.slate') : 'barrel.slate',
              trail: 'trail.none', muzzle: 'muzzle.none', killfx: 'killfx.none', deathfx: 'deathfx.classic',
              title: Math.random() < deco ? r(Object.values(I).filter((i) => ['title.newbie', 'title.rookie'].includes(i.id)), 'title.none') : 'title.none',
              tag: Math.random() < deco ? r(by('tag', 0, 0), 'tag.white') : 'tag.white' }
          : { wrap: r(by('wrap', lo, hi), 'wrap.teal'), barrel: maybe(0.5 + 0.15 * b, 'barrel', 'barrel.slate'),
              trail: maybe(fx, 'trail', 'trail.none'), muzzle: maybe(fx, 'muzzle', 'muzzle.none'), killfx: maybe(fx, 'killfx', 'killfx.none'),
              deathfx: maybe(fx, 'deathfx', 'deathfx.classic'), title: maybe(deco, 'title', 'title.none'), tag: maybe(deco, 'tag', 'tag.white') };
        botPool.push(eq);
        sendOne(slot++, Meta.lookFrom(eq));
      }
    });
  }
  let gameReady = false;  // the game calls tonkReady once it's running (before that, it can't be called into)
  window.tonkReady = () => { gameReady = true; sendLook(); sendBotLooks(); };

  // ---------------- Background tab ----------------
  // Browsers stop requestAnimationFrame (and slow timers down) in background tabs. A worker's timer keeps running, so it
  // pings the game about 20 times a second; the game only steps the match itself when no frame has been drawn lately.
  try {
    const src = 'setInterval(() => postMessage(0), 50);';
    const ticker = new Worker(URL.createObjectURL(new Blob([src], { type: 'text/javascript' })));
    ticker.onmessage = () => { if (gameReady && Module._tonk_bg_tick) Module._tonk_bg_tick(); };
  } catch (e) { /* no workers: the match pauses in background tabs */ }
  const slotCache = new Map();
  let matchGuardians = 0;
  let matchExtra = {}, curMap = 0, isRampage = false, winnerIdLast = 0;
  window.tonkMatchStart = (map, md) => {
    slotCache.clear(); $('feedrows').innerHTML = ''; matchGuardians = 0; matchExtra = {}; curMap = map || 0; isRampage = md === 1;
    $('board').classList.add('hidden'); $('alive').classList.toggle('hidden', isRampage); $('mmlabel').classList.add('hidden');
    showStatRows();
  };
  // Going into (or out of) a temple: the screen dims, then comes back.
  function fadeFlash() {
    const f = $('fade');
    f.style.transition = 'none'; f.style.opacity = '0.85';
    void f.offsetWidth;
    f.style.transition = 'opacity .55s ease-out .12s'; f.style.opacity = '0';
  }
  window.tonkExtra = (crates, legend, mobs, mages, score) => { matchExtra = { crates, legend, mobs, mages, score }; };
  function who(id) {  // name + equipped cosmetics (and look) of any tank in the match
    if (id === 1) return { name: myName, eq: Meta.p.equipped };
    if (id >= 2001 && id < 3001) {
      const o = ownerOf(id), w = o ? who(o) : { eq: Meta.ENEMY_EQ };
      return { name: nameOf(id), eq: { ...w.eq, tag: 'tag.white', title: 'title.none' }, look: greyLook(w.look || Meta.lookFrom(w.eq)) };
    }
    if (id >= 3901 && id < 5001) return { name: nameOf(id), eq: GUARD_EQ, look: MAGE_LOOK };
    if (id >= 1001 && id < 5001) return { name: nameOf(id), eq: GUARD_EQ, look: GUARD_LOOK };
    let slot = slotCache.get(id);
    if (slot === undefined) { slot = Module._tonk_look_slot ? Module._tonk_look_slot(id) : -1; slotCache.set(id, slot); }
    return { name: nameOf(id), eq: slot >= 0 && botPool[slot] ? botPool[slot] : Meta.ENEMY_EQ };
  }
  // plain: a solid color only (for outlined text, where a gradient fill would fight the outline).
  function nameHtml(name, eq, plain) {
    const tag = I[eq.tag] || I['tag.white'];
    return `<span style="${plain ? 'color:' + Meta.tagBase(tag) : Meta.tagCss(tag)}">${esc(name)}</span>`;
  }
  function miniTank(eq, px = 26, look) {
    const cv = el('canvas'), d = DPR();
    cv.width = cv.height = px * d;
    const x = cv.getContext('2d');
    Meta.drawTank(x, px * d * 0.42, px * d * 0.55, px * d * 0.27, -0.6, look || Meta.lookFrom(eq), 0);
    return cv;
  }

  // ---------------- Animated canvases ----------------
  // Every preview is a Meta.scene. Thumbnails render one still frame; hovered cards and detail panels animate.
  const live = new Set();
  let lastT = performance.now();
  (function loop(now) {
    const dt = Math.min(0.05, (now - lastT) / 1000);
    lastT = now;
    for (const p of live) {
      if (!p.cv.isConnected) { live.delete(p); continue; }
      p.step(dt); p.draw();
    }
    requestAnimationFrame(loop);
  })(lastT);
  const WARM = { trail: 1.4, muzzle: 0.36, killfx: 1.55, deathfx: 1.04, wrap: 0.3, barrel: 0.3, title: 0.3, tag: 0.3, loadout: 0.3 };
  // Returns { cv, step, draw }; `cv` is sized for the element's CSS size (w x h) at device resolution.
  function scenePreview(w, h, item, opts = {}) {
    const cv = el('canvas'), d = DPR();
    cv.width = Math.round(w * d); cv.height = Math.round(h * d);
    const sc = Meta.scene(item, { name: previewName(), ...opts });
    const kind = opts.kind || (item ? item.cat : 'loadout');
    for (let t = 0; t < (WARM[kind] || 0.3); t += 1 / 60) sc.step(1 / 60);
    const p = { cv, step: (dt) => sc.step(dt), draw: () => sc.draw(cv) };
    p.draw();
    return p;
  }
  function currencyPreview(w, h, r) {
    const cv = el('canvas'), d = DPR();
    cv.width = Math.round(w * d); cv.height = Math.round(h * d);
    const x = cv.getContext('2d'), W = cv.width, H = cv.height;
    x.fillStyle = r.cr ? '#fff4d6' : '#dcf7f3'; x.fillRect(0, 0, W, H);
    x.strokeStyle = 'rgba(91,74,46,0.08)'; x.lineWidth = d;
    for (let gx = 0; gx < W; gx += 24 * d) { x.beginPath(); x.moveTo(gx, 0); x.lineTo(gx, H); x.stroke(); }
    for (let gy = 0; gy < H; gy += 24 * d) { x.beginPath(); x.moveTo(0, gy); x.lineTo(W, gy); x.stroke(); }
    const R = Math.min(W, H) * 0.2, n = r.cr ? Math.min(3, 1 + Math.floor(r.cr / 50)) : Math.min(3, r.at);
    const spots = [[0, 0], [-0.9, 0.35], [0.9, 0.3]];
    for (let i = n - 1; i >= 0; i--) Meta.drawCurrency(x, r.cr ? 'cr' : 'at', W / 2 + spots[i][0] * R, H * 0.44 + spots[i][1] * R, R);
    x.font = `700 ${H * 0.17}px Fredoka, sans-serif`; x.textAlign = 'center'; x.textBaseline = 'middle'; x.lineJoin = 'round';
    x.lineWidth = H * 0.05; x.strokeStyle = INK; x.fillStyle = '#fff';
    const label = r.cr ? '+' + r.cr : '+' + r.at;
    x.strokeText(label, W / 2, H * 0.82); x.fillText(label, W / 2, H * 0.82);
    return { cv, step() {}, draw() {} };
  }
  const previewName = () => ($('name').value.trim() || Meta.p.name || 'Tank').slice(0, 15);

  // ---------------- Tooltip ----------------
  const tip = $('tip');
  function tipHtml(info) {
    const ra = info.rarity >= 0 ? `<span class="ra" style="color:${Meta.RCOL[info.rarity]}">${Meta.RARITY[info.rarity]}</span>` : '';
    return `<div class="tn">${esc(info.name)}</div>` + (info.type || ra ? `<div class="tt"><span class="ty">${esc(info.type || '')}</span>${ra}</div>` : '') +
      (info.desc ? `<div class="td">${esc(info.desc)}</div>` : '') + (info.sub ? `<div class="ts">${esc(info.sub)}</div>` : '');
  }
  function attachTip(node, info) {
    node.addEventListener('mouseenter', () => { tip.innerHTML = tipHtml(typeof info === 'function' ? info() : info); tip.classList.remove('hidden'); });
    node.addEventListener('mousemove', (e) => {
      const r = tip.getBoundingClientRect();
      let x = e.clientX + 16, y = e.clientY + 18;
      if (x + r.width > innerWidth - 8) x = e.clientX - r.width - 12;
      if (y + r.height > innerHeight - 8) y = e.clientY - r.height - 12;
      tip.style.left = x + 'px'; tip.style.top = y + 'px';
    });
    node.addEventListener('mouseleave', () => tip.classList.add('hidden'));
  }
  const itemTip = (it, sub) => ({ name: it.name, type: Meta.typeName(it), rarity: it.rarity, desc: it.desc, sub });

  // ---------------- Screens ----------------
  function setMenu(on) {
    inMenu = on;
    $('home').classList.toggle('hidden', !on);
    $('hud').classList.toggle('hidden', on);
    if (on) {
      $('end').classList.add('hidden');
      $('specbar').classList.add('hidden');
      $('vignette').classList.remove('out');
      $('notes').innerHTML = '';
      spectating = false;
      if (endAnim) live.delete(endAnim);
      refreshProfile();
    }
  }

  // ---- The admin account: its name asks for a passcode every time you press Play. Only a hash of the passcode is kept
  // here, but this is a browser game with no server yet: anyone can read this file, so it only keeps out the casual.
  const ADMIN_NAME = 'FitCocoIvy', ADMIN_HASH = 3681330129358877;
  const cyrb53 = (str, seed = 0) => {
    let h1 = 0xdeadbeef ^ seed, h2 = 0x41c6ce57 ^ seed;
    for (let i = 0, ch; i < str.length; i++) { ch = str.charCodeAt(i); h1 = Math.imul(h1 ^ ch, 2654435761); h2 = Math.imul(h2 ^ ch, 1597334677); }
    h1 = Math.imul(h1 ^ (h1 >>> 16), 2246822507); h1 ^= Math.imul(h2 ^ (h2 >>> 13), 3266489909);
    h2 = Math.imul(h2 ^ (h2 >>> 16), 2246822507); h2 ^= Math.imul(h1 ^ (h1 >>> 13), 3266489909);
    return 4294967296 * (2097151 & h2) + (h1 >>> 0);
  };
  let passOk = false;  // the passcode was just entered (for this press of Play)
  function setAdmin(on) {
    Meta.setAdmin(on);
    call('set_admin', ['number'], [on ? 1 : 0]);
    if (!on) $('admin').classList.add('hidden');
    sendLook();
  }
  function askPasscode() {
    $('passbox').classList.remove('hidden');
    $('passerr').textContent = '';
    $('passin').value = '';
    setTimeout(() => $('passin').focus(), 30);
  }
  function submitPasscode() {
    if (cyrb53('tonk:' + $('passin').value, 7) !== ADMIN_HASH) {
      $('passerr').textContent = 'Wrong passcode.';
      $('passbox').querySelector('.pcard').classList.remove('shake'); void $('passbox').offsetWidth;
      $('passbox').querySelector('.pcard').classList.add('shake');
      $('passin').select();
      return;
    }
    $('passbox').classList.add('hidden');
    setAdmin(true);
    const got = Meta.grantAdmin();
    toast(got.length ? 'Welcome, admin! Unlocked ' + got.length + ' exclusive cosmetics (see your Locker).' : 'Welcome back, admin! Press ` for the admin panel.', true);
    passOk = true;
    startGame();
  }
  $('passgo').onclick = submitPasscode;
  $('passin').addEventListener('keydown', (e) => { e.stopPropagation(); if (e.key === 'Enter') submitPasscode(); if (e.key === 'Escape') $('passbox').classList.add('hidden'); });
  $('passskip').onclick = () => { $('passbox').classList.add('hidden'); setAdmin(false); passOk = true; startGame(); };
  $('passcancel').onclick = () => $('passbox').classList.add('hidden');

  function startGame() {
    if (!$('modal').classList.contains('hidden') || !$('passbox').classList.contains('hidden')) return;
    const typed = $('name').value.trim();
    if (typed === ADMIN_NAME && !passOk) { askPasscode(); return; }
    if (typed !== ADMIN_NAME && Meta.admin) setAdmin(false);
    passOk = false;
    if (mode === 'sandbox' && $('sandbox').classList.contains('hidden')) { openSandbox(); return; }
    beginMatch();
  }
  function beginMatch() {
    myName = ($('name').value.trim() || 'Tank').slice(0, 15);
    Meta.p.name = $('name').value.trim().slice(0, 15);
    Meta.save();
    ended = dead = spectating = false;
    respawnMode = mode === 'sandbox' || mode === 'rampage';
    devTouched = false;
    styleMyName();
    sendLook();
    if (mode === 'sandbox') {
      const c = sandCfg;
      respawnMode = !!c.respawn;
      call('play_sandbox', Array(12).fill('number'), [c.rules, c.map, c.size, c.fog, c.r0, c.r1, c.time, c.bots, c.respawn ? 1 : 0,
        c.temples ? 1 : 0, c.loot, c.tier]);
      isRampage = c.rules === 1;
    } else call('play_mode', ['number', 'number'], [mode === 'rampage' ? 1 : 0, 0]);
    setMenu(false);
    note(mode === 'sandbox' ? 'Sandbox: ' + (['Crossroads', 'Ruins', 'Canyon', 'an empty map'][sandCfg.map]) + (sandCfg.rules ? ', Rocket Rampage rules' : '')
      : (mode === 'rampage' ? 'Rocket Rampage on ' : 'Map: ') + MAPS[curMap], 'rgba(43,33,64,.85)');
    if (Meta.admin) note('Admin: press ` for your panel.', '#e0a020');
    $('name').blur();
  }

  // ---- Sandbox setup: rules, map, play-area size, fog, bots, loot, temples, respawn (remembered in this browser) ----
  const SAND_KEY = 'tonk.sandbox.v1';
  let sandCfg = { rules: 0, map: 0, size: 14000, fog: 1, r0: 5000, r1: 1200, time: 120, bots: 15, tier: -1, loot: 100, temples: true, respawn: true };
  try { sandCfg = Object.assign(sandCfg, JSON.parse(localStorage.getItem(SAND_KEY) || '{}')); } catch (e) { /* private mode */ }
  function openSandbox() {
    const c = sandCfg;
    $('sbRules').value = c.rules; $('sbMap').value = c.map; $('sbSize').value = c.size; $('sbFog').value = c.fog;
    $('sbR0').value = c.r0; $('sbR1').value = c.r1; $('sbTime').value = c.time; $('sbBots').value = c.bots; $('sbTier').value = c.tier;
    $('sbLoot').value = c.loot; $('sbTemples').checked = c.temples; $('sbRespawn').checked = c.respawn;
    sbShow();
    $('sandbox').classList.remove('hidden');
  }
  function sbShow() {
    $('sbSizeV').textContent = (+$('sbSize').value).toLocaleString();
    $('sbR0V').textContent = (+$('sbR0').value).toLocaleString();
    $('sbR1V').textContent = (+$('sbR1').value).toLocaleString();
    $('sbTimeV').textContent = $('sbTime').value + ' s';
    const fog = +$('sbFog').value;
    for (const id of ['sbR0row', 'sbR1row', 'sbTimerow']) $(id).classList.toggle('dim', !(fog === 2 || (fog === 3 && id === 'sbR0row')));
    $('sbTemplesRow').classList.toggle('dim', +$('sbRules').value === 1);
  }
  $('sbTier').innerHTML = '<option value="-1">Normal mix</option>' + BOT_TIERS.map((t, i) => `<option value="${i}">All ${t}${i >= 6 ? ' (test)' : ''}</option>`).join('');
  for (const id of ['sbRules', 'sbMap', 'sbSize', 'sbFog', 'sbR0', 'sbR1', 'sbTime']) $(id).oninput = sbShow;
  $('sbStart').onclick = () => {
    sandCfg = { rules: +$('sbRules').value, map: +$('sbMap').value, size: +$('sbSize').value, fog: +$('sbFog').value,
      r0: +$('sbR0').value, r1: +$('sbR1').value, time: +$('sbTime').value, bots: Math.max(0, Math.min(39, +$('sbBots').value || 0)),
      tier: +$('sbTier').value, loot: +$('sbLoot').value, temples: $('sbTemples').checked, respawn: $('sbRespawn').checked };
    try { localStorage.setItem(SAND_KEY, JSON.stringify(sandCfg)); } catch (e) { /* private mode */ }
    $('sandbox').classList.add('hidden');
    mode = 'sandbox'; updateModes();
    beginMatch();
  };
  $('sbCancel').onclick = () => $('sandbox').classList.add('hidden');
  for (const el2 of $('sandbox').querySelectorAll('input')) el2.addEventListener('keydown', (e) => e.stopPropagation());
  function styleMyName() {
    const eq = Meta.p.equipped, title = I[eq.title];
    $('pname').textContent = myName;
    $('pname').style.color = Meta.tagBase(I[eq.tag]);
    $('ptitle2').textContent = title.id === 'title.none' ? '' : title.name;
    $('ptitle2').style.color = title.color;
  }
  $('play').onclick = startGame;
  $('name').addEventListener('keydown', (e) => { if (e.key === 'Enter') startGame(); e.stopPropagation(); });
  const MODE_DESC = { br: '40 tanks, one survivor. No respawns. Earns Credits and Path XP.', sandbox: 'Build your own match: rules, map, size, fog, bots and loot. No rewards.',
    rampage: '20 tanks, 3 minutes, Rockets only (a hit takes half your health). The fog closes in. Highest score wins; respawn after 3 seconds.' };
  for (const b of document.querySelectorAll('#modes .seg[data-mode]')) b.onclick = () => { mode = b.dataset.mode; updateModes(); };
  function updateModes() {
    for (const b of document.querySelectorAll('#modes .seg[data-mode]')) b.classList.toggle('on', b.dataset.mode === mode);
    $('modedesc').textContent = MODE_DESC[mode] || '';
  }
  updateModes();

  function toMenu() { call('to_menu', [], []); setMenu(true); setSpeed(1); setLinkMode(0); }
  $('cont').onclick = toMenu;
  $('spectate').onclick = () => { $('end').classList.add('hidden'); spectating = true; $('specbar').classList.remove('hidden'); };
  $('specPrev').onclick = () => call('spectate', ['number'], [-1]);
  $('specNext').onclick = () => call('spectate', ['number'], [1]);
  $('specLeave').onclick = toMenu;

  addEventListener('keydown', (e) => {
    if (e.code === 'Backquote') {
      if (Meta.admin && !e.shiftKey) $('admin').classList.toggle('hidden');
      else toggleDev();
      return;
    }
    if (e.code === 'KeyM' && !(document.activeElement && document.activeElement.tagName === 'INPUT')) { Sfx.setOn(!Sfx.on); showSound(); return; }
    if (e.key === 'Escape') {
      tip.classList.add('hidden');
      if ($('fmenu')) $('fmenu').remove();
      else if (!$('modal').classList.contains('hidden')) closeModal();
      return;
    }
    // Enter spawns from the lobby only. On the death screen it does nothing, so you can't fall into a new match by accident.
    if (e.key === 'Enter' && inMenu && document.activeElement !== $('name') && $('modal').classList.contains('hidden')) startGame();
  });

  // ---------------- Lobby: profile, news, tiles ----------------
  const NEWS = [
    ['v1.4 · Nov 17, 2026', ['Bots now play like people of different skill: Newbie, Rookie, Intermediate, Skilled, Pro and Elite (the better, the rarer).',
      'Bots have objectives (loot, attack, contest loot, hunt, retreat, raid temples, recover) and an aggression meter, and they dodge with human reaction times.',
      'Rocket Rampage: a much bigger arena, bots spread out, and the Rocket starts recharging the moment you fire it.',
      'Dev panel: make your own bots (including the Unreal, Omniscient and Omnipotent test bots) and a Bot Info Overlay.']],
    ['v1.3 · Nov 17, 2026', ['Arena Temples: much thicker walls, a bigger Mage room, and real lighting: you are the light and the walls cast smooth shadows.',
      'Knocked out in a temple? It throws you out with half the health you went in with. Twice, and the temples close to you.',
      'The small temple guardians are now Temple Guardians, and much weaker. Bots dodge less and no longer get stuck or twitch.',
      'Rocket Rampage: your Rocket is your barrel. It slides out of your tank as it recharges.']],
    ['v1.2 · Nov 16, 2026', ['Arena Temples were redesigned: thick stone spirals, carpets and braziers, and you can only see the corridor you are in. Stand in the Mage\'s portal for 2 seconds to get out.',
      'The three temples are spread out on different sides of the map, and the storm always ends at one of them. The three Arena Guardians in the middle are gone.',
      'Rocket Rampage: 20 tanks spread out, Rockets only (half your health per hit), a closing fog, more walls, and only Move Speed and the new Field of View upgrade.',
      'More walls on every map. Crates are bigger; superpowers are small light-red squares.']],
    ['v1.1 · Nov 16, 2026', ['Arena Temples: three per match. Step into the portal and fight down a dark spiral to the Arena Mage. Beat it and a portal takes you to the edge of the safe zone.',
      'Crates: shoot them open for 3 items. A Legendary Crate lands 2-3 minutes in (watch for the red X on the minimap).',
      'New mode: Rocket Rampage. 3 minutes, Rockets for everyone, highest score wins.', 'New Mythic: Sentry Robot, a grey copy of you that learns as it fights.',
      'Maps: Crossroads, Ruins and Canyon, each with its own walls and temples.', 'Bullets bounce off walls and fly further. Rapid Shot is stronger, Air Strike bombs go off sooner.',
      'New pickup and rocket looks, better explosions and teleports. New achievements.']],
    ['v1.0 · Nov 16, 2026', ['Sound effects for superpowers, explosions, hits, knockouts and pickups. Press M to mute.',
      'Rockets and Homing Rockets blow holes in walls.', 'Achievements: each one gives a title, and some give Credits or Arsenal Tokens.',
      'The Arsenal is coming: take a look at the locked weapon bays.', 'New Rainbow collection and 15 new Featured items. Featured now shows 4 items.',
      'Collections: click one to see every item in it. The Locker hides locked items (you can turn that off) and only the Shop sells things.']],
    ['v0.9 · Nov 15, 2026', ['Bots now have a skill level (Noob, Normal, Intermediate, Pro) and a play style. Smarter bots judge fights, grab loot on the way out and dodge rockets.',
      'Bigger map, with loot spread more thinly.', 'New upgrade: Bullet Damage. Upgrades are circles; hover the upgrade panel to see what each one does.',
      'Rebalanced superpowers: Rockets are slower and weaker, Rapid Shot doubles your fire rate, Lightning stuns, the Singularity silences.',
      'Cooldowns start when a superpower is over (for example when your rocket explodes).', 'Bullets grow and fade as they fly.',
      'The match keeps playing when you switch tabs.', 'Arena Guardians no longer fight each other or die in the storm.']],
    ['v0.8 · Nov 15, 2026', ['Smoother movement: tanks build up speed and skid a little when they stop.',
      'New upgrade: Fire Rate. Upgrades now set a tier; ones below your tier are faded out.',
      '12 new superpowers (Rocket, Air Strike, Teleport, Turret, ...) and 2 Mythics from the Arena Guardians.',
      'Relics: Repair, Sentry Drone, Loot Drone and Radar. Keys R and T.',
      'Three Arena Guardians protect the middle of the map. Loot gets better toward the middle.',
      'Beaten tanks drop their upgrades, superpower and relics.']],
    ['v0.7 · Nov 15, 2026', ['New look: fresh colors, a new font and a new lobby.', 'Shop: Featured items every 3 hours and 2 daily-ish deals with up to 40% off.',
      'New cosmetics: Kill FX, Titles and Name Tag Colors.', 'Item details with live previews, filters and tooltips.',
      'The Legendary Path was rebuilt: 60 levels with exclusive rewards.', 'Kill feed, a new death screen and new pickup designs.', 'Bots are easier.']],
    ['v0.6 · Nov 14, 2026', ['Shop, Locker and the Legendary Path.', 'Credits and Path XP from Battle Royale.', 'Smoother motion, hit flashes and recoil.']],
    ['v0.5', ['Stat upgrades in five rarities.', 'Power-ups recharge: E or Shift to use, F to swap.', 'Walls block tanks and bullets.']],
    ['v0.4', ['Smart bots that loot, fight, retreat and respect the zone.', 'Sandbox mode.']],
    ['v0.3', ['The Closing Ring: six phases, shown on the minimap.']],
  ];
  $('newsbody').innerHTML = NEWS.map(([v, lines]) => `<div class="ne"><b>${v}</b><ul>${lines.map((l) => `<li>${l}</li>`).join('')}</ul></div>`).join('');
  // The wheel scrolls the news list itself, never the page or the game behind it.
  $('newsbody').addEventListener('wheel', (e) => e.stopPropagation(), { passive: true });

  function drawTiles() {
    const t = performance.now() / 1000;
    const prep = (id) => { const cv = $(id).querySelector('canvas'), d = DPR(); if (cv.width !== 86 * d) { cv.width = cv.height = 86 * d; } const x = cv.getContext('2d'); x.setTransform(d, 0, 0, d, 0, 0); x.clearRect(0, 0, 86, 86); return x; };
    {
      const x = prep('openShop');
      Meta.drawCurrency(x, 'cr', 30, 52, 17); Meta.drawCurrency(x, 'cr', 56, 56, 15); Meta.drawCurrency(x, 'cr', 44, 30, 19);
    }
    { const x = prep('openLocker'); Meta.drawTank(x, 38, 48, 21, -0.6 + Math.sin(t) * 0.2, Meta.look(), t); }
    {
      const x = prep('openPath');
      x.save(); x.translate(43, 43);
      x.beginPath(); for (let k = 0; k < 10; k++) { const r = k % 2 ? 17 : 34, a = -Math.PI / 2 + k * Math.PI / 5; x.lineTo(Math.cos(a) * r, Math.sin(a) * r); }
      x.closePath(); x.fillStyle = '#ffc93c'; x.strokeStyle = INK; x.lineWidth = 4; x.lineJoin = 'round'; x.fill(); x.stroke();
      x.font = '700 20px Fredoka, sans-serif'; x.textAlign = 'center'; x.textBaseline = 'middle'; x.fillStyle = INK;
      x.fillText(String(Meta.levelInfo(Meta.p.xp).level), 0, 3);
      x.restore();
    }
    {  // Arsenal: two crossed barrels behind an Arsenal Token
      const x = prep('openArsenal');
      x.save(); x.translate(43, 43);
      for (const a of [-0.8, 0.8]) {
        x.save(); x.rotate(a);
        x.fillStyle = '#a3a7b8'; x.strokeStyle = INK; x.lineWidth = 3.5; x.lineJoin = 'round';
        x.beginPath(); x.roundRect(-9, -34, 18, 60, 5); x.fill(); x.stroke();
        x.fillStyle = '#6f6a86'; x.beginPath(); x.roundRect(-11, -36, 22, 10, 4); x.fill(); x.stroke();
        x.restore();
      }
      x.restore();
      Meta.drawCurrency(x, 'at', 43, 50 + Math.sin(t * 2.5) * 1.5, 17);
    }
    {  // Achievements: a trophy
      const x = prep('openAch');
      x.save(); x.translate(43, 46); x.scale(1.05, 1.05);
      x.strokeStyle = INK; x.lineWidth = 3.5; x.lineJoin = 'round';
      x.beginPath(); x.moveTo(-17, -24); x.lineTo(-24, -24); x.quadraticCurveTo(-28, -6, -12, -4); x.moveTo(17, -24); x.lineTo(24, -24); x.quadraticCurveTo(28, -6, 12, -4); x.stroke();
      x.fillStyle = '#ffc93c';
      x.beginPath(); x.moveTo(-18, -28); x.lineTo(18, -28); x.lineTo(16, -8); x.quadraticCurveTo(0, 8, -16, -8); x.closePath(); x.fill(); x.stroke();
      x.fillRect(-4, 2, 8, 10); x.strokeRect(-4, 2, 8, 10);
      x.beginPath(); x.roundRect(-14, 12, 28, 9, 3); x.fill(); x.stroke();
      x.fillStyle = 'rgba(255,255,255,.6)'; x.beginPath(); x.ellipse(-8, -18, 3, 6, 0.3, 0, 7); x.fill();
      const k = 0.5 + 0.5 * Math.sin(t * 3);
      x.fillStyle = `rgba(255,255,255,${0.4 + 0.6 * k})`;
      x.beginPath(); for (let i = 0; i < 8; i++) { const rr = i % 2 ? 2 : 6, a = i * Math.PI / 4; x.lineTo(20 + Math.cos(a) * rr, -32 + Math.sin(a) * rr); } x.closePath(); x.fill();
      x.restore();
    }
  }
  setInterval(() => { if (inMenu && $('modal').classList.contains('hidden')) drawTiles(); }, 50);

  const left = (ms) => { const s = Math.max(0, Math.floor(ms / 1000)); const h = Math.floor(s / 3600), m = Math.floor(s / 60) % 60;
    return (h ? h + 'h ' : '') + String(m).padStart(h ? 2 : 1, '0') + 'm ' + String(s % 60).padStart(2, '0') + 's'; };
  function refreshProfile() {
    const p = Meta.p, li = Meta.levelInfo(p.xp);
    $('plvl').innerHTML = '<b>' + li.level + '</b>';
    $('pxpfill').style.width = (li.max ? 100 : 100 * li.into / li.need) + '%';
    $('pxptext').textContent = li.max ? 'Complete!' : num(li.into) + ' / ' + num(li.need) + ' XP';
    $('pcredits').textContent = num(p.credits);
    $('ptokens').textContent = num(p.tokens);
    $('pathSub').textContent = 'Level ' + li.level;
    { const list = Meta.achievements(); $('achSub').textContent = list.filter((x) => x.done).length + ' / ' + list.length + ' unlocked'; }
    $('shopBadge').classList.toggle('hidden', p.seenShopSlot === Meta.slotNow());
    $('shopSub').textContent = 'New in ' + left(Meta.rotation().next - Date.now()).replace(/ \d+s$/, '');
    if (!$('name').value && p.name) $('name').value = p.name;
    for (const e of document.querySelectorAll('.mcredits')) e.textContent = num(p.credits);
    for (const e of document.querySelectorAll('.mtokens')) e.textContent = num(p.tokens);
    drawTiles();
  }
  $('profile').onclick = () => openModal('path');
  $('openShop').onclick = () => openModal('shop');
  $('openLocker').onclick = () => openModal('locker');
  $('openPath').onclick = () => openModal('path');
  $('openArsenal').onclick = () => openModal('arsenal');
  $('openAch').onclick = () => openModal('ach');

  function achToast(id) {
    const a = Meta.achOf(id), rw = a && a.reward;
    const extra = rw ? [rw.cr ? coin + ' ' + num(rw.cr) : '', rw.at ? gem + ' ' + rw.at : ''].filter(Boolean).join(' ') : '';
    toast(`${TROPHY()} Achievement: <b>${esc(I[id].name)}</b>` + (extra ? ' · ' + extra : ''), true);
  }
  function toast(html, gold) {
    const t = el('div', 'toast' + (gold ? ' gold' : ''), html);
    $('toasts').appendChild(t);
    setTimeout(() => { t.style.transition = 'opacity .4s'; t.style.opacity = '0'; }, 2600);
    setTimeout(() => t.remove(), 3100);
  }

  // ---------------- Names and titles above tanks (2D overlay) ----------------
  const lc = $('labels'), lx = lc.getContext('2d');
  let labelFs = 20;
  window.tonkLabelsBegin = (w, h, zoom) => {
    const d = DPR();
    if (lc.width !== w * d || lc.height !== h * d) { lc.width = w * d; lc.height = h * d; }
    lx.setTransform(d, 0, 0, d, 0, 0); lx.clearRect(0, 0, w, h);
    labelFs = Math.max(10, 19 * zoom);
  };
  // A Sentry Robot's tag: smaller, grey, with a BOT badge.
  function botLabel(x, y, name) {
    const fs = labelFs * 0.72;
    lx.save();
    lx.font = `600 ${fs}px Fredoka, "Trebuchet MS", sans-serif`;
    lx.textBaseline = 'middle'; lx.lineJoin = 'round';
    const tw = lx.measureText(name).width, pw = fs * 2.1, gap = fs * 0.35, x0 = x - (pw + gap + tw) / 2;
    lx.fillStyle = '#5d6272'; lx.strokeStyle = 'rgba(42,33,64,0.92)'; lx.lineWidth = fs * 0.16;
    lx.beginPath(); lx.roundRect(x0, y - fs * 0.56, pw, fs * 1.08, fs * 0.3); lx.fill(); lx.stroke();
    lx.font = `700 ${fs * 0.66}px Fredoka, "Trebuchet MS", sans-serif`; lx.textAlign = 'center'; lx.fillStyle = '#fff';
    lx.fillText('BOT', x0 + pw / 2, y + fs * 0.03);
    lx.font = `600 ${fs}px Fredoka, "Trebuchet MS", sans-serif`; lx.textAlign = 'left';
    lx.lineWidth = fs * 0.26; lx.strokeText(name, x0 + pw + gap, y);
    lx.fillStyle = '#d5d8e0'; lx.fillText(name, x0 + pw + gap, y);
    lx.restore();
  }
  window.tonkLabel = (id, x, y, owner) => {
    if (owner) { botLabel(x, y, nameOf(id)); return; }
    const w = who(id);
    Meta.drawLabel(lx, x, y, w.name, I[w.eq.tag] || I['tag.white'], I[w.eq.title], labelFs, performance.now() / 1000);
  };
  window.tonkLabelsEnd = () => {};
  // Dev panel: Bot Info Overlay, a small card under each bot: skill and type, objective and target, health, aggression
  // and dodging, loadout, upgrades and affinities.
  window.tonkBotInfo = (id, x, y, who, target, feel, gear0, stats, hpk, likes) => {
    const tier = who & 15, style = (who >> 4) & 15, obj = (who >> 8) & 31, kind = (who >> 13) & 3, brain = (who >> 15) & 3;
    const aggr = feel & 255, dodge = (feel >> 8) & 255, held = gear0 & 31, r0 = (gear0 >> 5) & 15, r1 = (gear0 >> 9) & 15;
    const hp = hpk & 4095, maxHp = (hpk >> 12) & 4095, affL = likes & 1023, affF = (likes >> 10) & 1023, affR = (likes >> 20) & 1023;
    const fs = Math.max(10, labelFs * 0.56), lh = fs * 1.22;
    const tag = kind === 1 ? 'Temple Guardian' : kind === 2 ? 'Sentry Robot' : '';
    const fight = obj === 3 || obj === 4 || obj === 5;
    const lines = [];
    if (brain && BOT_PRESETS[brain]) lines.push([BOT_PRESETS[brain].name + ' · preset', BOT_PRESETS[brain].col]);
    else lines.push([(BOT_TIERS[tier] || '?') + ' ' + (BOT_STYLES[style] || '') + (tag ? ' · ' + tag : kind === 3 ? ' · test' : ''), TIER_COLS[tier] || '#fff']);
    lines.push(['▶ ' + (BOT_GOALS[obj] || '?') + (target && fight ? ' → ' + nameOf(target) : ''), fight ? '#ff8a7a' : obj === 6 ? '#8ec5ff' : '#ffe08a']);
    lines.push(['HP ' + hp + '/' + maxHp + ' · aggr ' + aggr + '% · dodge ' + dodge + '%', '#e9e3f5']);
    const gear = [held ? (PU[held] || PU[0])[0] : '', r0 ? (RELICS[r0] || RELICS[0])[0] : '', r1 ? (RELICS[r1] || RELICS[0])[0] : ''].filter(Boolean);
    lines.push([gear.length ? gear.join(' · ') : 'no superpower or relics', '#cdbfff']);
    const st = [];
    for (let k = 0; k < 7; k++) { const v = stats % 6; stats = Math.floor(stats / 6); if (v) st.push(STAT_SHORT[k] + ' ' + v); }
    lines.push([st.length ? st.join('  ') : 'no upgrades', '#bfe8c6']);
    if ((!kind || kind === 3) && !brain) lines.push(['likes: loot ' + (affL / 100).toFixed(1) + ' · fight ' + (affF / 100).toFixed(1) + (affR ? ' · raid ' + (affR / 100).toFixed(1) : ''), '#b9aed0']);
    lx.save();
    lx.font = `500 ${fs}px Fredoka, "Trebuchet MS", sans-serif`;
    lx.textBaseline = 'top'; lx.textAlign = 'center';
    let w = 0;
    for (const [t] of lines) w = Math.max(w, lx.measureText(t).width);
    const pad = fs * 0.45, h = lines.length * lh + pad * 2 - (lh - fs);
    lx.fillStyle = 'rgba(28,20,44,0.82)';
    lx.beginPath(); lx.roundRect(x - w / 2 - pad, y, w + pad * 2, h, fs * 0.4); lx.fill();
    lines.forEach(([t, c], i) => { lx.fillStyle = c; lx.fillText(t, x, y + pad + i * lh); });
    lx.restore();
  };
  // Blinded by an Omnipotent test bot: the screen goes almost black for a moment.
  window.tonkBlind = (v) => { $('blind').style.opacity = inMenu ? 0 : Math.min(1, v / 40); };

  // ---------------- HUD ----------------
  window.tonkHud = (xp, kills, health, maxHealth, alive) => {
    dead = !alive;
    if (inMenu) return;
    $('respawn').classList.toggle('hidden', !(dead && respawnMode));
    $('bars').classList.toggle('hidden', dead && !respawnMode);
    $('stats').classList.toggle('hidden', dead && !respawnMode);
    const f = Math.max(0, Math.min(1, health / maxHealth));
    const bar = $('health').querySelector('i');
    bar.style.width = f * 100 + '%';
    bar.style.background = f > 0.5 ? '#73db66' : f > 0.25 ? '#ffc740' : '#ff6154';
    $('health').querySelector('span').textContent = Math.max(0, health) + ' / ' + maxHealth;
  };

  // Upgrades: one cell per stat (icon + 5 tiers); each lit tier is shown in the rarity color it came from. No labels:
  // hover a cell for its name and what it does.
  const statNow = [0, 0, 0, 0, 0, 0];
  $('statrows').innerHTML = STATS.map(([, c], k) => `<div class="srow" data-k="${k}" style="--c:${c}">${statIcon(k)}<div class="seg5" data-stat="${k}"><b></b><b></b><b></b><b></b><b></b></div></div>`).join('');
  for (const row of document.querySelectorAll('.srow')) {
    const k = +row.dataset.k;
    attachTip(row, () => {  // just the name, your rarity and tier, and what it gives
      const [n, , , per, unit] = STATS[k], t = statNow[k];
      return { name: n, type: 'Tier ' + t, rarity: t > 0 ? t - 1 : -1, sub: '+' + per * t + unit };
    });
  }
  // Battle Royale shows the six combat stats; Rocket Rampage only Move Speed and Field of View.
  function showStatRows() {
    for (const row of document.querySelectorAll('.srow')) {
      const k = +row.dataset.k;
      row.classList.toggle('hidden', isRampage ? k !== 0 && k !== 6 : k === 6);
    }
    $('relichead').classList.toggle('hidden', isRampage);  // no relics in Rocket Rampage
    $('relics').classList.toggle('hidden', isRampage);
  }
  window.tonkStats = (...tiers) => {
    for (const seg of document.querySelectorAll('.seg5')) {
      const k = +seg.dataset.stat, v = tiers[k] || 0;
      statNow[k] = v;
      [...seg.children].forEach((e, i) => { e.classList.toggle('on', i < v); e.style.setProperty('--c', i < v ? Meta.RCOL[v - 1] : ''); });
    }
  };

  // Superpower bar under the health bar. kind, state (0 ready / 1 active / 2 cooldown), fraction*1000, nearby superpower
  // pickup, seconds*10, nearby relic pickup, Instinct dodge window left*1000, active power.
  let lastPu = -1;
  window.tonkPower = (kind, state, frac, nearKind, secsT, nearRelic, pend, active) => {
    if (inMenu) return;
    const bar = $('sbar');
    bar.classList.toggle('hidden', !kind || dead);
    if (kind !== lastPu) { $('sico').innerHTML = kind ? puIcon(kind) : ''; lastPu = kind; }
    if (kind) {
      const tr = $('strack'), fill = tr.firstElementChild, txt = $('sname');
      tr.className = state === 0 ? 'ready' : state === 1 ? 'active' : '';
      if (state === 0) { fill.style.width = '100%'; txt.textContent = PU[kind][0]; }
      else if (state === 1) {
        fill.style.width = frac / 10 + '%';
        if (active === 14) {
          tr.classList.toggle('dodge', pend > 0);
          txt.textContent = pend > 0 ? 'DODGE! Press E' : 'Instinct · ' + (frac * 3 / 1000).toFixed(1).replace(/\.0$/, '') + ' dodges';
        } else txt.textContent = (secsT / 10).toFixed(1) + 's';
      } else { fill.style.width = frac / 10 + '%'; txt.textContent = (secsT / 10).toFixed(1) + 's'; }
    }
    const sb = $('swapbar');
    if (kind && nearKind) {
      sb.innerHTML = `Press <kbd>F</kbd> to swap ${PU[kind][0]} for ${PU[nearKind][0]}`;
      sb.classList.remove('hidden');
    } else if (nearRelic && relicNow[0]) {
      sb.innerHTML = `Press <kbd>F</kbd> to swap your oldest relic (${RELICS[relicNow[0]][0]}) for ${RELICS[nearRelic][0]}`;
      sb.classList.remove('hidden');
    } else sb.classList.add('hidden');
  };


  // Relic slots: hexagons with the key (R = oldest, T = newer), a status line and a hover tooltip.
  const relicNow = [0, 0];
  const relicKey = ['R', 'T'];
  function relicStatus(k, st, ex) {
    switch (k) {
      case 1: return [st === 2 ? 'Repairing (slow, +50% damage taken)' : st === 1 ? 'Healing 1 HP/s' : 'Heals after 5 s unharmed', 0];
      case 2: return [st === 3 ? 'Rebuilding ' + Math.ceil(ex / 10) + 's' : st === 2 ? 'OVERLOADED' : st === 1 ? 'On' : 'Off', 0];
      case 3: return [st === 2 ? 'Diving at your cursor!' : ex >= 100 ? 'Hold to launch…' : st === 1 ? 'Out on a loot trip (' + ex + ' hits left)' : 'Waiting for loot', ex >= 100 ? ex - 100 : 0];
      case 4: return ['Minimap: enemies + walls', 0];
      default: return ['', 0];
    }
  }
  for (const slot of document.querySelectorAll('.rslot')) {
    const i = +slot.dataset.slot;
    attachTip(slot, () => {
      const k = relicNow[i];
      if (!k) return { name: 'Empty Relic Slot', type: '', rarity: -1 };
      return { name: RELICS[k][0], type: 'Relic · key ' + relicKey[i], rarity: -1, desc: RELICS[k][2], sub: '[' + relicKey[i] + '] ' + RELICS[k][3] };
    });
  }
  window.tonkRelics = (r0, r1, s0, s1, e0, e1) => {
    if (inMenu) return;
    const R = [r0, r1], S = [s0, s1], E = [e0, e1];
    for (const slot of document.querySelectorAll('.rslot')) {
      const i = +slot.dataset.slot, k = R[i];
      const [status, prog] = relicStatus(k, S[i], E[i]);
      const sig = k + '|' + status + '|' + prog;
      if (slot.dataset.sig === sig) continue;
      slot.dataset.sig = sig;
      relicNow[i] = k;
      slot.classList.toggle('empty', !k);
      slot.innerHTML = `<div class="hex">${relicIcon(k)}${k ? `<kbd>${relicKey[i]}</kbd>` : ''}</div><div class="rt"><div class="rn">${k ? RELICS[k][0] : 'Empty'}</div><div class="rs">${k ? esc(status) : 'Find a relic'}</div>${prog ? `<div class="rb2"><i style="width:${prog}%"></i></div>` : ''}</div>`;
    }
  };
  $('statsToggle').onclick = () => {
    const hide = $('statsbody').style.display !== 'none';
    $('statsbody').style.display = hide ? 'none' : 'block';
    $('statsToggle').innerHTML = hide ? '+' : '&minus;';
  };

  function note(text, color) {
    const n = el('div', 'note', text);
    n.style.background = color;
    const box = $('notes');
    box.appendChild(n);
    while (box.children.length > 4) box.firstChild.remove();
    setTimeout(() => { n.style.opacity = '0'; }, 2600);
    setTimeout(() => n.remove(), 3000);
  }
  // A big alert in the middle of the screen (it doesn't block the game; it fades after a few seconds).
  let alertTimer = 0;
  function alertBox(title, text) {
    const b = $('alertbox');
    b.innerHTML = '<b>' + esc(title) + '</b><span>' + esc(text) + '</span>';
    b.classList.remove('hidden', 'gone');
    clearTimeout(alertTimer);
    alertTimer = setTimeout(() => b.classList.add('gone'), 5000);
  }
  // Sound effects from the game (web/sfx.js). M or the speaker button turns them off.
  window.tonkSfx = (kind, a, vol, pan) => { if (!inMenu) Sfx.play(kind, a, vol, pan, { mythic: kind === 5 && !!(PU[a] && PU[a][3]) }); };
  function showSound() { $('sound').innerHTML = Sfx.on ? SPK_ON : SPK_OFF; $('sound').title = Sfx.on ? 'Sound on (M)' : 'Sound off (M)'; }
  $('sound').onclick = () => { Sfx.unlock(); Sfx.setOn(!Sfx.on); showSound(); $('sound').blur(); };
  showSound();

  window.tonkNotify = (kind, a, b) => {
    if (inMenu) return;
    if (kind === 0) note('You eliminated ' + esc(nameOf(a)), 'rgba(255,107,92,.9)');
    else if (kind === 1) note(STATS[a][0] + ' → ' + Meta.RARITY[b - 1] + ' (tier ' + b + ')', Meta.RCOL[b - 1]);
    else if (kind === 2) note((PU[a][3] ? 'Mythic! ' : 'Superpower: ') + PU[a][0] + ' · press E', PU[a][1]);
    else if (kind === 3) note('Relic: ' + RELICS[a][0] + ' · key ' + relicKey[b], RELICS[a][1]);
    else if (kind === 4) note(a === 3 ? 'Your Loot Drone was destroyed! The relic is gone.' : 'Your Sentry Drone was shot down. Rebuilding…', 'rgba(255,107,92,.9)');
    else if (kind === 5) note('Dodged!', '#ffb31f');
    else if (kind === 6) note('Too slow! The hit landed.', 'rgba(255,107,92,.9)');
    else if (kind === 7) note(['', 'The guardian dropped a Legendary upgrade!', 'The guardian dropped a relic!', 'The guardian dropped a MYTHIC superpower!'][a] || '', '#ffb31f');
    else if (kind === 8) note('A Legendary Crate lands in 10 seconds! Look for the red X on the minimap.', '#ffb31f');
    else if (kind === 9) note('The Legendary Crate has landed!', '#ffb31f');
    else if (kind === 10) note(a ? 'You broke open the Legendary Crate!' : 'Someone broke open the Legendary Crate.', a ? '#ffb31f' : 'rgba(80,70,110,.9)');
    else if (kind === 11) { fadeFlash(); note('Arena Temple! Fight your way to the Arena Mage in the middle.', '#8a5cff'); }
    else if (kind === 12) {
      fadeFlash();
      if (a === 1 && b >= 2) alertBox('Knocked out in a temple again!', 'The temple threw you out with half the health you went in with. That was your second time: the temples won’t let you in again this match.');
      else note(['The portal took you to the edge of the safe zone.', 'Knocked out in the temple! It threw you out with half the health you went in with. One more time and the temples close to you.', 'Out of time! The temple threw you out.', 'The temple closed and sent you out.'][a] || '', a === 0 || a === 3 ? '#2ec4b6' : 'rgba(255,107,92,.9)');
    }
    else if (kind === 14) note('The temples won’t let you in: you were knocked out in them twice.', 'rgba(255,107,92,.9)');
    else if (kind === 13) note('You beat the Arena Mage! Stand in the portal in its room for 2 seconds to get out.', '#ffb31f');
  };

  // Arena Temple: the minimap says so, and the banner shows the time left (or that the portal is open).
  window.tonkTemple = (inT, leftT, cleared, style, portal) => {
    if (inMenu) return;
    $('mmlabel').classList.toggle('hidden', !inT);
    if (!inT) return;
    $('outmsg').classList.add('hidden');
    $('vignette').classList.remove('out');
    $('zonemain').textContent = !cleared ? 'Arena Temple · ' + mmss(leftT / 10) + ' left'
      : portal > 0 ? 'Leaving… ' + Math.min(100, portal) + '%' : 'The Mage is beaten! Stand in the portal';
  };
  // Rocket Rampage: time left, your score and rank, and the top five.
  window.tonkRampage = (leftT, score, rank, kills, ...top) => {
    if (inMenu) return;
    $('zonemain').textContent = 'Rocket Rampage · ' + mmss(leftT / 10);
    $('zonesub').textContent = 'Score ' + num(score) + ' · #' + rank + ' · ' + kills + (kills === 1 ? ' kill' : ' kills');
    let html = '';
    for (let i = 0; i < 5; i++) {
      const id = top[i * 2], sc = top[i * 2 + 1];
      if (!id) continue;
      const w = who(id);
      html += `<div class="br${id === 1 ? ' me' : ''}"><b>${i + 1}</b><span>${nameHtml(w.name, w.eq)}</span><i>${num(sc)}</i></div>`;
    }
    if (rank > 5) html += `<div class="br me"><b>${rank}</b><span>${nameHtml(myName, Meta.p.equipped)}</span><i>${num(score)}</i></div>`;
    $('board').innerHTML = '<div class="bh">Top scores</div>' + html;
    $('board').classList.remove('hidden');
  };

  // Kill feed: who eliminated who, with each tank's wrap and barrel (no trails).
  window.tonkKill = (killer, victim, vOwner, kOwner) => {
    if (inMenu) return;
    if ((killer === 1 || kOwner === 1) && victim >= 1001 && victim < 2001) matchGuardians++;
    if ((victim >= 2001 && victim < 3901) || vOwner) return;  // Sentry Robots and Temple Guardians don't make the feed
    const row = el('div', 'kf' + (killer === 1 || victim === 1 ? ' me' : ''));
    if (killer && killer !== victim) {
      const k = who(killer);
      row.appendChild(miniTank(k.eq, 26, k.look));
      row.appendChild(el('span', 'n', nameHtml(k.name, k.eq)));
      row.insertAdjacentHTML('beforeend', CROSS);
    } else {
      row.insertAdjacentHTML('beforeend', svg('<circle r="7" fill="#7b4dff" stroke="#fff" stroke-width="1.5"/><path d="M-4 -1 a4 4 0 0 1 8 0 a2.5 2.5 0 0 1 -5 0" fill="none" stroke="#fff" stroke-width="1.5"/>', '-9 -9 18 18'));
      row.appendChild(el('span', 'n', 'The storm'));
    }
    const v = who(victim);
    row.appendChild(miniTank(v.eq, 26, v.look));
    row.appendChild(el('span', 'n', nameHtml(v.name, v.eq)));
    const box = $('feedrows');
    box.prepend(row);
    while (box.children.length > 6) box.lastChild.remove();
    setTimeout(() => { row.style.opacity = '0'; }, 7000);
    setTimeout(() => row.remove(), 7600);
  };

  let winnerName = '';
  // alive, total, ringPhase, shrinking, secsLeft*10, dmg/s, outside, matchOver, myPlace, iWon, matchSec, ringRadius, winnerId, myKills
  window.tonkMatch = (alive, total, phase, shrinking, leftT, dmg, outside, over, place, won, sec, radius, winnerId, kills) => {
    if (inMenu) return;
    const lft = leftT / 10;
    $('zone').classList.toggle('shrink', !!shrinking);
    let main;
    if (radius > 1e6) main = 'No storm';
    else if (shrinking) main = 'Storm closing! ' + Math.ceil(lft) + 's';
    else if (phase < 0) main = 'Storm in ' + mmss(lft);
    else if (lft <= 0) main = 'Final circle';
    else main = 'Next storm in ' + mmss(lft);
    $('zonemain').textContent = main;
    $('zonesub').textContent = kills + (kills === 1 ? ' kill' : ' kills') + ' · ' + mmss(sec);
    $('aliveN').textContent = alive;
    $('vignette').classList.toggle('out', !!outside);
    $('outmsg').classList.toggle('hidden', !outside);
    winnerName = over && winnerId ? nameOf(winnerId) : '';
    winnerIdLast = over ? winnerId : 0;
    if (spectating && winnerName && !won) $('specname').textContent = winnerName + ' wins!';
  };

  window.tonkBot = (id, obj, target, health, aggr, tier, held, kills, spec, styleBrain) => {
    const style = styleBrain & 15, brain = (styleBrain >> 4) & 3;
    if (spectating && !winnerName) $('specname').textContent = 'Spectating ' + nameOf(id);
    $('botinfo').innerHTML = spec
      ? '<b>' + esc(nameOf(id)) + '</b> — ' + (BOT_GOALS[obj] || '?') + (target && obj >= 3 && obj <= 5 ? ' → ' + esc(nameOf(target)) : '') +
        '<br>HP ' + health + ' · kills ' + kills + ' · ' + (PU[held] || PU[0])[0] +
        '<br>' + (brain && BOT_PRESETS[brain] ? BOT_PRESETS[brain].name + ' · preset'
          : (BOT_TIERS[tier] || '?') + ' ' + (BOT_STYLES[style] || '') + ' · aggression ' + aggr + '%')
      : 'Following you.';
  };

  // ---------------- Death / victory screen ----------------
  let endAnim = null;
  window.tonkResult = (won, killer, score, secs, place, total, kills, upgrades, powerUses) => {
    if (inMenu || ended || (respawnMode && !isRampage)) return;
    ended = true;
    const ramp = isRampage, ex = matchExtra;
    // Who got you: their tank (with name and title), or the storm.
    const cvs = $('killercv'), d = DPR();
    cvs.width = 260 * d; cvs.height = 200 * d;
    if (endAnim) live.delete(endAnim);
    $('endsub').className = won ? 'win' : '';
    if (won) {
      $('endsub').textContent = 'Victory!';
      const sc = Meta.scene(null, { kind: 'loadout', name: myName });
      endAnim = { cv: cvs, step: sc.step, draw: () => sc.draw(cvs) };
      $('endname').innerHTML = nameHtml(myName, Meta.p.equipped, true);
    } else if ((killer && killer !== 1) || (ramp && winnerIdLast)) {
      $('endsub').textContent = ramp ? 'Time\'s up! The winner' : 'Eliminated by';
      const k = who(ramp ? winnerIdLast : killer);
      const sc = Meta.scene(null, { kind: 'loadout', eq: k.eq, name: k.name, look: k.look });
      endAnim = { cv: cvs, step: sc.step, draw: () => sc.draw(cvs) };
      $('endname').innerHTML = nameHtml(k.name, k.eq, true);
    } else {
      $('endsub').textContent = 'Eliminated by';
      endAnim = { cv: cvs, t: 0, step(dt) { this.t += dt; }, draw() { drawStorm(cvs, this.t); } };
      $('endname').textContent = 'The storm';
    }
    endAnim.draw();
    live.add(endAnim);

    $('endstats').innerHTML = (ramp ? [['#' + place, 'of ' + total], [num(ex.score || 0), 'Score'], [kills, kills === 1 ? 'Kill' : 'Kills'], [upgrades, 'Upgrades']]
      : [['#' + place, 'of ' + total], [kills, kills === 1 ? 'Kill' : 'Kills'], [mmss(secs), 'Survived'], [upgrades, 'Upgrades']])
      .map(([v, k]) => `<div class="es"><b>${v}</b><span>${k}</span></div>`).join('');

    const lines = $('xplines'), totalBox = $('xptotal'), pathBox = $('endpath');
    lines.innerHTML = ''; totalBox.innerHTML = ''; pathBox.innerHTML = '';
    if (devTouched || (mode !== 'br' && mode !== 'rampage')) {
      lines.innerHTML = `<div class="empty-note">No rewards this time: rewards are only given in Battle Royale with no dev tools.</div>`;
      pathBox.classList.add('hidden');
    } else {
      pathBox.classList.remove('hidden');
      const res = Meta.applyMatch({ won: !!won, secs, place, kills, upgrades, powerUses, guardians: matchGuardians, mode: ramp ? 'rampage' : 'br',
        score: ex.score || 0, crates: ex.crates || 0, legend: ex.legend || 0, mobs: ex.mobs || 0, mages: ex.mages || 0 });
      res.ach.forEach((id, i) => setTimeout(() => achToast(id), 600 + i * 500));
      res.lines.forEach((l, i) => {
        const row = el('div', 'xl', `<span class="g">${esc(l.t)}</span><span class="d">${esc(l.d)}</span><span class="x">+${num(l.xp)} XP</span><span class="c">${l.cr ? coin + ' ' + num(l.cr) : ''}</span>`);
        lines.appendChild(row);
        setTimeout(() => row.classList.add('in'), 150 + i * 220);
      });
      totalBox.innerHTML = `<span>Total</span><span class="x" id="txp">+0 XP</span><span>${coin} <b id="tcr">0</b></span>`;
      const t0 = performance.now(), dur = 600, delay = 150 + res.lines.length * 220;
      const count = () => {
        const k = Math.max(0, Math.min(1, (performance.now() - t0 - delay) / dur));
        if (!$('txp')) return;
        $('txp').textContent = '+' + num(res.xp * k) + ' XP';
        $('tcr').textContent = num(res.credits * k);
        if (k < 1) requestAnimationFrame(count); else showPath(res);
      };
      requestAnimationFrame(count);
    }
    $('spectate').classList.toggle('hidden', !!won || ramp);
    $('end').classList.remove('hidden');
  };

  function drawStorm(cv, t) {
    const x = cv.getContext('2d'), W = cv.width, H = cv.height;
    x.fillStyle = '#e8e0fb'; x.fillRect(0, 0, W, H);
    x.save(); x.translate(W / 2, H / 2);
    for (let k = 0; k < 4; k++) {
      x.strokeStyle = `rgba(123,77,255,${0.25 + k * 0.15})`; x.lineWidth = W * 0.035;
      x.beginPath(); x.arc(0, 0, H * (0.12 + k * 0.09), t * (1 + k * 0.4) + k, t * (1 + k * 0.4) + k + 4.2); x.stroke();
    }
    x.restore();
  }

  // Path progress on the death screen: only the level you are on and the one you're working toward.
  function nodeCard(L, w = 118) {
    const node = el('div', 'pn node');
    node.style.width = w + 'px';
    const rs = Meta.PATH[L - 1];
    const main = rs.find((r) => r.item) || rs[0];
    const info = L === Meta.MAX_LEVEL ? { name: 'Legend Set', type: 'Wrap + Trail + Title', rarity: 4, desc: 'The Legend wrap, the Legend Trail and the Legend title.' } : Meta.rewardInfo(main);
    const c = cardShell(info.rarity, L === Meta.MAX_LEVEL ? 'Legend Set' : info.name, `Lv ${L}`);
    const p = main.item ? scenePreview(w - 6, (w - 6) * 0.8, I[main.item], L === Meta.MAX_LEVEL ? { kind: 'trail', eq: { wrap: 'wrap.legend', trail: 'trail.legend', title: 'title.legend' } } : {})
      : currencyPreview(w - 6, (w - 6) * 0.8, main);
    c.prepend(p.cv);
    hoverAnim(c, p);
    attachTip(c, info);
    node.appendChild(c);
    return node;
  }
  function showPath(res) {
    const box = $('endpath');
    const b = res.before, a = res.after;
    box.innerHTML = '';
    box.appendChild(nodeCard(a.level));
    const mid = el('div', 'mid');
    mid.innerHTML = `<div class="lv">${a.max ? 'Legendary Path complete!' : 'Level ' + a.level + ' → ' + (a.level + 1)}</div>
      <div class="bar"><i id="epfill" style="width:${a.level > b.level ? 0 : (b.max ? 100 : 100 * b.into / b.need)}%"></i></div>
      <div class="sm">${a.max ? '' : num(a.into) + ' / ' + num(a.need) + ' XP'}</div>`;
    const unlocked = [...res.granted.map((g) => g.item ? I[g.item].name : g.cr ? num(g.cr) + ' Credits' : g.at + ' Arsenal Token' + (g.at > 1 ? 's' : '')),
      ...res.ach.map((id) => I[id].name + ' (title)')];
    if (unlocked.length) mid.appendChild(el('div', 'lvup', (a.level > b.level ? 'Level up! ' : '') + 'Unlocked: ' + esc(unlocked.join(', '))));
    box.appendChild(mid);
    if (!a.max) box.appendChild(nodeCard(a.level + 1));
    requestAnimationFrame(() => requestAnimationFrame(() => { const f = $('epfill'); if (f) f.style.width = (a.max ? 100 : 100 * a.into / a.need) + '%'; }));
  }

  // ---------------- Cards ----------------
  function cardShell(rarity, name, foot) {
    const c = el('div', 'icard');
    c.style.setProperty('--rc', rarity >= 0 ? Meta.RCOL[rarity] : '#ffc63d');
    c.appendChild(el('div', 'rb'));
    c.appendChild(el('div', 'nm', esc(name)));
    c.appendChild(el('div', 'ft', foot));
    return c;
  }
  function hoverAnim(c, p) {
    c.addEventListener('mouseenter', () => live.add(p));
    c.addEventListener('mouseleave', () => live.delete(p));
  }
  function priceHtml(it, price) {
    if (price == null) return '';
    const full = it.src === 'shop' && price < it.price ? ` <s>${num(it.price)}</s>` : '';
    return `<span class="p">${coin}${num(price)}</span>${full}`;
  }
  // ctx: 'shop' | 'locker' ; size: [w, h] CSS px of the preview
  function itemCard(it, ctx, size = [150, 120], extra = {}) {
    const p = Meta.p, owned = p.owned.includes(it.id), equipped = p.equipped[it.cat] === it.id;
    const price = extra.price !== undefined ? extra.price : Meta.offerPrice(it.id);
    let foot;
    if (owned) foot = equipped ? 'Equipped' : 'Owned';
    else if (price != null) foot = priceHtml(it, price);
    else foot = it.src === 'path' ? 'Path Lv ' + Meta.pathLevelOf[it.id] : it.src === 'ach' ? 'Achievement' : 'Featured only';
    if (ctx === 'locker' && !owned) foot = 'Locked';
    const c = cardShell(it.rarity, it.name, foot);
    if (ctx === 'locker' && !owned) c.classList.add('locked');
    if (sel === it.id) c.classList.add('sel');
    const pv = scenePreview(size[0], size[1], it);
    c.prepend(pv.cv);
    if (equipped) c.appendChild(el('div', 'eqd', 'Equipped'));
    else if (ctx === 'locker' && !owned) c.appendChild(el('div', 'lock', LOCK));
    if (extra.pct) c.appendChild(el('div', 'off', '-' + extra.pct + '%'));
    hoverAnim(c, pv);
    attachTip(c, () => itemTip(it, owned ? (equipped ? 'Equipped' : 'Owned') : ctx === 'locker' ? 'Locked' : Meta.howToGet(it)));
    c.onclick = () => { sel = it.id; tip.classList.add('hidden'); renderModal(true); };
    return c;
  }

  // ---------------- Shop / Locker / Path ----------------
  let modalKind = '', shopTab = 'featured', shopCat = 'wrap', lockerCat = 'wrap', sortBy = 'rar-asc', sel = null, confirmId = null, tickId = 0;
  let collView = null;  // name of the collection being shown (opened from an item's details)
  const TITLES = { shop: 'Shop', locker: 'Locker', path: 'Legendary Path', arsenal: 'Arsenal', ach: 'Achievements' };
  function openModal(kind) {
    modalKind = kind;
    sel = null; confirmId = null; collView = null;
    $('modal').classList.remove('hidden');
    $('mpanel').dataset.kind = kind;
    $('mtitle').textContent = TITLES[kind];
    if (kind === 'shop') { Meta.p.seenShopSlot = Meta.slotNow(); Meta.save(); }
    renderModal();
    clearInterval(tickId);
    tickId = setInterval(updateCountdowns, 1000);
  }
  function closeModal() {
    $('modal').classList.add('hidden');
    $('mbody').innerHTML = '';
    tip.classList.add('hidden');
    clearInterval(tickId);
    refreshProfile();
    sendLook();
  }
  $('mclose').onclick = closeModal;
  $('modal').addEventListener('mousedown', (e) => { if (e.target === $('modal')) closeModal(); });

  let shownSlot = -1;
  function updateCountdowns() {
    const rot = Meta.rotation();
    for (const e of document.querySelectorAll('[data-until]')) e.textContent = 'Refreshes in ' + left(+e.dataset.until - Date.now());
    if (modalKind === 'shop' && shownSlot !== -1 && rot.slot !== shownSlot) renderModal(true);  // a new rotation just started
  }

  function topTabs(list, cur, onPick) {
    const box = $('mtabs');
    box.innerHTML = '';
    for (const [id, label] of list) {
      const b = el('button', 'ttab' + (id === cur ? ' on' : ''), label);
      b.onclick = () => onPick(id);
      box.appendChild(b);
    }
  }
  function sideTabs(cats, cur, onPick, count) {
    const s = el('div', 'sidetabs');
    for (const c of cats) {
      const b = el('button', 'stab' + (c.id === cur ? ' on' : ''), `<span>${c.name}</span><small>${count(c.id)}</small>`);
      b.onclick = () => onPick(c.id);
      s.appendChild(b);
    }
    return s;
  }
  const SORTS = [['price-asc', 'Price', '↑'], ['price-desc', 'Price', '↓'], ['rar-asc', 'Rarity', '↑'], ['rar-desc', 'Rarity', '↓']];
  function sortItems(list) {
    const price = (i) => Meta.offerPrice(i.id) ?? (i.src === 'featured' ? Meta.FEATURED_PRICE[i.rarity] : 0);
    const f = {
      'price-asc': (a, b) => price(a) - price(b) || a.rarity - b.rarity, 'price-desc': (a, b) => price(b) - price(a) || b.rarity - a.rarity,
      'rar-asc': (a, b) => a.rarity - b.rarity || price(a) - price(b), 'rar-desc': (a, b) => b.rarity - a.rarity || price(b) - price(a),
    }[sortBy];
    return list.slice().sort((a, b) => f(a, b) || a.name.localeCompare(b.name));
  }
  function filterButton() {
    const w = el('div', 'filter');
    const cur = SORTS.find((s) => s[0] === sortBy);
    const b = el('button', 'btn small ghost', `${FUNNEL}<span>${cur[1]} ${cur[2]}</span>`);
    b.onclick = (e) => {
      e.stopPropagation();
      if ($('fmenu')) { $('fmenu').remove(); return; }
      const m = el('div', 'fmenu'); m.id = 'fmenu';
      for (const [id, k, arrow] of SORTS) {
        const o = el('button', id === sortBy ? 'on' : '', `<span>${k}</span><span>${arrow === '↑' ? 'Low to high ↑' : 'High to low ↓'}</span>`);
        o.onclick = () => { sortBy = id; renderModal(true); };
        m.appendChild(o);
      }
      w.appendChild(m);
    };
    w.appendChild(b);
    return w;
  }
  document.addEventListener('mousedown', (e) => { const m = $('fmenu'); if (m && !m.parentElement.contains(e.target)) m.remove(); });

  function renderModal(keepScroll) {
    const body = $('mbody');
    const sc = body.querySelector('.content'), y = keepScroll && sc ? sc.scrollTop : 0;
    const tr = body.querySelector('.track'), tx = tr ? tr.scrollLeft : null;
    body.innerHTML = '';
    tip.classList.add('hidden');
    refreshProfile();
    if (collView && (modalKind === 'shop' || modalKind === 'locker')) renderCollection(body, modalKind);
    else if (modalKind === 'shop') renderShop(body);
    else if (modalKind === 'locker') renderLocker(body);
    else if (modalKind === 'arsenal') renderArsenal(body);
    else if (modalKind === 'ach') renderAchievements(body);
    else renderPath(body, tx);
    const sc2 = body.querySelector('.content');
    if (sc2) sc2.scrollTop = y;
  }

  function section(title, sub, until) {
    const s = el('div', 'sec');
    s.appendChild(el('h3', '', title));
    if (sub) s.appendChild(el('span', 'sub', sub));
    if (until) { const r = el('span', 'refresh', 'Refreshes in ' + left(until - Date.now())); r.dataset.until = until; s.appendChild(r); }
    return s;
  }

  function renderShop(body) {
    topTabs([['featured', 'Featured'], ['all', 'All']], shopTab, (id) => { shopTab = id; sel = null; confirmId = null; collView = null; renderModal(); });
    const rot = Meta.rotation();
    shownSlot = rot.slot;
    if (shopTab === 'featured') {
      const c = el('div', 'content');
      c.appendChild(section('Featured', 'Exclusive items you can only get here', rot.next));
      const f = el('div', 'feat');
      rot.featured.forEach((it) => f.appendChild(itemCard(it, 'shop', [200, 150], { price: Meta.FEATURED_PRICE[it.rarity] })));
      c.appendChild(f);
      c.appendChild(section('Deals', 'Discounts on shop items'));
      const dl = el('div', 'deals');
      for (const d of rot.deals) {
        const card = itemCard(d.item, 'shop', [190, 142], { price: d.price, pct: d.pct });
        const info = el('div', 'info');
        info.appendChild(card.querySelector('.nm')); info.appendChild(card.querySelector('.ft'));
        info.insertAdjacentHTML('beforeend', `<div style="padding:0 10px 4px;font-size:14px;color:var(--muted)">${esc(Meta.RARITY[d.item.rarity])} ${esc(Meta.typeName(d.item))}</div>`);
        card.appendChild(info);
        dl.appendChild(card);
      }
      c.appendChild(dl);
      c.appendChild(el('div', 'pathnote', 'Credits come from playing Battle Royale. Everything in the shop is cosmetic and never changes how your tank plays.'));
      body.appendChild(c);
    } else {
      const cats = Meta.CATS.filter((c) => c.id !== 'title');
      body.appendChild(sideTabs(cats, shopCat, (id) => { shopCat = id; sel = null; confirmId = null; renderModal(); },
        (id) => Meta.SHOP_ITEMS.filter((i) => i.cat === id).length));
      const c = el('div', 'content');
      const items = sortItems(Meta.SHOP_ITEMS.filter((i) => i.cat === shopCat));
      const tb = el('div', 'toolbar');
      tb.appendChild(el('h3', '', `${Meta.CAT[shopCat].name}<small>${items.length} items</small>`));
      tb.appendChild(filterButton());
      c.appendChild(tb);
      const deals = Object.fromEntries(rot.deals.map((d) => [d.item.id, d]));
      const g = el('div', 'grid');
      items.forEach((it) => g.appendChild(itemCard(it, 'shop', [150, 120], deals[it.id] ? { price: deals[it.id].price, pct: deals[it.id].pct } : {})));
      c.appendChild(g);
      body.appendChild(c);
    }
    body.appendChild(detailPanel('shop'));
  }

  function switchBox(label, on, onChange) {
    const w = el('label', 'switch' + (on ? ' on' : ''), `<i></i><span>${label}</span>`);
    w.onclick = (e) => { e.preventDefault(); onChange(!on); };
    return w;
  }
  const LOCKER_TABS = [...Meta.CATS, { id: 'colls', name: 'Collections' }];
  function renderLocker(body) {
    $('mtabs').innerHTML = '';
    const p = Meta.p;
    const all = (cat) => Object.values(I).filter((i) => i.cat === cat && Meta.isVisible(i));
    const done = Meta.COLLECTIONS.filter((n) => Meta.collItems(n).every((i) => p.owned.includes(i.id))).length;
    body.appendChild(sideTabs(LOCKER_TABS, lockerCat, (id) => { lockerCat = id; sel = null; confirmId = null; renderModal(); },
      (id) => id === 'colls' ? done + '/' + Meta.COLLECTIONS.length : all(id).filter((i) => p.owned.includes(i.id)).length + '/' + all(id).length));
    const c = el('div', 'content');
    if (lockerCat === 'colls') {
      renderCollectionList(c);
      body.appendChild(c);
      body.appendChild(detailPanel('locker'));
      return;
    }
    const every = sortItems(all(lockerCat)).sort((a, b) => p.owned.includes(b.id) - p.owned.includes(a.id));
    const items = p.hideLocked ? every.filter((i) => p.owned.includes(i.id)) : every;
    const tb = el('div', 'toolbar');
    tb.appendChild(el('h3', '', `${Meta.CAT[lockerCat].name}<small>${every.filter((i) => p.owned.includes(i.id)).length} of ${every.length} owned</small>`));
    tb.appendChild(switchBox('Hide locked', p.hideLocked, (v) => { p.hideLocked = v; Meta.save(); renderModal(true); }));
    tb.appendChild(filterButton());
    c.appendChild(tb);
    const g = el('div', 'grid');
    items.forEach((it) => g.appendChild(itemCard(it, 'locker')));
    c.appendChild(g);
    body.appendChild(c);
    body.appendChild(detailPanel('locker'));
  }

  // ---------------- Collections ----------------
  // Owned items first, then by rarity.
  function collSorted(name) {
    const p = Meta.p;
    return Meta.collItems(name).sort((a, b) => p.owned.includes(b.id) - p.owned.includes(a.id) || a.rarity - b.rarity || a.name.localeCompare(b.name));
  }
  function openColl(name) { collView = name; sel = null; confirmId = null; renderModal(); }
  function renderCollectionList(c) {
    const p = Meta.p;
    const tb = el('div', 'toolbar');
    tb.appendChild(el('h3', '', `Collections<small>${Meta.COLLECTIONS.length} sets</small>`));
    c.appendChild(tb);
    const g = el('div', 'colls');
    for (const name of Meta.COLLECTIONS) {
      const items = collSorted(name), have = items.filter((i) => p.owned.includes(i.id)).length;
      const top = Math.max(...items.map((i) => i.rarity));
      const card = el('button', 'ccard' + (have === items.length ? ' full' : ''));
      card.style.setProperty('--rc', Meta.RCOL[top]);
      const row = el('div', 'cprev');
      for (const it of items.slice(0, 3)) {
        const pv = scenePreview(64, 52, it, { noName: true, thumb: true });
        if (!p.owned.includes(it.id)) pv.cv.classList.add('dim');
        row.appendChild(pv.cv);
      }
      card.appendChild(row);
      card.appendChild(el('div', 'cn', esc(name) + (have === items.length ? ' ' + CHECK : '')));
      card.appendChild(el('div', 'cbar', `<div class="bar"><i style="width:${100 * have / items.length}%"></i></div><span>${have} / ${items.length}</span>`));
      card.onclick = () => openColl(name);
      g.appendChild(card);
    }
    c.appendChild(g);
  }
  // Every item in one collection. From the Locker, locked items don't say where they come from.
  function renderCollection(body, ctx) {
    const p = Meta.p, items = collSorted(collView);
    const c = el('div', 'content');
    const tb = el('div', 'toolbar');
    const back = el('button', 'btn small ghost', '&#9664; Back');
    back.onclick = () => { collView = null; sel = null; confirmId = null; renderModal(); };
    tb.appendChild(back);
    tb.appendChild(el('h3', '', `${esc(collView)}<small>${items.filter((i) => p.owned.includes(i.id)).length} of ${items.length} owned · collection</small>`));
    c.appendChild(tb);
    const g = el('div', 'grid');
    items.forEach((it) => g.appendChild(itemCard(it, ctx)));
    c.appendChild(g);
    body.appendChild(c);
    body.appendChild(detailPanel(ctx));
  }

  // The panel on the right: a live preview on the arena grid, rarity, name, type, collection, description and actions.
  function detailPanel(ctx) {
    const d = el('div', ''); d.id = 'detail';
    const it = sel && I[sel];
    if (!it) {
      const pv = scenePreview(302, 226, null, { kind: 'loadout' });
      live.add(pv);
      d.appendChild(pv.cv);
      d.appendChild(el('div', 'empty-note', ctx === 'shop' ? 'Pick an item to see it up close.<br>Your current loadout is shown above.' : 'Pick an item to see it and equip it.<br>This is your current loadout.'));
      const eq = Meta.p.equipped;
      d.appendChild(el('div', 'coll', Meta.CATS.map((c) => `<div><b>${c.one}:</b> ${esc(I[eq[c.id]].name)}</div>`).join('')));
      return d;
    }
    const p = Meta.p, owned = p.owned.includes(it.id), equipped = p.equipped[it.cat] === it.id;
    const price = Meta.offerPrice(it.id);
    const pv = scenePreview(302, 226, it);
    live.add(pv);
    d.appendChild(pv.cv);
    const rar = el('div', 'rar', Meta.RARITY[it.rarity]); rar.style.background = Meta.RCOL[it.rarity];
    d.appendChild(rar);
    d.appendChild(el('h3', '', esc(it.name)));
    d.appendChild(el('div', 'ty', esc(Meta.typeName(it))));
    if (it.coll) {
      const cl = el('div', 'coll', 'Collection: ');
      const lk = el('button', 'clink', esc(it.coll) + ' &#9656;');
      lk.title = 'See every item in this collection';
      lk.onclick = () => openColl(it.coll);
      cl.appendChild(lk);
      d.appendChild(cl);
    }
    d.appendChild(el('div', 'desc', esc(it.desc)));
    if (ctx === 'shop' || owned) d.appendChild(el('div', 'src', owned ? 'In your Locker.' : 'Get it from: ' + esc(Meta.howToGet(it))));
    d.appendChild(el('div', 'grow'));
    const act = el('div', 'act');
    if (owned) {
      const b = el('button', 'btn teal', equipped ? 'Equipped' : 'Equip');
      b.disabled = equipped;
      b.onclick = () => { Meta.equip(it.id); sendLook(); toast('Equipped ' + esc(it.name)); renderModal(true); };
      act.appendChild(b);
    } else if (ctx === 'locker') {
      act.appendChild(el('div', 'note2', 'You don\'t own this yet.'));
    } else if (price != null) {
      if (confirmId === it.id) {
        const cf = el('div', 'confirm', `Buy <b>${esc(it.name)}</b> for ${coin} <b>${num(price)}</b>?`);
        const r = el('div', 'r');
        const yes = el('button', 'btn orange', 'Confirm'), no = el('button', 'btn grey', 'Cancel');
        yes.style.flex = no.style.flex = '1';
        yes.onclick = () => {
          const res = Meta.buy(it.id);
          confirmId = null;
          if (res) {
            toast(`${esc(it.name)} is yours!`, true);
            for (const a of res.ach) achToast(a);
          }
          renderModal(true);
        };
        no.onclick = () => { confirmId = null; renderModal(true); };
        r.appendChild(yes); r.appendChild(no); cf.appendChild(r);
        act.appendChild(cf);
      } else {
        const can = p.credits >= price;
        const b = el('button', 'btn orange', `Buy ${coin} ${num(price)}`);
        b.disabled = !can;
        b.onclick = () => { confirmId = it.id; renderModal(true); };
        act.appendChild(b);
        if (!can) act.appendChild(el('div', 'note2', `You need ${num(price - p.credits)} more Credits.`));
      }
    } else {
      act.appendChild(el('div', 'note2', it.src === 'featured' ? 'Not in the Featured rotation right now. Check back later!'
        : it.src === 'path' ? 'Unlocks automatically at Legendary Path level ' + Meta.pathLevelOf[it.id] + '.'
        : it.src === 'ach' ? 'Unlocks when you complete the achievement.' : ''));
    }
    d.appendChild(act);
    return d;
  }

  function renderPath(body, keepX) {
    $('mtabs').innerHTML = '';
    const p = Meta.p, li = Meta.levelInfo(p.xp);
    const wrap = el('div', ''); wrap.id = 'pathwrap';
    const head = el('div', ''); head.id = 'pathhead';
    head.innerHTML = `<div class="lv">${li.level}</div><div class="info"><div class="t">Level ${li.level}<small>${li.max ? 'Path complete!' : num(li.into) + ' / ' + num(li.need) + ' XP to level ' + (li.level + 1)}</small></div>
      <div class="bar"><i style="width:${li.max ? 100 : 100 * li.into / li.need}%"></i></div><div class="hint">Scroll or drag to see all 60 levels</div></div>`;
    wrap.appendChild(head);
    const track = el('div', 'track'), row = el('div', 'trow');
    const NODE = 150, STEP = NODE + 16;  // node width + gap
    const line = el('div', 'line'); line.style.width = (Meta.MAX_LEVEL - 1) * STEP + 'px';
    const fill = el('i'); fill.style.width = Math.min((Meta.MAX_LEVEL - 1) * STEP, ((li.level - 1) + (li.max ? 0 : li.into / li.need)) * STEP) + 'px';
    line.appendChild(fill); row.appendChild(line);
    Meta.PATH.forEach((rs, i) => {
      const L = i + 1;
      const n = nodeCard(L, NODE);
      n.classList.remove('node');
      n.classList.toggle('done', L <= li.level);
      n.classList.toggle('cur', L === li.level + 1);
      if (rs.some((r) => r.item && I[r.item].rarity >= 3)) n.classList.add('big');
      n.prepend(el('div', 'lvn', L));
      if (L <= li.level) n.appendChild(el('div', 'chk', CHECK));
      row.appendChild(n);
    });
    track.appendChild(row);
    wrap.appendChild(track);
    // Sideways scrolling with the mouse wheel (no scroll bar), plus click-and-drag.
    track.addEventListener('wheel', (e) => { track.scrollLeft += (Math.abs(e.deltaX) > Math.abs(e.deltaY) ? e.deltaX : e.deltaY); e.preventDefault(); }, { passive: false });
    let drag = null;
    track.addEventListener('mousedown', (e) => { drag = { x: e.clientX, s: track.scrollLeft, moved: false }; });
    addEventListener('mousemove', (e) => {
      if (!drag) return;
      const dx = e.clientX - drag.x;
      if (Math.abs(dx) > 3) { drag.moved = true; track.classList.add('drag'); }
      track.scrollLeft = drag.s - dx / parseFloat(getComputedStyle(document.documentElement).getPropertyValue('--s') || 1);
    });
    addEventListener('mouseup', () => { drag = null; track.classList.remove('drag'); });

    const foot = el('div', ''); foot.id = 'pathfoot';
    const s = p.stats;
    foot.appendChild(section('Career', ''));
    foot.appendChild(el('div', 'career', [['Matches', s.matches], ['Wins', s.wins], ['Kills', s.kills], ['Best place', s.bestPlace ? '#' + s.bestPlace : '-'],
      ['Most kills', s.bestKills], ['Items owned', p.owned.length + ' / ' + Object.keys(I).length]].map(([k, v]) => `<div><b>${v}</b><span>${k}</span></div>`).join('')));
    foot.appendChild(el('div', 'pathnote', 'Earn Path XP in every Battle Royale match: survive, get kills, collect upgrades, use power-ups and place well. Rewards unlock automatically and go to your Locker. Everything on the Path is exclusive to the Path. Arsenal Tokens will unlock weapons in the Arsenal.'));
    wrap.appendChild(foot);
    body.appendChild(wrap);
    requestAnimationFrame(() => { track.scrollLeft = keepX != null ? keepX : Math.max(0, (li.level - 3) * STEP); });
  }

  // ---------------- Arsenal (placeholder until weapons arrive) ----------------
  // Six locked weapon bays with silhouettes. Each silhouette is a tank with a different barrel.
  const BAYS = [
    (x) => { x.roundRect(-7, -46, 14, 40, 4); },                                                   // long barrel
    (x) => { x.roundRect(-15, -36, 11, 30, 4); x.roundRect(4, -36, 11, 30, 4); },                   // twin
    (x) => { x.moveTo(-8, -6); x.lineTo(-20, -34); x.lineTo(20, -34); x.lineTo(8, -6); x.closePath(); },  // wide
    (x) => { for (const dx of [-11, 0, 11]) x.roundRect(dx - 4, -38, 8, 32, 3); },                 // triple
    (x) => { x.roundRect(-15, -40, 30, 26, 7); },                                                  // short and fat
    (x) => { x.roundRect(-5, -52, 10, 48, 3); x.roundRect(-10, -26, 20, 8, 3); },                  // very long
  ];
  function bayCanvas(k, t) {
    const cv = el('canvas'), d = DPR();
    cv.width = cv.height = 150 * d;
    const x = cv.getContext('2d');
    x.setTransform(d * 1.35, 0, 0, d * 1.35, 0, 0);
    x.translate(55.5, 66);
    x.shadowColor = 'rgba(79,216,200,.85)'; x.shadowBlur = 10;
    x.fillStyle = '#3a2f58'; x.strokeStyle = '#4fd8c8'; x.lineWidth = 2.5; x.lineJoin = 'round';
    x.beginPath(); BAYS[k](x); x.fill(); x.stroke();
    x.beginPath(); x.arc(0, 0, 26, 0, 7); x.fill(); x.stroke();
    x.shadowBlur = 0;
    x.fillStyle = 'rgba(79,216,200,.9)'; x.font = '700 22px Fredoka, sans-serif'; x.textAlign = 'center'; x.textBaseline = 'middle';
    x.fillText('?', 0, 1);
    return cv;
  }
  function renderArsenal(body) {
    $('mtabs').innerHTML = '';
    const w = el('div', ''); w.id = 'arsenal';
    const hero = el('div', 'ahero');
    hero.innerHTML = `<div class="stripes"></div><div class="soon">Coming soon</div>
      <h3>The Arsenal</h3>
      <p>New weapons for your tank are being built. Each one will change how you fight: how fast you fire, how big your bullets are, how far they fly.
      Spend Arsenal Tokens here to unlock them.</p>
      <div class="atok">${gem}<b>${num(Meta.p.tokens)}</b><span>Arsenal Token${Meta.p.tokens === 1 ? '' : 's'} saved up</span></div>`;
    w.appendChild(hero);
    const rack = el('div', 'rack');
    BAYS.forEach((_, k) => {
      const bay = el('div', 'bay');
      bay.style.animationDelay = (k * 0.35) + 's';
      bay.appendChild(bayCanvas(k));
      bay.appendChild(el('div', 'bn', 'Weapon bay ' + (k + 1)));
      bay.appendChild(el('div', 'bl', LOCK + '<span>Locked</span>'));
      rack.appendChild(bay);
    });
    w.appendChild(rack);
    w.appendChild(el('div', 'pathnote', 'Arsenal Tokens come from the Legendary Path and from achievements. Keep them: they carry over when weapons arrive.'));
    body.appendChild(w);
  }

  // ---------------- Achievements ----------------
  // Each achievement is named after the title it gives you. Unfinished ones come first, closest to done at the top.
  function titleHtml(t) {
    const cls = t.anim === 'gold' ? 'tgold' : t.anim === 'rainbow' ? 'trainbow' : '';
    return `<span class="atl ${cls}" style="${cls ? '' : 'color:' + t.color}">${esc(t.name)}</span>`;
  }
  function renderAchievements(body) {
    $('mtabs').innerHTML = '';
    const p = Meta.p, list = Meta.achievements();
    const doneN = list.filter((x) => x.done).length;
    const w = el('div', 'content'); w.id = 'achs';
    const head = el('div', 'ahead');
    head.innerHTML = `<div class="tro">${TROPHY()}</div><div class="info"><div class="t">${doneN} of ${list.length} unlocked</div>
      <div class="bar"><i style="width:${100 * doneN / list.length}%"></i></div>
      <div class="hint">Every achievement gives you a title to wear above your name. Some give Credits or Arsenal Tokens too.</div></div>`;
    w.appendChild(head);
    const fmt = (x, v) => x.a.fmt === 'time' ? Math.floor(v / 60) + ':' + String(Math.floor(v % 60)).padStart(2, '0') : num(v);
    list.sort((a, b) => a.done - b.done || (a.done ? 0 : b.value / b.goal - a.value / a.goal));
    const g = el('div', 'alist');
    for (const x of list) {
      const t = I[x.a.id], rw = x.a.reward;
      const row = el('div', 'arow' + (x.done ? ' done' : ''));
      row.appendChild(el('div', 'aic', x.done ? CHECK : TROPHY('#d9d2c3')));
      const main = el('div', 'amain');
      main.appendChild(el('div', 'aname', titleHtml(t) + '<small>Title</small>'));
      main.appendChild(el('div', 'adesc', esc(x.a.text)));
      if (x.done) {
        const eq = p.equipped.title === t.id;
        const b = el('button', 'btn small ' + (eq ? 'grey' : 'teal'), eq ? 'Wearing' : 'Wear title');
        b.disabled = eq;
        b.onclick = () => { Meta.equip(t.id); sendLook(); toast('Now wearing ' + esc(t.name)); renderModal(true); };
        const got = el('div', 'agot', CHECK + '<span>Unlocked</span>');
        got.appendChild(b);
        main.appendChild(got);
      } else {
        main.appendChild(el('div', 'aprog', `<div class="bar"><i style="width:${100 * x.value / x.goal}%"></i></div><span>${fmt(x, x.value)} / ${fmt(x, x.goal)}</span>`));
      }
      row.appendChild(main);
      const rew = el('div', 'arew');
      if (rw && rw.cr) rew.appendChild(el('span', 'chip', coin + '<b>' + num(rw.cr) + '</b>'));
      if (rw && rw.at) rew.appendChild(el('span', 'chip', gem + '<b>' + rw.at + '</b>'));
      row.appendChild(rew);
      attachTip(row, itemTip(t, x.done ? 'Unlocked' : x.a.text));
      g.appendChild(row);
    }
    w.appendChild(g);
    body.appendChild(w);
  }

  // ---------------- Dev panel ----------------
  function applySettings() {
    respawnMode = $('respawnChk').checked; ended = dead = spectating = false; devTouched = true; mode = 'dev';
    $('end').classList.add('hidden');
    myName = ($('name').value.trim() || 'Tank').slice(0, 15); styleMyName();
    sendLook();
    call('restart', ['number', 'number', 'number'], [+$('nBots').value, respawnMode ? 1 : 0, 0]);
    call('set_god', ['number'], [$('god').checked ? 1 : 0]);
    call('set_debug_bots', ['number'], [$('dbg').checked ? 1 : 0]);
    setMenu(false);
  }
  const toggleDev = () => { if ($('dev').classList.toggle('hidden')) setLinkMode(0); };  // (hidden: the mouse shoots again)
  $('devlink').onclick = toggleDev;
  $('restart').onclick = applySettings;
  $('respawnChk').onchange = () => { devTouched = true; respawnMode = $('respawnChk').checked; call('set_respawn', ['number'], [respawnMode ? 1 : 0]); };
  // Speed: pause, slow motion or fast-forward. Pausing and slow motion slow the effects, animations and sounds too.
  let curSpeed = 1;
  function setSpeed(s) {
    curSpeed = s;
    if ($('lnPause')) $('lnPause').classList.toggle('on', s === 0);
    call('set_timescale', ['number'], [s]);
    Sfx.setRate(s);
    for (const b of $('speeds').children) b.classList.toggle('on', +b.dataset.s === s);
  }
  $('speeds').onclick = (e) => {
    const s = e.target.dataset && e.target.dataset.s; if (s === undefined) return;
    if (+s !== 1) devTouched = true;
    setSpeed(+s);
  };
  $('ghost').onchange = () => { devTouched = true; call('set_ghost', ['number'], [$('ghost').checked ? 1 : 0]); };
  $('tgSet').onclick = () => {
    devTouched = true;
    const n = Module.ccall('tonk_debug_set_target', 'number', ['number', 'number'], [+$('tgWho').value, +$('tgWhat').value]);
    toast(n ? (+$('tgWhat').value ? 'Red lines drawn for ' + n + ' bot' + (n > 1 ? 's' : '') : 'Red lines removed for ' + n + ' bot' + (n > 1 ? 's' : '')) : 'No bots to change (watch a bot first, or make some)');
  };
  // ---- Dev panel pages: MAIN and [BOTS] ----
  function devPage(pg) {
    for (const b of $('devtabs').children) b.classList.toggle('on', b.dataset.pg === pg);
    for (const d of document.querySelectorAll('#dev .devpg')) d.classList.toggle('hidden', d.dataset.pg !== pg);
    if (pg !== 'bots') setLinkMode(0);
  }
  $('devtabs').onclick = (e) => { const pg = e.target.dataset && e.target.dataset.pg; if (pg) devPage(pg); };
  // ---- Lines: click a bot, then click (or drag to) other tanks. Red: it goes after them. Blue: they're a team. ----
  let linkMode = 0, linkSel = 0;
  const LINK_HINT = [
    'Pick a line color, then click a bot in the arena.',
    'Red: click a bot, then click (or drag to) every tank it should go after. Click a line\u2019s end again to remove it. Esc or click the ground to let go.',
    'Blue: click a tank, then click (or drag to) its teammates. Teamed tanks never attack or hurt each other. Click again to remove. Esc or click the ground to let go.',
  ];
  function lnHint() {
    const h = $('lnHint');
    h.className = 'lnhint' + (linkMode === 1 ? ' red' : linkMode === 2 ? ' blue' : '');
    h.textContent = linkMode && linkSel ? 'Picked ' + nameOf(linkSel) + '. Now click (or drag to) ' +
      (linkMode === 1 ? 'the tanks it should go after.' : 'its teammates.') + ' Esc or click the ground to let go.' : LINK_HINT[linkMode];
  }
  function setLinkMode(m) {
    linkMode = m;
    for (const b of $('lnModes').querySelectorAll('[data-m]')) b.classList.toggle('on', +b.dataset.m === m);
    const cv = $('canvas'); if (cv) cv.classList.toggle('linking', m > 0);
    try { call('set_link_mode', ['number'], [m]); } catch (e) {}
    linkSel = 0; lnHint();
  }
  $('lnModes').onclick = (e) => {
    const m = e.target.dataset && e.target.dataset.m; if (m === undefined) return;
    if (inMenu && +m) { toast('Start a match first'); return; }
    devTouched = true;
    setLinkMode(+m);
  };
  $('lnPause').onclick = () => { devTouched = true; setSpeed(curSpeed === 0 ? 1 : 0); };
  $('lnShow').onchange = () => call('show_links', ['number'], [$('lnShow').checked ? 1 : 0]);
  $('lnClrSel').onclick = () => { if (!linkSel) { toast('Click a tank with the line tool first'); return; } call('clear_links', ['number', 'number'], [0, 1]); };
  $('lnClrRed').onclick = () => call('clear_links', ['number', 'number'], [1, 0]);
  $('lnClrBlue').onclick = () => call('clear_links', ['number', 'number'], [2, 0]);
  $('lnClrAll').onclick = () => call('clear_links', ['number', 'number'], [0, 0]);
  const LINK_MSG = { 1: 'Red line: ', 2: 'Removed red line: ', 3: 'Teamed up: ', 4: 'No longer a team: ' };
  window.tonkLinks = (red, blue, sel, code) => {
    linkSel = sel;
    $('lnCount').textContent = red || blue ? [red ? red + ' red' : '', blue ? blue + ' blue' : ''].filter(Boolean).join(' \u00b7 ') : 'No lines';
    lnHint();
    if (code === 5) toast('Red lines start at a bot (they tell it who to go after)');
    else if (code === 6) toast('Arena Guardians can\u2019t take lines');
  };
  $('sbOpen').onclick = () => openSandbox();
  $('skipRing').onclick = () => { devTouched = true; call('skip_ring', [], []); };
  $('add60').onclick = () => { devTouched = true; call('add_time', ['number'], [60]); };
  $('toMid').onclick = () => { devTouched = true; call('debug_goto', ['number', 'number'], [7000, 8150]); };
  $('legendNow').onclick = () => { devTouched = true; call('debug_legend', [], []); };
  $('toTemple').onclick = () => { devTouched = true; call('debug_temple', [], []); };
  $('toCrate').onclick = () => { devTouched = true; call('debug_crate', [], []); };
  $('edge').onclick = () => { devTouched = true; call('teleport_edge', [], []); };
  $('god').onchange = () => { devTouched = true; call('set_god', ['number'], [$('god').checked ? 1 : 0]); };
  $('dbg').onchange = () => call('set_debug_bots', ['number'], [$('dbg').checked ? 1 : 0]);
  $('botInfoChk').onchange = () => call('set_bot_info', ['number'], [$('botInfoChk').checked ? 1 : 0]);
  // ---- Make a bot: skill, type, aggression, aim, dodging, power, upgrades, superpower, relics ----
  $('bmTier').innerHTML = BOT_TIERS.map((t, i) => `<option value="${i}"${i === 2 ? ' selected' : ''}>${t}${i >= 6 ? ' (test)' : ''}</option>`).join('');
  $('bmStyle').innerHTML = BOT_STYLES.map((t, i) => `<option value="${i}">${t}</option>`).join('');
  $('bmPow').innerHTML = PU.map((p, i) => p[4] === 'admin' ? '' : `<option value="${i}">${i ? p[0] : 'None'}</option>`).join('');
  $('bmRel0').innerHTML = $('bmRel1').innerHTML = RELICS.map((p, i) => `<option value="${i}">${i ? p[0] : 'None'}</option>`).join('');
  $('bmStats').innerHTML = STAT_SHORT.map((n, k) => `<label>${n}<input type="number" min="0" max="5" value="0" data-k="${k}"></label>`).join('');
  const bmShow = () => {
    $('bmAggrV').textContent = $('bmAggr').value + '%';
    const auto = $('bmAuto').checked;
    $('bmAim').disabled = $('bmDodge').disabled = auto;
    $('bmAimV').textContent = auto ? '—' : $('bmAim').value;
    $('bmDodgeV').textContent = auto ? '—' : $('bmDodge').value + '%';
    $('bmPowerV').textContent = '×' + (+$('bmPower').value / 100).toFixed(1);
  };
  for (const k of ['bmAggr', 'bmAuto', 'bmAim', 'bmDodge', 'bmPower']) $(k).oninput = bmShow;
  $('bmAuto').onchange = bmShow;
  $('bmTier').onchange = () => { if (+$('bmTier').value === 8 && +$('bmPower').value === 100) $('bmPower').value = 160; bmShow(); };
  bmShow();
  $('bmMax').onclick = () => { for (const i of $('bmStats').querySelectorAll('input')) i.value = 5; };
  $('bmSpawn').onclick = () => {
    if (inMenu) { toast('Start a match first'); return; }
    devTouched = true;
    let stats = 0;
    const inputs = [...$('bmStats').querySelectorAll('input')];
    for (let k = inputs.length - 1; k >= 0; k--) stats = stats * 6 + Math.max(0, Math.min(5, +inputs[k].value || 0));
    const auto = $('bmAuto').checked, tier = +$('bmTier').value, style = +$('bmStyle').value;
    const id = Module.ccall('tonk_debug_spawn_bot', 'number', Array(10).fill('number'),
      [tier, style, +$('bmAggr').value, auto ? -1 : +$('bmAim').value, auto ? -1 : +$('bmDodge').value, +$('bmPower').value,
       +$('bmPow').value, +$('bmRel0').value, +$('bmRel1').value, stats]);
    if (id) { devBotNames.set(id, BOT_TIERS[tier] + ' ' + BOT_STYLES[style] + ' ' + (id - 5000)); toast('Made ' + nameOf(id)); }
  };
  $('bmClear').onclick = () => { call('debug_clear_bots', [], []); toast('Removed your bots'); };

  // ---- Presets: the hand-made bots (core/bot_presets.h) ----
  $('presetList').innerHTML = BOT_PRESETS.map((p, i) => p ? `<div class="preset" style="--pc:${p.col}"><div class="pi"><b>${esc(p.name)}</b><div class="pd">${esc(p.desc)}</div></div><button data-preset="${i}">Spawn</button></div>` : '').join('');
  const spawnPreset = (k) => {
    const id = Module.ccall('tonk_debug_spawn_preset', 'number', ['number', 'number'], [k, $('presetSpread').checked ? 1 : 0]);
    if (id) devBotNames.set(id, BOT_PRESETS[k].name);
    return id;
  };
  $('presetList').onclick = (e) => {
    const b = e.target.closest('button[data-preset]'); if (!b) return;
    if (inMenu) { toast('Start a match first'); return; }
    devTouched = true;
    const id = spawnPreset(+b.dataset.preset);
    toast(id ? 'Spawned ' + nameOf(id) : 'Presets only play Battle Royale rules');
  };
  $('presetAll').onclick = () => {
    if (inMenu) { toast('Start a match first'); return; }
    devTouched = true;
    let n = 0;
    for (let k = 1; k < BOT_PRESETS.length; k++) if (spawnPreset(k)) n++;
    toast(n ? 'Spawned all three presets' : 'Presets only play Battle Royale rules');
  };
  $('presetClear').onclick = () => { call('debug_clear_bots', [], []); toast('Removed your bots'); };

  // ---- Admin panel (the admin account only): fun, bizarre commands ----
  const ADMIN_CMDS = [
    ['You', [[0, 'Giant'], [1, 'Tiny'], [2, 'Normal size'], [20, 'Max me'], [14, 'Overdrive', 1]]],
    ['Weapons', [[3, 'Bullet Storm', 1], [4, 'Bouncy Bullets', 1], [18, 'Coconut Barrage'], [19, 'Ivy Snare']]],
    ['Chaos', [[5, 'Meteor Shower'], [6, 'Black Hole'], [7, 'Shockwave'], [15, 'Nuke'], [9, 'Loot Rain'], [13, 'Disco', 1]]],
    ['Bots', [[8, 'Freeze everyone'], [12, 'Come here!'], [11, 'Swap with a bot'], [10, 'Minion Army'], [16, "Everyone's a Newbie"], [17, 'Total chaos']]],
  ];
  const ADMIN_TIPS = { 0: 'Twice as big', 1: 'Half as big', 5: 'Meteors rain down around you for 6 s', 6: 'A huge Singularity under your cursor',
    7: 'Flings everyone near you away', 15: 'A huge blast around you (it can\'t hurt you)', 9: 'Epic and Legendary upgrades, superpowers and relics',
    13: 'Everyone else goes rainbow', 8: 'Nobody else can move for 5 s', 12: 'Every bot appears in a ring around you',
    10: 'Four Sentry Robots on your side for 60 s', 16: 'Every bot becomes a Newbie', 17: 'Every bot becomes Omnipotent. Good luck.',
    3: 'Every shot fires 12 bullets all around', 4: 'Bullets bounce 25 times', 14: 'Double damage, fire rate, speed and range',
    18: 'Your exclusive superpower: coconuts rain toward your cursor', 19: 'Your exclusive superpower: vines root everyone at your cursor', 20: 'Every stat Legendary, full health' };
  $('adminbody').innerHTML = ADMIN_CMDS.map(([h, cmds]) => `<h4>${h}</h4><div class="r">` +
    cmds.map(([c, n, t]) => `<button data-cmd="${c}"${t ? ' data-toggle="1"' : ''} title="${esc(ADMIN_TIPS[c] || '')}">${esc(n)}</button>`).join('') + '</div>').join('');
  $('adminbody').onclick = (e) => {
    const b = e.target.closest('button[data-cmd]'); if (!b) return;
    const r = Module.ccall('tonk_admin', 'number', ['number'], [+b.dataset.cmd]);
    b.blur();
    if (!r) { toast(inMenu ? 'Start a match first' : 'Nothing happened'); return; }
    if (b.dataset.toggle) b.classList.toggle('on', r === 1);
    else toast(b.textContent + '!');
  };
  $('adminclose').onclick = () => $('admin').classList.add('hidden');
  $('next').onclick = () => call('spectate', ['number'], [1]);
  $('prev').onclick = () => call('spectate', ['number'], [-1]);
  $('me').onclick = () => call('spectate', ['number'], [0]);
  $('addCr').onclick = () => { Meta.p.credits += 1000; Meta.save(); refreshProfile(); if (!$('modal').classList.contains('hidden')) renderModal(true); toast('+1,000 Credits'); };
  $('addXp').onclick = () => {
    Meta.p.xp += 2500;
    const got = Meta.grantLevels();
    Meta.checkAchievements().forEach(achToast);
    refreshProfile();
    if (!$('modal').classList.contains('hidden')) renderModal(true);
    toast('+2,500 Path XP' + (got.length ? ' · now level ' + Meta.levelInfo(Meta.p.xp).level : ''), got.length > 0);
  };
  $('givePow').innerHTML = PU.map((p, i) => i && p[4] !== 'admin' ? `<option value="${i}">${p[0]}</option>` : '').join('');
  $('giveRel').innerHTML = RELICS.slice(1).map((p, i) => `<option value="${i + 1}">${p[0]}</option>`).join('');
  $('givePowBtn').onclick = () => { devTouched = true; call('debug_give', ['number', 'number'], [+$('givePow').value, 0]); };
  $('giveRelBtn').onclick = () => {
    devTouched = true;
    const k = +$('giveRel').value, slot = relicNow[0] && relicNow[0] !== k ? 1 : 0;
    call('debug_relic', ['number', 'number'], [slot, k]);
  };
  $('resetProf').onclick = () => { Meta.reset(); $('name').value = ''; refreshProfile(); sendLook(); toast('Profile reset'); };
  for (const b of document.querySelectorAll('#dev button')) b.addEventListener('mouseup', () => b.blur());

  setMenu(true);
  $('name').focus();
  // Achievements you already earned (for example before they existed) unlock on the next visit.
  Meta.checkAchievements().forEach((id, i) => setTimeout(() => { achToast(id); refreshProfile(); }, 800 + i * 450));
  // Canvas text uses Fredoka: redraw the lobby once the font has loaded.
  if (document.fonts) document.fonts.ready.then(() => refreshProfile());
})();
