#ifndef BGAME_DJ_H
#define BGAME_DJ_H

void
bgame_dj_set_playlist(const char** tracks, int num_tracks);

const char*
bgame_dj_next_track(void);

const char*
bgame_dj_current_track(void);

void
bgame_dj_set_crossfade_time(float fade_out_time_s, float fade_in_time_s);

#endif
