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
