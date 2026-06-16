cmake_minimum_required(VERSION 3.15)
project(bgame)

set(SOURCES
	"src/libs.c"
	"src/log.c"
	"src/allocator.c"
	"src/allocator/tracked.c"
	"src/allocator/frame.c"
	"src/allocator/cute_framework.c"
	"src/entrypoint.c"
	"src/scene.c"
	"src/shader.c"
	"src/asset.c"
	"src/asset/sprite.c"
	"src/asset/audio.c"
	"src/asset/binary.c"
	"src/asset/font.c"
	"src/asset/collision_shape.c"
	"src/asset/image.c"
	"src/dj.c"
	"src/collision.c"
	"src/ui.c"
	"src/ui/rich_text.c"
	"src/internal.c"
	"src/ui/gen/overlay_shd.h"
)

compile_shader(
	${CMAKE_CURRENT_SOURCE_DIR}/src/ui/overlay.glsl
	bgame_ui_overlay_shd_bytecode
	${CMAKE_CURRENT_SOURCE_DIR}/src/ui/gen/overlay_shd.h
)

add_library(bgame STATIC "${SOURCES}")
target_compile_definitions(bgame PUBLIC BGAME_RELOADABLE=$<IF:$<BOOL:${RELOADABLE}>,1,0>)
target_include_directories(bgame PUBLIC include)
target_link_libraries(bgame PUBLIC
	cute
	$<$<BOOL:${RELOADABLE}>:remodule>
	blibs
	clay
)
if(MSVC)
    target_compile_options(bgame PRIVATE "/experimental:c11atomics")
endif()

add_library(bgame-loader STATIC "src/loader.c")
target_compile_definitions(bgame-loader PUBLIC BGAME_RELOADABLE=$<IF:$<BOOL:${RELOADABLE}>,1,0>)
target_include_directories(bgame-loader PRIVATE include)
target_link_libraries(bgame-loader PUBLIC cute $<$<BOOL:${RELOADABLE}>:remodule> blibs)

add_library(bgame-loader-stub STATIC "src/loader_stub.c")
