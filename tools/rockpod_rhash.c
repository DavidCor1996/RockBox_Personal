/* Host helper for generating console-correct RetroAchievements hashes. */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "rc_hash.h"

static void print_error(const char *message,
                        const rc_hash_iterator_t *iterator)
{
    (void)iterator;
    fprintf(stderr, "%s\n", message);
}

int main(int argc, char **argv)
{
    rc_hash_iterator_t iterator;
    char hash[33];
    char *end;
    unsigned long console_id;

    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <console-id> <content-path>\n", argv[0]);
        return 2;
    }

    errno = 0;
    console_id = strtoul(argv[1], &end, 10);
    if (errno || *argv[1] == '\0' || *end != '\0' || console_id == 0 ||
        console_id > 255)
    {
        fprintf(stderr, "invalid console id: %s\n", argv[1]);
        return 2;
    }

    rc_hash_initialize_iterator(&iterator, argv[2], NULL, 0);
    iterator.callbacks.error_message = print_error;
    if (!rc_hash_generate(hash, (uint32_t)console_id, &iterator))
    {
        rc_hash_destroy_iterator(&iterator);
        fprintf(stderr, "unable to hash: %s\n", argv[2]);
        return 1;
    }

    rc_hash_destroy_iterator(&iterator);
    puts(hash);
    return 0;
}
