# Logging and structured failure handling for RockPod Setup.
#
# Every failure carries a stable error code from the RockPod Easy Installer
# specification so support documentation can reference it directly.

RP_LOG_FILE=""
RP_STAGE="starting"
RP_DEVICE_CHANGED="no"
RP_LAST_SAFE_STATE="nothing written"

log_init()
{
    local log_dir="${HOME}/Library/Logs/RockPod"

    mkdir -p "${log_dir}" 2>/dev/null || true
    RP_LOG_FILE="${log_dir}/rockpod-setup-$(date -u +%Y%m%dT%H%M%SZ).log"
    : >"${RP_LOG_FILE}"
    log "RockPod Setup ${RP_SETUP_VERSION} (release ${RP_RELEASE_ID}, target ${RP_TARGET})"
    log "host: $(sw_vers -productName 2>/dev/null) $(sw_vers -productVersion 2>/dev/null) $(uname -m)"
}

log()
{
    local line="[$(date -u +%H:%M:%S)] $*"

    if [ -n "${RP_LOG_FILE}" ]; then
        echo "${line}" >>"${RP_LOG_FILE}"
    fi
    echo "${line}" >&2
}

# Record the stage the state machine has reached. The journal is the resume
# record described by the specification; it holds no secrets.
stage()
{
    RP_STAGE="$1"
    log "stage: ${RP_STAGE}"
    if [ -n "${RP_JOURNAL_FILE}" ]; then
        echo "$(date -u +%Y-%m-%dT%H:%M:%SZ) ${RP_RELEASE_ID} ${RP_STAGE}" \
            >>"${RP_JOURNAL_FILE}"
    fi
}

# Mark that the device has been modified, so a later failure reports honestly.
mark_device_changed()
{
    RP_DEVICE_CHANGED="yes"
}

mark_safe_state()
{
    RP_LAST_SAFE_STATE="$1"
}

# fail <error-code> <what happened> <next action>
fail()
{
    local code="$1"
    local what="$2"
    local next="$3"

    log "FAILED ${code}: ${what}"
    ui_failure "${code}" "${what}" "${next}"
    exit 1
}
