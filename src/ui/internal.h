#ifndef BGAME_UI_INTERNAL_H
#define BGAME_UI_INTERNAL_H

#include <bgame/ui.h>
#include <bgame/ui/inspect.h>
#include <bhamt.h>

typedef struct bgame_ui_node_s bgame_ui_node_t;

typedef struct {
	bgame_ui_rect_t initial;
	bgame_ui_rect_t target;
	bgame_ui_rect_t displayed;
	float elapsed;
	bgame_ui_transition_phase_t phase;
	bool exiting;
} bgame_ui_transition_record_t;

struct bgame_ui_node_s {
	bgame_ui_id_t id;
	/*! Frame copy, NULL for an anonymous element */
	const char* name;
	uint64_t index;
	bgame_ui_loc_t loc;
	/*! Where a composite opens it, NULL file for an element placed where it is declared */
	bgame_ui_loc_t def;
	/*! The outlet it was declared in, NULL for none; a frame copy */
	const char* outlet;
	/*! A composite's root: it owns what is declared under it */
	bool composite;
	/*! The composite root whose code declared it, 0 for none */
	bgame_ui_id_t owner;
	bgame_ui_element_t decl;

	bgame_ui_node_t* parent;
	bgame_ui_node_t* first_child;
	bgame_ui_node_t* last_child;
	bgame_ui_node_t* next_sibling;
	bgame_ui_node_t* prev_sibling;
	bgame_ui_node_t* next_floating;
	bgame_ui_node_t* next_transition;
	int depth;

	// Layout
	float min_size[2];
	float size[2];
	float pos[2];
	bgame_ui_size_t content_size;
	bgame_ui_rect_t bounds;
	/*! The bounds inside the padding */
	bgame_ui_rect_t content;
	/*! The visible part of the bounds */
	bgame_ui_rect_t clip;
	bool clipped_out;
	/*! A floating element whose target is nowhere */
	bool hidden;

	// Transition
	bgame_ui_transition_record_t* transition;
	bgame_ui_transition_state_t transition_state;
	bgame_ui_pose_t pose;
	bool has_pose;
	bool unhoverable;
};

typedef BHAMT_TABLE(bgame_ui_id_t, bgame_ui_node_t*) bgame_ui_node_index_t;
typedef BHAMT_TABLE(bgame_ui_id_t, bgame_ui_transition_record_t*) bgame_ui_transition_table_t;

/*! One frame's tree, in that frame's arena */
typedef struct {
	bgame_ui_node_t root;
	bgame_ui_node_index_t index;
	bgame_ui_transition_table_t transitions;
	bgame_ui_node_t* first_floating;
	bgame_ui_node_t* last_floating;
	bgame_ui_node_t* first_transition;
	/*! Floating roots in paint order */
	bgame_ui_node_t** floating;
	int num_floating;
	int num_nodes;
	int num_content;
	int num_clip;
	int num_transition;
	bgame_ui_command_t* commands;
	int num_commands;
	const bgame_ui_id_t* over;
	int num_over;
	bgame_ui_frame_t input;
} bgame_ui_tree_t;

/*! Last frame's tree, NULL before the first frame */
const bgame_ui_tree_t*
bgame_ui_previous_tree(void);

/*! The tree bgame_ui_commands draws: this frame's once it ended, else the last one */
const bgame_ui_tree_t*
bgame_ui_rendered_tree(void);

/*! The box of an element of this frame, after the layout, false for one that is not laid out */
bool
bgame_ui_element_bounds(bgame_ui_id_t id, bgame_ui_rect_t* bounds);

/*! The element the inspector highlights this frame, 0 for none. Once per frame. */
bgame_ui_id_t
bgame_ui_inspect_highlight(void);

bool
bgame_ui_inspect_frame(const bgame_ui_frame_t* input, const bgame_ui_id_t* over, int num_over);

// Focus hooks

typedef struct {
	float x;
	float y;
	bool moved;
	bool down;
	bool pressed;
	bool released;
	/*! The elements under the pointer, outermost first */
	const bgame_ui_id_t* over;
	int num_over;
} bgame_ui_pointer_t;

/*! Before the elements */
void
bgame_ui_focus_begin(const bgame_ui_pointer_t* pointer);

/*! After the layout */
void
bgame_ui_focus_end(void);

void
bgame_ui_focus_open(bgame_ui_id_t id, const bgame_ui_focus_t* focus);

void
bgame_ui_focus_close(void);

#endif
