set(SOURCES
	"src/libs.c"
	"src/log.c"
	"src/allocator.c"
	"src/allocator/tracked.c"
	"src/allocator/frame.c"
	"src/allocator/cute_framework.c"
	"src/entrypoint.c"
	"src/scene.c"
	"src/asset.c"
	"src/asset/9patch.c"
	"src/asset/sprite.c"
	"src/asset/audio.c"
	"src/asset/binary.c"
	"src/asset/font.c"
	"src/asset/shader.c"
	"src/asset/collision_shape.c"
	"src/dj.c"
	"src/ui.c"
	"src/ui/9patch.c"
	"src/ui/rich_text.c"
	"src/internal.c"
)

add_library(bgame STATIC "${SOURCES}")
target_compile_definitions(bgame PUBLIC BGAME_RELOADABLE=$<IF:$<BOOL:${RELOADABLE}>,1,0>)
target_include_directories(bgame PUBLIC include)
target_link_libraries(bgame PUBLIC
	cute
	$<$<BOOL:${RELOADABLE}>:remodule>
	blibs
)

add_library(bgame-loader STATIC "src/loader.c")
target_compile_definitions(bgame-loader PUBLIC BGAME_RELOADABLE=$<IF:$<BOOL:${RELOADABLE}>,1,0>)
target_include_directories(bgame-loader PRIVATE include)
target_link_libraries(bgame-loader PUBLIC cute $<$<BOOL:${RELOADABLE}>:remodule> blibs)

add_library(bgame-loader-stub STATIC "src/loader_stub.c")
