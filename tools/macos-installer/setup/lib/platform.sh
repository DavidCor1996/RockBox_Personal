# Absolute paths to the macOS system tools RockPod Setup depends on.
#
# Absolute paths are used so a modified PATH cannot substitute a different
# diskutil or shasum during a privileged-looking operation. The two prefixes are
# overridable purely so tools/macos_installer_gate.sh can exercise this code on
# the Linux build host with stub tools; on a Mac they are never set.

: "${RP_BIN:=/usr/bin}"
: "${RP_SBIN:=/usr/sbin}"
