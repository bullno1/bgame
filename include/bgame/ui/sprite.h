#ifndef BGAME_UI_SPRITE_H
#define BGAME_UI_SPRITE_H

#include "../ui.h"
#include "../asset/sprite.h"
#include "../draw.h"

struct CF_Sprite;

typedef enum {
	BGAME_UI_SPRITE_STRETCH,
	BGAME_UI_SPRITE_9_SLICE,
	BGAME_UI_SPRITE_9_SLICE_TILED,
	// The frame repeated over the element, see bgame_draw_sprite_tiled.
	BGAME_UI_SPRITE_TILED,
} bgame_ui_sprite_mode_t;

typedef struct {
	const struct CF_Sprite* sprite;
	bgame_ui_sprite_mode_t mode;
	// TILED only. `origin` is in pixels from the element's top-left, y down;
	// a positive rotation turns counter-clockwise on screen.
	bgame_tiling_t tiling;
} bgame_ui_sprite_config_t;

/*! Content drawing a sprite over the element's box */
bgame_ui_content_t
bgame_ui_sprite(bgame_ui_sprite_config_t config);

/*! A sprite instance playing an animation, kept as state of the open scope */
struct CF_Sprite*
bgame_ui_sprite_at_anim(const struct CF_Sprite* sprite, const char* animation);

#define BUI_SPRITE(...) bgame_ui_sprite(((bgame_ui_sprite_config_t[1]){ __VA_OPT__([0] = ) __VA_ARGS__ })[0])

#endif
