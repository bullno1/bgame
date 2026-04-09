#ifndef BGAME_UI_SPACER_H
#define BGAME_UI_SPACER_H

#include "../ui.h"

static inline void
bgame_ui_vspacer(Clay_ElementId id) {
	CLAY(id, {
		.layout.sizing = { CLAY_SIZING_PERCENT(1.f), CLAY_SIZING_GROW(0) }
	}) {
	}
}

static inline void
bgame_ui_hspacer(Clay_ElementId id) {
	CLAY(id, {
		.layout.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_PERCENT(1.f) }
	}) {
	}
}

#endif
