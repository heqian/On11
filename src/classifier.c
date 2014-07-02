#include "classifier.h"

// It's not magic. It's not Math. It's a model trained with real data -- data collected from a poor experimental subject: me...

uint32_t classify(Feature feature) {
	uint32_t type = 0;
	double probability = 0.0;

	double class0 = 6.95 +
		feature.meanV * 0.87 +
		feature.meanH * -0.26 +
		feature.deviationV * -0.03 +
		feature.deviationH * -0.11;
	probability = class0;
	type = 0;

	double class1 = 2.7 +
		feature.meanV * 0.05 +
		feature.meanH * -0.05 +
		feature.deviationV * 0 +
		feature.deviationH * 0;
	if (class1 > probability) {
		probability = class1;
		type = 1;
	}

	double class2 = -3.73 +
		feature.meanV * -0.16 +
		feature.meanH * 0.1  +
		feature.deviationV * 0 +
		feature.deviationH * 0;
	if (class2 > probability) {
		probability = class2;
		type = 2;
	}

	double class3 = -65.76 +
		feature.meanH * 0.31 +
		feature.deviationV * 0 +
		feature.deviationH * 0;
	if (class3 > probability) {
		probability = class3;
		type = 3;
	}

	return type;
}
