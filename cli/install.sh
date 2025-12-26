#!/usr/bin/env bash
set -euo pipefail

REPO="${REPO:-volandoo/fluxiondb}"
TAG_PREFIX="${TAG_PREFIX:-cli-v}"
BINARY_BASE_NAME="fluxiondb"
API_URL="https://api.github.com/repos/${REPO}/releases?per_page=25"
WORKDIR=""

cleanup() {
    if [[ -n "${WORKDIR}" && -d "${WORKDIR}" ]]; then
        rm -rf "${WORKDIR}"
    fi
}
trap cleanup EXIT

error() {
    echo "FluxionDB CLI installer: $*" >&2
    exit 1
}

require_cmd() {
    command -v "$1" >/dev/null 2>&1 || error "missing required command: $1"
}

detect_platform() {
    local uname_out
    uname_out="$(uname -s 2>/dev/null || true)"
    case "${uname_out}" in
        Linux*) PLATFORM="linux"; ARCHIVE_EXT="tar.gz";;
        Darwin*) PLATFORM="darwin"; ARCHIVE_EXT="tar.gz";;
        MINGW*|MSYS*|CYGWIN*) PLATFORM="windows"; ARCHIVE_EXT="zip";;
        *) error "unsupported operating system: ${uname_out:-unknown}"
    esac
}

detect_arch() {
    local machine
    machine="$(uname -m 2>/dev/null || true)"
    case "${machine}" in
        x86_64|amd64) ARCH="amd64";;
        arm64|aarch64) ARCH="arm64";;
        *) error "unsupported cpu architecture: ${machine:-unknown}"
    esac
}

fetch_asset_url() {
    local asset_name="$1"
    curl -fsSL "${API_URL}" | node - "$TAG_PREFIX" "$asset_name" <<'NODE' || return 1
const fs = require("fs");

const prefix = process.argv[2];
const assetName = process.argv[3];
const releases = JSON.parse(fs.readFileSync(0, "utf8"));

for (const release of releases) {
    const tag = release.tag_name || "";
    if (!tag.startsWith(prefix)) continue;
    if (release.draft || release.prerelease) continue;
    for (const asset of release.assets || []) {
        if (asset.name === assetName && asset.browser_download_url) {
            process.stdout.write(asset.browser_download_url + "\n");
            process.exit(0);
        }
    }
}

process.stderr.write(
    `Could not find asset named ${assetName} under releases prefixed with ${prefix}\n`,
);
process.exit(1);
NODE
}

ensure_dir() {
    local dir="$1"
    if [ -d "${dir}" ]; then
        return
    fi
    if mkdir -p "${dir}" 2>/dev/null; then
        return
    fi
    if command -v sudo >/dev/null 2>&1; then
        sudo mkdir -p "${dir}"
    else
        error "need write access to create ${dir} (set INSTALL_DIR or rerun with sudo)"
    fi
}

copy_with_privilege() {
    if "$@"; then
        return
    fi
    if command -v sudo >/dev/null 2>&1; then
        sudo "$@"
        return
    fi
    error "need elevated permissions to run: $*"
}

main() {
    require_cmd curl
    require_cmd node
    if ! command -v mktemp >/dev/null 2>&1; then
        error "mktemp is required"
    fi

    detect_platform
    detect_arch

    if [[ "${ARCHIVE_EXT}" == "tar.gz" ]]; then
        require_cmd tar
    else
        require_cmd unzip
    fi

    local default_dir="/usr/local/bin"
    if [[ "${PLATFORM}" == "windows" ]]; then
        default_dir="${HOME}/.local/bin"
    fi
    INSTALL_DIR="${INSTALL_DIR:-$default_dir}"

    TARGET_NAME="${BINARY_BASE_NAME}"
    if [[ "${PLATFORM}" == "windows" ]]; then
        TARGET_NAME="${BINARY_BASE_NAME}.exe"
    fi

    local asset_name="fluxiondb-${PLATFORM}-${ARCH}.${ARCHIVE_EXT}"
    local download_url
    download_url="$(fetch_asset_url "$asset_name")" || error "failed to locate release asset"

    WORKDIR="$(mktemp -d)"
    local archive_path="${WORKDIR}/${asset_name}"
    echo "Downloading ${asset_name}..."
    curl -fL -o "${archive_path}" "${download_url}"

    if [[ "${ARCHIVE_EXT}" == "tar.gz" ]]; then
        tar -xzf "${archive_path}" -C "${WORKDIR}"
    else
        unzip -q "${archive_path}" -d "${WORKDIR}"
    fi

    local extracted="${WORKDIR}/${TARGET_NAME}"
    if [[ ! -f "${extracted}" ]]; then
        extracted="${WORKDIR}/${BINARY_BASE_NAME}"
        [[ -f "${extracted}" ]] || error "downloaded archive did not include ${TARGET_NAME}"
    fi
    chmod +x "${extracted}"

    ensure_dir "${INSTALL_DIR}"

    local target_path="${INSTALL_DIR}/${TARGET_NAME}"
    echo "Installing to ${target_path}..."
    copy_with_privilege cp "${extracted}" "${target_path}"
    copy_with_privilege chmod 755 "${target_path}"

    echo "FluxionDB CLI installed to ${target_path}"
    echo "Ensure ${INSTALL_DIR} is in your PATH."
}

main "$@"
