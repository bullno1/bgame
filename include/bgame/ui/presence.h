#ifndef BGAME_UI_PRESENCE_H
#define BGAME_UI_PRESENCE_H

#include "../ui.h"

/**
 * An element that is present while a condition holds and leaves through its
 * transition once it stops
 *
 *     BUI_PRESENCE("Tools", show_tools, {
 *         .transition = { .duration = 0.2f, .enter.offset = { -40.f, 0.f }, .exit.offset = { -40.f, 0.f } },
 *         ...
 *     }) {
 *         ...
 *     }
 *
 * The block runs while the condition holds and, once it stops, until the exit has
 * run, with `exiting` set by the block; the caller keeps nothing but the condition.
 * A condition that holds again during the exit brings the element back. The
 * transition is what gives the element an enter and an exit: without one the block
 * is a plain conditional. Leaving, the element is unhoverable and takes no input,
 * as any exiting element.
 *
 * The key is a name, a @ref BUI_KEY or a @ref bgame_ui_make_key, never
 * @ref BUI_GEN_ID: the element has to be found before it is declared.
 */

/*! Open the element, declared at @p loc, when present or still leaving: true when the body is to run and @ref bgame_ui_presence_end to be called */
bool
bgame_ui_presence_begin_at(bgame_ui_loc_t loc, bgame_ui_key_t key, bool present, const bgame_ui_element_t* element);

void
bgame_ui_presence_end(void);

/*! `bgame_ui_presence_begin("Name", present, { .transition = ... })`: @ref bgame_ui_presence_begin_at from here */
#define bgame_ui_presence_begin bgame_ui__presence_begin_line(__LINE__)
#define bgame_ui__presence_begin_line(LINE) \
	bgame_ui_presence_begin_at((bgame_ui_loc_t){ __FILE__, LINE }, bgame_ui__presence_begin_args
#define bgame_ui__presence_begin_args(KEY, PRESENT, ...) \
	BUI__KEY_OF(KEY), (PRESENT), BUI__ELEM(__VA_ARGS__))

/*! `BUI_PRESENCE("Name", present, { declaration }) { children }`, placed at the line of `BUI_PRESENCE`; `break` leaves the element open, as in a `BUI` block */
#define BUI_PRESENCE BUI__PRESENCE_LINE(__LINE__)
#define BUI__PRESENCE_LINE(LINE) \
	for (bool bgame_ui__presence = bgame_ui_presence_begin_at((bgame_ui_loc_t){ __FILE__, LINE }, BUI__PRESENCE_ARGS
#define BUI__PRESENCE_ARGS(KEY, PRESENT, ...) \
	BUI__KEY_OF(KEY), (PRESENT), BUI__ELEM(__VA_ARGS__)); bgame_ui__presence; bgame_ui_presence_end(), bgame_ui__presence = false)

#endif
