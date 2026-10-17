#ifndef BGAME_UI_SPACER_H
#define BGAME_UI_SPACER_H

#include "../ui.h"

/*! Takes the vertical slack of a column */
static inline void
bgame_ui_vspacer(void) {
	BUI({ .layout.sizing = { BUI_PERCENT(1.f), BUI_GROW() } }) {
	}
}

/*! Takes the horizontal slack of a row */
static inline void
bgame_ui_hspacer(void) {
	BUI({ .layout.sizing = { BUI_GROW(), BUI_PERCENT(1.f) } }) {
	}
}

#endif
