option(RELOADABLE "Is the program reloadable" ON)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_EXTENSIONS OFF)

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
		target_link_libraries(${NAME} PRIVATE bgame bgame-loader bgame-loader-stub)
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

add_subdirectory(${CMAKE_CURRENT_LIST_DIR})

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

if (EMSCRIPTEN)
	target_compile_definitions("${PHYSFS_TARGET}" PRIVATE PHYSFS_PLATFORM_LINUX=1)

	add_custom_target(copy-emscripten-shell
		COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_CURRENT_LIST_DIR}/emscripten ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/emscripten
	)
	add_library(bgame-emscripten-shell INTERFACE)
	add_dependencies(bgame-emscripten-shell copy-emscripten-shell)
	target_link_options(bgame-emscripten-shell INTERFACE --shell-file "${CMAKE_CURRENT_LIST_DIR}/emscripten/shell.html")
else ()
	get_target_property(S2N_OPTS s2n COMPILE_OPTIONS)
	list(REMOVE_ITEM S2N_OPTS "-std=gnu99")
	set_target_properties(s2n PROPERTIES COMPILE_OPTIONS "${S2N_OPTS}")
endif ()
