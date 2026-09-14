# Sourced by the steamrt scripts: runs a command inside the Steam Runtime SDK,
# directly when already inside it, through podman otherwise.

BUILD_IMAGE=registry.gitlab.steamos.cloud/steamrt/steamrt4/sdk:latest

if [ -r /etc/os-release ]
then
	. /etc/os-release
elif [ -r /usr/lib/os-release ]
then
	. /usr/lib/os-release
fi

steamsdk() {
	if [ "${ID}" = "steamrt" ]
	then
		"$@"
	else
		podman run \
			--rm \
			-v ~/.cache/ccache:/root/.cache/ccache \
			-e CCACHE_DIR=/root/.cache/ccache \
			-v "$(pwd)":/workspace \
			-w /workspace \
			${BUILD_IMAGE} \
			"$@"
	fi
}
