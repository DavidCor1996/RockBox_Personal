/***************************************************************************
 * Offline RetroAchievements runtime shared by Rockbox emulator plugins.
 ****************************************************************************/

#ifndef ROCKACHIEVEMENTS_H
#define ROCKACHIEVEMENTS_H

#include "plugin.h"

#define ROCKACHIEVEMENTS_WORKSPACE_MIN (192 * 1024)
#define ROCKACHIEVEMENTS_WORKSPACE_TARGET (256 * 1024)

typedef uint32_t (*rockachievements_peek_t)(uint32_t address,
                                            uint32_t num_bytes,
                                            void *userdata);

struct rockachievements_runtime
{
    void *runtime;
    rockachievements_peek_t peek;
    void *peek_userdata;
    char game_key[21];
    char progress_path[MAX_PATH];
    unsigned active_count;
    bool enabled;
    bool hardcore;
};

bool rockachievements_available(const char *launch_target);
bool rockachievements_init(struct rockachievements_runtime *runtime,
                           const char *launch_target,
                           rockachievements_peek_t peek,
                           void *peek_userdata,
                           void *workspace,
                           size_t workspace_size);
void rockachievements_do_frame(struct rockachievements_runtime *runtime);
void rockachievements_reset(struct rockachievements_runtime *runtime);
void rockachievements_shutdown(struct rockachievements_runtime *runtime);
bool rockachievements_hardcore_active(
    const struct rockachievements_runtime *runtime);
bool rockachievements_any_hardcore_active(void);

#endif
