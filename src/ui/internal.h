#ifndef BGAME_UI_INTERNAL_H
#define BGAME_UI_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include <bgame/ui/focus.h>

/*! The id of the layout element being declared */
uint32_t
bgame_ui_open_element_id(void);

/*! The box of a layout element, false for one that is not laid out */
bool
bgame_ui_element_bounds(uint32_t id, bgame_ui_rect_t* bounds);

typedef struct {
	float x;
	float y;
	bool moved;
	bool down;
	bool pressed;
	bool released;
	/*! The layout elements under the pointer, outermost first */
	const uint32_t* over;
	int num_over;
} bgame_ui_pointer_t;

/*! Before the layout begins */
void
bgame_ui_focus_begin(const bgame_ui_pointer_t* pointer);

/*! After the layout ends */
void
bgame_ui_focus_end(void);

#endif
