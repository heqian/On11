#ifndef LOWPASSFILTER_H
#define LOWPASSFILTER_H
#include <pebble.h>

typedef struct {
	double kAccelerometerMinStep;
	double kAccelerometerNoiseAttenuation;
	double filterConstant;
	int16_t x;
	int16_t y;
	int16_t z;
} LowPassFilter;

uint32_t wdSqrt(uint32_t n);
uint32_t norm(int16_t x, int16_t y, int16_t z);
double clamp(double v, double min, double max);
void initLowPassFilter(LowPassFilter* filter);
void goThroughFilter(LowPassFilter* filter, int16_t x, int16_t y, int16_t z);

#endif
