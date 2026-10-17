#ifndef BGAME_UI_RICH_TEXT_H
#define BGAME_UI_RICH_TEXT_H

#include "../ui.h"
#include "../asset/font.h"
#include "../str.h"
#include <cute_color.h>

typedef enum {
	BGAME_UI_RICH_TEXT_JUSTIFY_LEFT = 0,
	BGAME_UI_RICH_TEXT_JUSTIFY_CENTER,
	BGAME_UI_RICH_TEXT_JUSTIFY_RIGHT,
} bgame_ui_rich_text_justify_t;

/*! A stroke around the glyphs, drawn under the fill. None when the color is transparent. */
typedef struct {
	CF_Color color;
	float thickness;
} bgame_ui_text_outline_t;

/*! A copy of the text drawn under it, offset in UI pixels (y down). None when the color is transparent. */
typedef struct {
	CF_Color color;
	CF_V2 offset;
	int blur;
} bgame_ui_text_shadow_t;

/*! How a text looks. Always complete: a scope derives one by copying and editing. */
typedef struct {
	const bgame_font_t* font;
	float font_size;
	CF_Color color;
	bgame_ui_text_outline_t outline;
	bgame_ui_text_shadow_t shadow;
} bgame_ui_text_style_t;

typedef BUI_CTX_TAG(bgame_ui_text_style_t) bgame_ui_text_style_tag_t;

/**
 * The style of every text that names none
 *
 * A style without a font, this one when nothing was pushed, draws red in the
 * built-in font so that a missing style is seen.
 */
extern bgame_ui_text_style_tag_t bgame_ui_text_style;

typedef struct {
	bgame_str_t text;
	/*! Where the lines sit in a box wider than the text */
	bgame_ui_rich_text_justify_t justify;
	/*! NULL for the current @ref bgame_ui_text_style */
	const bgame_ui_text_style_t* style;
} bgame_ui_rich_text_t;

/**
 * Content drawing a text in the element's box, wrapped at its width
 *
 * A fit width is as wide as the text; a grow or fixed width wraps it.
 */
bgame_ui_content_t
bgame_ui_rich_text(bgame_ui_rich_text_t rich_text);

/*! `BUI_TEXT(.text = ..., .style = ...)` */
#define BUI_TEXT(...) bgame_ui_rich_text((bgame_ui_rich_text_t){ __VA_ARGS__ })

#endif
