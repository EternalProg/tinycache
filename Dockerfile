FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    git \
    libboost-system-dev \
    libspdlog-dev \
    libxxhash-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY CMakeLists.txt config.toml ./
COPY include ./include
COPY src ./src

RUN cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_TESTS=OFF \
    -DENABLE_BENCHMARKS=OFF \
    && cmake --build build -j"$(nproc)"

FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libboost-system-dev \
    libfmt-dev \
    libxxhash0 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/build/tinycache ./tinycache
COPY --from=builder /app/config.toml ./config.toml

RUN useradd --create-home --shell /usr/sbin/nologin tinycache
USER tinycache

EXPOSE 8080

ENTRYPOINT ["./tinycache"]
