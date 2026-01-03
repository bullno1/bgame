#ifndef BGAME_ASSET_FONT_H
#define BGAME_ASSET_FONT_H

#include <bgame/asset.h>

typedef struct {
	const char* name;
} bgame_font_t;

const char*
bgame_load_font(bgame_asset_bundle_t* bundle, const char* path);

extern bgame_asset_type_t font;

#define BGAME_DEFINE_FONT(NAME) BGAME_DEFINE_ASSET(NAME, font, bgame_font_t*)

#endif
