#ifndef BGAME_FRAME_ALLOCATOR_H
#define BGAME_FRAME_ALLOCATOR_H

#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <bmacro.h>
#include <stdio.h>

struct bgame_allocator_s;

extern struct bgame_allocator_s* bgame_frame_allocator;

void*
bgame_alloc_for_frame(size_t size, size_t alignment);

void*
bgame_begin_frame_alloc(void);

void
bgame_end_frame_alloc(void* marker);

static inline void*
bgame_make_frame_copy(void* data, size_t size, size_t alignment) {
	void* copy = bgame_alloc_for_frame(size, alignment);
	memcpy(copy, data, size);
	return copy;
}

static inline const char*
bgame_vfmt(const char* fmt, va_list args) {
	char fmt_buf[512];
	va_list args_copy;
	va_copy(args_copy, args);
	int len = vsnprintf(fmt_buf, sizeof(fmt_buf), fmt, args_copy);

	char* result = bgame_alloc_for_frame(len + 1, _Alignof(char));
	if (len >= (int)sizeof(fmt_buf)) {
		vsnprintf(result, len + 1, fmt, args);
	} else {
		memcpy(result, fmt_buf, len + 1);
	}

	va_end(args_copy);

	return result;
}

BFORMAT_ATTRIBUTE(1, 2)
static inline const char*
bgame_fmt(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	const char* result = bgame_vfmt(fmt, args);
	va_end(args);
	return result;
}

#define bgame_make_frame_copy(X) \
	bgame_make_frame_copy(&(X), sizeof((X)), _Alignof(BTYPEOF((X))))

#endif
