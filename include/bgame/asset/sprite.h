#ifndef BGAME_ASSET_SPRITE_H
#define BGAME_ASSET_SPRITE_H

#include <bgame/asset.h>

struct CF_Sprite;

struct CF_Sprite*
bgame_load_sprite(bgame_asset_bundle_t* bundle, const char* path);

extern bgame_asset_type_t sprite;

#define BGAME_DEFINE_SPRITE(NAME) BGAME_DEFINE_ASSET(NAME, sprite, struct CF_Sprite*)

#endif
