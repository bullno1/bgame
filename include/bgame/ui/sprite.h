#ifndef BGAME_UI_SPRITE_H
#define BGAME_UI_SPRITE_H

#include "../ui.h"
#include "../asset/sprite.h"

typedef enum {
	BGAME_UI_SPRITE_STRETCH,
	BGAME_UI_SPRITE_9_SLICE,
	BGAME_UI_SPRITE_9_SLICE_TILED,
} bgame_ui_sprite_mode_t;

typedef struct {
	struct CF_Sprite* sprite;
	bgame_ui_sprite_mode_t mode;
} bgame_ui_sprite_config_t;

Clay_CustomElementConfig
bgame_ui_sprite(bgame_ui_sprite_config_t config);

#endif
