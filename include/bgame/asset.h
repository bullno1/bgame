#ifndef BGAME_ASSET_H
#define BGAME_ASSET_H

#include <bgame/reloadable.h>
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

typedef enum {
	BGAME_ASSET_LOADED,
	BGAME_ASSET_UNCHANGED,
	BGAME_ASSET_ERROR,
} bgame_asset_load_result_t;

typedef bgame_asset_load_result_t (*bgame_asset_load_fn_t)(
	bgame_asset_bundle_t* bundle,
	void* asset,
	const char* path,
	const void* args
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
bgame_asset_begin_load(bgame_asset_bundle_t** bundle_ptr);

bool
bgame_asset_source_changed(bgame_asset_bundle_t* bundle, void* asset);

void*
bgame_asset_load(
	bgame_asset_bundle_t* bundle,
	bgame_asset_type_t* type,
	const char* path,
	const void* args
);

void
bgame_asset_retain(
	bgame_asset_bundle_t* bundle,
	void* asset
);

void
bgame_asset_unload(bgame_asset_bundle_t* bundle, void* asset);

void
bgame_asset_end_load(bgame_asset_bundle_t* bundle);

void*
bgame_asset_malloc(bgame_asset_bundle_t* bundle, size_t size);

void*
bgame_asset_realloc(bgame_asset_bundle_t* bundle, void* ptr, size_t size);

void
bgame_asset_free(bgame_asset_bundle_t* bundle, void* ptr);

void
bgame_asset_check_bundle(bgame_asset_bundle_t* bundle);

void
bgame_asset_begin_check(bgame_asset_bundle_t* bundle);

bool
bgame_asset_check(bgame_asset_bundle_t* bundle, void* asset);

void
bgame_asset_end_check(bgame_asset_bundle_t* bundle);

void
bgame_asset_destroy_bundle(bgame_asset_bundle_t* bundle);

static inline bool
bgame_file_has_extension(const char* filename, const char* extension) {
    const char* dot = strrchr(filename, '.');
    if (dot == NULL || dot == filename) {
        return false;
    }

    return strcmp(dot + 1, extension) == 0;
}

#endif
