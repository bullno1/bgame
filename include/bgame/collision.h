#ifndef BGAME_COLLISION_H
#define BGAME_COLLISION_H

#include <cute_math.h>

typedef struct {
	CF_ShapeType type;

	union {
		CF_Poly poly;
		CF_Circle circle;
		CF_Aabb aabb;
		CF_Capsule capsule;
	} data;
} bgame_collision_shape_t;

bgame_collision_shape_t
bgame_transform_collision_shape(const bgame_collision_shape_t* shape, CF_M3x2 transform);

CF_Aabb
bgame_make_aabb_from_shape(const bgame_collision_shape_t* shape, CF_M3x2 transform);

#endif
