#ifndef BGAME_EASING_H
#define BGAME_EASING_H

#include <math.h>
#include <cute_math.h>
#include <bminmax.h>

typedef float (*bgame_easing_fn_t)(float from, float to, float factor);

static inline float
bgame_easing_linear(float from, float to, float factor) {
	return from + (to - from) * factor;
}

static inline float
bgame_easing_cubic_out(float from, float to, float factor) {
	return bgame_easing_linear(from, to, factor * factor * factor);
}

static inline float
bgame_easing_cubic_inout(float from, float to, float factor) {
	return bgame_easing_linear(
		from, to,
		factor < 0.5f
			? 4.f * factor * factor * factor
			: 1.f - powf(-2.f * factor + 2.f, 3.f) / 2.f
	);
}

static inline float
bgame_easing_sine_out(float from, float to, float factor) {
	return bgame_easing_linear(
		from, to,
		1.f - cosf((factor * CF_PI) / 2.f)
	);
}

static inline float
bgame_easing_sine_inout(float from, float to, float factor) {
	return bgame_easing_linear(
		from, to,
		-(cosf(CF_PI * factor) - 1.0) * 0.5f
	);
}

// x is factor, y is point
static inline float
bgame_easing_path(
	CF_V2 path[],
	int num_points,
	bgame_easing_fn_t easing_fn,
	float factor
) {
	int i;
	for (i = 0; i < num_points; ++i) {
		if (factor <= path[i].x) { break; }
	}

	float from = path[BCLAMP(i - 1, 0, num_points - 1)].y;
	float to   = path[BCLAMP(i    , 0, num_points - 1)].y;
	return easing_fn(from, to, factor);
}

#endif
