#include <bgame/ui/focus.h>
#include <bgame/allocator/frame.h>
#include <bgame/reloadable.h>
#include <bfmt_log.h>
#include <bhamt.h>
#include <bminmax.h>
#include <math.h>
#include <string.h>
#include "internal.h"

typedef struct focus_node_s focus_node_t;

struct focus_node_s {
	bgame_ui_id_t id;
	uint32_t flags;
	uint32_t claims;
	bool is_group;
	bool has_bounds;
	bgame_ui_rect_t bounds;

	focus_node_t* parent;
	focus_node_t* first_child;
	focus_node_t* last_child;
	focus_node_t* next_sibling;
	focus_node_t* prev_sibling;

	// Carried over from the last build by id
	bgame_ui_id_t remembered;
	bgame_ui_id_t restore;
};

typedef struct {
	focus_node_t root;
	int num_nodes;
	BHAMT_TABLE(bgame_ui_id_t, focus_node_t*) index;
} focus_tree_t;

typedef struct {
	focus_tree_t* previous;
	focus_tree_t* current;
	focus_node_t* open;

	bgame_ui_id_t focused;
	bgame_ui_id_t focused_before;
	bgame_ui_id_t autofocus;
	bgame_ui_id_t routed[32];
	bgame_ui_rect_t focused_bounds;

	bgame_ui_id_t hovered;
	bgame_ui_id_t captured;
	bgame_ui_id_t pointed;
	bool pointer_down;
	bool pointer_active;
	bgame_ui_pointer_policy_t policy;
	bgame_ui_input_mode_t mode;
} focus_state_t;

#define WRAP_FLAG(AXIS) ((AXIS) == 0 ? BGAME_UI_FOCUS_WRAP_H : BGAME_UI_FOCUS_WRAP_V)

BGAME_VAR(focus_state_t, bgame_ui_focus_state) = { 0 };

// Tree {{{

static focus_node_t*
find_node(focus_tree_t* tree, bgame_ui_id_t id) {
	if (tree == NULL || id == 0) { return NULL; }
	focus_node_t** slot = bhamt_get(&tree->index, id);
	return slot != NULL ? *slot : NULL;
}

static bool
is_enabled(const focus_node_t* node) {
	for (; node != NULL; node = node->parent) {
		if (node->flags & BGAME_UI_FOCUS_DISABLED) { return false; }
	}
	return true;
}

/*! Whether the element or an ancestor was exiting last frame; walks the UI tree, since the one leaving need not be a focus node */
static bool
is_leaving(bgame_ui_id_t id) {
	const bgame_ui_tree_t* tree = bgame_ui_previous_tree();
	if (tree == NULL || id == 0) { return false; }
	bgame_ui_node_t* const* slot = bhamt_get(&tree->index, id);
	for (const bgame_ui_node_t* node = slot != NULL ? *slot : NULL; node != NULL; node = node->parent) {
		if (node->transition != NULL && node->transition->phase == BGAME_UI_TRANSITION_EXITING) {
			return true;
		}
	}
	return false;
}

static bool
is_focusable(const focus_node_t* node) {
	if (node == NULL || node->parent == NULL) { return false; }
	if (node->is_group && !(node->flags & BGAME_UI_FOCUS_ENTERABLE)) { return false; }
	return is_enabled(node);
}

static bool
is_scope(const focus_node_t* node) {
	return node->parent == NULL
		|| (node->flags & (BGAME_UI_FOCUS_MODAL | BGAME_UI_FOCUS_ENTERABLE));
}

/*! The nearest strict ancestor focus stays inside of */
static focus_node_t*
scope_of(focus_node_t* node) {
	focus_node_t* scope = node->parent;
	while (!is_scope(scope)) { scope = scope->parent; }
	return scope;
}

static bool
is_inside(const focus_node_t* node, const focus_node_t* ancestor) {
	for (node = node->parent; node != NULL; node = node->parent) {
		if (node == ancestor) { return true; }
	}
	return false;
}

/*! Targets in a group, in declaration order; an enterable group counts as one */
static void
collect_targets(
	focus_node_t* group,
	const focus_node_t* exclude,
	focus_node_t** out,
	int* count
) {
	for (focus_node_t* child = group->first_child; child != NULL; child = child->next_sibling) {
		if (child->flags & BGAME_UI_FOCUS_DISABLED) { continue; }
		if (is_leaving(child->id)) { continue; }
		if (child != exclude && is_focusable(child)) { out[(*count)++] = child; }
		if (child->is_group && !(child->flags & BGAME_UI_FOCUS_ENTERABLE)) {
			collect_targets(child, exclude, out, count);
		}
	}
}

static focus_node_t**
collect_candidates(focus_tree_t* tree, focus_node_t* group, const focus_node_t* exclude, int* count) {
	focus_node_t** out = bgame_alloc_for_frame(
		sizeof(focus_node_t*) * (size_t)BMAX(tree->num_nodes, 1),
		_Alignof(focus_node_t*)
	);
	*count = 0;
	collect_targets(group, exclude, out, count);
	return out;
}

// }}}

// Geometry {{{

static float
rect_min(const bgame_ui_rect_t* rect, int axis) {
	return axis == 0 ? rect->x : rect->y;
}

static float
rect_max(const bgame_ui_rect_t* rect, int axis) {
	return axis == 0 ? rect->x + rect->width : rect->y + rect->height;
}

static float
rect_center(const bgame_ui_rect_t* rect, int axis) {
	return (rect_min(rect, axis) + rect_max(rect, axis)) * 0.5f;
}

static float
interval_distance(const bgame_ui_rect_t* a, const bgame_ui_rect_t* b, int axis) {
	float gap = BMAX(rect_min(a, axis) - rect_max(b, axis), rect_min(b, axis) - rect_max(a, axis));
	return BMAX(gap, 0.f);
}

/*! How far from `from` a target is, in a direction; negative for one behind it */
static float
score_direction(const bgame_ui_rect_t* from, const bgame_ui_rect_t* to, int axis, float sign) {
	float advance = sign * (rect_center(to, axis) - rect_center(from, axis));
	if (advance <= 0.f) { return -1.f; }

	int other = 1 - axis;
	float gap = sign > 0.f
		? rect_min(to, axis) - rect_max(from, axis)
		: rect_min(from, axis) - rect_max(to, axis);
	float misalign = fabsf(rect_center(to, other) - rect_center(from, other));
	return BMAX(gap, 0.f) + 2.f * interval_distance(from, to, other) + 0.1f * misalign;
}

static focus_node_t*
nearest_in_direction(
	focus_node_t** cands,
	int count,
	const bgame_ui_rect_t* from,
	int axis,
	float sign
) {
	focus_node_t* best = NULL;
	float best_score = 0.f;
	for (int i = 0; i < count; ++i) {
		float s = score_direction(from, &cands[i]->bounds, axis, sign);
		if (s < 0.f) { continue; }
		if (best == NULL || s < best_score) {
			best = cands[i];
			best_score = s;
		}
	}
	return best;
}

/*! The farthest target behind, for wrapping around */
static focus_node_t*
farthest_behind(
	focus_node_t** cands,
	int count,
	const bgame_ui_rect_t* from,
	int axis,
	float sign
) {
	int other = 1 - axis;
	focus_node_t* best = NULL;
	float best_score = 0.f;
	for (int i = 0; i < count; ++i) {
		const bgame_ui_rect_t* to = &cands[i]->bounds;
		float advance = sign * (rect_center(to, axis) - rect_center(from, axis));
		if (advance >= 0.f) { continue; }
		float misalign = fabsf(rect_center(to, other) - rect_center(from, other));
		float s = advance + 2.f * interval_distance(from, to, other) + 0.1f * misalign;
		if (best == NULL || s < best_score) {
			best = cands[i];
			best_score = s;
		}
	}
	return best;
}

static focus_node_t*
nearest_to(focus_node_t** cands, int count, const bgame_ui_rect_t* from) {
	focus_node_t* best = NULL;
	float best_dist = 0.f;
	for (int i = 0; i < count; ++i) {
		float dx = rect_center(&cands[i]->bounds, 0) - rect_center(from, 0);
		float dy = rect_center(&cands[i]->bounds, 1) - rect_center(from, 1);
		float dist = dx * dx + dy * dy;
		if (best == NULL || dist < best_dist) {
			best = cands[i];
			best_dist = dist;
		}
	}
	return best;
}

// }}}

// Choosing {{{

/*! Where focus lands when it goes into a group: remembered, default, nearest, first */
static focus_node_t*
resolve_into(focus_tree_t* tree, focus_node_t* group, const bgame_ui_rect_t* near) {
	int count;
	focus_node_t** cands = collect_candidates(tree, group, NULL, &count);
	if (count == 0) { return NULL; }

	if (group->remembered != 0) {
		for (int i = 0; i < count; ++i) {
			if (cands[i]->id == group->remembered) { return cands[i]; }
		}
	}
	for (int i = 0; i < count; ++i) {
		if (cands[i]->flags & BGAME_UI_FOCUS_DEFAULT) { return cands[i]; }
	}
	if (near != NULL) { return nearest_to(cands, count, near); }
	return cands[0];
}

/*! Somewhere to go when the focused node is gone */
static focus_node_t*
find_fallback(void) {
	focus_state_t* state = &bgame_ui_focus_state;
	focus_tree_t* tree = state->current;
	focus_node_t* old = find_node(state->previous, state->focused);
	if (old == NULL) { return resolve_into(tree, &tree->root, NULL); }

	for (focus_node_t* ancestor = old->parent; ancestor->parent != NULL; ancestor = ancestor->parent) {
		focus_node_t* survivor = find_node(tree, ancestor->id);
		if (survivor == NULL || is_leaving(ancestor->id)) {
			if (is_scope(ancestor) && ancestor->restore != 0) {
				focus_node_t* restored = find_node(tree, ancestor->restore);
				if (is_focusable(restored) && !is_leaving(restored->id)) { return restored; }
			}
			continue;
		}
		focus_node_t* target = resolve_into(tree, survivor, &old->bounds);
		if (target != NULL) { return target; }
	}

	return resolve_into(tree, &tree->root, &old->bounds);
}

static void
set_mode_navigation(void) {
	if (!bgame_ui_focus_state.pointer_active) {
		bgame_ui_focus_state.mode = BGAME_UI_INPUT_NAVIGATION;
	}
}

// }}}

// Frame {{{

static void
fill_bounds(focus_node_t* node) {
	focus_state_t* state = &bgame_ui_focus_state;
	if (node->parent != NULL && !node->has_bounds) {
		node->has_bounds = bgame_ui_element_bounds(node->id, &node->bounds);
	}

	bgame_ui_rect_t children = { 0 };
	bool any = false;
	for (focus_node_t* child = node->first_child; child != NULL; child = child->next_sibling) {
		fill_bounds(child);
		if (!child->has_bounds) { continue; }
		if (!any) {
			children = child->bounds;
			any = true;
		} else {
			float min_x = BMIN(children.x, child->bounds.x);
			float min_y = BMIN(children.y, child->bounds.y);
			float max_x = BMAX(children.x + children.width, child->bounds.x + child->bounds.width);
			float max_y = BMAX(children.y + children.height, child->bounds.y + child->bounds.height);
			children = (bgame_ui_rect_t){ min_x, min_y, max_x - min_x, max_y - min_y };
		}
	}
	if (!node->has_bounds && any) {
		node->bounds = children;
		node->has_bounds = true;
	}
}

static void
remember_focused(focus_tree_t* tree, focus_node_t* focused) {
	focus_state_t* state = &bgame_ui_focus_state;
	focus_node_t* before = find_node(tree, state->focused_before);

	for (focus_node_t* ancestor = focused->parent; ancestor != NULL; ancestor = ancestor->parent) {
		ancestor->remembered = focused->id;
		// Entering a scope: note where to go back to once it is gone
		if (
			is_scope(ancestor)
			&& ancestor->parent != NULL
			&& state->focused_before != state->focused
			&& (before == NULL || !is_inside(before, ancestor))
		) {
			ancestor->restore = state->focused_before;
		}
	}
}

void
bgame_ui_focus_begin(const bgame_ui_pointer_t* pointer) {
	focus_state_t* state = &bgame_ui_focus_state;

	state->previous = state->current;
	state->current = bgame_alloc_for_frame(sizeof(focus_tree_t), _Alignof(focus_tree_t));
	*state->current = (focus_tree_t){ .root.is_group = true };
	state->open = &state->current->root;
	state->autofocus = 0;

	state->pointer_down = pointer->down;
	state->pointer_active = pointer->moved || pointer->pressed || pointer->released;
	if (state->pointer_active) { state->mode = BGAME_UI_INPUT_POINTER; }

	bgame_ui_id_t under = 0;
	state->pointed = 0;
	focus_tree_t* tree = state->previous;
	if (tree != NULL) {
		focus_node_t* focused = find_node(tree, state->focused);
		focus_node_t* scope = focused != NULL ? scope_of(focused) : &tree->root;
		for (int i = pointer->num_over - 1; i >= 0; --i) {
			focus_node_t* node = find_node(tree, pointer->over[i]);
			if (node == NULL) { continue; }
			if (state->pointed == 0) { state->pointed = node->id; }
			if (is_focusable(node) && scope_of(node) == scope) {
				under = node->id;
				break;
			}
		}
	}

	if (pointer->pressed && under != 0) { state->captured = under; }
	state->hovered = state->captured != 0 ? state->captured : under;

	switch (state->policy) {
		case BGAME_UI_POINTER_FOLLOW:
			if ((pointer->moved || pointer->pressed) && under != 0) { state->focused = under; }
			break;
		case BGAME_UI_POINTER_CLICK:
			if (pointer->pressed && under != 0) { state->focused = under; }
			break;
		case BGAME_UI_POINTER_IGNORE:
			break;
	}
}

void
bgame_ui_focus_end(void) {
	focus_state_t* state = &bgame_ui_focus_state;
	focus_tree_t* tree = state->current;
	if (tree == NULL) { return; }
	if (state->open != &tree->root) {
		BFMT_ERROR("Unclosed focus node");
	}

	fill_bounds(&tree->root);

	if (state->autofocus != 0) {
		focus_node_t* node = find_node(tree, state->autofocus);
		if (node != NULL) {
			focus_node_t* target = node->is_group ? resolve_into(tree, node, NULL) : node;
			if (is_focusable(target)) { state->focused = target->id; }
		}
	}

	focus_node_t* focused = find_node(tree, state->focused);
	if (!is_focusable(focused) || is_leaving(state->focused)) {
		focused = find_fallback();
		state->focused = focused != NULL ? focused->id : 0;
	}

	if (focused != NULL) {
		remember_focused(tree, focused);
		state->focused_bounds = focused->bounds;
	} else {
		state->focused_bounds = (bgame_ui_rect_t){ 0 };
	}
	state->focused_before = state->focused;

	memset(state->routed, 0, sizeof(state->routed));
	if (!state->pointer_down) { state->captured = 0; }
	state->open = NULL;
}

// }}}

// Declaring {{{

void
bgame_ui_focus_open(bgame_ui_id_t id, const bgame_ui_focus_t* focus) {
	focus_state_t* state = &bgame_ui_focus_state;
	focus_tree_t* tree = state->current;
	if (tree == NULL || state->open == NULL) {
		BFMT_ERROR("Focus node declared outside of a UI frame");
		return;
	}

	focus_node_t* node = bgame_alloc_for_frame(sizeof(focus_node_t), _Alignof(focus_node_t));
	*node = (focus_node_t){
		.id = id,
		.flags = focus->flags,
		.claims = focus->claims,
		.is_group = focus->kind == BGAME_UI_FOCUS_GROUP,
		.parent = state->open,
		.prev_sibling = state->open->last_child,
	};
	if (state->open->last_child != NULL) {
		state->open->last_child->next_sibling = node;
	} else {
		state->open->first_child = node;
	}
	state->open->last_child = node;
	++tree->num_nodes;

	bool is_new;
	focus_node_t** slot = bhamt_upsert_ex(&tree->index, node->id, bgame_frame_arena(), &is_new);
	if (!is_new) {
		BFMT_ERROR("Duplicate focus node id ", node->id);
	}
	*slot = node;

	focus_node_t* before = find_node(state->previous, node->id);
	if (before != NULL) {
		node->remembered = before->remembered;
		node->restore = before->restore;
	} else if ((node->flags & BGAME_UI_FOCUS_AUTOFOCUS) && state->autofocus == 0) {
		state->autofocus = node->id;
	}

	state->open = node;
}

void
bgame_ui_focus_close(void) {
	focus_state_t* state = &bgame_ui_focus_state;
	if (state->open == NULL || state->open->parent == NULL) {
		BFMT_ERROR("Focus node closed without one open");
		return;
	}
	state->open = state->open->parent;
}

// }}}

// Querying {{{

bool
bgame_ui_focused(void) {
	focus_state_t* state = &bgame_ui_focus_state;
	return state->open != NULL && state->open->parent != NULL && state->open->id == state->focused;
}

bool
bgame_ui_focus_gained(void) {
	focus_state_t* state = &bgame_ui_focus_state;
	return bgame_ui_focused() && state->focused_before != state->open->id;
}

bool
bgame_ui_focus_lost(void) {
	focus_state_t* state = &bgame_ui_focus_state;
	return state->open != NULL
		&& state->open->parent != NULL
		&& state->open->id == state->focused_before
		&& state->open->id != state->focused;
}

bool
bgame_ui_focus_within(void) {
	focus_state_t* state = &bgame_ui_focus_state;
	if (bgame_ui_focused()) { return true; }
	if (state->open == NULL) { return false; }
	focus_node_t* focused = find_node(state->previous, state->focused);
	for (; focused != NULL; focused = focused->parent) {
		if (focused->id == state->open->id) { return true; }
	}
	return false;
}

bool
bgame_ui_focus_action(uint32_t actions) {
	focus_state_t* state = &bgame_ui_focus_state;
	if (state->open == NULL) { return false; }
	for (int bit = 0; bit < 32; ++bit) {
		if ((actions & (1u << bit)) && state->routed[bit] == state->open->id) { return true; }
	}
	return false;
}

// }}}

// Sending input {{{

static bool
route_from(focus_node_t* node, uint32_t actions) {
	focus_state_t* state = &bgame_ui_focus_state;
	bool any = false;
	for (; node != NULL && actions != 0; node = node->parent) {
		uint32_t taken = node->claims & actions;
		if (taken == 0) { continue; }
		for (int bit = 0; bit < 32; ++bit) {
			if (taken & (1u << bit)) { state->routed[bit] = node->id; }
		}
		actions &= ~taken;
		any = true;
	}
	return any;
}

bool
bgame_ui_focus_route(uint32_t actions) {
	focus_state_t* state = &bgame_ui_focus_state;
	focus_tree_t* tree = state->previous;
	if (tree == NULL) { return false; }
	set_mode_navigation();
	if (is_leaving(state->focused)) { return true; }
	return route_from(find_node(tree, state->focused), actions);
}

bool
bgame_ui_focus_route_pointer(uint32_t actions) {
	focus_state_t* state = &bgame_ui_focus_state;
	focus_tree_t* tree = state->previous;
	if (tree == NULL) { return false; }
	// The over chain never holds a leaving element, so no check is needed here
	return route_from(find_node(tree, state->pointed), actions);
}

bool
bgame_ui_focus_move(bgame_ui_focus_dir_t dir) {
	focus_state_t* state = &bgame_ui_focus_state;
	focus_tree_t* tree = state->previous;
	if (tree == NULL) { return false; }
	set_mode_navigation();
	if (is_leaving(state->focused)) { return false; }

	focus_node_t* cur = find_node(tree, state->focused);
	if (!is_focusable(cur)) {
		focus_node_t* target = resolve_into(tree, &tree->root, NULL);
		if (target == NULL) { return false; }
		state->focused = target->id;
		return true;
	}

	focus_node_t* target = NULL;
	switch (dir) {
		case BGAME_UI_FOCUS_ENTER:
			if (cur->is_group) { target = resolve_into(tree, cur, NULL); }
			break;
		case BGAME_UI_FOCUS_LEAVE:
			for (focus_node_t* a = cur->parent; a->parent != NULL; a = a->parent) {
				if ((a->flags & BGAME_UI_FOCUS_ENTERABLE) && is_enabled(a)) {
					target = a;
					break;
				}
			}
			break;
		case BGAME_UI_FOCUS_NEXT:
		case BGAME_UI_FOCUS_PREV: {
			int count;
			focus_node_t** cands = collect_candidates(tree, scope_of(cur), NULL, &count);
			int step = dir == BGAME_UI_FOCUS_NEXT ? 1 : count - 1;
			for (int i = 0; i < count; ++i) {
				if (cands[i] == cur) {
					target = cands[(i + step) % count];
					break;
				}
			}
			if (target == cur) { target = NULL; }
		} break;
		case BGAME_UI_FOCUS_LEFT:
		case BGAME_UI_FOCUS_RIGHT:
		case BGAME_UI_FOCUS_UP:
		case BGAME_UI_FOCUS_DOWN: {
			int axis = dir == BGAME_UI_FOCUS_LEFT || dir == BGAME_UI_FOCUS_RIGHT ? 0 : 1;
			float sign = dir == BGAME_UI_FOCUS_RIGHT || dir == BGAME_UI_FOCUS_DOWN ? 1.f : -1.f;
			focus_node_t* scope = scope_of(cur);
			int count;
			focus_node_t** cands = collect_candidates(tree, scope, cur, &count);
			target = nearest_in_direction(cands, count, &cur->bounds, axis, sign);
			if (target == NULL) {
				for (focus_node_t* group = cur->parent; group != NULL; group = group->parent) {
					if (group->flags & WRAP_FLAG(axis)) {
						cands = collect_candidates(tree, group, cur, &count);
						target = farthest_behind(cands, count, &cur->bounds, axis, sign);
						break;
					}
					if (group == scope) { break; }
				}
			}
		} break;
	}

	if (target == NULL) { return false; }
	state->focused = target->id;
	return true;
}

void
bgame_ui_focus_set(bgame_ui_id_t id) {
	bgame_ui_focus_state.focused = id;
}

bgame_ui_id_t
bgame_ui_focus_id(void) {
	return bgame_ui_focus_state.focused;
}

bgame_ui_rect_t
bgame_ui_focus_bounds(void) {
	return bgame_ui_focus_state.focused_bounds;
}

bgame_ui_id_t
bgame_ui_hovered(void) {
	return bgame_ui_focus_state.hovered;
}

void
bgame_ui_set_pointer_policy(bgame_ui_pointer_policy_t policy) {
	bgame_ui_focus_state.policy = policy;
}

bgame_ui_input_mode_t
bgame_ui_input_mode(void) {
	return bgame_ui_focus_state.mode;
}

// }}}
