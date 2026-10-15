#ifndef BGAME_DRAW_H
#define BGAME_DRAW_H

#include <cute_math.h>

struct CF_Sprite;

// One sprite frame repeated over the plane: a tile corner sits at `origin`, the
// grid is turned by `rotation` radians about it, and `scale` is draw units per
// texel (0 means 1).
typedef struct {
	CF_V2 origin;
	float rotation;
	float scale;
} bgame_tiling_t;

// Fills `area` with the sprite's current frame repeated per `tiling`.
// `area` and `tiling.origin` are in the current draw space; the sprite's own
// transform, scale and offset are ignored.
void
bgame_draw_sprite_tiled(const struct CF_Sprite* sprite, CF_Aabb area, bgame_tiling_t tiling);

#endif
