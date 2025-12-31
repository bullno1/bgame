#ifndef BGAME_STR_H
#define BGAME_STR_H

#define BGAME_LIT_STR(STR) { .len = sizeof("" STR) - 1, .chars = STR }

typedef struct {
	const char* chars;
	int len;
} bgame_str_t;

#endif
