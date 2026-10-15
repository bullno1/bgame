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
	struct CF_Sprite* sprite;
	bgame_ui_sprite_mode_t mode;
	// TILED only. `origin` is in pixels from the element's top-left, y down;
	// a positive rotation turns counter-clockwise on screen.
	bgame_tiling_t tiling;
} bgame_ui_sprite_config_t;

Clay_CustomElementConfig
bgame_ui_sprite(bgame_ui_sprite_config_t config);

struct CF_Sprite*
bgame_ui_sprite_at_anim(const struct CF_Sprite* sprite, const char* animation);

#endif
