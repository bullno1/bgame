# Sourced by every script in this directory. The caller sets CMD_DIR to the
# directory of the running script, and the current directory must be the
# project root, which holds bgame.env:
#
#   APP=my-game                    # executable name, required
#   TESTS=my-game-tests            # test executable name, optional
#   WATCH_DIRS="src assets deps"   # directories the watch scripts monitor
#
# Overridable through the environment: BUILD_TYPE (RelWithDebInfo), RELOADABLE (ON).

if [ ! -f bgame.env ]
then
	echo "bgame.env not found: run from the project root" >&2
	exit 1
fi
. ./bgame.env

: "${APP:?bgame.env must set APP}"
TESTS=${TESTS:-}
WATCH_DIRS=${WATCH_DIRS:-src deps}

BUILD_TYPE=${BUILD_TYPE:-RelWithDebInfo}
RELOADABLE=${RELOADABLE:-ON}
if [ "${RELOADABLE}" = "ON" ]
then
	SUFFIX="reloadable"
else
	SUFFIX="static"
fi
BUILD_DIR="${BUILD_TYPE}-${SUFFIX}"

# The bgame checkout, as a path relative to the project root when it lives
# inside it, so it stays valid in a container that mounts the root elsewhere.
# cd -P resolves through a `cmd -> deps/bgame/cmd` symlink.
ROOT=$(pwd -P)
BGAME_DIR=$(cd -P "${CMD_DIR}/../.." && pwd -P)
BGAME_DIR=${BGAME_DIR#"${ROOT}"/}

require_tests() {
	if [ -z "${TESTS}" ]
	then
		echo "bgame.env does not set TESTS" >&2
		exit 1
	fi
}

# Discards pending events on stdin until none arrive for WATCH_SETTLE seconds,
# so a burst of writes (editor save, git checkout) triggers a single build.
# `read` consumes one line at a time, so nothing beyond the burst is lost.
WATCH_SETTLE=${WATCH_SETTLE:-0.3}
drain_events() {
	while timeout "${WATCH_SETTLE}" sh -c 'read -r _'
	do
		:
	done
}

# Builds, then runs the tests when the platform has a test script and
# bgame.env sets TESTS. Arguments are passed to the build script. Never fails,
# so a broken build or test keeps the watch loop alive.
build_and_test() {
	echo "Building"
	if ! "${CMD_DIR}/build" "$@"
	then
		echo "Build failed"
		return 0
	fi
	if [ -n "${TESTS}" ] && [ -x "${CMD_DIR}/test" ]
	then
		echo "Testing"
		"${CMD_DIR}/test" || echo "Tests failed"
	fi
	echo "Done"
}

# Builds once, then rebuilds whenever files under WATCH_DIRS change. Events
# are drained between builds so each burst results in one build. Only returns
# when inotifywait exits.
watch_and_build() {
	build_and_test "$@"
	inotifywait -r -m -q -e CLOSE_WRITE ${WATCH_DIRS} | while read -r _
	do
		drain_events
		clear
		build_and_test "$@"
	done
}
