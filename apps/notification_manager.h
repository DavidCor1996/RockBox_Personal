#ifndef ROCKPOD_NOTIFICATION_MANAGER_H
#define ROCKPOD_NOTIFICATION_MANAGER_H

#include <stdbool.h>
#include "notification.h"

#define NOTIFICATION_HISTORY_MAX 24

struct notification_record
{
    struct notification_request request;
    unsigned long sequence;
    bool read;
};

void notification_manager_init(void);
void notification_manager_service(void);
void notification_manager_hibernate_prepare(void);
void notification_manager_hibernate_resume(void);
void notification_manager_hibernate_abort(void);
bool notification_post(const struct notification_request *request);
bool notification_schedule(const struct notification_request *request,
                           long rtc_deadline);
void notification_cancel(unsigned source, unsigned kind,
                         uint32_t stable_id);
int notification_manager_count(void);
int notification_manager_unread_count(void);
bool notification_manager_get(int index, struct notification_record *record);
void notification_manager_mark_read(int index);
void notification_manager_remove(int index);
void notification_manager_clear(void);
const struct notification_record *notification_manager_banner(void);
long notification_manager_banner_started(void);
void notification_manager_set_center_active(bool active);
bool notification_manager_banners_suppressed(void);
void notification_manager_set_banners_suppressed(bool suppressed);
void notification_manager_set_desktop_mode(bool active);
void notification_manager_flush(void);
void notification_manager_prepare_visuals(void);
int notification_manager_visual_font(bool bold);
void notification_manager_test_banner(void);

#endif
