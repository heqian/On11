#include <pebble.h>
#include "utility.h"

// AppMessage
enum {
	RESET_TIME_KEY = 0x0,
	PEDOMETER_SENSITIVITY_KEY = 0x1,
	STEP_GOAL_KEY = 0x2,
	ACTIVE_TIME_GOAL_KEY = 0x3
};

// Config
static int32_t mResetTime = 0;	// Minutes since 00:00 e.g. 09:30 AM == 570
static int32_t mPedometerSensitivity = 15;	// Range: [0, 100]
static int32_t mStepGoal = 10000;
static int32_t mActiveTimeGoal = 60;	// Minutes

// UI
static Window* mWindow = NULL;
static Layer* mWatchfaceLayer = NULL;

// Activity
static Counter mCounter;
static uint32_t mCurrentType = 0;

// Status
static bool mIsBluetoothConnected = true;
static BatteryChargeState mBatteryChargeState;


static void loadConfig() {
	mResetTime = persist_exists(6) ? persist_read_int(6) : 0;
	mPedometerSensitivity = persist_exists(7) ? persist_read_int(7) : 15;
	mStepGoal = persist_exists(8) ? persist_read_int(8) : 10000;
	mActiveTimeGoal = persist_exists(9) ? persist_read_int(9) : 60;
}


// Save persistent values
static void saveConfig() {
	persist_write_int(6, mResetTime);
	persist_write_int(7, mPedometerSensitivity);
	persist_write_int(8, mStepGoal);
	persist_write_int(9, mActiveTimeGoal);
}


static void updateBattery(BatteryChargeState batteryChargeState) {
	mBatteryChargeState = batteryChargeState;
	layer_mark_dirty(mWatchfaceLayer);
}


// Handle tap event
static void processTapEvent(AccelAxisType axis, int32_t direction) {
}


// Handle Bluetooth connection event
static void processBluetoothConnectionEvent(bool connected) {
	mIsBluetoothConnected = connected;
	layer_mark_dirty(mWatchfaceLayer);
	
	if (connected) {
		vibes_short_pulse();
	} else {
		vibes_double_pulse();
	}
}


// Render clock
static void updateClock(struct tm* tickTime, TimeUnits unitsChanged) {
	layer_mark_dirty(mWatchfaceLayer);
}


// Draw progress
static void drawProgress(GContext* context, GColor strokeColor, GColor innerColor, uint16_t stroke, GPoint center, int16_t width, int16_t height, double percent) {
	percent = percent > 100.0 ? 100.0 : percent;
	percent = percent < 0.0 ? 0.0 : percent;
		
	double progressLength = percent / 100.0 * (width + height) * 2;
	GRect rect;
	
	// 6 parts (lines) of a full progress circle
	// Part 5: start point
	if (progressLength == (width + height) * 2) {
		rect = GRect(
			center.x - stroke / 2,
			center.y - height / 2,
			stroke,
			stroke
		);
		graphics_context_set_fill_color(context, strokeColor);
		graphics_fill_rect(context, rect, 0, GCornerNone);
	}
	
	// Part 5: top left
	if (width * 1.5 + height * 2 < progressLength && progressLength < width * 1.5 + height * 2 + stroke * 2)
		progressLength = width * 1.5 + height * 2 + stroke * 2;
	if (width * 1.5 + height * 2 + stroke * 2 <= progressLength) {
		// Draw corner
		rect = GRect(
			center.x - width / 2 + stroke,
			center.y - height / 2 + stroke,
			stroke / 2,
			stroke / 2
		);
		graphics_context_set_fill_color(context, strokeColor);
		graphics_fill_rect(context, rect, stroke / 2, GCornerNone);
		rect = GRect(
			center.x - width / 2 + stroke,
			center.y - height / 2 + stroke,
			stroke,
			stroke
		);
		graphics_context_set_fill_color(context, innerColor);
		graphics_fill_rect(context, rect, stroke / 2, GCornersAll);
		// Draw line
		rect = GRect(
			center.x - width / 2,
			center.y - height / 2,
			progressLength - width * 1.5 - height * 2,
			stroke
		);
		graphics_context_set_fill_color(context, strokeColor);
		graphics_fill_rect(context, rect, stroke / 2, GCornersAll);
		
		progressLength = width * 1.5 + height * 2;
	}
	
	// Part 4: left
	if (width * 1.5 + height < progressLength && progressLength < width * 1.5 + height + stroke * 2)
		progressLength = width * 1.5 + height + stroke * 2;
	if (width * 1.5 + height + stroke * 2 <= progressLength) {
		// Draw corner
		rect = GRect(
			center.x - width / 2 + stroke,
			center.y + height / 2 - stroke * 1.5,
			stroke / 2,
			stroke / 2
		);
		graphics_context_set_fill_color(context, strokeColor);
		graphics_fill_rect(context, rect, stroke / 2, GCornerNone);
		rect = GRect(
			center.x - width / 2 + stroke,
			center.y + height / 2 - stroke * 2,
			stroke,
			stroke
		);
		graphics_context_set_fill_color(context, innerColor);
		graphics_fill_rect(context, rect, stroke / 2, GCornersAll);
		// Draw line
		rect = GRect(
			center.x - width / 2,
			center.y + height / 2 - (progressLength - width * 1.5 - height),
			stroke,
			progressLength - width * 1.5 - height
		);
		graphics_context_set_fill_color(context, strokeColor);
		graphics_fill_rect(context, rect, stroke / 2, GCornersAll);
		
		progressLength = width * 1.5 + height;
	}
	
	// Part 3: bottom right
	if (width / 2 + height < progressLength && progressLength < width / 2 + height + stroke * 2)
		progressLength = width / 2 + height + stroke * 2;
	if (width / 2 + height + stroke * 2 <= progressLength) {
		// Draw corner
		rect = GRect(
			center.x + width / 2 - stroke * 1.5,
			center.y + height / 2 - stroke * 1.5,
			stroke / 2,
			stroke / 2
		);
		graphics_context_set_fill_color(context, strokeColor);
		graphics_fill_rect(context, rect, stroke / 2, GCornerNone);
		rect = GRect(
			center.x + width / 2 - stroke * 2,
			center.y + height / 2 - stroke * 2,
			stroke,
			stroke
		);
		graphics_context_set_fill_color(context, innerColor);
		graphics_fill_rect(context, rect, stroke / 2, GCornersAll);
		// Draw line
		rect = GRect(
			center.x + width / 2 - (progressLength - width / 2 - height),
			center.y + height / 2 - stroke,
			progressLength - width / 2 - height,
			stroke
		);
		graphics_context_set_fill_color(context, strokeColor);
		graphics_fill_rect(context, rect, stroke / 2, GCornersAll);
		
		progressLength = width / 2 + height;
	}
	
	// Part 2: right
	if (width / 2 < progressLength && progressLength < width / 2 + stroke * 2)
		progressLength = width / 2 + stroke * 2;
	if (width / 2 + stroke * 2 <= progressLength) {
		// Draw corner
		rect = GRect(
			center.x + width / 2 - stroke * 1.5,
			center.y - height / 2 + stroke,
			stroke / 2,
			stroke / 2
		);
		graphics_context_set_fill_color(context, strokeColor);
		graphics_fill_rect(context, rect, stroke / 2, GCornerNone);
		rect = GRect(
			center.x + width / 2 - stroke * 2,
			center.y - height / 2 + stroke,
			stroke,
			stroke
		);
		graphics_context_set_fill_color(context, innerColor);
		graphics_fill_rect(context, rect, stroke / 2, GCornersAll);
		// Draw line
		rect = GRect(
			center.x + width / 2 - stroke,
			center.y - height / 2,
			stroke,
			progressLength - width / 2
		);
		graphics_context_set_fill_color(context, strokeColor);
		graphics_fill_rect(context, rect, stroke / 2, GCornersAll);
		
		progressLength = width / 2;
	}
	
	// Part 1: top right
	progressLength = progressLength < stroke ? stroke : progressLength;
	if (stroke <= progressLength) {
		graphics_context_set_fill_color(context, strokeColor);
		rect = GRect(
			center.x - stroke / 2,
			center.y - height / 2,
			progressLength + stroke / 2,
			stroke
		);
		graphics_fill_rect(context, rect, stroke / 2, GCornersAll);
	}
}


// Render window
static void updateWatchface(Layer* layer, GContext* context) {
	GRect bounds = layer_get_frame(layer);
	GPoint centerPoint = grect_center_point(&bounds);
	uint16_t adjustment = 16;
	uint16_t margin = 4;
	
	// Draw background
	graphics_context_set_fill_color(context, GColorBlack);
	graphics_fill_rect(context, bounds, 0, GCornerNone);
	
	// Draw and progress circles
	uint16_t stroke = 12;
	drawProgress(context, GColorRajah, GColorBlack, stroke, centerPoint, bounds.size.w, bounds.size.h, 100.0);
	drawProgress(context, GColorOrange, GColorBlack, stroke, centerPoint, bounds.size.w, bounds.size.h, 100.0 * mCounter.steps / mStepGoal);
	drawProgress(context, GColorMintGreen, GColorBlack, stroke, centerPoint, bounds.size.w - stroke * 2, bounds.size.h - stroke * 2, 100.0);
	drawProgress(context, GColorGreen, GColorBlack, stroke, centerPoint, bounds.size.w - stroke * 2, bounds.size.h - stroke * 2, 100.0 * (mCounter.walkTime + mCounter.jogTime) / 60.0 / mActiveTimeGoal);
	
	// Update time
	time_t now = time(NULL);
	struct tm* currentTime = localtime(&now);
	static char timeString[] = "00";
	
	// Draw hour: 12h or 24h
	if (clock_is_24h_style()) {
		strftime(timeString, sizeof(timeString), "%H", currentTime);
	} else {
		strftime(timeString, sizeof(timeString), "%I", currentTime);
	}
	GSize hourSize = graphics_text_layout_get_content_size(
		timeString,
		fonts_get_system_font(FONT_KEY_LECO_38_BOLD_NUMBERS),
		bounds,
		GTextOverflowModeFill,
		GTextAlignmentCenter
	);
	GRect hourRect = GRect(
		centerPoint.x - hourSize.w / 2,
		centerPoint.y - hourSize.h - adjustment,
		hourSize.w,
		hourSize.h
	);
	graphics_context_set_text_color(context, GColorRed);
	graphics_draw_text(
		context,
		timeString,
		fonts_get_system_font(FONT_KEY_LECO_38_BOLD_NUMBERS),
		hourRect,
		GTextOverflowModeFill,
		GTextAlignmentCenter,
		NULL
	);
	
	// Draw minute
	strftime(timeString, sizeof(timeString), "%M", currentTime);
	GSize minuteSize = graphics_text_layout_get_content_size(
		timeString,
		fonts_get_system_font(FONT_KEY_LECO_38_BOLD_NUMBERS),
		bounds,
		GTextOverflowModeFill,
		GTextAlignmentCenter
	);
	GRect minuteRect = GRect(
		centerPoint.x - minuteSize.w / 2,
		centerPoint.y - adjustment,
		minuteSize.w,
		minuteSize.h
	);
	graphics_context_set_text_color(context, GColorRed);
	graphics_draw_text(
		context,
		timeString,
		fonts_get_system_font(FONT_KEY_LECO_38_BOLD_NUMBERS),
		minuteRect,
		GTextOverflowModeFill,
		GTextAlignmentCenter,
		NULL
	);
	
	// Draw step count
	static char stepString[] = "00000";
	snprintf(stepString, sizeof(stepString), "%lu", mCounter.steps);
	GSize stepSize = graphics_text_layout_get_content_size(
		stepString,
		fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS),
		bounds,
		GTextOverflowModeFill,
		GTextAlignmentCenter
	);
	GRect stepRect = GRect(
		centerPoint.x - stepSize.w / 2,
		centerPoint.y + minuteSize.h - adjustment + margin,
		stepSize.w,
		stepSize.h
	);
	graphics_context_set_text_color(context, GColorOrange);
	graphics_draw_text(
		context,
		stepString,
		fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS),
		stepRect,
		GTextOverflowModeFill,
		GTextAlignmentCenter,
		NULL
	);
	
	// Draw Bluetooth status
	static char bluetoothString[] = "x";
	if (! mIsBluetoothConnected) {
		GSize bluetoothSize = graphics_text_layout_get_content_size(
			bluetoothString,
			fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
			bounds,
			GTextOverflowModeFill,
			GTextAlignmentCenter
		);
		GRect bluetoothRect = GRect(
			centerPoint.x - bluetoothSize.w / 2,
			centerPoint.y - hourSize.h - bluetoothSize.h - adjustment + 6,
			bluetoothSize.w,
			bluetoothSize.h
		);
		graphics_context_set_text_color(context, GColorCyan);
		graphics_draw_text(
			context,
			bluetoothString,
			fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
			bluetoothRect,
			GTextOverflowModeFill,
			GTextAlignmentCenter,
			NULL
		);
	}
}


// Send update to watchface
static void sendConfigToWorker() {
	AppWorkerMessage message;
	message.data0 = (uint16_t) mResetTime;
	app_worker_send_message(101, &message);
	message.data0 = (uint16_t) mPedometerSensitivity;
	app_worker_send_message(102, &message);
}


// App Message Sync
static void messageReceived(DictionaryIterator* received, void* context) {
	APP_LOG(APP_LOG_LEVEL_INFO, "App Message: Received");

	Tuple* resetTuple = dict_find(received, RESET_TIME_KEY);
	Tuple* sensitivityTuple = dict_find(received, PEDOMETER_SENSITIVITY_KEY);
	Tuple* stepGoalTuple = dict_find(received, STEP_GOAL_KEY);
	Tuple* activeTimeGoalTuple = dict_find(received, ACTIVE_TIME_GOAL_KEY);

	mResetTime = resetTuple->value->int32;
	mPedometerSensitivity = sensitivityTuple->value->int32;
	mStepGoal = stepGoalTuple->value->int32;
	mActiveTimeGoal = activeTimeGoalTuple->value->int32;

	// Refresh UI
	layer_mark_dirty(mWatchfaceLayer);
	
	// Send a message to worker
	sendConfigToWorker();
}


static void messageDropped(AppMessageResult reason, void* context) {
	APP_LOG(APP_LOG_LEVEL_INFO, "App Message: Dropped");
}


static void messageSent(DictionaryIterator* sent, void* context) {
	APP_LOG(APP_LOG_LEVEL_INFO, "App Message: Sent");
}


static void messageFailed(DictionaryIterator* sent, AppMessageResult reason, void* context) {
	APP_LOG(APP_LOG_LEVEL_INFO, "App Message: Failed");
}


static void workerMessageReceived(uint16_t type, AppWorkerMessage *data) {
	switch (type) {
		case 0:
			mCounter.sleepTime = (uint32_t) data->data0;
			break;
		case 1:
			mCounter.sitTime = (uint32_t) data->data0;
			break;
		case 2:
			mCounter.walkTime = (uint32_t) data->data0;
			break;
		case 3:
			mCounter.jogTime = (uint32_t) data->data0;
			break;
		case 4:
			mCounter.steps = (uint32_t) data->data0;
			break;
		case 5:
			mCurrentType = (uint32_t) data->data0;
			layer_mark_dirty(mWatchfaceLayer);
			break;
		default:
			break;
	}
}


static void windowLoad(Window *window) {
	// Setup UIs
	Layer* rootLayer = window_get_root_layer(mWindow);
	GRect bounds = layer_get_bounds(rootLayer);
	mWatchfaceLayer = layer_create(bounds);
	layer_add_child(rootLayer, mWatchfaceLayer);
	
	// Register function for update
	layer_set_update_proc(mWatchfaceLayer, updateWatchface);
}


static void windowUnload(Window *window) {
	// Destroy UIs
	layer_destroy(mWatchfaceLayer);
}


static void init(void) {
	// Load persistent values
	loadConfig();

	// Start the worker and send the config
	app_worker_message_subscribe(&workerMessageReceived);
	AppWorkerResult result = app_worker_launch();
	sendConfigToWorker();
	APP_LOG(APP_LOG_LEVEL_INFO, "Worker launch result: %d", result);

	// Request an immediate status update from the worker
	AppWorkerMessage message;
	message.data0 = (uint16_t) 0;
	app_worker_send_message(100, &message);
	// Immediately check Bluetooth and battery status
	mIsBluetoothConnected = bluetooth_connection_service_peek();
	mBatteryChargeState = battery_state_service_peek();

	// AppMessage
	app_message_register_inbox_received(&messageReceived);
	app_message_register_inbox_dropped(&messageDropped);
	app_message_register_outbox_sent(&messageSent);
	app_message_register_outbox_failed(&messageFailed);
	// https://developer.getpebble.com/2/api-reference/group___dictionary.html#ga0551d069624fb5bfc066fecfa4153bde
	app_message_open(56, 12);

	// For main window
	mWindow = window_create();

	window_set_window_handlers(
		mWindow,
		(WindowHandlers) {
			.load = windowLoad,
			.unload = windowUnload,
		}
	);
	window_stack_push(mWindow, true);

	// Subscribe services
	// For Bluetooth connection
	bluetooth_connection_service_subscribe(processBluetoothConnectionEvent);
	// For tap event
	accel_tap_service_subscribe(&processTapEvent);
	// For tick timer
	tick_timer_service_subscribe(MINUTE_UNIT, &updateClock);
	// For other state
    battery_state_service_subscribe(&updateBattery);
}


static void deinit(void) {
	// Save configuration
	saveConfig();

	// Unsubscribe services
	accel_tap_service_unsubscribe();
	// bluetooth_connection_service_unsubscribe();
	tick_timer_service_unsubscribe();
	battery_state_service_unsubscribe();

	// Unsubscribe messages
	app_message_deregister_callbacks();
	app_worker_message_unsubscribe();

	window_destroy(mWindow);
}


int main(void) {
	init();
	app_event_loop();
	deinit();
}
