#include "internal.h"
#include <bgame/reloadable.h>
#include <bgame/allocator/frame.h>
#include <bfmt_log.h>
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL_stdinc.h>
// The web has no processes to spawn: the page asks the server that serves it
#ifdef __EMSCRIPTEN__
#include <emscripten/em_js.h>
#else
#include <SDL3/SDL_process.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_error.h>
#endif

/*! The editor command, with `{file}` and `{line}` */
#ifndef BGAME_UI_EDITOR_ENV
#define BGAME_UI_EDITOR_ENV "BGAME_EDITOR"
#endif

#define MAX_EDITOR_ARGS 32

BGAME_VAR(bgame_ui_id_t, ui_inspect_selected) = 0;
BGAME_VAR(bool, ui_inspect_picking) = false;
BGAME_VAR(uint32_t, ui_inspect_pick_flags) = 0;
/*! The selection a cancelled pick goes back to */
BGAME_VAR(bgame_ui_id_t, ui_inspect_pick_restore) = 0;
/*! A lock swallows its press and the release that follows */
BGAME_VAR(bool, ui_inspect_swallowing) = false;
// Plain statics on purpose: per frame
static bgame_ui_id_t ui_inspect_hovered = 0;
static bgame_ui_id_t ui_inspect_picked = 0;
/*! A renderer called @ref bgame_ui_inspect_hover this frame */
static bool ui_inspect_shown = false;

static void open_in_editor(bgame_ui_loc_t loc);

bgame_ui_id_t
bgame_ui_inspect_highlight(void) {
	bool shown = ui_inspect_shown;
	bgame_ui_id_t hovered = ui_inspect_hovered;
	ui_inspect_shown = false;
	ui_inspect_hovered = 0;
	if (shown && hovered != 0) { return hovered; }
	if (shown || ui_inspect_picking) { return ui_inspect_selected; }
	return 0;
}

void
bgame_ui_inspect_hover(bgame_ui_id_t id) {
	ui_inspect_shown = true;
	ui_inspect_hovered = id;
}

// Picking {{{

void
bgame_ui_inspect_pick_start(uint32_t flags) {
	if (!ui_inspect_picking) { ui_inspect_pick_restore = ui_inspect_selected; }
	ui_inspect_picking = true;
	ui_inspect_pick_flags = flags;
}

void
bgame_ui_inspect_pick_stop(void) {
	ui_inspect_picking = false;
}

bool
bgame_ui_inspect_picking(void) {
	return ui_inspect_picking;
}

bgame_ui_id_t
bgame_ui_inspect_picked(void) {
	return ui_inspect_picked;
}

/*! The innermost element under the pointer, or the innermost named one, or the composite that owns it */
static bgame_ui_id_t
pick_candidate(const bgame_ui_id_t* over, int num_over) {
	const bgame_ui_tree_t* tree = bgame_ui_previous_tree();
	if (tree == NULL) { return 0; }
	bool named = ui_inspect_pick_flags & BGAME_UI_PICK_NAMED;
	const bgame_ui_node_t* found = NULL;
	for (int i = num_over - 1; i >= 0 && found == NULL; --i) {
		bgame_ui_node_t* const* slot = bhamt_get(&tree->index, over[i]);
		if (slot != NULL && (!named || (*slot)->name != NULL)) { found = *slot; }
	}
	if (found == NULL) { return 0; }
	if (!(ui_inspect_pick_flags & BGAME_UI_PICK_DEEP)) {
		// Up to an element nobody owns: the composite's root, or an outer composite's
		while (found->owner != 0) {
			bgame_ui_node_t* const* owner = bhamt_get(&tree->index, found->owner);
			if (owner == NULL) { break; }
			found = *owner;
		}
	}
	return found->id;
}

bool
bgame_ui_inspect_frame(const bgame_ui_frame_t* input, const bgame_ui_id_t* over, int num_over) {
	ui_inspect_picked = 0;
	if (ui_inspect_swallowing) {
		// Until the button that locked is up again, so the release lands nowhere either
		ui_inspect_swallowing = input->pointer_down || input->pointer_released;
		if (ui_inspect_swallowing) { return true; }
	}
	if (!ui_inspect_picking) { return false; }
	bool blocks = !(ui_inspect_pick_flags & BGAME_UI_PICK_PASS_POINTER);

	ui_inspect_selected = pick_candidate(over, num_over);
	if ((ui_inspect_pick_flags & BGAME_UI_PICK_CANCEL_ON_ALT) && input->pointer_alt_pressed) {
		ui_inspect_selected = ui_inspect_pick_restore;
		ui_inspect_picking = false;
	} else if ((ui_inspect_pick_flags & BGAME_UI_PICK_LOCK_ON_CLICK) && input->pointer_pressed) {
		ui_inspect_picked = ui_inspect_selected;
		ui_inspect_picking = false;
		ui_inspect_swallowing = blocks;
		if ((ui_inspect_pick_flags & BGAME_UI_PICK_EDIT) && ui_inspect_picked != 0) {
			bgame_ui_inspect_edit(ui_inspect_picked);
		}
	}
	return blocks;
}

// }}}

// Selection {{{

bgame_ui_id_t
bgame_ui_inspect_selected(void) {
	return ui_inspect_selected;
}

void
bgame_ui_inspect_select(bgame_ui_id_t id) {
	ui_inspect_selected = id;
}

bool
bgame_ui_inspect_edit(bgame_ui_id_t id) {
	const bgame_ui_tree_t* tree = bgame_ui_previous_tree();
	if (tree == NULL || id == 0) { return false; }
	bgame_ui_node_t* const* slot = bhamt_get(&tree->index, id);
	if (slot == NULL) { return false; }
	return bgame_ui_inspect_open((*slot)->loc);
}

bool
bgame_ui_inspect_open(bgame_ui_loc_t loc) {
	if (loc.file == NULL) { return false; }
	open_in_editor(loc);
	return true;
}

// }}}

// Jumping to the code {{{

#ifndef __EMSCRIPTEN__

/*! `{file}` and `{line}` replaced, in the frame arena */
static const char*
expand_template(const char* arg, size_t len, bgame_ui_loc_t loc) {
	char line[16];
	int line_len = snprintf(line, sizeof(line), "%d", loc.line);
	size_t file_len = strlen(loc.file);
	// Every placeholder is 6 bytes, so this bounds the expansion
	size_t capacity = len + 1 + (len / 6 + 1) * (file_len > (size_t)line_len ? file_len : (size_t)line_len);
	char* out = bgame_alloc_for_frame(capacity, _Alignof(char));
	size_t n = 0;
	for (size_t i = 0; i < len;) {
		if (len - i >= 6 && memcmp(arg + i, "{file}", 6) == 0) {
			memcpy(out + n, loc.file, file_len);
			n += file_len;
			i += 6;
		} else if (len - i >= 6 && memcmp(arg + i, "{line}", 6) == 0) {
			memcpy(out + n, line, (size_t)line_len);
			n += (size_t)line_len;
			i += 6;
		} else {
			out[n++] = arg[i++];
		}
	}
	out[n] = '\0';
	return out;
}

/*! Whitespace separates arguments, double quotes group them */
static int
split_command(const char* text, size_t size, bgame_ui_loc_t loc, const char** args) {
	int count = 0;
	size_t i = 0;
	while (i < size && count < MAX_EDITOR_ARGS - 1) {
		while (i < size && (text[i] == ' ' || text[i] == '\t' || text[i] == '\r' || text[i] == '\n')) { ++i; }
		if (i >= size) { break; }
		size_t start;
		size_t len;
		if (text[i] == '"') {
			start = ++i;
			while (i < size && text[i] != '"') { ++i; }
			len = i - start;
			if (i < size) { ++i; }
		} else {
			start = i;
			while (i < size && text[i] != ' ' && text[i] != '\t' && text[i] != '\r' && text[i] != '\n') { ++i; }
			len = i - start;
		}
		args[count++] = expand_template(text + start, len, loc);
	}
	args[count] = NULL;
	return count;
}

#else

/*! The dev server (`cmd/web/run`) runs the editor command for the page */
EM_JS(void, bgame_ui__request_editor, (const char* file, int line), {
	var url = '/__open-in-editor?file=' + encodeURIComponent(UTF8ToString(file)) + '&line=' + line;
	fetch(url, { method: 'POST' }).then(function (response) {
		if (!response.ok) {
			response.text().then(function (text) { console.warn('Could not open the editor: ' + text); });
		}
	}).catch(function (error) {
		console.warn('Could not open the editor: ' + error);
	});
})

#endif

/*! Logs the location, then runs the editor command when the user set one */
static void
open_in_editor(bgame_ui_loc_t loc) {
	if (loc.file == NULL) { return; }
	BFMT_INFO(loc.file, ":", loc.line);

#ifdef __EMSCRIPTEN__
	bgame_ui__request_editor(loc.file, loc.line);
#else
	const char* text = SDL_getenv(BGAME_UI_EDITOR_ENV);
	if (text == NULL) { return; }
	const char* args[MAX_EDITOR_ARGS];
	int count = split_command(text, strlen(text), loc, args);
	if (count == 0) { return; }

	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void*)args);
	SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);
	SDL_Process* process = SDL_CreateProcessWithProperties(props);
	SDL_DestroyProperties(props);
	if (process == NULL) {
		BFMT_ERROR("Could not run ", args[0], ": ", SDL_GetError());
		return;
	}
	SDL_DestroyProcess(process);
#endif
}

// }}}

// Names {{{

const char*
bgame_ui_sizing_name(bgame_ui_sizing_type_t type) {
	switch (type) {
		case BGAME_UI_SIZING_FIT: return "fit";
		case BGAME_UI_SIZING_GROW: return "grow";
		case BGAME_UI_SIZING_FIXED: return "fixed";
		case BGAME_UI_SIZING_PERCENT: return "percent";
	}
	return "?";
}

const char*
bgame_ui_align_name(bgame_ui_align_t align) {
	switch (align) {
		case BGAME_UI_ALIGN_START: return "start";
		case BGAME_UI_ALIGN_CENTER: return "center";
		case BGAME_UI_ALIGN_END: return "end";
	}
	return "?";
}

const char*
bgame_ui_attach_name(bgame_ui_attach_t attach) {
	switch (attach) {
		case BGAME_UI_ATTACH_NONE: return "none";
		case BGAME_UI_ATTACH_PARENT: return "parent";
		case BGAME_UI_ATTACH_ELEMENT: return "element";
		case BGAME_UI_ATTACH_ROOT: return "root";
	}
	return "?";
}

const char*
bgame_ui_transition_phase_name(bgame_ui_transition_phase_t phase) {
	switch (phase) {
		case BGAME_UI_TRANSITION_IDLE: return "idle";
		case BGAME_UI_TRANSITION_ENTERING: return "entering";
		case BGAME_UI_TRANSITION_MOVING: return "moving";
		case BGAME_UI_TRANSITION_EXITING: return "exiting";
	}
	return "?";
}

// }}}

// Nodes {{{

static void
fill_node(bgame_ui_inspect_node_t* out, const bgame_ui_node_t* node, int depth, int num_descendants) {
	*out = (bgame_ui_inspect_node_t){
		.id = node->id,
		.name = node->name,
		.index = node->index,
		.loc = node->loc,
		.def = node->def,
		.outlet = node->outlet,
		.owner = node->owner,
		.decl = &node->decl,
		.depth = depth,
		.num_descendants = num_descendants,
		.first_child = num_descendants > 0 ? out + 1 : NULL,
		.bounds = node->bounds,
		.clip = node->clip,
		.transition = node->transition != NULL ? &node->transition_state : NULL,
	};
}

/*! The children of a node after it, pre-order; returns the number written */
static int
flatten(bgame_ui_inspect_node_t* out, const bgame_ui_node_t* node, int depth) {
	int count = 0;
	bgame_ui_inspect_node_t* prev = NULL;
	for (const bgame_ui_node_t* child = node->first_child; child != NULL; child = child->next_sibling) {
		bgame_ui_inspect_node_t* slot = &out[count++];
		int num_descendants = flatten(out + count, child, depth + 1);
		fill_node(slot, child, depth, num_descendants);
		if (prev != NULL) { prev->next_sibling = slot; }
		prev = slot;
		count += num_descendants;
	}
	return count;
}

const bgame_ui_inspect_node_t*
bgame_ui_inspect_tree(void) {
	const bgame_ui_tree_t* tree = bgame_ui_previous_tree();
	if (tree == NULL) { return NULL; }
	bgame_ui_inspect_node_t* nodes = bgame_alloc_for_frame(
		sizeof(bgame_ui_inspect_node_t) * (size_t)(tree->num_nodes + 1),
		_Alignof(bgame_ui_inspect_node_t)
	);
	int count = flatten(nodes + 1, &tree->root, 1);
	fill_node(nodes, &tree->root, 0, count);
	return nodes;
}

const bgame_ui_inspect_node_t*
bgame_ui_inspect_find(const bgame_ui_inspect_node_t* tree, bgame_ui_id_t id) {
	if (tree == NULL || id == 0) { return NULL; }
	for (int i = 0; i < tree->num_descendants; ++i) {
		if (tree[1 + i].id == id) { return &tree[1 + i]; }
	}
	return NULL;
}

// }}}
