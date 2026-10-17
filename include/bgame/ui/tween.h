#ifndef BGAME_UI_TWEEN_H
#define BGAME_UI_TWEEN_H

#include "../ui.h"

/*! A value transitioning to its target, kept per element like @ref bgame_ui_state */
typedef struct {
	float value;
	float from;
	float to;
	float elapsed;
	bool started;
} bgame_ui_tween_t;

typedef BUI_STATE_TAG(bgame_ui_tween_t) bgame_ui_tween_tag_t;

typedef struct {
	float target;
	/*! Seconds from a changed target to reaching it; 0 jumps */
	float duration;
	/*! NULL for smoothstep */
	bgame_ui_curve_fn_t curve;
	/*! Start from @ref initial when first seen; by default the tween starts at its target and nothing moves */
	bool from_initial;
	float initial;
} bgame_ui_tween_desc_t;

/**
 * The value of the tween keyed by @p TAG under the open scope
 *
 * `bgame_ui_tween(glow, { .target = 1.f, .duration = 0.15f })`, or a
 * @ref bgame_ui_tween_desc_t variable.
 * A changed target starts a new animation from the current value.
 * Call once per frame per element.
 */
#define bgame_ui_tween(TAG, ...) \
	bgame_ui__tween(&(TAG), ((bgame_ui_tween_desc_t[1]){ __VA_OPT__([0] =) __VA_ARGS__ })[0])

/*! Whether the tween keyed by @p TAG sits on its target */
#define bgame_ui_tween_done(TAG) bgame_ui__tween_done(&(TAG))

float
bgame_ui__tween(bgame_ui_tween_tag_t* tag, bgame_ui_tween_desc_t desc);

bool
bgame_ui__tween_done(bgame_ui_tween_tag_t* tag);

#endif
