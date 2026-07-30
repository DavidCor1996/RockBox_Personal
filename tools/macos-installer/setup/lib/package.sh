# Verification of the embedded Personal Rockbox release payload.
#
# This is an explicitly versioned offline installer, so the artifacts ship
# inside the application bundle instead of being downloaded. The integrity
# controls the specification requires for downloads still apply: every payload
# file is hashed, the package target is checked against the detected device, and
# the archive is rejected if it contains unsafe entries.

sha256_of()
{
    ${RP_BIN}/shasum -a 256 "$1" 2>/dev/null | awk '{ print $1 }'
}

# Confirm every payload file matches the SHA256SUMS written at build time.
verify_payload_hashes()
{
    local line
    local want
    local name
    local got

    stage "verified"

    if [ ! -f "${RP_PAYLOAD}/SHA256SUMS" ]; then
        fail "RP-HASH-MISMATCH" \
            "The installer payload is missing its checksum manifest." \
            "Download RockPod Setup again."
    fi

    while IFS= read -r line; do
        [ -n "${line}" ] || continue
        want="$(echo "${line}" | awk '{ print $1 }')"
        name="$(echo "${line}" | awk '{ print $2 }')"
        name="${name#\*}"
        got="$(sha256_of "${RP_PAYLOAD}/${name}")"
        if [ "${got}" != "${want}" ]; then
            fail "RP-HASH-MISMATCH" \
                "The bundled file ${name} failed verification." \
                "Download RockPod Setup again. Do not eject the iPod if an install was already running."
        fi
        log "payload ok: ${name} ${want}"
    done <"${RP_PAYLOAD}/SHA256SUMS"
}

# Reject a package built for a different Rockbox target.
verify_package_target()
{
    local target

    target="$(awk -F': *' '/^Target:/ { print $2; exit }' "${RP_PAYLOAD}/rockbox-info.txt" 2>/dev/null)"
    if [ "${target}" != "${RP_TARGET}" ]; then
        fail "RP-PKG-TARGET" \
            "The bundled firmware reports target '${target:-unknown}' but this installer is for ${RP_TARGET}." \
            "Download the RockPod Setup build that matches your iPod."
    fi
}

# Refuse an archive that could escape the mount point or replace a file with a
# link or special file.
verify_package_entries()
{
    local names="${RP_WORK}/zip-names.txt"
    local kinds="${RP_WORK}/zip-kinds.txt"

    if ! ${RP_BIN}/unzip -tqq "${RP_PAYLOAD}/rockbox.zip" >/dev/null 2>&1; then
        fail "RP-HASH-MISMATCH" \
            "The bundled Rockbox package is damaged." \
            "Download RockPod Setup again."
    fi

    ${RP_BIN}/unzip -Z1 "${RP_PAYLOAD}/rockbox.zip" >"${names}" 2>/dev/null

    if grep -q '^/' "${names}"; then
        fail "RP-PKG-UNSAFE-ZIP" \
            "The bundled package contains an absolute path." \
            "Download RockPod Setup again."
    fi
    if grep -q '\(^\|/\)\.\.\(/\|$\)' "${names}"; then
        fail "RP-PKG-UNSAFE-ZIP" \
            "The bundled package contains a parent-directory path." \
            "Download RockPod Setup again."
    fi
    if grep -q '\\\\' "${names}"; then
        fail "RP-PKG-UNSAFE-ZIP" \
            "The bundled package contains a backslash path." \
            "Download RockPod Setup again."
    fi

    # Symlinks and special files never appear in a valid Rockbox package.
    ${RP_BIN}/unzip -Z "${RP_PAYLOAD}/rockbox.zip" 2>/dev/null \
        | awk 'NR > 1 { print substr($1, 1, 1) }' >"${kinds}"
    if grep -q '^[lbcps]' "${kinds}"; then
        fail "RP-PKG-UNSAFE-ZIP" \
            "The bundled package contains a link or special file." \
            "Download RockPod Setup again."
    fi

    # The iPod's FAT32 volume is case-insensitive; colliding names would make
    # the installed result depend on extraction order.
    if tr 'A-Z' 'a-z' <"${names}" | sort | uniq -d | grep -q .; then
        fail "RP-PKG-UNSAFE-ZIP" \
            "The bundled package contains names that collide on a FAT32 volume." \
            "Download RockPod Setup again."
    fi

    log "package entries validated: $(wc -l <"${names}" | tr -d ' ') entries"
}
