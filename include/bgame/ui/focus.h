#ifndef BGAME_UI_FOCUS_H
#define BGAME_UI_FOCUS_H

#include <stdint.h>
#include <stdbool.h>
#include "../utils.h"

/**
 * Keyboard and joypad focus for the UI.
 *
 * A focus tree is declared alongside the layout, one node per element that
 * takes part in navigation.
 *
 * A target is where focus can land on.
 *
 * A group scopes its children: it remembers which of them was focused last and
 * bounds wrapping and trapping. The tree is rebuilt every frame. Focus and
 * action routing uses the tree built in the previous frame.
 *
 * Input is handled by the game: an action is a bit the game defines, routed from
 * the focused node up through its ancestors to the first that claims it.
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

/*! Bits for @ref bgame_ui_focus_desc_t::flags */
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

/*! A box in UI space: top-left origin, y down */
typedef struct {
	float x;
	float y;
	float width;
	float height;
} bgame_ui_rect_t;

/*! A focus node */
typedef struct {
	/*! 0 for the open layout element */
	uint32_t id;
	/*! @ref bgame_ui_focus_flag_t bits */
	uint32_t flags;
	/*! Action bits this node takes, see @ref bgame_ui_focus_route */
	uint32_t claims;
	/*! Zero for the layout element's box, or a group's children */
	bgame_ui_rect_t bounds;
} bgame_ui_focus_desc_t;

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

// Declaring the tree

void
bgame_ui_focus_open_target(const bgame_ui_focus_desc_t* desc);

void
bgame_ui_focus_open_group(const bgame_ui_focus_desc_t* desc);

void
bgame_ui_focus_close(void);

#define BUI__FOCUS_DESC(...) \
	((bgame_ui_focus_desc_t[1]){ __VA_OPT__([0] =) __VA_ARGS__ })

/*! A target, described as `BUI_FOCUS({ .claims = ... })`; the block is its children */
#define BUI_FOCUS(...) \
	BGAME_SCOPE( \
		bgame_ui_focus_open_target(BUI__FOCUS_DESC(__VA_ARGS__)), \
		bgame_ui_focus_close() \
	)

/*! A group, described the same way; the block is its children */
#define BUI_FOCUS_GROUP(...) \
	BGAME_SCOPE( \
		bgame_ui_focus_open_group(BUI__FOCUS_DESC(__VA_ARGS__)), \
		bgame_ui_focus_close() \
	)

// Asking inside a node, during the build

/*! Whether the open node is focused */
bool
bgame_ui_focused(void);

/*! Whether the open node became focused since the last build */
bool
bgame_ui_focus_gained(void);

/*! Whether the open node stopped being focused since the last build */
bool
bgame_ui_focus_lost(void);

/*! Whether the focus is on the open node or inside it */
bool
bgame_ui_focus_within(void);

/*! Whether the open node was handed any of these actions this frame */
bool
bgame_ui_focus_action(uint32_t actions);

// Feeding input, before the nodes

/*! Hand actions to the first claimant from the focused node up; false for none */
bool
bgame_ui_focus_route(uint32_t actions);

/*! Move the focus; false when there was nowhere to go */
bool
bgame_ui_focus_move(bgame_ui_focus_dir_t dir);

/*! Focus a node by id, 0 for none */
void
bgame_ui_focus_set(uint32_t id);

/*! The focused node, 0 for none */
uint32_t
bgame_ui_focus_id(void);

/*! The box of the focused node, zero for none */
bgame_ui_rect_t
bgame_ui_focus_bounds(void);

/*! The target under the pointer, or captured by a press on it, 0 for none */
uint32_t
bgame_ui_hovered(void);

void
bgame_ui_set_pointer_policy(bgame_ui_pointer_policy_t policy);

bgame_ui_input_mode_t
bgame_ui_input_mode(void);

#endif
