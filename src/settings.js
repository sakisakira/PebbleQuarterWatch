Pebble.addEventListener('ready',
  function(e) {
    console.log('ready received!!');
  });
Pebble.addEventListener('showConfiguration',
  function(e) {
    console.log('showConfiguration received.');
    Pebble.openURL('http://sakira.jp/pebble/quarter_analogue_settings.html');
  });
Pebble.addEventListener('webviewclosed',
  function(e) {
    console.log('webviewclosed received. :' + e.response);
    var json = JSON.parse(e.response);
    var dict = {
      'Key_BackgroundColor' : (json.bg_color === "white") ? 1 : 0,
      'Key_Interval' : (json.interval === "second") ? 1 : 0,
      'Key_HourDigit' : (json.hour_digit === "show") ? 1 : 0,
    };
    Pebble.sendAppMessage(dict,
        function(e) {
          console.log('Send Settings to Pebble');
        },
        function(e) {
          console.log('Error sending settings');
        }
    );
  });