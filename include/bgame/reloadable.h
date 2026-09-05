#ifndef BGAME_RELOADABLE_H
#define BGAME_RELOADABLE_H

#include <stdbool.h>
#include "handle.h"

#ifndef BGAME_RELOADABLE
#	define BGAME_RELOADABLE 0
#endif

#define BGAME_PRIVATE_VAR(NAMESPACE, TYPE, VAR) \
	static TYPE VAR = { 0 }; \
	BGAME_PERSIST_VAR_EX(NAMESPACE, VAR)

#if BGAME_RELOADABLE
#	include <remodule.h>
#	define BGAME_VAR(TYPE, VAR) REMODULE_VAR(TYPE, VAR)
#	define BGAME_PERSIST_VAR(NAME) REMODULE_PERSIST_VAR(NAME)
#	define BGAME_PERSIST_VAR_EX(NAMESPACE, VAR) REMODULE_PERSIST_VAR_EX(VAR, NAMESPACE)
#	define BGAME_FN_WRAPPER(FN) BSFN(FN)
#else
#	define BGAME_VAR(TYPE, VAR) TYPE VAR
#	define BGAME_PERSIST_VAR(NAME)
#	define BGAME_PERSIST_VAR_EX(NAMESPACE, VAR)
#	define BSFN_NO_RELOAD
#	define BGAME_FN_WRAPPER(FN) FN
#endif

#include <bsfn.h>

typedef struct { bgame_handle_t internal; } bgame_reload_block_t;

bgame_reload_block_t
bgame_block_reload_at(const char* file, int line);

void
bgame_unblock_reload(bgame_reload_block_t block);

bool
bgame_is_reload_enabled(void);

#define bgame_block_reload() bgame_block_reload_at(__FILE__, __LINE__)

#endif
