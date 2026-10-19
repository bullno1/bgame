#ifndef BGAME_SOUND_INTERNAL_H
#define BGAME_SOUND_INTERNAL_H

#include <bgame/sound.h>

// How far a source at the listener's side is panned: 1 is hard left or right
#ifndef BGAME_SOUND_PAN_WIDTH
#	define BGAME_SOUND_PAN_WIDTH 0.7f
#endif

/*! How a placed sound reaches the listener: a falloff in [0, 1] and a pan in [0, 1] */
typedef struct {
	float volume;
	float pan;
} bgame_sound_mix_t;

/*!
 * Falloff and pan of a source against a listener, pure.
 * A max distance not past the min is a sound not placed in space.
 */
bgame_sound_mix_t
bgame_sound_mix(
	float min_distance,
	float max_distance,
	bgame_sound_listener_t listener,
	CF_V3 pos
);

float
bgame_sound_db_to_gain(float db);

float
bgame_sound_semitones_to_pitch(float semitones);

#endif
