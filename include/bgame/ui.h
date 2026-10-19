#ifndef BGAME_UI_H
#define BGAME_UI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <cute_math.h>
#include <bfmt.h>
#include <bgame/scope.h>
#include <bgame/asset/translation.h>

/*! The language the source text is written in, for its plural rules */
#ifndef BGAME_UI_SOURCE_LANGUAGE
#define BGAME_UI_SOURCE_LANGUAGE "en"
#endif

/*! Identity of an element across frames, 0 for none */
typedef uint64_t bgame_ui_id_t;

/*! A box in UI space: top-left origin, y down */
typedef struct {
	float x, y, width, height;
} bgame_ui_rect_t;

typedef struct {
	float width, height;
} bgame_ui_size_t;

/*! A place in the code, for the inspector to show and jump to */
typedef struct {
	const char* file;
	int line;
} bgame_ui_loc_t;

/*! Expands into the current source location */
#define BUI_HERE() ((bgame_ui_loc_t){ __FILE__, __LINE__ })

// Layout {{{

typedef enum {
	/*! As large as the content, within min and max */
	BGAME_UI_SIZING_FIT = 0,
	/*! A share of what the parent has left, within min and max */
	BGAME_UI_SIZING_GROW,
	/*! Pixels */
	BGAME_UI_SIZING_FIXED,
	/*! A fraction of the parent, 0 to 1 */
	BGAME_UI_SIZING_PERCENT,
} bgame_ui_sizing_type_t;

typedef struct {
	bgame_ui_sizing_type_t type;
	float value;
	/*! 0 for no bound */
	float min, max;
} bgame_ui_sizing_t;

#define BUI_FIT(...)   ((bgame_ui_sizing_t){ .type = BGAME_UI_SIZING_FIT, __VA_ARGS__ })
#define BUI_GROW(...)  ((bgame_ui_sizing_t){ .type = BGAME_UI_SIZING_GROW, __VA_ARGS__ })
#define BUI_FIXED(V)   ((bgame_ui_sizing_t){ .type = BGAME_UI_SIZING_FIXED, .value = (V) })
#define BUI_PERCENT(V) ((bgame_ui_sizing_t){ .type = BGAME_UI_SIZING_PERCENT, .value = (V) })

typedef enum {
	BGAME_UI_LEFT_TO_RIGHT = 0,
	BGAME_UI_TOP_TO_BOTTOM,
} bgame_ui_direction_t;

typedef enum {
	BGAME_UI_ALIGN_START = 0,
	BGAME_UI_ALIGN_CENTER,
	BGAME_UI_ALIGN_END,
} bgame_ui_align_t;

typedef struct {
	bgame_ui_align_t x, y;
} bgame_ui_align2_t;

typedef struct {
	float left, right, top, bottom;
} bgame_ui_padding_t;

typedef struct {
	struct { bgame_ui_sizing_t width, height; } sizing;
	bgame_ui_direction_t direction;
	bgame_ui_padding_t padding;
	float child_gap;
	bgame_ui_align2_t child_align;
	/*! Width over height the box is shrunk to once sized, 0 for none */
	float aspect_ratio;
} bgame_ui_layout_t;

/*! Children are cut at the box on the chosen axes; the offset scrolls them */
typedef struct {
	bool horizontal, vertical;
	CF_V2 offset;
} bgame_ui_clip_t;

typedef enum {
	/*! Laid out in the flow */
	BGAME_UI_ATTACH_NONE = 0,
	/*! Over the parent */
	BGAME_UI_ATTACH_PARENT,
	/*! Over @ref bgame_ui_floating_t::target */
	BGAME_UI_ATTACH_ELEMENT,
	/*! Over the whole UI */
	BGAME_UI_ATTACH_ROOT,
} bgame_ui_attach_t;

/*! Taken out of the flow and placed over something, drawn after it */
typedef struct {
	bgame_ui_attach_t attach;
	bgame_ui_id_t target;
	/*! How to attach the element to its target */
	struct {
		bgame_ui_align2_t element;
		bgame_ui_align2_t target;
	} attach_points;
	CF_V2 offset;
	/*! Higher draws later, ties in declaration order */
	int16_t z;
	/*! The pointer sees through to what is below */
	bool passthrough;
} bgame_ui_floating_t;

// }}}

// Transition {{{

/*! A departure from the laid-out box. Zero is none. */
typedef struct {
	/*! Pixels in UI space */
	CF_V2 offset;
	/*! Fraction of each dimension lost: { 1, 1 } is down to nothing */
	CF_V2 shrink;
	/*! Radians, counter-clockwise on screen */
	float rotation;
	/*! Fraction of opacity lost: 1 is invisible */
	float fade;
	/*! What shrink and rotation anchor to, as fractions of the box from its center: { 0, -0.5 } is the top edge */
	CF_V2 pivot;
} bgame_ui_transform_t;

typedef float (*bgame_ui_curve_fn_t)(float progress);

/**
 * How the element arrives, moves and leaves
 */
typedef struct {
	/*! Seconds, 0 for none */
	float duration;
	/*! NULL for smoothstep. Shapes motion only; progress is reported raw. */
	bgame_ui_curve_fn_t curve;
	bgame_ui_transform_t enter;
	bgame_ui_transform_t exit;
	/*! Leaving: declared on the one that leaves, inherited by every transition under it */
	bool exiting;
} bgame_ui_transition_t;

typedef enum {
	BGAME_UI_TRANSITION_IDLE = 0,
	BGAME_UI_TRANSITION_ENTERING,
	BGAME_UI_TRANSITION_MOVING,
	BGAME_UI_TRANSITION_EXITING,
} bgame_ui_transition_phase_t;

typedef struct {
	bgame_ui_transition_phase_t phase;
	/*! 0 to 1 through the phase, linear */
	float progress;
} bgame_ui_transition_state_t;

// }}}

// Content {{{

/*! Where an element ended up, as a draw sees it */
typedef struct {
	bgame_ui_id_t id;
	bgame_ui_rect_t bounds;
	/*! The part of @ref bounds left visible by clipping */
	bgame_ui_rect_t clip;
	/*! NULL for an element without a transition */
	const bgame_ui_transition_state_t* transition;
	void* userdata;
} bgame_ui_box_t;

typedef void (*bgame_ui_draw_fn_t)(const bgame_ui_box_t* box, void* userdata);

/*! The content's size when given this much width, inside the padding */
typedef bgame_ui_size_t (*bgame_ui_measure_fn_t)(float width, void* userdata);

/*! What the element draws of its own, under its children */
typedef struct {
	bgame_ui_draw_fn_t draw;
	/*! NULL for content with no size of its own */
	bgame_ui_measure_fn_t measure;
	/*! Must live until the frame is rendered, see bgame_make_frame_copy */
	void* userdata;
	/*! For the inspector, NULL for none */
	const char* debug_name;
} bgame_ui_content_t;

// }}}

// Focus {{{

typedef enum {
	BGAME_UI_FOCUS_NONE = 0,
	/*! Where focus can land */
	BGAME_UI_FOCUS_TARGET,
	/*! Scopes its children and remembers which was focused last */
	BGAME_UI_FOCUS_GROUP,
} bgame_ui_focus_kind_t;

/*! Flags and actions as in bgame/ui/focus.h */
typedef struct {
	bgame_ui_focus_kind_t kind;
	uint32_t flags;
	uint32_t claims;
} bgame_ui_focus_t;

// }}}

/*! An element. Zero is a fit-sized, undrawn box in the flow. */
typedef struct {
	bgame_ui_layout_t layout;
	bgame_ui_clip_t clip;
	bgame_ui_floating_t floating;
	bgame_ui_content_t content;
	bgame_ui_focus_t focus;
	bgame_ui_transition_t transition;
} bgame_ui_element_t;

// Building {{{

/*! The UI's input for a frame */
typedef struct {
	bgame_ui_size_t size;
	float dt;
	CF_V2 pointer;
	bool pointer_down;
	bool pointer_pressed;
	bool pointer_released;
	/*! The secondary button, which cancels a pick */
	bool pointer_alt_pressed;
	float wheel;
} bgame_ui_frame_t;

/*! Begin a frame, NULL for the app's window and mouse. Hover is decided against last frame's layout. */
void
bgame_ui_begin(const bgame_ui_frame_t* frame);

/*! Lay out what was declared and play the transitions */
void
bgame_ui_end(void);

/**
 * What distinguishes different elements under the same scope.
 *
 * If @ref bgame_ui_key_t::id is set (i.e: non-zero), it will be used as the
 * element's id.
 * Otherwise, a fresh id will be generated under the current scope using
 * @ref bgame_ui_key_t::name and @ref bgame_ui_key_t::index.
 */
typedef struct {
	const char* name;
	uint64_t index;

	/*! If zero, an id will be generated under the current scope */
	bgame_ui_id_t id;
} bgame_ui_key_t;

#define BUI_KEY(NAME, INDEX) ((bgame_ui_key_t){ .name = (NAME), .index = (INDEX) })

/*! The key of an anonymous element */
#define BUI_GEN_ID() ((const char*)NULL)

static inline bgame_ui_key_t
bgame_ui__key_of_key(bgame_ui_key_t key) {
	return key;
}

static inline bgame_ui_key_t
bgame_ui__key_of_name(const char* name) {
	return (bgame_ui_key_t){ .name = name };
}

#define BUI__KEY_OF(KEY) \
	_Generic((KEY), bgame_ui_key_t: bgame_ui__key_of_key, default: bgame_ui__key_of_name)(KEY)

/*! Begin an element under the open scope, declared at @p loc; the scope is its own from here */
bgame_ui_id_t
bgame_ui__begin(bgame_ui_loc_t loc, bgame_ui_key_t key);

/**
 * @ref bgame_ui__begin for a composite's root, placed at its caller's @p loc and opened at @p def
 *
 * The root owns what the composite declares under it, up to an outlet.
 */
bgame_ui_id_t
bgame_ui__begin_at(bgame_ui_loc_t def, bgame_ui_loc_t loc, bgame_ui_key_t key);

/*! Give the begun element its declaration, returning its id */
bgame_ui_id_t
bgame_ui__configure(const bgame_ui_element_t* element);

/**
 * Open a composite's root, returning its id: `bgame_ui_open_at(loc, "Name", &element)`
 *
 * @p loc is the location the caller gave, see @ref BUI_HERE: the inspector reports this
 * line as where the element is defined.
 */
#define bgame_ui_open_at bgame_ui__open_at_line(__LINE__)
#define bgame_ui__open_at_line(LINE) \
	(bgame_ui__begin_at((bgame_ui_loc_t){ __FILE__, LINE }, bgame_ui__open_at_args
#define bgame_ui__open_at_args(LOC, KEY, ...) \
	(LOC), BUI__KEY_OF(KEY)), bgame_ui__configure(__VA_ARGS__))

void
bgame_ui_close(void);

bool
bgame_ui__block(void);

/**
 * Call a function taking a @ref bgame_ui_loc_t first with the location of the call
 *
 * `FN` must be a function, not a macro.
 * `#define my_composite BUI_CALL_WITH_LOC(my_composite_at)` expands `my_composite(x, y)`
 * into `my_composite_at(BUI_HERE(), x, y)` and `my_composite()` into `my_composite_at(BUI_HERE())`.
 */
#define BUI_CALL_WITH_LOC(FN) FN(BUI_HERE() BUI__CALL_WITH_LOC_ARGS
#define BUI__CALL_WITH_LOC_ARGS(...) __VA_OPT__(,) __VA_ARGS__)

/**
 * @ref BUI_CALL_WITH_LOC for a function taking a config struct, as a braced list or a variable
 *
 * `#define my_composite BUI_CALL_WITH_LOC_AND_CONFIG(my_composite_at, my_composite_config_t)`
 * expands `my_composite({ .a = 1 })` and `my_composite(config)` into
 * `my_composite_at(BUI_HERE(), ...)`, the way @ref BUI takes its declaration.
 */
#define BUI_CALL_WITH_LOC_AND_CONFIG(FN, TYPE) FN(BUI_HERE(), ((TYPE[1])BUI__CALL_WITH_LOC_AND_CONFIG_ARGS
#define BUI__CALL_WITH_LOC_AND_CONFIG_ARGS(...) { __VA_OPT__([0] =) __VA_ARGS__ })[0])

/*! `bgame_ui_open("Name", element)`: @ref bgame_ui_open_at from here. Spelled out, since the wrapper takes a function, not a macro. */
#define bgame_ui_open bgame_ui__open_line(__LINE__)
#define bgame_ui__open_line(LINE) (bgame_ui__begin((bgame_ui_loc_t){ __FILE__, LINE }, bgame_ui__open_args
#define bgame_ui__open_args(KEY, ...) BUI__KEY_OF(KEY)), bgame_ui__configure(__VA_ARGS__))

#define BUI__ELEM(...) ((bgame_ui_element_t[1]){ __VA_OPT__([0] =) __VA_ARGS__ })

/**
 * `BUI` placed at a location given to the composite by its caller, see @ref BUI_HERE
 *
 * The element is the composite's root and owns what the following code block declares.
 * A pick on an owned element lands on this line.
 * A deep pick (@ref BGAME_UI_PICK_DEEP) bypasses this rule.
 * Content the caller provides goes in an outlet, see @ref bgame_ui_outlet_begin.
 */
#define BUI_AT BUI__AT_LINE(__LINE__)
#define BUI__AT_LINE(LINE) \
	for (bgame_ui__begin_at((bgame_ui_loc_t){ __FILE__, LINE }, BUI__AT_ARGS
#define BUI__AT_ARGS(LOC, KEY, ...) \
	(LOC), BUI__KEY_OF(KEY)), bgame_ui__configure(BUI__ELEM(__VA_ARGS__)); bgame_ui__block(); bgame_ui_close())

/*! `BUI("X", { .layout = ... }) { children }`, placed at the line of `BUI` */
#define BUI BUI__AT(__LINE__)
#define BUI__AT(LINE) \
	for (bgame_ui__begin((bgame_ui_loc_t){ __FILE__, LINE }, BUI__ARGS
#define BUI__ARGS(KEY, ...) \
	BUI__KEY_OF(KEY)), bgame_ui__configure(BUI__ELEM(__VA_ARGS__)); bgame_ui__block(); bgame_ui_close())

/*! Narrow the id scope without an element, for code that emits siblings */
void
bgame_ui_push_id(const char* name, uint64_t index);

void
bgame_ui_pop_id(void);

#define BUI_ID_SCOPE(NAME, INDEX) \
	BGAME_SCOPE(bgame_ui_push_id((NAME), (INDEX)), bgame_ui_pop_id())

/**
 * Begin an outlet: where a composite puts the content its caller provides
 *
 * The outlet has its own id scope so its element cannot have id collision with
 * the composite. Elements inside an outlet are attributed to the caller.
 */
void
bgame_ui_outlet_begin(const char* name);

void
bgame_ui_outlet_end(void);

/*! `BUI_OUTLET("body") { config.body(config.ctx); }` */
#define BUI_OUTLET(NAME) \
	for (bool bgame_ui__outlet = (bgame_ui_outlet_begin(NAME), true); bgame_ui__outlet; bgame_ui_outlet_end(), bgame_ui__outlet = false)

/*! The open element, 0 at the root */
bgame_ui_id_t
bgame_ui_open_id(void);

/*! The open scope: the open element or the last pushed id */
bgame_ui_id_t
bgame_ui_scope_id(void);

/*! What @ref bgame_ui_open gives this name and index under @p scope. */
bgame_ui_id_t
bgame_ui_make_id(bgame_ui_id_t scope, const char* name, uint64_t index);

/**
 * A key with its id resolved under @p scope, naming the element before it is declared.
 */
bgame_ui_key_t
bgame_ui_make_key(bgame_ui_id_t scope, const char* name, uint64_t index);

/*! Where an element was last frame, false if it was not there */
bool
bgame_ui_bounds(bgame_ui_id_t id, bgame_ui_rect_t* bounds);

/*! Last frame's transition of an element, NULL if it had none */
const bgame_ui_transition_state_t*
bgame_ui_transition_state(bgame_ui_id_t id);

/*! Whether the element's exit ran to its end last frame, so it can stop being declared */
bool
bgame_ui_exited(bgame_ui_id_t id);

/*! The elements under the pointer last frame, outermost first */
const bgame_ui_id_t*
bgame_ui_over(int* count);

/*! The delta time of the frame begun, as given to @ref bgame_ui_begin */
float
bgame_ui_dt(void);

/*! Pixels, y down, from a UI rectangle to CF's y-up drawing space */
static inline CF_Aabb
bgame_ui_aabb(bgame_ui_rect_t rect) {
	return (CF_Aabb){
		.min = { rect.x, -(rect.y + rect.height) },
		.max = { rect.x + rect.width, -rect.y },
	};
}

// }}}

// Rendering {{{

typedef enum {
	/*! @ref bgame_ui_command_t::draw with @ref bgame_ui_command_t::box */
	BGAME_UI_CMD_DRAW,
	/*! Cut everything until the matching end at the box */
	BGAME_UI_CMD_CLIP_BEGIN,
	BGAME_UI_CMD_CLIP_END,
	/*! Transform everything until the matching end */
	BGAME_UI_CMD_POSE_BEGIN,
	BGAME_UI_CMD_POSE_END,
	/*! The inspector's selection, over everything */
	BGAME_UI_CMD_HIGHLIGHT,
} bgame_ui_command_type_t;

typedef struct {
	/*! In UI space */
	CF_M3x2 transform;
	float opacity;
} bgame_ui_pose_t;

typedef struct {
	bgame_ui_command_type_t type;
	bgame_ui_box_t box;
	bgame_ui_draw_fn_t draw;
	bgame_ui_pose_t pose;
} bgame_ui_command_t;

/*! This frame's layout in paint order, frame-lived */
const bgame_ui_command_t*
bgame_ui_commands(int* count);

/*! Draw the commands with cute_framework */
void
bgame_ui_render(void);

// }}}

// Text {{{

/**
 * The language UI text is translated to, NULL for the source text.
 *
 * The translation is referenced, not copied: unload it only after setting
 * another.
 */
void
bgame_ui_set_translation(bgame_translation_t* translation);

bgame_translation_t*
bgame_ui_translation(void);

/*! The current locale, for one use */
bfmt_locale_t*
bgame_ui_locale(void);

// }}}

// State and context {{{

/*! Declare a tag to be used with @ref bgame_ui_state */
#define BUI_STATE_TAG(TYPE) struct { TYPE bgame_ui__type; }

/*! Declare a tag to be used with @ref bgame_ui_push_ctx */
#define BUI_CTX_TAG(TYPE) struct { bgame_ui__ctx_link_t bgame_ui__link; TYPE bgame_ui__ctx; }

/**
 * Retrieve transient UI state
 *
 * Keyed by @p TAG and the open scope's id. Zero-initialized when first seen,
 * carried over from frame to frame while declared, and dropped a frame after
 * it stops being. The address is only good for the frame.
 */
#define bgame_ui_state(TAG) \
	(typeof(TAG.bgame_ui__type)*)bgame_ui__state( \
		sizeof(TAG.bgame_ui__type), \
		_Alignof(typeof(TAG.bgame_ui__type)), \
		&TAG \
	)

/**
 * Push a copy of the current context, zero-initialized if there is none
 *
 * For passing data down a UI tree, such as a theme.
 */
#define bgame_ui_push_ctx(TAG) \
	((typeof(TAG.bgame_ui__ctx)*)bgame_ui__push_ctx( \
		sizeof(TAG), \
		_Alignof(typeof(TAG)), \
		offsetof(typeof(TAG), bgame_ui__ctx), \
		&TAG \
	))

/*! Pop the current context and return the previous one, NULL when the stack is empty */
#define bgame_ui_pop_ctx(TAG) \
	((typeof(TAG.bgame_ui__ctx)*)bgame_ui__pop_ctx( \
		offsetof(typeof(TAG), bgame_ui__ctx), \
		&TAG \
	))

/*! The current context, zero when nothing was pushed. Only the pusher may change it. */
#define bgame_ui_peek_ctx(TAG) \
	((const typeof(TAG.bgame_ui__ctx)*)bgame_ui__peek_ctx( \
		sizeof(TAG), \
		_Alignof(typeof(TAG)), \
		offsetof(typeof(TAG), bgame_ui__ctx), \
		&TAG \
	))

/**
 * Push a context for the duration of the following block
 *
 * The new entry is available as `BUI_CTX` inside the block.
 * An optional expression, evaluated once before the block, may initialize it.
 */
#define BUI_WITH_CTX(TAG, ...) \
	for ( \
		typeof(TAG.bgame_ui__ctx)* BUI_CTX = bgame_ui_push_ctx(TAG); \
		(BUI_CTX != NULL) && (__VA_OPT__((void)(__VA_ARGS__),) 1); \
		bgame_ui_pop_ctx(TAG), BUI_CTX = NULL \
	)

typedef struct bgame_ui__ctx_link_s bgame_ui__ctx_link_t;

struct bgame_ui__ctx_link_s {
	bgame_ui__ctx_link_t* next;
};

void*
bgame_ui__state(size_t size, size_t alignment, const void* tag);

void*
bgame_ui__push_ctx(size_t size, size_t alignment, ptrdiff_t offset, const void* tag);

void*
bgame_ui__pop_ctx(ptrdiff_t offset, const void* tag);

const void*
bgame_ui__peek_ctx(size_t size, size_t alignment, ptrdiff_t offset, const void* tag);

// }}}

#endif
