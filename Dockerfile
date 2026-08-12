# The target platform is selected by the build, not here: a constant
# FROM --platform= pins the base image without setting the platform of the
# resulting manifest, which leaves the image advertising the platform it was
# built on.
FROM ubuntu:24.04 AS builder

ARG DEBIAN_FRONTEND=noninteractive

# libpq-dev is needed (not just libpq5): the Makefile calls pg_config --includedir
RUN apt-get update && \
    apt-get install --yes --no-install-recommends \
        ca-certificates \
        build-essential \
        clang \
        make \
        pkg-config \
        libconfig-dev \
        libhiredis-dev \
        libjson-c-dev \
        libpq-dev \
        librdkafka-dev && \
    rm -rf /var/lib/apt/lists/*

ENV CC=clang
ENV CFLAGS=-O2

WORKDIR /opt/src
COPY . .

RUN make clean_release && make

FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

# runtime libraries only, mirroring the Depends: in debian/control
RUN apt-get update && \
    apt-get install --yes --no-install-recommends \
        ca-certificates \
        libconfig9 \
        libhiredis1.1.0 \
        libjson-c5 \
        libpq5 \
        librdkafka1 && \
    rm -rf /var/lib/apt/lists/*

# fixed uid/gid so mounted config/state volumes have predictable ownership
# (the deb intentionally uses a dynamic uid; containers need a known one)
RUN groupadd --system --gid 999 schaufel && \
    useradd --system --uid 999 --gid schaufel --no-create-home \
        --home-dir /var/lib/schaufel --shell /usr/sbin/nologin schaufel && \
    mkdir -p /etc/schaufel /var/lib/schaufel && \
    chown schaufel:schaufel /etc/schaufel /var/lib/schaufel

COPY --from=builder /opt/src/bin/schaufel /usr/local/bin/schaufel
COPY --from=builder /opt/src/doc/ /usr/share/doc/schaufel/

ENV CONFIG_FILE=/etc/schaufel/schaufel.conf

USER schaufel
WORKDIR /var/lib/schaufel

# Mount your config over CONFIG_FILE; see /usr/share/doc/schaufel/schaufel.conf
# for a reference. Set the logger type to "stdout" so output reaches the
# container log instead of a file.
CMD exec /usr/local/bin/schaufel -C "${CONFIG_FILE}"
