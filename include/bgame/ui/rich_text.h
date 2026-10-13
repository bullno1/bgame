#ifndef BGAME_UI_RICH_TEXT_H
#define BGAME_UI_RICH_TEXT_H

#include "../ui.h"
#include "../asset/font.h"
#include "../str.h"

typedef enum {
	BGAME_UI_RICH_TEXT_WRAP = 0,
	BGAME_UI_RICH_TEXT_GROW,
} bgame_ui_rich_text_sizing_t;

typedef enum {
	BGAME_UI_RICH_TEXT_JUSTIFY_LEFT = 0,
	BGAME_UI_RICH_TEXT_JUSTIFY_CENTER,
	BGAME_UI_RICH_TEXT_JUSTIFY_RIGHT,
} bgame_ui_rich_text_justify_t;

typedef struct {
	bgame_ui_rich_text_sizing_t sizing;
	bgame_ui_rich_text_justify_t justify;
	bgame_str_t text;
	const bgame_font_t* font;
	float font_size;
	int font_blur;
	Clay_Color color;
} bgame_ui_rich_text_t;

void
bgame_ui_rich_text(
	Clay_ElementId id,
	bgame_ui_rich_text_t rich_text
);

#endif
