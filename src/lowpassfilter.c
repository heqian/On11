#include "lowpassfilter.h"

#define iter1(N) \
    try = root + (1 << (N)); \
    if (n >= try << (N))   \
    {   n -= try << (N);   \
        root |= 2 << (N); \
    }

uint32_t wdSqrt(uint32_t n) {
	uint32_t root = 0, try;
	iter1 (15);    iter1 (14);    iter1 (13);    iter1 (12);
	iter1 (11);    iter1 (10);    iter1 ( 9);    iter1 ( 8);
	iter1 ( 7);    iter1 ( 6);    iter1 ( 5);    iter1 ( 4);
	iter1 ( 3);    iter1 ( 2);    iter1 ( 1);    iter1 ( 0);
	return root >> 1;
}


double clamp(double v, double min, double max) {
	if (v > max)
		return max;
	else if (v < min)
		return min;
	else
		return v;
}


uint32_t norm(int16_t x, int16_t y, int16_t z) {
	return wdSqrt(
		  (uint32_t) x * (uint32_t) x
		+ (uint32_t) y * (uint32_t) y
		+ (uint32_t) z * (uint32_t) z);
}


void initLowPassFilter(LowPassFilter* filter) {
	double rate = 100.0;
	double freq = 100.0;
	double dt = 1.0 / rate;
	double RC = 1.0 / freq;

	filter->filterConstant = dt / (dt + RC);
	filter->kAccelerometerMinStep = 0.02;
	filter->kAccelerometerNoiseAttenuation = 3.0;
	filter->x = 0;
	filter->y = 0;
	filter->z = 0;
}


void goThroughFilter(LowPassFilter* filter, int16_t x, int16_t y, int16_t z) {
	double alpha = filter->filterConstant;

	double d = clamp(
		abs(norm(filter->x, filter->y, filter->z) - norm(x, y, z)) / filter->kAccelerometerMinStep - 1.0,
		0.0,
		1.0
	);
	alpha = (1.0 - d) * filter->filterConstant / filter->kAccelerometerNoiseAttenuation + d * filter->filterConstant;

	filter->x = x * alpha + filter->x * (1.0 - alpha);
	filter->y = y * alpha + filter->y * (1.0 - alpha);
	filter->z = z * alpha + filter->z * (1.0 - alpha);
}
