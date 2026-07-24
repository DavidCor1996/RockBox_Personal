/*
 * Copyright (C) 2026 The Rockpod contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <stddef.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <unistd.h>

static int write_all(const char *text, size_t length)
{
    size_t offset = 0;

    while (offset < length) {
        ssize_t result = write(STDOUT_FILENO, text + offset, length - offset);

        if (result < 0 && errno == EINTR)
            continue;
        if (result <= 0)
            return -1;
        offset += (size_t)result;
    }
    return 0;
}

#define EMIT(text) do { \
    static const char message[] = text "\n"; \
    if (write_all(message, sizeof(message) - 1) != 0) \
        return 10; \
} while (0)

int main(void)
{
    struct utsname name;
    unsigned char *page;

    EMIT("IPOD6G_ECLAIR_PROBE:BEGIN");

    if (sizeof(void *) != 4)
        return 11;
    if (getpid() <= 0)
        return 12;
    if (uname(&name) != 0 || name.sysname[0] == '\0')
        return 13;
    EMIT("IPOD6G_ECLAIR_PROBE:SYSCALLS_OK");

    page = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED)
        return 14;
    page[0] = 0x25;
    page[4095] = 0x6e;
    if (page[0] != 0x25 || page[4095] != 0x6e)
        return 15;
    if (munmap(page, 4096) != 0)
        return 16;
    EMIT("IPOD6G_ECLAIR_PROBE:MEMORY_OK");
    EMIT("IPOD6G_ECLAIR_PROBE:PASS");
    return 0;
}
