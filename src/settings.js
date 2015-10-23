Pebble.addEventListener('ready',
  function(e) {
    console.log('ready received!!');
  });
Pebble.addEventListener('showConfiguration',
  function(e) {
    console.log('showConfiguration received.');
    var platform = Pebble.getActiveWatchInfo().platform;
    if (platform == "basalt" || platform == "chalk") {
      Pebble.openURL('http://sakira.jp/pebble/quarter_analogue_settings_color.html');
    } else {
      Pebble.openURL('http://sakira.jp/pebble/quarter_analogue_settings_bw.html');
    }
  });
Pebble.addEventListener('webviewclosed',
  function(e) {
    console.log('webviewclosed received. :' + decodeURIComponent(e.response));

    var json = JSON.parse(decodeURIComponent(e.response));
    var dict = {
      'Key_BackgroundColor' : parseInt(json.bg_color, 2),
      'Key_HandColor' : parseInt(json.hand_color, 2),
      'Key_HourColor' : parseInt(json.hour_color, 2),
      'Key_DivisionColor' : parseInt(json.division_color, 2),
      'Key_Interval' : (json.interval === "second") ? 1 : 0,
      'Key_HourDigit' : (json.hour_digit === "show") ? 1 : 0,
    };
    console.log('dict:' + JSON.stringify(dict));
    Pebble.sendAppMessage(dict,
        function(e) {
          console.log('Send Settings to Pebble');
        },
        function(e) {
          console.log('Error sending settings');
        }
    );
  });