#include "internal.h"
#include "../internal.h"
#include <bgame/reloadable.h>
#include <bgame/shader.h>
#include <bfmt_log.h>
#include <cute_app.h>
#include <cute_draw.h>
#include "gen/overlay_shd.h"

#define MAX_POSE_DEPTH 32

BGAME_VAR(CF_Shader, ui_overlay_shader) = { 0 };

BGAME_AFTER_RELOAD(bgame_ui_render) {
	bgame_load_draw_shader(&ui_overlay_shader, bgame_ui_overlay_shd_bytecode);
}

static CF_Rect
scissor_of(bgame_ui_rect_t rect) {
	int x0 = (int)floorf(rect.x);
	int y0 = (int)floorf(rect.y);
	int x1 = (int)ceilf(rect.x + rect.width);
	int y1 = (int)ceilf(rect.y + rect.height);
	return (CF_Rect){ x0, y0, x1 - x0, y1 - y0 };
}

static void
draw_highlight_rect(bgame_ui_rect_t rect, CF_Color color, float fill_alpha) {
	CF_Aabb aabb = bgame_ui_aabb(rect);
	cf_draw_push_color(cf_make_color_rgba_f(color.r, color.g, color.b, fill_alpha));
	cf_draw_box_fill(aabb, 0.f);
	cf_draw_pop_color();
	cf_draw_push_color(color);
	cf_draw_box(aabb, 1.f, 0.f);
	cf_draw_pop_color();
}

/*! The padding in orange, the content in blue over it, as a browser inspector does */
static void
draw_highlight(const bgame_ui_box_t* box) {
	draw_highlight_rect(box->bounds, cf_make_color_rgba_f(1.f, 0.5f, 0.f, 1.f), 0.25f);
	draw_highlight_rect(box->content, cf_make_color_rgba_f(0.3f, 0.6f, 1.f, 1.f), 0.25f);
}

void
bgame_ui_render(void) {
	int count;
	const bgame_ui_command_t* commands = bgame_ui_commands(&count);
	if (ui_overlay_shader.id == 0) {
		bgame_load_draw_shader(&ui_overlay_shader, bgame_ui_overlay_shd_bytecode);
	}

	int width, height;
	cf_app_get_size(&width, &height);
	cf_draw_push();
	cf_draw_translate(-(float)width * 0.5f, (float)height * 0.5f);

	// Poses are in UI space, y down; the drawing space under the translation is y up
	CF_M3x2 flip = cf_make_scale(cf_v2(1.f, -1.f));
	float opacity[MAX_POSE_DEPTH + 1] = { 1.f };
	int depth = 0;

	for (int i = 0; i < count; ++i) {
		const bgame_ui_command_t* command = &commands[i];
		switch (command->type) {
			case BGAME_UI_CMD_DRAW:
				command->draw(&command->box, command->box.userdata);
				break;
			case BGAME_UI_CMD_CLIP_BEGIN:
				cf_draw_push_scissor(scissor_of(command->box.bounds));
				break;
			case BGAME_UI_CMD_CLIP_END:
				cf_draw_pop_scissor();
				break;
			case BGAME_UI_CMD_POSE_BEGIN: {
				if (depth >= MAX_POSE_DEPTH) {
					BFMT_ERROR("UI poses nested deeper than ", MAX_POSE_DEPTH);
					break;
				}
				cf_draw_push();
				cf_draw_transform(cf_mul_m32(cf_mul_m32(flip, command->pose.transform), flip));
				float o = opacity[depth] * command->pose.opacity;
				opacity[++depth] = o;
				// Colors are premultiplied, so every channel scales
				cf_draw_push_shader(ui_overlay_shader);
				cf_draw_set_uniform_color("u_overlay_color", cf_make_color_rgba_f(o, o, o, o));
			} break;
			case BGAME_UI_CMD_POSE_END:
				if (depth == 0) { break; }
				--depth;
				cf_draw_pop_shader();
				cf_draw_pop();
				if (depth > 0) {
					float o = opacity[depth];
					cf_draw_set_uniform_color("u_overlay_color", cf_make_color_rgba_f(o, o, o, o));
				}
				break;
			case BGAME_UI_CMD_HIGHLIGHT:
				draw_highlight(&command->box);
				break;
		}
	}

	cf_draw_pop();
}
