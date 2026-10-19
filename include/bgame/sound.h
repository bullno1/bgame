#ifndef BGAME_SOUND_H
#define BGAME_SOUND_H

#include <autolist.h>
#include <cute_math3d.h>
#include <cute_audio.h>
#include <stdbool.h>

/*! A null-terminated list of clips for a sound, each the address of a `BGAME_AUDIO` variable */
#define BGAME_AUDIO_LIST(...) (struct CF_Audio**[]){ __VA_ARGS__, 0 }

/*! A volume group. Every sound names one and the group's volume scales all of its sounds. */
typedef struct bgame_sound_bus_s {
	const char* name;
	/*! Assigned by bgame, never by the definition */
	int index;
} bgame_sound_bus_t;

/*! A sound: what to play when a name is posted and how */
typedef struct bgame_sound_s {
	const char* name;
	/*! Assigned by bgame, never by the definition */
	int index;

	const bgame_sound_bus_t* bus;
	/*! A `BGAME_AUDIO_LIST`, picked from at random per play, never the same twice in a row */
	struct CF_Audio*** clips;

	/*! In dB, 0 plays the clip as recorded */
	float gain_db;
	/*! Added per play, uniform within plus or minus this much */
	float gain_jitter_db;
	/*! In semitones, uniform within plus or minus this much */
	float pitch_jitter;

	/*! Seconds before the sound may play again. 0 refuses nothing. */
	float cooldown;
	/*! Instances playing at once; a play past the cap is dropped. 0 is no cap. */
	int max_voices;

	/*!
	 * In world units: full volume within min, silent past max.
	 * Both 0 is a sound not placed in space: no falloff, center pan.
	 */
	float min_distance;
	float max_distance;
} bgame_sound_t;

/*! The call's own part of a play: where the sound is and what it adds to the definition */
typedef struct {
	/*! Ignored by a sound not placed in space */
	CF_V3 pos;
	/*! In dB, added to the definition's */
	float gain_db;
	/*! In semitones, added to the jitter */
	float pitch;
} bgame_sound_play_args_t;

typedef struct {
	CF_V3 pos;
	/*! Where the listener faces, for the pan. Unset, it faces -z like the identity. */
	CF_Quat rotation;
} bgame_sound_listener_t;

// The name is the variable, as for an asset: the app chooses its prefixes
#define BGAME_DEFINE_SOUND_BUS(NAME) \
	extern bgame_sound_bus_t NAME; \
	AUTOLIST_ADD_ENTRY(bgame__sound_bus_list, NAME, NAME) \
	bgame_sound_bus_t NAME = { .name = #NAME, .index = -1 }

#define BGAME_DECLARE_SOUND_BUS(NAME) \
	extern bgame_sound_bus_t NAME

#define BGAME_DEFINE_SOUND(NAME, ...) \
	extern bgame_sound_t NAME; \
	AUTOLIST_ADD_ENTRY(bgame__sound_list, NAME, NAME) \
	bgame_sound_t NAME = { .name = #NAME, .index = -1, __VA_ARGS__ }

#define BGAME_DECLARE_SOUND(NAME, ...) \
	extern bgame_sound_t NAME; \
	static_assert(sizeof((bgame_sound_t){ __VA_ARGS__ }) > 0, "")

#ifndef BGAME_DEFINE_SOUNDS
#	define BGAME_SOUND_BUS BGAME_DECLARE_SOUND_BUS
#	define BGAME_SOUND BGAME_DECLARE_SOUND
#else
#	define BGAME_SOUND_BUS BGAME_DEFINE_SOUND_BUS
#	define BGAME_SOUND BGAME_DEFINE_SOUND
#endif

#define BGAME_FOREACH_DEFINED_SOUND_BUS(ITR) \
	AUTOLIST_FOREACH(bgame__sound_bus_itr, bgame__sound_bus_list) \
		for ( \
			bgame_sound_bus_t* ITR = bgame__sound_bus_itr->value_addr; \
			ITR != NULL; \
			ITR = NULL \
		)

#define BGAME_FOREACH_DEFINED_SOUND(ITR) \
	AUTOLIST_FOREACH(bgame__sound_itr, bgame__sound_list) \
		for ( \
			bgame_sound_t* ITR = bgame__sound_itr->value_addr; \
			ITR != NULL; \
			ITR = NULL \
		)

AUTOLIST_DECLARE(bgame__sound_bus_list)
AUTOLIST_DECLARE(bgame__sound_list)

/*!
 * Plays a sound, or refuses it: in its cooldown, at its voice cap, out of range, or with
 * a clip that is not loaded. The handle is for the rare caller that stops it.
 */
CF_Sound
bgame_sound_play(const bgame_sound_t* def, bgame_sound_play_args_t play);

/*!
 * Once per frame, by the app: forgets the voices that finished and moves the placed ones
 * that are still playing with the listener.
 */
void
bgame_sound_update(void);

/*! Where placed sounds are heard from and which way they face. Playing sounds follow it. */
void
bgame_sound_set_listener(CF_V3 pos, CF_Quat rotation);

bgame_sound_listener_t
bgame_sound_listener(void);

float
bgame_sound_bus_volume(const bgame_sound_bus_t* bus);

/*! In [0, 1]. Applies to the sounds already playing too. */
void
bgame_sound_set_bus_volume(const bgame_sound_bus_t* bus, float volume);

/*! How many instances of a sound are playing, of every sound for NULL */
int
bgame_sound_num_voices(const bgame_sound_t* def);

#endif
