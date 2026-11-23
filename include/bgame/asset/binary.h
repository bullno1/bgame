#ifndef BGAME_BINARY_H
#define BGAME_BINARY_H

#include <bgame/asset.h>
#include <stdint.h>
#include <stddef.h>

typedef struct bgame_binary_s {
	size_t size;
	size_t capacity;
	uint8_t* data;
} bgame_binary_t;

bgame_binary_t*
bgame_load_binary(bgame_asset_bundle_t* bundle, const char* path);

extern bgame_asset_type_t binary;

#define BGAME_BINARY(NAME, ...) BGAME_ASSET(NAME, binary, bgame_binary_t*, __VA_ARGS__)

#endif
