#include <bgame/asset.h>
#include <bgame/asset/audio.h>
#include <cute_audio.h>
#include <blog.h>

static bool
bgame_audio_load(
	bgame_asset_bundle_t* bundle,
	void* asset,
	const char* path
) {
	CF_Audio* audio = asset;

	if (audio->id != 0) {
		cf_audio_destroy(*audio);
	}

	if (bgame_file_has_extension(path, "ogg")) {
		*audio = cf_audio_load_ogg(path);
		return audio->id != 0;
	} else if (bgame_file_has_extension(path, "wav")) {
		*audio = cf_audio_load_wav(path);
		return audio->id != 0;
	} else {
		BLOG_ERROR("Unsupported audio format");
		return false;
	}
}

static void
bgame_audio_unload(
	bgame_asset_bundle_t* bundle,
	void* asset
) {
	CF_Audio* audio = asset;
	if (audio->id != 0) {
		cf_audio_destroy(*audio);
		audio->id = 0;
	}
}

BGAME_ASSET_TYPE(audio) = {
	.name = "audio",
	.size = sizeof(CF_Audio),
	.load = bgame_audio_load,
	.unload = bgame_audio_unload,
};

struct CF_Audio*
bgame_load_audio(struct bgame_asset_bundle_s* bundle, const char* path) {
	return bgame_asset_load(bundle, &audio, path);
}
