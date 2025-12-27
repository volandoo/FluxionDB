# Builder stage compiles the Qt server and stages supporting tools.
FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    qt6-base-dev \
    libqt6websockets6-dev \
    sqlite3 \
    curl \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# Bring in the full source tree for compilation only.
COPY . .

RUN mkdir -p build && cd build \
    && qmake6 --version \
    && qmake6 ../fluxiondb.pro CONFIG+=release \
    && make -j"$(nproc)"

# Download the latest released FluxionDB CLI into a temp dir we can copy from later.
RUN chmod +x cli/install.sh \
    && INSTALL_DIR=/tmp/fluxiondb-cli ./cli/install.sh


# Runtime stage keeps only the binaries we need at runtime.
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    libqt6core6 \
    libqt6network6 \
    libqt6websockets6 \
    libqt6sql6 \
    libqt6sql6-sqlite \
    libsqlite3-0 \
    libreadline8 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy in only the server, sqlite3 CLI, and FluxionDB CLI artifacts.
COPY --from=builder /src/build/fluxiondb /usr/local/bin/_database
COPY --from=builder /usr/bin/sqlite3 /usr/local/bin/sqlite3
COPY --from=builder /tmp/fluxiondb-cli/fluxiondb /usr/local/bin/fluxiondb-cli.bin

EXPOSE 8080

# Run the server binary by default; pass runtime args like --secret-key via `docker run`.
ENTRYPOINT [ "/usr/local/bin/_database" ]
