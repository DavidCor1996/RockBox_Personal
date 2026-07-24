/* SPDX-License-Identifier: GPL-2.0-only */

#include <stddef.h>
#include <jffs2/mini_inflate.h>
#include "payload_constants.h"

#define COMPRESSED_ADDRESS 0x08010000u
#define UBOOT_ADDRESS       0x22000000u
#define WDTCON              0x3c800000u
#define SWRCON              0x3c500050u

static void *copy_bytes(void *destination, const void *source, size length)
{
    unsigned char *out = destination;
    const unsigned char *in = source;
    void *result = destination;

    while (length-- != 0)
        *out++ = *in++;
    return result;
}

static unsigned int crc32(const unsigned char *data, size_t length)
{
    unsigned int crc = 0xffffffffu;
    unsigned int bit;

    while (length-- != 0)
    {
        crc ^= *data++;
        for (bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

void __attribute__((noreturn)) relocator_main(void)
{
    unsigned char *destination = (unsigned char *)UBOOT_ADDRESS;
    long decoded;

    decoded = decompress_block(destination,
                               (unsigned char *)COMPRESSED_ADDRESS,
                               copy_bytes);
    if (decoded != (long)UBOOT_UNCOMPRESSED_SIZE ||
        crc32(destination, UBOOT_UNCOMPRESSED_SIZE) != UBOOT_CRC32)
    {
        for (;;)
            ;
    }

#ifdef N25_RELOCATOR_CHECKPOINT_RESET
    *(volatile unsigned int *)SWRCON = 0x000000a5u;
    *(volatile unsigned int *)WDTCON = 0x00100000u;
    for (;;)
        ;
#endif

    asm volatile(
        "mov r0, #0\n"
        "mcr p15, 0, r0, c7, c10, 4\n"
        "mcr p15, 0, r0, c7, c5, 0\n"
        "mcr p15, 0, r0, c7, c5, 4\n"
        : : : "r0", "memory");

    ((void (*)(void))UBOOT_ADDRESS)();
    for (;;)
        ;
}
