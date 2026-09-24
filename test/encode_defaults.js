// Prints the default config blob the way src/pkjs/index.js encodes it, so
// the C side can be checked against the real phone format.
var buildConfig = require('../src/pkjs/config');
var MODE_KEYS = ['U', 'F', 'T'];
var bytes = [2, 5, 1];   // version, reveal seconds, style flags
MODE_KEYS.forEach(function (k) {
  var def = buildConfig.DEFAULTS[k];
  bytes.push(parseInt(def.fg, 10), parseInt(def.bg, 10), parseInt(def.ac, 10),
             parseInt(def.behind, 10), parseInt(def.ahead, 10), 0);
  for (var i = 0; i < 4; i++) {
    var l = def.lines[i] || ['0', '1', '0', false];
    bytes.push(parseInt(l[0], 10), parseInt(l[1], 10), parseInt(l[2], 10), l[3] ? 1 : 0);
  }
});
console.log(bytes.join(' '));
