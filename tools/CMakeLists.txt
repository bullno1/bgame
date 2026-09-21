# bgame-shaderc, the offline shader compiler: a C port of the cute-shaderc front end
# (shaderc.c) on top of the compiler in the cute_framework tree, which has no dependency
# beyond CF's own headers. It is a build tool, so it never links the cute library.
#
# The compiler is C++, and so is shaderc_builtins.cpp, which only exists to read CF's builtin
# shaders out of a header full of raw string literals.

get_target_property(CF_DIR cute SOURCE_DIR)

set(CF_COMPILER_SOURCES
	"${CF_DIR}/tools/cute_shader.cpp"
	# Standalone tools need a ckit implementation, cf_alloc / cf_free and the CF_ASSERT
	# handler; CF apps get all three from the cute library.
	"${CF_DIR}/tools/cute_shader_ckit.cpp"
	"${CF_DIR}/tools/cute_shader_assert.cpp"
	"${CF_DIR}/src/cute_alloc.cpp"
)

add_executable(bgame-shaderc
	"shaderc.c"
	"shaderc_builtins.cpp"
	${CF_COMPILER_SOURCES}
)
target_include_directories(bgame-shaderc PRIVATE "${CF_DIR}/tools")
target_include_directories(bgame-shaderc SYSTEM PRIVATE
	"${CF_DIR}/include"
	"${CF_DIR}/src"
	"${CF_DIR}/libraries"
)
target_link_libraries(bgame-shaderc PRIVATE blibs)
set_target_properties(bgame-shaderc PROPERTIES
	CXX_STANDARD 20
	CXX_STANDARD_REQUIRED ON
	# Keep it out of the shared runtime output directory that prelude.cmake points at bin/.
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

# CF's sources are not written for bgame's warning flags.
if (MSVC)
	set_source_files_properties(${CF_COMPILER_SOURCES} PROPERTIES COMPILE_OPTIONS "/w")
else ()
	set_source_files_properties(${CF_COMPILER_SOURCES} PROPERTIES COMPILE_OPTIONS "-w")
endif ()

if (EMSCRIPTEN)
	# The tool is a build step, not a web page: it runs through node (the Emscripten
	# toolchain's CMAKE_CROSSCOMPILING_EMULATOR, which CMake prepends when a custom command
	# invokes this target). NODERAWFS maps the host filesystem so the paths work as-is, and
	# EXIT_RUNTIME turns main's return into the process exit code.
	target_link_options(bgame-shaderc PRIVATE
		-sENVIRONMENT=node
		-sNODERAWFS=1
		-sEXIT_RUNTIME=1
		-sALLOW_MEMORY_GROWTH=1
	)
endif ()
