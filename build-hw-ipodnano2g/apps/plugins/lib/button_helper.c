/* Don't change this file! */
/* It is automatically generated of button.h */
#include "plugin.h"
#include "button.h"
#include "button_helper.h"

const size_t button_helper_maxbuffer = 28;

static const struct available_button buttons[33] = {
    {"BUTTON_NONE", 0x00000000},
    {"BUTTON_SELECT", 0x00000001},
    {"BUTTON_MENU", 0x00000002},
    {"BUTTON_LEFT", 0x00000004},
    {"BUTTON_RIGHT", 0x00000008},
    {"BUTTON_SCROLL_FWD", 0x00000010},
    {"BUTTON_SCROLL_BACK", 0x00000020},
    {"BUTTON_PLAY", 0x00000040},
    {"BUTTON_RC_VOL_DOWN", 0x00008000},
    {"BUTTON_RC_VOL_UP", 0x00010000},
    {"BUTTON_RC_RIGHT", 0x00020000},
    {"BUTTON_RC_LEFT", 0x00040000},
    {"BUTTON_RC_STOP", 0x00080000},
    {"BUTTON_RC_PLAY", 0x00100000},
    {"BUTTON_RC_MENU", 0x00200000},
    {"BUTTON_RC_SELECT", 0x00400000},
    {"BUTTON_RC_UP", 0x00800000},
    {"BUTTON_RC_DOWN", 0x01000000},
    {"BUTTON_REL", 0x02000000},
    {"BUTTON_REPEAT", 0x04000000},
    {"BUTTON_TOUCHSCREEN", 0x08000000},
    {"BUTTON_MULTIMEDIA", 0x10000000},
    {"BUTTON_REDRAW", 0x20000000},
    {"BUTTON_MULTIMEDIA_FFWD", (BUTTON_MULTIMEDIA|0x20)},
    {"BUTTON_REMOTE", (BUTTON_RC_UP|BUTTON_RC_DOWN |BUTTON_RC_SELECT|BUTTON_RC_PLAY |BUTTON_RC_PLAY|BUTTON_RC_STOP |BUTTON_RC_LEFT|BUTTON_RC_RIGHT |BUTTON_RC_VOL_UP|BUTTON_RC_VOL_DOWN)},
    {"BUTTON_MULTIMEDIA_PLAYPAUSE", (BUTTON_MULTIMEDIA|0x01)},
    {"BUTTON_MULTIMEDIA_STOP", (BUTTON_MULTIMEDIA|0x02)},
    {"BUTTON_MULTIMEDIA_REW", (BUTTON_MULTIMEDIA|0x10)},
    {"BUTTON_MULTIMEDIA_NEXT", (BUTTON_MULTIMEDIA|0x08)},
    {"BUTTON_MULTIMEDIA_PREV", (BUTTON_MULTIMEDIA|0x04)},
    {"BUTTON_MAIN", (BUTTON_SELECT|BUTTON_MENU |BUTTON_LEFT|BUTTON_RIGHT|BUTTON_SCROLL_FWD |BUTTON_SCROLL_BACK|BUTTON_PLAY)},
    {"BUTTON_MULTIMEDIA_ALL", (BUTTON_MULTIMEDIA_PLAYPAUSE| BUTTON_MULTIMEDIA_STOP| BUTTON_MULTIMEDIA_PREV| BUTTON_MULTIMEDIA_NEXT| BUTTON_MULTIMEDIA_REW | BUTTON_MULTIMEDIA_FFWD)},
    {"\0", 0} /* sentinel */
};
const int available_button_count = 32;
const struct available_button * const available_buttons = buttons;

int get_button_names(char *buf, size_t bufsz, unsigned long button)
{
    int len = 0;
    buf[0] = '\0';
    const struct available_button *btn = buttons;
    while(btn->name[0] != '\0')
    {
        if(btn->value == 0)
        {
            if (button == 0)
            {
                buf[0] = '\0';
                len = rb->strlcat(buf, btn->name, bufsz);
                return len;
            }
        }
        else if ((button & btn->value) == btn->value)
        {
            if (len > 0)
                rb->strlcat(buf, " | ", bufsz);
            len = rb->strlcat(buf, btn->name, bufsz);
        }
        btn++;
    }
    return len;
}
