Pebble.addEventListener("ready", function(e) {
	console.log("On11 watchface is ready! " + e.ready);
});

Pebble.addEventListener("showConfiguration", function() {
	console.log("Open configuration page!");
	Pebble.openURL('http://on11.mobi/configure-time.html');
});

Pebble.addEventListener("webviewclosed", function(e) {
	console.log("Configuration page has been closed!");
	var options = JSON.parse(decodeURIComponent(e.response));
	if (JSON.stringify(options) != "{}") {
		Pebble.sendAppMessage(options);
	}
});
