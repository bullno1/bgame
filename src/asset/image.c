#include <bgame/asset/image.h>
#include <cute_image.h>

static bool
bgame_image_load(
	bgame_asset_bundle_t* bundle,
	void* asset,
	const char* path
) {
	CF_Image* image = asset;
	return cf_image_load_png(path, image).code == CF_RESULT_SUCCESS;
}

static void
bgame_image_unload(
	bgame_asset_bundle_t* bundle,
	void* asset
) {
	CF_Image* image = asset;
	cf_image_free(image);
}

BGAME_ASSET_TYPE(image) = {
	.name = "image",
	.size = sizeof(CF_Image),
	.load = bgame_image_load,
	.unload = bgame_image_unload,
};

struct CF_Image*
bgame_load_image(struct bgame_asset_bundle_s* bundle, const char* path) {
	return bgame_asset_load(bundle, &image, path);
}
