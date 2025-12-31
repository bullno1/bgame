#ifndef BGAME_BINARY_H
#define BGAME_BINARY_H

#include <stdint.h>
#include <stddef.h>

struct bgame_asset_bundle_s;

typedef struct bgame_binary_s {
	size_t size;
	size_t capacity;
	uint8_t* data;
} bgame_binary_t;

bgame_binary_t*
bgame_load_binary(struct bgame_asset_bundle_s* bundle, const char* path);

#endif
