#ifndef BGAME_UI_MODAL_H
#define BGAME_UI_MODAL_H

#include "../ui.h"

/**
 * One modal at a time, owned by the library, in the manner of dear imgui's popups.
 *
 * `bgame_ui_modal_open("Confirm")` anywhere, then every frame, where it is drawn:
 *
 *     if (bgame_ui_modal_begin("Confirm", { ... })) {
 *         ...
 *         bgame_ui_modal_end();
 *     }
 *
 * Begin is true while the modal is open or leaving and declares nothing otherwise.
 * `bgame_ui_modal_close()` inside starts the exit. A coroutine drives one frame per
 * iteration with `while` in place of `if`, yielding after the end.
 * Modal names have a scope of their own, so open and begin need not share one.
 *
 * The element floats over the root as a modal, autofocused focus group whatever the
 * declaration says; the declaration gives the rest: content, layout, focus claims, and
 * the transition whose exit times the leaving, inherited by the transitions under it.
 * Without a duration the modal is gone the frame after the close.
 */

/*! Open the named modal; false when one is already open or leaving */
bool
bgame_ui_modal_open(const char* name);

/*! Open the named modal's element, declared at @p loc, when it is open or leaving: true when the body is to run and @ref bgame_ui_modal_end to be called */
bool
bgame_ui_modal_begin_at(bgame_ui_loc_t loc, const char* name, const bgame_ui_element_t* element);

void
bgame_ui_modal_end(void);

/*! Start closing the modal between begin and end, or the open one outside of any */
void
bgame_ui_modal_close(void);

/*! `bgame_ui_modal_begin("Name", { .content = ..., .transition = ... })`: @ref bgame_ui_modal_begin_at from here */
#define bgame_ui_modal_begin bgame_ui__modal_begin_line(__LINE__)
#define bgame_ui__modal_begin_line(LINE) \
	bgame_ui_modal_begin_at((bgame_ui_loc_t){ __FILE__, LINE }, bgame_ui__modal_begin_args
#define bgame_ui__modal_begin_args(NAME, ...) (NAME), BUI__ELEM(__VA_ARGS__))

#endif
