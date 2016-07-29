#include <pebble_worker.h>
#include "recognizer.h"

#define DATA_LOG_INTERVAL_S	60

// Classification
static LowPassFilter mFilter;

// Config
static bool mIsDriving = false;
static int32_t mResetTime = 0;	// Minutes since 00:00 e.g. 09:30 AM == 570
static int32_t mPedometerSensitivity = 20;	// Range: [0, 100]

// DataLogging
DataLoggingSessionRef mDataLog;

static Counter mCounter;
static Counter mLastCounter;
static uint32_t mActivityType = 0;


// Load persistent values
static void loadStatus() {
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
	mResetTime = persist_exists(7) ? persist_read_int(7) : 0;
	mIsDriving = persist_exists(9) ? persist_read_bool(9) : false;
	mPedometerSensitivity = persist_exists(11) ? persist_read_int(11) : 20;
}


// Save persistent values
static void saveStatus() {
	persist_write_int(0, mCounter.sleepTime);
	persist_write_int(1, mCounter.sitTime);
	persist_write_int(2, mCounter.walkTime);
	persist_write_int(3, mCounter.jogTime);
	persist_write_int(4, mCounter.steps);
	persist_write_int(5, mCounter.timestamp);
}


// Send update to watchface
static void sendStatusToWatchface() {
	AppWorkerMessage message;
	message.data0 = (uint16_t) mCounter.sleepTime;
	app_worker_send_message(0, &message);
	message.data0 = (uint16_t) mCounter.sitTime;
	app_worker_send_message(1, &message);
	message.data0 = (uint16_t) mCounter.walkTime;
	app_worker_send_message(2, &message);
	message.data0 = (uint16_t) mCounter.jogTime;
	app_worker_send_message(3, &message);
	message.data0 = (uint16_t) mCounter.steps;
	app_worker_send_message(4, &message);
	message.data0 = (uint16_t) mActivityType;
	app_worker_send_message(5, &message);
}


// Handle accleration data
static void processAccelerometerData(AccelData* acceleration, uint32_t size) {
	// Throw the variables into below function, it will update it for you.
	uint32_t result = analyzeAcceleration(&mActivityType, &mCounter, &mFilter, mIsDriving, mPedometerSensitivity, acceleration, size);
	if (result == 0) {
		// Check if need to send a data log
		if (mCounter.timestamp - mLastCounter.timestamp >= DATA_LOG_INTERVAL_S) {
			uint32_t sleepTime = mCounter.sleepTime - mLastCounter.sleepTime;
			uint32_t sitTime = mCounter.sitTime - mLastCounter.sitTime;
			uint32_t walkTime = mCounter.walkTime - mLastCounter.walkTime;
			uint32_t jogTime = mCounter.jogTime - mLastCounter.jogTime;
			uint32_t steps = mCounter.steps - mLastCounter.steps;
			uint32_t timestamp = mCounter.timestamp;

			// DataLogging: send to the companion app
			int len = sizeof(uint32_t);
			uint8_t bytes[len * 6];
			memcpy(bytes + len * 0, &sleepTime, len);
			memcpy(bytes + len * 1, &sitTime, len);
			memcpy(bytes + len * 2, &walkTime, len);
			memcpy(bytes + len * 3, &jogTime, len);
			memcpy(bytes + len * 4, &steps, len);
			memcpy(bytes + len * 5, &timestamp, len);
			data_logging_log(mDataLog, &bytes, 1);

			// Check if needs a reset
			time_t timeNow = time(NULL);
			struct tm* now = localtime(&timeNow);
			uint32_t currentTime = (uint32_t) timeNow;
			uint32_t resetTime = (uint32_t) timeNow;
			resetTime = resetTime - (now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec) + mResetTime * 60;
			if (mLastCounter.timestamp <= resetTime && resetTime <= currentTime) {
				mCounter.sleepTime = 0;
				mCounter.sitTime = 0;
				mCounter.walkTime = 0;
				mCounter.jogTime = 0;
				mCounter.steps = 0;
				mCounter.timestamp = currentTime;
			}

			mLastCounter = mCounter;
		}

		// Send a status update to watchface
		sendStatusToWatchface();
	}
}


// App Message Sync
static void workerMessageReceived(uint16_t type, AppWorkerMessage *data) {
	switch(type) {
		case 100:	// Requested an immediate update
			sendStatusToWatchface();
			break;
		case 101:
			mPedometerSensitivity = (int32_t) data->data0;
			break;
		case 102:
			mResetTime = (int32_t) data->data0;
			break;
		case 103:
			mIsDriving = (bool) data->data0;
			break;
		default:
			break;
	}
}


static void init() {
	// Load persistent values
	loadStatus();
	loadConfig();

	// Check if needs a reset
	time_t timeNow = time(NULL);
	struct tm* now = localtime(&timeNow);
	uint32_t currentTime = (uint32_t) timeNow;
	uint32_t resetTime = (uint32_t) timeNow;
	resetTime = resetTime - (now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec) + mResetTime * 60;
	if (mCounter.timestamp <= resetTime && resetTime <= currentTime) {
		mCounter.sleepTime = 0;
		mCounter.sitTime = 0;
		mCounter.walkTime = 0;
		mCounter.jogTime = 0;
		mCounter.steps = 0;
	}
	mCounter.timestamp = currentTime;
	mLastCounter = mCounter;

	// Initialize data log
	// DataLogging
	mDataLog = data_logging_create(0, DATA_LOGGING_BYTE_ARRAY, sizeof(uint32_t) * 6, false);

	// For accelerometer
	accel_data_service_subscribe(BATCH_SIZE, &processAccelerometerData);
	accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);

	// Initiate low pass filter
	initLowPassFilter(&mFilter);

	// AppWorkerMessage
	app_worker_message_subscribe(&workerMessageReceived);
}


static void deinit() {
	saveStatus();

	// Close data log
	data_logging_finish(mDataLog);
	// Unsubscribe acceleration
	accel_data_service_unsubscribe();
	// Unsubscribe worker message
	app_worker_message_unsubscribe();
}


int main(void) {
	init();
	worker_event_loop();
	deinit();
}
