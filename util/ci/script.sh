#!/bin/bash -e

install_linux_deps() {
	local pkgs=(cmake libgd-dev libsqlite3-dev libleveldb-dev libpq-dev libhiredis-dev)

	sudo apt-get update
	sudo apt-get install -y --no-install-recommends ${pkgs[@]} "$@"
}

run_build() {
	cmake . -DCMAKE_BUILD_TYPE=Debug \
		-DENABLE_LEVELDB=1 -DENABLE_POSTGRESQL=1 -DENABLE_REDIS=1

	make -j2
}
