#include <bgame/asset.h>
#include <bgame/asset/binary.h>
#include <cute_file_system.h>

static bgame_asset_load_result_t
bgame_binary_load(
	bgame_asset_bundle_t* bundle,
	void* asset,
	const char* path,
	const void* args
) {
	bgame_binary_t* binary = asset;
	if (!bgame_asset_source_changed(bundle, asset)) {
		return BGAME_ASSET_UNCHANGED;
	}

	CF_File* file = cf_fs_open_file_for_read(path);
	if (file == NULL) { return BGAME_ASSET_ERROR; }
	size_t size = cf_fs_size(file);

	if (binary->data == NULL || binary->capacity < size) {
		binary->data = bgame_asset_realloc(bundle, binary->data, size);
		binary->capacity = size;
	}
	binary->size = size;
	bgame_asset_load_result_t result = cf_fs_read(file, binary->data, size) == size
		? BGAME_ASSET_LOADED
		: BGAME_ASSET_ERROR;
	cf_fs_close(file);
	return result;
}

static void
bgame_binary_unload(bgame_asset_bundle_t* bundle, void* asset) {
	bgame_binary_t* binary = asset;
	bgame_asset_free(bundle, binary->data);
}

BGAME_ASSET_TYPE(binary) = {
	.name = "binary",
	.size = sizeof(bgame_binary_t),
	.load = bgame_binary_load,
	.unload = bgame_binary_unload,
};

bgame_binary_t*
bgame_load_binary(struct bgame_asset_bundle_s* bundle, const char* path) {
	return bgame_asset_load(bundle, &binary, path, NULL);
}
