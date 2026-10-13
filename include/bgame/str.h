#ifndef BGAME_STR_H
#define BGAME_STR_H

#include <string.h>

#define BGAME_LIT_STR(STR) { .len = sizeof("" STR) - 1, .chars = STR }

typedef struct {
	const char* chars;
	int len;
} bgame_str_t;

static inline bgame_str_t
bgame_str(const char* cstr) {
	return (bgame_str_t){ .chars = cstr, .len = (int)strlen(cstr) };
}

#endif
