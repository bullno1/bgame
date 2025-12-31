#include "internal.h"
#include <bgame/dj.h>
#include <cute_rnd.h>
#include <cute_audio.h>
#include <cute_string.h>
#include <bgame/reloadable.h>
#include <string.h>
#include <blog.h>
#include <time.h>

static struct {
	const char** tracks;
	int num_tracks;

	const char* current_track;
	CF_Audio current_audio;

	CF_Rnd rng;
	bool initialized;

	float fade_out_time;
	float fade_in_time;
} bgame_dj_ctx = { 0 };

BGAME_PERSIST_VAR(bgame_dj_ctx)

static void
bgame_dj_on_music_finished(void* userdata) {
	(void)userdata;

	if (bgame_dj_ctx.current_audio.id != 0) {
		BLOG_DEBUG("Picking next track");
		bgame_dj_next_track();
	}
}

BGAME_BEFORE_RELOAD(bgame_dj) {
	if (bgame_dj_ctx.initialized) {
		// Save the track name into the string pool so they survive reload
		// This will waste some memory but:
		//
		// * We typically do not have that many track names
		// * This only happens during development
		// * The string is only interned once

		if (bgame_dj_ctx.current_track != NULL) {
			bgame_dj_ctx.current_track = cf_sintern(bgame_dj_ctx.current_track);
		}
	}
}

BGAME_AFTER_RELOAD(bgame_dj) {
	if (bgame_dj_ctx.initialized) {
		cf_music_set_on_finish_callback(bgame_dj_on_music_finished, NULL, true);
	}
}

static void
bgame_dj_init(void) {
	if (!bgame_dj_ctx.initialized) {
		bgame_dj_ctx.rng = cf_rnd_seed(time(NULL));
		bgame_dj_ctx.initialized = true;
		cf_music_set_on_finish_callback(bgame_dj_on_music_finished, NULL, true);

		bgame_dj_ctx.fade_in_time = 1.5f;
		bgame_dj_ctx.fade_out_time = 1.5f;
	}
}

void
bgame_dj_set_playlist(const char** tracks, int num_tracks) {
	bgame_dj_init();

	cf_music_set_loop(false);
	bgame_dj_ctx.tracks = tracks;
	bgame_dj_ctx.num_tracks = num_tracks;

	bool track_is_in_list = false;
	if (bgame_dj_ctx.current_track != NULL) {
		for (int i = 0; i < num_tracks; ++i) {
			if (strcmp(tracks[i], bgame_dj_ctx.current_track) == 0) {
				track_is_in_list = true;
				break;
			}
		}
	}

	if (!track_is_in_list) {
		bgame_dj_next_track();
	}
}

const char*
bgame_dj_next_track(void) {
	bgame_dj_init();

	const char* next_track = NULL;
	if (bgame_dj_ctx.num_tracks > 0) {
		if (bgame_dj_ctx.num_tracks == 1) {
			next_track = bgame_dj_ctx.tracks[0];
		} else {
			// Try to find a different track
			do {
				int track_index = cf_rnd_range_int(
					&bgame_dj_ctx.rng,
					0,
					bgame_dj_ctx.num_tracks - 1
				);
				next_track = bgame_dj_ctx.tracks[track_index];
			} while (
				bgame_dj_ctx.current_track != NULL
				&&
				strcmp(next_track, bgame_dj_ctx.current_track) == 0
			);
		}
	}

	CF_Audio next_audio = { 0 };
	if (next_track != NULL) {
		if (
			bgame_dj_ctx.current_track != NULL
			&&
			strcmp(next_track, bgame_dj_ctx.current_track) == 0
		) {
			next_audio = bgame_dj_ctx.current_audio;
		} else {
			if (bgame_dj_ctx.current_audio.id != 0) {
				cf_audio_destroy(bgame_dj_ctx.current_audio);
			}

			next_audio = cf_audio_load_ogg(next_track);
		}
	}

	if (next_audio.id != 0) {
		BLOG_INFO("Playing: %s", next_track);
		cf_music_switch_to(
			next_audio,
			bgame_dj_ctx.fade_out_time,
			bgame_dj_ctx.fade_in_time
		);
	} else {
		cf_music_stop(bgame_dj_ctx.fade_out_time);
	}

	bgame_dj_ctx.current_track = next_track;
	bgame_dj_ctx.current_audio = next_audio;

	return next_track;
}

const char*
bgame_dj_current_track(void) {
	bgame_dj_init();

	return bgame_dj_ctx.current_track;
}

void
bgame_dj_set_crossfade_time(float fade_out_time_s, float fade_in_time_s) {
	bgame_dj_ctx.fade_in_time = fade_in_time_s;
	bgame_dj_ctx.fade_out_time = fade_out_time_s;
}
