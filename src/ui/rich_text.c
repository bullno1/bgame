#include <bgame/ui/rich_text.h>
#include <bgame/allocator/frame.h>
#include <cute_draw.h>
#include <float.h>

bgame_ui_text_style_tag_t bgame_ui_text_style;

// CF's embedded font, always loaded
static const bgame_font_t bgame_ui_fallback_font = { .name = "Calibri" };

static const bgame_ui_text_style_t bgame_ui_fallback_text_style = {
	.font = &bgame_ui_fallback_font,
	.font_size = 20.f,
	.color = { .r = 1.f, .g = 0.f, .b = 0.f, .a = 1.f },
};

// What the callbacks need, copied into the frame
typedef struct {
	bgame_str_t text;
	bgame_ui_rich_text_justify_t justify;
	bgame_ui_text_style_t style;
	/*! Written by the measure, read by the draw */
	float wrap_width;
	CF_V2 size;
	/*! Unwrapped, once measured without a width; a wider box keeps it */
	CF_V2 natural;
	bool has_natural;
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
	cf_push_text_wrap_width(draw->wrap_width);
}

static void
bgame_ui_pop_text_config(const bgame_ui_rich_text_draw_t* draw) {
	(void)draw;
	cf_pop_text_wrap_width();
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

static bgame_ui_size_t
bgame_ui_measure_rich_text(float width, void* userdata) {
	bgame_ui_rich_text_draw_t* draw = userdata;
	// A box the text fits in does not wrap it, whatever rounding says
	if (draw->has_natural && width >= draw->natural.x) {
		draw->wrap_width = FLT_MAX;
		draw->size = draw->natural;
	} else {
		draw->wrap_width = width;
		bgame_ui_push_text_config(draw);
		draw->size = cf_text_size(draw->text.chars, draw->text.len);
		bgame_ui_pop_text_config(draw);
		if (width == FLT_MAX) {
			draw->natural = draw->size;
			draw->has_natural = true;
		}
	}
	return (bgame_ui_size_t){ draw->size.x, draw->size.y };
}

static void
bgame_ui_render_rich_text(const bgame_ui_box_t* box, void* userdata) {
	const bgame_ui_rich_text_draw_t* draw = userdata;
	const bgame_ui_text_outline_t* outline = &draw->style.outline;
	const bgame_ui_text_shadow_t* shadow = &draw->style.shadow;

	float slack = box->content.width - draw->size.x;
	float justify = 0.f;
	switch (draw->justify) {
		case BGAME_UI_RICH_TEXT_JUSTIFY_LEFT: justify = 0.f; break;
		case BGAME_UI_RICH_TEXT_JUSTIFY_CENTER: justify = 0.5f; break;
		case BGAME_UI_RICH_TEXT_JUSTIFY_RIGHT: justify = 1.f; break;
	}
	CF_V2 pos = { box->content.x + slack * justify, -box->content.y };
	bool outlined = outline->color.a > 0.f && outline->thickness > 0.f;

	bgame_ui_push_text_config(draw);
	cf_push_text_id(box->id);

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

bgame_ui_content_t
bgame_ui_rich_text(bgame_ui_rich_text_t rich_text) {
	bgame_ui_rich_text_draw_t* draw = bgame_make_frame_copy(((bgame_ui_rich_text_draw_t){
		.text = rich_text.text,
		.justify = rich_text.justify,
		.style = *bgame_ui_resolve_text_style(rich_text.style),
		.wrap_width = FLT_MAX,
	}));

	return (bgame_ui_content_t){
		.draw = bgame_ui_render_rich_text,
		.measure = bgame_ui_measure_rich_text,
		.userdata = draw,
		.debug_name = "text",
	};
}
