#ifndef BGAME_ASSET_COLLISION_SHAPE_H
#define BGAME_ASSET_COLLISION_SHAPE_H

#include <cute_math.h>

struct bgame_asset_bundle_s;

typedef struct {
	CF_ShapeType type;

	union {
		CF_Poly poly;
		CF_Circle circle;
		CF_Aabb aabb;
		CF_Capsule capsule;
	};
} bgame_collision_shape_t;

bgame_collision_shape_t*
bgame_load_collision_shape(struct bgame_asset_bundle_s* bundle, const char* path);

#endif
