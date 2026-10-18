#include "ui/internal.h"
#include <bgame/reloadable.h>
#include <bgame/allocator/frame.h>
#include <bgame/chibihash64.h>
#include <bgame/chibihash64-stream.h>
#include <bfmt_log.h>
#include <bfmt_cldr.h>
#include <bminmax.h>
#include <cute_app.h>
#include <cute_input.h>
#include <cute_time.h>
#include <dcimgui.h>
#include <float.h>
#include <math.h>
#include <string.h>

#define AXIS_X 0
#define AXIS_Y 1
#define EPSILON 0.001f

typedef struct ui_scope_s ui_scope_t;

struct ui_scope_s {
	bgame_ui_id_t id;
	/*! NULL for a pushed id */
	bgame_ui_node_t* node;
	ui_scope_t* parent;
	int num_anonymous;
};

typedef struct {
	const void* tag;
	bgame_ui_id_t id;
} ui_state_key_t;

typedef struct {
	bgame_ui__ctx_link_t* current;
	bgame_ui__ctx_link_t* pool;
	bgame_ui__ctx_link_t* empty;  // What a peek of an empty stack sees
} ui_ctx_stack_t;

typedef BHAMT_TABLE(ui_state_key_t, void*) ui_state_table_t;
typedef BHAMT_TABLE(const void*, ui_ctx_stack_t) ui_ctx_stacks_t;
typedef BHAMT_TABLE(const char*, const char*) ui_string_table_t;

// Trees live in the frame arena, which outlives a reload; a change to the node struct
// needs a restart, like a component size.
BGAME_VAR(bgame_ui_tree_t*, ui_previous) = NULL;
BGAME_VAR(bgame_ui_tree_t*, ui_current) = NULL;
BGAME_VAR(bgame_translation_t*, ui_translation) = NULL;
BGAME_VAR(ui_state_table_t, ui_previous_states) = { 0 };
BGAME_VAR(ui_state_table_t, ui_current_states) = { 0 };
// Plain statics on purpose: rebuilt by every bgame_ui_begin
static ui_ctx_stacks_t ui_ctx_stacks = { 0 };
static ui_string_table_t ui_frame_strings = { 0 };
static ui_scope_t ui_root_scope = { 0 };
static ui_scope_t* ui_scope = NULL;
static bgame_ui_node_t* ui_open = NULL;
static bool ui_block_pending = false;
static bool ui_duplicate_reported = false;

// Plain static on purpose: a reload zeroes it, so its callbacks are never stale
static struct {
	bool valid;
	bgame_translation_t* translation;
	int version;
	bfmt_locale_t locale;
} ui_locale_cache = { 0 };

// Geometry {{{

static bool
rect_empty(bgame_ui_rect_t rect) {
	return rect.width <= 0.f || rect.height <= 0.f;
}

static bgame_ui_rect_t
rect_intersect(bgame_ui_rect_t a, bgame_ui_rect_t b) {
	float x0 = BMAX(a.x, b.x);
	float y0 = BMAX(a.y, b.y);
	float x1 = BMIN(a.x + a.width, b.x + b.width);
	float y1 = BMIN(a.y + a.height, b.y + b.height);
	return (bgame_ui_rect_t){ x0, y0, BMAX(x1 - x0, 0.f), BMAX(y1 - y0, 0.f) };
}

static bool
rect_contains(bgame_ui_rect_t rect, CF_V2 p) {
	return rect.x <= p.x && p.x < rect.x + rect.width
		&& rect.y <= p.y && p.y < rect.y + rect.height;
}

static bool
rect_equal(bgame_ui_rect_t a, bgame_ui_rect_t b) {
	return fabsf(a.x - b.x) < EPSILON
		&& fabsf(a.y - b.y) < EPSILON
		&& fabsf(a.width - b.width) < EPSILON
		&& fabsf(a.height - b.height) < EPSILON;
}

static bgame_ui_rect_t
rect_lerp(bgame_ui_rect_t a, bgame_ui_rect_t b, float t) {
	return (bgame_ui_rect_t){
		cf_lerp(a.x, b.x, t),
		cf_lerp(a.y, b.y, t),
		cf_lerp(a.width, b.width, t),
		cf_lerp(a.height, b.height, t),
	};
}

static float
align_offset(bgame_ui_align_t align, float slack) {
	switch (align) {
		case BGAME_UI_ALIGN_START: return 0.f;
		case BGAME_UI_ALIGN_CENTER: return slack * 0.5f;
		case BGAME_UI_ALIGN_END: return slack;
	}
	return 0.f;
}

// }}}

// Ids {{{

bgame_ui_id_t
bgame_ui_make_id(bgame_ui_id_t scope, const char* name, uint64_t index) {
	ChibiHash64Ctx ctx = chibihash64_init(0);
	uint8_t named = name != NULL;
	chibihash64_append(&ctx, &scope, sizeof(scope));
	chibihash64_append(&ctx, &named, sizeof(named));
	if (name != NULL) { chibihash64_append(&ctx, name, strlen(name)); }
	chibihash64_append(&ctx, &index, sizeof(index));
	uint64_t id = chibihash64_finish(&ctx);
	return id != 0 ? id : 1;
}

bgame_ui_key_t
bgame_ui_make_key(bgame_ui_id_t scope, const char* name, uint64_t index) {
	return (bgame_ui_key_t){
		.name = name,
		.index = index,
		.id = bgame_ui_make_id(scope, name, index),
	};
}

bgame_ui_id_t
bgame_ui_open_id(void) {
	return ui_open != NULL ? ui_open->id : 0;
}

bgame_ui_id_t
bgame_ui_scope_id(void) {
	return ui_scope != NULL ? ui_scope->id : 0;
}

static ui_scope_t*
push_scope(bgame_ui_id_t id, bgame_ui_node_t* node) {
	ui_scope_t* scope = bgame_alloc_for_frame(sizeof(ui_scope_t), _Alignof(ui_scope_t));
	*scope = (ui_scope_t){ .id = id, .node = node, .parent = ui_scope };
	ui_scope = scope;
	return scope;
}

void
bgame_ui_push_id(const char* name, uint64_t index) {
	if (ui_scope == NULL) {
		BFMT_ERROR("Id pushed outside of a UI frame");
		return;
	}
	push_scope(bgame_ui_make_id(ui_scope->id, name, index), NULL);
}

void
bgame_ui_pop_id(void) {
	if (ui_scope == NULL || ui_scope->node != NULL) {
		BFMT_ERROR("Id popped without one pushed");
		return;
	}
	ui_scope = ui_scope->parent;
}

// }}}

// Lookups {{{

static bgame_ui_node_t*
find_node(const bgame_ui_tree_t* tree, bgame_ui_id_t id) {
	if (tree == NULL || id == 0) { return NULL; }
	bgame_ui_node_t* const* slot = bhamt_get(&tree->index, id);
	return slot != NULL ? *slot : NULL;
}

const bgame_ui_tree_t*
bgame_ui_previous_tree(void) {
	return ui_previous;
}

bool
bgame_ui_element_bounds(bgame_ui_id_t id, bgame_ui_rect_t* bounds) {
	bgame_ui_node_t* node = find_node(ui_current, id);
	if (node == NULL || node->hidden) { return false; }
	*bounds = node->bounds;
	return true;
}

bool
bgame_ui_bounds(bgame_ui_id_t id, bgame_ui_rect_t* bounds) {
	bgame_ui_node_t* node = find_node(ui_previous, id);
	if (node == NULL || node->hidden) { return false; }
	*bounds = node->bounds;
	return true;
}

const bgame_ui_transition_state_t*
bgame_ui_transition_state(bgame_ui_id_t id) {
	bgame_ui_node_t* node = find_node(ui_previous, id);
	if (node == NULL || node->transition == NULL) { return NULL; }
	return &node->transition_state;
}

bool
bgame_ui_exited(bgame_ui_id_t id) {
	const bgame_ui_transition_state_t* state = bgame_ui_transition_state(id);
	return state != NULL
		&& state->phase == BGAME_UI_TRANSITION_EXITING
		&& state->progress >= 1.f;
}

const bgame_ui_id_t*
bgame_ui_over(int* count) {
	if (ui_current == NULL) {
		*count = 0;
		return NULL;
	}
	*count = ui_current->num_over;
	return ui_current->over;
}

float
bgame_ui_dt(void) {
	return ui_current != NULL ? ui_current->input.dt : 0.f;
}

const bgame_ui_command_t*
bgame_ui_commands(int* count) {
	// The frame just ended is the one to draw
	bgame_ui_tree_t* tree = ui_scope == NULL ? ui_current : ui_previous;
	if (tree == NULL) {
		*count = 0;
		return NULL;
	}
	*count = tree->num_commands;
	return tree->commands;
}

// }}}

// Building {{{

static bool
node_in_flow(const bgame_ui_node_t* node) {
	return node->decl.floating.attach == BGAME_UI_ATTACH_NONE;
}

static bool
node_clips(const bgame_ui_node_t* node) {
	return node->decl.clip.horizontal || node->decl.clip.vertical;
}

// Copy a string into the frame arena so it survives hot reload
static const char*
frame_strcpy(const char* str) {
	if (str == NULL) { return NULL; }
	bool is_new;
	const char** slot = bhamt_upsert_ex(&ui_frame_strings, str, bgame_frame_arena(), &is_new);
	if (is_new) {
		size_t len = strlen(str);
		char* copy = bgame_alloc_for_frame(len + 1, _Alignof(char));
		memcpy(copy, str, len + 1);
		*slot = copy;
	}
	return *slot;
}

bgame_ui_id_t
bgame_ui__begin(bgame_ui_loc_t loc, bgame_ui_key_t key) {
	return bgame_ui__begin_at((bgame_ui_loc_t){ 0 }, loc, key, false);
}

bgame_ui_id_t
bgame_ui__begin_at(bgame_ui_loc_t def, bgame_ui_loc_t loc, bgame_ui_key_t key, bool block) {
	if (ui_scope == NULL) {
		BFMT_ERROR("Element opened outside of a UI frame");
		return 0;
	}
	bgame_ui_tree_t* tree = ui_current;
	barena_t* arena = bgame_frame_arena();

	uint64_t index = key.index;
	bgame_ui_id_t id = key.id;
	if (id == 0) {
		if (key.name == NULL) { index = (uint64_t)ui_scope->num_anonymous++; }
		id = bgame_ui_make_id(ui_scope->id, key.name, index);
	}

	bgame_ui_node_t* node = bgame_alloc_for_frame(sizeof(bgame_ui_node_t), _Alignof(bgame_ui_node_t));
	*node = (bgame_ui_node_t){
		.id = id,
		.index = index,
		.loc = { frame_strcpy(loc.file), loc.line },
		.def = { frame_strcpy(def.file), def.line },
		.widget = block,
		.internal = ui_open->widget || ui_open->internal,
		.parent = ui_open,
		.prev_sibling = ui_open->last_child,
		.depth = ui_open->depth + 1,
	};
	if (key.name != NULL) {
		size_t len = strlen(key.name);
		char* copy = bgame_alloc_for_frame(len + 1, _Alignof(char));
		memcpy(copy, key.name, len + 1);
		node->name = copy;
	}

	if (ui_open->last_child != NULL) {
		ui_open->last_child->next_sibling = node;
	} else {
		ui_open->first_child = node;
	}
	ui_open->last_child = node;
	++tree->num_nodes;

	bool is_new;
	bgame_ui_node_t** slot = bhamt_upsert_ex(&tree->index, id, arena, &is_new);
	if (is_new) {
		*slot = node;
	} else if (!ui_duplicate_reported) {
		BFMT_ERROR("Duplicate UI element ", key.name != NULL ? key.name : "(anonymous)", " #", index);
		ui_duplicate_reported = true;
	}

	push_scope(id, node);
	ui_open = node;
	return id;
}

bgame_ui_id_t
bgame_ui__configure(const bgame_ui_element_t* element) {
	if (ui_scope == NULL || ui_open == NULL || ui_open->parent == NULL) {
		BFMT_ERROR("Element configured without one begun");
		return 0;
	}
	bgame_ui_tree_t* tree = ui_current;
	bgame_ui_node_t* node = ui_open;
	node->decl = *element;
	node->decl.content.debug_name = frame_strcpy(element->content.debug_name);

	if (!node_in_flow(node)) {
		if (tree->last_floating != NULL) {
			tree->last_floating->next_floating = node;
		} else {
			tree->first_floating = node;
		}
		tree->last_floating = node;
		++tree->num_floating;
	}
	if (element->transition.duration > 0.f) {
		node->next_transition = tree->first_transition;
		tree->first_transition = node;
		++tree->num_transition;
	}
	if (element->content.draw != NULL) { ++tree->num_content; }
	if (node_clips(node)) { ++tree->num_clip; }

	if (element->focus.kind != BGAME_UI_FOCUS_NONE) {
		bgame_ui_focus_open(node->id, &element->focus);
	}

	ui_block_pending = true;
	return node->id;
}

bool
bgame_ui__block(void) {
	bool pending = ui_block_pending;
	ui_block_pending = false;
	return pending;
}

void
bgame_ui_close(void) {
	if (ui_scope == NULL) {
		BFMT_ERROR("Element closed outside of a UI frame");
		return;
	}
	if (ui_scope->node == NULL) {
		BFMT_ERROR("Element closed with an id still pushed");
		return;
	}
	if (ui_open == NULL || ui_open->parent == NULL) {
		BFMT_ERROR("Element closed without one open");
		return;
	}

	bgame_ui_node_t* node = ui_open;
	if (node->decl.focus.kind != BGAME_UI_FOCUS_NONE) {
		bgame_ui_focus_close();
	}
	ui_open = node->parent;
	ui_scope = ui_scope->parent;
	ui_block_pending = false;
}

// }}}

// Layout {{{

static const bgame_ui_sizing_t*
node_sizing(const bgame_ui_node_t* node, int axis) {
	return axis == AXIS_X ? &node->decl.layout.sizing.width : &node->decl.layout.sizing.height;
}

static float
node_padding_start(const bgame_ui_node_t* node, int axis) {
	return axis == AXIS_X ? node->decl.layout.padding.left : node->decl.layout.padding.top;
}

static float
node_padding_sum(const bgame_ui_node_t* node, int axis) {
	const bgame_ui_padding_t* p = &node->decl.layout.padding;
	return axis == AXIS_X ? p->left + p->right : p->top + p->bottom;
}

static int
node_main_axis(const bgame_ui_node_t* node) {
	return node->decl.layout.direction == BGAME_UI_LEFT_TO_RIGHT ? AXIS_X : AXIS_Y;
}

static float
clamp_size(float size, const bgame_ui_sizing_t* sizing) {
	if (sizing->min > 0.f && size < sizing->min) { size = sizing->min; }
	if (sizing->max > 0.f && size > sizing->max) { size = sizing->max; }
	return BMAX(size, 0.f);
}

/*! What the children take along an axis: the sum with gaps along the direction, the largest across */
static float
children_extent(const bgame_ui_node_t* node, int axis, bool use_min) {
	bool along = node_main_axis(node) == axis;
	float total = 0.f;
	int count = 0;
	for (bgame_ui_node_t* child = node->first_child; child != NULL; child = child->next_sibling) {
		if (!node_in_flow(child)) { continue; }
		float size = use_min ? child->min_size[axis] : child->size[axis];
		if (along) {
			total += size;
			++count;
		} else {
			total = BMAX(total, size);
		}
	}
	if (along && count > 1) { total += node->decl.layout.child_gap * (float)(count - 1); }
	return total;
}

/*! Bottom up: what each element needs at least, as far as it can be known without the parent */
static void
compute_node_min(bgame_ui_node_t* node, int axis) {
	for (bgame_ui_node_t* child = node->first_child; child != NULL; child = child->next_sibling) {
		if (node_in_flow(child)) { compute_node_min(child, axis); }
	}

	const bgame_ui_sizing_t* sizing = node_sizing(node, axis);
	float fit = children_extent(node, axis, true);
	if (node->decl.content.measure != NULL) {
		if (axis == AXIS_X) {
			// Content drives the width only when asked to; a growing width wraps it instead
			if (sizing->type == BGAME_UI_SIZING_FIT) {
				node->content_size = node->decl.content.measure(FLT_MAX, node->decl.content.userdata);
				fit = BMAX(fit, node->content_size.width);
			}
		} else {
			fit = BMAX(fit, node->content_size.height);
		}
	}
	fit += node_padding_sum(node, axis);

	switch (sizing->type) {
		case BGAME_UI_SIZING_FIXED:
			node->min_size[axis] = BMAX(sizing->value, 0.f);
			break;
		case BGAME_UI_SIZING_PERCENT:
			node->min_size[axis] = 0.f;
			break;
		case BGAME_UI_SIZING_FIT:
		case BGAME_UI_SIZING_GROW:
			node->min_size[axis] = clamp_size(fit, sizing);
			break;
	}
}

/*! The size a child starts from inside a parent of this inner size */
static float
child_base_size(const bgame_ui_node_t* child, int axis, float inner, bool along) {
	const bgame_ui_sizing_t* sizing = node_sizing(child, axis);
	switch (sizing->type) {
		case BGAME_UI_SIZING_FIXED: return BMAX(sizing->value, 0.f);
		case BGAME_UI_SIZING_FIT: return child->min_size[axis];
		case BGAME_UI_SIZING_PERCENT: return clamp_size(sizing->value * inner, sizing);
		case BGAME_UI_SIZING_GROW: return along ? child->min_size[axis] : clamp_size(inner, sizing);
	}
	return 0.f;
}

static bool
child_grows(const bgame_ui_node_t* child, int axis) {
	return node_in_flow(child) && node_sizing(child, axis)->type == BGAME_UI_SIZING_GROW;
}

/*! Hand the slack to the growing children, the smallest first so they even out */
static void
distribute_child_slack(bgame_ui_node_t* node, int axis, float remaining) {
	while (remaining > EPSILON) {
		float smallest = FLT_MAX;
		float second = FLT_MAX;
		int count = 0;
		for (bgame_ui_node_t* child = node->first_child; child != NULL; child = child->next_sibling) {
			if (!child_grows(child, axis)) { continue; }
			const bgame_ui_sizing_t* sizing = node_sizing(child, axis);
			float size = child->size[axis];
			if (sizing->max > 0.f && size >= sizing->max - EPSILON) { continue; }
			if (size < smallest - EPSILON) {
				second = smallest;
				smallest = size;
				count = 1;
			} else if (size < smallest + EPSILON) {
				++count;
			} else if (size < second) {
				second = size;
			}
		}
		if (count == 0) { break; }

		float step = BMIN(second - smallest, remaining / (float)count);
		float given = 0.f;
		for (bgame_ui_node_t* child = node->first_child; child != NULL; child = child->next_sibling) {
			if (!child_grows(child, axis)) { continue; }
			if (fabsf(child->size[axis] - smallest) > EPSILON) { continue; }
			const bgame_ui_sizing_t* sizing = node_sizing(child, axis);
			float add = step;
			if (sizing->max > 0.f) { add = BMIN(add, sizing->max - child->size[axis]); }
			child->size[axis] += add;
			given += add;
		}
		remaining -= given;
		if (given <= EPSILON) { break; }
	}
}

/*! Take the overflow from the growing children, the largest first, down to their min */
static void
absorb_child_overflow(bgame_ui_node_t* node, int axis, float overflow) {
	while (overflow > EPSILON) {
		float largest = -FLT_MAX;
		float second = -FLT_MAX;
		int count = 0;
		for (bgame_ui_node_t* child = node->first_child; child != NULL; child = child->next_sibling) {
			if (!child_grows(child, axis)) { continue; }
			float size = child->size[axis];
			if (size <= node_sizing(child, axis)->min + EPSILON) { continue; }
			if (size > largest + EPSILON) {
				second = largest;
				largest = size;
				count = 1;
			} else if (size > largest - EPSILON) {
				++count;
			} else if (size > second) {
				second = size;
			}
		}
		if (count == 0) { break; }

		float step = BMIN(largest - BMAX(second, 0.f), overflow / (float)count);
		float taken = 0.f;
		for (bgame_ui_node_t* child = node->first_child; child != NULL; child = child->next_sibling) {
			if (!child_grows(child, axis)) { continue; }
			if (fabsf(child->size[axis] - largest) > EPSILON) { continue; }
			float floor = node_sizing(child, axis)->min;
			float take = BMIN(step, child->size[axis] - floor);
			child->size[axis] -= take;
			taken += take;
		}
		overflow -= taken;
		if (taken <= EPSILON) { break; }
	}
}

/*! Top down: the final size along an axis, given the element's own */
static void
resolve_node_size(bgame_ui_node_t* node, int axis, float size) {
	node->size[axis] = size;
	float inner = BMAX(size - node_padding_sum(node, axis), 0.f);
	bool along = node_main_axis(node) == axis;

	float total = 0.f;
	int count = 0;
	for (bgame_ui_node_t* child = node->first_child; child != NULL; child = child->next_sibling) {
		if (!node_in_flow(child)) { continue; }
		child->size[axis] = child_base_size(child, axis, inner, along);
		total += child->size[axis];
		++count;
	}
	if (count == 0) { return; }

	if (along) {
		total += node->decl.layout.child_gap * (float)(count - 1);
		float remaining = inner - total;
		if (remaining > EPSILON) {
			distribute_child_slack(node, axis, remaining);
		} else if (remaining < -EPSILON) {
			absorb_child_overflow(node, axis, -remaining);
		}
	}

	for (bgame_ui_node_t* child = node->first_child; child != NULL; child = child->next_sibling) {
		if (node_in_flow(child)) { resolve_node_size(child, axis, child->size[axis]); }
	}
}

/*! With the widths settled, what the content makes of them */
static void
measure_node_contents(bgame_ui_node_t* node) {
	if (node->decl.content.measure != NULL) {
		float inner = BMAX(node->size[AXIS_X] - node_padding_sum(node, AXIS_X), 0.f);
		node->content_size = node->decl.content.measure(inner, node->decl.content.userdata);
	}
	for (bgame_ui_node_t* child = node->first_child; child != NULL; child = child->next_sibling) {
		if (node_in_flow(child)) { measure_node_contents(child); }
	}
}

static void
apply_node_aspect(bgame_ui_node_t* node) {
	float ratio = node->decl.layout.aspect_ratio;
	if (ratio > 0.f && node->size[AXIS_Y] > 0.f) {
		if (node->size[AXIS_X] / node->size[AXIS_Y] > ratio) {
			node->size[AXIS_X] = node->size[AXIS_Y] * ratio;
		} else {
			node->size[AXIS_Y] = node->size[AXIS_X] / ratio;
		}
	}
	for (bgame_ui_node_t* child = node->first_child; child != NULL; child = child->next_sibling) {
		if (node_in_flow(child)) { apply_node_aspect(child); }
	}
}

static void
place_node(bgame_ui_node_t* node, float x, float y) {
	node->pos[AXIS_X] = x;
	node->pos[AXIS_Y] = y;
	node->bounds = (bgame_ui_rect_t){ x, y, node->size[AXIS_X], node->size[AXIS_Y] };

	int along = node_main_axis(node);
	int across = 1 - along;
	const bgame_ui_layout_t* layout = &node->decl.layout;
	bgame_ui_align_t align[2] = { layout->child_align.x, layout->child_align.y };
	float inner[2] = {
		BMAX(node->size[AXIS_X] - node_padding_sum(node, AXIS_X), 0.f),
		BMAX(node->size[AXIS_Y] - node_padding_sum(node, AXIS_Y), 0.f),
	};
	float block = children_extent(node, along, false);
	float cursor = node->pos[along] + node_padding_start(node, along) + align_offset(align[along], inner[along] - block);
	float offset[2] = { 0.f, 0.f };
	if (node_clips(node)) {
		offset[AXIS_X] = node->decl.clip.offset.x;
		offset[AXIS_Y] = node->decl.clip.offset.y;
	}

	for (bgame_ui_node_t* child = node->first_child; child != NULL; child = child->next_sibling) {
		if (!node_in_flow(child)) { continue; }
		float p[2];
		p[along] = cursor;
		p[across] = node->pos[across]
			+ node_padding_start(node, across)
			+ align_offset(align[across], inner[across] - child->size[across]);
		place_node(child, p[AXIS_X] + offset[AXIS_X], p[AXIS_Y] + offset[AXIS_Y]);
		cursor += child->size[along] + layout->child_gap;
	}
}

/*! A root's own size: the element's sizing against what it was given */
static float
root_size(const bgame_ui_node_t* node, int axis, float available) {
	return child_base_size(node, axis, available, false);
}

static void
layout_subtree(bgame_ui_node_t* node, bgame_ui_size_t available) {
	compute_node_min(node, AXIS_X);
	resolve_node_size(node, AXIS_X, root_size(node, AXIS_X, available.width));
	measure_node_contents(node);
	compute_node_min(node, AXIS_Y);
	resolve_node_size(node, AXIS_Y, root_size(node, AXIS_Y, available.height));
	apply_node_aspect(node);
}

static void
layout_floating(bgame_ui_tree_t* tree, bgame_ui_node_t* node) {
	const bgame_ui_floating_t* floating = &node->decl.floating;
	bgame_ui_rect_t target;
	switch (floating->attach) {
		case BGAME_UI_ATTACH_PARENT:
			target = node->parent->bounds;
			break;
		case BGAME_UI_ATTACH_ROOT:
			target = tree->root.bounds;
			break;
		case BGAME_UI_ATTACH_ELEMENT: {
			bgame_ui_node_t* anchor = find_node(tree, floating->target);
			if (anchor != NULL && !anchor->hidden) {
				target = anchor->bounds;
			} else if (!bgame_ui_bounds(floating->target, &target)) {
				node->hidden = true;
				return;
			}
		} break;
		case BGAME_UI_ATTACH_NONE:
			return;
	}

	layout_subtree(node, (bgame_ui_size_t){ target.width, target.height });
	float x = target.x
		+ align_offset(floating->attach_points.target.x, target.width)
		- align_offset(floating->attach_points.element.x, node->size[AXIS_X])
		+ floating->offset.x;
	float y = target.y
		+ align_offset(floating->attach_points.target.y, target.height)
		- align_offset(floating->attach_points.element.y, node->size[AXIS_Y])
		+ floating->offset.y;
	place_node(node, x, y);
}

static void
sort_floating(bgame_ui_tree_t* tree) {
	bgame_ui_node_t** roots = bgame_alloc_for_frame(
		sizeof(bgame_ui_node_t*) * (size_t)BMAX(tree->num_floating, 1),
		_Alignof(bgame_ui_node_t*)
	);
	int count = 0;
	for (bgame_ui_node_t* node = tree->first_floating; node != NULL; node = node->next_floating) {
		// Insertion keeps declaration order among equal z
		int i = count++;
		while (i > 0 && roots[i - 1]->decl.floating.z > node->decl.floating.z) {
			roots[i] = roots[i - 1];
			--i;
		}
		roots[i] = node;
	}
	tree->floating = roots;
}

static void
layout_tree(bgame_ui_tree_t* tree) {
	bgame_ui_node_t* root = &tree->root;
	layout_subtree(root, tree->input.size);
	place_node(root, 0.f, 0.f);

	// In declaration order, so a target is placed before what attaches to it
	for (bgame_ui_node_t* node = tree->first_floating; node != NULL; node = node->next_floating) {
		layout_floating(tree, node);
	}
	sort_floating(tree);
}

// }}}

// Transitions {{{

static void
translate_subtree(bgame_ui_node_t* node, float dx, float dy) {
	for (bgame_ui_node_t* child = node->first_child; child != NULL; child = child->next_sibling) {
		child->bounds.x += dx;
		child->bounds.y += dy;
		translate_subtree(child, dx, dy);
	}
}

static void
make_node_pose(bgame_ui_node_t* node, const bgame_ui_transform_t* transform, float weight) {
	if (weight <= 0.f) { return; }
	bool any = transform->offset.x != 0.f || transform->offset.y != 0.f
		|| transform->shrink.x != 0.f || transform->shrink.y != 0.f
		|| transform->rotation != 0.f
		|| transform->fade != 0.f;
	if (!any) { return; }

	bgame_ui_rect_t box = node->bounds;
	CF_V2 center = cf_v2(box.x + box.width * 0.5f, box.y + box.height * 0.5f);
	CF_V2 pivot = cf_v2(
		center.x + transform->pivot.x * box.width,
		center.y + transform->pivot.y * box.height
	);
	CF_V2 scale = cf_v2(1.f - transform->shrink.x * weight, 1.f - transform->shrink.y * weight);
	CF_V2 offset = cf_mul(transform->offset, weight);
	// UI space is y down, so a counter-clockwise turn on screen is a negative angle here
	float rotation = -transform->rotation * weight;

	node->pose = (bgame_ui_pose_t){
		.transform = cf_mul_m32(
			cf_make_transform_TSR(cf_add(pivot, offset), scale, rotation),
			cf_make_translation(cf_neg(pivot))
		),
		.opacity = 1.f - transform->fade * weight,
	};
	node->has_pose = true;
}

static void
run_transitions(bgame_ui_tree_t* tree) {
	barena_t* arena = bgame_frame_arena();
	float dt = tree->input.dt;

	for (bgame_ui_node_t* node = tree->first_transition; node != NULL; node = node->next_transition) {
		if (node->hidden) { continue; }
		const bgame_ui_transition_t* tr = &node->decl.transition;
		bgame_ui_rect_t target = node->bounds;
		// A subtree leaves with its ancestor, so a child's exit pose plays along
		bool exiting = tr->exiting;
		for (const bgame_ui_node_t* p = node->parent; p != NULL && !exiting; p = p->parent) {
			exiting = p->decl.transition.duration > 0.f && p->decl.transition.exiting;
		}

		bgame_ui_transition_record_t* previous = NULL;
		if (ui_previous != NULL) {
			bgame_ui_transition_record_t** slot = bhamt_get(&ui_previous->transitions, node->id);
			if (slot != NULL) { previous = *slot; }
		}

		bgame_ui_transition_record_t* rec = bgame_alloc_for_frame(
			sizeof(bgame_ui_transition_record_t), _Alignof(bgame_ui_transition_record_t)
		);
		if (previous == NULL) {
			*rec = (bgame_ui_transition_record_t){
				.initial = target,
				.target = target,
				.displayed = target,
				.phase = BGAME_UI_TRANSITION_ENTERING,
			};
		} else {
			*rec = *previous;
			rec->elapsed += dt;
			// A reversal mid-phase picks up where the other phase was, so mirrored poses meet
			float done = tr->duration > 0.f ? BMIN(previous->elapsed / tr->duration, 1.f) : 1.f;
			if (exiting && !previous->exiting) {
				rec->phase = BGAME_UI_TRANSITION_EXITING;
				rec->elapsed = previous->phase == BGAME_UI_TRANSITION_ENTERING ? tr->duration * (1.f - done) : 0.f;
				rec->initial = previous->displayed;
			} else if (!exiting && previous->phase == BGAME_UI_TRANSITION_EXITING) {
				rec->phase = done < 1.f ? BGAME_UI_TRANSITION_ENTERING : BGAME_UI_TRANSITION_IDLE;
				rec->elapsed = tr->duration * (1.f - done);
				rec->initial = previous->displayed;
			}
			if (!rect_equal(target, previous->target)) {
				rec->initial = previous->displayed;
				if (rec->phase == BGAME_UI_TRANSITION_IDLE || rec->phase == BGAME_UI_TRANSITION_MOVING) {
					rec->phase = BGAME_UI_TRANSITION_MOVING;
					rec->elapsed = 0.f;
				}
			}
			rec->target = target;
		}
		rec->exiting = exiting;

		float progress = tr->duration > 0.f ? BMIN(rec->elapsed / tr->duration, 1.f) : 1.f;
		bgame_ui_curve_fn_t curve = tr->curve != NULL ? tr->curve : cf_smoothstep;
		float t = curve(progress);
		rec->displayed = rect_lerp(rec->initial, rec->target, t);
		if (
			progress >= 1.f
			&& (rec->phase == BGAME_UI_TRANSITION_MOVING || rec->phase == BGAME_UI_TRANSITION_ENTERING)
		) {
			rec->phase = BGAME_UI_TRANSITION_IDLE;
			rec->initial = rec->target;
		}

		node->transition = rec;
		node->transition_state = (bgame_ui_transition_state_t){ .phase = rec->phase, .progress = progress };
		node->unhoverable = rec->phase == BGAME_UI_TRANSITION_ENTERING || rec->phase == BGAME_UI_TRANSITION_EXITING;

		translate_subtree(node, rec->displayed.x - target.x, rec->displayed.y - target.y);
		node->bounds = rec->displayed;
		switch (rec->phase) {
			case BGAME_UI_TRANSITION_ENTERING: make_node_pose(node, &tr->enter, 1.f - t); break;
			case BGAME_UI_TRANSITION_EXITING: make_node_pose(node, &tr->exit, t); break;
			default: break;
		}

		bhamt_put(&tree->transitions, node->id, rec, arena);
	}
}

// }}}

// Clipping and commands {{{

static void
compute_node_clips(bgame_ui_node_t* node, bgame_ui_rect_t outer) {
	node->clip = rect_intersect(node->bounds, outer);
	node->clipped_out = rect_empty(node->clip);

	bgame_ui_rect_t inner = outer;
	if (node->decl.clip.horizontal) {
		inner.x = node->clip.x;
		inner.width = node->clip.width;
	}
	if (node->decl.clip.vertical) {
		inner.y = node->clip.y;
		inner.height = node->clip.height;
	}
	for (bgame_ui_node_t* child = node->first_child; child != NULL; child = child->next_sibling) {
		if (node_in_flow(child)) { compute_node_clips(child, inner); }
	}
}

static bgame_ui_box_t
node_box(const bgame_ui_node_t* node) {
	return (bgame_ui_box_t){
		.id = node->id,
		.bounds = node->bounds,
		.clip = node->clip,
		.transition = node->transition != NULL ? &node->transition_state : NULL,
		.userdata = node->decl.content.userdata,
	};
}

static void
push_command(bgame_ui_tree_t* tree, bgame_ui_command_t command) {
	tree->commands[tree->num_commands++] = command;
}

static void
emit_node_commands(bgame_ui_tree_t* tree, bgame_ui_node_t* node) {
	if (node->hidden) { return; }
	bgame_ui_box_t box = node_box(node);

	if (node->has_pose) {
		push_command(tree, (bgame_ui_command_t){ .type = BGAME_UI_CMD_POSE_BEGIN, .box = box, .pose = node->pose });
	}
	if (node->decl.content.draw != NULL && !node->clipped_out) {
		push_command(tree, (bgame_ui_command_t){ .type = BGAME_UI_CMD_DRAW, .box = box, .draw = node->decl.content.draw });
	}

	bool clip = node_clips(node) && !node->clipped_out && node->first_child != NULL;
	if (clip) {
		bgame_ui_box_t clip_box = box;
		clip_box.bounds = node->clip;
		push_command(tree, (bgame_ui_command_t){ .type = BGAME_UI_CMD_CLIP_BEGIN, .box = clip_box });
	}
	for (bgame_ui_node_t* child = node->first_child; child != NULL; child = child->next_sibling) {
		if (node_in_flow(child)) { emit_node_commands(tree, child); }
	}
	if (clip) {
		push_command(tree, (bgame_ui_command_t){ .type = BGAME_UI_CMD_CLIP_END, .box = box });
	}
	if (node->has_pose) {
		push_command(tree, (bgame_ui_command_t){ .type = BGAME_UI_CMD_POSE_END, .box = box });
	}
}

static void
emit_tree_commands(bgame_ui_tree_t* tree) {
	bgame_ui_rect_t screen = tree->root.bounds;
	compute_node_clips(&tree->root, screen);
	for (int i = 0; i < tree->num_floating; ++i) {
		compute_node_clips(tree->floating[i], screen);
	}

	int capacity = tree->num_content + 2 * tree->num_clip + 2 * tree->num_transition + 1;
	tree->commands = bgame_alloc_for_frame(
		sizeof(bgame_ui_command_t) * (size_t)capacity,
		_Alignof(bgame_ui_command_t)
	);
	tree->num_commands = 0;

	emit_node_commands(tree, &tree->root);
	for (int i = 0; i < tree->num_floating; ++i) {
		emit_node_commands(tree, tree->floating[i]);
	}

	bgame_ui_node_t* selected = find_node(tree, bgame_ui_inspect_highlight());
	if (selected != NULL && !selected->hidden) {
		push_command(tree, (bgame_ui_command_t){ .type = BGAME_UI_CMD_HIGHLIGHT, .box = node_box(selected) });
	}
}

// }}}

// Hover {{{

static bool
node_hoverable_at(const bgame_ui_node_t* node, CF_V2 p) {
	return !node->hidden && !node->unhoverable && rect_contains(node->clip, p);
}

/*! The chain of containing elements down from one, the last declared sibling first */
static int
descend_over_chain(const bgame_ui_node_t* node, CF_V2 p, bgame_ui_id_t* out, int count) {
	out[count++] = node->id;
	for (bgame_ui_node_t* child = node->last_child; child != NULL; child = child->prev_sibling) {
		if (node_in_flow(child) && node_hoverable_at(child, p)) {
			return descend_over_chain(child, p, out, count);
		}
	}
	return count;
}

static void
compute_over_list(bgame_ui_tree_t* tree, const bgame_ui_tree_t* previous, CF_V2 p) {
	tree->over = NULL;
	tree->num_over = 0;
	if (previous == NULL) { return; }

	// Deep enough for any chain
	bgame_ui_id_t* out = bgame_alloc_for_frame(
		sizeof(bgame_ui_id_t) * (size_t)BMAX(previous->num_nodes, 1),
		_Alignof(bgame_ui_id_t)
	);

	// Floating subtrees cover the flow; the topmost one under the pointer takes it
	for (int i = previous->num_floating - 1; i >= 0; --i) {
		const bgame_ui_node_t* root = previous->floating[i];
		if (root->decl.floating.passthrough || !node_hoverable_at(root, p)) { continue; }
		tree->over = out;
		tree->num_over = descend_over_chain(root, p, out, 0);
		return;
	}
	for (bgame_ui_node_t* child = previous->root.last_child; child != NULL; child = child->prev_sibling) {
		if (node_in_flow(child) && node_hoverable_at(child, p)) {
			tree->over = out;
			tree->num_over = descend_over_chain(child, p, out, 0);
			return;
		}
	}
}

// }}}

// Frame {{{

/*! The game sets imgui up, so there may be no context */
static bool
imgui_has_pointer(void) {
	return ImGui_GetCurrentContext() != NULL && ImGui_GetIO()->WantCaptureMouse;
}

static void
read_app_input(bgame_ui_frame_t* input) {
	int width, height;
	cf_app_get_size(&width, &height);
	*input = (bgame_ui_frame_t){
		.size = { (float)width, (float)height },
		.dt = CF_DELTA_TIME,
		.wheel = cf_mouse_wheel_motion(),
	};
	if (imgui_has_pointer()) {
		// Off screen and up, so nothing is hovered, pressed or kept captured under the window
		input->pointer = cf_v2(-1.f, -1.f);
		input->wheel = 0.f;
		return;
	}
	input->pointer = cf_v2(cf_mouse_x(), cf_mouse_y());
	input->pointer_down = cf_mouse_down(CF_MOUSE_BUTTON_LEFT);
	input->pointer_pressed = cf_mouse_just_pressed(CF_MOUSE_BUTTON_LEFT);
	input->pointer_released = cf_mouse_just_released(CF_MOUSE_BUTTON_LEFT);
	input->pointer_alt_pressed = cf_mouse_just_pressed(CF_MOUSE_BUTTON_RIGHT);
}

static bhamt_hash_t
state_key_hash(const void* gkey, size_t size) {
	(void)size;
	const ui_state_key_t* key = gkey;
	ChibiHash64Ctx ctx = chibihash64_init(0);
	chibihash64_append(&ctx, &key->tag, sizeof(key->tag));
	chibihash64_append(&ctx, &key->id, sizeof(key->id));
	return chibihash64_finish(&ctx);
}

static bool
state_key_eq(const void* glhs, const void* grhs, size_t size) {
	(void)size;
	const ui_state_key_t* lhs = glhs;
	const ui_state_key_t* rhs = grhs;
	return lhs->tag == rhs->tag && lhs->id == rhs->id;
}

static bhamt_hash_t
ptr_hash(const void* key, size_t size) {
	(void)size;
	return chibihash64(key, sizeof(void*), 0);
}

static bool
ptr_eq(const void* glhs, const void* grhs, size_t size) {
	(void)size;
	const void* const* lhs = glhs;
	const void* const* rhs = grhs;
	return *lhs == *rhs;
}

void
bgame_ui_begin(const bgame_ui_frame_t* frame) {
	if (ui_scope != NULL) {
		BFMT_ERROR("UI frame begun inside another");
		return;
	}
	bgame_ui_frame_t input;
	if (frame != NULL) {
		input = *frame;
	} else {
		read_app_input(&input);
	}

	ui_previous = ui_current;
	bgame_ui_tree_t* tree = bgame_alloc_for_frame(sizeof(bgame_ui_tree_t), _Alignof(bgame_ui_tree_t));
	*tree = (bgame_ui_tree_t){ .input = input };
	tree->root.decl.layout.sizing.width = BUI_FIXED(input.size.width);
	tree->root.decl.layout.sizing.height = BUI_FIXED(input.size.height);
	tree->root.bounds = (bgame_ui_rect_t){ 0.f, 0.f, input.size.width, input.size.height };
	// Integer keys: the defaults, so nothing points into the module
	bhamt_init(&tree->index, NULL, NULL);
	bhamt_init(&tree->transitions, NULL, NULL);
	ui_current = tree;

	compute_over_list(tree, ui_previous, input.pointer);
	bool moved = ui_previous != NULL
		&& (ui_previous->input.pointer.x != input.pointer.x || ui_previous->input.pointer.y != input.pointer.y);
	if (bgame_ui_inspect_frame(&input, tree->over, tree->num_over)) {
		// The pick owns the pointer: the UI sees none
		input.pointer = cf_v2(-1.f, -1.f);
		input.pointer_down = false;
		input.pointer_pressed = false;
		input.pointer_released = false;
		tree->over = NULL;
		tree->num_over = 0;
	}
	bgame_ui_focus_begin(&(bgame_ui_pointer_t){
		.x = input.pointer.x,
		.y = input.pointer.y,
		.moved = moved,
		.down = input.pointer_down,
		.pressed = input.pointer_pressed,
		.released = input.pointer_released,
		.over = tree->over,
		.num_over = tree->num_over,
	});

	// Swap UI state. The tables outlive the module, so the functions are wrapped.
	ui_previous_states = ui_current_states;
	bhamt_init(
		&ui_current_states,
		BGAME_FN_WRAPPER(state_key_hash),
		BGAME_FN_WRAPPER(state_key_eq)
	);

	// Reset context stacks
	bhamt_init(&ui_ctx_stacks, BGAME_FN_WRAPPER(ptr_hash), BGAME_FN_WRAPPER(ptr_eq));
	bhamt_init(&ui_frame_strings, BGAME_FN_WRAPPER(ptr_hash), BGAME_FN_WRAPPER(ptr_eq));

	ui_root_scope = (ui_scope_t){ .node = &tree->root };
	ui_scope = &ui_root_scope;
	ui_open = &tree->root;
	ui_duplicate_reported = false;
}

void
bgame_ui_end(void) {
	if (ui_scope == NULL) {
		BFMT_ERROR("UI frame ended without one begun");
		return;
	}
	bgame_ui_tree_t* tree = ui_current;
	if (ui_open != &tree->root || ui_scope != &ui_root_scope) {
		BFMT_ERROR("UI frame ended with an element or id still open");
	}

	layout_tree(tree);
	run_transitions(tree);
	emit_tree_commands(tree);
	bgame_ui_focus_end();

	ui_previous = tree;
	ui_scope = NULL;
	ui_open = NULL;
}

// }}}

// Translation {{{

static const char*
ui_translate(void* userdata, const char* context, const char* key) {
	return bgame_translation_lookup(userdata, context, key);
}

static void
ui_report_error(void* userdata, const char* context, const char* key, const char* template, int offset) {
	bgame_translation_t* translation = userdata;
	if (translation != NULL && template != key) {
		BFMT_WARN(
			"Bad translation at offset ", offset, " in \"", template, "\" for \"", key, "\"",
			context != NULL ? " in context " : "", context != NULL ? context : ""
		);
		bgame_translation_drop(translation, context, key);
	} else {
		BFMT_WARN("Bad template at offset ", offset, " in \"", template, "\"");
	}
}

void
bgame_ui_set_translation(bgame_translation_t* translation) {
	ui_translation = translation;
}

bgame_translation_t*
bgame_ui_translation(void) {
	return ui_translation;
}

bfmt_locale_t*
bgame_ui_locale(void) {
	bgame_translation_t* translation = ui_translation;
	int version = translation != NULL ? bgame_asset_version(translation) : 0;
	if (
		!ui_locale_cache.valid
		|| ui_locale_cache.translation != translation
		|| ui_locale_cache.version != version
	) {
		const char* language = translation != NULL ? bgame_translation_language(translation) : BGAME_UI_SOURCE_LANGUAGE;
		ui_locale_cache.locale = (bfmt_locale_t){
			.plural_rule = bfmt_cldr_plural_rule(language),
			.translate = translation != NULL ? ui_translate : NULL,
			.report_error = ui_report_error,
			.userdata = translation,
		};
		ui_locale_cache.valid = true;
		ui_locale_cache.translation = translation;
		ui_locale_cache.version = version;
	}
	return &ui_locale_cache.locale;
}

// }}}

// State and context {{{

void*
bgame_ui__state(size_t size, size_t alignment, const void* tag) {
	ui_state_key_t key = {
		.id = bgame_ui_scope_id(),
		.tag = tag,
	};
	barena_t* frame_arena = bgame_frame_arena();
	void** state_ptr = bhamt_upsert(&ui_current_states, key, frame_arena);
	if (*state_ptr == NULL) {  // Never seen this frame
		*state_ptr = bgame_alloc_for_frame(size, alignment);

		void** previous_state_ptr = bhamt_get(&ui_previous_states, key);
		if (previous_state_ptr == NULL) {  // Never seen before
			memset(*state_ptr, 0, size);
		} else {  // Seen last frame
			memcpy(*state_ptr, *previous_state_ptr, size);
		}
	}

	return *state_ptr;
}

void*
bgame_ui__push_ctx(size_t size, size_t alignment, ptrdiff_t offset, const void* tag) {
	barena_t* frame_arena = bgame_frame_arena();

	ui_ctx_stack_t* stack = bhamt_upsert(&ui_ctx_stacks, tag, frame_arena);

	// Alloc a new context
	bgame_ui__ctx_link_t* ctx;
	if (stack->pool != NULL) {  // From pool
		ctx = stack->pool;
		stack->pool = ctx->next;
	} else {  // From arena
		ctx = bgame_alloc_for_frame(size, alignment);
	}

	// Copy from current context or zero-initialize
	if (stack->current != NULL) {
		memcpy(ctx, stack->current, size);
	} else {
		memset(ctx, 0, size);
	}

	// Link to existing stack
	ctx->next = stack->current;
	stack->current = ctx;

	return (char*)ctx + offset;
}

void*
bgame_ui__pop_ctx(ptrdiff_t offset, const void* tag) {
	ui_ctx_stack_t* stack = bhamt_get(&ui_ctx_stacks, tag);
	if (stack == NULL || stack->current == NULL) { return NULL; }

	bgame_ui__ctx_link_t* ctx = stack->current;
	stack->current = ctx->next;
	ctx->next = stack->pool;
	stack->pool = ctx;

	return stack->current != NULL ? (char*)stack->current + offset : NULL;
}

const void*
bgame_ui__peek_ctx(size_t size, size_t alignment, ptrdiff_t offset, const void* tag) {
	barena_t* frame_arena = bgame_frame_arena();
	ui_ctx_stack_t* stack = bhamt_upsert(&ui_ctx_stacks, tag, frame_arena);

	if (stack->current != NULL) {
		return (char*)stack->current + offset;
	}

	if (stack->empty == NULL) {
		stack->empty = bgame_alloc_for_frame(size, alignment);
		memset(stack->empty, 0, size);
	}
	return (char*)stack->empty + offset;
}

// }}}
