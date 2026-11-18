BUILD_IMAGE="localhost/bgame-appimage:$(sha256sum "${CMD_DIR}/Containerfile" | cut -c1-12)"

appimagesdk_image() {
	if ! podman image exists "${BUILD_IMAGE}"
	then
		echo "Building ${BUILD_IMAGE}"
		podman build -t "${BUILD_IMAGE}" -f "${CMD_DIR}/Containerfile" "${CMD_DIR}"
	fi
}

appimagesdk() {
	if [ "${BGAME_SDK}" = "appimage" ]
	then
		"$@"
	else
		appimagesdk_image
		mkdir -p ~/.cache/ccache
		podman run \
			--rm \
			-v ~/.cache/ccache:/root/.cache/ccache \
			-e CCACHE_DIR=/root/.cache/ccache \
			-v "$(pwd)":/workspace \
			-w /workspace \
			"${BUILD_IMAGE}" \
			"$@"
	fi
}
