/***************************************************************************
 * Broadcom BCM2722 host interface used by the iPod Video (5G/5.5G).
 *
 * The normal LCD driver owns this device.  These calls temporarily replace
 * the small NOR LCD image with Apple's retail VMCS image for H.264 playback,
 * then restore the normal Rockbox LCD image before returning to the UI.
 ****************************************************************************/
#ifndef BCM2722_H
#define BCM2722_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef IPOD_VIDEO

bool bcm2722_video_start(const void *vmcs, size_t length);
void bcm2722_video_stop(void);
bool bcm2722_video_active(void);

uint32_t bcm2722_read32(uint32_t address);
void bcm2722_write32(uint32_t address, uint32_t value);
uint16_t bcm2722_read16(uint32_t address);
void bcm2722_write16(uint32_t address, uint16_t value);
bool bcm2722_read_buffer(uint32_t address, void *buffer, size_t length);
bool bcm2722_write_buffer(uint32_t address, const void *buffer,
                          size_t length);
void bcm2722_notify(void);

#endif /* IPOD_VIDEO */
#endif /* BCM2722_H */
