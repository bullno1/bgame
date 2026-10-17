#include <bgame/ui/fill.h>
#include <bgame/allocator/frame.h>
#include <cute_draw.h>

static void
render_fill(const bgame_ui_box_t* box, void* userdata) {
	CF_Color color = *(CF_Color*)userdata;
	cf_draw_push_color(color);
	cf_draw_box_fill(bgame_ui_aabb(box->bounds), 0.f);
	cf_draw_pop_color();
}

bgame_ui_content_t
bgame_ui_fill(CF_Color color) {
	return (bgame_ui_content_t){
		.draw = render_fill,
		.userdata = bgame_make_frame_copy(color),
		.debug_name = "fill",
	};
}
