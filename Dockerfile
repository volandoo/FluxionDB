# Builder stage compiles the lean C++ server and stages supporting tools.
FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    zlib1g-dev \
    libsqlite3-dev \
    nlohmann-json3-dev \
    sqlite3 \
    curl \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN git clone --depth 1 https://github.com/uNetworking/uWebSockets.git /opt/uWebSockets \
    && git -C /opt/uWebSockets submodule update --init --depth 1 uSockets \
    && make -C /opt/uWebSockets/uSockets WITH_OPENSSL=0

WORKDIR /src

# Bring in the full source tree for compilation only.
COPY . .

RUN mkdir -p build && cd build \
    && cmake -DCMAKE_BUILD_TYPE=Release -DUWEBSOCKETS_ROOT=/opt/uWebSockets .. \
    && cmake --build . -j"$(nproc)"

# Download the latest released FluxionDB CLI into a temp dir we can copy from later.
RUN chmod +x cli/install.sh \
    && INSTALL_DIR=/tmp/fluxiondb-cli ./cli/install.sh


# Runtime stage keeps only the binaries we need at runtime.
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    libsqlite3-0 \
    zlib1g \
    libreadline8 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy in only the server, sqlite3 CLI, and FluxionDB CLI artifacts.
COPY --from=builder /src/build/fluxiondb /usr/local/bin/_database
COPY --from=builder /usr/bin/sqlite3 /usr/local/bin/sqlite3
COPY --from=builder /tmp/fluxiondb-cli/fluxiondb /usr/local/bin/fluxiondb
COPY docker-entrypoint.sh /usr/local/bin/fluxiondb-entrypoint

RUN chmod +x /usr/local/bin/fluxiondb-entrypoint

EXPOSE 8080

# Run the server through a small shim that can export FLUXIONDB_APIKEY from --secret-key.
ENTRYPOINT [ "/usr/local/bin/fluxiondb-entrypoint" ]
