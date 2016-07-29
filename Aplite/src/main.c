#include <pebble.h>
#include "utility.h"

// AppMessage
enum {
	COLOR_THEME_KEY = 0x0,
	RESET_TIME_KEY = 0x1,
	SPEED_THRESHOLD_KEY = 0x2,
	REQUEST_SPEED_KEY = 0x3,
	SPEED_KEY = 0x4,
	BATTERY_THRESHOLD_KEY = 0x5,
	PEDOMETER_SENSITIVITY_KEY = 0x6
};

// Config
static bool mColorTheme = true;
static bool mIsDriving = false;
static int32_t mResetTime = 0;	// Minutes since 00:00 e.g. 09:30 AM == 570
static int32_t mSpeedThreshold = 0;	// 1044 means 10.44 m/s
static int32_t mBatteryThreshold = 0;	// Percentage of battery that should considered as "low"
static int32_t mPedometerSensitivity = 20;	// Range: [0, 100]

// UI
static Window* mWindow = NULL;
static Layer* mDashboardLayer = NULL;
static TextLayer* mClockLayer = NULL;
static TextLayer* mBatteryLayer = NULL;
static BitmapLayer* mDrivingLayer = NULL;
static TextLayer* mDateLayer = NULL;
#ifndef PBL_COLOR
static InverterLayer* mInverterLayer = NULL;
#endif
static GBitmap* mCarBitmap = NULL;
static GBitmap* mStepBitmap = NULL;
static GBitmap* mSitBitmap = NULL;
static GBitmap* mWalkBitmap = NULL;
static GBitmap* mJogBitmap = NULL;
static int16_t mStateHeight = 26;
static int16_t mTitleHeight = 18;
static int16_t mCounterHeight = 24;

static Counter mCounter;
static uint32_t mCurrentType = 0;


static void requestSpeed() {
	DictionaryIterator* data;
	app_message_outbox_begin(&data);
	Tuplet value = TupletInteger(REQUEST_SPEED_KEY, 0);
	dict_write_tuplet(data, &value);
	app_message_outbox_send();
}


// Load persistent values
static void loadStatus() {
	// Status data
	mCounter.sleepTime = persist_exists(0) ? persist_read_int(0) : 0;
	mCounter.sitTime = persist_exists(1) ? persist_read_int(1) : 0;
	mCounter.walkTime = persist_exists(2) ? persist_read_int(2) : 0;
	mCounter.jogTime = persist_exists(3) ? persist_read_int(3) : 0;
	mCounter.steps = persist_exists(4) ? persist_read_int(4) : 0;
	if (persist_exists(5)) {
		mCounter.timestamp = persist_read_int(5);
	} else {
		mCounter.timestamp = (uint32_t) time(NULL);
	}
}


static void loadConfig() {
	mColorTheme = persist_exists(6) ? persist_read_bool(6) : true;
	mResetTime = persist_exists(7) ? persist_read_int(7) : 0;
	mSpeedThreshold = persist_exists(8) ? persist_read_int(8) : 0;
	mIsDriving = persist_exists(9) ? persist_read_bool(9) : false;
	mBatteryThreshold = persist_exists(10) ? persist_read_int(10) : 0;
	mPedometerSensitivity = persist_exists(11) ? persist_read_int(11) : 20;
}


// Save persistent values
static void saveConfig() {
	persist_write_bool(6, mColorTheme);
	persist_write_int(7, mResetTime);
	persist_write_int(8, mSpeedThreshold);
	persist_write_bool(9, mIsDriving);
	persist_write_int(10, mBatteryThreshold);
	persist_write_int(11, mPedometerSensitivity);
}


static void updateBattery(BatteryChargeState state) {
	static char percent[] = "[ 100% ]";

#ifndef PBL_COLOR
	text_layer_set_text_color(mBatteryLayer, GColorBlack);
	text_layer_set_background_color(mBatteryLayer, GColorWhite);
#else
	if (state.charge_percent <= mBatteryThreshold) {
		text_layer_set_text_color(mBatteryLayer, GColorBlack);
	} else {
		text_layer_set_text_color(mBatteryLayer, GColorWhite);
	}
	text_layer_set_background_color(mBatteryLayer, GColorOrange);
#endif

	if (state.charge_percent <= mBatteryThreshold) {
		snprintf(percent, sizeof(percent), "[ %d%% ]", state.charge_percent);
	} else {
		snprintf(percent, sizeof(percent), "%d%%", state.charge_percent);
	}

	text_layer_set_text(mBatteryLayer, percent);
}


// Render clock
static void updateClock(struct tm* tickTime, TimeUnits unitsChanged) {
	static char timeString[] = "00:00";
	static char dateString[] = "Fri 13";
	
#ifndef PBL_COLOR
	text_layer_set_text_color(mClockLayer, GColorBlack);
	text_layer_set_background_color(mClockLayer, GColorWhite);
	text_layer_set_text_color(mDateLayer, GColorBlack);
	text_layer_set_background_color(mDateLayer, GColorWhite);
#else
	text_layer_set_text_color(mClockLayer, GColorWhite);
	text_layer_set_background_color(mClockLayer, GColorOrange);
	text_layer_set_text_color(mDateLayer, GColorWhite);
	text_layer_set_background_color(mDateLayer, GColorOrange);
#endif

	// 12h or 24h
	if (clock_is_24h_style()) {
		strftime(timeString, sizeof(timeString), "%R", tickTime);
	} else {
		strftime(timeString, sizeof(timeString), "%I:%M", tickTime);
	}
	text_layer_set_text(mClockLayer, timeString);
	strftime(dateString, sizeof(dateString), "%a %d", tickTime);

	// Update date
	text_layer_set_text(mDateLayer, dateString);

	// Check speed
	if (tickTime->tm_min % 5 == 0 && mSpeedThreshold != 0) {	// Check only if time is XX:05, XX:10, XX:15, etc
		mIsDriving = false;
		layer_set_hidden(bitmap_layer_get_layer(mDrivingLayer), !mIsDriving);
		requestSpeed();
	}
}


// Render dashboard
static void updateDashboard(Layer* layer, GContext* context) {
#ifndef PBL_COLOR
	graphics_context_set_text_color(context, GColorWhite);
	graphics_context_set_fill_color(context, GColorBlack);
#else
	graphics_context_set_text_color(context, GColorBlack);
	graphics_context_set_fill_color(context, GColorGreen);
#endif
	GRect bounds = layer_get_frame(layer);
	int16_t w = bounds.size.w / 2;
	// int16_t h = bounds.size.h / 2;
	int16_t radius = 8;
	char tmpStr[64];


	/*
	 * Titles
	 */
	GRect titleRect;
	// Steps
	titleRect = GRect(0, 0, w, mTitleHeight);
	graphics_draw_bitmap_in_rect(context, mStepBitmap, titleRect);
	// Sit
	titleRect = GRect(w, 0, w, mTitleHeight);
	graphics_draw_bitmap_in_rect(context, mSitBitmap, titleRect);
	// Walk
	titleRect = GRect(0, bounds.size.h - mTitleHeight, w, mTitleHeight);
	graphics_draw_bitmap_in_rect(context, mWalkBitmap, titleRect);
	// Jog
	titleRect = GRect(w, bounds.size.h - mTitleHeight, w, mTitleHeight);
	graphics_draw_bitmap_in_rect(context, mJogBitmap, titleRect);


	/*
	 * Time background colors
	 */
	GRect counterRect;
	counterRect = GRect(0, mTitleHeight, bounds.size.w, mCounterHeight);
	graphics_fill_rect(context, counterRect, 0, GCornerNone);
	counterRect = GRect(0, bounds.size.h - mTitleHeight - mCounterHeight, bounds.size.w, mCounterHeight);
	graphics_fill_rect(context, counterRect, 0, GCornerNone);

	// Fill selected color
	switch (mCurrentType) {
		case 0:
			// Do NOTHING
			break;
		case 1:
#ifndef PBL_COLOR
			graphics_context_set_fill_color(context, GColorWhite);
#else
			graphics_context_set_fill_color(context, GColorOrange);
#endif
			counterRect = GRect(0, mTitleHeight, w, mCounterHeight);
			graphics_fill_rect(context, counterRect, 0, GCornerNone);
			counterRect = GRect(w, mTitleHeight, w, mCounterHeight);
			graphics_fill_rect(context, counterRect, radius, GCornerTopLeft);
#ifndef PBL_COLOR
			graphics_context_set_fill_color(context, GColorBlack);
#else
			graphics_context_set_fill_color(context, GColorGreen);
#endif
			counterRect = GRect(0, mTitleHeight, w, mCounterHeight);
			graphics_fill_rect(context, counterRect, radius, GCornerBottomRight);
			break;
		case 2:
#ifndef PBL_COLOR
			graphics_context_set_fill_color(context, GColorWhite);
#else
			graphics_context_set_fill_color(context, GColorOrange);
#endif
			counterRect = GRect(0, bounds.size.h - mCounterHeight - mTitleHeight, w, mCounterHeight);
			graphics_fill_rect(context, counterRect, radius, GCornerBottomRight);
			counterRect = GRect(w, bounds.size.h - mCounterHeight - mTitleHeight, w, mCounterHeight);
			graphics_fill_rect(context, counterRect, 0, GCornerNone);
#ifndef PBL_COLOR
			graphics_context_set_fill_color(context, GColorBlack);
#else
			graphics_context_set_fill_color(context, GColorGreen);
#endif
			counterRect = GRect(w, bounds.size.h - mCounterHeight - mTitleHeight, w, mCounterHeight);
			graphics_fill_rect(context, counterRect, radius, GCornerTopLeft);
			break;
		case 3:
#ifndef PBL_COLOR
			graphics_context_set_fill_color(context, GColorWhite);
#else
			graphics_context_set_fill_color(context, GColorOrange);
#endif
			counterRect = GRect(0, bounds.size.h - mCounterHeight - mTitleHeight, w, mCounterHeight);
			graphics_fill_rect(context, counterRect, 0, GCornerNone);
			counterRect = GRect(w, bounds.size.h - mCounterHeight - mTitleHeight, w, mCounterHeight);
			graphics_fill_rect(context, counterRect, radius, GCornerBottomLeft);
#ifndef PBL_COLOR
			graphics_context_set_fill_color(context, GColorBlack);
#else
			graphics_context_set_fill_color(context, GColorGreen);
#endif
			counterRect = GRect(0, bounds.size.h - mCounterHeight - mTitleHeight, w, mCounterHeight);
			graphics_fill_rect(context, counterRect, radius, GCornerTopRight);
			break;
		default:
			break;
	}


	/*
	 * Step counter
	 */
	counterRect = GRect(0, mTitleHeight, w, mCounterHeight);
	snprintf(tmpStr, 64, "%d", (int) mCounter.steps);
	graphics_draw_text(
		context,
		tmpStr,
		fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
		counterRect,
		GTextOverflowModeWordWrap,
		GTextAlignmentCenter,
		NULL
	);


	/*
	 * Time counters
	 */
	// Draw the un-selected ones
	if (mCurrentType != 1) {
		counterRect = GRect(w, mTitleHeight, w, mCounterHeight);
		snprintf(tmpStr, 64, "%02d:%02d", (int) (mCounter.sitTime / 3600 % 24), (int) (mCounter.sitTime / 60 % 60));
		graphics_draw_text(
			context,
			tmpStr,
			fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
			counterRect,
			GTextOverflowModeWordWrap,
			GTextAlignmentCenter,
			NULL
		);
	}

	if (mCurrentType != 2) {
		counterRect = GRect(0, bounds.size.h - mCounterHeight - mTitleHeight, w, mCounterHeight);
		snprintf(tmpStr, 64, "%02d:%02d", (int) (mCounter.walkTime / 3600 % 24), (int) (mCounter.walkTime / 60 % 60));
		graphics_draw_text(
			context,
			tmpStr,
			fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
			counterRect,
			GTextOverflowModeWordWrap,
			GTextAlignmentCenter,
			NULL
		);
	}

	if (mCurrentType != 3) {
		counterRect = GRect(w, bounds.size.h - mCounterHeight - mTitleHeight, w, mCounterHeight);
		snprintf(tmpStr, 64, "%02d:%02d", (int) (mCounter.jogTime / 3600 % 24), (int) (mCounter.jogTime / 60 % 60));
		graphics_draw_text(
			context,
			tmpStr,
			fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
			counterRect,
			GTextOverflowModeWordWrap,
			GTextAlignmentCenter,
			NULL
		);
	}

	// Draw the selected one
#ifndef PBL_COLOR
	graphics_context_set_text_color(context, GColorBlack);
#else
	graphics_context_set_text_color(context, GColorWhite);
#endif
	switch (mCurrentType) {
		case 0:
			// Do NOTHING
			break;
		case 1:
			counterRect = GRect(w, mTitleHeight, w, mCounterHeight);
			snprintf(tmpStr, 64, "%02d:%02d", (int) (mCounter.sitTime / 3600 % 24), (int) (mCounter.sitTime / 60 % 60));
			graphics_draw_text(
				context,
				tmpStr,
				fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
				counterRect,
				GTextOverflowModeWordWrap,
				GTextAlignmentCenter,
				NULL
			);
			break;
		case 2:
			counterRect = GRect(0, bounds.size.h - mCounterHeight - mTitleHeight, w, mCounterHeight);
			snprintf(tmpStr, 64, "%02d:%02d", (int) (mCounter.walkTime / 3600 % 24), (int) (mCounter.walkTime / 60 % 60));
			graphics_draw_text(
				context,
				tmpStr,
				fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
				counterRect,
				GTextOverflowModeWordWrap,
				GTextAlignmentCenter,
				NULL
			);
			break;
		case 3:
			counterRect = GRect(w, bounds.size.h - mCounterHeight - mTitleHeight, w, mCounterHeight);
			snprintf(tmpStr, 64, "%02d:%02d", (int) (mCounter.jogTime / 3600 % 24), (int) (mCounter.jogTime / 60 % 60));
			graphics_draw_text(
				context,
				tmpStr,
				fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
				counterRect,
				GTextOverflowModeWordWrap,
				GTextAlignmentCenter,
				NULL
			);
			break;
		default:
			break;
	}
}


// Handle tap event
static void processTapEvent(AccelAxisType axis, int32_t direction) {
}


// Handle Bluetooth connection event
static void processBluetoothConnectionEvent(bool connected) {
	if (connected) {
		vibes_short_pulse();
	} else {
		vibes_double_pulse();
	}
}

// App Message Sync
static void messageReceived(DictionaryIterator* received, void* context) {
	APP_LOG(APP_LOG_LEVEL_INFO, "App Message: Received");

	if (dict_size(received) == 56) {
		Tuple* colorTuple = dict_find(received, COLOR_THEME_KEY);
		Tuple* resetTuple = dict_find(received, RESET_TIME_KEY);
		Tuple* speedTuple = dict_find(received, SPEED_THRESHOLD_KEY);
		Tuple* batteryTuple = dict_find(received, BATTERY_THRESHOLD_KEY);
		Tuple* sensitivityTuple = dict_find(received, PEDOMETER_SENSITIVITY_KEY);

		mColorTheme = (bool) colorTuple->value->int32;
		mResetTime = resetTuple->value->int32;
		mSpeedThreshold = speedTuple->value->int32;
		mBatteryThreshold = batteryTuple->value->int32;
		mPedometerSensitivity = sensitivityTuple->value->int32;

		// Refresh UI
		layer_mark_dirty(mDashboardLayer);
		time_t currentTime = time(NULL);
		struct tm* time = localtime(&currentTime);
		updateClock(time, MINUTE_UNIT);
		updateBattery(battery_state_service_peek());
#ifndef PBL_COLOR
		layer_set_hidden(inverter_layer_get_layer(mInverterLayer), mColorTheme);
#endif

		// Send a message to worker
		AppWorkerMessage message;
		message.data0 = (uint16_t) mPedometerSensitivity;
		app_worker_send_message(101, &message);
		message.data0 = (uint16_t) mResetTime;
		app_worker_send_message(102, &message);
	} else {
		Tuple* speedTuple = dict_find(received, SPEED_KEY);
		int speed = speedTuple->value->int32;
#ifndef PBL_COLOR
		if (speed >= mSpeedThreshold) {
			mIsDriving = true;
			layer_set_hidden(bitmap_layer_get_layer(mDrivingLayer), !mIsDriving);
		} else {
			mIsDriving = false;
			layer_set_hidden(bitmap_layer_get_layer(mDrivingLayer), !mIsDriving);
		}
#endif
		APP_LOG(APP_LOG_LEVEL_INFO, "Speed: %d (threshold: %d)", speed, (int) mSpeedThreshold);

		// Send a message to worker
		AppWorkerMessage message;
		message.data0 = (uint16_t) mIsDriving;
		app_worker_send_message(103, &message);
	}
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
			layer_mark_dirty(mDashboardLayer);
			break;
		default:
			break;
	}
}


static void windowLoad(Window *window) {
	// Setup UIs
	Layer* windowLayer = window_get_root_layer(mWindow);
	GRect bounds = layer_get_bounds(windowLayer);

    mDashboardLayer = layer_create(bounds);
	GRect rect = GRect(0, mTitleHeight + mCounterHeight, bounds.size.w, bounds.size.h - (mTitleHeight + mCounterHeight) * 2);
	mClockLayer = text_layer_create(rect);
	rect = GRect(0, bounds.size.h - mStateHeight - mTitleHeight - mCounterHeight, bounds.size.w / 2, mStateHeight);
	mBatteryLayer = text_layer_create(rect);
	rect = GRect((bounds.size.w - mStateHeight) / 2, bounds.size.h - mStateHeight - mTitleHeight - mCounterHeight, mStateHeight, mStateHeight);
	mDrivingLayer = bitmap_layer_create(rect);
	rect = GRect(bounds.size.w / 2, bounds.size.h - mStateHeight - mTitleHeight - mCounterHeight, bounds.size.w / 2, mStateHeight);
	mDateLayer = text_layer_create(rect);
#ifndef PBL_COLOR
	mInverterLayer = inverter_layer_create(bounds);
#endif

	// Register function for update
	layer_set_update_proc(mDashboardLayer, updateDashboard);

	// Adjuest text font for clcok
	text_layer_set_font(mClockLayer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
	text_layer_set_text_alignment(mClockLayer, GTextAlignmentCenter);

	// Adjuest text font for states
	text_layer_set_font(mBatteryLayer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	text_layer_set_text_alignment(mBatteryLayer, GTextAlignmentCenter);
	text_layer_set_font(mDateLayer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	text_layer_set_text_alignment(mDateLayer, GTextAlignmentCenter);
	mCarBitmap = gbitmap_create_with_resource(RESOURCE_ID_CAR);
	mStepBitmap = gbitmap_create_with_resource(RESOURCE_ID_STEP);
	mSitBitmap = gbitmap_create_with_resource(RESOURCE_ID_SIT);
	mWalkBitmap = gbitmap_create_with_resource(RESOURCE_ID_WALK);
	mJogBitmap = gbitmap_create_with_resource(RESOURCE_ID_JOG);
	bitmap_layer_set_bitmap(mDrivingLayer, mCarBitmap);
	bitmap_layer_set_alignment(mDrivingLayer, GAlignCenter);

	// Add to window layer
    layer_add_child(windowLayer, mDashboardLayer);
	layer_add_child(windowLayer, text_layer_get_layer(mClockLayer));
	layer_add_child(windowLayer, text_layer_get_layer(mBatteryLayer));
	layer_add_child(windowLayer, text_layer_get_layer(mDateLayer));
	layer_add_child(windowLayer, bitmap_layer_get_layer(mDrivingLayer));
	layer_set_hidden(bitmap_layer_get_layer(mDrivingLayer), !mIsDriving);
#ifndef PBL_COLOR
	layer_add_child(windowLayer, inverter_layer_get_layer(mInverterLayer));
#endif

	// Immediately update
	time_t currentTime = time(NULL);
	struct tm* time = localtime(&currentTime);
	updateClock(time, MINUTE_UNIT);
	updateBattery(battery_state_service_peek());
#ifndef PBL_COLOR
	layer_set_hidden(inverter_layer_get_layer(mInverterLayer), mColorTheme);
#endif
}


static void windowUnload(Window *window) {
	// Destroy UIs
	layer_destroy(mDashboardLayer);

	gbitmap_destroy(mCarBitmap);
	gbitmap_destroy(mStepBitmap);
	gbitmap_destroy(mSitBitmap);
	gbitmap_destroy(mWalkBitmap);
	gbitmap_destroy(mJogBitmap);
	text_layer_destroy(mClockLayer);
	text_layer_destroy(mBatteryLayer);
	text_layer_destroy(mDateLayer);
	bitmap_layer_destroy(mDrivingLayer);
#ifndef PBL_COLOR
	inverter_layer_destroy(mInverterLayer);
#endif
}


static void init(void) {
	// Load persistent values
	loadStatus();
	loadConfig();

	// AppMessage
	app_message_register_inbox_received(&messageReceived);
	app_message_register_inbox_dropped(&messageDropped);
	app_message_register_outbox_sent(&messageSent);
	app_message_register_outbox_failed(&messageFailed);
	// https://developer.getpebble.com/2/api-reference/group___dictionary.html#ga0551d069624fb5bfc066fecfa4153bde
	app_message_open(56, 12);

	// AppWorkerMessage
	app_worker_message_subscribe(&workerMessageReceived);

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
	// Start worker
	AppWorkerResult result = app_worker_launch();
	// Request an update
	AppWorkerMessage message;
	message.data0 = (uint16_t) 0;
	app_worker_send_message(100, &message);

	APP_LOG(APP_LOG_LEVEL_INFO, "Worker launch result: %d", result);
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
