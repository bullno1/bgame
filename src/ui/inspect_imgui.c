#include <bgame/ui/inspect.h>
#include <bgame/entrypoint.h>
#include <dcimgui.h>
#include <inttypes.h>

// Per frame: the row under the pointer
static bgame_ui_id_t ui_inspect_imgui_hovered = 0;

static void
draw_loc(const char* label, bgame_ui_loc_t loc) {
	if (loc.file == NULL) { return; }
	ImGui_LabelText(label, "%s:%d", bgame_project_file(loc.file), loc.line);
	ImGui_SameLine();
	ImGui_PushIDPtr(label);
	if (ImGui_Button("Open")) { bgame_ui_inspect_open(loc); }
	ImGui_PopID();
}

static const char*
content_name(const bgame_ui_element_t* decl) {
	if (decl->content.draw == NULL) { return ""; }
	return decl->content.debug_name != NULL ? decl->content.debug_name : "draw";
}

static void
draw_node(const bgame_ui_inspect_node_t* node) {
	bgame_ui_id_t selected = bgame_ui_inspect_selected();
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
		| ImGuiTreeNodeFlags_SpanFullWidth
		| ImGuiTreeNodeFlags_DefaultOpen;
	if (node->first_child == NULL) { flags |= ImGuiTreeNodeFlags_Leaf; }
	if (node->id == selected) { flags |= ImGuiTreeNodeFlags_Selected; }

	// What a composite declares is dimmed, as a pick skips it
	if (node->owner != 0) { ImGui_PushStyleColorImVec4(ImGuiCol_Text, *ImGui_GetStyleColorVec4(ImGuiCol_TextDisabled)); }
	// The id is the tree key, since the node itself is new every frame
	bool open = ImGui_TreeNodeExPtr(
		(const void*)(uintptr_t)node->id, flags,
		"%s #%" PRIu64 "  %s",
		node->name != NULL ? node->name : "(anonymous)", node->index,
		content_name(node->decl)
	);
	if (node->owner != 0) { ImGui_PopStyleColor(); }
	if (ImGui_IsItemHovered(0)) { ui_inspect_imgui_hovered = node->id; }
	if (ImGui_IsItemClicked()) { bgame_ui_inspect_select(selected == node->id ? 0 : node->id); }

	if (open) {
		BGAME_UI_INSPECT_FOREACH_CHILD(child, node) {
			draw_node(child);
		}
		ImGui_TreePop();
	}
}

static void
draw_sizing(const char* label, const bgame_ui_sizing_t* sizing) {
	ImGui_LabelText(label, "%s %.1f [%.0f, %.0f]", bgame_ui_sizing_name(sizing->type), sizing->value, sizing->min, sizing->max);
}

static void
draw_details(const bgame_ui_inspect_node_t* tree, const bgame_ui_inspect_node_t* node) {
	const bgame_ui_element_t* decl = node->decl;
	ImGui_SeparatorText(node->name != NULL ? node->name : "(anonymous)");
	draw_loc("Source", node->loc);
	draw_loc("Defined", node->def);
	if (node->outlet != NULL) { ImGui_LabelText("Outlet", "%s", node->outlet); }
	if (node->owner != 0) {
		const bgame_ui_inspect_node_t* owner = bgame_ui_inspect_find(tree, node->owner);
		if (owner != NULL) {
			ImGui_LabelText("Owner", "%s #%" PRIu64, owner->name != NULL ? owner->name : "(anonymous)", owner->index);
		}
	}
	ImGui_LabelText("Id", "%016" PRIx64, node->id);
	ImGui_LabelText("Bounds", "%.1f, %.1f  %.1f x %.1f", node->bounds.x, node->bounds.y, node->bounds.width, node->bounds.height);
	ImGui_LabelText("Content", "%.1f, %.1f  %.1f x %.1f", node->content.x, node->content.y, node->content.width, node->content.height);
	ImGui_LabelText("Visible", "%.1f, %.1f  %.1f x %.1f", node->clip.x, node->clip.y, node->clip.width, node->clip.height);
	draw_sizing("Width", &decl->layout.sizing.width);
	draw_sizing("Height", &decl->layout.sizing.height);
	ImGui_LabelText("Direction", "%s", decl->layout.direction == BGAME_UI_LEFT_TO_RIGHT ? "left to right" : "top to bottom");
	ImGui_LabelText("Padding", "%.0f %.0f %.0f %.0f", decl->layout.padding.left, decl->layout.padding.right, decl->layout.padding.top, decl->layout.padding.bottom);
	ImGui_LabelText("Child gap", "%.0f", decl->layout.child_gap);
	ImGui_LabelText("Child align", "%s, %s", bgame_ui_align_name(decl->layout.child_align.x), bgame_ui_align_name(decl->layout.child_align.y));
	if (decl->layout.aspect_ratio > 0.f) { ImGui_LabelText("Aspect ratio", "%.3f", decl->layout.aspect_ratio); }
	if (decl->clip.horizontal || decl->clip.vertical) {
		ImGui_LabelText("Clip", "%s%s offset %.0f, %.0f", decl->clip.horizontal ? "h" : "", decl->clip.vertical ? "v" : "", decl->clip.offset.x, decl->clip.offset.y);
	}
	if (decl->floating.attach != BGAME_UI_ATTACH_NONE) {
		ImGui_LabelText("Floating", "%s z %d%s", bgame_ui_attach_name(decl->floating.attach), decl->floating.z, decl->floating.passthrough ? " passthrough" : "");
	}
	if (decl->content.draw != NULL) {
		ImGui_LabelText("Content", "%s%s", content_name(decl), decl->content.measure != NULL ? ", measured" : "");
	}
	if (decl->focus.kind != BGAME_UI_FOCUS_NONE) {
		ImGui_LabelText("Focus", "%s flags %x claims %x", decl->focus.kind == BGAME_UI_FOCUS_GROUP ? "group" : "target", decl->focus.flags, decl->focus.claims);
	}
	if (node->transition != NULL) {
		ImGui_LabelText("Transition", "%s %.2f", bgame_ui_transition_phase_name(node->transition->phase), node->transition->progress);
	}
}

void
bgame_ui_inspect(void) {
	const bgame_ui_inspect_node_t* tree = bgame_ui_inspect_tree();
	if (tree == NULL) {
		ImGui_TextDisabled("No frame yet");
		return;
	}
	int num_commands;
	bgame_ui_commands(&num_commands);
	ImGui_Text("%d elements, %d commands", tree->num_descendants, num_commands);

	ui_inspect_imgui_hovered = 0;
	if (ImGui_BeginChild("Tree", (ImVec2){ 0.f, 300.f }, ImGuiChildFlags_ResizeY, 0)) {
		BGAME_UI_INSPECT_FOREACH_CHILD(child, tree) {
			draw_node(child);
		}
	}
	ImGui_EndChild();
	bgame_ui_inspect_hover(ui_inspect_imgui_hovered);

	bgame_ui_id_t shown = ui_inspect_imgui_hovered != 0 ? ui_inspect_imgui_hovered : bgame_ui_inspect_selected();
	const bgame_ui_inspect_node_t* node = bgame_ui_inspect_find(tree, shown);
	if (node != NULL) { draw_details(tree, node); }
}
