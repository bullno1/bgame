#include <bgame/ui/rich_text.h>
#include <bgame/allocator/frame.h>
#include <cute_draw.h>

static void
bgame_ui_push_text_config(const bgame_ui_rich_text_t* config, float wrap_width) {
	cf_push_font(config->font->name);
	cf_push_font_size(config->font_size);
	if (config->sizing == BGAME_UI_RICH_TEXT_WRAP) {
		cf_push_text_wrap_width(wrap_width);
	}
}

static void
bgame_ui_pop_text_config(const bgame_ui_rich_text_t* config) {
	if (config->sizing == BGAME_UI_RICH_TEXT_WRAP) {
		cf_pop_text_wrap_width();
	}
	cf_pop_font_size();
	cf_pop_font();
}

static void
bgame_ui_draw_text_pass(
	const bgame_ui_rich_text_t* rich_text,
	CF_V2 pos,
	Clay_Color color,
	float stroke,
	int blur
) {
	cf_push_text_stroke(stroke);
	cf_push_font_blur(blur);
	cf_draw_push_color(bgame_ui_color_from_clay(color));
	cf_draw_text(rich_text->text.chars, pos, rich_text->text.len);
	cf_draw_pop_color();
	cf_pop_font_blur();
	cf_pop_text_stroke();
}

static void
bgame_ui_render_rich_text(
	const Clay_RenderCommand* command,
	void* userdata
) {
	const bgame_ui_rich_text_t* rich_text = userdata;
	const float* wrap_width = command->userData;
	const bgame_ui_text_outline_t* outline = &rich_text->outline;
	const bgame_ui_text_shadow_t* shadow = &rich_text->shadow;
	CF_V2 pos = { command->boundingBox.x, -command->boundingBox.y };
	bool outlined = outline->color.a > 0.f && outline->thickness > 0.f;

	bgame_ui_push_text_config(rich_text, *wrap_width);
	cf_push_text_id(command->id);

	if (shadow->color.a > 0.f) {
		CF_V2 shadow_pos = { pos.x + shadow->offset.x, pos.y - shadow->offset.y };
		// CF cannot blur a stroke, so a soft shadow is the fill alone
		if (outlined && shadow->blur == 0) {
			bgame_ui_draw_text_pass(rich_text, shadow_pos, shadow->color, outline->thickness, 0);
		}
		bgame_ui_draw_text_pass(rich_text, shadow_pos, shadow->color, 0.f, shadow->blur);
	}
	if (outlined) {
		bgame_ui_draw_text_pass(rich_text, pos, outline->color, outline->thickness, 0);
	}
	bgame_ui_draw_text_pass(rich_text, pos, rich_text->color, 0.f, 0);

	cf_pop_text_id();
	bgame_ui_pop_text_config(rich_text);
}

void
bgame_ui_rich_text(
	Clay_ElementId id,
	bgame_ui_rich_text_t rich_text
) {
	Clay_LayoutAlignmentX align_x;
	switch (rich_text.justify) {
		case BGAME_UI_RICH_TEXT_JUSTIFY_LEFT:
			align_x = CLAY_ALIGN_X_LEFT;
			break;
		case BGAME_UI_RICH_TEXT_JUSTIFY_CENTER:
			align_x = CLAY_ALIGN_X_CENTER;
			break;
		case BGAME_UI_RICH_TEXT_JUSTIFY_RIGHT:
			align_x = CLAY_ALIGN_X_RIGHT;
			break;
	}
	BUI(id, {
		.layout = {
			.sizing = {
				.width = rich_text.sizing == BGAME_UI_RICH_TEXT_WRAP
					? CLAY_SIZING_GROW(0)
					: CLAY_SIZING_FIT(0),
				.height = CLAY_SIZING_FIT(0),
			},
			.childAlignment.x = align_x
		},
	}) {
		Clay_ElementData size_data = Clay_GetElementData(id);

		if (size_data.found) {
			bgame_ui_push_text_config(&rich_text, size_data.boundingBox.width);
			CF_V2 size = cf_text_size(rich_text.text.chars, rich_text.text.len);
			bgame_ui_pop_text_config(&rich_text);

			BUI(CLAY_ID_LOCAL("Content"), {
				.layout = {
					.sizing = {
						.width = CLAY_SIZING_FIXED(size.x),
						.height = CLAY_SIZING_FIXED(size.y),
					},
				},
				.custom = bgame_custom_ui_element(
					bgame_ui_render_rich_text,
					bgame_make_frame_copy(rich_text)
				),
				.userData = bgame_make_frame_copy(size_data.boundingBox.width)
			}) {
			}
		}
	}
}
