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

#endif
