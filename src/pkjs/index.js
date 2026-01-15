var Clay = require('./pebble-clay');

function isColorWatch() {
  if (Pebble.getActiveWatchInfo) {
    var info = Pebble.getActiveWatchInfo();
    if (info && info.platform) {
      return ['basalt', 'chalk', 'emery', 'flint'].indexOf(info.platform) !== -1;
    }
  }
  return true;
}

var themeOptions = isColorWatch() ? [
  { value: 0, label: 'Dark' },
  { value: 1, label: 'Light' },
  { value: 2, label: 'Warm' },
  { value: 3, label: 'Natural' },
  { value: 4, label: 'Cool' },
  { value: 5, label: 'Dusk' }
] : [
  { value: 0, label: 'Dark' },
  { value: 1, label: 'Light' }
];

var clayConfig = [
  {
    type: 'heading',
    defaultValue: 'Flow'
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Theme'
      },
      {
        type: 'select',
        messageKey: 'theme',
        label: 'Theme',
        defaultValue: 0,
        options: themeOptions
      }
    ]
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Time'
      },
      {
        type: 'select',
        messageKey: 'time_format',
        label: 'Time format',
        defaultValue: 0,
        options: [
          { value: 0, label: '24-hour' },
          { value: 1, label: '12-hour' }
        ]
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Save'
  }
];

var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

var pendingConfig = false;
var pendingTimer = null;

function openConfig() {
  Pebble.openURL(clay.generateUrl());
}

function requestThemeAndOpen() {
  pendingConfig = true;
  if (pendingTimer) {
    clearTimeout(pendingTimer);
  }
  pendingTimer = setTimeout(function() {
    pendingConfig = false;
    openConfig();
  }, 800);

  Pebble.sendAppMessage({ theme_request: 1, time_format_request: 1 }, function() {}, function() {});
}

Pebble.addEventListener('showConfiguration', function() {
  requestThemeAndOpen();
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && e.response) {
    var settings = clay.getSettings(e.response);
    Pebble.sendAppMessage(settings, function() {}, function() {});
  }
});

Pebble.addEventListener('appmessage', function(e) {
  if (!e || !e.payload) {
    return;
  }
  var nextSettings = {};
  if (typeof e.payload.theme !== 'undefined') {
    nextSettings.theme = e.payload.theme;
  }
  if (typeof e.payload.time_format !== 'undefined') {
    nextSettings.time_format = e.payload.time_format;
  }
  if (Object.keys(nextSettings).length > 0) {
    clay.setSettings(nextSettings);
  }
  if (pendingConfig) {
    pendingConfig = false;
    if (pendingTimer) {
      clearTimeout(pendingTimer);
      pendingTimer = null;
    }
    openConfig();
  }
});
