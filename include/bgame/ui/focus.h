#ifndef BGAME_UI_FOCUS_H
#define BGAME_UI_FOCUS_H

#include <stdint.h>
#include <stdbool.h>
#include "../ui.h"

/**
 * Keyboard and joypad focus for the UI.
 *
 * An element takes part in navigation through its `focus` field.
 *
 * A target is where focus can land.
 *
 * A group scopes its children, remembers which of them was focused last and bounds
 * wrapping and trapping.
 *
 * The tree is rebuilt every frame. Focus and action routing use the tree built
 * in the previous frame.
 *
 * Input is handled by the game: an action is a bit the game defines, routed from
 * the focused element up through its ancestors to the first that claims it.
 */

/*! How to move the focus with @ref bgame_ui_focus_move */
typedef enum {
	/*! The nearest target to the left */
	BGAME_UI_FOCUS_LEFT,
	/*! The nearest target to the right */
	BGAME_UI_FOCUS_RIGHT,
	/*! The nearest target above */
	BGAME_UI_FOCUS_UP,
	/*! The nearest target below */
	BGAME_UI_FOCUS_DOWN,
	/*! The next target in declaration order */
	BGAME_UI_FOCUS_NEXT,
	/*! The previous target in declaration order */
	BGAME_UI_FOCUS_PREV,
	/*! Into an enterable group, to the child focused last */
	BGAME_UI_FOCUS_ENTER,
	/*! Out of an entered group, back onto the group */
	BGAME_UI_FOCUS_LEAVE,
} bgame_ui_focus_dir_t;

/*! Bits for @ref bgame_ui_focus_t::flags */
typedef enum {
	/*! This element is only declared for the layout, skipped by navigation */
	BGAME_UI_FOCUS_DISABLED  = 1 << 0,
	/*! Focused when this element appears */
	BGAME_UI_FOCUS_AUTOFOCUS = 1 << 1,
	/*! The child a group hands focus to when it has none remembered */
	BGAME_UI_FOCUS_DEFAULT   = 1 << 2,
	/*! Keep navigation and focus inside the group while present */
	BGAME_UI_FOCUS_MODAL     = 1 << 3,
	/*! A group that is also a target. Becomes modal once entered */
	BGAME_UI_FOCUS_ENTERABLE = 1 << 4,
	/*! Left and right wrap around inside this group */
	BGAME_UI_FOCUS_WRAP_H    = 1 << 5,
	/*! Up and down wrap around inside this group */
	BGAME_UI_FOCUS_WRAP_V    = 1 << 6,
} bgame_ui_focus_flag_t;

/*! How the pointer moves the focus */
typedef enum {
	/*! Hovering focuses */
	BGAME_UI_POINTER_FOLLOW,
	/*! Only a click focuses */
	BGAME_UI_POINTER_CLICK,
	/*! The pointer never moves focus */
	BGAME_UI_POINTER_IGNORE,
} bgame_ui_pointer_policy_t;

/*! What the player is driving the UI with, to show or hide the focus */
typedef enum {
	/*! The last input was a key or joypad */
	BGAME_UI_INPUT_NAVIGATION,
	/*! The last input was the pointer */
	BGAME_UI_INPUT_POINTER,
} bgame_ui_input_mode_t;

// Querying inside an element, during the build

/*! Whether the innermost open focus element is focused */
bool
bgame_ui_focused(void);

/*! Whether the element became focused since the last build */
bool
bgame_ui_focus_gained(void);

/*! Whether the element stopped being focused since the last build */
bool
bgame_ui_focus_lost(void);

/*! Whether the focus is on the element or inside it */
bool
bgame_ui_focus_within(void);

/*! Whether the element was handed any of these actions this frame */
bool
bgame_ui_focus_action(uint32_t actions);

// Feeding input, before the elements

/**
 * Route actions to the first claimant from the focused element up
 *
 * @return The claimant's id or 0 for none
 */
bgame_ui_id_t
bgame_ui_focus_route(uint32_t actions);

/**
 * Route actions to the first claimant from the innermost focus element under the pointer up
 *
 * @return The claimant's id or 0 for none
 */
bgame_ui_id_t
bgame_ui_focus_route_pointer(uint32_t actions);

/*! Move the focus; false when there was nowhere to go */
bool
bgame_ui_focus_move(bgame_ui_focus_dir_t dir);

/*! Focus an element by id, 0 for none */
void
bgame_ui_focus_set(bgame_ui_id_t id);

/*! The focused element, 0 for none */
bgame_ui_id_t
bgame_ui_focus_id(void);

/*! The box of the focused element, zero for none */
bgame_ui_rect_t
bgame_ui_focus_bounds(void);

/*! The target under the pointer, or captured by a press on it, 0 for none */
bgame_ui_id_t
bgame_ui_hovered(void);

void
bgame_ui_set_pointer_policy(bgame_ui_pointer_policy_t policy);

bgame_ui_input_mode_t
bgame_ui_input_mode(void);

#endif
