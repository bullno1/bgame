#ifndef BGAME_ASSET_H
#define BGAME_ASSET_H

#include <bgame/reloadable.h>
#include <bgame/allocator.h>
#include <autolist.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#if BGAME_RELOADABLE
#	define BGAME_ASSET_TYPE(NAME) \
	AUTOLIST_ENTRY(bgame_asset_type_list, bgame_asset_type_t, NAME)
#else
#	define BGAME_ASSET_TYPE(NAME) \
	static bgame_asset_type_t NAME
#endif

typedef struct bgame_asset_bundle_s bgame_asset_bundle_t;

typedef bool (*bgame_asset_load_fn_t)(
	bgame_asset_bundle_t* bundle,
	void* asset,
	const char* path
);
typedef void (*bgame_asset_unload_fn_t)(
	bgame_asset_bundle_t* bundle,
	void* asset
);

typedef struct bgame_asset_type_s {
	const char* name;
	size_t size;
	bgame_asset_load_fn_t load;
	bgame_asset_unload_fn_t unload;
} bgame_asset_type_t;

void
bgame_asset_init(bgame_asset_bundle_t** bundle_ptr, bgame_allocator_t* allocator);

void
bgame_asset_cleanup(bgame_asset_bundle_t** bundle_ptr);

void*
bgame_asset_load(
	bgame_asset_bundle_t* bundle,
	bgame_asset_type_t* type,
	const char* path
);

int
bgame_asset_version(bgame_asset_bundle_t* bundle, void* asset);

void
bgame_asset_unload(bgame_asset_bundle_t* bundle, void* asset);

void*
bgame_asset_malloc(bgame_asset_bundle_t* bundle, size_t size);

void*
bgame_asset_realloc(bgame_asset_bundle_t* bundle, void* ptr, size_t size);

void
bgame_asset_free(bgame_asset_bundle_t* bundle, void* ptr);

void
bgame_asset_check_bundle(bgame_asset_bundle_t* bundle);

static inline bool
bgame_file_has_extension(const char* filename, const char* extension) {
    const char* dot = strrchr(filename, '.');
    if (dot == NULL || dot == filename) {
        return false;
    }

    return strcmp(dot + 1, extension) == 0;
}

#endif
