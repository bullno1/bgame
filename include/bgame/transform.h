#ifndef BGAME_TRANSFORM_H
#define BGAME_TRANSFORM_H

#include <cute_math.h>

/// Transform in decomposed components
typedef struct bgame_transform_s {
	CF_V2 translation;
	CF_V2 scale;
	float rotation;
} bgame_transform_t;

static inline bgame_transform_t
bgame_transform_identity(void) {
	return (bgame_transform_t){
		.translation = { 0.f, 0.f },
		.scale = { 1.f, 1.f },
		.rotation = 0.f,
	};
}

static inline CF_M3x2
bgame_lerp_transform(bgame_transform_t previous, bgame_transform_t current, float t) {
	CF_V2 translation = cf_lerp(previous.translation, current.translation, t);
	CF_V2 scale = cf_lerp(previous.scale, current.scale, t);
	float rotation = cf_lerp( previous.rotation, current.rotation, t);
	return cf_make_transform_TSR(translation, scale, rotation);
}

#endif
