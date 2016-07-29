#include "recognizer.h"

// Acceleration
static uint32_t mDataSize = 0;
static AccelData mAcceleration[SAMPLE_SIZE];

// Handle accleration data
uint32_t analyzeAcceleration(uint32_t* currentType, Counter* counter, LowPassFilter* filter, bool isDriving, int32_t sensitivity, AccelData* acceleration, uint32_t size) {
	// I don't know if Pebble's API will fail to return 0 sample. Just in case.
	if (size == 0) {
		APP_LOG(APP_LOG_LEVEL_INFO, "No acceleration sample!!");
		return 1;
	} else {
		// Add samples
		uint32_t i = 0;
		for (; i < size && mDataSize + i < SAMPLE_SIZE; i++) {
			mAcceleration[mDataSize + i].x = acceleration[i].x;
			mAcceleration[mDataSize + i].y = acceleration[i].y;
			mAcceleration[mDataSize + i].z = acceleration[i].z;
		}
		mDataSize += i;
	}

	// Check if enough
	if (mDataSize < SAMPLE_SIZE) {	// Not enough, so add data to collection first
		APP_LOG(APP_LOG_LEVEL_INFO, "Sample collector: %d/%d", (int) mDataSize, SAMPLE_SIZE);
		return 2;
	} else {	// Enough for classification
		int16_t maxV = -32767;	// Actually, it should be -32768. I just hate asymmetry...
		int16_t minV = 32767;
		Feature feature;
		feature.meanV = 0.0, feature.meanH = 0.0, feature.deviationV = 0.0, feature.deviationH = 0.0;
		for (uint32_t i = 0; i < SAMPLE_SIZE; i++) {
			// Filter out the gravity vector
			goThroughFilter(filter, mAcceleration[i].x, mAcceleration[i].y, mAcceleration[i].z);
			// Convert to linear acceleration
			mAcceleration[i].x = mAcceleration[i].x - filter->x;
			mAcceleration[i].y = mAcceleration[i].y - filter->y;
			mAcceleration[i].z = mAcceleration[i].z - filter->z;

			// Project 3D acceleration vector to gravity direction
			double v = ((double) mAcceleration[i].x * (double) filter->x
					+ (double) mAcceleration[i].y * (double) filter->y
					+ (double) mAcceleration[i].z * (double) filter->z) / (double) norm((int16_t) filter->x, (int16_t) filter->y, (int16_t) filter->z);
			double h = (double) wdSqrt((uint32_t)
					((double) mAcceleration[i].x * (double) mAcceleration[i].x
					+ (double) mAcceleration[i].y * (double) mAcceleration[i].y
					+ (double) mAcceleration[i].z * (double) mAcceleration[i].z - v * v));

			feature.meanV += v;
			feature.meanH += h;
			feature.deviationV += v * v;
			feature.deviationH += h * h;

			// Reuse it for later step counter
			mAcceleration[i].x = v;
			mAcceleration[i].y = h;
			if (mAcceleration[i].x > maxV)
				maxV = mAcceleration[i].x;
			if (mAcceleration[i].x < minV)
				minV = mAcceleration[i].x;
		}

		feature.meanV /= (mDataSize - 1);
		feature.meanH /= (mDataSize - 1);
		feature.deviationV = feature.deviationV / (mDataSize - 1) - feature.meanV * feature.meanV;
		feature.deviationH = feature.deviationH / (mDataSize - 1) - feature.meanH * feature.meanH;

		// Classification
		*currentType = classify(feature);

		APP_LOG(APP_LOG_LEVEL_INFO, "%d %d %d %d: %d", (int) feature.meanV, (int) feature.meanH, (int) feature.deviationV, (int) feature.deviationH, (int) *currentType);

		// Update
		if (*currentType > 1 && isDriving) {
			// If the user is driving, then, activity type is "sitting"
			*currentType = 1;
		}

		// Update time
		uint32_t timestamp = (uint32_t) time(NULL);
		uint32_t elapsedTime = timestamp - counter->timestamp;

		switch(*currentType) {
			case 0:
				counter->sleepTime += elapsedTime;
				break;
			case 1:
				counter->sitTime += elapsedTime;
				break;
			case 2:
				counter->walkTime += elapsedTime;
				break;
			case 3:
				counter->jogTime += elapsedTime;
				break;
		}
		counter->timestamp = timestamp;

		// Count steps
		uint32_t steps = 0;
		if (*currentType > 1) {	// Walking or Jogging
			int direction = 0;

			double ratio = 0.0;
			if (*currentType == 2) {	// Only filer steps for walking, but NOT for jogging!
				ratio = (double) sensitivity / 100.0;
			}

			for (uint32_t i = 0; i < SAMPLE_SIZE; i++) {
				if (mAcceleration[i].x > feature.meanV + (maxV - feature.meanV) * ratio) {
					if (direction == -1)
						steps++;
					direction = 1;
				} else if (mAcceleration[i].x < feature.meanV + (minV - feature.meanV) * ratio) {
					if (direction == 1)
						steps++;
					direction = -1;
				}
			}
			steps /= 4;

			if (*currentType == 2) {
				if (steps > elapsedTime * MAX_WALKING_SPEED) {	// Driving may be recognized as walking. Fix it.
					*currentType = 1;
					counter->sitTime += elapsedTime;
					counter->walkTime -= elapsedTime;
					steps = 0;
				}
			}

			counter->steps += steps;
		}
		
		// Clean up for next round
		// Sliding window: move the latter half to the front
		for (uint32_t i = 0; i < SAMPLE_SIZE / 2; i++) {
			mAcceleration[i].x = mAcceleration[SAMPLE_SIZE / 2 + i].x;
			mAcceleration[i].y = mAcceleration[SAMPLE_SIZE / 2 + i].y;
			mAcceleration[i].z = mAcceleration[SAMPLE_SIZE / 2 + i].z;
		}
		mDataSize = SAMPLE_SIZE / 2;
		return 0;
	}
}
