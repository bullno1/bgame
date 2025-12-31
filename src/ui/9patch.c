#include <bgame/ui/9patch.h>
#include <bgame/asset/9patch.h>

static void
bgame_ui_render_9patch(
	const Clay_RenderCommand* command,
	void* userdata
) {
	bgame_draw_9patch(userdata, bgame_ui_aabb(command->boundingBox));
}

Clay_CustomElementConfig
bgame_ui_9patch(const struct bgame_9patch_s* nine_patch) {
	return bgame_custom_ui_element(bgame_ui_render_9patch, (void*)nine_patch);
}
