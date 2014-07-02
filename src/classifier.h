#ifndef CLASSIFIER_H

#include <pebble.h>

typedef struct {
	double meanH;
	double meanV;
	double deviationH;
	double deviationV;
} Feature;

uint32_t classify(Feature feature);

#endif
