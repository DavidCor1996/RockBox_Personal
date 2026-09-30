/* Offline campaign state, shared by the game and host validation. GPL-2.0+. */
#ifndef EIGHTBIT_REBELLION_STATE_H
#define EIGHTBIT_REBELLION_STATE_H
#include <stdint.h>
#include <stdbool.h>

#define BR_SAVE_MAGIC 0x31524238u
#define BR_SAVE_VERSION 1u

struct br_state
{
    uint32_t magic, version, sequence, scene, x, quest, collected;
    uint32_t tracks, coins, health, weapon, costume, checksum;
};

static inline uint32_t br_checksum(const struct br_state *s)
{
    const unsigned char *bytes = (const unsigned char *)s;
    uint32_t hash = 2166136261u;
    unsigned i;
    for (i = 0; i < sizeof(*s) - sizeof(s->checksum); ++i)
        hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}

static inline bool br_state_valid(const struct br_state *s,
                                 unsigned scenes, unsigned quests)
{
    return s->magic == BR_SAVE_MAGIC && s->version == BR_SAVE_VERSION &&
        s->checksum == br_checksum(s) && s->scene < scenes &&
        s->x < 4096 && s->quest <= quests && s->health >= 1 &&
        s->health <= 5 && s->weapon <= 3 && s->costume < 3 &&
        s->tracks < 64 && s->coins <= 1000000;
}

static inline unsigned br_progress(uint32_t bits)
{
    unsigned count = 0;
    for (; bits; bits &= bits - 1)
        ++count;
    return count;
}

static inline bool br_collect(struct br_state *s, unsigned object,
                              unsigned total)
{
    if (!total || total > 31 || object >= total ||
        (s->collected & (1u << object)))
        return false;
    s->collected |= 1u << object;
    s->coins += 2;
    return true;
}

static inline bool br_complete(struct br_state *s, unsigned total,
                               unsigned reward)
{
    if (!total || total > 31 ||
        s->collected != ((1u << total) - 1))
        return false;
    s->tracks |= reward;
    s->coins += 10;
    s->collected = 0;
    s->quest++;
    return true;
}
#endif
