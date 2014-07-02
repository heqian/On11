#include <pebble.h>
#include "recognizer.h"

#define DEBUG				false

// Config
static bool mIsDriving = false;
static int32_t mPedometerSensitivity = 20;	// Range: [0, 100]

// UI
static Window* mWindow = NULL;

// Classification
static LowPassFilter mFilter;
static uint32_t mCurrentType = 1;

static Counter mCounter;

// Handle accleration data
void processAccelerometerData(AccelData* acceleration, uint32_t size) {
	Counter counter = mCounter;

	// Throw the variables into below function, it will update it for you.
	if (analyzeAcceleration(&mCurrentType, &mCounter, &mFilter, mIsDriving, mPedometerSensitivity, acceleration, size)) {
		// Log data for companion app
		uint32_t steps = mCounter.steps - counter.steps;

		// DEBUG
		if (DEBUG) {
			char tmpStr[32];
			switch(mCurrentType) {
				case 0:
					snprintf(tmpStr, sizeof(tmpStr), "Sleep");
					break;
				case 1:
					snprintf(tmpStr, sizeof(tmpStr), "Sit");
					break;
				case 2:
					snprintf(tmpStr, sizeof(tmpStr), "Walk");
					break;
				case 3:
					snprintf(tmpStr, sizeof(tmpStr), "Jog");
					break;
			}
			app_log(APP_LOG_LEVEL_INFO, "Activity type", 0, tmpStr, mWindow);

			snprintf(tmpStr, 32, "%5d (+) -> %5d", (int) steps, (int) (mCounter.steps));
			app_log(APP_LOG_LEVEL_INFO, "Steps change", 0, tmpStr, mWindow);
		}

		// You may want to log this and update UI
		// ... //
	}
}

static void windowLoad(Window *window) {
	// Create & Setup UIs
	Layer* windowLayer = window_get_root_layer(mWindow);
	GRect bounds = layer_get_bounds(windowLayer);

	// ... //
}

static void windowUnload(Window *window) {
	if (DEBUG) {
		char msg[] = "windowUnload() called";
		app_log(APP_LOG_LEVEL_DEBUG, "DEBUG", 0, msg, mWindow);
	}

	// Destroy UIs
	// ... //
}

static void init(void) {
	// Load persistent values
	mCounter.sleepTime = persist_exists(0) ? persist_read_int(0) : 0;
	mCounter.sitTime = persist_exists(1) ? persist_read_int(1) : 0;
	mCounter.walkTime = persist_exists(2) ? persist_read_int(2) : 0;
	mCounter.jogTime = persist_exists(3) ? persist_read_int(3) : 0;
	if (persist_exists(4)) {
		mCounter.timestamp = persist_read_int(4);
	} else {
		mCounter.timestamp = (uint32_t) time(NULL);
	}
	mCounter.steps = persist_exists(5) ? persist_read_int(5) : 0;

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

	// Setup accelerometer API
	accel_data_service_subscribe(BATCH_SIZE, &processAccelerometerData);
	accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);

	// Initiate low pass filter for later use
	initLowPassFilter(&mFilter);
}

static void deinit(void) {
	if (DEBUG) {
		char msg[] = "deinit() called";
		app_log(APP_LOG_LEVEL_DEBUG, "DEBUG", 0, msg, mWindow);
	}

	// Save persistent values
	persist_write_int(0, mCounter.sleepTime);
	persist_write_int(1, mCounter.sitTime);
	persist_write_int(2, mCounter.walkTime);
	persist_write_int(3, mCounter.jogTime);
	persist_write_int(4, mCounter.timestamp);
	persist_write_int(5, mCounter.steps);

	accel_data_service_unsubscribe();

	window_destroy(mWindow);
}

int main(void) {
	init();

	// Main loop
	app_event_loop();

	deinit();
}
