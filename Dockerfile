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

# Wrap the CLI binary so env defaults are applied automatically.
RUN cat <<'EOF' >/usr/local/bin/fluxiondb
#!/usr/bin/env bash
set -euo pipefail
CLI_BIN="/usr/local/bin/fluxiondb-cli.bin"
secret_file="/run/fluxiondb-apikey"

if [[ -z "${FLUXIONDB_URL:-}" ]]; then
    export FLUXIONDB_URL="ws://localhost:8080"
fi

if [[ -z "${FLUXIONDB_APIKEY:-}" && -f "${secret_file}" ]]; then
    FLUXIONDB_APIKEY="$(cat "${secret_file}" 2>/dev/null)"
    export FLUXIONDB_APIKEY
fi

exec "${CLI_BIN}" "$@"
EOF
RUN chmod +x /usr/local/bin/fluxiondb
RUN ln -sf /usr/local/bin/fluxiondb /usr/local/bin/fluxiondb-cli

# Provide a default CLI URL; API keys are captured from --secret-key at runtime.
ENV FLUXIONDB_URL=ws://localhost:8080

# Entry point wrapper detects --secret-key and shares it with CLI sessions.
RUN cat <<'EOF' >/usr/local/bin/start-fluxiondb.sh
#!/usr/bin/env bash
set -euo pipefail

state_file="/run/fluxiondb-apikey"
secret="${FLUXIONDB_APIKEY:-}"
args=("$@")

mkdir -p "$(dirname "${state_file}")"

i=0
while (( i < ${#args[@]} )); do
    arg="${args[$i]}"
    case "$arg" in
        --secret-key)
            if (( i + 1 >= ${#args[@]} )); then
                echo "Error: --secret-key requires a value" >&2
                exit 1
            fi
            secret="${args[$((i + 1))]}"
            ((i += 2))
            continue
            ;;
        --secret-key=*)
            secret="${arg#*=}"
            ((i++))
            continue
            ;;
        *)
            ((i++))
            ;;
    esac
done

if [[ -z "${secret}" ]]; then
    echo "Error: secret key not provided. Pass --secret-key <value>." >&2
    exit 1
fi

printf '%s' "${secret}" > "${state_file}"
chmod 600 "${state_file}"

export FLUXIONDB_APIKEY="${secret}"

exec /usr/local/bin/_database "${args[@]}"
EOF
RUN chmod +x /usr/local/bin/start-fluxiondb.sh

# Show a helpful note whenever someone opens an interactive shell.
RUN cat <<'EOF' >/etc/profile.d/fluxiondb-cli.sh
#!/bin/sh
case "$-" in
    *i*) ;;
    *) return 0 ;;
esac
[ -n "$FLUXIONDB_CLI_HINT_SHOWN" ] && return 0

secret_file="/run/fluxiondb-apikey"
if [ -z "$FLUXIONDB_APIKEY" ] && [ -f "$secret_file" ]; then
    FLUXIONDB_APIKEY="$(cat "$secret_file" 2>/dev/null)"
    export FLUXIONDB_APIKEY
fi

export FLUXIONDB_CLI_HINT_SHOWN=1
cat <<EOM
FluxionDB container tips:
  - The CLI binary is available as: fluxiondb
  - FLUXIONDB_URL=${FLUXIONDB_URL:-ws://localhost:8080}
  - FLUXIONDB_APIKEY=${FLUXIONDB_APIKEY:-<not set>}
  - Example: fluxiondb list-keys
  - sqlite3 is installed for direct inspection of any mounted .db files
EOM
EOF
RUN chmod +x /etc/profile.d/fluxiondb-cli.sh
RUN printf '\n. /etc/profile.d/fluxiondb-cli.sh\n' >> /etc/bash.bashrc

EXPOSE 8080

# Run the server binary by default; pass runtime args like --secret-key via `docker run`.
ENTRYPOINT [ "/usr/local/bin/start-fluxiondb.sh" ]
