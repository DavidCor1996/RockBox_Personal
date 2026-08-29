/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2014 by Cástor Muñoz
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include "cpu.h"
#include "system.h"

#include "s5l87xx.h"
#include "uc87xx.h"


/*
 * s5l8702 UC87XX HW: 1 UARTC, 4 ports
 */
static struct uartc_port *uartc_port_l[UARTC_N_PORTS];
const struct uartc s5l8702_uartc =
{
    .id       = 0,
    .baddr    = UARTC_BASE_ADDR,
    .port_off = UARTC_PORT_OFFSET,
    .n_ports  = UARTC_N_PORTS,
    .port_l   = uartc_port_l,
};

#if defined(IPOD_6G) && defined(IPOD6G_HIBERNATE_STAGE3) && \
        IPOD6G_HIBERNATE_STAGE3 && !defined(BOOTLOADER)
static struct
{
    bool active;
    uint32_t ulcon;
    uint32_t ucon;
    uint32_t ufcon;
    uint32_t umcon;
    uint32_t ubrdiv;
    uint32_t ubrcontx;
    uint32_t ubrconrx;
} uart_hibernate_state[UARTC_N_PORTS];
#endif

/*
 * Device level functions specific to S5L8702
 */
void uart_target_enable_gpio(int uart_id, int port_id)
{
    (void) uart_id;
    switch (port_id) {
        case 0:
            /* configure UART0 Tx/Rx GPIO ports */
            PCON0 = (PCON0 & 0xff00ffff) | 0x00220000;
            break;
        case 1:
            /* configure UART1 GPIO ports, including RTS/CTS signals */
            PCON13 = (PCON13 & 0xff0000ff) | 0x00222200;
            break;
        case 2:
        case 3:
            /* unknown */
        default:
            break;
    }
}

void uart_target_disable_gpio(int uart_id, int port_id)
{
    (void) uart_id;
    switch (port_id) {
        /* configure minimal power consumption */
        case 0:
            PCON0 = (PCON0 & 0xff00ffff) | 0x00ee0000;
            break;
        case 1:
            PCON13 = (PCON13 & 0xff0000ff) | 0x00eeee00;
            break;
        case 2:
        case 3:
        default:
            break;
    }
}

void uart_target_enable_irq(int uart_id, int port_id)
{
    (void) uart_id;
    VIC0INTENABLE = 1 << IRQ_UART(port_id);
}

void uart_target_disable_irq(int uart_id, int port_id)
{
    (void) uart_id;
    VIC0INTENCLEAR = 1 << IRQ_UART(port_id);
}

void uart_target_clear_irq(int uart_id, int port_id)
{
    (void) uart_id;
    (void) port_id;
}

void uart_target_enable_clocks(int uart_id)
{
    (void) uart_id;
    PWRCON(1) &= ~(1 << (CLOCKGATE_UARTC - 32));
}

void uart_target_disable_clocks(int uart_id)
{
    (void) uart_id;
    PWRCON(1) |= (1 << (CLOCKGATE_UARTC - 32));
}

/*
 * ISRs
 */

/* On Classic, PORT0 interrupts are not used when iAP is disabled */
#if !defined(IPOD_6G) || defined(IPOD_ACCESSORY_PROTOCOL)
void ICODE_ATTR INT_UART0(void)
{
    uartc_callback(&s5l8702_uartc, 0);
}
#endif

/* PORT1,2,3 not used on Classic */
#ifndef IPOD_6G
void ICODE_ATTR INT_UART1(void)
{
    uartc_callback(&s5l8702_uartc, 1);
}

void ICODE_ATTR INT_UART2(void)
{
    uartc_callback(&s5l8702_uartc, 2);
}

void ICODE_ATTR INT_UART3(void)
{
    uartc_callback(&s5l8702_uartc, 3);
}
#endif

/* Main init */
void uart_init(void)
{
    uartc_open(&s5l8702_uartc);
}

#if defined(IPOD_6G) && defined(IPOD6G_HIBERNATE_STAGE3) && \
        IPOD6G_HIBERNATE_STAGE3 && !defined(BOOTLOADER)
void uart_hibernate_suspend(void)
{
    for (int id = 0; id < UARTC_N_PORTS; id++)
    {
        uint32_t baddr = s5l8702_uartc.baddr +
                         s5l8702_uartc.port_off * id;
        uart_hibernate_state[id].active = uartc_port_l[id] != NULL;
        if (!uart_hibernate_state[id].active)
            continue;

        uart_hibernate_state[id].ulcon = ULCON(baddr);
        uart_hibernate_state[id].ucon = UCON(baddr);
        uart_hibernate_state[id].ufcon = UFCON(baddr);
        uart_hibernate_state[id].umcon = UMCON(baddr);
        uart_hibernate_state[id].ubrdiv = UBRDIV(baddr);
        uart_hibernate_state[id].ubrcontx = UBRCONTX(baddr);
        uart_hibernate_state[id].ubrconrx = UBRCONRX(baddr);
    }
}

void uart_hibernate_resume(void)
{
    uart_target_enable_clocks(s5l8702_uartc.id);

    for (int id = 0; id < UARTC_N_PORTS; id++)
    {
        if (!uart_hibernate_state[id].active || !uartc_port_l[id])
            continue;

        uint32_t baddr = s5l8702_uartc.baddr +
                         s5l8702_uartc.port_off * id;
        uart_target_enable_gpio(s5l8702_uartc.id, id);
        UCON(baddr) = 0;
        ULCON(baddr) = uart_hibernate_state[id].ulcon;
        UBRDIV(baddr) = uart_hibernate_state[id].ubrdiv;
        UBRCONTX(baddr) = uart_hibernate_state[id].ubrcontx;
        UBRCONRX(baddr) = uart_hibernate_state[id].ubrconrx;
        UFCON(baddr) = uart_hibernate_state[id].ufcon;
        UMCON(baddr) = uart_hibernate_state[id].umcon;
        UTRSTAT(baddr) = ~0u;
        UCON(baddr) = uart_hibernate_state[id].ucon;
    }
}
#endif
