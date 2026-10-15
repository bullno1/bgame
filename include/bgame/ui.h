#ifndef BGAME_UI_H
#define BGAME_UI_H

#include <clay.h>
#include <bgame/asset/font.h>
#include <cute_color.h>
#include <bfmt.h>
#include <bgame/asset/translation.h>

/*! The language the source text is written in, for its plural rules */
#ifndef BGAME_UI_SOURCE_LANGUAGE
#define BGAME_UI_SOURCE_LANGUAGE "en"
#endif

#define BUI(...) CLAY(__VA_ARGS__)

#define BUI_GEN_ID() ((Clay_ElementId){ 0 })

#define BGAME_UI_STATE_TAG(TYPE) struct { TYPE bgame_ui__type; }

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

typedef float (*bgame_ui_transition_curve_fn_t)(float progress);

typedef struct {
	bgame_ui_transition_curve_fn_t curve;
	bgame_ui_transition_curve_fn_t enter_curve;
	bgame_ui_transition_curve_fn_t exit_curve;
} bgame_ui_transition_config_t;

void
bgame_ui_begin(void);

Clay_RenderCommandArray
bgame_ui_end(void);

/**
 * The language UI text is translated to, NULL for the source text.
 *
 * The translation is referenced, not copied: unload it only after setting
 * another.
 */
void
bgame_ui_set_translation(bgame_translation_t* translation);

bgame_translation_t*
bgame_ui_translation(void);

/*! The current locale */
bfmt_locale_t*
bgame_ui_locale(void);

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

/**
 * Retrieve transient UI state
 *
 * The state is keyed by @p tag and the current open element's id.
 * If the state was not found, it will be allocated and zero initialized.
 * The same value will be returned the next frame when this function is called
 * again.
 *
 * The state is arena-allocated so it does not have a stable address.
 * It will be copied from frame to frame.
 *
 * @param size How big the state is
 * @param alignment The alignment of the state type
 * @param tag An arbitrary pointer to differentiate states of the same elements
 * @return the transient UI state
 */
void*
bgame_ui_state(size_t size, size_t alignment, const void* tag);

#define bgame_ui_state(TAG) (typeof(TAG.bgame_ui__type)*)bgame_ui_state(sizeof(TAG.bgame_ui__type), _Alignof(typeof(TAG.bgame_ui__type)), &TAG)

Clay_CustomElementConfig
bgame_custom_ui_element(
	bgame_custom_ui_render_fn_t render_fn,
	void* userdata
);

bool
bgame_ui_transition(Clay_TransitionCallbackArguments arguments);

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

static inline Clay_Color
bgame_ui_color_rgb(uint8_t r, uint8_t g, uint8_t b) {
	return bgame_ui_color_from_cf(cf_make_color_rgb(r, g, b));
}

static inline Clay_Color
bgame_ui_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	return bgame_ui_color_from_cf(cf_make_color_rgba(r, g, b, a));
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
