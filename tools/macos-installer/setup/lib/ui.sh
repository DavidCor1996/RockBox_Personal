# Native macOS dialogs for RockPod Setup.
#
# The specification's Qt visual shell cannot be produced from this repository's
# Linux build host, so the safety-critical flow uses AppleScript dialogs, which
# are present on every supported macOS release, are keyboard navigable, and are
# read by VoiceOver without extra work.

RP_UI_TITLE="RockPod Setup"

# Escape a string for embedding in an AppleScript string literal.
as_quote()
{
    printf '%s' "$1" | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g'
}

osa()
{
    ${RP_BIN}/osascript -e "$1"
}

# ui_notify <message>
# Non-blocking stage feedback while a long operation runs.
ui_notify()
{
    osa "display notification \"$(as_quote "$1")\" with title \"${RP_UI_TITLE}\"" \
        >/dev/null 2>&1 || true
}

# ui_info <heading> <body>
ui_info()
{
    osa "display dialog \"$(as_quote "$1")

$(as_quote "$2")\" with title \"${RP_UI_TITLE}\" buttons {\"OK\"} default button \"OK\"" \
        >/dev/null 2>&1 || true
}

# ui_confirm <heading> <body> <confirm-label>
# Returns 0 when the user chooses the confirm button, 1 when they cancel.
ui_confirm()
{
    local answer

    answer="$(osa "button returned of (display dialog \"$(as_quote "$1")

$(as_quote "$2")\" with title \"${RP_UI_TITLE}\" buttons {\"Cancel\", \"$(as_quote "$3")\"} default button \"$(as_quote "$3")\" cancel button \"Cancel\")" 2>/dev/null)"

    [ "${answer}" = "$3" ]
}

# ui_failure <code> <what> <next action>
# A hard stop never offers "Continue anyway".
ui_failure()
{
    local code="$1"
    local what="$2"
    local next="$3"
    local body
    local answer

    body="What Setup was doing: ${RP_STAGE}
Was the iPod changed: ${RP_DEVICE_CHANGED}
Last verified safe state: ${RP_LAST_SAFE_STATE}
What to do next: ${next}
Error code: ${code}"

    answer="$(osa "button returned of (display dialog \"$(as_quote "${what}")

$(as_quote "${body}")\" with title \"${RP_UI_TITLE}\" buttons {\"Save Diagnostic Report\", \"Quit\"} default button \"Quit\" with icon stop)" 2>/dev/null)"

    if [ "${answer}" = "Save Diagnostic Report" ]; then
        save_diagnostic_report "${code}"
    fi
}

# Copy the redacted log to the Desktop and reveal it.
save_diagnostic_report()
{
    local code="$1"
    local target="${HOME}/Desktop/RockPod-Setup-Report-${code}-$(date -u +%Y%m%dT%H%M%SZ).txt"

    # Redact the account name from displayed paths; never include media metadata.
    sed -e "s#${HOME}#~#g" -e "s#/Users/$(id -un)#~#g" \
        "${RP_LOG_FILE}" >"${target}" 2>/dev/null || return 0
    ${RP_BIN}/open -R "${target}" >/dev/null 2>&1 || true
}
