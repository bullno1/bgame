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

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_EXTENSIONS OFF)
