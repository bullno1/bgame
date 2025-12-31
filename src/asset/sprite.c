#include <blog.h>
#include <bgame/asset.h>
#include <bgame/asset/sprite.h>
#include <cute_image.h>
#include <cute_sprite.h>
#include <string.h>

static bgame_asset_load_result_t
bgame_sprite_load(
	bgame_asset_bundle_t* bundle,
	void* asset,
	const char* path,
	const void* args
) {
	CF_Sprite* sprite = asset;
	if (!bgame_asset_source_changed(bundle, sprite)) {
		return BGAME_ASSET_UNCHANGED;
	}

	if (bgame_file_has_extension(path, "png")) {
		if (sprite->easy_sprite_id == 0) {
			CF_Result result = { 0 };
			*sprite = cf_make_easy_sprite_from_png(path, &result);
			if (result.code != CF_RESULT_SUCCESS) {
				BLOG_ERROR("Could not load sprite: %s", result.details != NULL ? result.details : "No details");
				return BGAME_ASSET_ERROR;
			} else {
				return BGAME_ASSET_LOADED;
			}
		} else {
			CF_Image png;
			CF_Result result = cf_image_load_png(path, &png);
			if (result.code != CF_RESULT_SUCCESS) {
				BLOG_ERROR("Could not load sprite: %s", result.details != NULL ? result.details : "No details");
				return BGAME_ASSET_ERROR;
			}
			cf_easy_sprite_update_pixels(sprite, png.pix);
			cf_image_free(&png);
			return BGAME_ASSET_LOADED;
		}
	} else if (
		bgame_file_has_extension(path, "ase")
		||
		bgame_file_has_extension(path, "aseprite")
	) {
		if (sprite->name == NULL) {
			*sprite = cf_make_sprite(path);
			return BGAME_ASSET_LOADED;
		} else {
			// Preserve transformation
			CF_V2 scale = sprite->scale;
			*sprite = cf_sprite_reload(sprite);
			sprite->scale = scale;
			return BGAME_ASSET_LOADED;
		}
	} else {
		BLOG_ERROR("Unsupported sprite format");
		return BGAME_ASSET_ERROR;
	}
}

static void
bgame_sprite_unload(
	bgame_asset_bundle_t* bundle,
	void* asset
) {
	CF_Sprite* sprite = asset;
	if (sprite->easy_sprite_id > 0) {
		cf_easy_sprite_unload(sprite);
	} else if (sprite->name != NULL) {
		cf_sprite_unload(sprite->name);
	}
}

BGAME_ASSET_TYPE(sprite) = {
	.name = "sprite",
	.size = sizeof(CF_Sprite),
	.load = bgame_sprite_load,
	.unload = bgame_sprite_unload,
};

CF_Sprite*
bgame_load_sprite(struct bgame_asset_bundle_s* bundle, const char* path) {
	return bgame_asset_load(bundle, &sprite, path, NULL);
}
