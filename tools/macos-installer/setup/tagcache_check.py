"""Standalone Rockbox tagcache validator for RockPod Setup on macOS.

This is a stdlib-only reduction of rockpod/services/rockbox_tagcache.py. It
parses the master index and the filename tag file, then confirms every indexed
media path still resolves under the iPod mount point. It is used as the deep
half of the database guard; the shell installer performs the structural half so
the guard still runs on a Mac without any Python interpreter.

usage: tagcache_check.py <mount-path>
exit 0 with a summary on stdout, exit 1 with the reason on stderr.
"""

import os
import struct
import sys

TAGCACHE_MAGIC = 0x54434810
UNTAGGED = "<Untagged>"

TAG_FILENAME = 4
TAG_COUNT = 23
ROW_SIZE = (TAG_COUNT + 1) * 4
FLAG_DELETED = 0x0001

MASTER = "database_idx.tcd"
FILENAME_TAGS = "database_4.tcd"
COMMIT_MARKERS = ("database_commit.tcd", "database_hostcommit.tcd")


class GuardError(Exception):
    pass


def read_blob(path):
    try:
        with open(path, "rb") as handle:
            return handle.read()
    except OSError as exc:
        raise GuardError("%s is unreadable (%s)" % (os.path.basename(path), exc))


def decode_tag_entry(blob, offset, endian):
    if offset < 0 or offset + 8 > len(blob):
        raise GuardError("a filename offset points outside database_4.tcd")
    tag_length, _idx_id = struct.unpack_from("%sii" % endian, blob, offset)
    if tag_length <= 0:
        raise GuardError("a filename entry has an invalid length")
    start = offset + 8
    end = start + tag_length
    if end > len(blob):
        raise GuardError("a filename entry runs past the end of database_4.tcd")
    raw = blob[start:end].split(b"\0", 1)[0]
    value = raw.decode("utf-8", errors="replace").strip()
    return "" if value == UNTAGGED else value


def validate(mount):
    base = os.path.join(mount, ".rockbox")

    for marker in COMMIT_MARKERS:
        if os.path.exists(os.path.join(base, marker)):
            raise GuardError("a database transaction (%s) is incomplete" % marker)

    master = read_blob(os.path.join(base, MASTER))
    if len(master) < 24:
        raise GuardError("database_idx.tcd is truncated")

    if struct.unpack_from("<I", master, 0)[0] == TAGCACHE_MAGIC:
        endian = "<"
    elif struct.unpack_from(">I", master, 0)[0] == TAGCACHE_MAGIC:
        endian = ">"
    else:
        raise GuardError("database_idx.tcd has an unknown header")

    _magic, _datasize, entry_count, _serial, _commitid, dirty = struct.unpack_from(
        "%s6i" % endian, master, 0
    )
    if dirty:
        raise GuardError("database_idx.tcd is marked dirty")
    if entry_count < 0:
        raise GuardError("database_idx.tcd has an invalid entry count")
    if len(master) < 24 + entry_count * ROW_SIZE:
        raise GuardError("database_idx.tcd is shorter than its entry count")

    names = read_blob(os.path.join(base, FILENAME_TAGS))
    if len(names) < 12:
        raise GuardError("database_4.tcd is truncated")
    magic, datasize, _entries = struct.unpack_from("%siii" % endian, names, 0)
    if magic != TAGCACHE_MAGIC or datasize < 0 or len(names) < 12 + datasize:
        raise GuardError("database_4.tcd has an invalid header")

    total = 0
    missing = []
    for index in range(entry_count):
        offset = 24 + index * ROW_SIZE
        row = struct.unpack_from("%s%di" % (endian, TAG_COUNT + 1), master, offset)
        if row[-1] & FLAG_DELETED:
            continue
        relative = decode_tag_entry(names, row[TAG_FILENAME], endian)
        if not relative:
            continue
        total += 1
        if not os.path.isfile(os.path.join(mount, relative.lstrip("/"))):
            if len(missing) < 5:
                missing.append(relative)

    if total == 0:
        raise GuardError("the parsed database contains no tracks")
    if missing:
        raise GuardError(
            "%d or more indexed paths are missing (first: %s)"
            % (len(missing), missing[0])
        )
    return total


def main():
    if len(sys.argv) != 2:
        sys.stderr.write("usage: tagcache_check.py <mount-path>\n")
        return 2
    try:
        total = validate(sys.argv[1])
    except GuardError as exc:
        sys.stderr.write("%s\n" % exc)
        return 1
    sys.stdout.write("validated %d tracks\n" % total)
    return 0


if __name__ == "__main__":
    sys.exit(main())
