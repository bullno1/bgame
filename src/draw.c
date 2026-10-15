#include "internal.h"
#include <bgame/draw.h>
#include <bgame/reloadable.h>
#include <bgame/shader.h>
#include <cute_draw.h>
#include <cute_sprite.h>
#include <math.h>
#include "draw/gen/sprite_tiled_shd.h"

BGAME_VAR(CF_Shader, bgame_sprite_tiled_shader) = { 0 };

BGAME_AFTER_RELOAD(bgame_draw) {
	if (bgame_sprite_tiled_shader.id != 0) {
		bgame_load_draw_shader(&bgame_sprite_tiled_shader, bgame_sprite_tiled_shd_bytecode);
	}
}

void
bgame_draw_sprite_tiled(const CF_Sprite* sprite, CF_Aabb area, bgame_tiling_t tiling) {
	float scale = tiling.scale > 0.f ? tiling.scale : 1.f;
	float c = cosf(tiling.rotation);
	float s = sinf(tiling.rotation);

	// Keep the origin within one tile period so a scroll that runs for hours
	// stays precise; the pattern cannot tell.
	CF_V2 period = cf_v2((float)sprite->w * scale, (float)sprite->h * scale);
	CF_V2 origin = tiling.origin;
	CF_V2 local = cf_v2(c * origin.x + s * origin.y, -s * origin.x + c * origin.y);
	local = cf_v2(fmodf(local.x, period.x), fmodf(local.y, period.y));
	origin = cf_v2(c * local.x - s * local.y, s * local.x + c * local.y);

	// The quad is the area, axis aligned; the pattern turns in the shader.
	CF_Sprite quad = *sprite;
	quad.transform = cf_make_transform_TR(cf_center(area), 0.f);
	quad.scale = cf_v2(cf_width(area) / (float)quad.w, cf_height(area) / (float)quad.h);
	quad.offset = cf_v2(0.f, 0.f);

	if (bgame_sprite_tiled_shader.id == 0) {
		bgame_load_draw_shader(&bgame_sprite_tiled_shader, bgame_sprite_tiled_shd_bytecode);
	}
	cf_draw_push_shader(bgame_sprite_tiled_shader);
	// A soft edge would tile the atlas's transparent ring into every repeat.
	cf_draw_push_sprite_edge(CF_SPRITE_EDGE_HARD);
	cf_draw_push_vertex_attributes(origin.x, origin.y, c / scale, s / scale);
	cf_draw_sprite(&quad);
	cf_draw_pop_vertex_attributes();
	cf_draw_pop_sprite_edge();
	cf_draw_pop_shader();
}
