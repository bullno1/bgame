#ifndef BGAME_ASSET_SPRITE_H
#define BGAME_ASSET_SPRITE_H

struct bgame_asset_bundle_s;
struct CF_Sprite;

struct CF_Sprite*
bgame_load_sprite(struct bgame_asset_bundle_s* bundle, const char* path);

#endif
