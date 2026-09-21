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

# compile_<type>_shader(INPUT VAR_NAME OUTPUT [INCLUDE_DIRS <dir>...] [DEPENDS <file>...])
#
# INCLUDE_DIRS: search path for `#include "file"`, on top of CF's builtin includes.
# DEPENDS: the included files. cute-shaderc writes no depfile, so list them by hand or an
# edit to one does not rebuild the shader.
function (bgame_compile_shader TYPE INPUT VAR_NAME OUTPUT)
	cmake_parse_arguments(PARSE_ARGV 4 ARG "" "" "INCLUDE_DIRS;DEPENDS")

	set(INCLUDE_FLAGS "")
	foreach (DIR IN LISTS ARG_INCLUDE_DIRS)
		list(APPEND INCLUDE_FLAGS "-I${DIR}")
	endforeach ()

	add_custom_command(
		OUTPUT ${OUTPUT}
		COMMAND cute-shaderc
			-type=${TYPE}
			-varname=${VAR_NAME}
			-oheader=${OUTPUT}
			${INCLUDE_FLAGS}
			${INPUT}
		DEPENDS ${INPUT} ${ARG_DEPENDS} cute-shaderc
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

if (NOT TARGET cute-shaderc)
	message(FATAL_ERROR "bgame needs the cute-shaderc target from cute_framework: add_subdirectory(cute_framework) and keep CF_CUTE_SHADERC on")
endif ()
# It is a build tool, not part of the app, so keep it out of the shared
# runtime output directory that prelude.cmake points at bin/.
set_target_properties(cute-shaderc PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tools"
)

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
