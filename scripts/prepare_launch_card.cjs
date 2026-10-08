// Encode the generated master as Vita-compatible indexed PNG with transparency.
// Requires Node.js and sharp. No artwork is generated or downloaded here.
const sharp = require('sharp');
const path = require('node:path');
const root = path.resolve(__dirname, '..');
const input = path.join(root, 'assets/source/new-mission.png');
const output = path.join(root, 'assets/sce_sys/livearea/contents/startup-r3.png');

sharp(input)
  .resize(425, 344, {fit: 'contain', background: {r: 0, g: 0, b: 0, alpha: 0}})
  .png({palette: true, colours: 256, dither: 0.6, effort: 10, compressionLevel: 9})
  .toFile(output)
  .then(info => console.log(JSON.stringify({output, ...info})))
  .catch(error => {console.error(error); process.exitCode = 1;});
