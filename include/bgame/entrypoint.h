#ifndef BGAME_ENTRYPOINT_H
#define BGAME_ENTRYPOINT_H

#define REMODULE_PLUGIN_IMPLEMENTATION
#include "reloadable.h"
#include "app.h"

extern const char* bgame_entry_file;

#define BGAME_APP \
	extern bgame_app_t bgame_app; \
	BGAME_ENTRYPOINT(bgame_app) \
	bgame_app_t bgame_app =

#if BGAME_RELOADABLE

#define BGAME_ENTRYPOINT(APP) \
	void remodule_entry(remodule_op_t op, void* userdata) { \
		bgame_entry_file = __FILE__; \
		bgame_remodule(APP, op, userdata); \
	}

void
bgame_remodule(bgame_app_t app, remodule_op_t op, void* userdata);

#else

#include <SDL3/SDL_init.h>

#define BGAME_ENTRYPOINT(APP) \
	SDL_AppResult SDLCALL SDL_AppInit(void** appstate, int argc, char* argv[]) { \
		bgame_entry_file = __FILE__; \
		*appstate = &APP; \
		return bgame_static(&APP, argc, (const char**)argv); \
	}

SDL_AppResult
bgame_static(bgame_app_t* app, int argc, const char** argv);

#endif

#endif
