option(RELOADABLE "Is the program reloadable" ON)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_EXTENSIONS OFF)

if (MSVC)
	add_compile_options(/W3 /WX)
	add_compile_options(/wd4200) # Flexible array member is a standard feature since C99
	add_compile_options(/wd4324) # _Alignof is intentional
	add_compile_options(/wd4100) # Unreferenced parameter
	add_compile_options(/wd4459) # Hiding global definition
	add_compile_options(/experimental:c11atomics)  # Atomics
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
	add_compile_options(
		-Wall -Wextra -pedantic -Werror
		-Wno-unused-parameter
		-Wno-unused-variable
		-Wno-unused-function
		-Wno-overlength-strings
		-Wno-missing-field-initializers
		-Wno-dollar-in-identifier-extension
		-Wno-error=c23-extensions
	)
endif()

function (add_bgame_app NAME SOURCES)
	if (RELOADABLE)
		add_library(${NAME} SHARED ${SOURCES})
		set_target_properties(${NAME} PROPERTIES C_VISIBILITY_PRESET hidden)
		target_link_libraries(${NAME} PRIVATE bgame)
		if (NOT MSVC)
			target_link_options(${NAME} PRIVATE
				-Wl,--exclude-libs,ALL
				-Wl,--no-whole-archive
				-Wl,--no-undefined
			)
		endif ()
		set_target_properties(${NAME} PROPERTIES
			OUTPUT_NAME "${NAME}"
			PREFIX ""
		)

		add_executable(${NAME}-loader "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/loader_stub.c")
		target_link_libraries(${NAME}-loader PRIVATE bgame-loader)
		set_target_properties(${NAME}-loader PROPERTIES
			OUTPUT_NAME "${NAME}"
			PREFIX ""
		)
	else ()
		add_executable(${NAME} ${SOURCES})
		target_link_libraries(${NAME} PRIVATE bgame bgame-loader-stub bgame-loader)
	endif ()

	if (EMSCRIPTEN)
		set_target_properties(${NAME} PROPERTIES OUTPUT_NAME "${NAME}" SUFFIX ".html")
		target_compile_options(${NAME} PRIVATE
			-fno-rtti
			-fno-exceptions
			-gsplit-dwarf
		)
		target_link_options(${NAME} PRIVATE
			-sASYNCIFY=1
			-sALLOW_MEMORY_GROWTH=1
			-gseparate-dwarf
		)
	endif ()
endfunction ()

# compile_<type>_shader(INPUT VAR_NAME OUTPUT [INCLUDE_DIRS <dir>...] [DEFINES <name>[=<value>]...] [DEPENDS <file>...])
#
# Compiles with bgame-shaderc (tools/shaderc.c) into a C header.
#
# BGAME_SHADER_STAGE is always defined, as a plain number so it means the same with or without
# any header. include/glsl is always on the search path: `#include "bgame.glsl"` for the
# BGAME_SHADER_STAGE_* names of those numbers, along with Varying().
#
# INCLUDE_DIRS: search path for `#include "file"`, ahead of bgame's and CF's builtin includes.
# DEFINES: more preprocessor macros.
# DEPENDS: only for an include that is itself generated. Everything a shader includes,
# transitively, is tracked through the depfile the compiler writes.
function (bgame_compile_shader TYPE INPUT VAR_NAME OUTPUT)
	cmake_parse_arguments(PARSE_ARGV 4 ARG "" "" "INCLUDE_DIRS;DEFINES;DEPENDS")

	set(INCLUDE_FLAGS "")
	foreach (DIR IN LISTS ARG_INCLUDE_DIRS)
		list(APPEND INCLUDE_FLAGS "-I${DIR}")
	endforeach ()
	list(APPEND INCLUDE_FLAGS "-I${CMAKE_CURRENT_FUNCTION_LIST_DIR}/include/glsl")

	# Keep in sync with BGAME_SHADER_STAGE_* in include/glsl/bgame.glsl.
	if (TYPE STREQUAL "vertex")
		set(STAGE 0)
	elseif (TYPE STREQUAL "fragment")
		set(STAGE 1)
	elseif (TYPE STREQUAL "compute")
		set(STAGE 2)
	elseif (TYPE STREQUAL "draw")
		set(STAGE 3)
	else ()
		message(FATAL_ERROR "Unknown shader type: ${TYPE}")
	endif ()
	set(DEFINE_FLAGS "-DBGAME_SHADER_STAGE=${STAGE}")
	foreach (DEFINE IN LISTS ARG_DEFINES)
		list(APPEND DEFINE_FLAGS "-D${DEFINE}")
	endforeach ()

	set(DEPFILE "${CMAKE_CURRENT_BINARY_DIR}/${VAR_NAME}.d")

	add_custom_command(
		OUTPUT ${OUTPUT}
		COMMAND bgame-shaderc
			--type=${TYPE}
			--varname=${VAR_NAME}
			--header=${OUTPUT}
			--depfile=${DEPFILE}
			${INCLUDE_FLAGS}
			${DEFINE_FLAGS}
			${INPUT}
		DEPENDS ${INPUT} ${ARG_DEPENDS} bgame-shaderc
		DEPFILE ${DEPFILE}
	)
endfunction ()

function (compile_draw_shader INPUT VAR_NAME OUTPUT)
	bgame_compile_shader(draw ${INPUT} ${VAR_NAME} ${OUTPUT} ${ARGN})
endfunction ()

function (compile_vertex_shader INPUT VAR_NAME OUTPUT)
	bgame_compile_shader(vertex ${INPUT} ${VAR_NAME} ${OUTPUT} ${ARGN})
endfunction ()

function (compile_fragment_shader INPUT VAR_NAME OUTPUT)
	bgame_compile_shader(fragment ${INPUT} ${VAR_NAME} ${OUTPUT} ${ARGN})
endfunction ()

add_subdirectory(${CMAKE_CURRENT_LIST_DIR})

# CF's builtin shader includes (smooth_uv.shd and friends) only exist inside bgame-shaderc.
# Editor tooling resolves #include on disk, so every build keeps a copy: add the directory to
# the include path of a language server or of glslangValidator.
#
# Empty means .build/cf-builtins under the project root, next to the build directories the
# cmd scripts create: one path for the editor whatever the platform or build type. Every
# configuration dumps the same files there, and the tool only rewrites one whose content
# changed.
set(BGAME_SHADER_BUILTINS_DIR "" CACHE PATH
	"Where CF's builtin shader includes are dumped for editor tooling. Empty: .build/cf-builtins under the project root")
if (BGAME_SHADER_BUILTINS_DIR)
	set(BGAME_SHADER_BUILTINS_OUT "${BGAME_SHADER_BUILTINS_DIR}")
else ()
	# Spelled with CMAKE_SOURCE_DIR: prelude.cmake points CMAKE_BINARY_DIR at the source tree.
	set(BGAME_SHADER_BUILTINS_OUT "${CMAKE_SOURCE_DIR}/.build/cf-builtins")
endif ()

# The stamp is the output rather than the files: their mtime only moves when CF changes them,
# so a Makefile generator would find them older than a rebuilt tool on every build. It stays
# in this configuration's build directory while the files are shared, or one configuration's
# dump would mark it done for all the others. The tool is the only dependency since the
# builtins are compiled into it.
set(BGAME_SHADER_BUILTINS_STAMP "${CMAKE_CURRENT_BINARY_DIR}/cf-builtins.stamp")
add_custom_command(
	OUTPUT "${BGAME_SHADER_BUILTINS_STAMP}"
	COMMAND ${CMAKE_COMMAND} -E make_directory "${BGAME_SHADER_BUILTINS_OUT}"
	COMMAND bgame-shaderc --dump-builtins "${BGAME_SHADER_BUILTINS_OUT}"
	COMMAND ${CMAKE_COMMAND} -E touch "${BGAME_SHADER_BUILTINS_STAMP}"
	DEPENDS bgame-shaderc
	COMMENT "Dumping CF's builtin shader includes to ${BGAME_SHADER_BUILTINS_OUT}"
)
add_custom_target(bgame-shader-builtins ALL
	DEPENDS "${BGAME_SHADER_BUILTINS_STAMP}"
)

if (LINUX OR EMSCRIPTEN)
	if (RELOADABLE)
		set(PHYSFS_TARGET physfs)
	else ()
		set(PHYSFS_TARGET physfs-static)
	endif ()

	set_target_properties("${PHYSFS_TARGET}" PROPERTIES
		C_STANDARD 99
		C_STANDARD_REQUIRED ON
		C_EXTENSIONS OFF
	)
	target_compile_definitions("${PHYSFS_TARGET}" PRIVATE _POSIX_C_SOURCE=200112L)
endif ()

if (EMSCRIPTEN)
	target_compile_definitions("${PHYSFS_TARGET}" PRIVATE PHYSFS_PLATFORM_LINUX=1)

	add_custom_target(copy-emscripten-shell
		COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_CURRENT_LIST_DIR}/emscripten ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/emscripten
	)
	add_library(bgame-emscripten-shell INTERFACE)
	add_dependencies(bgame-emscripten-shell copy-emscripten-shell)
	target_link_options(bgame-emscripten-shell INTERFACE --shell-file "${CMAKE_CURRENT_LIST_DIR}/emscripten/shell.html")
elseif (LINUX)
	get_target_property(S2N_OPTS s2n COMPILE_OPTIONS)
	list(REMOVE_ITEM S2N_OPTS "-std=gnu99")
	set_target_properties(s2n PROPERTIES COMPILE_OPTIONS "${S2N_OPTS}")
endif ()
