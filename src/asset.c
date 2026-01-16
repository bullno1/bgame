#include <bgame/asset.h>
#include <bgame/str.h>
#include <bgame/reloadable.h>
#include <bgame/allocator.h>
#include <bgame/allocator/frame.h>
#include <blog.h>
#include <bmacro.h>
#include <bhash.h>
#include <cute_file_system.h>
#include <cute_string.h>

#if BGAME_RELOADABLE
#include <bresmon.h>
#endif

typedef struct {
	const char* type;
	bgame_str_t path;
} bgame_asset_key_t;

typedef struct {
	bgame_asset_key_t key;

	int source_version;
	int loaded_version;

#if BGAME_RELOADABLE
	bresmon_watch_t* watch;
#endif

	_Alignas(BGAME_MAX_ALIGN_TYPE) char data[];
} bgame_asset_t;

typedef BHASH_TABLE(bgame_asset_key_t, bgame_asset_t*) bgame_asset_cache_t;
typedef BHASH_TABLE(const char*, bgame_asset_type_t*) bgame_asset_registry_t;

struct bgame_asset_bundle_s {
	bgame_asset_cache_t assets;
	bgame_allocator_t* allocator;

#if BGAME_RELOADABLE
	int code_version;  // To replace watch callback
	bresmon_t* monitor;
#endif
};

AUTOLIST_DEFINE(bgame__asset_type_list)
AUTOLIST_DEFINE(bgame__asset_list)

static bool bgame_asset_initialized = false;
BGAME_VAR(bgame_asset_registry_t, bgame_asset_registry) = { 0 };
BGAME_VAR(int, bgame_asset_code_version) = 0;

static void
bgame_asset_sys_init(void) {
	if (bgame_asset_initialized) { return; }
	bgame_asset_initialized = true;
	++bgame_asset_code_version;

	bhash_config_t hconfig = bhash_config_default();
	hconfig.memctx = bgame_default_allocator;
	hconfig.removable = false;
	bhash_reinit(&bgame_asset_registry, hconfig);

	AUTOLIST_FOREACH(itr, bgame__asset_type_list) {
		bgame_asset_type_t* type = itr->value_addr;
		const char* type_name = sintern(type->name);
		bhash_put(&bgame_asset_registry, type_name, type);
		BLOG_DEBUG("Registered asset type: %s", type_name);
	}
}

static const bgame_asset_type_t*
bgame_asset_lookup_type(const char* name) {
	bgame_asset_sys_init();

	bhash_index_t index = bhash_find(&bgame_asset_registry, name);
	return bhash_is_valid(index) ? bgame_asset_registry.values[index] : NULL;
}

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
	return bhash__chibihash64(asset_key->path.chars, asset_key->path.len, (uint64_t)asset_key->type);
}

static bool
bgame_asset_key_eq(const void* lhs, const void* rhs, size_t size) {
	const bgame_asset_key_t* lhs_key = lhs;
	const bgame_asset_key_t* rhs_key = rhs;
	return lhs_key->type == rhs_key->type
		&& bgame_str_eq(&lhs_key->path, &rhs_key->path, 0);
}

static inline bgame_str_t
bgame_asset_strcpy(const char* str, bgame_allocator_t* allocator) {
	size_t len = strlen(str);
	char* chars = bgame_malloc(len + 1, allocator);
	memcpy(chars, str, len);
	chars[len] = '\0';
	return (bgame_str_t){
		.len = len,
		.chars = chars,
	};
}

static inline void
bgame_asset_strfree(bgame_str_t str, bgame_allocator_t* allocator) {
	bgame_free((char*)str.chars, allocator);
}

static inline bgame_str_t
bgame_asset_strref(const char* str) {
	size_t len = strlen(str);
	return (bgame_str_t){
		.len = len,
		.chars = str,
	};
}

#if BGAME_RELOADABLE

static void
bgame_asset_on_file_changed(const char* file, void* userdata) {
	bgame_asset_t* asset = userdata;
	++asset->source_version;
}

#endif

void
bgame_asset_init(bgame_asset_bundle_t** bundle_ptr, bgame_allocator_t* allocator) {
	bgame_asset_sys_init();

	bgame_asset_bundle_t* bundle = *bundle_ptr;
	if (bundle == NULL) {
		bundle = bgame_malloc(sizeof(bgame_asset_bundle_t), allocator);
		*bundle = (bgame_asset_bundle_t){ .allocator = allocator };
#if BGAME_RELOADABLE
		bundle->monitor = bresmon_create(allocator);
		bundle->code_version = bgame_asset_code_version;
#endif
		*bundle_ptr = bundle;
	}

	bhash_config_t config = bhash_config_default();
	config.memctx = allocator;
	config.hash = bgame_asset_key_hash;
	config.eq = bgame_asset_key_eq;
	bhash_reinit(&bundle->assets, config);
}

static inline void
bgame_asset_destroy(bgame_asset_bundle_t* bundle, bgame_asset_t* asset) {
#if BGAME_RELOADABLE
	bresmon_unwatch(asset->watch);
#endif

	bgame_asset_strfree(asset->key.path, bundle->allocator);
	bgame_free(asset, bundle->allocator);
}

void
bgame_asset_cleanup(bgame_asset_bundle_t** bundle_ptr) {
	bgame_asset_sys_init();

	bgame_asset_bundle_t* bundle = *bundle_ptr;
	if (bundle == NULL) { return; }

	bhash_index_t num_assets = bhash_len(&bundle->assets);
	for (bhash_index_t i = 0; i < num_assets; ++i) {
		bgame_asset_t* asset = bundle->assets.values[i];
		const bgame_asset_type_t* asset_type = bgame_asset_lookup_type(asset->key.type);
		asset_type->unload(bundle, asset->data);
		bgame_asset_destroy(bundle, asset);
	}

	bhash_cleanup(&bundle->assets);

#if BGAME_RELOADABLE
	bresmon_destroy(bundle->monitor);
#endif

	bgame_free(bundle, bundle->allocator);
	*bundle_ptr = NULL;
}

int
bgame_asset_version(void* asset_data) {
	bgame_asset_t* asset = BCONTAINER_OF(asset_data, bgame_asset_t, data);
	return asset->loaded_version;
}

void*
bgame_asset_load(
	bgame_asset_bundle_t* bundle,
	bgame_asset_type_t* type,
	const char* path
) {
	const char* type_name = sintern(type->name);
	bgame_asset_key_t asset_key = {
		.type = type_name,
		.path = bgame_asset_strref(path),
	};

#if !BGAME_RELOADABLE
	bhash_put(&bgame_asset_registry, type_name, type);
#endif

	bhash_index_t index = bhash_find(&bundle->assets, asset_key);
	if (bhash_is_valid(index)) {
		bgame_asset_t* asset = bundle->assets.values[index];
		return asset->data;
	}

	size_t asset_size = sizeof(bgame_asset_t) + type->size;
	bgame_asset_t* asset = bgame_malloc(asset_size, bundle->allocator);
	memset(asset, 0, asset_size);

	if (!type->load(bundle, asset->data, path)) {
		BLOG_ERROR("Could not load %s: %s", type->name, path);
		bgame_free(asset, bundle->allocator);
		return NULL;
	}

	BLOG_INFO("Loaded %s: %s (%p)", type->name, path, (void*)asset->data);

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

	bgame_asset_key_t key = {
		.type = type_name,
		.path = bgame_asset_strcpy(path, bundle->allocator),
	};
	bhash_put(&bundle->assets, key, asset);
	asset->key = key;

	return asset->data;
}

void*
bgame_asset_load_def(bgame_asset_bundle_t* bundle, bgame_asset_def_t* def) {
	void** var_ptr = (void**)def->var;
	BLOG_DEBUG("Loading predefined asset: %s", def->name);
	return *var_ptr = bgame_asset_load(bundle, def->type, def->meta->path);
}

void
bgame_asset_unload(bgame_asset_bundle_t* bundle, void* asset_data) {
	bgame_asset_t* asset = BCONTAINER_OF(asset_data, bgame_asset_t, data);

	BLOG_INFO("Unloading %s: %s", asset->key.type, asset->key.path.chars);

	const bgame_asset_type_t* type = bgame_asset_lookup_type(asset->key.type);
	type->unload(bundle, asset->data);
	bhash_remove(&bundle->assets, asset->key);
	bgame_asset_destroy(bundle, asset);
}

void*
bgame_asset_malloc(bgame_asset_bundle_t* bundle, size_t size) {
	return bgame_malloc(size, bundle->allocator);
}

void*
bgame_asset_realloc(bgame_asset_bundle_t* bundle, void* ptr, size_t size) {
	return bgame_realloc(ptr, size, bundle->allocator);
}

void
bgame_asset_free(bgame_asset_bundle_t* bundle, void* ptr) {
	bgame_free(ptr, bundle->allocator);
}

void
bgame_asset_check_bundle(bgame_asset_bundle_t* bundle) {
#if BGAME_RELOADABLE
	bgame_asset_sys_init();

	if (bundle->code_version != bgame_asset_code_version) {
		bhash_index_t num_assets = bhash_len(&bundle->assets);
		for (bhash_index_t i = 0; i < num_assets; ++i) {
			bgame_asset_t* asset = bundle->assets.values[i];
			bresmon_set_watch_callback(asset->watch, bgame_asset_on_file_changed, asset);
		}
		bundle->code_version = bgame_asset_code_version;
	}

	if (bresmon_check(bundle->monitor, false) > 0) {
		bhash_index_t num_assets = bhash_len(&bundle->assets);
		for (bhash_index_t i = 0; i < num_assets; ++i) {
			bgame_asset_t* asset = bundle->assets.values[i];

			if (asset->loaded_version != asset->source_version) {
				const bgame_asset_type_t* type = bgame_asset_lookup_type(asset->key.type);
				if (type->load(bundle, asset->data, asset->key.path.chars)) {
					BLOG_INFO("Reloaded %s: %s", asset->key.type, asset->key.path.chars);
				} else {
					BLOG_ERROR("Could not reload %s: %s", type->name, asset->key.path.chars);
				}
				asset->loaded_version = asset->source_version;
			}
		}
	}
#endif
}
