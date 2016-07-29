var mPosition = null;

function calculateDistance(lat1, lon1, lat2, lon2) {
	var R = 6371000; // meter
	var dLat = (lat2 - lat1) * Math.PI / 180;
	var dLon = (lon2 - lon1) * Math.PI / 180;
	var a = Math.sin(dLat / 2) * Math.sin(dLat / 2) +
			Math.cos(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180) *
			Math.sin(dLon / 2) * Math.sin(dLon / 2);
	var c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
	var d = R * c;
	return d;
}

Pebble.addEventListener("ready", function(e) {
	console.log("On11 watchface is ready! " + e.ready);
});

Pebble.addEventListener("showConfiguration", function() {
	console.log("Open configuration page!");
	Pebble.openURL('http://on11.mobi/configure.html');
});

Pebble.addEventListener("webviewclosed", function(e) {
	console.log("Configuration page has been closed!");
	var options = JSON.parse(decodeURIComponent(e.response));
	if (JSON.stringify(options) != "{}") {
		Pebble.sendAppMessage(options);
	}
});

Pebble.addEventListener("appmessage", function(e) {
	console.log("Received message: " + e.payload);

	var options = {
		enableHighAccuracy: true,
		timeout: 10000,
		maximumAge: 0
	};

	navigator.geolocation.getCurrentPosition(
		function(position) {
			var speed = 0;

			if (mPosition != null) {
				var distance = calculateDistance(mPosition.coords.latitude, mPosition.coords.longitude,
												position.coords.latitude, position.coords.longitude);
				speed = distance / (position.timestamp - mPosition.timestamp) * 1000;	// m/s
				console.log("(" + mPosition.coords.latitude + ", " + mPosition.coords.longitude + ") to (" + position.coords.latitude + ", " + position.coords.longitude + ")");
				console.log("Distance: " + distance);
				console.log("Current speed: " + speed);
			}

			Pebble.sendAppMessage(
				{ "speed": Math.round(speed * 100) },
				function(e) {
					console.log("Successfully delivered message with transactionId=" + e.data.transactionId);
				},
				function(e) {
					console.log("Unable to deliver message with transactionId=" + e.data.transactionId);
				}
			);

			// Update
			mPosition = position;
		},
		function(error) {
			console.log("Location ERROR: " + error.message);
		},
		options
	);
});
