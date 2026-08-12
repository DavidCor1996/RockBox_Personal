/* Varvara devices that do not require their own Rockbox backend. */

#ifndef UXN_DEVICES_H
#define UXN_DEVICES_H

#include "plugin.h"
#include "uxn.h"

void system_deo(uint8_t addr);
uint8_t system_dei(uint8_t addr);

void console_deo(uint8_t addr);
void controller_deo(uint8_t addr);
void controller_down(uint8_t mask);
void controller_up(uint8_t mask);
void controller_key(uint8_t key);

void mouse_deo(uint8_t addr);
void mouse_down(uint8_t mask);
void mouse_up(uint8_t mask);
void mouse_pos(uint16_t x, uint16_t y);
void mouse_scroll(int16_t x, int16_t y);

uint8_t datetime_dei(uint8_t addr);

#endif
