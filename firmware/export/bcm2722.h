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

enum bcm2722_video_stage
{
    BCM2722_VIDEO_STAGE_OFF = 0,
    BCM2722_VIDEO_STAGE_VALIDATE,
    BCM2722_VIDEO_STAGE_LCD_HANDOFF,
    BCM2722_VIDEO_STAGE_POWER_RESET,
    BCM2722_VIDEO_STAGE_BOOTSTRAP,
    BCM2722_VIDEO_STAGE_UPLOAD,
    BCM2722_VIDEO_STAGE_VERIFY,
    BCM2722_VIDEO_STAGE_START,
    BCM2722_VIDEO_STAGE_RUNTIME_BOOTSTRAP,
    BCM2722_VIDEO_STAGE_READY,
    BCM2722_VIDEO_STAGE_STOPPED,
};

bool bcm2722_video_start(const void *vmcs, size_t length);
void bcm2722_video_stop(void);
bool bcm2722_videoout_set_mode(int mode);
bool bcm2722_video_active(void);
bool bcm2722_video_faulted(void);
const char *bcm2722_video_error(void);
enum bcm2722_video_stage bcm2722_video_get_stage(void);

bool bcm2722_read32(uint32_t address, uint32_t *value);
bool bcm2722_write32(uint32_t address, uint32_t value);
bool bcm2722_read16(uint32_t address, uint16_t *value);
bool bcm2722_write16(uint32_t address, uint16_t value);
bool bcm2722_read_buffer(uint32_t address, void *buffer, size_t length);
bool bcm2722_write_buffer(uint32_t address, const void *buffer,
                          size_t length);
bool bcm2722_notify(void);

#endif /* IPOD_VIDEO */
#endif /* BCM2722_H */
