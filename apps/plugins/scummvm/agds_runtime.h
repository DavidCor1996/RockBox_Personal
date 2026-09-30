/***************************************************************************
 * Native AGDS bootstrap runtime for the Rockbox ScummVM plugin.
 ****************************************************************************/

#ifndef SCUMMVM_AGDS_RUNTIME_H
#define SCUMMVM_AGDS_RUNTIME_H

#include "scummvm.h"
#include "agds_vm.h"
#include "video.h"

#define SCUMMVM_AGDS_DIALOG_TEXT 512

struct scummvm_agds_dialog_overlay {
    char text[SCUMMVM_AGDS_DIALOG_TEXT];
    int16_t font_slot;
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t type;
    bool npc;
};

bool scummvm_agds_runtime_init(const struct scummvm_target *target,
                               struct scummvm_video *video,
                               char *status, size_t status_size);
void scummvm_agds_runtime_input(int x, int y, bool click, bool look);
void scummvm_agds_runtime_key(const char *key);
bool scummvm_agds_runtime_dialog_overlay(
    struct scummvm_agds_dialog_overlay *overlay);
unsigned scummvm_agds_runtime_object_text_count(void);
bool scummvm_agds_runtime_object_text(
    unsigned index, struct scummvm_agds_object_text *text);
bool scummvm_agds_runtime_font(
    unsigned slot, struct scummvm_agds_font *font);
bool scummvm_agds_runtime_pointer_visible(void);
bool scummvm_agds_runtime_main_menu(void);
bool scummvm_agds_runtime_frame(char *status, size_t status_size);
void scummvm_agds_runtime_reset(void);

#endif
