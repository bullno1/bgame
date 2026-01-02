#ifndef BGAME_ASSET_COLLISION_SHAPE_H
#define BGAME_ASSET_COLLISION_SHAPE_H

#include <bgame/collision.h>
#include <bgame/asset.h>

bgame_collision_shape_t*
bgame_load_collision_shape(bgame_asset_bundle_t* bundle, const char* path);

extern bgame_asset_type_t collision_shape;

#define BGAME_DEFINE_COLLISION_SHAPE(NAME) \
	BGAME_DEFINE_ASSET(NAME, collision_shape, bgame_collision_shape_t*)

#endif
