#ifndef BGAME_UI_H
#define BGAME_UI_H

#include "clay.h"
#include <bgame/asset/font.h>
#include <cute_color.h>

#define BUI(...) CLAY(__VA_ARGS__)

#define BUI_GEN_ID() ((Clay_ElementId){ 0 })

struct bgame_asset_bundle_s;

typedef void (*bgame_custom_ui_render_fn_t)(
	const Clay_RenderCommand* command,
	void* userdata
);

typedef enum {
	BGAME_UI_ENABLE_TEXT_EFFECT,
	BGAME_UI_DISABLE_TEXT_EFFECT,
} bgame_ui_text_effect_state_t;

typedef struct {
	bgame_ui_text_effect_state_t effect;
	int blur;
} bgame_ui_text_config_t;

void
bgame_update_ui(void);

void
bgame_register_ui_font(uint16_t id, const char* name);

const char*
bgame_get_ui_font_name(uint16_t id) ;

void
bgame_begin_render_ui(void);

void
bgame_handle_ui_render_command(const Clay_RenderCommand* command);

void
bgame_end_render_ui(void);

void*
bgame_ui_text_config(bgame_ui_text_config_t config);

Clay_CustomElementConfig
bgame_custom_ui_element(
	bgame_custom_ui_render_fn_t render_fn,
	void* userdata
);

static inline void
bgame_load_ui_font(
	struct bgame_asset_bundle_s* bundle,
	uint16_t id,
	const char* path
) {
	bgame_register_ui_font(id, bgame_load_font(bundle, path));
}

static inline void
bgame_render_ui(Clay_RenderCommandArray commands) {
	bgame_begin_render_ui();
	for (int i = 0; i < commands.length; i++) {
		const Clay_RenderCommand* command = &commands.internalArray[i];
		bgame_handle_ui_render_command(command);
	}
	bgame_end_render_ui();
}

static inline Clay_Color
bgame_ui_color_from_cf(CF_Color color) {
	return (Clay_Color){
		.a = color.a,
		.r = color.r,
		.g = color.g,
		.b = color.b,
	};
}

static inline CF_Color
bgame_ui_color_from_clay(Clay_Color color) {
	return (CF_Color) {
		.a = color.a <= 1.f ? color.a : color.a / 255.f,
		.r = color.r <= 1.f ? color.r : color.r / 255.f,
		.g = color.g <= 1.f ? color.g : color.g / 255.f,
		.b = color.b <= 1.f ? color.b : color.b / 255.f,
	};
}

static inline CF_Aabb
bgame_ui_aabb(Clay_BoundingBox bbox) {
	return (CF_Aabb){
		.min = {
			.x = bbox.x,
			.y = -(bbox.y + bbox.height),
		},
		.max = {
			.x = bbox.x + bbox.width,
			.y = -bbox.y,
		},
	};
}

#endif
