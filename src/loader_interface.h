#ifndef BGAME_LOADER_INTERFACE_H
#define BGAME_LOADER_INTERFACE_H

#include <bgame/app.h>
#include <stdbool.h>
#include <bsfn.h>

typedef struct bgame_loader_interface_s {
	int argc;
	const char** argv;
	bool reload_blocked;

	bgame_app_t app;
	void (*update)(struct bgame_loader_interface_s* interface);
	bsfn_ctx_t* bsfn;
} bgame_loader_interface_t;

#endif
