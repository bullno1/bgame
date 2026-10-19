#include "sound/internal.h"
#include "internal.h"
#include <bgame/reloadable.h>
#include <bfmt_log.h>
#include <cute_time.h>
#include <cute_rnd.h>
#include <math.h>
#include <string.h>

AUTOLIST_DEFINE(bgame__sound_bus_list)
AUTOLIST_DEFINE(bgame__sound_list)

#define MAX_BUSES 8
#define MAX_DEFS 256
#define MAX_VOICES 64
#define NAME_MAX 64

// Registrations move on reload, so the records keep names and the registrations hold indices
typedef struct {
	char name[NAME_MAX];
	float volume;
} bgame_sound_bus_state_t;

typedef struct {
	char name[NAME_MAX];
	double last_play;
	bool played;
	bool warned;
	int last_clip;
} bgame_sound_def_state_t;

typedef struct {
	CF_Sound sound;
	int def;
	int bus;
	// Gain before the bus and the falloff, so either can be reapplied
	float gain;
	bool spatial;
	CF_V3 pos;
	float min_distance;
	float max_distance;
} bgame_sound_voice_t;

typedef struct {
	bgame_sound_bus_state_t buses[MAX_BUSES];
	int num_buses;
	bgame_sound_def_state_t defs[MAX_DEFS];
	int num_defs;
	bgame_sound_voice_t voices[MAX_VOICES];
	int num_voices;
	bgame_sound_listener_t listener;
	CF_Rnd rnd;
} bgame_sound_state_t;

BGAME_PRIVATE_VAR(bgame_sound, bgame_sound_state_t, sound_state)

static int
find_bus(const char* name) {
	for (int i = 0; i < sound_state.num_buses; ++i) {
		if (strcmp(sound_state.buses[i].name, name) == 0) { return i; }
	}
	return -1;
}

static int
find_def(const char* name) {
	for (int i = 0; i < sound_state.num_defs; ++i) {
		if (strcmp(sound_state.defs[i].name, name) == 0) { return i; }
	}
	return -1;
}

static void
sound_resolve(void) {
	bgame_sound_state_t* s = &sound_state;
	if (s->rnd.state[0] == 0 && s->rnd.state[1] == 0) { s->rnd = cf_rnd_seed(0x5eed); }

	BGAME_FOREACH_DEFINED_SOUND_BUS(bus) {
		bus->index = -1;
		if (strlen(bus->name) >= NAME_MAX) {
			BFMT_ERROR("Sound bus name too long: ", bus->name);
			continue;
		}
		int index = find_bus(bus->name);
		if (index < 0) {
			if (s->num_buses >= MAX_BUSES) {
				BFMT_ERROR("Too many sound buses, dropping ", bus->name);
				continue;
			}
			index = s->num_buses++;
			bgame_sound_bus_state_t* state = &s->buses[index];
			strcpy(state->name, bus->name);
			state->volume = 1.f;
		}
		bus->index = index;
	}

	BGAME_FOREACH_DEFINED_SOUND(def) {
		def->index = -1;
		if (strlen(def->name) >= NAME_MAX) {
			BFMT_ERROR("Sound name too long: ", def->name);
			continue;
		}
		if (def->bus == NULL || def->bus->index < 0) {
			BFMT_ERROR("Sound ", def->name, " has no bus");
			continue;
		}
		if (def->clips == NULL || def->clips[0] == NULL) {
			BFMT_ERROR("Sound ", def->name, " has no clips");
			continue;
		}
		if (def->max_distance != 0.f && def->max_distance <= def->min_distance) {
			BFMT_WARN("Sound ", def->name, ": max_distance is not past min_distance, playing it unplaced");
		}
		int index = find_def(def->name);
		if (index < 0) {
			if (s->num_defs >= MAX_DEFS) {
				BFMT_ERROR("Too many sounds, dropping ", def->name);
				continue;
			}
			index = s->num_defs++;
			strcpy(s->defs[index].name, def->name);
		}
		def->index = index;
	}
}

BGAME_ON_LOAD(bgame_sound) {
	sound_resolve();
}

BGAME_AFTER_RELOAD(bgame_sound) {
	sound_resolve();
}

static void
prune_voices(void) {
	bgame_sound_state_t* s = &sound_state;
	for (int i = 0; i < s->num_voices;) {
		if (cf_sound_is_active(s->voices[i].sound)) {
			++i;
		} else {
			s->voices[i] = s->voices[--s->num_voices];
		}
	}
}

static int
count_voices(int row) {
	bgame_sound_state_t* s = &sound_state;
	int count = 0;
	for (int i = 0; i < s->num_voices; ++i) {
		if (s->voices[i].def == row) { ++count; }
	}
	return count;
}

static void
apply_voice(const bgame_sound_voice_t* voice) {
	bgame_sound_state_t* s = &sound_state;
	bgame_sound_mix_t mix = bgame_sound_mix(
		voice->min_distance, voice->max_distance, s->listener, voice->pos
	);
	float volume = voice->gain * mix.volume * s->buses[voice->bus].volume;
	cf_sound_set_volume(voice->sound, fminf(volume, 1.f));
	cf_sound_set_pan(voice->sound, mix.pan);
}

static float
jitter(float amount) {
	if (amount <= 0.f) { return 0.f; }
	return cf_rnd_range_float(&sound_state.rnd, -amount, amount);
}

CF_Sound
bgame_sound_play(const bgame_sound_t* def, bgame_sound_play_args_t play) {
	CF_Sound none = { 0 };
	bgame_sound_state_t* s = &sound_state;
	// Unresolved: refused and logged at load
	int row = def->index;
	if (row < 0 || row >= s->num_defs) { return none; }
	bgame_sound_def_state_t* state = &s->defs[row];
	double now = CF_SECONDS;

	prune_voices();
	if (def->cooldown > 0.f && state->played && now - state->last_play < def->cooldown) {
		return none;
	}
	if (def->max_voices > 0 && count_voices(row) >= def->max_voices) {
		return none;
	}

	int num_clips = 0;
	while (def->clips[num_clips] != NULL) { ++num_clips; }
	// Uniform among every clip but the last one played
	int clip = 0;
	if (num_clips > 1) {
		clip = cf_rnd_range_int(&s->rnd, 0, num_clips - 2);
		if (clip >= state->last_clip) { ++clip; }
	}
	CF_Audio* audio = *def->clips[clip];
	if (audio == NULL || audio->id == 0) {
		if (!state->warned) {
			BFMT_ERROR("Sound ", def->name, ": clip ", clip, " is not loaded");
			state->warned = true;
		}
		return none;
	}

	bool spatial = def->max_distance > def->min_distance;
	bgame_sound_mix_t mix = bgame_sound_mix(
		def->min_distance, def->max_distance, s->listener, play.pos
	);
	if (spatial && mix.volume <= 0.f) { return none; }

	float gain = bgame_sound_db_to_gain(def->gain_db + play.gain_db + jitter(def->gain_jitter_db));
	CF_SoundParams params = cf_sound_params_defaults();
	params.volume = fminf(gain * mix.volume * s->buses[def->bus->index].volume, 1.f);
	params.pan = mix.pan;
	params.pitch = bgame_sound_semitones_to_pitch(play.pitch + jitter(def->pitch_jitter));

	CF_Sound sound = cf_play_sound(*audio, params);
	if (sound.id == 0) { return none; }

	state->last_play = now;
	state->played = true;
	state->last_clip = clip;

	// Past the cap the voice plays untracked: it is only lost to the counts and the glides
	if (s->num_voices < MAX_VOICES) {
		s->voices[s->num_voices++] = (bgame_sound_voice_t){
			.sound = sound,
			.def = row,
			.bus = def->bus->index,
			.gain = gain,
			.spatial = spatial,
			.pos = play.pos,
			.min_distance = def->min_distance,
			.max_distance = def->max_distance,
		};
	}
	return sound;
}

void
bgame_sound_set_listener(CF_V3 pos, CF_Quat rotation) {
	sound_state.listener = (bgame_sound_listener_t){ .pos = pos, .rotation = rotation };
}

bgame_sound_listener_t
bgame_sound_listener(void) {
	return sound_state.listener;
}

float
bgame_sound_bus_volume(const bgame_sound_bus_t* bus) {
	if (bus->index < 0 || bus->index >= sound_state.num_buses) { return 0.f; }
	return sound_state.buses[bus->index].volume;
}

void
bgame_sound_set_bus_volume(const bgame_sound_bus_t* bus, float volume) {
	bgame_sound_state_t* s = &sound_state;
	if (bus->index < 0 || bus->index >= s->num_buses) { return; }
	s->buses[bus->index].volume = fminf(fmaxf(volume, 0.f), 1.f);

	prune_voices();
	for (int i = 0; i < s->num_voices; ++i) {
		if (s->voices[i].bus == bus->index) { apply_voice(&s->voices[i]); }
	}
}

int
bgame_sound_num_voices(const bgame_sound_t* def) {
	if (def == NULL) { return sound_state.num_voices; }
	return def->index < 0 ? 0 : count_voices(def->index);
}

void
bgame_sound_update(void) {
	bgame_sound_state_t* s = &sound_state;
	prune_voices();
	for (int i = 0; i < s->num_voices; ++i) {
		if (s->voices[i].spatial) { apply_voice(&s->voices[i]); }
	}
}
