#ifndef BGAME_ASSET_9PATCH_H
#define BGAME_ASSET_9PATCH_H

#include <cute_math.h>

struct bgame_asset_bundle_s;

typedef struct {
	// The size of the border in each direction
	int left, right, top, bottom;
} bgame_9patch_config_t;

typedef struct bgame_9patch_s bgame_9patch_t;

bgame_9patch_t*
bgame_load_9patch(
	struct bgame_asset_bundle_s* bundle,
	const char* path,
	bgame_9patch_config_t config
);

void
bgame_draw_9patch(const bgame_9patch_t* nine_patch, CF_Aabb aabb);

#endif
