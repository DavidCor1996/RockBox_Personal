/***************************************************************************
 * Native color-emoji fallback used by Rockbox's shared text renderer.
 * Artwork is Twemoji under CC-BY 4.0; see
 * assets/ipodjs/rockbox/social-emoji/LICENSE-GRAPHICS.txt.
 ***************************************************************************/

#ifndef ROCKBOX_SOCIAL_EMOJI_H
#define ROCKBOX_SOCIAL_EMOJI_H

#define SOCIAL_EMOJI_WIDTH         14
#define SOCIAL_EMOJI_HEIGHT        14
#define SOCIAL_EMOJI_ATLAS_COLUMNS 16
#define SOCIAL_EMOJI_ATLAS_WIDTH   (SOCIAL_EMOJI_WIDTH * SOCIAL_EMOJI_ATLAS_COLUMNS)
#define SOCIAL_EMOJI_MAX_SEQUENCE  10

#if defined(HAVE_LCD_COLOR) && !defined(BOOTLOADER)
/* Returns the number of input codepoints consumed, or zero for no match. */
int social_emoji_lookup(const ucschar_t *text, unsigned short *cell);
#endif

#endif /* ROCKBOX_SOCIAL_EMOJI_H */
