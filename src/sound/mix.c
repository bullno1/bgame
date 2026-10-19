#include "internal.h"
#include <math.h>

float
bgame_sound_db_to_gain(float db) {
	return powf(10.f, db / 20.f);
}

float
bgame_sound_semitones_to_pitch(float semitones) {
	return powf(2.f, semitones / 12.f);
}

bgame_sound_mix_t
bgame_sound_mix(
	float min_distance,
	float max_distance,
	bgame_sound_listener_t listener,
	CF_V3 pos
) {
	bgame_sound_mix_t mix = { .volume = 1.f, .pan = 0.5f };
	if (max_distance <= min_distance) { return mix; }

	CF_V3 d = cf_sub_v3(pos, listener.pos);
	float dist = cf_len_v3(d);

	float t = (dist - min_distance) / (max_distance - min_distance);
	if (t >= 1.f) {
		mix.volume = 0.f;
		return mix;
	}
	// Squared so it drops quickly near the source and tails off, closer to how distance sounds
	float falloff = 1.f - fmaxf(t, 0.f);
	mix.volume = falloff * falloff;

	// A stereo pan only needs how far to the side the source is: the listener's right axis
	CF_V3 right = cf_mul_q_v3(listener.rotation, cf_v3(1.f, 0.f, 0.f));
	float right_len = cf_len_v3(right);
	if (dist > 0.f && right_len > 0.f) {
		float side = cf_dot_v3(d, right) / (dist * right_len);
		mix.pan = 0.5f + 0.5f * BGAME_SOUND_PAN_WIDTH * side;
	}
	return mix;
}
