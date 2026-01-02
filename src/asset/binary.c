#include <bgame/asset.h>
#include <bgame/asset/binary.h>
#include <cute_file_system.h>

static bool
bgame_binary_load(
	bgame_asset_bundle_t* bundle,
	void* asset,
	const char* path
) {
	bgame_binary_t* binary = asset;

	CF_File* file = cf_fs_open_file_for_read(path);
	if (file == NULL) { return false; }
	size_t size = cf_fs_size(file);

	if (binary->data == NULL || binary->capacity < size) {
		binary->data = bgame_asset_realloc(bundle, binary->data, size);
		binary->capacity = size;
	}
	binary->size = size;
	bool result = cf_fs_read(file, binary->data, size) == size;
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
	return bgame_asset_load(bundle, &binary, path);
}
