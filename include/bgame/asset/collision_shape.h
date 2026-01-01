#ifndef BGAME_ASSET_COLLISION_SHAPE_H
#define BGAME_ASSET_COLLISION_SHAPE_H

#include <cute_math.h>
#include <bgame/collision.h>

struct bgame_asset_bundle_s;

bgame_collision_shape_t*
bgame_load_collision_shape(struct bgame_asset_bundle_s* bundle, const char* path);

#endif
