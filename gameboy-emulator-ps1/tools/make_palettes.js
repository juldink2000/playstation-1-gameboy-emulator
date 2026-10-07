// Maakt src/gb_palettes_extra.h uit het npm-pakket "gb-palettes"
// (de lijst met ~60 paletten van HerrZatacke).
//
// Gebruik, in de map gb-ps1-test-2 (Node.js nodig):
//   npm install gb-palettes
//   node tools/make_palettes.js
//
// Daarna cmake --build build. Het bestand wordt automatisch meegenomen.

const fs = require('fs');
const pkg = require('gb-palettes');

let list = Array.isArray(pkg) ? pkg : Object.values(pkg);
list = list.filter((p) => p && Array.isArray(p.palette) && p.palette.length === 4);

if (!list.length) {
  console.error('Geen paletten gevonden in gb-palettes (andere indeling?).');
  process.exit(1);
}

// De debugfont kent alleen hoofdletters/cijfers: rest eruit, max 18 tekens.
const clean = (s) => String(s).toUpperCase().replace(/[^A-Z0-9 ]/g, '').trim().slice(0, 18);

// Standaard-ingebouwde namen niet nogmaals toevoegen.
const skip = new Set(['bw']);

let out = '/* Gegenereerd door tools/make_palettes.js - niet met de hand aanpassen. */\n';
for (const p of list) {
  if (skip.has(p.shortName)) continue;
  const c = p.palette.map((h) => '0x' + String(h).replace('#', '').toUpperCase());
  out += `\t{ "${clean(p.name || p.shortName)}", { HEXC(${c[0]}), HEXC(${c[1]}), HEXC(${c[2]}), HEXC(${c[3]}) } },\n`;
}

fs.writeFileSync('src/gb_palettes_extra.h', out);
console.log(`${list.length} paletten geschreven naar src/gb_palettes_extra.h`);
