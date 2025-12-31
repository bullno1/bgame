#ifndef BGAME_RELOADABLE_H
#define BGAME_RELOADABLE_H

#include <stdbool.h>

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
#else
#	define BGAME_VAR(TYPE, VAR) TYPE VAR
#	define BGAME_PERSIST_VAR(NAME)
#	define BGAME_PERSIST_VAR_EX(NAMESPACE, VAR)
#endif

void
bgame_block_reload(void);

void
bgame_unblock_reload(void);

bool
bgame_is_reload_enabled(void);

#endif
