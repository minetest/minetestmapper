ARG DOCKER_IMAGE=alpine:3.19
FROM $DOCKER_IMAGE AS builder

RUN apk add --no-cache build-base cmake \
		gd-dev sqlite-dev postgresql-dev hiredis-dev leveldb-dev \
		ninja

COPY . /usr/src/minetestmapper
WORKDIR /usr/src/minetestmapper

RUN cmake -B build -G Ninja && \
    cmake --build build --parallel $(nproc) && \
    cmake --install build

RUN ls /usr/local/share/minetest

FROM $DOCKER_IMAGE AS runtime

RUN apk add --no-cache libstdc++ libgcc libpq \
        gd sqlite-libs postgresql hiredis leveldb

COPY --from=builder /usr/local/share/minetest /usr/local/share/minetest
COPY --from=builder /usr/local/bin/minetestmapper /usr/local/bin/minetestmapper
COPY COPYING /usr/local/share/minetest/minetestmapper.COPYING

ENTRYPOINT ["/usr/local/bin/minetestmapper"]
