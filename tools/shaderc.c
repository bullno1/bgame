// bgame-shaderc: an offline shader compiler for Cute Framework. Precompiles GLSL into
// cross-platform bytecode, as a C header or a raw SPIR-V blob.
//
// Ported to C from cute_framework/tools/cute_shaderc.cpp (e32a2a03). Everything that writes
// the outputs is upstream's; the front end differs:
//
// * Arguments are parsed with barg, so options are spelled `--type vertex`, `-o file`.
// * -D<name>[=<value>] defines a preprocessor macro.
// * --depfile writes a Makefile-style depfile listing every file the compile read,
//   however deep the #include that pulled it in.
// * --dump-builtins writes CF's builtin includes to a directory, for tools that have to find
//   them on disk (a language server, glslangValidator).
//
// The compiler itself (cute_shader.cpp, cute_spirv.h) is still built from the CF tree, and
// CF's builtin shaders reach this file through shaderc_builtins.cpp.
//
// See: https://randygaul.github.io/cute_framework/topics/shader_compilation

// mkdir under a strict -std.
#if defined(__linux__) && !defined(_DEFAULT_SOURCE)
#	define _DEFAULT_SOURCE 1
#endif

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <errno.h>
#ifdef _WIN32
#	include <direct.h>
#else
#	include <sys/stat.h>
#endif
#include <cute_alloc.h>
#include "cute_shader.h"
#include "shaderc_builtins.h"

// Only for bhash_hash (chibihash64): included ahead of BLIB_IMPLEMENTATION so the table
// implementation stays out.
#include <bhash.h>

// bhamt never frees, so its nodes go in the arena. barray stays on the libc allocator.
#define BHAMT_ALLOC(size, align, ctx) barena_memalign((barena_t*)(ctx), (size), (align))
#define BLIB_IMPLEMENTATION
#include <barg.h>
#include <barena.h>
#include <barray.h>
#include <bhamt.h>

#define HEADER_LINE_SIZE 16

typedef enum
{
	SHADER_TYPE_VERTEX,
	SHADER_TYPE_FRAGMENT,
	SHADER_TYPE_COMPUTE,
	SHADER_TYPE_DRAW,
} shader_type_t;

// The content comes from the arena: nobody frees it, it goes away with everything else.
static char* read_file(barena_t* arena, const char* path, size_t* size_out)
{
	errno = 0;
	FILE* file = fopen(path, "rb");
	if (file == NULL) {
		return NULL;
	}

	if (fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		return NULL;
	}
	long size = ftell(file);
	if (size < 0) {
		fclose(file);
		return NULL;
	}

	if (fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		return NULL;
	}
	barena_snapshot_t snapshot = barena_snapshot(arena);
	char* content = barena_memalign(arena, (size_t)size + 1, _Alignof(char));
	fread(content, size, 1, file);
	if (ferror(file)) {
		barena_restore(arena, snapshot);
		fclose(file);
		return NULL;
	}
	content[size] = 0;
	if (size_out != NULL) { *size_out = (size_t)size; }
	fclose(file);
	return content;
}

static bool write_bytecode(
	FILE* file,
	CF_ShaderCompilerResult compile_result,
	const char* var_name,
	const char* suffix
)
{
	const uint8_t* content = compile_result.bytecode.content;

	// Write preprocessed shader as comment (-verbose only; it dominates header size).
	if (compile_result.preprocessed_source) {
		fprintf(file, "/*\n");
		fprintf(file, "%.*s\n", (int)compile_result.preprocessed_source_size, compile_result.preprocessed_source);
		fprintf(file, "*/\n");
	}

	// Write the bytecode.
	fprintf(file, "#ifndef __EMSCRIPTEN__\n");
	fprintf(file, "static const uint8_t %s%s_content[%zu] = {", var_name, suffix, compile_result.bytecode.size);
	for (size_t i = 0; i < compile_result.bytecode.size; ++i) {
		if ((i % HEADER_LINE_SIZE) == 0) {
			fprintf(file, "\n\t");
		}
		fprintf(file, "0x%02X,", content[i]);
	}
	fprintf(file, "\n};\n");
	fprintf(file, "#endif\n");

	// Write GLSL 300 (absent with --no-gles: the bytecode then only works on
	// non-GLES backends).
	if (compile_result.bytecode.glsl300_src) {
		fprintf(file, "static const char %s%s_glsl300_src[%zu] =\n\"", var_name, suffix, compile_result.bytecode.glsl300_src_size + 1);
		for (size_t i = 0; i < compile_result.bytecode.glsl300_src_size; ++i) {
			char ch = compile_result.bytecode.glsl300_src[i];
			if (ch == '\n') {  // Escape new line as \n and start an actual new line
				fprintf(file, "\\n\"\n\"");
			} else {
				fprintf(file, "%c", ch);
			}
		}
		fprintf(file, "\";\n");
	} else {
		fprintf(file, "#define %s%s_glsl300_src NULL\n", var_name, suffix);
	}

	// Write HLSL (for D3D12, compiled to DXBC by the system FXC at runtime).
	if (compile_result.bytecode.hlsl_src) {
		fprintf(file, "static const char %s%s_hlsl_src[%zu] =\n\"", var_name, suffix, compile_result.bytecode.hlsl_src_size + 1);
		for (size_t i = 0; i < compile_result.bytecode.hlsl_src_size; ++i) {
			char ch = compile_result.bytecode.hlsl_src[i];
			if (ch == '\n') {  // Escape new line as \n and start an actual new line
				fprintf(file, "\\n\"\n\"");
			} else {
				fprintf(file, "%c", ch);
			}
		}
		fprintf(file, "\";\n");
	} else {
		fprintf(file, "#define %s%s_hlsl_src NULL\n", var_name, suffix);
	}

	// Write MSL (for Metal, compiled by the OS at runtime).
	if (compile_result.bytecode.msl_src) {
		fprintf(file, "static const char %s%s_msl_src[%zu] =\n\"", var_name, suffix, compile_result.bytecode.msl_src_size + 1);
		for (size_t i = 0; i < compile_result.bytecode.msl_src_size; ++i) {
			char ch = compile_result.bytecode.msl_src[i];
			if (ch == '\n') {  // Escape new line as \n and start an actual new line
				fprintf(file, "\\n\"\n\"");
			} else if (ch == '"') {
				fprintf(file, "\\\"");
			} else {
				fprintf(file, "%c", ch);
			}
		}
		fprintf(file, "\";\n");
	} else {
		fprintf(file, "#define %s%s_msl_src NULL\n", var_name, suffix);
	}

	// Write reflection info.
	const CF_ShaderInfo* shader_info = &compile_result.bytecode.shader_info;

	if (shader_info->num_images > 0) {
		fprintf(file, "static const char* %s%s_image_names[%d] = {\n   ", var_name, suffix, shader_info->num_images);
		for (int i = 0; i < shader_info->num_images; ++i) {
			fprintf(file, " \"%s\",", shader_info->image_names[i]);
		}
		fprintf(file, "\n};\n");
		fprintf(file, "static int %s%s_image_binding_slots[%d] = {", var_name, suffix, shader_info->num_images);
		for (int i = 0; i < shader_info->num_images; ++i) {
			fprintf(file, " %d,", shader_info->image_binding_slots[i]);
		}
		fprintf(file, " };\n");
	} else {
		fprintf(file, "#define %s%s_image_names NULL\n", var_name, suffix);
		fprintf(file, "#define %s%s_image_binding_slots NULL\n", var_name, suffix);
	}

	if (shader_info->num_uniforms > 0) {
		fprintf(file, "static CF_ShaderUniformInfo %s%s_uniforms[%d] = {\n", var_name, suffix, shader_info->num_uniforms);
		for (int i = 0; i < shader_info->num_uniforms; ++i) {
			fprintf(file, "\t{\n");
			fprintf(file, "\t\t.block_name = \"%s\",\n", shader_info->uniforms[i].block_name);
			fprintf(file, "\t\t.block_index = %d,\n", shader_info->uniforms[i].block_index);
			fprintf(file, "\t\t.block_size = %d,\n", shader_info->uniforms[i].block_size);
			fprintf(file, "\t\t.num_members = %d,\n", shader_info->uniforms[i].num_members);
			fprintf(file, "\t},\n");
		}
		fprintf(file, "};\n");
	} else {
		fprintf(file, "#define %s%s_uniforms NULL\n", var_name, suffix);
	}

	if (shader_info->num_uniform_members > 0) {
		fprintf(file, "static CF_ShaderUniformMemberInfo %s%s_uniform_members[%d] = {\n", var_name, suffix, shader_info->num_uniform_members);
		for (int i = 0; i < shader_info->num_uniform_members; ++i) {
			fprintf(file, "\t{\n");
			fprintf(file, "\t\t.name = \"%s\",\n", shader_info->uniform_members[i].name);
			fprintf(file, "\t\t.type = %s,\n", cf_shader_info_data_type_to_string(shader_info->uniform_members[i].type));
			fprintf(file, "\t\t.offset = %d,\n", shader_info->uniform_members[i].offset);
			fprintf(file, "\t\t.array_length = %d,\n", shader_info->uniform_members[i].array_length);
			fprintf(file, "\t},\n");
		}
		fprintf(file, "};\n");
	} else {
		fprintf(file, "#define %s%s_uniform_members NULL\n", var_name, suffix);
	}

	if (shader_info->num_inputs > 0) {
		fprintf(file, "static CF_ShaderInputInfo %s%s_inputs[%d] = {\n", var_name, suffix, shader_info->num_inputs);
		for (int i = 0; i < shader_info->num_inputs; ++i) {
			fprintf(file, "\t{\n");
			fprintf(file, "\t\t.name = \"%s\",\n", shader_info->inputs[i].name);
			fprintf(file, "\t\t.location = %d,\n", shader_info->inputs[i].location);
			fprintf(file, "\t\t.format = %s,\n", cf_shader_info_data_type_to_string(shader_info->inputs[i].format));
			fprintf(file, "\t},\n");
		}
		fprintf(file, "};\n");
	} else {
		fprintf(file, "#define %s%s_inputs NULL\n", var_name, suffix);
	}

	return ferror(file) == 0;
}

static bool write_bytecode_struct_contents(
	FILE* file,
	CF_ShaderCompilerResult compile_result,
	const char* var_name,
	const char* suffix,
	int tabs
)
{
	const CF_ShaderInfo* shader_info = &compile_result.bytecode.shader_info;

#define TABS() fprintf(file, "%s", tabs == 0 ? "" : (tabs == 1 ? "\t" : (tabs == 2 ? "\t\t" : (tabs == 3 ? "\t\t\t" : "\t\t\t\t"))))

	// Write the struct.
	fprintf(file, "#ifndef __EMSCRIPTEN__\n");
	TABS(); fprintf(file, ".content = %s%s_content,\n", var_name, suffix);
	TABS(); fprintf(file, ".size = %zu,\n", compile_result.bytecode.size);
	fprintf(file, "#endif\n");
	TABS(); fprintf(file, ".glsl300_src = %s%s_glsl300_src,\n", var_name, suffix);
	TABS(); fprintf(file, ".glsl300_src_size = %zu,\n", compile_result.bytecode.glsl300_src_size);
	TABS(); fprintf(file, ".hlsl_src = %s%s_hlsl_src,\n", var_name, suffix);
	TABS(); fprintf(file, ".hlsl_src_size = %zu,\n", compile_result.bytecode.hlsl_src_size);
	TABS(); fprintf(file, ".msl_src = %s%s_msl_src,\n", var_name, suffix);
	TABS(); fprintf(file, ".msl_src_size = %zu,\n", compile_result.bytecode.msl_src_size);
	TABS(); fprintf(file, ".shader_info = {\n");
	TABS(); fprintf(file, "\t.num_samplers = %d,\n", shader_info->num_samplers);
	TABS(); fprintf(file, "\t.num_storage_textures = %d,\n", shader_info->num_storage_textures);
	TABS(); fprintf(file, "\t.num_storage_buffers = %d,\n", shader_info->num_storage_buffers);
	TABS(); fprintf(file, "\t.num_readwrite_storage_textures = %d,\n", shader_info->num_readwrite_storage_textures);
	TABS(); fprintf(file, "\t.num_readwrite_storage_buffers = %d,\n", shader_info->num_readwrite_storage_buffers);
	TABS(); fprintf(file, "\t.num_images = %d,\n", shader_info->num_images);
	TABS(); fprintf(file, "\t.image_names = %s%s_image_names,\n", var_name, suffix);
	TABS(); fprintf(file, "\t.image_binding_slots = %s%s_image_binding_slots,\n", var_name, suffix);
	TABS(); fprintf(file, "\t.num_uniforms = %d,\n", shader_info->num_uniforms);
	TABS(); fprintf(file, "\t.uniforms = %s%s_uniforms,\n", var_name, suffix);
	TABS(); fprintf(file, "\t.num_uniform_members = %d,\n", shader_info->num_uniform_members);
	TABS(); fprintf(file, "\t.uniform_members = %s%s_uniform_members,\n", var_name, suffix);
	TABS(); fprintf(file, "\t.num_inputs = %d,\n", shader_info->num_inputs);
	TABS(); fprintf(file, "\t.inputs = %s%s_inputs,\n", var_name, suffix);
	TABS(); fprintf(file, "\t.local_size = { %d, %d, %d },\n", shader_info->local_size[0], shader_info->local_size[1], shader_info->local_size[2]);
	TABS(); fprintf(file, "},\n");

#undef TABS

	return ferror(file) == 0;
}


static bool write_bytecode_struct(
	FILE* file,
	CF_ShaderCompilerResult compile_result,
	const char* var_name,
	const char* suffix
)
{
	write_bytecode(file, compile_result, var_name, suffix);
	fprintf(file, "static const CF_ShaderBytecode %s%s = {\n", var_name, suffix);
	write_bytecode_struct_contents(file, compile_result, var_name, suffix, 1);
	fprintf(file, "};\n");

	return ferror(file) == 0;
}

static bool write_draw_bytecode_struct(
	FILE* file,
	CF_ShaderCompilerResult draw_result,
	CF_ShaderCompilerResult blit_result,
	const char* var_name
)
{
	fprintf(file, "static const CF_DrawShaderBytecode %s = {\n", var_name);
	fprintf(file, "\t.draw_shader = {\n");
	if (!write_bytecode_struct_contents(file, draw_result, var_name, "_draw", 2)) {
		fclose(file);
		return false;
	}
	fprintf(file, "\t},\n");
	fprintf(file, "\t.blit_shader = {\n");
	if (!write_bytecode_struct_contents(file, blit_result, var_name, "_blit", 2)) {
		fclose(file);
		return false;
	}
	fprintf(file, "\t},\n");
	fprintf(file, "};\n");

	return ferror(file) == 0;
}

static bool write_draw_header_file(
	const char* path,
	CF_ShaderCompilerResult draw_result,
	CF_ShaderCompilerResult blit_result,
	const char* var_name
)
{
	errno = 0;
	FILE* file = fopen(path, "wb");
	if (file == NULL) {
		return false;
	}

	fprintf(file, "#pragma once\n\n");

	// Write the constant.
	write_bytecode(file, draw_result, var_name, "_draw");
	write_bytecode(file, blit_result, var_name, "_blit");
	write_draw_bytecode_struct(file, draw_result, blit_result, var_name);

	if (ferror(file) != 0) {
		fclose(file);
		return false;
	}

	if (fflush(file) != 0) {
		fclose(file);
		return false;
	}

	return fclose(file) == 0;
}

static bool write_standalone_header_file(
	const char* path,
	CF_ShaderCompilerResult compile_result,
	const char* var_name
)
{
	errno = 0;
	FILE* file = fopen(path, "wb");
	if (file == NULL) {
		return false;
	}

	fprintf(file, "#pragma once\n\n");

	if (!write_bytecode_struct(file, compile_result, var_name, "")) {
		fclose(file);
		return false;
	}
	if (fflush(file) != 0) {
		fclose(file);
		return false;
	}

	return fclose(file) == 0;
}

static bool write_bytecode_file(
	const char* path,
	CF_ShaderCompilerResult compile_result
)
{
	errno = 0;
	FILE* file = fopen(path, "wb");
	if (file == NULL) { return false; }

	fwrite(compile_result.bytecode.content, compile_result.bytecode.size, 1, file);
	if (ferror(file) != 0) {
		fclose(file);
		return false;
	}
	if (fflush(file) != 0) {
		fclose(file);
		return false;
	}

	return fclose(file) == 0;
}

//--------------------------------------------------------------------------------------------------
// Arena.

// File contents, the string copies the lists point at and the set of dependencies come from
// one arena that is dropped at exit. The first two are bytes, so they are allocated unaligned. The
// lists themselves are barrays on the libc allocator, each freed on its own.

static char*
arena_strndup(barena_t* arena, const char* str, size_t len)
{
	char* copy = barena_memalign(arena, len + 1, _Alignof(char));
	memcpy(copy, str, len);
	copy[len] = '\0';
	return copy;
}

//--------------------------------------------------------------------------------------------------
// Dependency tracking. Every file the compiler reads comes through the VFS, including the
// includes of includes, so recording successful reads there yields the transitive closure.
// Builtin includes live in memory and never show up, which is right: they change with the
// tool, and the build already depends on that.

typedef struct
{
	barena_t* arena;
	// Iterates in hash order, not include order. A depfile does not care, and the order
	// is still the same from run to run: the hash only looks at the path's bytes.
	BHAMT_SET(const char*) files;
} dep_tracker_t;

// The keys are strings, not the pointers to them.
static bhamt_hash_t dep_path_hash(const void* key, size_t size)
{
	(void)size;
	const char* path = *(const char* const*)key;
	return bhash_hash(path, strlen(path));
}

static bool dep_path_eq(const void* lhs, const void* rhs, size_t size)
{
	(void)size;
	return strcmp(*(const char* const*)lhs, *(const char* const*)rhs) == 0;
}

static void dep_tracker_init(dep_tracker_t* tracker, barena_t* arena)
{
	*tracker = (dep_tracker_t){ .arena = arena };
	bhamt_init(&tracker->files, dep_path_hash, dep_path_eq);
}

static void dep_tracker_add(dep_tracker_t* tracker, const char* path)
{
	// Within one compile CF's automatic include guard already reads a file once, but the
	// draw path runs four passes over the same stub, each with its own guard.
	if (bhamt_has(&tracker->files, path)) { return; }

	// The set keeps the pointer, so it has to be the copy: `path` dies with the call.
	const char* copy = arena_strndup(tracker->arena, path, strlen(path));
	bhamt_set_add(&tracker->files, copy, tracker->arena);
}

static char* tracked_read_file_content(const char* path, size_t* len, void* context)
{
	// Include dirs are probed in order; only the hit is a dependency.
	dep_tracker_t* tracker = context;
	char* content = read_file(tracker->arena, path, len);
	if (content != NULL) { dep_tracker_add(tracker, path); }
	return content;
}

// The compiler copies the content and hands it straight back. It stays in the arena.
static void tracked_free_file_content(char* content, void* context)
{
	(void)content;
	(void)context;
}

static void write_depfile_path(FILE* file, const char* path)
{
	for (const char* c = path; *c != '\0'; ++c) {
		switch (*c) {
			case ' ':  fputs("\\ ", file); break;
			case '#':  fputs("\\#", file); break;
			case '$':  fputs("$$", file); break;
			case '\\': fputc('/', file); break;  // A backslash is an escape to make and ninja
			default:   fputc(*c, file); break;
		}
	}
}

// The tracker is not const: BHAMT_FOREACH declares its node with typeof(table->root), which
// through a const table is a const pointer it cannot advance.
static bool write_depfile(const char* path, const char* target, const char* input_path, dep_tracker_t* tracker)
{
	errno = 0;
	FILE* file = fopen(path, "wb");
	if (file == NULL) { return false; }

	write_depfile_path(file, target);
	fputs(": \\\n  ", file);
	write_depfile_path(file, input_path);
	BHAMT_FOREACH(node, &tracker->files) {
		fputs(" \\\n  ", file);
		write_depfile_path(file, node->key);
	}
	fputc('\n', file);

	bool ok = !ferror(file);
	return fclose(file) == 0 && ok;
}

//--------------------------------------------------------------------------------------------------
// --dump-builtins. The builtin includes only exist inside this tool, so anything else that
// wants to resolve `#include "smooth_uv.shd"` needs them as files.

static bool make_dir(const char* path)
{
	errno = 0;
#ifdef _WIN32
	int result = _mkdir(path);
#else
	int result = mkdir(path, 0777);
#endif
	return result == 0 || errno == EEXIST;
}

// Leaves a file with the right content alone, so its mtime only moves when CF changes it
// and an editor watching the directory is not woken by every build.
static bool write_file_if_changed(barena_t* arena, const char* path, const char* content)
{
	size_t size = strlen(content);

	barena_snapshot_t snapshot = barena_snapshot(arena);
	size_t old_size = 0;
	char* old_content = read_file(arena, path, &old_size);
	bool unchanged = old_content != NULL && old_size == size && memcmp(old_content, content, size) == 0;
	barena_restore(arena, snapshot);
	if (unchanged) { return true; }

	errno = 0;
	FILE* file = fopen(path, "wb");
	if (file == NULL) { return false; }

	bool ok = fwrite(content, 1, size, file) == size;
	return fclose(file) == 0 && ok;
}

static bool dump_builtins(barena_t* arena, const char* dir)
{
	if (!make_dir(dir)) {
		perror(dir);
		return false;
	}

	size_t dir_len = strlen(dir);
	for (int i = 0; i < shaderc_num_builtin_includes; ++i) {
		const CF_ShaderCompilerFile* builtin = &shaderc_builtin_includes[i];

		size_t name_len = strlen(builtin->name);
		char* path = barena_memalign(arena, dir_len + 1 + name_len + 1, _Alignof(char));
		memcpy(path, dir, dir_len);
		path[dir_len] = '/';
		memcpy(path + dir_len + 1, builtin->name, name_len + 1);

		if (!write_file_if_changed(arena, path, builtin->content)) {
			perror(path);
			return false;
		}
	}

	return true;
}

//--------------------------------------------------------------------------------------------------
// Option parsers.

typedef struct
{
	barena_t* arena;
	barray(CF_ShaderCompilerDefine) defines;
} define_list_t;

// <name>[=<value>]. The name is a copy since argv is not ours to cut up.
static const char* parse_define(void* userdata, const char* value)
{
	define_list_t* list = userdata;

	const char* equal = strchr(value, '=');
	size_t name_len = equal != NULL ? (size_t)(equal - value) : strlen(value);
	if (name_len == 0) { return "A define needs a name"; }

	CF_ShaderCompilerDefine define = {
		.name = arena_strndup(list->arena, value, name_len),
		.value = equal != NULL ? equal + 1 : "1",
	};
	barray_push(list->defines, define, NULL);
	return NULL;
}

// The values point into argv, so there is nothing to copy.
static const char* parse_include_dir(void* userdata, const char* value)
{
	barray(const char*)* dirs = userdata;
	barray_push(*dirs, value, NULL);
	return NULL;
}

static const char* parse_type(void* userdata, const char* value)
{
	shader_type_t* type = userdata;
	if (strcmp(value, "vertex") == 0) {
		*type = SHADER_TYPE_VERTEX;
	} else if (strcmp(value, "fragment") == 0) {
		*type = SHADER_TYPE_FRAGMENT;
	} else if (strcmp(value, "compute") == 0) {
		*type = SHADER_TYPE_COMPUTE;
	} else if (strcmp(value, "draw") == 0) {
		*type = SHADER_TYPE_DRAW;
	} else {
		return "Invalid shader type";
	}
	return NULL;
}

int main(int argc, const char* argv[])
{
	const char* input_path = NULL;
	const char* output_header_path = NULL;
	const char* output_bytecode_path = NULL;
	const char* output_depfile_path = NULL;
	const char* dump_builtins_dir = NULL;
	const char* var_name = NULL;
	bool nogles = false;
	bool verbose = false;
	shader_type_t type = SHADER_TYPE_DRAW;

	barena_pool_t arena_pool;
	barena_pool_init(&arena_pool, 64 * 1024);
	barena_t arena;
	barena_init(&arena, &arena_pool);

	define_list_t user_defines = { .arena = &arena };
	barray(const char*) include_dirs = NULL;
	dep_tracker_t dep_tracker;
	dep_tracker_init(&dep_tracker, &arena);
	barray(CF_ShaderCompilerFile) builtin_includes = NULL;

	int return_code = 1;
	char* input_content = NULL;

	barg_opt_t opts[] = {
		{
			.name = "type",
			.short_name = 't',
			.summary = "The shader type, defaults to draw",
			.description =
				"One of:\n"
				"* draw: Draw shader for `cf_make_draw_shader_from_bytecode`.\n"
				"* vertex: Standalone vertex shader for `cf_make_shader_from_bytecode`.\n"
				"* fragment: Standalone fragment shader for `cf_make_shader_from_bytecode`.\n"
				"* compute: Standalone compute shader.",
			.value_name = "type",
			.parser = { &type, parse_type },
		},
		{
			.name = "include-dir",
			.short_name = 'I',
			.summary = "Add a directory to the #include search path",
			.value_name = "dir",
			.repeatable = true,
			.parser = { &include_dirs, parse_include_dir },
		},
		{
			.name = "define",
			.short_name = 'D',
			.summary = "Define a preprocessor macro, the value defaults to 1",
			.value_name = "name[=value]",
			.repeatable = true,
			.parser = { &user_defines, parse_define },
		},
		{
			.name = "header",
			.short_name = 'o',
			.summary = "Where to write the C header, also requires --varname",
			.value_name = "file",
			.parser = barg_str(&output_header_path),
		},
		{
			.name = "varname",
			.short_name = 'n',
			.summary = "The variable name inside the C header",
			.value_name = "name",
			.parser = barg_str(&var_name),
		},
		{
			.name = "bytecode",
			.short_name = 'b',
			.summary = "Where to write the raw SPIRV blob, not for draw shaders",
			.value_name = "file",
			.parser = barg_str(&output_bytecode_path),
		},
		{
			.name = "depfile",
			.short_name = 'd',
			.summary = "Where to write a Makefile-style depfile",
			.description =
				"Lists the input and every file it includes, transitively.\n"
				"The target is the header, or the bytecode when there is no header.",
			.value_name = "file",
			.parser = barg_str(&output_depfile_path),
		},
		{
			.name = "dump-builtins",
			.summary = "Write CF's builtin includes to a directory and exit",
			.description =
				"For tools that resolve #include on disk: add the directory to their search path.\n"
				"The directory is created if needed, its parents are not. Files that already have\n"
				"the right content are left alone. Takes no input.",
			.value_name = "dir",
			.parser = barg_str(&dump_builtins_dir),
		},
		{
			.name = "no-gles",
			.summary = "Skip the GLSL ES 300 output",
			.boolean = true,
			.parser = barg_boolean(&nogles),
		},
		{
			.name = "verbose",
			.short_name = 'v',
			.summary = "Embed the preprocessed source",
			.boolean = true,
			.parser = barg_boolean(&verbose),
		},
		barg_opt_help(),
	};

	barg_t barg = {
		.num_opts = sizeof(opts) / sizeof(opts[0]),
		.opts = opts,
		.allow_positional = true,
		.usage = "bgame-shaderc [options] [--] <input>\n       bgame-shaderc --dump-builtins <dir>",
		.summary = "Compile GLSL into SPIRV bytecode and generate a C header for embedding.",
	};

	barg_result_t parse_result = barg_parse(&barg, argc, argv);
	if (parse_result.status != BARG_OK) {
		barg_print_result(&barg, parse_result, stderr);
		return_code = parse_result.status == BARG_SHOW_HELP ? 0 : 1;
		goto end;
	}

	if (dump_builtins_dir != NULL) {
		if (parse_result.arg_index < argc) {
			fprintf(stderr, "--dump-builtins takes no input\n");
			goto end;
		}
		if (dump_builtins(&arena, dump_builtins_dir)) { return_code = 0; }
		goto end;
	}

	{
		if (parse_result.arg_index >= argc) {
			fprintf(stderr, "Please specify an input, use --help for more info\n");
			fprintf(stderr, "You can also visit https://randygaul.github.io/cute_framework/topics/shader_compilation\n");
			goto end;
		}

		if (parse_result.arg_index + 1 < argc) {
			fprintf(stderr, "Please specify only one input file, after the options\n");
			goto end;
		}
		input_path = argv[parse_result.arg_index];

		if (output_header_path != NULL && var_name == NULL) {
			fprintf(stderr, "--header also requires --varname\n");
			goto end;
		}

		if (
			!(type == SHADER_TYPE_VERTEX || type == SHADER_TYPE_FRAGMENT || type == SHADER_TYPE_COMPUTE)
			&& output_bytecode_path != NULL
		) {
			fprintf(stderr, "--bytecode is only valid for shader of type 'vertex', 'fragment', or 'compute'\n");
			goto end;
		}

		if (output_depfile_path != NULL && output_header_path == NULL && output_bytecode_path == NULL) {
			fprintf(stderr, "--depfile needs an output to name as its target: --header or --bytecode\n");
			goto end;
		}
	}

	for (int i = 0; i < shaderc_num_builtin_includes; ++i) {
		barray_push(builtin_includes, shaderc_builtin_includes[i], NULL);
	}

	CF_ShaderCompilerVfs vfs = {
		.read_file_content = tracked_read_file_content,
		.free_file_content = tracked_free_file_content,
		.context = &dep_tracker,
	};

	input_content = read_file(&arena, input_path, NULL);
	if (input_content == NULL) {
		perror("Error while reading input");
		goto end;
	}

	if (type == SHADER_TYPE_DRAW) {
		CF_ShaderCompilerFile stub = {
			.name = "shader_stub.shd",
			.content = input_content,
		};
		barray_push(builtin_includes, stub, NULL);

		// CF_PAYLOAD_BINDING and CF_GLES are pushed after the user's defines. A push can
		// move the array, so the config is pointed at it again each time.
		CF_ShaderCompilerConfig config = {
			.num_builtin_defines = (int)barray_len(user_defines.defines),
			.builtin_defines = user_defines.defines,

			.num_builtin_includes = (int)barray_len(builtin_includes),
			.builtin_includes = builtin_includes,

			.num_include_dirs = (int)barray_len(include_dirs),
			.include_dirs = include_dirs,

			.automatic_include_guard = true,
			.return_preprocessed_source = verbose,

			// The SSBO draw flavor cannot transpile to GLSL 300 es; the GLES flavor's
			// transpile is grafted in below.
			.skip_glsl300 = true,

			.vfs = &vfs,
		};

		// The payload storage buffer binds right after the stub's last sampler.
		// Scan the preprocessed stub so comments/macros can't fool the scan.
		char payload_binding_str[16];
		int payload_binding = 1;
		{
			char* preprocessed = cute_shader_preprocess(input_content, config);
			if (preprocessed) {
				payload_binding = shaderc_compute_payload_binding(preprocessed);
				cf_free(preprocessed);
			}
		}
		snprintf(payload_binding_str, sizeof(payload_binding_str), "%d", payload_binding);
		CF_ShaderCompilerDefine payload_binding_define = { "CF_PAYLOAD_BINDING", payload_binding_str };
		barray_push(user_defines.defines, payload_binding_define, NULL);
		config.builtin_defines = user_defines.defines;
		config.num_builtin_defines = (int)barray_len(user_defines.defines);

		CF_ShaderCompilerResult draw_shader_result = cute_shader_compile(
			shaderc_draw_fs,
			CUTE_SHADER_STAGE_FRAGMENT,
			config
		);
		if (!draw_shader_result.success) {
			fprintf(stderr, "%s\n", draw_shader_result.error_message);
			cute_shader_free_result(draw_shader_result);
			goto end;
		}

		// GLES3/WebGL2 runs the texel-fetch flavor of the draw shader (CF_GLES
		// define); compile the same stub against it and graft its GLSL 300 es
		// output into the primary bytecode. Skipped entirely with --no-gles.
		CF_ShaderCompilerResult draw_gles_result = { 0 };
		if (!nogles) {
			config.skip_glsl300 = false;
			config.skip_hlsl = true; // Only the GLSL 300 output is grafted from this flavor.
			config.skip_msl = true;
			CF_ShaderCompilerDefine gles_define = { "CF_GLES", "1" };
			barray_push(user_defines.defines, gles_define, NULL);
			config.builtin_defines = user_defines.defines;
			config.num_builtin_defines = (int)barray_len(user_defines.defines);
			draw_gles_result = cute_shader_compile(
				shaderc_draw_fs,
				CUTE_SHADER_STAGE_FRAGMENT,
				config
			);
			if (!draw_gles_result.success) {
				fprintf(stderr, "%s\n", draw_gles_result.error_message);
				cute_shader_free_result(draw_shader_result);
				cute_shader_free_result(draw_gles_result);
				goto end;
			}
			draw_shader_result.bytecode.glsl300_src = draw_gles_result.bytecode.glsl300_src;
			draw_shader_result.bytecode.glsl300_src_size = draw_gles_result.bytecode.glsl300_src_size;
			// The blit shader below wants its GLSL 300 too; drop the CF_GLES define
			// (blit has no flavors) but keep transpilation on.
			(void)barray_pop(user_defines.defines);
			config.num_builtin_defines = (int)barray_len(user_defines.defines);
			config.skip_hlsl = false;
			config.skip_msl = false;
		} else {
			config.skip_glsl300 = true;
		}

		CF_ShaderCompilerResult blit_shader_result = cute_shader_compile(
			shaderc_blit_fs,
			CUTE_SHADER_STAGE_FRAGMENT,
			config
		);
		if (!blit_shader_result.success) {
			fprintf(stderr, "%s\n", blit_shader_result.error_message);
			cute_shader_free_result(draw_shader_result);
			cute_shader_free_result(draw_gles_result);
			cute_shader_free_result(blit_shader_result);
			goto end;
		}

		bool wrote_ok = true;
		if (output_header_path != NULL) {
			wrote_ok = write_draw_header_file(
				output_header_path,
				draw_shader_result, blit_shader_result,
				var_name
			);
			if (!wrote_ok) perror("Error while writing header");
		}

		// The grafted glsl300 pointer belongs to draw_gles_result; detach before freeing.
		draw_shader_result.bytecode.glsl300_src = NULL;
		draw_shader_result.bytecode.glsl300_src_size = 0;
		cute_shader_free_result(draw_shader_result);
		cute_shader_free_result(draw_gles_result);
		cute_shader_free_result(blit_shader_result);
		if (!wrote_ok) goto end;
	} else {
		CF_ShaderCompilerFile stub = {
			.name = "shader_stub.shd",
			.content = shaderc_shader_stub,
		};
		barray_push(builtin_includes, stub, NULL);

		CF_ShaderCompilerConfig config = {
			.num_builtin_defines = (int)barray_len(user_defines.defines),
			.builtin_defines = user_defines.defines,

			.num_builtin_includes = (int)barray_len(builtin_includes),
			.builtin_includes = builtin_includes,

			.num_include_dirs = (int)barray_len(include_dirs),
			.include_dirs = include_dirs,

			.automatic_include_guard = true,
			.return_preprocessed_source = verbose,
			.skip_glsl300 = nogles,

			.vfs = &vfs,
		};

		CF_ShaderCompilerStage compile_stage = CUTE_SHADER_STAGE_FRAGMENT;
		if (type == SHADER_TYPE_VERTEX) compile_stage = CUTE_SHADER_STAGE_VERTEX;
		else if (type == SHADER_TYPE_COMPUTE) compile_stage = CUTE_SHADER_STAGE_COMPUTE;

		CF_ShaderCompilerResult result = cute_shader_compile(
			input_content,
			compile_stage,
			config
		);
		if (!result.success) {
			fprintf(stderr, "%s\n", result.error_message);
			cute_shader_free_result(result);
			goto end;
		}

		if (output_header_path != NULL) {
			if (!write_standalone_header_file(
				output_header_path,
				result,
				var_name
			)) {
				perror("Error while writing header");
				cute_shader_free_result(result);
				goto end;
			}
		}

		if (output_bytecode_path != NULL) {
			if (!write_bytecode_file(output_bytecode_path, result)) {
				perror("Error while writing bytecode");
				cute_shader_free_result(result);
				goto end;
			}
		}

		cute_shader_free_result(result);
	}

	// Last, so a depfile only ever describes outputs that were written.
	if (output_depfile_path != NULL) {
		const char* target = output_header_path != NULL ? output_header_path : output_bytecode_path;
		if (!write_depfile(output_depfile_path, target, input_path, &dep_tracker)) {
			perror("Error while writing depfile");
			goto end;
		}
	}

	return_code = 0;
end:
	barray_free(builtin_includes, NULL);
	barray_free(include_dirs, NULL);
	barray_free(user_defines.defines, NULL);
	barena_reset(&arena);
	barena_pool_cleanup(&arena_pool);

	return return_code;
}
