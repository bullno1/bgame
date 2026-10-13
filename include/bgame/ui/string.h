#ifndef BGAME_UI_STRING_H
#define BGAME_UI_STRING_H

#include "../ui.h"
#include "../allocator/frame.h"

static inline Clay_String
bgame_ui_stringv(const char* fmt, va_list args) {
	Clay_String result = { 0 };
	result.chars = bgame_vfmt(&result.length, fmt, args);
	return result;
}

BFORMAT_ATTRIBUTE(1, 2)
static inline Clay_String
bgame_ui_string(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	Clay_String result = bgame_ui_stringv(fmt, args);
	va_end(args);
	return result;
}

/**
 * Translated text for the UI, in the current locale.
 *
 * Takes what @ref bfmt_translate takes after its locale and lives until the
 * end of the frame.
 */
#define bgame_ui_text(...) bgame_translate(bgame_ui_locale(), __VA_ARGS__)

/*! Same as @ref bgame_ui_text with a context, as @ref bfmt_ptranslate */
#define bgame_ui_ptext(CONTEXT, ...) bgame_ptranslate(bgame_ui_locale(), CONTEXT, __VA_ARGS__)

/*! A @ref bfmt_text_t for the UI, in the current locale, with its bindings as @ref bfmt_translate_text */
#define bgame_ui_text_of(TEXT, ...) \
	bgame_translate_text(bgame_ui_locale(), TEXT __VA_OPT__(,) __VA_ARGS__)

#endif
