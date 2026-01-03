#ifndef BGAME_ASSET_IMAGE_H
#define BGAME_ASSET_IMAGE_H

#include <bgame/asset.h>

struct bgame_asset_bundle_s;

struct CF_Image*
bgame_load_image(bgame_asset_bundle_t* bundle, const char* path);

extern bgame_asset_type_t image;

#define BGAME_DEFINE_IMAGE(NAME) BGAME_DEFINE_ASSET(NAME, image, struct CF_Image*)

#endif
