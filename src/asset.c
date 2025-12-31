#include "internal.h"
#include <bgame/reloadable.h>
#include <bgame/asset.h>
#include <bgame/allocator.h>
#include <bgame/allocator/tracked.h>
#include <bgame/allocator/frame.h>
#include <blog.h>
#include <bhash.h>
#include <cute_file_system.h>
#include <cute_string.h>

#if BGAME_RELOADABLE
#include <bresmon.h>
#endif

BGAME_DECLARE_TRACKED_ALLOCATOR(bgame_asset)

typedef struct {
	size_t len;
	const char* chars;
} bgame_str_t;

typedef struct {
	bgame_asset_type_t* type;
	bgame_str_t path;
} bgame_asset_key_t;

typedef struct {
	int ref_count;
	bool dynamic;
	bgame_asset_key_t key;

	int source_version;
	int loaded_version;

#if BGAME_RELOADABLE
	int previous_version;
	bresmon_watch_t* watch;
#endif

	_Alignas(BGAME_MAX_ALIGN_TYPE) char data[];
} bgame_asset_t;

typedef BHASH_TABLE(bgame_asset_key_t, bgame_asset_t*) bgame_asset_cache_t;

#if BGAME_RELOADABLE

static bool bgame_asset_initialized = false;
BGAME_VAR(int, bgame_asset_code_version) = 0;

typedef BHASH_TABLE(bgame_str_t, bgame_asset_type_t*) bgame_asset_registry_t;
BGAME_VAR(bgame_asset_registry_t, bgame_asset_registry) = { 0 };

typedef BHASH_TABLE(bgame_asset_type_t*, bgame_asset_type_t*) bgame_asset_type_translation_t;
BGAME_VAR(bgame_asset_type_translation_t, bgame_asset_type_translation) = { 0 };

AUTOLIST_DEFINE(bgame_asset_type_list)

#endif

struct bgame_asset_bundle_s {
	bgame_asset_cache_t assets;
	bool loading;
#if BGAME_RELOADABLE
	int code_version;
	bresmon_t* monitor;
#endif
};

#if BGAME_RELOADABLE

static bhash_hash_t
bgame_str_hash(const void* key, size_t size) {
	const bgame_str_t* str = key;
	return bhash_hash(str->chars, str->len);
}

#endif

static bool
bgame_str_eq(const void* lhs, const void* rhs, size_t size) {
	const bgame_str_t* lhs_str = lhs;
	const bgame_str_t* rhs_str = rhs;
	return lhs_str->len == rhs_str->len
		&& memcmp(lhs_str->chars, rhs_str->chars, lhs_str->len) == 0;
}

static bhash_hash_t
bgame_asset_key_hash(const void* key, size_t size) {
	const bgame_asset_key_t* asset_key = key;
	uint64_t type_hash = bhash__chibihash64(&asset_key->type, sizeof(asset_key->type), 0);
	return bhash__chibihash64(asset_key->path.chars, asset_key->path.len, type_hash);
}

static bool
bgame_asset_key_eq(const void* lhs, const void* rhs, size_t size) {
	const bgame_asset_key_t* lhs_key = lhs;
	const bgame_asset_key_t* rhs_key = rhs;
	return lhs_key->type == rhs_key->type
		&& bgame_str_eq(&lhs_key->path, &rhs_key->path, 0);
}

static inline bgame_str_t
bgame_asset_strcpy(const char* str) {
	size_t len = strlen(str);
	char* chars = bgame_malloc(len + 1, bgame_asset);
	memcpy(chars, str, len);
	chars[len] = '\0';
	return (bgame_str_t){
		.len = len,
		.chars = chars,
	};
}

static inline void
bgame_asset_strfree(bgame_str_t str) {
	bgame_free((char*)str.chars, bgame_asset);
}

static inline bgame_str_t
bgame_asset_strref(const char* str) {
	size_t len = strlen(str);
	return (bgame_str_t){
		.len = len,
		.chars = str,
	};
}

static void
bgame_asset_init(void) {
#if BGAME_RELOADABLE
	if (bgame_asset_initialized) { return; }

	{
		bhash_config_t config = bhash_config_default();
		config.hash = bgame_str_hash;
		config.eq = bgame_str_eq;
		config.memctx = bgame_asset;
		bhash_reinit(&bgame_asset_registry, config);
	}
	{
		bhash_config_t config = bhash_config_default();
		config.memctx = bgame_asset;
		bhash_reinit(&bgame_asset_type_translation, config);
		bhash_clear(&bgame_asset_type_translation);
	}

	AUTOLIST_FOREACH(entry, bgame_asset_type_list) {
		bgame_str_t entry_name = bgame_asset_strref(entry->name);
		bhash_index_t entry_index = bhash_find(&bgame_asset_registry, entry_name);
		bgame_asset_type_t* type = entry->value_addr;

		bgame_asset_type_t* canonical_type;
		if (bhash_is_valid(entry_index)) {
			canonical_type = bgame_asset_registry.values[entry_index];
			*canonical_type = *type;
			BLOG_DEBUG("Updated asset type: %s", type->name);
		} else {
			canonical_type = bgame_malloc(sizeof(bgame_asset_type_t), bgame_asset);
			*canonical_type = *type;
			bgame_str_t type_name = bgame_asset_strcpy(entry->name);
			bhash_put(&bgame_asset_registry, type_name, canonical_type);
			BLOG_DEBUG("Registered asset type: %s", type->name);
		}

		bhash_put(&bgame_asset_type_translation, type, canonical_type);
	}

	bgame_asset_initialized = true;
	++bgame_asset_code_version;
#endif
}

#if BGAME_RELOADABLE

static void
bgame_asset_on_file_changed(const char* file, void* userdata) {
	bgame_asset_t* asset = userdata;
	++asset->source_version;
}

#endif

void
bgame_asset_begin_load(bgame_asset_bundle_t** bundle_ptr) {
	bgame_asset_init();

	bgame_asset_bundle_t* bundle = *bundle_ptr;
	if (bundle == NULL) {
		bundle = bgame_malloc(sizeof(bgame_asset_bundle_t), bgame_asset);
		*bundle = (bgame_asset_bundle_t){ 0 };
#if BGAME_RELOADABLE
		bundle->monitor = bresmon_create(bgame_asset);
		bundle->code_version = bgame_asset_code_version;
#endif
		*bundle_ptr = bundle;
	}

	bhash_config_t config = bhash_config_default();
	config.memctx = bgame_asset;
	config.hash = bgame_asset_key_hash;
	config.eq = bgame_asset_key_eq;
	bhash_reinit(&bundle->assets, config);

	bhash_index_t num_assets = bhash_len(&bundle->assets);
	for (bhash_index_t i = 0; i < num_assets; ++i) {
		bgame_asset_t* asset = bundle->assets.values[i];

		// If an asset is loaded within bgame_asset_begin_load and
		// bgame_asset_end_load, it is eligible for purging when not mentioned
		// again
		// TODO: this may not be correct without tracking asset dependency
		if (!asset->dynamic) {
			asset->ref_count = 0;
		}
	}

	bundle->loading = true;
}

static inline bgame_asset_type_t*
bgame_asset_translate_type(bgame_asset_type_t* type) {
#if BGAME_RELOADABLE
	bhash_index_t index = bhash_find(&bgame_asset_type_translation, type);
	if (!bhash_is_valid(index)) {
		return NULL;
	}

	return bgame_asset_type_translation.values[index];
#else
	return type;
#endif
}

static inline void
bgame_asset_destroy(bgame_asset_t* asset) {
#if BGAME_RELOADABLE
	bresmon_unwatch(asset->watch);
#endif

	bgame_asset_strfree(asset->key.path);
	bgame_free(asset, bgame_asset);
}

bool
bgame_asset_source_changed(bgame_asset_bundle_t* bundle, void* asset_data) {
	bgame_asset_t* asset = (void*)((char*)asset_data - offsetof(bgame_asset_t, data));
	return asset->loaded_version != asset->source_version;
}

static void*
bgame_asset_load_impl(
	bgame_asset_bundle_t* bundle,
	bgame_asset_type_t* type,
	const char* path,
	const void* args
) {
	bgame_asset_key_t asset_key = {
		.type = type,
		.path = bgame_asset_strref(path),
	};
	bhash_index_t asset_index = bhash_find(&bundle->assets, asset_key);
	bool is_new_asset = !bhash_is_valid(asset_index);
	bgame_asset_t* asset;
	if (is_new_asset) {
		size_t asset_size = sizeof(bgame_asset_t) + type->size;
		asset = bgame_malloc(asset_size, bgame_asset);
		memset(asset, 0, asset_size);
		asset->key = (bgame_asset_key_t){
			.type = type,
			.path = bgame_asset_strcpy(path),
		};
		asset->source_version = 1;
		asset->dynamic = !bundle->loading;
#if BGAME_RELOADABLE
		const char* actual_path = cf_fs_get_actual_path(path);
		if (actual_path != NULL) {
			// Asset path is usually given as /assets/<type>/<filename>.<ext>
			// So we skip the leading slash and find the first slash
			// This is not guaranteed to work if the mount structure is fancier
			const char* basename = strchr(path + 1, '/');
			if (basename == NULL) {
				basename = path;
			}
			size_t actual_path_len = strlen(actual_path);
			size_t basename_len = strlen(basename);
			size_t watch_name_len = actual_path_len + basename_len;
			char* watch_name = bgame_alloc_for_frame(watch_name_len + 1, _Alignof(char));
			memcpy(watch_name, actual_path, actual_path_len);
			memcpy(watch_name + actual_path_len, basename, basename_len);
			watch_name[watch_name_len] = '\0';

			asset->watch = bresmon_watch(bundle->monitor, watch_name, bgame_asset_on_file_changed, asset);
			if (asset->watch) {
				BLOG_DEBUG("Watching %s", watch_name);
			} else {
				BLOG_WARN("Could not watch %s", watch_name);
			}
		}
#endif
		BLOG_DEBUG("Created new %s for %s: %p", type->name, path, (void*)asset);
	} else {
		asset = bundle->assets.values[asset_index];
	}

	bgame_asset_load_result_t result = type->load(bundle, asset->data, path, args);
	switch (result) {
		case BGAME_ASSET_LOADED:
			BLOG_INFO("Loaded %s: %s (%p)", type->name, path, (void*)asset->data);
			if (is_new_asset) {
				bhash_put(&bundle->assets, asset->key, asset);
			}
			break;
		case BGAME_ASSET_UNCHANGED:
			BLOG_INFO("Reused cache for %s: %s (%p)", type->name, path, (void*)asset->data);
			if (is_new_asset) {
				bhash_put(&bundle->assets, asset->key, asset);
				BLOG_WARN("New asset is unchanged");
			}
			break;
		case BGAME_ASSET_ERROR:
			BLOG_ERROR("Could not load %s: %s", type->name, path);
			if (is_new_asset) {
				bgame_asset_destroy(asset);
			}
			asset = NULL;
			break;
	}

	if (asset != NULL) {
		asset->ref_count += 1;

		// Delay version increment so other dependending assets can use
		// bgame_asset_source_changed to check.
		if (bundle->loading) {
			asset->loaded_version = asset->source_version;
		}
	}

	return asset != NULL ? asset->data : NULL;
}

void*
bgame_asset_load(
	bgame_asset_bundle_t* bundle,
	bgame_asset_type_t* type,
	const char* path,
	const void* args
) {
	bgame_asset_init();

	const char* type_name = type->name;
	type = bgame_asset_translate_type(type);
	if (type == NULL) {
		BLOG_ERROR("Asset type %s is not declared with BGAME_ASSET_TYPE", type_name);
		return NULL;
	}

	return bgame_asset_load_impl(bundle, type, path, args);
}

void
bgame_asset_unload(bgame_asset_bundle_t* bundle, void* asset_data) {
	bgame_asset_init();

	bgame_asset_t* asset = (void*)((char*)asset_data - offsetof(bgame_asset_t, data));

	--asset->ref_count;
	if (!bundle->loading && asset->ref_count <= 0) {
		BLOG_INFO("Unloading %s: %s", asset->key.type->name, asset->key.path.chars);

		asset->key.type->unload(bundle, asset->data);
		bhash_remove(&bundle->assets, asset->key);
		bgame_asset_destroy(asset);
	}
}

void
bgame_asset_end_load(bgame_asset_bundle_t* bundle) {
	bundle->loading = false;

	for (bhash_index_t i = 0; i < bhash_len(&bundle->assets);) {
		bgame_asset_key_t asset_key = bundle->assets.keys[i];
		bgame_asset_t* asset = bundle->assets.values[i];

		if (asset->ref_count <= 0) {
			BLOG_INFO(
				"Purging %s: %s (%p)",
				asset_key.type->name,
				bundle->assets.keys[i].path.chars,
				(void*)asset->data
			);

			asset->key.type->unload(bundle, asset->data);
			bhash_remove(&bundle->assets, asset_key);
			bgame_asset_destroy(asset);
		} else {
#if BGAME_RELOADABLE
			asset->previous_version = asset->loaded_version = asset->source_version;
#endif
			++i;
		}
	}
}

void*
bgame_asset_malloc(bgame_asset_bundle_t* bundle, size_t size) {
	return bgame_malloc(size, bgame_asset);
}

void*
bgame_asset_realloc(bgame_asset_bundle_t* bundle, void* ptr, size_t size) {
	return bgame_realloc(ptr, size, bgame_asset);
}

void
bgame_asset_free(bgame_asset_bundle_t* bundle, void* ptr) {
	bgame_free(ptr, bgame_asset);
}

void
bgame_asset_begin_check(bgame_asset_bundle_t* bundle) {
#if BGAME_RELOADABLE
	bgame_asset_init();

	if (bundle->code_version != bgame_asset_code_version) {
		bhash_index_t num_assets = bhash_len(&bundle->assets);
		for (bhash_index_t i = 0; i < num_assets; ++i) {
			bgame_asset_t* asset = bundle->assets.values[i];
			bresmon_set_watch_callback(asset->watch, bgame_asset_on_file_changed, asset);
		}

		bundle->code_version = bgame_asset_code_version;
	}

	if (bresmon_check(bundle->monitor, false) > 0) {
		bgame_asset_begin_load(&bundle);

		// Do this for all assets since we are not tracking asset dependency
		bhash_index_t num_assets = bhash_len(&bundle->assets);
		for (bhash_index_t i = 0; i < num_assets; ++i) {
			bgame_asset_t* asset = bundle->assets.values[i];
			asset->previous_version = asset->loaded_version;
			bgame_asset_load_impl(bundle, asset->key.type, asset->key.path.chars, NULL);
			if (asset->ref_count == 0) {
				asset->ref_count = 1;  // Prevent purging due to failure
			}
		}
	}
#endif
}

void
bgame_asset_end_check(bgame_asset_bundle_t* bundle) {
#if BGAME_RELOADABLE
	bgame_asset_init();
	if (bundle->loading) {
		bgame_asset_end_load(bundle);
	}
#endif
}

void
bgame_asset_check_bundle(bgame_asset_bundle_t* bundle) {
#if BGAME_RELOADABLE
	bgame_asset_begin_check(bundle);
	bgame_asset_end_check(bundle);
#endif
}

bool
bgame_asset_check(bgame_asset_bundle_t* bundle, void* asset_data) {
#if BGAME_RELOADABLE
	if (asset_data == NULL) { return false; }

	bgame_asset_init();

	bgame_asset_t* asset = (void*)((char*)asset_data - offsetof(bgame_asset_t, data));
	return asset->previous_version != asset->loaded_version;
#else
	return false;
#endif
}

void
bgame_asset_retain(bgame_asset_bundle_t* bundle, void* asset_data) {
#if BGAME_RELOADABLE
	if (asset_data == NULL) { return; }

	bgame_asset_init();

	bgame_asset_t* asset = (void*)((char*)asset_data - offsetof(bgame_asset_t, data));
	BLOG_INFO("Retaining %s: %s", asset->key.type->name, asset->key.path.chars);
	asset->ref_count += 1;
#endif
}

void
bgame_asset_destroy_bundle(bgame_asset_bundle_t* bundle) {
	bhash_index_t num_assets = bhash_len(&bundle->assets);
	for (bhash_index_t i = 0; i < num_assets; ++i) {
		bgame_asset_t* asset = bundle->assets.values[i];
		asset->key.type->unload(bundle, asset->data);
		bgame_asset_destroy(asset);
	}

	bhash_cleanup(&bundle->assets);

#if BGAME_RELOADABLE
	bresmon_destroy(bundle->monitor);
#endif

	bgame_free(bundle, bgame_asset);
}
