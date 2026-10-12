# Common project setup for bgame apps.
#
# include() this right after project() and before any add_subdirectory(), so
# every target, dependencies included, lands in the same output directory:
#
#   bin/<PLATFORM_NAME>/<CONFIG>-<reloadable|static>
#
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

# Nothing uses C++ modules; the scan only costs time and trips ninja on a reconfigure
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# --- cute_framework ---

# A reloadable app loads it as a shared library
if (RELOADABLE)
	set(CF_FRAMEWORK_STATIC OFF)
else ()
	set(CF_FRAMEWORK_STATIC ON)
endif ()
set(CF_FRAMEWORK_BUILD_SAMPLES OFF)
set(CF_FRAMEWORK_BUILD_TESTS OFF)

# Use SDL3 from the system when it is installed
set(BGAME_SYSTEM_SDL AUTO CACHE STRING "Link the installed SDL3: ON, OFF, or AUTO (when found)")
set_property(CACHE BGAME_SYSTEM_SDL PROPERTY STRINGS AUTO ON OFF)
# A cross build must not pick up the host's SDL (the msvc-wine toolchain finds it in /usr/lib)
if (EMSCRIPTEN OR (CMAKE_CROSSCOMPILING AND BGAME_SYSTEM_SDL STREQUAL "AUTO"))
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

# --- physfs ---

# Assets are directories or zip files
foreach (FORMAT 7Z GRP WAD CSM HOG MVL QPAK ROFS SLB ISO9660 VDF LECARCHIVES POD)
	set(PHYSFS_ARCHIVE_${FORMAT} FALSE CACHE BOOL "")
endforeach ()

# --- s2n ---

# -Werror on its own sources must not break the build with a newer compiler
set(UNSAFE_TREAT_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
# It prefers a static libcrypto, whose own dependencies (zlib, zstd) it never links
set(S2N_USE_CRYPTO_SHARED_LIBS ON CACHE BOOL "" FORCE)

# Where the dependencies come from when they have to be added
set(BGAME_CUTE_FRAMEWORK_URL https://github.com/bullno1/cute_framework.git CACHE STRING "")
set(BGAME_REMODULE_URL https://github.com/bullno1/remodule.git CACHE STRING "")
set(BGAME_BLIBS_URL https://github.com/bullno1/libs.git CACHE STRING "")
set(BGAME_CLAY_URL https://github.com/bullno1/clay.git CACHE STRING "")

# Adds the dependencies from DIR (the current directory by default), each a submodule named
# as its target, adding or checking out the ones that are not there. cute_framework is
# SYSTEM, so an app can compile with stricter warnings than it does.
function (bgame_add_dependencies)
	if (ARGC GREATER 0)
		set(DIR "${ARGV0}")
	else ()
		set(DIR "${CMAKE_CURRENT_SOURCE_DIR}")
	endif ()
	cmake_path(ABSOLUTE_PATH DIR BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" NORMALIZE)
	cmake_path(RELATIVE_PATH DIR BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" OUTPUT_VARIABLE REL)

	foreach (DEP cute_framework remodule blibs clay)
		file(GLOB CONTENT LIST_DIRECTORIES true "${DIR}/${DEP}/*")
		if (CONTENT)
			continue ()
		endif ()
		find_package(Git QUIET)
		set(PATH "${REL}/${DEP}")
		cmake_path(NORMAL_PATH PATH)
		if (NOT GIT_FOUND)
			message(FATAL_ERROR "${PATH} is missing and there is no git to get it")
		endif ()
		# Already a submodule (a clone without --recurse-submodules), or not yet
		execute_process(
			COMMAND "${GIT_EXECUTABLE}" ls-files --error-unmatch -- "${PATH}"
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
			RESULT_VARIABLE TRACKED
			OUTPUT_QUIET ERROR_QUIET
		)
		if (TRACKED EQUAL 0)
			message(STATUS "Checking out submodule ${PATH}")
			set(COMMAND submodule update --init --recursive -- "${PATH}")
		else ()
			string(TOUPPER "${DEP}" NAME)
			message(STATUS "Adding submodule ${PATH} from ${BGAME_${NAME}_URL}")
			set(COMMAND submodule add -- "${BGAME_${NAME}_URL}" "${PATH}")
		endif ()
		execute_process(
			COMMAND "${GIT_EXECUTABLE}" ${COMMAND}
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
			RESULT_VARIABLE RESULT
		)
		if (NOT RESULT EQUAL 0)
			message(FATAL_ERROR "git ${COMMAND} failed (${RESULT})")
		endif ()
	endforeach ()

	set(CF "${REL}/cute_framework")
	cmake_path(NORMAL_PATH CF)
	add_subdirectory("${DIR}/cute_framework" "${CMAKE_CURRENT_BINARY_DIR}/${CF}" SYSTEM EXCLUDE_FROM_ALL)

	foreach (LIB remodule blibs clay)
		add_library(${LIB} INTERFACE)
		target_include_directories(${LIB} INTERFACE "${DIR}/${LIB}")
	endforeach ()
endfunction ()
