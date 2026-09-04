#!/usr/bin/env bash
set -euo pipefail

# Build the small, auditable x264 compatibility patch that reproduces the
# decoder-facing H.264 syntax measured from iTunes 9.2.1 / QuickTime 7.6.6.
# The binary is local tooling and is intentionally not checked into git.

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
commit="c24e06c2e184345ceb33eb20a15d1024d9fd3497"
archive_sha256="090d730e867fc63631782a1287974635d1237d0fa7c6fd1d09fd543620a56689"
archive_url="https://github.com/mirror/x264/archive/${commit}.tar.gz"
target_dir="${repo_root}/rockpod/.tools"
target="${target_dir}/x264-apple-ipod"
patch_file="${repo_root}/tools/patches/x264-apple-ipod-exact.patch"
build_root="$(mktemp -d -t rockpod-x264-apple.XXXXXXXX)"

cleanup() {
    rm -rf -- "${build_root}"
}
trap cleanup EXIT INT TERM

for tool in curl sha256sum tar patch make; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        echo "Missing required build tool: ${tool}" >&2
        exit 1
    fi
done

archive="${build_root}/x264.tar.gz"
source_dir="${build_root}/src"
curl --fail --location --silent --show-error "${archive_url}" --output "${archive}"
printf '%s  %s\n' "${archive_sha256}" "${archive}" | sha256sum --check --status
mkdir -p "${source_dir}"
tar -xzf "${archive}" -C "${source_dir}" --strip-components=1
patch --directory="${source_dir}" --strip=1 < "${patch_file}"

configure_args=(
    --enable-static
    --disable-opencl
)
assembler="${ROCKPOD_NASM:-}"
if [[ -z "${assembler}" && -x "${target_dir}/nasm" ]]; then
    assembler="${target_dir}/nasm"
elif [[ -z "${assembler}" ]] && command -v nasm >/dev/null 2>&1; then
    assembler="$(command -v nasm)"
elif [[ -z "${assembler}" ]] && command -v yasm >/dev/null 2>&1; then
    assembler="$(command -v yasm)"
fi
if [[ -z "${assembler}" ]]; then
    echo "Warning: NASM/Yasm not found; building a slower encoder without CPU assembly." >&2
    echo "Set ROCKPOD_NASM or install rockpod/.tools/nasm to retain conversion speed." >&2
    configure_args+=(--disable-asm)
fi

(
    cd "${source_dir}"
    if [[ -n "${assembler}" ]]; then
        AS="${assembler}" ./configure "${configure_args[@]}"
    else
        ./configure "${configure_args[@]}"
    fi
    make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '2')"
)

mkdir -p "${target_dir}"
install -m 0755 "${source_dir}/x264" "${target}"
"${target}" --version 2>&1 | grep -F "RockPod Apple-iPod exact bitstream patch v5" >/dev/null
echo "Built ${target}"
