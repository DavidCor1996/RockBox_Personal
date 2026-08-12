/*
 * Uxn virtual machine core.
 *
 * Copyright (c) 2021-2025 Devine Lu Linvega, Andrew Alderwick,
 * Andrew Richards
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef UXN_H
#define UXN_H

#include <stdint.h>

#define UXN_PAGE_PROGRAM 0x0100
#define UXN_PAGE_SIZE 0x10000
#define UXN_BANKS 0x10
#define UXN_RAM_SIZE (UXN_PAGE_SIZE * UXN_BANKS)

#define PEEK2(d) ((uint16_t)((d)[0] << 8 | (d)[1]))
#define POKE2(d, v) do { \
    (d)[0] = (uint8_t)((v) >> 8); \
    (d)[1] = (uint8_t)(v); \
} while (0)

struct uxn_stack
{
    uint8_t dat[0x100];
    uint8_t ptr;
};

struct uxn_vm
{
    uint8_t *ram;
    uint8_t dev[0x100];
    struct uxn_stack wst;
    struct uxn_stack rst;
};

extern struct uxn_vm uxn;

uint8_t uxn_dei(uint8_t addr);
void uxn_deo(uint8_t addr, uint8_t value);
int uxn_eval(uint16_t pc);

#endif
