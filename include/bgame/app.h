#ifndef BGAME_APP_H
#define BGAME_APP_H

#include <stdbool.h>

typedef enum {
	BGAME_APP_INITIALIZING,
	BGAME_APP_REINITIALIZING,
	BGAME_APP_RUNNING,
	BGAME_APP_CLEANING_UP,
} bgame_app_state_t;

typedef struct bgame_app_s {
	const char* company;
	const char* name;
	const char* title;

	void (*init)(int argc, const char** argv);
	void (*update)(void);
	void (*cleanup)(void);

	void (*check_reload)(void);
	void (*before_reload)(void);
	void (*after_reload)(void);
} bgame_app_t;

bgame_app_state_t
bgame_app_state(void);

const bgame_app_t*
bgame_app_info(void);

void
bgame_app_make(int width, int height, int options, const char* argv0);

bool
bgame_app_mount_assets(void);

const char*
bgame_app_mount_user_dir(void);

void
bgame_app_init_imgui(void);

void
bgame_app_destroy(void);

#endif
