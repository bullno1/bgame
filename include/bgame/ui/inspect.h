#ifndef BGAME_UI_INSPECT_H
#define BGAME_UI_INSPECT_H

#include "../ui.h"

typedef enum {
	/*! Stop at the next left press, with what is under the pointer selected */
	BGAME_UI_PICK_LOCK_ON_CLICK = 1 << 0,
	/*! Stop at the next right press, with the selection from before the pick */
	BGAME_UI_PICK_CANCEL_ON_ALT = 1 << 1,
	/*! Skip anonymous elements */
	BGAME_UI_PICK_NAMED         = 1 << 2,
	/*! Let the UI see the pointer while picking. */
	BGAME_UI_PICK_PASS_POINTER  = 1 << 3,
	/**
	 * On lock, open the picked element's declaration with the editor command
	 *
	 * The command is built from the `BGAME_EDITOR` environment variable, processed
	 * as follow:
	 *
	 * * The string is split by white space into separate arguments
	 * * Double quotes group tokens into a single argument
	 * * `{file}` and `{line}` are substituted with the actual source location
	 *
	 * It is then executed without a shell.
	 * Example templates for popular editors:
	 *
	 *   * vim: `vim --servername GAME --remote-silent "+{line}|set cursorline|normal! $" {file}`
	 *     for a vim instance started with `--servername GAME`
	 *   * nvim: `nvim --server /tmp/nvim.sock --remote-send "<C-\><C-N>:silent drop {file} | {line}<CR>"`
	 *     for a nvim instance started with `--listen /tmp/nvim.sock`
	 *   * emacs: `emacsclient -n +{line} {file}`
	 *   * vscode: `code --goto {file}:{line}`
	 */
	BGAME_UI_PICK_EDIT          = 1 << 4,
	/*! Pick what a composite declares rather than the composite */
	BGAME_UI_PICK_DEEP          = 1 << 5,
} bgame_ui_pick_flag_t;

/*! Select what is under the pointer, every frame, until stopped or a flag ends it. The selection is highlighted meanwhile. */
void
bgame_ui_inspect_pick_start(uint32_t flags);

void
bgame_ui_inspect_pick_stop(void);

bool
bgame_ui_inspect_picking(void);

/*! The element a pick locked on this frame, 0 otherwise */
bgame_ui_id_t
bgame_ui_inspect_picked(void);

/*! The inspector's selection, 0 for none */
bgame_ui_id_t
bgame_ui_inspect_selected(void);

void
bgame_ui_inspect_select(bgame_ui_id_t id);

/**
 * Open an element's declaration with the editor command, false when the element is unknown
 *
 * The location is logged; the command in the `BGAME_EDITOR` environment variable, if
 * set, runs with `{file}` and `{line}` replaced. See @ref BGAME_UI_PICK_EDIT for templates.
 */
bool
bgame_ui_inspect_edit(bgame_ui_id_t id);

/*! @ref bgame_ui_inspect_edit for a location, false for one without a file */
bool
bgame_ui_inspect_open(bgame_ui_loc_t loc);

typedef struct bgame_ui_inspect_node_s bgame_ui_inspect_node_t;

/*! An element of last frame's tree */
struct bgame_ui_inspect_node_s {
	bgame_ui_id_t id;
	/*! NULL for an anonymous element and the root */
	const char* name;
	uint64_t index;
	/*! Where the element is placed: the caller's line for a composite's root */
	bgame_ui_loc_t loc;
	/*! Where a composite opens it, NULL file for an element placed where it is declared */
	bgame_ui_loc_t def;
	/*! The outlet it was declared in, NULL for none */
	const char* outlet;
	/*! The composite root whose code declared it, where a pick lands unless deep; 0 for none */
	bgame_ui_id_t owner;
	const bgame_ui_element_t* decl;
	/*! 0 for the root */
	int depth;
	/*! The subtree follows the node in memory, in declaration order */
	int num_descendants;
	/*! NULL for a leaf */
	const bgame_ui_inspect_node_t* first_child;
	/*! NULL for the last child */
	const bgame_ui_inspect_node_t* next_sibling;
	bgame_ui_rect_t bounds;
	/*! The part of @ref bounds left visible by clipping */
	bgame_ui_rect_t clip;
	/*! NULL for an element without a transition */
	const bgame_ui_transition_state_t* transition;
};

/*! The children of a node, in declaration order */
#define BGAME_UI_INSPECT_FOREACH_CHILD(child, parent) \
	for ( \
		const bgame_ui_inspect_node_t* child = (parent)->first_child; \
		child != NULL; \
		child = child->next_sibling \
	)

/**
 * Last frame's tree root
 *
 * The whole tree follows the root in memory, so `root + 1` for `root->num_descendants`
 * is every element in declaration order.
 */
const bgame_ui_inspect_node_t*
bgame_ui_inspect_tree(void);

/*! An element of a tree, NULL for one that is not in it */
const bgame_ui_inspect_node_t*
bgame_ui_inspect_find(const bgame_ui_inspect_node_t* tree, bgame_ui_id_t id);

/**
 * Called every frame a renderer shows the tree, with the element under its pointer, 0 for none
 *
 * The selection is highlighted while one does.
 */
void
bgame_ui_inspect_hover(bgame_ui_id_t id);

const char*
bgame_ui_sizing_name(bgame_ui_sizing_type_t type);

const char*
bgame_ui_align_name(bgame_ui_align_t align);

const char*
bgame_ui_attach_name(bgame_ui_attach_t attach);

const char*
bgame_ui_transition_phase_name(bgame_ui_transition_phase_t phase);

/*! The default renderer: the tree and the selection's properties with dear imgui */
void
bgame_ui_inspect(void);

#endif
