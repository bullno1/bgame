#include <bgame/ui/sprite.h>
#include <bgame/allocator/frame.h>
#include <cute_sprite.h>

static BGAME_UI_STATE_TAG(CF_Sprite) bgame_ui_sprite_tag;

static void
render_sprite(const Clay_RenderCommand* command, void* userdata) {
	bgame_ui_sprite_config_t config = *(bgame_ui_sprite_config_t*)userdata;
	Clay_BoundingBox bbox = command->boundingBox;

	CF_Sprite sprite = *config.sprite;
	sprite.transform.p.x =  bbox.x + bbox.width  * 0.5f;
	sprite.transform.p.y = -bbox.y - bbox.height * 0.5f;
	// Always scale, to maintain size or aspect ratio, set option in Clay
	sprite.scale = cf_v2(bbox.width / (float)sprite.w, bbox.height / (float)sprite.h);

	switch (config.mode) {
		case BGAME_UI_SPRITE_STRETCH:
			cf_draw_sprite(&sprite);
			break;
		case BGAME_UI_SPRITE_9_SLICE:
			cf_draw_sprite_9_slice(&sprite);
			break;
		case BGAME_UI_SPRITE_9_SLICE_TILED:
			cf_draw_sprite_9_slice_tiled(&sprite);
			break;
	}
}

Clay_CustomElementConfig
bgame_ui_sprite(bgame_ui_sprite_config_t config) {
	return bgame_custom_ui_element(render_sprite, bgame_make_frame_copy(config));
}

struct CF_Sprite*
bgame_ui_sprite_at_anim(const struct CF_Sprite* sprite, const char* animation) {
	CF_Sprite* sprite_instance = bgame_ui_state(bgame_ui_sprite_tag);
	animation = sintern(animation);
	bool should_update = true;

	if (sprite_instance->id != sprite->id) {
		*sprite_instance = *sprite;
		should_update = false;
	}

	if (sprite_instance->animation_name != animation) {
		cf_sprite_play(sprite_instance, animation);
		should_update = false;
	}

	if (should_update) {
		cf_sprite_update(sprite_instance);
	}

	return sprite_instance;
}
