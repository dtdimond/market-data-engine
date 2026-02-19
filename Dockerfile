# syntax=docker/dockerfile:1

# --- Builder stage ---
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git ca-certificates \
    libssl-dev zlib1g-dev \
    lsb-release wget gnupg && \
    rm -rf /var/lib/apt/lists/*

# Apache Arrow apt repository
RUN wget -qO- https://packages.apache.org/arrow/$(lsb_release --id --short | tr 'A-Z' 'a-z')/apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb \
    -O /tmp/apache-arrow-apt-source.deb && \
    apt-get install -y /tmp/apache-arrow-apt-source.deb && \
    rm /tmp/apache-arrow-apt-source.deb && \
    apt-get update && apt-get install -y --no-install-recommends \
    libarrow-dev libparquet-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Copy CMakeLists.txt first for better layer caching (FetchContent downloads)
COPY CMakeLists.txt .
RUN cmake -B out -DCMAKE_BUILD_TYPE=Release 2>&1 || true

# Copy source and build
COPY src/ src/
COPY tests/ tests/
COPY tools/ tools/
COPY docs/ docs/
RUN cmake -B out -DCMAKE_BUILD_TYPE=Release && \
    cmake --build out --target market_data_engine -j$(nproc)

# --- Runtime stage ---
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    lsb-release wget gnupg ca-certificates && \
    wget -qO- https://packages.apache.org/arrow/$(lsb_release --id --short | tr 'A-Z' 'a-z')/apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb \
    -O /tmp/apache-arrow-apt-source.deb && \
    apt-get install -y /tmp/apache-arrow-apt-source.deb && \
    rm /tmp/apache-arrow-apt-source.deb && \
    apt-get update && apt-get install -y --no-install-recommends \
    libarrow1700 libparquet1700 \
    libssl3 libcurl4 && \
    apt-get purge -y lsb-release wget gnupg && \
    apt-get autoremove -y && \
    rm -rf /var/lib/apt/lists/*

RUN useradd -m -s /bin/bash mde
USER mde
WORKDIR /app

COPY --from=builder /build/out/market_data_engine .

ENTRYPOINT ["/app/market_data_engine"]
