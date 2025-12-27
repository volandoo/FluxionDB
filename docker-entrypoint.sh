#!/bin/bash
set -euo pipefail

secret_key=""
args=()

while (($#)); do
    case "$1" in
        --secret-key=*)
            secret_key="${1#*=}"
            args+=("$1")
            ;;
        --secret-key)
            if (($# < 2)); then
                echo "error: --secret-key requires a value" >&2
                exit 1
            fi
            secret_key="$2"
            args+=("$1" "$2")
            shift
            ;;
        *)
            args+=("$1")
            ;;
    esac
    shift
done

if [[ -n "$secret_key" ]]; then
    export FLUXIONDB_APIKEY="$secret_key"
fi

export FLUXIONDB_URL="ws://localhost:8080"

exec /usr/local/bin/_database "${args[@]}"
