set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
add_compile_options("-fno-omit-frame-pointer")
add_link_options(
	"-fuse-ld=mold"
	"-Wl,--build-id"
	"-Wl,--separate-debug-file"
	"-Wl,--no-detach"
	"-Wl,--as-needed"
)

macro(cmake_minimum_required)
endmacro()
