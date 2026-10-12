# Common project setup for bgame apps.
#
# include() this right after project() and before any add_subdirectory(), so
# every target, dependencies included, lands in the same output directory:
#
#   bin/<PLATFORM_NAME>/<CONFIG>-<reloadable|static>
#
# bgame.cmake is included separately, after the dependencies.
option(RELOADABLE "Is the program reloadable" ON)

if (NOT PLATFORM_NAME)
	string(TOLOWER "${CMAKE_SYSTEM_NAME}" PLATFORM_NAME)
endif ()

# Keep outputs next to the sources instead of inside the build directory
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR})
set(BGAME_OUTPUT_DIRECTORY
	"${CMAKE_BINARY_DIR}/bin/${PLATFORM_NAME}/$<CONFIG>-$<IF:$<BOOL:${RELOADABLE}>,reloadable,static>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BGAME_OUTPUT_DIRECTORY}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${BGAME_OUTPUT_DIRECTORY}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${BGAME_OUTPUT_DIRECTORY}")

# Use SDL3 from the system when it is installed
set(BGAME_SYSTEM_SDL AUTO CACHE STRING "Link the installed SDL3: ON, OFF, or AUTO (when found)")
set_property(CACHE BGAME_SYSTEM_SDL PROPERTY STRINGS AUTO ON OFF)
if (EMSCRIPTEN)
	set(CF_USE_SYSTEM_SDL OFF)
elseif (BGAME_SYSTEM_SDL STREQUAL "AUTO")
	find_package(SDL3 3.4.0...<4.0.0 CONFIG QUIET)
	if (SDL3_FOUND)
		set(CF_USE_SYSTEM_SDL ON)
	else ()
		set(CF_USE_SYSTEM_SDL OFF)
	endif ()
else ()
	set(CF_USE_SYSTEM_SDL ${BGAME_SYSTEM_SDL})
endif ()
