#ifndef BGAME_FRAME_ALLOCATOR_H
#define BGAME_FRAME_ALLOCATOR_H

#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <bmacro.h>
#include <stdio.h>
#include <bfmt.h>
#include <bgame/str.h>

struct barena_s;
struct bgame_allocator_s;

extern struct bgame_allocator_s* bgame_frame_allocator;

void*
bgame_alloc_for_frame(size_t size, size_t alignment);

void*
bgame_begin_frame_alloc(void);

void
bgame_end_frame_alloc(void* marker);

/*! The arena of the current frame */
struct barena_s*
bgame_frame_arena(void);

/**
 * A stream collecting text into the frame arena.
 *
 * Pair with @ref bgame_end_str, which hands the text out as a @ref bgame_str_t.
 * A nested pair would drop all characters and log an error instead.
 */
bfmt_stream_t*
bgame_begin_str(void);

/*! The text collected since @ref bgame_begin_str, NUL-terminated, in the frame arena */
bgame_str_t
bgame_end_str(void);

/*! @ref bfmt_print into the frame arena, as a @ref bgame_str_t */
#define bgame_print(...) \
	(bfmt_print(bgame_begin_str(), __VA_ARGS__), bgame_end_str())

/*! @ref bfmt_translate into the frame arena, as a @ref bgame_str_t */
#define bgame_translate(LOCALE, ...) \
	(bfmt_translate(bgame_begin_str(), LOCALE, __VA_ARGS__), bgame_end_str())

/*! @ref bfmt_ptranslate into the frame arena, as a @ref bgame_str_t */
#define bgame_ptranslate(LOCALE, CONTEXT, ...) \
	(bfmt_ptranslate(bgame_begin_str(), LOCALE, CONTEXT, __VA_ARGS__), bgame_end_str())

/*! @ref bfmt_translate_text into the frame arena, as a @ref bgame_str_t */
#define bgame_translate_text(LOCALE, TEXT, ...) \
	(bfmt_translate_text(bgame_begin_str(), LOCALE, TEXT __VA_OPT__(,) __VA_ARGS__), bgame_end_str())

static inline void*
bgame_make_frame_copy(void* data, size_t size, size_t alignment) {
	void* copy = bgame_alloc_for_frame(size, alignment);
	memcpy(copy, data, size);
	return copy;
}

static inline const char*
bgame_vfmt(int* len_out, const char* fmt, va_list args) {
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

	if (len_out != NULL) { *len_out = len; }
	return result;
}

BFORMAT_ATTRIBUTE(1, 2)
static inline const char*
bgame_fmt(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	const char* result = bgame_vfmt(NULL, fmt, args);
	va_end(args);
	return result;
}

#define bgame_make_frame_copy(X) \
	bgame_make_frame_copy(&(X), sizeof((X)), _Alignof(BTYPEOF((X))))

#endif
