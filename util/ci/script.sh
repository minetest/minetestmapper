#!/bin/bash -e

install_linux_deps() {
	local pkgs=(cmake libgd-dev libsqlite3-dev libleveldb-dev libpq-dev libhiredis-dev libzstd-dev)

	sudo apt-get update
	sudo apt-get remove -y 'libgd3' nginx || : # ????
	sudo apt-get install -y --no-install-recommends "${pkgs[@]}" "$@"
}

run_build() {
	cmake . -DCMAKE_BUILD_TYPE=Debug \
		-DENABLE_LEVELDB=1 -DENABLE_POSTGRESQL=1 -DENABLE_REDIS=1

	make -j2
}

do_functional_test() {
	mkdir testmap
	echo "backend = sqlite3" >testmap/world.mt
	sqlite3 testmap/map.sqlite <<END
CREATE TABLE blocks(pos INT,data BLOB);
INSERT INTO blocks(pos, data) VALUES(0, x'$(cat util/ci/test_block)');
END

	./minetestmapper --noemptyimage -i ./testmap -o map.png
	file map.png
}
