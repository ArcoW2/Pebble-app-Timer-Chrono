// config.js - the phone config page (Clay).
//
// Layouts, colours and the reveal timeout live here because they are
// changed rarely and are painful to edit on a 200x228 screen. Everything
// that matters while the timer runs stays on the watch.
var MODES = [
  { key: 'U', title: 'Count up', lines: 3 },
  { key: 'F', title: 'Count down for', lines: 4 },
  { key: 'T', title: 'Count down until', lines: 4 }
];

var SOURCES = [
  { value: '0', label: 'None' },
  { value: '1', label: 'Total time' },
  { value: '2', label: 'Remaining (count down)' },
  { value: '3', label: 'Current lap' },
  { value: '4', label: 'Prior lap' },
  { value: '5', label: 'Lap before prior' },
  { value: '6', label: 'Fastest lap' },
  { value: '7', label: 'Average lap' },
  { value: '8', label: 'Current lap vs prior' },
  { value: '9', label: 'Current lap vs fastest' },
  { value: '10', label: 'Current lap vs average' },
  { value: '11', label: 'End time (until)' },
  { value: '12', label: 'Lap count' }
];

var SIZES = [
  { value: '3', label: 'XL' },
  { value: '0', label: 'L' },
  { value: '1', label: 'M' },
  { value: '2', label: 'S' }
];

var FORMATS = [
  { value: '0', label: 'Auto' },
  { value: '1', label: 'd h:mm:ss' },
  { value: '2', label: 'h:mm:ss' },
  { value: '3', label: 'mm:ss' },
  { value: '4', label: 'mm:ss.t' }
];

var COLORS = [
  { value: '0', label: 'Black' },
  { value: '1', label: 'White' },
  { value: '2', label: 'Oxford blue' },
  { value: '3', label: 'Dark gray' },
  { value: '4', label: 'Dark green' },
  { value: '5', label: 'Imperial purple' },
  { value: '6', label: 'Yellow' },
  { value: '7', label: 'Electric blue' },
  { value: '8', label: 'Light gray' },
  { value: '9', label: 'Chrome yellow' },
  { value: '10', label: 'Orange' },
  { value: '11', label: 'Vivid cerulean' },
  { value: '12', label: 'Shocking pink' },
  { value: '13', label: 'Red' },
  { value: '14', label: 'Green' },
  { value: '15', label: 'Dark red' },
  { value: '16', label: 'Islamic green' },
  { value: '17', label: 'Folly' },
  { value: '18', label: 'Bulgarian rose' }
];

// Defaults mirror settings.c, so the page shows what the watch does.
var DEFAULTS = {
  U: { fg: '1', bg: '0', ac: '10', behind: '13', ahead: '14',
       lines: [['1', '0', '0', true], ['3', '1', '3', true], ['9', '2', '0', false]] },
  F: { fg: '1', bg: '2', ac: '7', behind: '13', ahead: '14',
       lines: [['2', '0', '0', true], ['3', '2', '3', true], ['1', '2', '0', true]] },
  T: { fg: '9', bg: '0', ac: '11', behind: '13', ahead: '14',
       lines: [['2', '0', '0', true], ['3', '2', '3', true], ['11', '2', '0', false]] }
};

function select(messageKey, label, options, defaultValue) {
  return {
    type: 'select',
    messageKey: messageKey,
    label: label,
    defaultValue: defaultValue,
    options: options
  };
}

function lineItems(mode, index) {
  var prefix = mode.key + '_L' + (index + 1) + '_';
  var def = DEFAULTS[mode.key].lines[index] || ['0', '1', '0', false];
  return [
    { type: 'heading', defaultValue: 'Line ' + (index + 1), size: 6 },
    select(prefix + 'SRC', 'Shows', SOURCES, def[0]),
    select(prefix + 'SIZE', 'Size', SIZES, def[1]),
    select(prefix + 'FMT', 'Format', FORMATS, def[2]),
    {
      type: 'toggle',
      messageKey: prefix + 'THR',
      label: 'Adaptive resolution',
      description: 'Coarser while there is plenty of time left.',
      defaultValue: def[3]
    }
  ];
}

function modeSection(mode) {
  var def = DEFAULTS[mode.key];
  var items = [
    { type: 'heading', defaultValue: mode.title },
    select(mode.key + '_BG', 'Background', COLORS, def.bg),
    select(mode.key + '_FG', 'Digits', COLORS, def.fg),
    select(mode.key + '_AC', 'Accent', COLORS, def.ac),
    select(mode.key + '_BEHIND', 'Behind / overrun', COLORS, def.behind),
    select(mode.key + '_AHEAD', 'Ahead', COLORS, def.ahead)
  ];
  for (var i = 0; i < mode.lines; i++) {
    items = items.concat(lineItems(mode, i));
  }
  return { type: 'section', items: items };
}

// status: what the watch said about the last config it received.
module.exports = function buildConfig(status) {
  var general = [
    { type: 'heading', defaultValue: 'Timer / Chrono' },
    {
      type: 'text',
      defaultValue: status ? 'Watch: ' + status
                           : 'Lines are ordered top to bottom. Lines set to None are ' +
                             'left out. The watch checks that a layout fits and keeps ' +
                             'the previous one if it does not.'
    },
    {
      type: 'slider',
      messageKey: 'REVEAL',
      label: 'Tap reveal (seconds)',
      description: 'How long a tap shows full precision while running.',
      defaultValue: 5,
      min: 1,
      max: 30,
      step: 1
    },
    {
      type: 'slider',
      messageKey: 'GHOST',
      label: 'Ghost level',
      description: '0 = off. Higher draws unlit segments thicker, for more ' +
                   'of an LCD look. Too high and the lit digits lose contrast.',
      defaultValue: 1,
      min: 0,
      max: 3,
      step: 1
    }
  ];

  var config = [{ type: 'section', items: general }];
  for (var i = 0; i < MODES.length; i++) {
    config.push(modeSection(MODES[i]));
  }
  config.push({ type: 'submit', defaultValue: 'Save to watch' });
  return config;
};

module.exports.MODES = MODES;
module.exports.DEFAULTS = DEFAULTS;
