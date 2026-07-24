/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/ashmem.h>
#include <cutils/ashmem.h>

#define ASHMEM_DEVICE "/dev/ashmem"

/*
 * Linux removed the staging ashmem driver after Android moved to memfd.  The
 * Eclair ABI predates memfd_create, so use an unlinked file on /dev tmpfs when
 * the legacy device is absent.  This retains shared-memory file-descriptor
 * semantics without creating persistent storage.  Pinning and protection
 * ioctls degrade to the behavior of AOSP's historical host simulator.
 */
static int is_tmpfs_fallback(int fd)
{
    struct stat buf;

    return fstat(fd, &buf) == 0 && buf.st_nlink == 0 && S_ISREG(buf.st_mode);
}

static int create_tmpfs_fallback(size_t size)
{
    static unsigned int serial;
    const char *directory = "/dev";
    const char *emulation_directory;
    char path[256];
    unsigned int attempt;
    int fd = -1;

    /* QEMU user-mode shares the host's read-only /dev.  Tests may explicitly
     * redirect the immediately-unlinked backing file into a fresh /tmp
     * directory.  The storage-free target profile uses one exact directory
     * inside /dev tmpfs so Zygote can create regions while temporarily running
     * under an unprivileged UID. */
    emulation_directory = getenv("IPOD6G_ASHMEM_TMPDIR");
    if (emulation_directory != NULL) {
        size_t length = strlen(emulation_directory);

        if ((strncmp(emulation_directory, "/tmp/", 5) != 0 &&
                strcmp(emulation_directory, "/dev/ashmem-fallback") != 0) ||
                length < 6 || length > 160 ||
                strstr(emulation_directory, "/../") != NULL ||
                strcmp(emulation_directory + length - 3, "/..") == 0 ||
                strchr(emulation_directory, '\n') != NULL) {
            errno = EINVAL;
            return -1;
        }
        directory = emulation_directory;
    }

    /* Eclair's mkstemp waits for kernel entropy during early boot.  A PID and
     * retrying sequence are sufficient for an immediately unlinked tmpfs file. */
    for (attempt = 0; attempt < 64; ++attempt) {
        unsigned int value = serial++;
        int length = snprintf(path, sizeof(path),
                              "%s/.eclair-ashmem-%d-%u", directory,
                              getpid(), value);
        if (length < 0 || (size_t) length >= sizeof(path)) {
            errno = ENAMETOOLONG;
            return -1;
        }
        fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0 || errno != EEXIST)
            break;
    }
    if (fd < 0)
        return -1;
    if (ftruncate(fd, size) < 0 || unlink(path) < 0) {
        int saved_errno = errno;
        close(fd);
        unlink(path);
        errno = saved_errno;
        return -1;
    }
    return fd;
}

int ashmem_create_region(const char *name, size_t size)
{
    int fd, ret;

    fd = open(ASHMEM_DEVICE, O_RDWR);
    if (fd < 0) {
        if (errno == ENOENT || errno == ENODEV)
            return create_tmpfs_fallback(size);
        return -1;
    }

    if (name) {
        char buf[ASHMEM_NAME_LEN];

        strlcpy(buf, name, sizeof(buf));
        ret = ioctl(fd, ASHMEM_SET_NAME, buf);
        if (ret < 0)
            goto error;
    }

    ret = ioctl(fd, ASHMEM_SET_SIZE, size);
    if (ret < 0)
        goto error;

    return fd;

error:
    close(fd);
    return ret;
}

int ashmem_set_prot_region(int fd, int prot)
{
    if (is_tmpfs_fallback(fd))
        return 0;
    return ioctl(fd, ASHMEM_SET_PROT_MASK, prot);
}

int ashmem_pin_region(int fd, size_t offset, size_t len)
{
    struct ashmem_pin pin = { offset, len };
    if (is_tmpfs_fallback(fd))
        return ASHMEM_NOT_PURGED;
    return ioctl(fd, ASHMEM_PIN, &pin);
}

int ashmem_unpin_region(int fd, size_t offset, size_t len)
{
    struct ashmem_pin pin = { offset, len };
    if (is_tmpfs_fallback(fd))
        return ASHMEM_IS_UNPINNED;
    return ioctl(fd, ASHMEM_UNPIN, &pin);
}

int ashmem_get_size_region(int fd)
{
    struct stat buf;

    if (is_tmpfs_fallback(fd)) {
        if (fstat(fd, &buf) < 0)
            return -1;
        return (int) buf.st_size;
    }
    return ioctl(fd, ASHMEM_GET_SIZE, NULL);
}
