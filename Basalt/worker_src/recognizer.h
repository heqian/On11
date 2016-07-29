#ifndef _RECOGNIZER_H_
#define _RECOGNIZER_H_

#include <pebble_worker.h>
#include "utility.h"
#include "lowpassfilter.h"
#include "classifier.h"

#define SAMPLE_INTERVAL_S	8
#define BATCH_SIZE			10
#define SAMPLE_SIZE			(SAMPLE_INTERVAL_S * BATCH_SIZE)
#define MAX_WALKING_SPEED	3	// 3 steps per second


uint32_t analyzeAcceleration(uint32_t* currentType, Counter* counter, LowPassFilter* filter, int32_t sensitivity, AccelData* acceleration, uint32_t size);

#endif
