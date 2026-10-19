#ifndef BGAME_ASSET_AUDIO_H
#define BGAME_ASSET_AUDIO_H

#include <bgame/asset.h>

struct CF_Audio;

struct CF_Audio*
bgame_load_audio(bgame_asset_bundle_t* bundle, const char* path);

extern bgame_asset_type_t audio;

#define BGAME_AUDIO(NAME, ...) BGAME_ASSET(NAME, audio, struct CF_Audio*, __VA_ARGS__)

#endif
