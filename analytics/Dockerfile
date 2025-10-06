FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential cmake git curl zip unzip tar \
    pkg-config ninja-build libpq-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt
RUN git clone https://github.com/Microsoft/vcpkg.git && \
    cd vcpkg && ./bootstrap-vcpkg.sh

ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH="${VCPKG_ROOT}:${PATH}"

WORKDIR /build
COPY CMakeLists.txt vcpkg.json ./
COPY src/ ./src/
COPY tests/ ./tests/

RUN cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
    -DBUILD_TESTS=OFF \
    -G Ninja && \
    cmake --build build --config Release

FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    ca-certificates curl libpq5 \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -m -u 1000 -s /bin/bash soulscout

COPY --from=builder /build/build/analytics /usr/local/bin/analytics
RUN chmod +x /usr/local/bin/analytics

USER soulscout
WORKDIR /home/soulscout

EXPOSE 8083

HEALTHCHECK --interval=30s --timeout=10s --start-period=20s --retries=3 \
    CMD curl -f http://localhost:8083/health || exit 1

CMD ["/usr/local/bin/analytics"]