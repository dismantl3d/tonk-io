// Tonk.io sound effects, synthesized with WebAudio (no sound files to download).
// Only for superpowers, rocket / air-strike explosions, hitting and knocking out tanks, and picking things up.
// Every sound is built from a few short oscillator and filtered-noise layers with quick envelopes, a little shared
// reverb and a compressor at the end so loud moments don't clip.
'use strict';
const Sfx = (() => {
  const KEY = 'tonk.sound.v1';
  let set = { on: true, vol: 0.7 };
  try { set = Object.assign(set, JSON.parse(localStorage.getItem(KEY) || '{}')); } catch (e) { /* private mode */ }
  const saveSet = () => { try { localStorage.setItem(KEY, JSON.stringify(set)); } catch (e) {} };

  let ctx = null, master = null, verb = null, noiseBuf = null;
  // The game's speed: slow motion stretches sounds and lowers their pitch; pause suspends all audio.
  let rate = 1, paused = false;
  function init() {
    if (ctx) return true;
    const AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) return false;
    ctx = new AC();
    const comp = ctx.createDynamicsCompressor();
    comp.threshold.value = -14; comp.knee.value = 10; comp.ratio.value = 5; comp.attack.value = 0.003; comp.release.value = 0.18;
    master = ctx.createGain();
    master.gain.value = set.on ? set.vol : 0;
    master.connect(comp).connect(ctx.destination);
    // A small room: decaying stereo noise as the impulse response.
    const len = Math.floor(ctx.sampleRate * 1.3), ir = ctx.createBuffer(2, len, ctx.sampleRate);
    for (let c = 0; c < 2; c++) {
      const d = ir.getChannelData(c);
      for (let i = 0; i < len; i++) d[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / len, 3.2);
    }
    const conv = ctx.createConvolver(); conv.buffer = ir;
    verb = ctx.createGain(); verb.gain.value = 0.55;
    verb.connect(conv).connect(master);
    noiseBuf = ctx.createBuffer(1, ctx.sampleRate, ctx.sampleRate);
    const nd = noiseBuf.getChannelData(0);
    for (let i = 0; i < nd.length; i++) nd[i] = Math.random() * 2 - 1;
    return true;
  }
  // Browsers only allow sound after the player has clicked or pressed a key.
  const unlock = () => { if (init() && ctx.state !== 'running' && !paused) ctx.resume(); };
  addEventListener('pointerdown', unlock, true);
  addEventListener('keydown', unlock, true);

  // ---- building blocks ----
  // A bus for one sound: volume + stereo pan, with some of it sent to the reverb.
  function bus(vol, pan, wet = 0.15) {
    const g = ctx.createGain(); g.gain.value = vol;
    let node = g;
    if (ctx.createStereoPanner && pan) { const p = ctx.createStereoPanner(); p.pan.value = pan; g.connect(p); node = p; }
    node.connect(master);
    if (wet > 0) { const s = ctx.createGain(); s.gain.value = wet; node.connect(s); s.connect(verb); }
    return g;
  }
  function env(param, t, peak, a, dur) {
    param.setValueAtTime(0.0001, t);
    param.exponentialRampToValueAtTime(Math.max(peak, 0.0002), t + Math.max(a, 0.002));
    param.exponentialRampToValueAtTime(0.0001, t + Math.max(dur, a + 0.01));
  }
  // An oscillator note that slides from f0 to f1. o: { type, f0, f1, t, dur, g, a, slide ('exp' | 'lin'), lp, det }
  function tone(out, o) {
    const fr = Math.sqrt(rate);
    const t = ctx.currentTime + (o.t || 0) / rate, dur = (o.dur || 0.2) / rate;
    const osc = ctx.createOscillator();
    osc.type = o.type || 'sine';
    osc.frequency.setValueAtTime(o.f0 * fr, t);
    if (o.f1 && o.f1 !== o.f0) {
      if (o.slide === 'lin') osc.frequency.linearRampToValueAtTime(o.f1 * fr, t + (o.sdur ? o.sdur / rate : dur));
      else osc.frequency.exponentialRampToValueAtTime(o.f1 * fr, t + (o.sdur ? o.sdur / rate : dur));
    }
    if (o.det) osc.detune.value = o.det;
    const g = ctx.createGain();
    env(g.gain, t, o.g || 0.2, (o.a || 0.005) / rate, dur);
    let head = osc;
    if (o.lp) { const f = ctx.createBiquadFilter(); f.type = 'lowpass'; f.frequency.value = o.lp; f.Q.value = o.q || 0.7; osc.connect(f); head = f; }
    head.connect(g).connect(out);
    if (o.vib) {  // vibrato
      const l = ctx.createOscillator(), lg = ctx.createGain();
      l.frequency.value = o.vib[0] * rate; lg.gain.value = o.vib[1];
      l.connect(lg).connect(osc.frequency); l.start(t); l.stop(t + dur + 0.05);
    }
    osc.start(t); osc.stop(t + dur + 0.05);
  }
  // Filtered noise whose filter sweeps from f0 to f1. o: { type, f0, f1, q, t, dur, g, a }
  function noise(out, o) {
    const fr = Math.sqrt(rate);
    const t = ctx.currentTime + (o.t || 0) / rate, dur = (o.dur || 0.2) / rate;
    const src = ctx.createBufferSource();
    src.buffer = noiseBuf; src.loop = true;
    src.playbackRate.value = (o.rate || 1) * rate;
    const f = ctx.createBiquadFilter();
    f.type = o.type || 'lowpass'; f.Q.value = o.q || 0.8;
    f.frequency.setValueAtTime(o.f0 * fr, t);
    if (o.f1 && o.f1 !== o.f0) f.frequency.exponentialRampToValueAtTime(o.f1 * fr, t + (o.sdur ? o.sdur / rate : dur));
    if (o.f2) f.frequency.exponentialRampToValueAtTime(o.f2 * fr, t + dur);
    const g = ctx.createGain();
    env(g.gain, t, o.g || 0.3, (o.a || 0.004) / rate, dur);
    src.connect(f).connect(g).connect(out);
    src.start(t, Math.random() * 0.5); src.stop(t + dur + 0.05);
  }
  const jitter = (k = 0.06) => 1 + (Math.random() * 2 - 1) * k;

  // ---- superpowers (same order as Power in core/powerups.h) ----
  const POWER = {
    1(o) {  // Speed Boost: a rising whoosh
      noise(o, { type: 'bandpass', f0: 350, f1: 3200, q: 1.3, dur: 0.42, g: 0.9, a: 0.04 });
      tone(o, { type: 'sawtooth', f0: 200, f1: 640, dur: 0.3, g: 0.12, lp: 1800 });
      tone(o, { f0: 520, f1: 1040, dur: 0.22, g: 0.14, t: 0.05 });
    },
    2(o) {  // Shield: a bright, shimmering chord
      tone(o, { f0: 170, f1: 250, dur: 0.3, g: 0.28, a: 0.01 });
      [523, 659, 784, 1047].forEach((f, i) => { tone(o, { f0: f, dur: 0.75, g: 0.07, a: 0.01, t: i * 0.015, det: (i - 1.5) * 6 }); tone(o, { type: 'triangle', f0: f * 2, dur: 0.4, g: 0.02, t: i * 0.015 }); });
    },
    3(o) {  // Rocket: a launch hiss and a thump
      noise(o, { f0: 3200, f1: 260, dur: 0.55, g: 0.6, a: 0.004 });
      tone(o, { f0: 150, f1: 48, dur: 0.28, g: 0.55 });
      tone(o, { type: 'sawtooth', f0: 95, f1: 42, dur: 0.32, g: 0.12, lp: 600 });
    },
    4(o) {  // Homing Rocket: two lock-on beeps, then a lighter launch
      tone(o, { type: 'square', f0: 1320, dur: 0.06, g: 0.06, lp: 4000 });
      tone(o, { type: 'square', f0: 1760, dur: 0.07, g: 0.06, lp: 4000, t: 0.08 });
      noise(o, { f0: 2600, f1: 380, dur: 0.45, g: 0.42, t: 0.14 });
      tone(o, { f0: 130, f1: 55, dur: 0.24, g: 0.4, t: 0.14 });
    },
    5(o) {  // Rapid Shot: three quick rising blips
      [660, 880, 1175].forEach((f, i) => { tone(o, { type: 'square', f0: f, dur: 0.075, g: 0.07, lp: 3500, t: i * 0.05 }); noise(o, { type: 'highpass', f0: 4000, dur: 0.02, g: 0.12, t: i * 0.05 }); });
      tone(o, { f0: 1175, f1: 1568, dur: 0.18, g: 0.06, t: 0.15 });
    },
    6(o) {  // Air Strike: a radio double-beep, then the plane passing over
      tone(o, { type: 'square', f0: 1000, dur: 0.08, g: 0.05, lp: 2200 });
      tone(o, { type: 'square', f0: 1000, dur: 0.08, g: 0.05, lp: 2200, t: 0.13 });
      noise(o, { f0: 350, f1: 2000, f2: 300, sdur: 0.55, dur: 1.15, g: 0.4, a: 0.35, t: 0.2 });
      tone(o, { type: 'sawtooth', f0: 125, f1: 92, dur: 1.1, g: 0.05, a: 0.3, lp: 500, t: 0.2 });
    },
    7(o) {  // Spread Shot: a fan of three plucks
      [392, 494, 587].forEach((f, i) => tone(o, { type: 'triangle', f0: f * 2, f1: f * 1.96, dur: 0.28, g: 0.18, t: i * 0.028 }));
      noise(o, { type: 'bandpass', f0: 2200, q: 1.2, dur: 0.09, g: 0.25 });
    },
    8(o) {  // Lightning: a falling zap, electric crackles and a low rumble
      tone(o, { type: 'sawtooth', f0: 1900, f1: 70, dur: 0.38, g: 0.11, lp: 5000 });
      for (let i = 0; i < 9; i++) noise(o, { type: 'highpass', f0: 2500 + Math.random() * 3000, dur: 0.02 + Math.random() * 0.04, g: 0.3, t: Math.random() * 0.32 });
      noise(o, { f0: 260, f1: 90, dur: 0.85, g: 0.45, a: 0.02 });
    },
    9(o) {  // Dash: a short whoosh
      noise(o, { type: 'bandpass', f0: 700, f1: 2600, q: 1.4, dur: 0.2, g: 0.9, a: 0.015 });
      tone(o, { f0: 320, f1: 150, dur: 0.12, g: 0.25 });
    },
    10(o) {  // Turret: clunk, servo, ready ping
      noise(o, { type: 'highpass', f0: 1800, dur: 0.025, g: 0.4 });
      tone(o, { f0: 120, f1: 70, dur: 0.1, g: 0.35 });
      tone(o, { type: 'square', f0: 150, f1: 330, dur: 0.2, g: 0.06, lp: 900, t: 0.04 });
      noise(o, { type: 'highpass', f0: 1800, dur: 0.025, g: 0.3, t: 0.16 });
      tone(o, { f0: 1245, dur: 0.18, g: 0.09, t: 0.24 });
    },
    11(o) {  // Teleport: a warbling upward sweep with sparkles
      tone(o, { f0: 280, f1: 1900, dur: 0.28, g: 0.3, vib: [28, 45] });
      tone(o, { type: 'triangle', f0: 140, f1: 950, dur: 0.28, g: 0.18 });
      for (let i = 0; i < 6; i++) tone(o, { f0: 2000 + Math.random() * 2200, dur: 0.09, g: 0.07, t: 0.12 + i * 0.035 });
    },
    12(o) {  // Invisibility: a soft shimmer that fades downward
      tone(o, { f0: 1568, f1: 392, dur: 0.55, g: 0.11, a: 0.02 });
      tone(o, { type: 'triangle', f0: 1047, f1: 262, dur: 0.55, g: 0.08, a: 0.02, det: 8 });
      noise(o, { type: 'highpass', f0: 7000, f1: 1500, dur: 0.5, g: 0.08, a: 0.05 });
    },
    13(o) {  // Singularity: a deep falling drone with a swirling sweep
      tone(o, { f0: 230, f1: 38, dur: 1.15, g: 0.38, a: 0.05 });
      tone(o, { type: 'sawtooth', f0: 110, f1: 55, dur: 1.0, g: 0.06, a: 0.15, lp: 700, vib: [7, 6] });
      noise(o, { type: 'bandpass', f0: 300, f1: 1400, f2: 120, sdur: 0.4, q: 3, dur: 1.0, g: 0.3, a: 0.1 });
    },
    14(o) {  // Instinct: two heartbeats and a bell
      tone(o, { f0: 95, f1: 50, dur: 0.13, g: 0.55 });
      tone(o, { f0: 95, f1: 50, dur: 0.13, g: 0.45, t: 0.17 });
      tone(o, { f0: 1760, dur: 0.5, g: 0.07, t: 0.3 });
      tone(o, { f0: 2637, dur: 0.35, g: 0.035, t: 0.3 });
    },
    15(o) {  // Sentry Robot: a clank, a power-up whine and a two-note "online" chirp
      noise(o, { type: 'highpass', f0: 1500, dur: 0.03, g: 0.4 });
      tone(o, { f0: 110, f1: 65, dur: 0.12, g: 0.4 });
      tone(o, { type: 'sawtooth', f0: 90, f1: 520, dur: 0.38, g: 0.07, lp: 1400, t: 0.05 });
      tone(o, { type: 'square', f0: 988, dur: 0.07, g: 0.06, lp: 3000, t: 0.42 });
      tone(o, { type: 'square', f0: 1319, dur: 0.1, g: 0.06, lp: 3000, t: 0.5 });
    },
    16(o) {  // Coconut Barrage (admin): a quick run of hollow wooden knocks
      for (let i = 0; i < 5; i++) {
        tone(o, { type: 'triangle', f0: 520 - i * 40, f1: 300 - i * 25, dur: 0.07, g: 0.22, t: i * 0.06 });
        noise(o, { type: 'bandpass', f0: 1800, q: 4, dur: 0.03, g: 0.12, t: i * 0.06 });
      }
    },
    17(o) {  // Ivy Snare (admin): a leafy rustle and a low creak
      noise(o, { type: 'bandpass', f0: 2600, f1: 900, q: 1.5, dur: 0.45, g: 0.18, a: 0.04 });
      tone(o, { type: 'sawtooth', f0: 70, f1: 110, dur: 0.4, g: 0.08, lp: 500, t: 0.05 });
    },
  };

  function explosion(o, kind) {
    const big = kind === 1, k = jitter(0.08);
    noise(o, { f0: (big ? 2600 : 2000) * k, f1: 140, dur: big ? 1.0 : 0.75, g: 0.85, a: 0.003 });
    tone(o, { f0: (big ? 115 : 135) * k, f1: 34, dur: big ? 0.5 : 0.38, g: 0.85 });
    noise(o, { type: 'highpass', f0: 1600, dur: 0.14, g: 0.22 });
    tone(o, { type: 'triangle', f0: 70 * k, f1: 40, dur: 0.6, g: 0.25, a: 0.01 });
  }
  function wallBreak(o) {
    for (let i = 0; i < 7; i++) noise(o, { type: 'bandpass', f0: 280 + Math.random() * 700, q: 1.8, dur: 0.06 + Math.random() * 0.08, g: 0.6, t: Math.random() * 0.35 });
    tone(o, { f0: 85, f1: 42, dur: 0.22, g: 0.5 });
  }
  function hit(o) {
    const k = jitter(0.07);
    tone(o, { type: 'triangle', f0: 1500 * k, f1: 950 * k, dur: 0.06, g: 0.3 });
    noise(o, { type: 'highpass', f0: 3200, dur: 0.025, g: 0.2 });
  }
  function kill(o) {
    tone(o, { f0: 170, f1: 55, dur: 0.2, g: 0.5 });
    noise(o, { type: 'bandpass', f0: 1200, q: 0.9, dur: 0.12, g: 0.3 });
    tone(o, { f0: 880, dur: 0.32, g: 0.13, t: 0.03 }); tone(o, { type: 'triangle', f0: 1760, dur: 0.2, g: 0.04, t: 0.03 });
    tone(o, { f0: 1319, dur: 0.45, g: 0.13, t: 0.11 }); tone(o, { type: 'triangle', f0: 2637, dur: 0.25, g: 0.04, t: 0.11 });
    noise(o, { type: 'highpass', f0: 6000, dur: 0.25, g: 0.05, t: 0.1 });
  }
  // Upgrades: a pop and a note that gets higher and fuller with rarity.
  const UP_NOTES = [[659], [784], [880, 1319], [988, 1480, 1976], [1047, 1319, 1568, 2093]];
  function upgrade(o, r) {
    tone(o, { f0: 420, f1: 900, dur: 0.06, g: 0.25 });
    (UP_NOTES[r] || UP_NOTES[0]).forEach((f, i) => {
      tone(o, { f0: f, dur: 0.28, g: 0.16, t: 0.04 + i * 0.055 });
      tone(o, { type: 'triangle', f0: f * 2, dur: 0.14, g: 0.025, t: 0.04 + i * 0.055 });
    });
    if (r === 4) noise(o, { type: 'highpass', f0: 6500, dur: 0.4, g: 0.06, t: 0.1, a: 0.05 });
  }
  function powerPickup(o, mythic) {
    const notes = mythic ? [523, 659, 784, 1047, 1319, 1568] : [523, 659, 784, 1047];
    notes.forEach((f, i) => tone(o, { type: 'square', f0: f, dur: 0.12, g: 0.08, lp: 2800, t: i * 0.045 }));
    tone(o, { f0: notes[notes.length - 1], dur: 0.4, g: 0.13, t: notes.length * 0.045 });
    if (mythic) {
      [1047, 1319, 1568].forEach((f) => tone(o, { f0: f, dur: 1.0, g: 0.05, a: 0.02, t: 0.3, vib: [5, 6] }));
      noise(o, { type: 'highpass', f0: 6000, f1: 2500, dur: 0.8, g: 0.07, t: 0.25, a: 0.1 });
    }
  }
  function relicPickup(o) {
    tone(o, { f0: 262, dur: 0.45, g: 0.12, a: 0.01 });
    [1047, 1319, 1568, 2093].forEach((f, i) => tone(o, { f0: f, dur: 0.65, g: 0.06, a: 0.005, t: i * 0.06 }));
    noise(o, { type: 'highpass', f0: 5000, dur: 0.5, g: 0.04, t: 0.05, a: 0.05 });
  }

  // Don't stack too many of the same sound at once.
  const last = {}, GAP = { 1: 0.05, 2: 0.035, 7: 0.08 };
  // kind: 0 superpower, 1 explosion, 2 hit, 3 kill, 4 upgrade, 5 superpower pickup, 6 relic pickup, 7 wall breaks
  function play(kind, a, vol = 1, pan = 0, extra = {}) {
    if (!set.on || !ctx || ctx.state !== 'running' || document.hidden) return;
    const now = ctx.currentTime;
    if (GAP[kind] && last[kind] && now - last[kind] < GAP[kind]) return;
    last[kind] = now;
    const o = bus(vol, pan, kind === 6 || kind === 0 && (a === 2 || a === 13) ? 0.3 : kind === 2 ? 0.04 : 0.15);
    if (kind === 0) (POWER[a] || POWER[9])(o);
    else if (kind === 1) explosion(o, a);
    else if (kind === 2) hit(o);
    else if (kind === 3) kill(o);
    else if (kind === 4) upgrade(o, a);
    else if (kind === 5) powerPickup(o, extra.mythic);
    else if (kind === 6) relicPickup(o);
    else if (kind === 7) wallBreak(o);
  }
  function setOn(on) {
    set.on = !!on; saveSet();
    if (ctx) master.gain.setTargetAtTime(set.on ? set.vol : 0, ctx.currentTime, 0.02);
  }
  // s: the game's speed (0 pause, < 1 slow motion; fast-forward plays sounds normally).
  function setRate(s) {
    paused = s <= 0;
    rate = paused ? rate : Math.min(1, Math.max(0.05, s));
    if (!ctx) return;
    if (paused) ctx.suspend();
    else if (ctx.state === 'suspended') ctx.resume();
  }
  return { play, setOn, setRate, get on() { return set.on; }, get state() { return ctx ? ctx.state : 'off'; }, unlock };
})();
