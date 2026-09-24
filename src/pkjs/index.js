// index.js - phone side.
//
// The config page sends nothing while it is open: settings go to the watch
// once, when the page closes. There is no preview here on purpose, because
// the watch renderer is the only thing that knows how a layout really
// fits. The watch validates what it receives and reports back.
var Clay = require('pebble-clay');
var buildConfig = require('./config');

var MODE_KEYS = ['U', 'F', 'T'];
var LINES_PER_MODE = 4;
var CFG_VERSION = 2;
var STATUS_KEY = 'cfg_status';
var BLOB_KEY = 'cfg_blob';

var clay = null;

// ==== HELPERS ====

function readStatus() {
  try {
    return localStorage.getItem(STATUS_KEY) || '';
  } catch (e) {
    return '';
  }
}

function writeStatus(text) {
  try {
    localStorage.setItem(STATUS_KEY, text);
  } catch (e) {
    // no storage: the page simply shows no status
  }
}

// Clay hands back either the raw value or { value: ... }.
function valueOf(settings, key, fallback) {
  var v = settings[key];
  if (v && typeof v === 'object' && 'value' in v) v = v.value;
  if (v === undefined || v === null || v === '') return fallback;
  return v;
}

function intOf(settings, key, fallback) {
  var n = parseInt(valueOf(settings, key, fallback), 10);
  return isNaN(n) ? fallback : n;
}

function boolOf(settings, key, fallback) {
  var v = valueOf(settings, key, fallback);
  return v === true || v === 'true' || v === 1 || v === '1';
}

// ==== ENCODING ====
// [version, reveal, style, per mode: fg, bg, accent, nlines,
//  4 x (source, size, format, flags)]

function encode(settings) {
  var ghost = intOf(settings, 'GHOST', 6);
  if (ghost < 0) ghost = 0;
  if (ghost > 10) ghost = 10;
  var bytes = [CFG_VERSION, intOf(settings, 'REVEAL', 5), ghost];
  for (var m = 0; m < MODE_KEYS.length; m++) {
    var k = MODE_KEYS[m];
    var def = buildConfig.DEFAULTS[k];
    bytes.push(intOf(settings, k + '_FG', parseInt(def.fg, 10)));
    bytes.push(intOf(settings, k + '_BG', parseInt(def.bg, 10)));
    bytes.push(intOf(settings, k + '_AC', parseInt(def.ac, 10)));
    bytes.push(intOf(settings, k + '_BEHIND', parseInt(def.behind, 10)));
    bytes.push(intOf(settings, k + '_AHEAD', parseInt(def.ahead, 10)));
    bytes.push(0);  // line count: the watch compacts the list itself
    for (var i = 0; i < LINES_PER_MODE; i++) {
      var p = k + '_L' + (i + 1) + '_';
      bytes.push(intOf(settings, p + 'SRC', 0));
      bytes.push(intOf(settings, p + 'SIZE', 1));
      bytes.push(intOf(settings, p + 'FMT', 0));
      bytes.push(boolOf(settings, p + 'THR', false) ? 1 : 0);
    }
  }
  return bytes;
}

// ==== PEBBLE EVENTS ====

function sendBlob(bytes, onFail) {
  Pebble.sendAppMessage({ CFG: bytes }, function () {
    console.log('settings sent');
  }, function (err) {
    console.log('settings failed: ' + JSON.stringify(err));
    if (onFail) onFail();
  });
}

function storeBlob(bytes) {
  try {
    localStorage.setItem(BLOB_KEY, JSON.stringify(bytes));
  } catch (e) {
    // no storage: the watch keeps whatever it already has
  }
}

function storedBlob() {
  try {
    var raw = localStorage.getItem(BLOB_KEY);
    return raw ? JSON.parse(raw) : null;
  } catch (e) {
    return null;
  }
}

Pebble.addEventListener('showConfiguration', function () {
  clay = new Clay(buildConfig(readStatus()), null, { autoHandleEvents: false });
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response || !clay) return;
  var settings = clay.getSettings(e.response, false);
  var bytes = encode(settings);
  storeBlob(bytes);   // the watch asks for this at its next launch
  sendBlob(bytes, function () {
    writeStatus('saved on the phone, applied at the next app launch');
  });
});

Pebble.addEventListener('appmessage', function (e) {
  if (!e || !e.payload) return;
  if (e.payload.CFG_STATUS) writeStatus(e.payload.CFG_STATUS);
  if (e.payload.CFG_REQ !== undefined) {
    var bytes = storedBlob();
    if (bytes) sendBlob(bytes, null);   // app launched: hand it the settings
  }
});

Pebble.addEventListener('ready', function () {
  console.log('timer/chrono pkjs ready');
});
