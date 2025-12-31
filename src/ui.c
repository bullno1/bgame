#include "internal.h"
#include <bgame/ui.h>
#include <bgame/reloadable.h>
#include <bgame/allocator.h>
#include <bgame/allocator/frame.h>
#include <blog.h>
#include <bminmax.h>
#include <cute_app.h>
#include <cute_input.h>
#include <cute_time.h>
#include <cute_draw.h>

typedef struct {
	uint16_t capacity;
	const char* font_names[];
} bgame_ui_font_list_t;

typedef struct {
	void* userdata;
	void (*render)(const Clay_RenderCommand* command, void* userdata);
} bgame_custom_ui_element_t;

BGAME_VAR(void*, bgame_ui_mem) = NULL;
BGAME_VAR(bgame_ui_font_list_t*, bgame_ui_fonts) = NULL;

static void
bgame_ui_error(Clay_ErrorData errorText) {
	BLOG_ERROR("%.*s", errorText.errorText.length, errorText.errorText.chars);
}

const char*
bgame_get_ui_font_name(uint16_t id) {
	if (bgame_ui_fonts == NULL) { return "Calibri"; }
	if (id >= bgame_ui_fonts->capacity) { return "Calibri"; }

	return bgame_ui_fonts->font_names[id];
}

static void
bgame_ui_begin_text(const Clay_TextElementConfig* config) {
	const char* font_name = bgame_get_ui_font_name(config->fontId);
	cf_push_font(font_name);
	cf_push_font_size(config->fontSize);
	cf_draw_push_color(bgame_ui_color_from_clay(config->textColor));

	bgame_ui_text_config_t extra_config = { 0 };
	if (config->userData != NULL) {
		extra_config = *(bgame_ui_text_config_t*)config->userData;
	}
	cf_push_text_effect_active(extra_config.effect == BGAME_UI_ENABLE_TEXT_EFFECT);
	cf_push_font_blur(extra_config.blur);
}

static void
bgame_ui_end_text(void) {
	cf_pop_font_blur();
	cf_pop_text_effect_active();
	cf_draw_pop_color();
	cf_pop_font_size();
	cf_pop_font();
}

static Clay_Dimensions
bgame_ui_measure_text(
	Clay_StringSlice text,
	Clay_TextElementConfig* config,
	void* userdata
) {
	bgame_ui_begin_text(config);
	CF_V2 size = cf_text_size(text.chars, text.length);
	bgame_ui_end_text();

	return (Clay_Dimensions){ .width = size.x, .height = size.y };
}

static void
bgame_ui_init(void) {
	size_t mem_size = Clay_MinMemorySize();
	if (bgame_ui_mem == NULL) {
		bgame_ui_mem = bgame_malloc(mem_size, bgame_default_allocator);
	}

	Clay_Initialize(
		Clay_CreateArenaWithCapacityAndMemory(mem_size, bgame_ui_mem),
		(Clay_Dimensions){ .width = 1, .height = 1 },  // We can't call cf_app_get_size yet
		(Clay_ErrorHandler){
			.errorHandlerFunction = bgame_ui_error,
		}
	);
	Clay_SetMeasureTextFunction(bgame_ui_measure_text, NULL);
}

static void
bgame_ui_cleanup(void) {
	bgame_free(bgame_ui_mem, bgame_default_allocator);
	bgame_free(bgame_ui_fonts, bgame_default_allocator);
}

BGAME_ON_LOAD(bgame_ui) {
	bgame_ui_init();
}

BGAME_AFTER_RELOAD(bgame_ui) {
	bgame_ui_init();
}

BGAME_ON_UNLOAD(bgame_ui) {
	bgame_ui_cleanup();
}

void
bgame_update_ui(void) {
	int width, height;
	cf_app_get_size(&width, &height);
	Clay_SetLayoutDimensions((Clay_Dimensions){ (float)width, (float)height });

	Clay_SetPointerState(
		(Clay_Vector2){ cf_mouse_x(), cf_mouse_y() },
		cf_mouse_down(CF_MOUSE_BUTTON_LEFT)
	);

	Clay_UpdateScrollContainers(
		true,
		(Clay_Vector2){ .y = cf_mouse_wheel_motion() },
		CF_DELTA_TIME
	);
}

void
bgame_register_ui_font(uint16_t id, const char* name) {
	const char* default_font = sintern("Calibri");

	uint16_t old_capacity = bgame_ui_fonts != NULL ? bgame_ui_fonts->capacity : 0;
	if (id >= old_capacity) {
		uint16_t new_capacity = BMAX(BMAX(old_capacity, 1) * 2, id);
		bgame_ui_fonts = bgame_realloc(
			bgame_ui_fonts,
			sizeof(bgame_ui_font_list_t) + new_capacity * sizeof(const char*),
			bgame_default_allocator
		);
		bgame_ui_fonts->capacity = new_capacity;
		for (uint16_t i = old_capacity; i < new_capacity; ++i) {
			bgame_ui_fonts->font_names[i] = default_font;
		}
	}

	bgame_ui_fonts->font_names[id] = name != NULL ? name : default_font;
}

void
bgame_handle_ui_render_command(const Clay_RenderCommand* command) {
	switch (command->commandType) {
		case CLAY_RENDER_COMMAND_TYPE_NONE:
			break;
		case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
			cf_draw_push_color(
				bgame_ui_color_from_clay(
					command->renderData.rectangle.backgroundColor
				)
			);
			cf_draw_box_rounded_fill(
				bgame_ui_aabb(command->boundingBox),
				command->renderData.rectangle.cornerRadius.topLeft
			);
			cf_draw_pop_color();
		} break;
		case CLAY_RENDER_COMMAND_TYPE_BORDER: {
			Clay_BorderRenderData border = command->renderData.border;
			CF_Aabb aabb = bgame_ui_aabb(command->boundingBox);
			cf_draw_push_color(bgame_ui_color_from_clay(border.color));
			if (
				border.width.top == border.width.left
				&&
				border.width.top == border.width.right
				&&
				border.width.top == border.width.bottom
				&&
				border.cornerRadius.topLeft == border.cornerRadius.topRight
				&&
				border.cornerRadius.topLeft == border.cornerRadius.bottomRight
				&&
				border.cornerRadius.topLeft == border.cornerRadius.bottomLeft
			) {
				cf_draw_box_rounded(
					aabb,
					(float)border.width.top * 0.5f,
					border.cornerRadius.topLeft
				);
			} else {
				if (border.width.top > 0) {
					cf_draw_line(
						cf_v2(
							aabb.min.x + border.cornerRadius.topLeft,
							aabb.max.y
						),
						cf_v2(
							aabb.max.x - border.cornerRadius.topRight,
							aabb.max.y
						),
						(float)border.width.top * 0.5f
					);
				}

				if (border.width.bottom > 0) {
					cf_draw_line(
						cf_v2(
							aabb.min.x + border.cornerRadius.bottomLeft,
							aabb.min.y
						),
						cf_v2(
							aabb.max.x - border.cornerRadius.bottomRight,
							aabb.min.y
						),
						(float)border.width.bottom * 0.5f
					);
				}

				if (border.width.left > 0) {
					cf_draw_line(
						cf_v2(
							aabb.min.x,
							aabb.max.y - border.cornerRadius.topLeft
						),
						cf_v2(
							aabb.min.x,
							aabb.min.y + border.cornerRadius.bottomLeft
						),
						(float)border.width.left * 0.5f
					);
				}

				if (border.width.right > 0) {
					cf_draw_line(
						cf_v2(
							aabb.max.x,
							aabb.max.y - border.cornerRadius.topRight
						),
						cf_v2(
							aabb.max.x,
							aabb.min.y + border.cornerRadius.bottomRight
						),
						(float)border.width.right * 0.5f
					);
				}
			}
			cf_draw_pop_color();
		} break;
		case CLAY_RENDER_COMMAND_TYPE_TEXT: {
			bgame_ui_begin_text(&(Clay_TextElementConfig){
				.fontId = command->renderData.text.fontId,
				.fontSize = command->renderData.text.fontSize,
				.textColor = command->renderData.text.textColor,
				.userData = command->userData,
			});
			cf_draw_text(
				command->renderData.text.stringContents.chars,
				(CF_V2){ command->boundingBox.x, -command->boundingBox.y },
				command->renderData.text.stringContents.length
			);
			bgame_ui_end_text();
		} break;
		case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
			CF_Sprite sprite = *(CF_Sprite*)command->renderData.image.imageData;

			CF_V2 pivot = sprite.pivots[sprite.frame_index];
			sprite.scale.x = command->boundingBox.width / (float)sprite.w;
			sprite.scale.y = command->boundingBox.height / (float)sprite.h;
			sprite.transform.p.x = command->boundingBox.x - pivot.x + (float)sprite.w * sprite.scale.x * 0.5f;
			sprite.transform.p.y = -command->boundingBox.y - pivot.y - (float)sprite.h * sprite.scale.y * 0.5f;

			cf_draw_sprite(&sprite);
		} break;
		case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
			cf_draw_push_scissor((CF_Rect){
				.x = command->boundingBox.x,
				.y = command->boundingBox.y,
				.w = command->boundingBox.width,
				.h = command->boundingBox.height,
			});
		} break;
		case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
			cf_draw_pop_scissor();
		} break;
		case CLAY_RENDER_COMMAND_TYPE_CUSTOM: {
			bgame_custom_ui_element_t* element = command->renderData.custom.customData;
			if (element != NULL) {
				element->render(command, element->userdata);
			}
		} break;
	}
}

void
bgame_begin_render_ui(void) {
	int w, h;
	cf_app_get_size(&w, &h);

	float half_width = w * 0.5f;
	float half_height = h * 0.5f;
	cf_draw_push();
	cf_draw_translate(-half_width, half_height);
}

void
bgame_end_render_ui(void) {
	cf_draw_pop();
}

Clay_CustomElementConfig
bgame_custom_ui_element(
	bgame_custom_ui_render_fn_t render_fn,
	void* userdata
) {
	bgame_custom_ui_element_t element = {
		.render = render_fn,
		.userdata = userdata,
	};
	return (Clay_CustomElementConfig){
		.customData = bgame_make_frame_copy(element),
	};
}

void*
bgame_ui_text_config(bgame_ui_text_config_t config) {
	return bgame_make_frame_copy(config);
}
