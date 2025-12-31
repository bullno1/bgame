#ifndef BGAME_ASSET_AUDIO_H
#define BGAME_ASSET_AUDIO_H

struct bgame_asset_bundle_s;
struct CF_Audio;

struct CF_Audio*
bgame_load_audio(struct bgame_asset_bundle_s* bundle, const char* path);

#endif
