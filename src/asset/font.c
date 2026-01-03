#include <bgame/asset/font.h>
#include <bgame/asset.h>
#include <cute_draw.h>
#include <blog.h>

static bool
bgame_font_load(
	bgame_asset_bundle_t* bundle,
	void* asset,
	const char* path
) {
	bgame_font_t* font = asset;

	if (font->name != NULL) {
		cf_destroy_font(font->name);
		font->name = NULL;
	}

	CF_Result result = cf_make_font(path, path);
	if (result.code == CF_RESULT_SUCCESS) {
		font->name = sintern(path);
		return true;
	} else {
		BLOG_ERROR("Could not load font: %s", result.details != NULL ? result.details : "No details");
		return false;
	}
}

static void
bgame_font_unload(
	bgame_asset_bundle_t* bundle,
	void* asset
) {
	bgame_font_t* font = asset;
	if (font->name != NULL) {
		cf_destroy_font(font->name);
	}
}

BGAME_ASSET_TYPE(font) = {
	.name = "font",
	.size = sizeof(bgame_font_t),
	.load = bgame_font_load,
	.unload = bgame_font_unload,
};

const char*
bgame_load_font(struct bgame_asset_bundle_s* bundle, const char* path) {
	bgame_font_t* asset = bgame_asset_load(bundle, &font, path);
	if (asset != NULL && asset->name != NULL) {
		return asset->name;
	} else {
		return NULL;
	}
}
