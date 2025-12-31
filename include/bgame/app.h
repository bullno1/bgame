#ifndef BGAME_APP_H
#define BGAME_APP_H

typedef struct bgame_app_s {
	void (*init)(int argc, const char** argv);
	void (*update)(void);
	void (*cleanup)(void);
	void (*before_reload)(void);
	void (*after_reload)(void);
} bgame_app_t;

#endif
