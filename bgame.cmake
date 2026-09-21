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
