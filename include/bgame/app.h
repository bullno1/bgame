#ifndef BGAME_APP_H
#define BGAME_APP_H

typedef enum {
	BGAME_APP_INITIALIZING,
	BGAME_APP_REINITIALIZING,
	BGAME_APP_RUNNING,
	BGAME_APP_CLEANING_UP,
} bgame_app_state_t;

typedef struct bgame_app_s {
	void (*init)(int argc, const char** argv);
	void (*update)(void);
	void (*cleanup)(void);

	void (*check_reload)(void);
	void (*before_reload)(void);
	void (*after_reload)(void);
} bgame_app_t;

bgame_app_state_t
bgame_app_state(void);

#endif
