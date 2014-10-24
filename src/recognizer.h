#ifndef RECOGNIZER_H
#define RECOGNIZER_H
#include <pebble.h>
#include "lowpassfilter.h"
#include "classifier.h"

#define SAMPLE_INTERVAL_S	8
#define BATCH_SIZE			10
#define SAMPLE_SIZE			(SAMPLE_INTERVAL_S * BATCH_SIZE)

typedef struct {
	uint32_t sleepTime;
	uint32_t sitTime;
	uint32_t walkTime;
	uint32_t jogTime;
	uint32_t timestamp;
	uint32_t steps;
} Counter;

bool analyzeAcceleration(uint32_t* currentType, Counter* counter, LowPassFilter* filter, bool isDriving, int32_t sensitivity, AccelData* acceleration, uint32_t size);

#endif
