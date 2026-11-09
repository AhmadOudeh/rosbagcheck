# dev:     compilers, libraries and pytest; used by scripts/dev.sh and CI
# build:   compiles rosbagcheck
# runtime: only the program, runs as a normal user
FROM ubuntu:24.04 AS dev
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake pkg-config ca-certificates \
        libssl-dev libzstd-dev liblz4-dev nlohmann-json3-dev libcli11-dev libgtest-dev \
        python3 python3-venv \
    && rm -rf /var/lib/apt/lists/*
COPY requirements-dev.txt /tmp/
RUN python3 -m venv /opt/venv && /opt/venv/bin/pip install --no-cache-dir -r /tmp/requirements-dev.txt
ENV PATH=/opt/venv/bin:$PATH
WORKDIR /src

FROM dev AS build
COPY . /src
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --target rosbagcheck

FROM ubuntu:24.04 AS runtime
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends libssl3t64 libzstd1 liblz4-1 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home checker
COPY --from=build /src/build/rosbagcheck /usr/local/bin/rosbagcheck
USER checker
WORKDIR /data
ENTRYPOINT ["rosbagcheck"]
