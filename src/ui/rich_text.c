#include <bgame/ui/rich_text.h>
#include <bgame/allocator/frame.h>
#include <cute_draw.h>

bgame_ui_text_style_tag_t bgame_ui_text_style;

// CF's embedded font, always loaded
static const bgame_font_t bgame_ui_fallback_font = { .name = "Calibri" };

static const bgame_ui_text_style_t bgame_ui_fallback_text_style = {
	.font = &bgame_ui_fallback_font,
	.font_size = 20.f,
	.color = { .r = 1.f, .g = 0.f, .b = 0.f, .a = 1.f },
};

// What the render callback needs, copied into the frame
typedef struct {
	bgame_str_t text;
	bgame_ui_rich_text_sizing_t sizing;
	bgame_ui_text_style_t style;
	float wrap_width;
} bgame_ui_rich_text_draw_t;

static const bgame_ui_text_style_t*
bgame_ui_resolve_text_style(const bgame_ui_text_style_t* style) {
	if (style == NULL) {
		style = bgame_ui_peek_ctx(bgame_ui_text_style);
	}
	if (style->font == NULL || style->font_size <= 0.f) {
		style = &bgame_ui_fallback_text_style;
	}
	return style;
}

static void
bgame_ui_push_text_config(const bgame_ui_rich_text_draw_t* draw) {
	cf_push_font(draw->style.font->name);
	cf_push_font_size(draw->style.font_size);
	if (draw->sizing == BGAME_UI_RICH_TEXT_WRAP) {
		cf_push_text_wrap_width(draw->wrap_width);
	}
}

static void
bgame_ui_pop_text_config(const bgame_ui_rich_text_draw_t* draw) {
	if (draw->sizing == BGAME_UI_RICH_TEXT_WRAP) {
		cf_pop_text_wrap_width();
	}
	cf_pop_font_size();
	cf_pop_font();
}

static void
bgame_ui_draw_text_pass(
	const bgame_ui_rich_text_draw_t* draw,
	CF_V2 pos,
	CF_Color color,
	float stroke,
	int blur
) {
	cf_push_text_stroke(stroke);
	cf_push_font_blur(blur);
	cf_draw_push_color(color);
	cf_draw_text(draw->text.chars, pos, draw->text.len);
	cf_draw_pop_color();
	cf_pop_font_blur();
	cf_pop_text_stroke();
}

static void
bgame_ui_render_rich_text(
	const Clay_RenderCommand* command,
	void* userdata
) {
	const bgame_ui_rich_text_draw_t* draw = userdata;
	const bgame_ui_text_outline_t* outline = &draw->style.outline;
	const bgame_ui_text_shadow_t* shadow = &draw->style.shadow;
	CF_V2 pos = { command->boundingBox.x, -command->boundingBox.y };
	bool outlined = outline->color.a > 0.f && outline->thickness > 0.f;

	bgame_ui_push_text_config(draw);
	cf_push_text_id(command->id);

	if (shadow->color.a > 0.f) {
		CF_V2 shadow_pos = { pos.x + shadow->offset.x, pos.y - shadow->offset.y };
		// CF cannot blur a stroke, so a soft shadow is the fill alone
		if (outlined && shadow->blur == 0) {
			bgame_ui_draw_text_pass(draw, shadow_pos, shadow->color, outline->thickness, 0);
		}
		bgame_ui_draw_text_pass(draw, shadow_pos, shadow->color, 0.f, shadow->blur);
	}
	if (outlined) {
		bgame_ui_draw_text_pass(draw, pos, outline->color, outline->thickness, 0);
	}
	bgame_ui_draw_text_pass(draw, pos, draw->style.color, 0.f, 0);

	cf_pop_text_id();
	bgame_ui_pop_text_config(draw);
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
			bgame_ui_rich_text_draw_t* draw = bgame_make_frame_copy(((bgame_ui_rich_text_draw_t){
				.text = rich_text.text,
				.sizing = rich_text.sizing,
				.style = *bgame_ui_resolve_text_style(rich_text.style),
				.wrap_width = size_data.boundingBox.width,
			}));

			bgame_ui_push_text_config(draw);
			CF_V2 size = cf_text_size(draw->text.chars, draw->text.len);
			bgame_ui_pop_text_config(draw);

			BUI(CLAY_ID_LOCAL("Content"), {
				.layout = {
					.sizing = {
						.width = CLAY_SIZING_FIXED(size.x),
						.height = CLAY_SIZING_FIXED(size.y),
					},
				},
				.custom = bgame_custom_ui_element(bgame_ui_render_rich_text, draw),
			}) {
			}
		}
	}
}
