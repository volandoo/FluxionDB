#!/usr/bin/env bash
set -euo pipefail

REPO="${REPO:-volandoo/FluxionDB}"
TAG_PREFIX="${TAG_PREFIX:-cli-v}"
CLI_VERSION="${CLI_VERSION:-0.5.0}"
BINARY_BASE_NAME="fluxiondb"
TAG_NAME="${TAG_PREFIX}${CLI_VERSION}"
DOWNLOAD_BASE="https://github.com/${REPO}/releases/download/${TAG_NAME}"
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

    local asset_base="${BINARY_BASE_NAME}-${PLATFORM}-${ARCH}"
    local asset_name="${asset_base}.${ARCHIVE_EXT}"
    local download_url="${DOWNLOAD_BASE}/${asset_name}"

    WORKDIR="$(mktemp -d)"
    local archive_path="${WORKDIR}/${asset_name}"
    echo "Downloading ${asset_name}..."
    curl -fL -o "${archive_path}" "${download_url}"

    if [[ "${ARCHIVE_EXT}" == "tar.gz" ]]; then
        tar -xzf "${archive_path}" -C "${WORKDIR}"
    else
        unzip -q "${archive_path}" -d "${WORKDIR}"
    fi

    local extracted=""
    for candidate in \
        "${WORKDIR}/${TARGET_NAME}" \
        "${WORKDIR}/${BINARY_BASE_NAME}" \
        "${WORKDIR}/${BINARY_BASE_NAME}.exe" \
        "${WORKDIR}/${asset_base}" \
        "${WORKDIR}/${asset_base}.exe"
    do
        if [[ -f "${candidate}" ]]; then
            extracted="${candidate}"
            break
        fi
    done
    if [[ -z "${extracted}" ]] && command -v find >/dev/null 2>&1; then
        extracted="$(find "${WORKDIR}" -maxdepth 3 -type f \( \
            -name "${TARGET_NAME}" -o \
            -name "${BINARY_BASE_NAME}" -o \
            -name "${BINARY_BASE_NAME}.exe" -o \
            -name "${asset_base}" -o \
            -name "${asset_base}.exe" \
        \) | head -n 1 || true)"
    fi
    [[ -n "${extracted}" ]] || error "downloaded archive did not include ${TARGET_NAME}"
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
