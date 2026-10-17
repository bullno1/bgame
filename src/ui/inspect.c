#include "internal.h"
#include <bgame/reloadable.h>
#include <bgame/allocator/frame.h>
#include <bfmt_log.h>
#include <dcimgui.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL_stdinc.h>
// The web has no processes to spawn
#ifndef __EMSCRIPTEN__
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
static bool ui_inspect_shown = false;

static void open_in_editor(bgame_ui_loc_t loc);

bgame_ui_id_t
bgame_ui_inspect_highlight(void) {
	bool shown = ui_inspect_shown;
	ui_inspect_shown = false;
	if (shown && ui_inspect_hovered != 0) { return ui_inspect_hovered; }
	if (shown || ui_inspect_picking) { return ui_inspect_selected; }
	return 0;
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

/*! The innermost element under the pointer, or the innermost named one */
static bgame_ui_id_t
pick_candidate(const bgame_ui_id_t* over, int num_over) {
	if (!(ui_inspect_pick_flags & BGAME_UI_PICK_NAMED)) {
		return num_over > 0 ? over[num_over - 1] : 0;
	}
	const bgame_ui_tree_t* tree = bgame_ui_previous_tree();
	if (tree == NULL) { return 0; }
	for (int i = num_over - 1; i >= 0; --i) {
		bgame_ui_node_t* const* slot = bhamt_get(&tree->index, over[i]);
		if (slot != NULL && (*slot)->name != NULL) { return over[i]; }
	}
	return 0;
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
	if (slot == NULL || (*slot)->loc.file == NULL) { return false; }
	open_in_editor((*slot)->loc);
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

#endif

/*! Logs the location, then runs the editor command when the user set one */
static void
open_in_editor(bgame_ui_loc_t loc) {
	if (loc.file == NULL) { return; }
	BFMT_INFO(loc.file, ":", loc.line);

#ifndef __EMSCRIPTEN__
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

static void
draw_loc(const char* label, bgame_ui_loc_t loc) {
	if (loc.file == NULL) { return; }
	ImGui_LabelText(label, "%s:%d", loc.file, loc.line);
	ImGui_SameLine();
	ImGui_PushIDPtr(label);
	if (ImGui_Button("Open")) { open_in_editor(loc); }
	ImGui_PopID();
}

// }}}

static const char*
sizing_name(bgame_ui_sizing_type_t type) {
	switch (type) {
		case BGAME_UI_SIZING_FIT: return "fit";
		case BGAME_UI_SIZING_GROW: return "grow";
		case BGAME_UI_SIZING_FIXED: return "fixed";
		case BGAME_UI_SIZING_PERCENT: return "percent";
	}
	return "?";
}

static const char*
align_name(bgame_ui_align_t align) {
	switch (align) {
		case BGAME_UI_ALIGN_START: return "start";
		case BGAME_UI_ALIGN_CENTER: return "center";
		case BGAME_UI_ALIGN_END: return "end";
	}
	return "?";
}

static const char*
attach_name(bgame_ui_attach_t attach) {
	switch (attach) {
		case BGAME_UI_ATTACH_NONE: return "none";
		case BGAME_UI_ATTACH_PARENT: return "parent";
		case BGAME_UI_ATTACH_ELEMENT: return "element";
		case BGAME_UI_ATTACH_ROOT: return "root";
	}
	return "?";
}

static const char*
phase_name(bgame_ui_transition_phase_t phase) {
	switch (phase) {
		case BGAME_UI_TRANSITION_IDLE: return "idle";
		case BGAME_UI_TRANSITION_ENTERING: return "entering";
		case BGAME_UI_TRANSITION_MOVING: return "moving";
		case BGAME_UI_TRANSITION_EXITING: return "exiting";
	}
	return "?";
}

static void
draw_node(const bgame_ui_node_t* node) {
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
		| ImGuiTreeNodeFlags_SpanFullWidth
		| ImGuiTreeNodeFlags_DefaultOpen;
	if (node->first_child == NULL) { flags |= ImGuiTreeNodeFlags_Leaf; }
	if (node->id == ui_inspect_selected) { flags |= ImGuiTreeNodeFlags_Selected; }

	// The id is the tree key, since the node itself is new every frame
	const char* content = node->decl.content.draw != NULL
		? (node->decl.content.debug_name != NULL ? node->decl.content.debug_name : "draw")
		: "";
	bool open = ImGui_TreeNodeExPtr(
		(const void*)(uintptr_t)node->id, flags,
		"%s #%" PRIu64 "  %s",
		node->name != NULL ? node->name : "(anonymous)", node->index,
		content
	);
	if (ImGui_IsItemHovered(0)) { ui_inspect_hovered = node->id; }
	if (ImGui_IsItemClicked()) { ui_inspect_selected = ui_inspect_selected == node->id ? 0 : node->id; }

	if (open) {
		for (const bgame_ui_node_t* child = node->first_child; child != NULL; child = child->next_sibling) {
			draw_node(child);
		}
		ImGui_TreePop();
	}
}

static void
draw_sizing(const char* label, const bgame_ui_sizing_t* sizing) {
	ImGui_LabelText(label, "%s %.1f [%.0f, %.0f]", sizing_name(sizing->type), sizing->value, sizing->min, sizing->max);
}

static void
draw_details(const bgame_ui_node_t* node) {
	const bgame_ui_element_t* decl = &node->decl;
	ImGui_SeparatorText(node->name != NULL ? node->name : "(anonymous)");
	draw_loc("Source", node->loc);
	ImGui_LabelText("Id", "%016" PRIx64, node->id);
	ImGui_LabelText("Bounds", "%.1f, %.1f  %.1f x %.1f", node->bounds.x, node->bounds.y, node->bounds.width, node->bounds.height);
	ImGui_LabelText("Visible", "%.1f, %.1f  %.1f x %.1f", node->clip.x, node->clip.y, node->clip.width, node->clip.height);
	draw_sizing("Width", &decl->layout.sizing.width);
	draw_sizing("Height", &decl->layout.sizing.height);
	ImGui_LabelText("Direction", "%s", decl->layout.direction == BGAME_UI_LEFT_TO_RIGHT ? "left to right" : "top to bottom");
	ImGui_LabelText("Padding", "%.0f %.0f %.0f %.0f", decl->layout.padding.left, decl->layout.padding.right, decl->layout.padding.top, decl->layout.padding.bottom);
	ImGui_LabelText("Child gap", "%.0f", decl->layout.child_gap);
	ImGui_LabelText("Child align", "%s, %s", align_name(decl->layout.child_align.x), align_name(decl->layout.child_align.y));
	if (decl->layout.aspect_ratio > 0.f) { ImGui_LabelText("Aspect ratio", "%.3f", decl->layout.aspect_ratio); }
	if (decl->clip.horizontal || decl->clip.vertical) {
		ImGui_LabelText("Clip", "%s%s offset %.0f, %.0f", decl->clip.horizontal ? "h" : "", decl->clip.vertical ? "v" : "", decl->clip.offset.x, decl->clip.offset.y);
	}
	if (decl->floating.attach != BGAME_UI_ATTACH_NONE) {
		ImGui_LabelText("Floating", "%s z %d%s", attach_name(decl->floating.attach), decl->floating.z, decl->floating.passthrough ? " passthrough" : "");
	}
	if (decl->content.draw != NULL) {
		ImGui_LabelText(
			"Content", "%s%s",
			decl->content.debug_name != NULL ? decl->content.debug_name : "draw",
			decl->content.measure != NULL ? ", measured" : ""
		);
	}
	if (decl->focus.kind != BGAME_UI_FOCUS_NONE) {
		ImGui_LabelText("Focus", "%s flags %x claims %x", decl->focus.kind == BGAME_UI_FOCUS_GROUP ? "group" : "target", decl->focus.flags, decl->focus.claims);
	}
	if (node->transition != NULL) {
		ImGui_LabelText("Transition", "%s %.2f", phase_name(node->transition_state.phase), node->transition_state.progress);
	}
}

void
bgame_ui_inspect(void) {
	ui_inspect_shown = true;
	ui_inspect_hovered = 0;

	const bgame_ui_tree_t* tree = bgame_ui_previous_tree();
	if (tree == NULL) {
		ImGui_TextDisabled("No frame yet");
		return;
	}

	ImGui_Text("%d elements, %d commands", tree->num_nodes, tree->num_commands);

	if (ImGui_BeginChild("Tree", (ImVec2){ 0.f, 300.f }, ImGuiChildFlags_ResizeY, 0)) {
		for (const bgame_ui_node_t* child = tree->root.first_child; child != NULL; child = child->next_sibling) {
			draw_node(child);
		}
	}
	ImGui_EndChild();

	bgame_ui_id_t selected = ui_inspect_hovered != 0 ? ui_inspect_hovered : ui_inspect_selected;
	bgame_ui_node_t* const* slot = selected != 0 ? bhamt_get(&tree->index, selected) : NULL;
	if (slot != NULL) { draw_details(*slot); }
}
