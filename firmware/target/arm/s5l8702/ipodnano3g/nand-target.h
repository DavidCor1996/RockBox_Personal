/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/                 
 *
 * Copyright (C) 2026 by The Rockbox developers
 *
 ****************************************************************************/

#ifndef __NAND_TARGET_H__
#define __NAND_TARGET_H__

#include "config.h"
#include "inttypes.h"

struct nand_device_info_type
{
    uint32_t id;
    uint16_t blocks;
    uint16_t userblocks;
    uint16_t pagesperblock;
    uint8_t blocksizeexponent;
    uint8_t tunk1;
    uint8_t twp;
    uint8_t tunk2;
    uint8_t tunk3;
} __attribute__((packed));

uint32_t nand_read_page(uint32_t bank, uint32_t page, void* databuffer,
                        void* sparebuffer, uint32_t doecc,
                        uint32_t checkempty);
uint32_t nand_write_page(uint32_t bank, uint32_t page, void* databuffer,
                         void* sparebuffer, uint32_t doecc);
uint32_t nand_block_erase(uint32_t bank, uint32_t page);

uint32_t nand_read_page_fast(uint32_t page, void* databuffer,
                             void* sparebuffer, uint32_t doecc,
                             uint32_t checkempty);
uint32_t nand_write_page_start(uint32_t bank, uint32_t page, void* databuffer,
                               void* sparebuffer, uint32_t doecc);
uint32_t nand_write_page_collect(uint32_t bank);

const struct nand_device_info_type* nand_get_device_type(uint32_t bank);
uint32_t nand_reset(uint32_t bank);
int nand_device_init(void);
void nand_set_active(void);
long nand_last_activity(void);
void nand_power_up(void);
void nand_power_down(void);

#endif
