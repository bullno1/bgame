#include <bgame/asset.h>
#include <bgame/asset/translation.h>
#include <bgame/allocator.h>
#include <bgame/str.h>
#include <bgame/chibihash64-stream.h>
#include <bhash.h>
#include <bfmt_log.h>
#include <bfmt_cldr.h>
#include <cute_file_system.h>
#include <stdint.h>
#include <string.h>

typedef struct {
	bgame_str_t context;
	bgame_str_t id;
} bgame_translation_key_t;

typedef BHASH_TABLE(bgame_translation_key_t, const char*) bgame_translation_table_t;

struct bgame_translation_s {
	bgame_translation_table_t table;
	// Every string of the file, unescaped and NUL-terminated, in one block
	char* pool;
	size_t pool_len;
	const char* language;
};

static bhash_hash_t
bgame_translation_key_hash(const void* key, size_t size) {
	(void)size;
	const bgame_translation_key_t* translation_key = key;
	// Separated as gettext joins them, so the parts cannot shift into each other
	ChibiHash64Ctx ctx = chibihash64_init(0);
	chibihash64_append(&ctx, translation_key->context.chars, translation_key->context.len);
	chibihash64_append(&ctx, "\x04", 1);
	chibihash64_append(&ctx, translation_key->id.chars, translation_key->id.len);
	return chibihash64_finish(&ctx);
}

static bool
bgame_translation_key_eq(const void* lhs, const void* rhs, size_t size) {
	(void)size;
	const bgame_translation_key_t* lhs_key = lhs;
	const bgame_translation_key_t* rhs_key = rhs;
	return lhs_key->context.len == rhs_key->context.len
		&& lhs_key->id.len == rhs_key->id.len
		&& memcmp(lhs_key->context.chars, rhs_key->context.chars, (size_t)lhs_key->context.len) == 0
		&& memcmp(lhs_key->id.chars, rhs_key->id.chars, (size_t)lhs_key->id.len) == 0;
}

static bgame_translation_key_t
bgame_translation_make_key(const char* context, const char* key) {
	return (bgame_translation_key_t){
		.context = context != NULL ? bgame_str(context) : (bgame_str_t){ .chars = "", .len = 0 },
		.id = bgame_str(key),
	};
}

// Parser {{{

typedef enum {
	FIELD_NONE,
	FIELD_CONTEXT,
	FIELD_ID,
	FIELD_STR,
	FIELD_SKIPPED,  // msgid_plural, msgstr[n] and anything unknown
} bgame_po_field_t;

typedef struct {
	bgame_translation_t* translation;
	const char* path;
	int line;

	// The entry being read, as offsets into the pool
	bgame_po_field_t field;
	size_t field_start;
	bool has_context;
	bool has_id;
	bool has_str;
	bool fuzzy;
	bool plural;
	size_t context;
	size_t id;
	size_t str;
	int num_entries;
} bgame_po_parser_t;

static void
bgame_po_append(bgame_po_parser_t* parser, char c) {
	parser->translation->pool[parser->translation->pool_len++] = c;
}

// The string between the quotes of a line, unescaped, into the pool
static bool
bgame_po_read_string(bgame_po_parser_t* parser, const char* line, const char* end) {
	const char* open = memchr(line, '"', (size_t)(end - line));
	if (open == NULL) {
		BFMT_WARN(parser->path, ":", parser->line, ": expected a quoted string");
		return false;
	}
	const char* close = end;
	while (close > open && *(close - 1) != '"') { --close; }
	--close;
	if (close <= open) {
		BFMT_WARN(parser->path, ":", parser->line, ": unterminated string");
		return false;
	}

	for (const char* itr = open + 1; itr < close; ++itr) {
		if (*itr != '\\') {
			bgame_po_append(parser, *itr);
			continue;
		}
		if (++itr >= close) { break; }
		switch (*itr) {
			case 'n': bgame_po_append(parser, '\n'); break;
			case 't': bgame_po_append(parser, '\t'); break;
			case 'r': bgame_po_append(parser, '\r'); break;
			case 'a': bgame_po_append(parser, '\a'); break;
			case 'b': bgame_po_append(parser, '\b'); break;
			case 'f': bgame_po_append(parser, '\f'); break;
			case 'v': bgame_po_append(parser, '\v'); break;
			case 'x': {
				unsigned value = 0;
				int digits = 0;
				while (digits < 2 && itr + 1 < close) {
					char d = *(itr + 1);
					unsigned v;
					if (d >= '0' && d <= '9') { v = (unsigned)(d - '0'); }
					else if (d >= 'a' && d <= 'f') { v = (unsigned)(d - 'a' + 10); }
					else if (d >= 'A' && d <= 'F') { v = (unsigned)(d - 'A' + 10); }
					else { break; }
					value = value * 16 + v;
					++itr;
					++digits;
				}
				bgame_po_append(parser, (char)value);
			} break;
			default:
				if (*itr >= '0' && *itr <= '7') {
					unsigned value = (unsigned)(*itr - '0');
					int digits = 1;
					while (digits < 3 && itr + 1 < close && *(itr + 1) >= '0' && *(itr + 1) <= '7') {
						value = value * 8 + (unsigned)(*(itr + 1) - '0');
						++itr;
						++digits;
					}
					bgame_po_append(parser, (char)value);
				} else {
					bgame_po_append(parser, *itr);  // \" and \\ among others
				}
				break;
		}
	}
	return true;
}

static void
bgame_po_end_field(bgame_po_parser_t* parser) {
	if (parser->field == FIELD_NONE) { return; }
	if (parser->field != FIELD_SKIPPED) {
		bgame_po_append(parser, '\0');
	}
	parser->field = FIELD_NONE;
}

static void
bgame_po_begin_field(bgame_po_parser_t* parser, bgame_po_field_t field) {
	bgame_po_end_field(parser);
	parser->field = field;
	parser->field_start = parser->translation->pool_len;
	switch (field) {
		case FIELD_CONTEXT: parser->has_context = true; parser->context = parser->field_start; break;
		case FIELD_ID: parser->has_id = true; parser->id = parser->field_start; break;
		case FIELD_STR: parser->has_str = true; parser->str = parser->field_start; break;
		default: break;
	}
}

// The header is the entry with an empty id; its translation holds the fields
static void
bgame_po_read_header(bgame_po_parser_t* parser) {
	bgame_translation_t* translation = parser->translation;
	const char* header = translation->pool + parser->str;
	for (const char* line = header; *line != '\0';) {
		const char* end = strchr(line, '\n');
		if (end == NULL) { end = line + strlen(line); }
		const char* field = "Language:";
		size_t field_len = strlen(field);
		if ((size_t)(end - line) > field_len && memcmp(line, field, field_len) == 0) {
			const char* value = line + field_len;
			while (value < end && (*value == ' ' || *value == '\t')) { ++value; }
			const char* value_end = end;
			while (value_end > value && (*(value_end - 1) == ' ' || *(value_end - 1) == '\t')) { --value_end; }
			size_t start = translation->pool_len;
			memcpy(translation->pool + start, value, (size_t)(value_end - value));
			translation->pool_len += (size_t)(value_end - value);
			bgame_po_append(parser, '\0');
			translation->language = translation->pool + start;
			return;
		}
		line = *end == '\n' ? end + 1 : end;
	}
}

static void
bgame_po_end_entry(bgame_po_parser_t* parser) {
	bgame_po_end_field(parser);
	bgame_translation_t* translation = parser->translation;

	if (parser->has_id && parser->has_str) {
		const char* id = translation->pool + parser->id;
		const char* str = translation->pool + parser->str;
		if (*id == '\0' && !parser->has_context) {
			bgame_po_read_header(parser);
		} else if (parser->plural) {
			BFMT_WARN(parser->path, ":", parser->line, ": plural forms are not supported, skipping \"", id, "\"");
		} else if (!parser->fuzzy && *str != '\0') {
			bgame_translation_key_t key = {
				.context = parser->has_context
					? bgame_str(translation->pool + parser->context)
					: (bgame_str_t){ .chars = "", .len = 0 },
				.id = bgame_str(id),
			};
			bhash_put(&translation->table, key, str);
			++parser->num_entries;
		}
	} else if (parser->has_id || parser->has_context) {
		BFMT_WARN(parser->path, ":", parser->line, ": entry without msgstr");
	}

	parser->has_context = parser->has_id = parser->has_str = false;
	parser->fuzzy = parser->plural = false;
}

static bool
bgame_po_starts_with(const char* line, const char* end, const char* word) {
	size_t len = strlen(word);
	return (size_t)(end - line) >= len && memcmp(line, word, len) == 0;
}

static bool
bgame_po_parse_line(bgame_po_parser_t* parser, const char* line, const char* end) {
	if (line == end) {
		bgame_po_end_entry(parser);
		return true;
	}

	switch (*line) {
		case '#':
			// Comments come before an entry, so one ends the entry before it
			bgame_po_end_entry(parser);
			if (end - line >= 2 && line[1] == ',') {
				// Flags, comma-separated
				for (const char* itr = line + 2; itr < end;) {
					while (itr < end && (*itr == ' ' || *itr == ',')) { ++itr; }
					const char* flag_end = itr;
					while (flag_end < end && *flag_end != ',' && *flag_end != ' ') { ++flag_end; }
					if (flag_end - itr == 5 && memcmp(itr, "fuzzy", 5) == 0) {
						parser->fuzzy = true;
					}
					itr = flag_end;
				}
			}
			return true;
		case '"':
			if (parser->field == FIELD_NONE) {
				BFMT_WARN(parser->path, ":", parser->line, ": string without a keyword");
				return true;
			}
			if (parser->field == FIELD_SKIPPED) { return true; }
			return bgame_po_read_string(parser, line, end);
		default:
			break;
	}

	if (bgame_po_starts_with(line, end, "msgctxt")) {
		bgame_po_end_entry(parser);
		bgame_po_begin_field(parser, FIELD_CONTEXT);
	} else if (bgame_po_starts_with(line, end, "msgid_plural")) {
		parser->plural = true;
		bgame_po_begin_field(parser, FIELD_SKIPPED);
		return true;
	} else if (bgame_po_starts_with(line, end, "msgid")) {
		if (parser->has_id) { bgame_po_end_entry(parser); }
		bgame_po_begin_field(parser, FIELD_ID);
	} else if (bgame_po_starts_with(line, end, "msgstr[")) {
		parser->plural = true;
		bgame_po_begin_field(parser, FIELD_SKIPPED);
		return true;
	} else if (bgame_po_starts_with(line, end, "msgstr")) {
		bgame_po_begin_field(parser, FIELD_STR);
	} else {
		BFMT_WARN(parser->path, ":", parser->line, ": unknown keyword");
		bgame_po_begin_field(parser, FIELD_SKIPPED);
		return true;
	}

	return bgame_po_read_string(parser, line, end);
}

static bool
bgame_po_parse(bgame_po_parser_t* parser, const char* text, size_t len) {
	const char* end = text + len;
	for (const char* line = text; line < end;) {
		const char* line_end = memchr(line, '\n', (size_t)(end - line));
		const char* next = line_end != NULL ? line_end + 1 : end;
		if (line_end == NULL) { line_end = end; }
		if (line_end > line && *(line_end - 1) == '\r') { --line_end; }

		++parser->line;
		if (!bgame_po_parse_line(parser, line, line_end)) { return false; }
		line = next;
	}
	bgame_po_end_entry(parser);
	return true;
}

// }}}

static void
bgame_translation_release(bgame_asset_bundle_t* bundle, bgame_translation_t* translation) {
	bhash_cleanup(&translation->table);
	bgame_asset_free(bundle, translation->pool);
	*translation = (bgame_translation_t){ 0 };
}

static bool
bgame_translation_load(bgame_asset_bundle_t* bundle, void* asset, const char* path) {
	CF_File* file = cf_fs_open_file_for_read(path);
	if (file == NULL) { return false; }
	size_t size = cf_fs_size(file);
	char* text = bgame_asset_malloc(bundle, size);
	bool read = cf_fs_read(file, text, size) == size;
	cf_fs_close(file);
	if (!read) {
		bgame_asset_free(bundle, text);
		return false;
	}

	// Into a fresh translation, so that a broken file leaves the loaded one as it is.
	// Unescaping only shrinks a string and every string gains a NUL in place of
	// its quotes, so the pool fits in the size of the file plus the language.
	bgame_translation_t fresh = {
		.pool = bgame_asset_malloc(bundle, size + 1),
		.language = "",
	};
	// Stable stubs: the table outlives the module across a code reload
	bhash_init(&fresh.table, &(bhash_config_t){
		.hash = BGAME_FN_WRAPPER(bgame_translation_key_hash),
		.eq = BGAME_FN_WRAPPER(bgame_translation_key_eq),
		.memctx = bgame_asset_allocator(bundle),
	});
	bgame_po_parser_t parser = { .translation = &fresh, .path = path };
	bool ok = bgame_po_parse(&parser, text, size);
	bgame_asset_free(bundle, text);
	if (!ok) {
		bgame_translation_release(bundle, &fresh);
		return false;
	}

	if (*fresh.language == '\0') {
		BFMT_WARN(path, ": no Language header, every count will read as 'other'");
	} else if (bfmt_cldr_plural_rule(fresh.language) == NULL) {
		BFMT_WARN(path, ": no plural rules for language \"", fresh.language, "\", every count will read as 'other'");
	}
	BFMT_DEBUG(path, ": ", parser.num_entries, " translations for language \"", fresh.language, "\"");

	bgame_translation_t* translation = asset;
	if (translation->pool != NULL) {
		bgame_translation_release(bundle, translation);
	}
	*translation = fresh;
	return true;
}

static void
bgame_translation_unload(bgame_asset_bundle_t* bundle, void* asset) {
	bgame_translation_t* translation = asset;
	if (translation->pool != NULL) {
		bgame_translation_release(bundle, translation);
	}
}

BGAME_ASSET_TYPE(translation) = {
	.name = "translation",
	.size = sizeof(bgame_translation_t),
	.load = bgame_translation_load,
	.unload = bgame_translation_unload,
};

bgame_translation_t*
bgame_load_translation(bgame_asset_bundle_t* bundle, const char* path) {
	return bgame_asset_load(bundle, &translation, path);
}

const char*
bgame_translation_language(const bgame_translation_t* translation) {
	return translation->language;
}

const char*
bgame_translation_lookup(const bgame_translation_t* translation, const char* context, const char* key) {
	bgame_translation_key_t translation_key = bgame_translation_make_key(context, key);
	bhash_index_t index = bhash_find(&translation->table, translation_key);
	return bhash_is_valid(index) ? translation->table.values[index] : NULL;
}

void
bgame_translation_drop(bgame_translation_t* translation, const char* context, const char* key) {
	bgame_translation_key_t translation_key = bgame_translation_make_key(context, key);
	bhash_index_t index = bhash_find(&translation->table, translation_key);
	if (bhash_is_valid(index)) {
		translation->table.values[index] = NULL;
	}
}
