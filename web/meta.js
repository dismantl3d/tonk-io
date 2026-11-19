// Tonk.io meta game: catalog, profile, Credits, Legendary Path, Shop, Locker and item previews.
// Saved in this browser (localStorage) until Firebase is connected. Then the trusted server writes Credits, XP and
// inventory and purchases go through a Cloud Function (GAME_DESIGN.md section 12); this file only reads them.
'use strict';
const Meta = (() => {
  // ================= Catalog =================
  const RARITY = ['Common', 'Uncommon', 'Rare', 'Epic', 'Legendary'];
  const RCOL = ['#b4b9c6', '#5cd65c', '#3fa9f5', '#a45cff', '#ffb31f'];  // same as rarityCol() in client/main.cpp
  const CATS = [
    { id: 'wrap', name: 'Wraps', one: 'Wrap' },
    { id: 'barrel', name: 'Barrels', one: 'Barrel' },
    { id: 'trail', name: 'Trails', one: 'Trail' },
    { id: 'muzzle', name: 'Muzzle Flashes', one: 'Muzzle Flash' },
    { id: 'killfx', name: 'Kill FX', one: 'Kill FX' },
    { id: 'deathfx', name: 'Death FX', one: 'Death FX' },
    { id: 'title', name: 'Titles', one: 'Title' },
    { id: 'tag', name: 'Name Tag Colors', one: 'Name Tag Color' },
  ];
  const CAT = Object.fromEntries(CATS.map((c) => [c.id, c]));

  // Where an item comes from. Path items are only on the Path, featured items only in the 3-hour Featured rotation.
  const SRC = { default: 'Starter', shop: 'Shop', featured: 'Featured rotation', path: 'Legendary Path', ach: 'Achievement',
                admin: 'Admin exclusive' };

  // Patterns (same numbers as the shader in client/main.cpp):
  // 0 solid, 1 stripes, 2 checker, 3 camo, 4 gradient, 5 chrome, 6 rainbow, 7 swirl, 8 pulse, 9 dots, 10 rings, 11 split, 12 galaxy,
  // 13 rainbow stripes
  // [id, name, rarity, source, price, collection, description, fields]
  const DEFS = [
    // ---- Wraps ----
    ['wrap.teal', 'Teal', 0, 'default', 0, 'Starter Kit', 'Fresh off the assembly line.', { pat: 0, c1: '#2ec4b6' }],
    ['wrap.red', 'Red', 1, 'path', 0, 'Base Color Palette', 'Part of the Base Color Palette.', { pat: 0, c1: '#ff5a5f' }],
    ['wrap.orange', 'Orange', 1, 'path', 0, 'Base Color Palette', 'Part of the Base Color Palette.', { pat: 0, c1: '#ff9f1c' }],
    ['wrap.yellow', 'Yellow', 1, 'path', 0, 'Base Color Palette', 'Part of the Base Color Palette.', { pat: 0, c1: '#ffd23f' }],
    ['wrap.green', 'Green', 1, 'path', 0, 'Base Color Palette', 'Part of the Base Color Palette.', { pat: 0, c1: '#5cd65c' }],
    ['wrap.blue', 'Blue', 1, 'path', 0, 'Base Color Palette', 'Part of the Base Color Palette.', { pat: 0, c1: '#3fa9f5' }],
    ['wrap.purple', 'Purple', 1, 'path', 0, 'Base Color Palette', 'Part of the Base Color Palette.', { pat: 0, c1: '#9b5de5' }],
    ['wrap.hazard', 'Hazard Stripes', 3, 'path', 0, 'Construction Crew', 'Caution: this tank brakes for nobody.', { pat: 1, c1: '#ffd23f', c2: '#3a3150' }],
    ['wrap.legend', 'Legend', 4, 'path', 0, 'Legend Set', 'Only the ones who finished the Path wear this.', { pat: 7, c1: '#ffb31f', c2: '#7b4dff' }],
    ['wrap.bubblegum', 'Bubblegum', 0, 'shop', 200, 'Pastel Pack', 'Soft pink, hard hits.', { pat: 0, c1: '#ff8fc7' }],
    ['wrap.peach', 'Peach', 0, 'shop', 200, 'Pastel Pack', 'Fuzzy on the outside.', { pat: 0, c1: '#ffb38a' }],
    ['wrap.lilac', 'Lilac', 0, 'shop', 250, 'Pastel Pack', 'A calm color for chaotic fights.', { pat: 0, c1: '#c3a6ff' }],
    ['wrap.seafoam', 'Seafoam', 0, 'shop', 250, 'Pastel Pack', 'Smells faintly of the ocean.', { pat: 0, c1: '#8ee3c8' }],
    ['wrap.bullseye', 'Bullseye', 1, 'shop', 400, 'Arcade', 'Go on, aim for the middle.', { pat: 10, c1: '#ff5a5f', c2: '#ffffff' }],
    ['wrap.pinstripe', 'Pinstripe', 1, 'shop', 450, 'Street', 'Business on the outside, battle on the inside.', { pat: 1, c1: '#3a4170', c2: '#c9d1ee' }],
    ['wrap.checker', 'Checkerboard', 1, 'shop', 450, 'Street', 'Race-day ready.', { pat: 2, c1: '#3a3150', c2: '#f3efe6' }],
    ['wrap.polka', 'Polka Pop', 2, 'shop', 750, 'Street', 'Dots, dots and more dots.', { pat: 9, c1: '#ffd23f', c2: '#ff5a5f' }],
    ['wrap.tiger', 'Tiger', 2, 'shop', 800, 'Wild', 'Stripes earned in the long grass.', { pat: 1, c1: '#ff9f1c', c2: '#3a3150' }],
    ['wrap.jungle', 'Jungle Camo', 2, 'shop', 750, 'Wild', 'Now you see me.', { pat: 3, c1: '#6e9b4e', c2: '#3e5e2e' }],
    ['wrap.sunrise', 'Sunrise', 2, 'shop', 800, 'Skies', 'An early start to every match.', { pat: 4, c1: '#ff8fc7', c2: '#ffd23f' }],
    ['wrap.deepsea', 'Deep Sea', 2, 'shop', 800, 'Skies', 'Pressure-tested at the bottom of the map.', { pat: 4, c1: '#3fa9f5', c2: '#2a2f7f' }],
    ['wrap.bronze', 'Bronze', 2, 'shop', 700, 'Metals', 'Third place never looked this shiny.', { pat: 5, c1: '#c9874a' }],
    ['wrap.goldrush', 'Gold Rush', 3, 'shop', 1500, 'Metals', 'Polished every single morning.', { pat: 5, c1: '#f5b82e' }],
    ['wrap.neon', 'Neon Pulse', 3, 'shop', 1600, 'Arcade', 'Plugged straight into the wall.', { pat: 8, c1: '#ff4fd8', c2: '#4fe3ff' }],
    ['wrap.nebula', 'Nebula', 3, 'shop', 1800, 'Cosmic', 'A small galaxy, swirling.', { pat: 7, c1: '#7b4dff', c2: '#ff8fc7' }],
    ['wrap.prism', 'Prism', 4, 'shop', 3500, 'Rainbow', 'Every color at once.', { pat: 6, c1: '#ff8fc7' }],
    ['wrap.toxic', 'Toxic Camo', 1, 'featured', 0, 'Hazmat', 'Glows a little. Probably fine.', { pat: 3, c1: '#9bea5c', c2: '#3e7a2a' }],
    ['wrap.twoface', 'Two-Face', 2, 'featured', 0, 'Split Decision', 'Half hero, half villain.', { pat: 11, c1: '#3a3150', c2: '#ffffff' }],
    ['wrap.frostbite', 'Frostbite', 2, 'featured', 0, 'Winter Vault', 'Cold to the touch.', { pat: 4, c1: '#e8f7ff', c2: '#5fb8ff' }],
    ['wrap.molten', 'Molten Core', 3, 'featured', 0, 'Volcanic', 'Hot enough to melt the storm.', { pat: 8, c1: '#ff5a1f', c2: '#ffc23f' }],
    ['wrap.starfield', 'Starfield', 3, 'featured', 0, 'Cosmic Vault', 'Twinkle, twinkle, little tank.', { pat: 12, c1: '#2c2a6e', c2: '#ffffff' }],
    ['wrap.aurora', 'Aurora', 4, 'featured', 0, 'Polar Lights', 'The northern lights, on treads.', { pat: 7, c1: '#2ee6a8', c2: '#7b6cff' }],
    ['wrap.rainbowroad', 'Rainbow Road', 3, 'featured', 0, 'Rainbow', 'Every stripe a different color.', { pat: 13, c1: '#ff5a5f' }],
    ['wrap.blossom', 'Cherry Blossom', 1, 'featured', 0, 'Spring Fling', 'Petals on the breeze.', { pat: 9, c1: '#ffd1e3', c2: '#ff7aa8' }],
    ['wrap.zebra', 'Zebra', 1, 'featured', 0, 'Wild', 'Black and white and fast all over.', { pat: 1, c1: '#ffffff', c2: '#3a3150' }],
    ['wrap.midnightcamo', 'Midnight Camo', 2, 'featured', 0, 'Night Shift', 'Made for night raids.', { pat: 3, c1: '#2c3e66', c2: '#141b33' }],
    ['wrap.circuit', 'Circuit', 2, 'featured', 0, 'Arcade', 'Runs on pure electricity.', { pat: 10, c1: '#1fe0a0', c2: '#12303a' }],
    ['wrap.velvet', 'Royal Velvet', 2, 'featured', 0, 'Crown Jewels', 'Fit for a tank king.', { pat: 4, c1: '#ff4f9a', c2: '#5a1f9a' }],
    ['wrap.lavalamp', 'Lava Lamp', 3, 'featured', 0, 'Volcanic', 'Groovy, and very hot.', { pat: 7, c1: '#ff5a5f', c2: '#ffd23f' }],

    // ---- Barrels ----
    ['barrel.slate', 'Slate', 0, 'default', 0, 'Starter Kit', 'Standard issue.', { pat: 0, c1: '#a3a7b8' }],
    ['barrel.copper', 'Copper', 2, 'path', 0, 'Workshop', 'Hand-polished copper.', { pat: 5, c1: '#d9824a' }],
    ['barrel.ivory', 'Ivory', 0, 'shop', 200, 'Basics', 'Clean and simple.', { pat: 0, c1: '#ede6d6' }],
    ['barrel.charcoal', 'Charcoal', 0, 'shop', 200, 'Basics', 'Dark and dependable.', { pat: 0, c1: '#4a4658' }],
    ['barrel.carbon', 'Carbon Fiber', 1, 'shop', 450, 'Street', 'Light, strong, and very fast-looking.', { pat: 2, c1: '#2e2c38', c2: '#56536a' }],
    ['barrel.caution', 'Caution Tape', 1, 'shop', 450, 'Construction Crew', 'Do not cross.', { pat: 1, c1: '#ffd23f', c2: '#3a3150' }],
    ['barrel.chrome', 'Chrome', 2, 'shop', 750, 'Metals', 'You can see yourself in it.', { pat: 5, c1: '#c3ccd8' }],
    ['barrel.gold', 'Gold Barrel', 3, 'shop', 1500, 'Metals', 'Fire in style.', { pat: 5, c1: '#f5b82e' }],
    ['barrel.prism', 'Prism Barrel', 4, 'shop', 3000, 'Rainbow', 'Shoots rainbows. Not really.', { pat: 6, c1: '#ff8fc7' }],
    ['barrel.icicle', 'Icicle', 1, 'featured', 0, 'Winter Vault', 'Keep away from tongues.', { pat: 4, c1: '#e8f7ff', c2: '#7cc8ff' }],
    ['barrel.inferno', 'Inferno', 2, 'featured', 0, 'Volcanic', 'Warm from all that firing.', { pat: 4, c1: '#ff5a1f', c2: '#ffc23f' }],
    ['barrel.neon', 'Neon Tube', 3, 'featured', 0, 'Night Shift', 'Buzzes quietly when idle.', { pat: 8, c1: '#4fe3ff', c2: '#c8faff' }],
    ['barrel.bamboo', 'Bamboo', 1, 'featured', 0, 'Spring Fling', 'Grown, not built.', { pat: 1, c1: '#b5d96b', c2: '#6e9b4e' }],
    ['barrel.midnight', 'Midnight', 1, 'featured', 0, 'Night Shift', 'As dark as it gets.', { pat: 4, c1: '#3a4170', c2: '#0f1330' }],
    ['barrel.rainbow', 'Rainbow Barrel', 2, 'featured', 0, 'Rainbow', 'Striped in every color.', { pat: 13, c1: '#ff5a5f' }],

    // ---- Trails (numbers match trailFx in client/main.cpp) ----
    ['trail.none', 'No Trail', 0, 'default', 0, 'Starter Kit', 'Leave no trace.', { trail: 0 }],
    ['trail.dust', 'Dust Puff', 1, 'path', 0, 'Roadside', 'Kicks up dirt everywhere you go.', { trail: 1 }],
    ['trail.sparkle', 'Sparkle Line', 3, 'path', 0, 'Starlight', 'A line of twinkling stars.', { trail: 2 }],
    ['trail.legend', 'Legend Trail', 4, 'path', 0, 'Legend Set', 'Gold and violet, all the way home.', { trail: 6 }],
    ['trail.bubbles', 'Bubbles', 1, 'shop', 400, 'Bath Time', 'Pop, pop, pop.', { trail: 4 }],
    ['trail.mist', 'Mist', 1, 'shop', 400, 'Skies', 'A light fog that follows you around.', { trail: 7 }],
    ['trail.ember', 'Ember', 2, 'shop', 800, 'Volcanic', 'Hot sparks off the treads.', { trail: 5 }],
    ['trail.plasma', 'Plasma', 3, 'shop', 1500, 'Night Shift', 'Purple energy, slightly sticky.', { trail: 8 }],
    ['trail.rainbow', 'Rainbow Ribbon', 3, 'shop', 1600, 'Rainbow', 'A ribbon of every color.', { trail: 3 }],
    ['trail.comet', 'Comet', 2, 'featured', 0, 'Cosmic Vault', 'A glowing tail in your own color.', { trail: 9 }],
    ['trail.snow', 'Snowfall', 2, 'featured', 0, 'Winter Vault', 'Flurries wherever you roll.', { trail: 11 }],
    ['trail.party', 'Party Trail', 3, 'featured', 0, 'Party Pack', 'Every drive is a parade.', { trail: 10 }],

    // ---- Muzzle flashes (muzzleFx) ----
    ['muzzle.none', 'No Flash', 0, 'default', 0, 'Starter Kit', 'Just the shot.', { muzzle: 0 }],
    ['muzzle.spark', 'Spark', 2, 'path', 0, 'Workshop', 'A shower of hot sparks.', { muzzle: 1 }],
    ['muzzle.lightning', 'Lightning', 3, 'path', 0, 'Storm Chasers', 'Crackles with every shot.', { muzzle: 3 }],
    ['muzzle.flare', 'Flare', 1, 'shop', 400, 'Basics', 'A warm orange puff.', { muzzle: 4 }],
    ['muzzle.ringpop', 'Ring Pop', 1, 'shop', 450, 'Arcade', 'A crisp little ring.', { muzzle: 7 }],
    ['muzzle.star', 'Star Burst', 2, 'shop', 750, 'Starlight', 'Every shot is a star.', { muzzle: 2 }],
    ['muzzle.plasma', 'Plasma Ring', 3, 'shop', 1400, 'Night Shift', 'A violet ring of energy.', { muzzle: 5 }],
    ['muzzle.confetti', 'Confetti Cannon', 3, 'featured', 0, 'Party Pack', 'Celebrate every shot.', { muzzle: 6 }],
    ['muzzle.rainbow', 'Rainbow Burst', 3, 'featured', 0, 'Rainbow', 'A fan of every color with each shot.', { muzzle: 8 }],

    // ---- Kill FX (killFx) ----
    ['killfx.none', 'No Kill FX', 0, 'default', 0, 'Starter Kit', 'Just the knockout.', { kill: 0 }],
    ['killfx.shockwave', 'Shockwave', 3, 'path', 0, 'Storm Chasers', 'Two rings ripple out from your victim.', { kill: 1 }],
    ['killfx.supernova', 'Supernova', 4, 'path', 0, 'Rainbow', 'Rainbow rings and a burst of stars.', { kill: 6 }],
    ['killfx.stars', 'Star Pop', 2, 'shop', 800, 'Starlight', 'Your victim sees stars.', { kill: 2 }],
    ['killfx.coins', 'Coin Shower', 3, 'shop', 1500, 'Jackpot', 'Ka-ching!', { kill: 3 }],
    ['killfx.lightning', 'Lightning Strike', 3, 'shop', 1600, 'Storm Chasers', 'Struck from above.', { kill: 5 }],
    ['killfx.ghost', 'Boo!', 2, 'featured', 0, 'Spooky', 'Their ghost floats away.', { kill: 4 }],
    ['killfx.vortex', 'Vortex', 3, 'featured', 0, 'Cosmic Vault', 'Sparks spiral into the void.', { kill: 7 }],

    // ---- Death FX (deathBurst) ----
    ['deathfx.classic', 'Classic Fade', 0, 'default', 0, 'Starter Kit', 'A quiet exit.', { death: 0 }],
    ['deathfx.confetti', 'Confetti Pop', 2, 'path', 0, 'Party Pack', 'Go out with a party.', { death: 1 }],
    ['deathfx.fireworks', 'Fireworks', 3, 'path', 0, 'Starlight', 'Three bursts in the sky.', { death: 3 }],
    ['deathfx.streamers', 'Streamers', 1, 'shop', 450, 'Party Pack', 'Long ribbons in every color.', { death: 5 }],
    ['deathfx.pixel', 'Pixel Burst', 2, 'shop', 800, 'Arcade', 'Game over, in squares.', { death: 6 }],
    ['deathfx.shatter', 'Shatter', 3, 'shop', 1400, 'Metals', 'Your tank breaks into shards.', { death: 2 }],
    ['deathfx.royal', 'Royal Burst', 4, 'featured', 0, 'Crown Jewels', 'A golden shockwave and royal gems.', { death: 4 }],
    ['deathfx.rainbow', 'Over the Rainbow', 4, 'featured', 0, 'Rainbow', 'Rings of every color ripple out.', { death: 7 }],

    // ---- Titles (Path and achievements only) ----
    ['title.none', 'No Title', 0, 'default', 0, 'Starter Kit', 'Nothing above your name.', { color: '#ffffff' }],
    ['title.newbie', 'Newbie', 0, 'path', 0, 'Path Titles', 'just a small little noob with big dreams :)', { color: '#858585' }],
    ['title.rookie', 'Rookie', 0, 'path', 0, 'Path Titles', "There's still a long way to go...", { color: '#9aa5b1' }],
    ['title.scrapper', 'Scrapper', 0, 'path', 0, 'Path Titles', 'Scraps, scrapes and scrap metal.', { color: '#c49a72' }],
    ['title.ace', 'Tank Ace', 1, 'path', 0, 'Path Titles', 'Knows which end the bullets come out of.', { color: '#3fa9f5' }],
    ['title.sharpshooter', 'Sharpshooter', 2, 'path', 0, 'Path Titles', 'Rarely misses. Rarely.', { color: '#ff5a5f' }],
    ['title.zonerunner', 'Zone Runner', 2, 'path', 0, 'Path Titles', 'Always one step ahead of the storm.', { color: '#9b7bff' }],
    ['title.survivor', 'Survivor', 2, 'path', 0, 'Path Titles', 'Still standing.', { color: '#5cd65c' }],
    ['title.veteran', 'Veteran', 3, 'path', 0, 'Path Titles', 'Has seen a thousand storms close.', { color: '#ff9f1c' }],
    ['title.warlord', 'Warlord', 3, 'path', 0, 'Path Titles', 'Feared across the arena.', { color: '#ff4f6d' }],
    ['title.legend', 'Legend', 4, 'path', 0, 'Legend Set', 'Finished the Legendary Path.', { color: '#ffb31f', anim: 'gold' }],
    ['title.firstblood', 'First Blood', 1, 'ach', 0, 'Achievements', 'Got their first knockout.', { color: '#ff6b6b' }],
    ['title.regular', 'Regular', 1, 'ach', 0, 'Achievements', 'Always up for one more match.', { color: '#7fd1ae' }],
    ['title.contender', 'Contender', 1, 'ach', 0, 'Achievements', 'Never far from the final circle.', { color: '#6fa8ff' }],
    ['title.marathoner', 'Marathoner', 2, 'ach', 0, 'Achievements', 'Survived eight minutes in one match.', { color: '#4fc3ff' }],
    ['title.collector', 'Collector', 2, 'ach', 0, 'Achievements', 'Owns 20 cosmetics.', { color: '#2ec4b6' }],
    ['title.hoarder', 'Hoarder', 2, 'ach', 0, 'Achievements', 'Picks up everything that isn\'t nailed down.', { color: '#e0b94a' }],
    ['title.powerhungry', 'Power Hungry', 2, 'ach', 0, 'Achievements', 'Never saves a superpower for later.', { color: '#ff5fa2' }],
    ['title.pathfinder', 'Pathfinder', 2, 'ach', 0, 'Achievements', 'Halfway down the Legendary Path.', { color: '#47d6c8' }],
    ['title.bigspender', 'Big Spender', 2, 'ach', 0, 'Achievements', 'The shopkeepers know them by name.', { color: '#6ee36e' }],
    ['title.champion', 'Champion', 3, 'ach', 0, 'Achievements', 'Last tank standing.', { color: '#ffc63d' }],
    ['title.exterminator', 'Exterminator', 3, 'ach', 0, 'Achievements', 'Eight knockouts in one match.', { color: '#b47cff' }],
    ['title.giantslayer', 'Giant Slayer', 3, 'ach', 0, 'Achievements', 'Beat three Arena Mages.', { color: '#b388ff' }],
    ['title.centurion', 'Centurion', 3, 'ach', 0, 'Achievements', 'A hundred knockouts and counting.', { color: '#ff8a3d' }],
    ['title.dynasty', 'Dynasty', 4, 'ach', 0, 'Achievements', 'Ten wins. The arena is theirs.', { color: '#ffb31f', anim: 'gold' }],
    ['title.prismatic', 'Prismatic', 4, 'ach', 0, 'Rainbow', 'Owns every color there is.', { color: '#ff8fc7', anim: 'rainbow' }],
    ['title.cratecracker', 'Crate Cracker', 1, 'ach', 0, 'Achievements', 'No crate is safe.', { color: '#d9925a' }],
    ['title.treasurehunter', 'Treasure Hunter', 2, 'ach', 0, 'Achievements', 'Got to the Legendary Crate first.', { color: '#ffcf4a' }],
    ['title.guardianbane', 'Guardian Bane', 2, 'ach', 0, 'Achievements', 'The Temple Guardians fear them.', { color: '#9b8cff' }],
    ['title.templeraider', 'Temple Raider', 3, 'ach', 0, 'Achievements', 'Beat the Arena Mage in its own temple.', { color: '#ff9a3c' }],
    ['title.rocketeer', 'Rocketeer', 3, 'ach', 0, 'Achievements', 'Won a Rocket Rampage.', { color: '#ff5a5f' }],
    ['title.demolition', 'Demolition Expert', 2, 'ach', 0, 'Achievements', 'Scored 1,500 in one Rocket Rampage.', { color: '#ff7a3d' }],

    // ---- Name tag colors ----
    ['tag.white', 'White', 0, 'default', 0, 'Starter Kit', 'Plain and readable.', { color: '#ffffff' }],
    ['tag.sky', 'Sky', 0, 'path', 0, 'Path Colors', 'Clear skies ahead.', { color: '#8fd3ff' }],
    ['tag.mint', 'Mint', 1, 'path', 0, 'Path Colors', 'Cool and fresh.', { color: '#8ef0c0' }],
    ['tag.sunset', 'Sunset', 2, 'path', 0, 'Path Colors', 'Pink fading into gold.', { grad: ['#ff8fc7', '#ffd23f'] }],
    ['tag.lava', 'Lava', 2, 'path', 0, 'Path Colors', 'Slowly flowing.', { grad: ['#ff5a1f', '#ffc23f', '#ff5a1f'], anim: 'flow' }],
    ['tag.frost', 'Frost', 3, 'path', 0, 'Path Colors', 'An icy glow.', { grad: ['#ffffff', '#8fd3ff'], glow: '#7cc8ff' }],
    ['tag.holo', 'Holo', 4, 'path', 0, 'Rainbow', 'Shifts through every color.', { holo: true }],
    ['tag.ruby', 'Ruby', 0, 'shop', 200, 'Gemstones', 'A deep red name.', { color: '#ff6b78' }],
    ['tag.lemon', 'Lemon', 0, 'shop', 200, 'Basics', 'Bright and zesty.', { color: '#ffe45c' }],
    ['tag.grape', 'Grape', 1, 'shop', 400, 'Gemstones', 'A juicy purple.', { color: '#b18cff' }],
    ['tag.emerald', 'Emerald', 1, 'shop', 400, 'Gemstones', 'Green with envy.', { color: '#6deb8f' }],
    ['tag.neon', 'Neon Glow', 2, 'shop', 800, 'Night Shift', 'Glows in the dark.', { color: '#9ff6ff', glow: '#2ee6ff' }],
    ['tag.gold', 'Gold Leaf', 3, 'shop', 1300, 'Metals', 'Every letter in gold.', { grad: ['#ffe38a', '#e89b14'], glow: '#ffb31f' }],
    ['tag.toxic', 'Toxic Glow', 2, 'featured', 0, 'Hazmat', 'Radioactive lettering.', { color: '#c4ff6a', glow: '#6cff2e' }],
    ['tag.galaxy', 'Galaxy', 3, 'featured', 0, 'Cosmic Vault', 'Stars in every letter.', { grad: ['#7b4dff', '#ff8fc7', '#4fe3ff', '#7b4dff'], anim: 'flow' }],
    ['tag.rainbow', 'Rainbow', 3, 'featured', 0, 'Rainbow', 'Every color, flowing through your name.',
      { grad: ['#ff5a5f', '#ff9f1c', '#ffd23f', '#5cd65c', '#3fa9f5', '#9b5de5', '#ff5a5f'], anim: 'flow' }],
    ['tag.cottoncandy', 'Cotton Candy', 1, 'featured', 0, 'Pastel Pack', 'Sweet pink into lilac.', { grad: ['#ff8fc7', '#c3a6ff'] }],
    ['tag.ocean', 'Ocean', 2, 'featured', 0, 'Skies', 'Waves of blue.', { grad: ['#4fe3ff', '#3fa9f5', '#4fe3ff'], anim: 'flow' }],

    // ---- Admin exclusives (one account only; hidden from everyone else) ----
    ['wrap.cocoivy', 'Coconut Ivy', 4, 'admin', 0, 'Coco Ivy', 'Woven from ivy and coconut husk. Only one tank wears it.', { pat: 7, c1: '#2e8b57', c2: '#f3dcb0' }],
    ['wrap.cosmiccoco', 'Cosmic Coconut', 4, 'admin', 0, 'Coco Ivy', 'A coconut that fell out of the sky, and kept going.', { pat: 12, c1: '#3b2a1f', c2: '#fff1d6' }],
    ['barrel.ivyvine', 'Ivy Vine', 4, 'admin', 0, 'Coco Ivy', 'It grew there on its own.', { pat: 1, c1: '#3fae5a', c2: '#1f6b35' }],
    ['trail.cocoparty', 'Coconut Parade', 4, 'admin', 0, 'Coco Ivy', 'A party follows you everywhere.', { trail: 10 }],
    ['muzzle.coco', 'Coconut Pop', 4, 'admin', 0, 'Coco Ivy', 'Every shot, a little celebration.', { muzzle: 6 }],
    ['killfx.coco', 'Coconut Bonk', 4, 'admin', 0, 'Coco Ivy', 'Rainbows for whoever you beat.', { kill: 6 }],
    ['deathfx.coco', 'Royal Coconut', 4, 'admin', 0, 'Coco Ivy', 'Even going out is royal.', { death: 4 }],
    ['title.admin', 'Head Admin', 4, 'admin', 0, 'Coco Ivy', 'Runs the arena.', { color: '#ffb31f', anim: 'gold' }],
    ['title.coco', 'The Coconut', 4, 'admin', 0, 'Coco Ivy', 'Hard shell, sweet inside.', { color: '#8ef0c0', anim: 'rainbow' }],
    ['tag.ivy', 'Ivy Glow', 4, 'admin', 0, 'Coco Ivy', 'Your name, overgrown.', { grad: ['#b6ff9a', '#2e8b57', '#b6ff9a'], glow: '#3fae5a', anim: 'flow' }],
  ];
  const ITEMS = {};
  for (const [id, name, rarity, src, price, coll, desc, f] of DEFS) ITEMS[id] = { id, name, rarity, src, price, coll, desc, cat: id.split('.')[0], ...f };
  const DEFAULTS = { wrap: 'wrap.teal', barrel: 'barrel.slate', trail: 'trail.none', muzzle: 'muzzle.none', killfx: 'killfx.none',
                     deathfx: 'deathfx.classic', title: 'title.none', tag: 'tag.white' };
  const SHOP_ITEMS = Object.values(ITEMS).filter((i) => i.src === 'shop');
  const FEATURED_POOL = Object.values(ITEMS).filter((i) => i.src === 'featured');
  const FEATURED_PRICE = [500, 700, 1200, 2200, 4500];  // higher than the shop for the same rarity
  const FEATURED_COUNT = 4;  // items in each 3-hour Featured rotation
  const typeName = (it) => CAT[it.cat].one;
  // Collections, in the order they first appear in the catalog.
  const COLLECTIONS = [...new Set(DEFS.filter((d) => d[3] !== 'admin').map((d) => d[5]))];
  // Admin exclusives: only shown, and only worn, while the admin account is signed in (this session).
  const ADMIN_ITEMS = Object.values(ITEMS).filter((i) => i.src === 'admin');
  let admin = false;
  const isVisible = (i) => !!i && (i.src !== 'admin' || admin);
  const collItems = (name) => Object.values(ITEMS).filter((i) => i.coll === name);

  // ================= Legendary Path (60 levels) =================
  // Big cosmetics are spaced out; Credits, Arsenal Tokens, titles and name tag colors fill the levels between them.
  const PATH_DEF = [
    'title.newbie', 'cr:20', 'at:1', 'cr:25', 'title.rookie', 'wrap.red', 'cr:30', 'tag.sky', 'cr:35', 'trail.dust',
    'at:1', 'title.scrapper', 'cr:40', 'wrap.orange', 'muzzle.spark', 'cr:45', 'tag.mint', 'at:1', 'cr:50', 'barrel.copper',
    'title.ace', 'cr:55', 'wrap.yellow', 'at:1', 'deathfx.confetti', 'cr:60', 'tag.sunset', 'cr:65', 'title.sharpshooter', 'killfx.shockwave',
    'cr:70', 'at:2', 'wrap.green', 'cr:75', 'trail.sparkle', 'title.zonerunner', 'cr:80', 'tag.lava', 'at:2', 'wrap.hazard',
    'cr:85', 'title.survivor', 'cr:90', 'wrap.blue', 'muzzle.lightning', 'cr:95', 'at:2', 'tag.frost', 'cr:100', 'deathfx.fireworks',
    'title.veteran', 'cr:110', 'wrap.purple', 'at:3', 'killfx.supernova', 'cr:120', 'title.warlord', 'tag.holo', 'cr:150',
    'wrap.legend|trail.legend|title.legend',
  ];
  const PATH = PATH_DEF.map((s) => s.split('|').map((r) => {
    if (r.startsWith('cr:')) return { cr: +r.slice(3) };
    if (r.startsWith('at:')) return { at: +r.slice(3) };
    return { item: r };
  }));
  const MAX_LEVEL = PATH.length;  // 60
  // XP to go from level L to L+1. Each step costs more than the last, and the increase itself grows a little.
  const xpToNext = (L) => { const k = L - 1; return Math.round(200 + 15 * k + 0.8 * k * k); };
  function levelInfo(xp) {
    let L = 1, rest = xp;
    while (L < MAX_LEVEL && rest >= xpToNext(L)) { rest -= xpToNext(L); L++; }
    return { level: L, into: L >= MAX_LEVEL ? xpToNext(L) : rest, need: xpToNext(L), max: L >= MAX_LEVEL };
  }
  const pathLevelOf = {};
  PATH.forEach((rs, i) => rs.forEach((r) => { if (r.item) pathLevelOf[r.item] = i + 1; }));

  // A Path reward as a tooltip-able "thing": items, or Credits / Arsenal Tokens (type Currency, no rarity).
  function rewardInfo(r) {
    if (r.item) { const it = ITEMS[r.item]; return { name: it.name, type: typeName(it), rarity: it.rarity, desc: it.desc, item: it }; }
    if (r.cr) return { name: r.cr + ' Credits', type: 'Currency', rarity: -1, desc: '', cr: r.cr };
    return { name: r.at + ' Arsenal Token' + (r.at > 1 ? 's' : ''), type: 'Currency', rarity: -1, desc: 'Use this in the Arsenal', at: r.at };
  }

  // ================= Profile =================
  const KEY = 'tonk.profile.v2', OLD_KEY = 'tonk.profile.v1';
  let P;
  function fresh() {
    return { credits: 0, xp: 0, tokens: 0, claimed: 0, owned: Object.values(DEFAULTS), equipped: { ...DEFAULTS }, name: '',
             lastWinDay: '', seenShopSlot: -1, achDone: [], hideLocked: true,
             stats: { matches: 0, wins: 0, kills: 0, bestPlace: 0, bestKills: 0, longest: 0, upgrades: 0, powers: 0, top10: 0, guardians: 0,
                      spent: 0, crates: 0, legend: 0, mobs: 0, mages: 0, rampage: 0, rampWins: 0, rampBest: 0 } };
  }
  function load() {
    let raw = null;
    try { raw = JSON.parse(localStorage.getItem(KEY) || 'null'); } catch (e) { raw = null; }
    if (!raw) {  // carry Credits, Path XP, name and career stats over from the first version of the profile
      let old = null;
      try { old = JSON.parse(localStorage.getItem(OLD_KEY) || 'null'); } catch (e) { old = null; }
      raw = old ? { credits: old.credits || 0, xp: old.xp || 0, tokens: old.tokens || 0, name: old.name || '', stats: old.stats || {} } : {};
    }
    P = Object.assign(fresh(), raw);
    P.stats = Object.assign(fresh().stats, P.stats);
    P.achDone = (P.achDone || []).filter((id) => ITEMS[id]);
    P.owned = [...new Set([...Object.values(DEFAULTS), ...P.owned.filter((id) => ITEMS[id])])];
    P.equipped = Object.assign({ ...DEFAULTS }, P.equipped);
    for (const k of Object.keys(DEFAULTS)) if (!ITEMS[P.equipped[k]] || !P.owned.includes(P.equipped[k])) P.equipped[k] = DEFAULTS[k];
    P.adminEq = P.adminEq || {};
    stashAdmin();
    grantLevels();
  }
  // Signed out of the admin account: put its exclusives away (remembered for next time) and wear the defaults.
  function stashAdmin() {
    for (const k of Object.keys(DEFAULTS)) {
      const it = ITEMS[P.equipped[k]];
      if (it && it.src === 'admin') { P.adminEq[k] = it.id; P.equipped[k] = DEFAULTS[k]; }
    }
  }
  function setAdmin(on) {
    admin = !!on;
    if (admin) {  // wear the exclusives again
      for (const [k, id] of Object.entries(P.adminEq || {})) if (P.owned.includes(id)) P.equipped[k] = id;
    } else stashAdmin();
    save();
  }
  // The admin's exclusives, given the first time they sign in (returns the ones just given).
  function grantAdmin() {
    const got = ADMIN_ITEMS.filter((i) => !P.owned.includes(i.id));
    for (const i of got) P.owned.push(i.id);
    save();
    return got;
  }
  function save() { try { localStorage.setItem(KEY, JSON.stringify(P)); } catch (e) { /* private mode: keep it in memory */ } }

  function grantLevels() {
    const got = [];
    const L = levelInfo(P.xp).level;
    while (P.claimed < L) {
      P.claimed++;
      for (const r of PATH[P.claimed - 1]) {
        if (r.cr) P.credits += r.cr;
        if (r.at) P.tokens += r.at;
        if (r.item && !P.owned.includes(r.item)) P.owned.push(r.item);
        got.push({ level: P.claimed, ...r });
      }
    }
    save();
    return got;
  }

  // ================= Achievements =================
  // Each achievement is named after the title it gives you; some also give Credits or Arsenal Tokens.
  // [title id, what to do, goal, progress, extra reward, how to show the numbers]
  const rainbowItems = () => Object.values(ITEMS).filter((i) => i.coll === 'Rainbow' && i.src !== 'ach');
  const ACH_DEFS = [
    ['title.firstblood', 'Get your first kill in Battle Royale', 1, (s) => s.kills, { cr: 50 }],
    ['title.cratecracker', 'Break open 50 crates', 50, (s) => s.crates, { cr: 200 }],
    ['title.treasurehunter', 'Break open a Legendary Crate', 1, (s) => s.legend, { cr: 300 }],
    ['title.regular', 'Play 25 Battle Royale matches', 25, (s) => s.matches, { cr: 200 }],
    ['title.contender', 'Finish in the top 10 in 10 matches', 10, (s) => s.top10, { cr: 200 }],
    ['title.champion', 'Win a Battle Royale', 1, (s) => s.wins, { cr: 300 }],
    ['title.marathoner', 'Survive 8 minutes in one match', 480, (s) => s.longest, { cr: 150 }, 'time'],
    ['title.exterminator', 'Get 8 kills in one match', 8, (s) => s.bestKills, { cr: 250, at: 1 }],
    ['title.hoarder', 'Collect 500 upgrades', 500, (s) => s.upgrades, { cr: 250 }],
    ['title.powerhungry', 'Use 150 superpowers', 150, (s) => s.powers, { cr: 250 }],
    ['title.giantslayer', 'Beat 3 Arena Mages', 3, (s) => s.mages, { cr: 400, at: 1 }],
    ['title.guardianbane', 'Defeat 25 Temple Guardians', 25, (s) => s.mobs, { cr: 250 }],
    ['title.templeraider', 'Beat the Arena Mage at the heart of a temple', 1, (s) => s.mages, { cr: 500, at: 1 }],
    ['title.rocketeer', 'Win a Rocket Rampage', 1, (s) => s.rampWins, { cr: 300 }],
    ['title.demolition', 'Score 1,500 in one Rocket Rampage', 1500, (s) => s.rampBest, { cr: 200, at: 1 }],
    ['title.centurion', 'Get 100 kills in total', 100, (s) => s.kills, { cr: 500, at: 2 }],
    ['title.pathfinder', 'Reach Legendary Path level 30', 30, () => levelInfo(P.xp).level, { cr: 300 }],
    ['title.collector', 'Own 20 cosmetics', 20, () => P.owned.filter((id) => ITEMS[id] && ITEMS[id].src !== 'admin').length, null],
    ['title.bigspender', 'Spend 10,000 Credits in the Shop', 10000, (s) => s.spent, { at: 2 }],
    ['title.dynasty', 'Win 10 Battle Royales', 10, (s) => s.wins, { cr: 1000, at: 3 }],
    ['title.prismatic', 'Own every item in the Rainbow collection', 0, () => rainbowItems().filter((i) => P.owned.includes(i.id)).length, null],
  ];
  const ACHIEVEMENTS = ACH_DEFS.map(([id, text, goal, val, reward, fmt]) => ({ id, text, goal, val, reward, fmt }));
  for (const a of ACHIEVEMENTS) ITEMS[a.id].ach = a.text;
  const achGoal = (a) => a.goal || rainbowItems().length;
  // Progress of every achievement: { a, value, goal, done }
  function achievements() {
    return ACHIEVEMENTS.map((a) => {
      const goal = achGoal(a), done = P.achDone.includes(a.id);
      return { a, goal, done, value: done ? goal : Math.min(goal, a.val(P.stats) || 0) };
    });
  }
  // Unlocks every achievement that is now complete: gives the title and the extra reward. Returns the new ones.
  function checkAchievements() {
    const got = [];
    for (let again = true; again;) {  // one unlock can complete another (Collector, Prismatic)
      again = false;
      for (const a of ACHIEVEMENTS) {
        if (P.achDone.includes(a.id) || (a.val(P.stats) || 0) < achGoal(a)) continue;
        P.achDone.push(a.id);
        if (!P.owned.includes(a.id)) P.owned.push(a.id);
        if (a.reward && a.reward.cr) P.credits += a.reward.cr;
        if (a.reward && a.reward.at) P.tokens += a.reward.at;
        got.push(a.id);
        again = true;
      }
    }
    if (got.length) save();
    return got;
  }
  const achOf = (id) => ACHIEVEMENTS.find((a) => a.id === id);

  const today = () => Math.floor(Date.now() / 86400000);  // UTC day number
  const dayKey = () => String(today());

  // Match rewards (GAME_DESIGN.md section 10; stat upgrades take the place of shapes).
  // Rocket Rampage: score, kills and placement.
  function rampageRewards(r) {
    const place = r.place === 1 ? [300, 150] : r.place <= 3 ? [150, 75] : r.place <= 10 ? [50, 25] : [10, 5];
    const lines = [
      ['Score', String(r.score || 0), (r.score || 0) / 8, (r.score || 0) / 30],
      ['Kills', String(r.kills), 30 * r.kills, 10 * r.kills],
      ['Placement', '#' + r.place, place[0], place[1]],
    ];
    const out = lines.map(([t, d, x, c]) => ({ t, d, xp: Math.round(x), cr: Math.round(c) }));
    return { lines: out, xp: out.reduce((a, l) => a + l.xp, 0), credits: out.reduce((a, l) => a + l.cr, 0) };
  }
  function matchRewards(r) {
    if (r.mode === 'rampage') return rampageRewards(r);
    const min = r.secs / 60;
    const mul = r.kills >= 8 ? 1.3 : r.kills >= 5 ? 1.2 : r.kills >= 3 ? 1.1 : 1;
    const place = r.place === 1 ? [500, 250] : r.place <= 3 ? [250, 125] : r.place <= 10 ? [100, 50] : r.place <= 20 ? [40, 20] : [10, 5];
    const lines = [
      ['Time survived', Math.floor(min) + ':' + String(Math.floor(r.secs % 60)).padStart(2, '0'), 20 * min, 8 * min],
      ['Kills', r.kills + (mul > 1 ? ' (streak x' + mul.toFixed(1) + ')' : ''), 50 * r.kills * mul, 20 * r.kills * mul],
      ['Upgrades collected', String(r.upgrades), 4 * r.upgrades, 0],
      ['Power-ups used', String(r.powerUses), 10 * r.powerUses, 2 * r.powerUses],
      ['Placement', '#' + r.place, place[0], place[1]],
    ];
    if (r.mobs) lines.push(['Temple Guardians', r.mobs + ' x 30 XP', 30 * r.mobs, 0]);
    if (r.mages) lines.push(['Arena Mage', r.mages > 1 ? r.mages + ' beaten' : 'Beaten', 150 * r.mages, 75 * r.mages]);
    if (r.place === 1 && P.lastWinDay !== dayKey()) lines.push(['First win of the day', '', 200, 100]);
    const out = lines.map(([t, d, x, c]) => ({ t, d, xp: Math.round(x), cr: Math.round(c) }));
    return { lines: out, xp: out.reduce((a, l) => a + l.xp, 0), credits: out.reduce((a, l) => a + l.cr, 0) };
  }

  function applyMatch(r) {
    const rw = matchRewards(r);
    const before = levelInfo(P.xp);
    P.xp += rw.xp;
    P.credits += rw.credits;
    const s = P.stats;
    s.crates += r.crates || 0;
    s.legend += r.legend || 0;
    if (r.mode === 'rampage') {  // Rocket Rampage has its own stats; Battle Royale career stats stay Battle Royale only
      s.rampage++;
      if (r.place === 1) s.rampWins++;
      s.rampBest = Math.max(s.rampBest, r.score || 0);
      const granted = grantLevels();
      return { ...rw, before, after: levelInfo(P.xp), granted, ach: checkAchievements() };
    }
    s.mobs += r.mobs || 0;
    s.mages += r.mages || 0;
    s.matches++;
    s.kills += r.kills;
    s.bestKills = Math.max(s.bestKills, r.kills);
    s.longest = Math.max(s.longest, Math.floor(r.secs));
    s.upgrades += r.upgrades || 0;
    s.powers += r.powerUses || 0;
    s.guardians += r.guardians || 0;
    if (r.place <= 10) s.top10++;
    if (r.place === 1) { s.wins++; P.lastWinDay = dayKey(); }
    if (!s.bestPlace || r.place < s.bestPlace) s.bestPlace = r.place;
    const granted = grantLevels();
    const ach = checkAchievements();
    return { ...rw, before, after: levelInfo(P.xp), granted, ach };
  }

  // ================= Shop =================
  function rng(seed) { return () => { seed |= 0; seed = (seed + 0x6d2b79f5) | 0; let t = Math.imul(seed ^ (seed >>> 15), 1 | seed); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }
  const SLOT_MS = 3 * 3600 * 1000;  // the Featured tab refreshes every 3 hours (UTC-aligned)
  const slotNow = () => Math.floor(Date.now() / SLOT_MS);
  // Lower rarities show up more often than higher ones.
  const FEAT_WEIGHT = [0, 40, 30, 20, 10];
  // Deal discounts: 20% is the most common, lower discounts get slowly rarer, big ones get rare fast.
  const DISCOUNTS = [[5, 7], [10, 11], [15, 15], [20, 19], [25, 16], [30, 10], [35, 5], [40, 2]];
  function weighted(list, w, r) {
    const tot = list.reduce((a, x) => a + w(x), 0);
    let v = r() * tot;
    for (const x of list) { v -= w(x); if (v <= 0) return x; }
    return list[list.length - 1];
  }
  const rotCache = {};
  function rotation(slot = slotNow()) {
    if (rotCache[slot]) return rotCache[slot];
    const r = rng(slot * 7919 + 17);
    const pool = FEATURED_POOL.slice(), featured = [];
    while (featured.length < FEATURED_COUNT && pool.length) {
      const it = weighted(pool, (i) => FEAT_WEIGHT[i.rarity], r);
      featured.push(it); pool.splice(pool.indexOf(it), 1);
    }
    const sp = SHOP_ITEMS.slice(), deals = [];
    while (deals.length < 2 && sp.length) {
      const it = sp.splice(Math.floor(r() * sp.length), 1)[0];
      const pct = weighted(DISCOUNTS, (d) => d[1], r)[0];
      deals.push({ item: it, pct, price: Math.max(5, Math.round(it.price * (1 - pct / 100) / 5) * 5) });
    }
    return (rotCache[slot] = { slot, featured, deals, next: (slot + 1) * SLOT_MS });
  }
  // What an item costs right now (null if it can't be bought at the moment).
  function offerPrice(id) {
    const it = ITEMS[id];
    if (!it) return null;
    const rot = rotation();
    const deal = rot.deals.find((d) => d.item.id === id);
    if (deal) return deal.price;
    if (it.src === 'shop') return it.price;
    if (it.src === 'featured' && rot.featured.includes(it)) return FEATURED_PRICE[it.rarity];
    return null;
  }
  function buy(id) {
    const price = offerPrice(id);
    if (price == null || P.owned.includes(id) || P.credits < price) return null;
    P.credits -= price;
    P.stats.spent += price;
    P.owned.push(id);
    save();
    return { price, ach: checkAchievements() };
  }
  function equip(id) {
    const it = ITEMS[id];
    if (!it || !P.owned.includes(id) || !isVisible(it)) return false;
    P.equipped[it.cat] = id;
    save();
    return true;
  }
  // How to get an item you don't own yet (for the Locker).
  function howToGet(it) {
    if (it.src === 'path') return 'Legendary Path level ' + pathLevelOf[it.id];
    if (it.src === 'ach') return 'Achievement: ' + it.ach;
    if (it.src === 'shop') return 'Shop';
    if (it.src === 'featured') return 'Featured rotation (every 3 hours)';
    if (it.src === 'admin') return 'Admin exclusive';
    return '';
  }

  // ================= Looks =================
  const LOOK_CATS = ['wrap', 'barrel', 'trail', 'muzzle', 'killfx', 'deathfx'];
  function lookFrom(eq) {
    const w = eq.wrap === '_enemy' ? ENEMY_WRAP : ITEMS[eq.wrap] || ITEMS[DEFAULTS.wrap], b = ITEMS[eq.barrel] || ITEMS[DEFAULTS.barrel];
    return { pat: w.pat, c1: w.c1, c2: w.c2 || '#ffffff', bpat: b.pat, b1: b.c1, b2: b.c2 || '#ffffff',
             trail: (ITEMS[eq.trail] || {}).trail || 0, death: (ITEMS[eq.deathfx] || {}).death || 0,
             muzzle: (ITEMS[eq.muzzle] || {}).muzzle || 0, kill: (ITEMS[eq.killfx] || {}).kill || 0 };
  }
  const look = () => lookFrom(P.equipped);
  // Tanks with no cosmetics are coral with a slate barrel (kEnemyCol in client/main.cpp).
  const ENEMY_EQ = { ...DEFAULTS, wrap: '_enemy' };
  const ENEMY_WRAP = { id: '_enemy', name: 'Coral', cat: 'wrap', pat: 0, c1: '#ff7a5c' };

  // ================= 2D drawing (mirrors the shader in client/main.cpp) =================
  const hexRgb = (h) => [parseInt(h.slice(1, 3), 16), parseInt(h.slice(3, 5), 16), parseInt(h.slice(5, 7), 16)];
  const rgbStr = (c) => '#' + c.map((v) => Math.max(0, Math.min(255, Math.round(v))).toString(16).padStart(2, '0')).join('');
  const mixHex = (a, b, t) => { const x = hexRgb(a), y = hexRgb(b); return rgbStr(x.map((v, i) => v + (y[i] - v) * t)); };
  const shade = (h, k) => rgbStr(hexRgb(h).map((v) => v * k));
  const hue = (h, l = 50) => 'hsl(' + (((h % 1) + 1) % 1) * 360 + ',100%,' + l + '%)';
  const toHex = (c) => '#' + c.map((v) => Math.max(0, Math.min(255, Math.round(v * 255))).toString(16).padStart(2, '0')).join('');
  const hueHex = (h) => {  // same as hueC() in client/main.cpp
    h -= Math.floor(h);
    const f = (o) => { const k = (h * 6 + o) % 6; return Math.max(0, Math.min(1, Math.abs(k - 3) - 1)); };
    return toHex([f(0), f(4), f(2)]);
  };

  function edgeColor(pat, c1, c2, fill) {
    if ([1, 2, 3, 9, 10, 11].includes(pat)) return rgbStr(hexRgb(c1).map((v, i) => (v + hexRgb(c2)[i]) / 2 * 0.75));
    if (pat === 6 || pat === 13) return '#737373';
    if (pat === 12) return shade(c1, 0.6);
    return shade(fill || c1, 0.75);
  }

  // Fills the current clip with a pattern in unit space (the shape spans -1..1).
  function fillPattern(x, pat, c1, c2, t) {
    const S = 3;
    x.fillStyle = c1;
    switch (pat) {
      case 1: {  // diagonal stripes
        x.fillRect(-S, -S, 2 * S, 2 * S);
        x.fillStyle = c2;
        const w = 1 / 2.2;
        for (let m = -11; m <= 11; m++) {
          if (((m % 2) + 2) % 2 === 0) continue;
          const a = m * w, b = (m + 1) * w;
          x.beginPath(); x.moveTo(a + S, -S); x.lineTo(b + S, -S); x.lineTo(b - S, S); x.lineTo(a - S, S); x.closePath(); x.fill();
        }
        break;
      }
      case 2: {
        const w = 1 / 2.4;
        for (let i = -8; i < 8; i++) for (let j = -8; j < 8; j++) { x.fillStyle = ((i + j) & 1) ? c2 : c1; x.fillRect(i * w, j * w, w + 0.01, w + 0.01); }
        break;
      }
      case 3: {
        const n = 28, w = 2 / n, deep = rgbStr(hexRgb(c1).map((q, k) => (q + hexRgb(c2)[k]) / 2 * 0.85));
        for (let i = 0; i < n; i++) for (let j = 0; j < n; j++) {
          const ux = -1 + (i + 0.5) * w, uy = -1 + (j + 0.5) * w;
          const v = Math.sin(ux * 5.3 + 1.3) * Math.sin(uy * 4.1 - 0.7) + 0.6 * Math.sin((ux - uy) * 6.7 + 2.0);
          x.fillStyle = v > 0.35 ? c2 : v < -0.45 ? deep : c1;
          x.fillRect(-1 + i * w, -1 + j * w, w + 0.01, w + 0.01);
        }
        break;
      }
      case 4: { const g = x.createLinearGradient(0, -1, 0, 1); g.addColorStop(0, c1); g.addColorStop(1, c2); x.fillStyle = g; x.fillRect(-S, -S, 2 * S, 2 * S); break; }
      case 5: {
        const base = shade(c1, 0.8), g = x.createRadialGradient(-0.35, -0.45, 0, -0.35, -0.45, 1.111);
        g.addColorStop(0, mixHex(c1, '#ffffff', 0.85)); g.addColorStop(0.3, mixHex(c1, '#ffffff', 0.42));
        g.addColorStop(0.6, mixHex(c1, '#ffffff', 0.1)); g.addColorStop(1, base);
        x.fillStyle = base; x.fillRect(-S, -S, 2 * S, 2 * S); x.fillStyle = g; x.fillRect(-S, -S, 2 * S, 2 * S);
        break;
      }
      case 6: case 7: {
        const g = x.createConicGradient(pat === 6 ? t * 0.2 * Math.PI * 2 : -t * 1.5, 0, 0);
        if (pat === 6) for (let k = 0; k <= 6; k++) g.addColorStop(k / 6, 'hsl(' + (k * 60) + ',100%,59%)');
        else for (let k = 0; k <= 4; k++) g.addColorStop(k / 4, k % 2 ? c2 : c1);
        x.fillStyle = g; x.fillRect(-S, -S, 2 * S, 2 * S);
        break;
      }
      case 8: x.fillStyle = mixHex(c1, c2, 0.5 + 0.5 * Math.sin(t * 5)); x.fillRect(-S, -S, 2 * S, 2 * S); break;
      case 9: {
        x.fillRect(-S, -S, 2 * S, 2 * S);
        x.fillStyle = c2;
        for (let i = -4; i <= 4; i++) for (let j = -4; j <= 4; j++) { x.beginPath(); x.arc((i + 0.25) / 2, (j + 0.25) / 2, 0.11, 0, 7); x.fill(); }
        break;
      }
      case 10: {  // rings
        for (let k = 6; k >= 0; k--) { x.fillStyle = k % 2 ? c2 : c1; x.beginPath(); x.arc(0, 0, (k + 1) / 3.2, 0, 7); x.fill(); }
        break;
      }
      case 11: x.fillRect(-S, -S, 2 * S, S); x.fillStyle = c2; x.fillRect(-S, 0, 2 * S, S); break;  // split
      case 13: {  // rainbow stripes: the stripes of pattern 1, each one the next color
        const w = 1 / 2.2;
        for (let m = -11; m <= 11; m++) {
          const a = m * w, b = (m + 1) * w;
          x.fillStyle = mixHex(hueHex((((m % 6) + 6) % 6) / 6), '#ffffff', 0.12);
          x.beginPath(); x.moveTo(a + S, -S); x.lineTo(b + S + 0.01, -S); x.lineTo(b - S + 0.01, S); x.lineTo(a - S, S); x.closePath(); x.fill();
        }
        break;
      }
      case 12: {  // galaxy: darker toward the rim, twinkling stars
        const g = x.createRadialGradient(0, 0, 0, 0, 0, 1);
        g.addColorStop(0, c1); g.addColorStop(1, shade(c1, 0.55));
        x.fillStyle = g; x.fillRect(-S, -S, 2 * S, 2 * S);
        x.fillStyle = c2;
        for (let i = -4; i < 4; i++) for (let j = -4; j < 4; j++) {
          const h = Math.abs(Math.sin((i + 8) * 12.9898 + (j + 8) * 78.233) * 43758.5453) % 1;
          if (h <= 0.55) continue;
          const tw = 0.5 + 0.5 * Math.sin(t * 3 + h * 20);
          x.beginPath(); x.arc((i + 0.5) / 4, (j + 0.5) / 4, (0.06 + 0.1 * tw) / 4, 0, 7); x.fill();
        }
        break;
      }
      default: x.fillRect(-S, -S, 2 * S, 2 * S);
    }
  }

  // Shape path in local space. sides: -4 star, -3 ring, -2 capsule, -1 rect, 0 circle, >=3 polygon (vertex on +y).
  function shapePath(x, R, sides, aspect) {
    x.beginPath();
    const h = R * aspect;
    if (sides <= -3.5) {
      for (let k = 0; k < 10; k++) { const rr = k % 2 ? R * 0.45 : R, a = -Math.PI / 2 + k * Math.PI / 5; x.lineTo(Math.cos(a) * rr, Math.sin(a) * rr); }
    } else if (sides <= -2.5) { x.arc(0, 0, R + h, 0, Math.PI * 2); x.moveTo(Math.max(0, R - h), 0); x.arc(0, 0, Math.max(0, R - h), 0, Math.PI * 2, true); }
    else if (sides <= -1.5) x.roundRect(-R, -h, 2 * R, 2 * h, h);
    else if (sides <= -0.5) x.rect(-R, -h, 2 * R, 2 * h);
    else if (sides < 2.5) x.arc(0, 0, R, 0, Math.PI * 2);
    else for (let k = 0; k < sides; k++) { const a = k * Math.PI * 2 / sides; x.lineTo(Math.sin(a) * R, Math.cos(a) * R); }
    x.closePath();
  }

  // One primitive like the shader draws it: pattern fill, outline inside the edge (fill x 0.75), optional flash.
  function prim(x, px, py, R, rot, sides, o = {}) {
    const aspect = o.aspect ?? 1, pat = o.pat || 0, c1 = o.c || '#ffffff', c2 = o.c2 || '#ffffff', ol = o.ol ?? 3.5, t = o.t || 0;
    if (R <= 0.05) return;
    x.save();
    x.globalAlpha *= o.alpha ?? 1;
    x.translate(px, py); x.rotate(rot);
    shapePath(x, R, sides, aspect);
    const ring = sides <= -2.5 && sides > -3.5;
    if (!pat && !o.flash) {
      x.fillStyle = c1; x.fill(ring ? 'evenodd' : 'nonzero');
      if (ol > 0 && !ring) { x.save(); x.clip(); x.lineWidth = 2 * Math.min(ol, R * 0.5); x.strokeStyle = shade(c1, 0.75); x.stroke(); x.restore(); }
    } else {
      x.save(); x.clip(ring ? 'evenodd' : 'nonzero');
      const uy = sides <= -0.5 && sides > -2.5 ? R * aspect : R;
      x.save(); x.scale(R, uy); fillPattern(x, pat, c1, c2, t); x.restore();
      if (o.flash) { x.fillStyle = `rgba(255,255,255,${o.flash})`; x.fillRect(-R * 3, -R * 3, R * 6, R * 6); }
      if (ol > 0) { x.lineWidth = 2 * Math.min(ol, uy * 0.6); x.strokeStyle = edgeColor(pat, c1, c2); x.stroke(); }
      x.restore();
    }
    x.restore();
  }

  // A tank in world units (body radius 28, barrel 60 x 22), like drawTank() in client/main.cpp.
  function tankW(x, px, py, aim, lk, t, o = {}) {
    const kick = 3.5 * (o.recoil || 0), cx = Math.cos(aim), cy = Math.sin(aim), a = o.alpha ?? 1, s = o.scale || 1;
    prim(x, px + cx * (30 - kick) * s, py + cy * (30 - kick) * s, (30 - kick) * s, aim, -1, { aspect: 11 / (30 - kick), pat: lk.bpat, c: lk.b1, c2: lk.b2, t, alpha: a, flash: o.flash, ol: 3.5 * s });
    prim(x, px, py, 28 * s, aim, 0, { pat: lk.pat, c: lk.c1, c2: lk.c2, t, alpha: a, flash: o.flash, ol: 3.5 * s });
  }
  // Pixel-space wrapper for small icons (kill feed, cards).
  function drawTank(x, cx, cy, R, aim, lk, t, alpha = 1) {
    x.save(); x.translate(cx, cy); x.scale(R / 28, R / 28);
    tankW(x, 0, 0, aim, lk, t, { alpha });
    x.restore();
  }

  // ---- Name tags ----
  function tagFill(x, tag, cx, w, t) {
    if (tag.holo) {
      const g = x.createLinearGradient(cx - w / 2, 0, cx + w / 2, 0);
      for (let k = 0; k <= 6; k++) g.addColorStop(k / 6, hue(k / 6 * 0.8 + t * 0.25, 66));
      return g;
    }
    if (tag.grad) {
      const sh = tag.anim === 'flow' ? (t * 0.35) % 1 : 0;
      const g = x.createLinearGradient(cx - w / 2 - sh * w, 0, cx + w / 2 - sh * w + (tag.anim ? w : 0), 0);
      tag.grad.forEach((c, i) => g.addColorStop(i / (tag.grad.length - 1), c));
      return g;
    }
    return tag.color || '#ffffff';
  }
  const tagBase = (tag) => tag.holo ? '#9be7ff' : tag.grad ? tag.grad[0] : tag.color || '#ffffff';
  function titleFill(title, t) {
    if (title.anim === 'gold') return mixHex('#ffb31f', '#fff1b8', 0.5 + 0.5 * Math.sin(t * 3));
    if (title.anim === 'rainbow') return mixHex(hueHex(t * 0.25), '#ffffff', 0.15);
    return title.color;
  }
  // Draws "Title" (small, in the title's color) over the name (in the name tag color). fs = name font size.
  function drawLabel(x, cx, cy, name, tag, title, fs, t) {
    x.save();
    x.textAlign = 'center'; x.textBaseline = 'middle'; x.lineJoin = 'round';
    x.font = `600 ${fs}px Fredoka, "Trebuchet MS", sans-serif`;
    const w = x.measureText(name).width;
    x.lineWidth = fs * 0.26; x.strokeStyle = 'rgba(42,33,64,0.92)';
    x.strokeText(name, cx, cy);
    if (tag.glow) { x.shadowColor = tag.glow; x.shadowBlur = fs * 0.7; }
    x.fillStyle = tagFill(x, tag, cx, w, t);
    x.fillText(name, cx, cy);
    x.shadowBlur = 0;
    if (title && title.id !== 'title.none') {
      const f2 = fs * 0.62;
      x.font = `600 ${f2}px Fredoka, "Trebuchet MS", sans-serif`;
      const ty = cy - fs * 0.92;
      x.lineWidth = f2 * 0.3; x.strokeText(title.name, cx, ty);
      x.fillStyle = titleFill(title, t); x.fillText(title.name, cx, ty);
    }
    x.restore();
  }
  // CSS for a name in the page (HUD, death screen, kill feed).
  function tagCss(tag) {
    if (tag.holo) return 'background:linear-gradient(90deg,#ff8fc7,#ffd23f,#8ef0c0,#8fd3ff,#c3a6ff);-webkit-background-clip:text;background-clip:text;color:transparent;';
    if (tag.grad) return `background:linear-gradient(90deg,${tag.grad.join(',')});-webkit-background-clip:text;background-clip:text;color:transparent;` + (tag.glow ? `filter:drop-shadow(0 0 4px ${tag.glow});` : '');
    return `color:${tag.color};` + (tag.glow ? `text-shadow:0 0 8px ${tag.glow};` : '');
  }

  // ================= Particle effects (mirror client/main.cpp: same fields, same numbers) =================
  const frand = Math.random;
  const frange = (a, b) => a + (b - a) * frand();
  const frandDir = () => { const a = frange(0, Math.PI * 2); return [Math.cos(a), Math.sin(a)]; };
  const F = (o) => Object.assign({ x: 0, y: 0, vx: 0, vy: 0, ox: 0, oy: 0, age: 0, life: 0.25, r: 10, rot: 0, spin: 0, sides: 0, aspect: 1,
    a0: 1, drag: 0, ol: 0, grow: 0, grav: 0, shrink: false, c: '#ffffff', c2: '#ffffff', pat: 0 }, o);
  const GOLD = '#ffcc40', VIOLET = '#b061ff';

  function tankGhost(S, px, py, aim, lk) {
    const barrel = F({ x: px, y: py, ox: Math.cos(aim) * 30, oy: Math.sin(aim) * 30, r: 30, aspect: 11 / 30, sides: -1, rot: aim, ol: 3.5, grow: 0.5,
      life: 0.28, c: lk.b1, c2: lk.b2, pat: lk.bpat });
    S.hi.push(barrel, F({ ...barrel, ox: 0, oy: 0, r: 28, aspect: 1, sides: 0, c: lk.c1, c2: lk.c2, pat: lk.pat }));
  }
  function deathBurst(S, kind, px, py, lk) {
    const P = (o) => S.hi.push(F({ x: px, y: py, ...o }));
    switch (kind) {
      case 1: for (let i = 0; i < 30; i++) { const [dx, dy] = frandDir(), sp = frange(160, 520); P({ vx: dx * sp, vy: dy * sp, drag: 2.4, sides: -1, r: 5, aspect: 0.6, spin: frange(-12, 12), rot: frange(0, 6.28), life: frange(0.8, 1.3), c: hueHex(frand()), ol: 1.5 }); } break;
      case 2: for (let i = 0; i < 10; i++) { const [dx, dy] = frandDir(), [ex, ey] = frandDir(), sp = frange(120, 380); P({ x: px + ex * 10, y: py + ey * 10, vx: dx * sp, vy: dy * sp, drag: 2, sides: 3, r: frange(9, 14), spin: frange(-8, 8), rot: frange(0, 6.28), life: frange(0.6, 0.9), c: lk.c1, c2: lk.c2, pat: lk.pat, ol: 3 }); } break;
      case 3: for (let b = 0; b < 3; b++) {
        const [ex, ey] = frandDir(), d = frange(20, 70), col = mixHex(hueHex(frand()), '#ffffff', 0.2);
        for (let i = 0; i < 16; i++) { const a = i * Math.PI * 2 / 16, sp = frange(240, 320); P({ x: px + ex * d, y: py + ey * d, vx: Math.cos(a) * sp, vy: Math.sin(a) * sp, drag: 2.6, r: 3.5, life: frange(0.7, 1.0), c: col, shrink: true }); }
      } break;
      case 4: P({ r: 30, grow: 3, a0: 0.45, life: 0.5, c: '#ffcc40' });
        for (let i = 0; i < 22; i++) { const [dx, dy] = frandDir(), sp = frange(200, 460); P({ vx: dx * sp, vy: dy * sp, drag: 2.2, sides: 4, r: frange(5, 8), spin: frange(-9, 9), life: frange(0.7, 1.1), c: i & 1 ? GOLD : VIOLET, ol: 2, shrink: true }); }
        break;
      case 5: for (let i = 0; i < 14; i++) { const [dx, dy] = frandDir(), sp = frange(200, 420); P({ vx: dx * sp, vy: dy * sp, drag: 2, sides: -1, r: 11, aspect: 0.2, rot: Math.atan2(dy, dx), spin: frange(-3, 3), life: frange(0.9, 1.2), c: hueHex(frand()), ol: 1 }); } break;
      case 6: for (let i = 0; i < 20; i++) { const [dx, dy] = frandDir(), sp = frange(150, 400); P({ vx: dx * sp, vy: dy * sp, drag: 2.2, sides: -1, r: 5.5, life: frange(0.6, 0.9), c: i & 1 ? lk.c1 : lk.c2, ol: 2 }); } break;
      case 7:  // Over the Rainbow: six rings in rainbow order, then a ring of colored dots
        P({ r: 30, grow: 1.2, a0: 0.7, life: 0.3, c: '#ffffff' });
        for (let i = 0; i < 6; i++) P({ sides: -3, r: 22, aspect: 0.14, grow: 3.4, life: 0.65, age: -0.07 * i, c: hueHex(i / 6) });
        for (let i = 0; i < 24; i++) { const a = i * Math.PI * 2 / 24, sp = frange(260, 330); P({ vx: Math.cos(a) * sp, vy: Math.sin(a) * sp, drag: 2.4, r: 4, life: frange(0.8, 1.0), c: hueHex(i / 24), shrink: true }); }
        break;
      default: break;
    }
  }
  function muzzleFx(S, kind, tx, ty, aim) {
    const dx = Math.cos(aim), dy = Math.sin(aim);
    const P = (o) => S.hi.push(F({ x: tx, y: ty, ...o }));
    switch (kind) {
      case 1: for (let i = 0; i < 5; i++) { const a = aim + frange(-0.5, 0.5), sp = frange(300, 520); P({ vx: Math.cos(a) * sp, vy: Math.sin(a) * sp, drag: 6, r: 2.6, life: 0.18, c: '#ffe066', shrink: true }); } break;
      case 2: for (let i = 0; i < 2; i++) P({ x: tx + dx * 6, y: ty + dy * 6, sides: 4, r: 13, rot: aim + i * 0.785, life: 0.13, grow: 0.7, c: i ? '#fff299' : '#ffffff' }); break;
      case 3: for (let i = 0; i < 4; i++) { const a = aim + frange(-1, 1); P({ x: tx + Math.cos(a) * 10, y: ty + Math.sin(a) * 10, sides: -1, r: 9, aspect: 0.16, rot: a + frange(-0.6, 0.6), life: 0.1, c: '#8cf2ff' }); } break;
      case 4: P({ x: tx + dx * 4, y: ty + dy * 4, r: 13, grow: 1, a0: 0.8, life: 0.12, c: '#ff9933' }); break;
      case 5: P({ x: tx + dx * 4, y: ty + dy * 4, r: 10, grow: 1.6, a0: 0.6, life: 0.18, c: '#b366ff' }); break;
      case 6: for (let i = 0; i < 7; i++) { const a = aim + frange(-0.6, 0.6), sp = frange(260, 480); P({ vx: Math.cos(a) * sp, vy: Math.sin(a) * sp, drag: 4, grav: 260, sides: -1, r: 3.5, aspect: 0.55, spin: frange(-14, 14), life: 0.45, c: hueHex(frand()) }); } break;
      case 7: P({ x: tx + dx * 4, y: ty + dy * 4, sides: -3, r: 9, aspect: 0.2, grow: 1.8, life: 0.16, c: '#ffffff' }); break;
      case 8: for (let i = 0; i < 6; i++) { const a = aim + (i - 2.5) * 0.2, sp = 400; P({ vx: Math.cos(a) * sp, vy: Math.sin(a) * sp, drag: 6, r: 3.4, life: 0.24, c: mixHex(hueHex(i / 6), '#ffffff', 0.1), shrink: true }); } break;
      default: break;
    }
  }
  function killFx(S, kind, px, py, lk) {
    const P = (o) => S.hi.push(F({ x: px, y: py, ...o }));
    switch (kind) {
      case 1: for (let i = 0; i < 2; i++) P({ sides: -3, r: 30, aspect: 0.1, grow: 3.2, life: 0.5, age: -0.12 * i, c: i ? lk.c1 : '#ffffff', a0: 0.9 }); break;
      case 2: for (let i = 0; i < 10; i++) { const [dx, dy] = frandDir(), sp = frange(220, 420); P({ vx: dx * sp, vy: dy * sp, drag: 2.6, sides: -4, r: frange(9, 13), spin: frange(-7, 7), life: frange(0.6, 0.85), c: i & 1 ? '#ffd940' : '#ffffff', ol: 2, shrink: true }); } break;
      case 3: for (let i = 0; i < 14; i++) P({ vx: frange(-220, 220), vy: frange(-520, -260), grav: 1100, r: frange(7, 9), life: frange(0.8, 1.0), c: '#ffc938', ol: 2.5, spin: 4 }); break;
      case 4: {
        const body = { vy: -110, r: 24, a0: 0.75, life: 1.1, grow: 0.2, c: '#ffffff', ol: 2.5 };
        P(body);
        for (const e of [-1, 1]) P({ ...body, ox: e * 8, oy: -3, r: 3.6, ol: 0, c: '#2b2140', a0: 0.9, grow: 0 });
        break;
      }
      case 5:
        for (let i = 0; i < 3; i++) P({ x: px + frange(-14, 14), y: py - 90, sides: -1, r: 90, aspect: 0.05, rot: Math.PI / 2 + frange(-0.25, 0.25), life: 0.22, c: '#9eedff', age: -0.05 * i });
        P({ r: 40, grow: 1.2, a0: 0.6, life: 0.3, c: '#d9f7ff' });
        break;
      case 6:
        P({ r: 34, grow: 1.5, a0: 0.7, life: 0.35, c: '#ffffff' });
        for (let i = 0; i < 3; i++) P({ sides: -3, r: 26, aspect: 0.12, grow: 4, life: 0.6, age: -0.1 * i, c: hueHex(i / 3) });
        for (let i = 0; i < 20; i++) { const [dx, dy] = frandDir(), sp = frange(250, 520); P({ vx: dx * sp, vy: dy * sp, drag: 2.2, sides: -4, r: frange(6, 10), spin: frange(-8, 8), life: frange(0.7, 1.1), c: hueHex(frand()), ol: 2, shrink: true }); }
        break;
      case 7: for (let i = 0; i < 18; i++) {
        const a = i * Math.PI * 2 / 18, rx = Math.cos(a), ry = Math.sin(a);
        P({ x: px + rx * 95, y: py + ry * 95, vx: -ry * 330 - rx * 230, vy: rx * 330 - ry * 230, drag: 1.6, r: 5, life: 0.55, c: i & 1 ? '#7c54f2' : '#2ec4b5', shrink: true });
      } break;
      default: break;
    }
  }
  function trailFx(S, kind, px, py, bx, by, lk, clock) {
    const [ex, ey] = frandDir();
    const f = F({ x: px + bx * 26 + ex * 5, y: py + by * 26 + ey * 5 });
    const dir = () => frandDir();
    switch (kind) {
      case 1: Object.assign(f, { r: frange(7, 10), c: '#9e998f', a0: 0.45, life: 0.55, grow: 0.8 }); break;
      case 2: Object.assign(f, { sides: 4, r: frange(4, 6), c: frand() < 0.5 ? '#ffffff' : '#ffeb80', life: 0.45, spin: 6, shrink: true }); break;
      case 3: Object.assign(f, { r: 11, c: hueHex(clock * 0.8), a0: 0.6, life: 0.45, shrink: true }); break;
      case 4: { const [vx, vy] = dir(); Object.assign(f, { r: frange(4, 8), c: '#9edbff', a0: 0.55, life: 0.9, grow: 0.3, vx: vx * 20, vy: vy * 20, ol: 1.5 }); break; }
      case 5: { const [vx, vy] = dir(); Object.assign(f, { r: frange(5, 7), c: mixHex('#ff8c26', '#ff4026', frand()), a0: 0.85, life: 0.5, vx: vx * 40, vy: vy * 40, shrink: true }); break; }
      case 6: Object.assign(f, { sides: 4, r: 6, c: frand() < 0.5 ? GOLD : VIOLET, life: 0.6, shrink: true, spin: 5, ol: 2 }); break;
      case 7: Object.assign(f, { r: 13, c: '#ffffff', a0: 0.3, life: 0.7, grow: 1 }); break;
      case 8: Object.assign(f, { r: 9, c: '#b366ff', a0: 0.5, life: 0.4, shrink: true }); break;
      case 9: Object.assign(f, { r: 16, c: lk.c1, a0: 0.5, life: 0.3, shrink: true }); break;
      case 10: { const [vx, vy] = dir(); Object.assign(f, { sides: -1, r: 4, aspect: 0.55, c: hueHex(frand()), life: 0.6, spin: frange(-10, 10), rot: frange(0, 6.28), vx: vx * 30, vy: vy * 30, ol: 1 }); break; }
      case 11: { const [vx, vy] = dir(); Object.assign(f, { r: frange(3, 5), c: '#ffffff', life: 1.0, vx: vx * 18, vy: vy * 18, ol: 1.2 }); break; }
      default: return;
    }
    S.lo.push(f);
  }
  function stepFx(list, dt) {
    let n = 0;
    for (const f of list) {
      f.age += dt;
      if (f.age >= f.life) continue;
      if (f.age >= 0) {
        f.vy += f.grav * dt;
        f.x += f.vx * dt; f.y += f.vy * dt;
        if (f.drag > 0) { const k = Math.exp(-f.drag * dt); f.vx *= k; f.vy *= k; }
        f.rot += f.spin * dt;
      }
      list[n++] = f;
    }
    list.length = n;
  }
  function drawFx(x, list, t) {
    for (const f of list) {
      if (f.age < 0) continue;
      const k = f.age / f.life;
      let s = 1 + f.grow * k;
      if (f.shrink) s *= 1 - k;
      prim(x, f.x + f.ox * s, f.y + f.oy * s, f.r * s, f.rot, f.sides, { aspect: f.aspect, alpha: f.a0 * (1 - k), ol: f.ol, c: f.c, c2: f.c2, pat: f.pat, t });
    }
  }

  // ================= Preview scenes =================
  // A scene is a tiny piece of the arena: a grid floor, your tank wearing the item, and the item's real effect.
  function grid(x, cx, cy, vw, vh, px) {
    x.fillStyle = '#f3efe6'; x.fillRect(cx - vw, cy - vh, vw * 2, vh * 2);
    x.strokeStyle = 'rgba(91,74,46,0.075)'; x.lineWidth = Math.max(1 / px, 1.2 / px);
    x.beginPath();
    for (let gx = Math.floor((cx - vw) / 48) * 48; gx <= cx + vw; gx += 48) { x.moveTo(gx, cy - vh); x.lineTo(gx, cy + vh); }
    for (let gy = Math.floor((cy - vh) / 48) * 48; gy <= cy + vh; gy += 48) { x.moveTo(cx - vw, gy); x.lineTo(cx + vw, gy); }
    x.stroke();
  }
  function healthBar(x, px, py, frac) {
    prim(x, px, py + 48, 32, 0, -2, { aspect: 5 / 32, c: '#33294a', ol: 0 });
    if (frac > 0.01) prim(x, px - 32 + 2.5 + 29.5 * frac, py + 48, Math.max(3, 29.5 * frac), 0, -2, { aspect: 3 / Math.max(3, 29.5 * frac), c: frac > 0.5 ? '#73db66' : frac > 0.25 ? '#ffc740' : '#ff6154', ol: 0 });
  }

  // kind: 'wrap' | 'barrel' | 'trail' | 'muzzle' | 'killfx' | 'deathfx' | 'title' | 'tag' | 'loadout'
  function scene(item, opts = {}) {
    const eq = { ...P.equipped, ...(opts.eq || {}) };
    if (item && item.cat) eq[item.cat] = item.id;
    const lk = opts.look || lookFrom(eq), tag = ITEMS[eq.tag] || ITEMS['tag.white'], title = ITEMS[eq.title];
    const kind = opts.kind || (item ? item.cat : 'loadout');
    const name = opts.name || P.name || 'Tank';
    const S = { t: 0, lo: [], hi: [], bullets: [], view: 300, cx: 0, cy: 0 };
    const me = { x: 0, y: 0, aim: -0.4, recoil: 0, alpha: 1, trailT: 0, fire: 0.3, alive: true, dead: 0, flash: 0 };
    const foe = { x: 160, y: 0, hp: 1, alive: true, dead: 0, flash: 0, shown: 1, lk: lookFrom(ENEMY_EQ) };
    let labelSize = 19;
    if (kind === 'trail') S.view = 420;
    else if (kind === 'muzzle') { S.view = 520; me.x = -150; me.y = 10; }
    else if (kind === 'killfx') { S.view = 440; me.x = -130; me.y = 12; foe.x = 120; foe.y = 0; }
    else if (kind === 'deathfx') S.view = 380;
    else if (kind === 'title' || kind === 'tag') { S.view = 230; S.cy = -26; me.y = 30; labelSize = 27; }
    else S.view = opts.thumb ? 230 : 280;

    function fire(spd, range) {
      const dx = Math.cos(me.aim), dy = Math.sin(me.aim);
      me.recoil = 1;
      S.bullets.push({ x: me.x + dx * 58, y: me.y + dy * 58, vx: dx * spd, vy: dy * spd, left: range, r: 10 });
      if (lk.muzzle) muzzleFx(S, lk.muzzle, me.x + dx * 62, me.y + dy * 62, me.aim);
    }

    S.step = (dt) => {
      S.t += dt;
      const t = S.t;
      me.recoil = Math.max(0, me.recoil - dt * 9);
      me.flash = Math.max(0, me.flash - dt); foe.flash = Math.max(0, foe.flash - dt);
      if (kind === 'trail') {
        const w = 1.9, A = 135, B = 58;
        const nx = A * Math.sin(t * w), ny = B * Math.sin(2 * t * w);
        const vx = A * w * Math.cos(t * w), vy = 2 * B * w * Math.cos(2 * t * w), sp = Math.hypot(vx, vy);
        me.x = nx; me.y = ny + 10; me.aim = Math.atan2(vy, vx);
        me.trailT -= dt;
        if (lk.trail && sp > 60 && me.trailT <= 0) { me.trailT = 0.035; trailFx(S, lk.trail, me.x, me.y, -vx / sp, -vy / sp, lk, t); }
      } else if (kind === 'muzzle') {
        me.aim = -0.12 + Math.sin(t * 0.9) * 0.22;
        me.fire -= dt;
        if (me.fire <= 0) { me.fire = 0.5; fire(800, 470); }
      } else if (kind === 'killfx') {
        me.aim = Math.atan2(foe.y - me.y, foe.x - me.x);
        foe.y = Math.sin(t * 1.6) * 14;
        if (foe.alive) {
          me.fire -= dt;
          if (me.fire <= 0) { me.fire = 0.38; fire(950, 400); }
        } else if ((foe.dead += dt) > 1.6) { foe.alive = true; foe.hp = 1; foe.shown = 1; foe.dead = 0; foe.fade = 0; }
      } else if (kind === 'deathfx') {
        me.aim = -0.5 + Math.sin(t * 0.8) * 0.2;
        if (me.alive && (me.life = (me.life || 0) + dt) > 0.9) {
          me.alive = false; me.dead = 0;
          tankGhost(S, me.x, me.y, me.aim, lk);
          deathBurst(S, lk.death, me.x, me.y, lk);
        } else if (!me.alive && (me.dead += dt) > 1.7) { me.alive = true; me.life = 0; me.fade = 0; }
      } else {
        me.aim = -0.5 + Math.sin(t * 0.8) * (kind === 'title' || kind === 'tag' ? 0.2 : 0.35);
      }
      me.fade = Math.min(1, (me.fade ?? 1) + dt * 4);
      foe.fade = Math.min(1, (foe.fade ?? 1) + dt * 4);
      foe.shown += (foe.hp - foe.shown) * Math.min(1, dt * 10);
      // Bullets: fly, pop at the end of their range, or hit the target dummy.
      for (const b of S.bullets) {
        b.x += b.vx * dt; b.y += b.vy * dt; b.left -= Math.hypot(b.vx, b.vy) * dt;
        if (kind === 'killfx' && foe.alive && Math.hypot(b.x - foe.x, b.y - foe.y) < 38) {
          b.left = 0; foe.hp -= 0.34; foe.flash = 0.11;
          if (foe.hp <= 0.01) {
            foe.alive = false; foe.dead = 0;
            tankGhost(S, foe.x, foe.y, Math.PI, foe.lk);
            if (lk.kill) killFx(S, lk.kill, foe.x, foe.y, lk);
          }
        }
        if (b.left <= 0) S.hi.push(F({ x: b.x, y: b.y, r: b.r, grow: 0.6, life: 0.15, ol: 3.5, c: lk.c1 }));
      }
      S.bullets = S.bullets.filter((b) => b.left > 0);
      stepFx(S.lo, dt); stepFx(S.hi, dt);
    };

    S.draw = (cv) => {
      const x = cv.getContext('2d'), W = cv.width, H = cv.height, t = S.t;
      const px = W / S.view, vh = H / px / 2, vw = S.view / 2;
      x.setTransform(1, 0, 0, 1, 0, 0);
      x.clearRect(0, 0, W, H);
      x.setTransform(px, 0, 0, px, W / 2 - S.cx * px, H / 2 - S.cy * px);
      if (!opts.noGrid) grid(x, S.cx, S.cy, vw + 50, vh + 50, px);
      drawFx(x, S.lo, t);
      for (const b of S.bullets) prim(x, b.x, b.y, b.r, 0, 0, { c: lk.c1 });
      if (kind === 'killfx' && foe.alive) {
        tankW(x, foe.x, foe.y, Math.PI, foe.lk, t, { alpha: foe.fade, flash: foe.flash > 0 ? 0.45 : 0 });
        if (foe.hp < 0.99) healthBar(x, foe.x, foe.y, Math.max(0, foe.shown));
      }
      if (me.alive) tankW(x, me.x, me.y, me.aim, lk, t, { recoil: me.recoil, alpha: me.fade });
      drawFx(x, S.hi, t);
      if (me.alive && !opts.noName) drawLabel(x, me.x, me.y - (kind === 'title' || kind === 'tag' ? 56 : 52), name, tag, title, labelSize, t);
      x.setTransform(1, 0, 0, 1, 0, 0);
    };
    return S;
  }
  // Credits: a gold coin. Arsenal Tokens: a teal gem.
  function drawCurrency(x, kind, cx, cy, R) {
    x.save(); x.translate(cx, cy);
    if (kind === 'cr') {
      x.fillStyle = '#ffc63d'; x.strokeStyle = '#d68f14'; x.lineWidth = R * 0.16;
      x.beginPath(); x.arc(0, 0, R, 0, 7); x.fill(); x.stroke();
      x.strokeStyle = 'rgba(214,143,20,0.7)'; x.lineWidth = R * 0.1; x.beginPath(); x.arc(0, 0, R * 0.62, 0, 7); x.stroke();
      x.fillStyle = 'rgba(255,255,255,0.55)'; x.beginPath(); x.ellipse(-R * 0.35, -R * 0.4, R * 0.22, R * 0.12, -0.6, 0, 7); x.fill();
    } else {
      x.fillStyle = '#4fd8c8'; x.strokeStyle = '#1f9488'; x.lineWidth = R * 0.14; x.lineJoin = 'round';
      x.beginPath(); x.moveTo(0, -R); x.lineTo(R * 0.85, -R * 0.2); x.lineTo(0, R); x.lineTo(-R * 0.85, -R * 0.2); x.closePath(); x.fill(); x.stroke();
      x.fillStyle = 'rgba(255,255,255,0.45)'; x.beginPath(); x.moveTo(0, -R); x.lineTo(R * 0.85, -R * 0.2); x.lineTo(0, -R * 0.05); x.lineTo(-R * 0.85, -R * 0.2); x.closePath(); x.fill();
    }
    x.restore();
  }
  const animated = (it) => !!it && ([6, 7, 8, 12].includes(it.pat) || ['trail', 'muzzle', 'killfx', 'deathfx'].includes(it.cat) || it.holo || it.anim);

  load();
  return {
    RARITY, RCOL, CATS, CAT, SRC, ITEMS, DEFAULTS, SHOP_ITEMS, FEATURED_POOL, FEATURED_PRICE, PATH, MAX_LEVEL, pathLevelOf, ENEMY_EQ,
    get p() { return P; }, save, load, reset() { try { localStorage.removeItem(KEY); localStorage.removeItem(OLD_KEY); } catch (e) {} load(); },
    levelInfo, xpToNext, grantLevels, matchRewards, applyMatch, checkAchievements, achievements, achOf, rotation, offerPrice, buy, equip, howToGet,
    COLLECTIONS, collItems, ADMIN_ITEMS, isVisible, setAdmin, grantAdmin, get admin() { return admin; },
    look, lookFrom, typeName, rewardInfo, scene, drawTank, drawLabel, tagCss, tagBase, titleFill, drawCurrency, animated, dayKey, slotNow,
    hueHex, shade,
  };
})();
