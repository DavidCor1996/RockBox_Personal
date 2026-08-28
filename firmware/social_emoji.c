/***************************************************************************
 * Complete native Twemoji sequence lookup for Rockbox text rendering.
 ***************************************************************************/

#include "config.h"
#include "social_emoji.h"

#if defined(HAVE_LCD_COLOR) && !defined(BOOTLOADER)
#include "social_emoji_data.h"

int social_emoji_lookup(const ucschar_t *text, unsigned short *cell)
{
    int low = 0;
    int high = SOCIAL_EMOJI_BUCKET_COUNT - 1;

    if (!text || *text == 0)
        return 0;

    while (low <= high)
    {
        int middle = low + (high - low) / 2;
        unsigned int first = social_emoji_buckets[middle].codepoint;

        if ((unsigned int)*text < first)
            high = middle - 1;
        else if ((unsigned int)*text > first)
            low = middle + 1;
        else
        {
            int entry_index;
            int end = social_emoji_buckets[middle].start +
                      social_emoji_buckets[middle].count;

            /* Entries in a bucket are longest-first, so composite emoji win
             * over a valid single-codepoint prefix. */
            for (entry_index = social_emoji_buckets[middle].start;
                 entry_index < end; entry_index++)
            {
                const struct social_emoji_entry_data *entry =
                    &social_emoji_entries[entry_index];
                int key_index;
                int text_index = 0;

                for (key_index = 0; key_index < entry->length; key_index++)
                {
                    while (text[text_index] == 0xfe0f)
                        text_index++;
                    if ((unsigned int)text[text_index] !=
                        social_emoji_codepoints[entry->offset + key_index])
                        break;
                    text_index++;
                }
                if (key_index == entry->length)
                {
                    while (text[text_index] == 0xfe0f)
                        text_index++;
                    if (cell)
                        *cell = entry->cell;
                    return text_index;
                }
            }
            return 0;
        }
    }
    return 0;
}
#endif
