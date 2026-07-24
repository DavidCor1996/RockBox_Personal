#include "pocketsky.h"

#include <stdarg.h>

int ps_snprintf(char *buffer, size_t size, const char *format, ...)
{
    int result;
    va_list args;

    va_start(args, format);
    result = rb->vsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}

int ps_fprintf(void *stream, const char *format, ...)
{
    char buffer[96];
    va_list args;

    (void)stream;
    va_start(args, format);
    rb->vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    DEBUGF("Pocket Sky upstream: %s", buffer);
    return rb->strlen(buffer);
}

void ps_exit(int status)
{
    (void)status;
    DEBUGF("Pocket Sky upstream requested exit: %d\n", status);
}

void *ps_memcpy(void *destination, const void *source, size_t size)
{
    return rb->memcpy(destination, source, size);
}

void *ps_memset(void *destination, int value, size_t size)
{
    return rb->memset(destination, value, size);
}

int ps_strcmp(const char *left, const char *right)
{
    return rb->strcmp(left, right);
}

char *ps_strcpy(char *destination, const char *source)
{
    return rb->strcpy(destination, source);
}

size_t ps_strlen(const char *text)
{
    return rb->strlen(text);
}

unsigned short ps_get_u16(const unsigned char *data)
{
    return (unsigned short)(data[0] | ((unsigned short)data[1] << 8));
}

unsigned int ps_get_u32(const unsigned char *data)
{
    return (unsigned int)data[0] |
           ((unsigned int)data[1] << 8) |
           ((unsigned int)data[2] << 16) |
           ((unsigned int)data[3] << 24);
}

float ps_get_float(const unsigned char *data)
{
    union
    {
        unsigned int bits;
        float value;
    } converted;

    converted.bits = ps_get_u32(data);
    return converted.value;
}

void ps_put_error(const char *title, const char *detail)
{
    rb->lcd_clear_display();
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_putsxy(4, 12, title);
    rb->lcd_puts_scroll(0, 4, detail);
    rb->lcd_puts(0, 12, "MENU: exit");
    rb->lcd_update();
}

static char ps_ascii_lower(char value)
{
    if (value >= 'A' && value <= 'Z')
        return value + ('a' - 'A');
    return value;
}

bool ps_ascii_contains(const char *haystack, const char *needle)
{
    size_t haystack_length;
    size_t needle_length;
    size_t i;
    size_t j;

    if (!haystack || !needle)
        return false;
    haystack_length = rb->strlen(haystack);
    needle_length = rb->strlen(needle);
    if (needle_length == 0)
        return true;
    if (needle_length > haystack_length)
        return false;

    for (i = 0; i + needle_length <= haystack_length; ++i)
    {
        for (j = 0; j < needle_length; ++j)
        {
            if (ps_ascii_lower(haystack[i + j]) != ps_ascii_lower(needle[j]))
                break;
        }
        if (j == needle_length)
            return true;
    }
    return false;
}

int ps_decimal_to_microdegrees(const char *text, bool *ok)
{
    bool negative = false;
    int whole = 0;
    int fraction = 0;
    int digits = 0;

    *ok = false;
    if (!text)
        return 0;
    while (*text == ' ')
        ++text;
    if (*text == '-' || *text == '+')
    {
        negative = *text == '-';
        ++text;
    }
    if (*text < '0' || *text > '9')
        return 0;
    while (*text >= '0' && *text <= '9')
    {
        if (whole > 1000)
            return 0;
        whole = whole * 10 + (*text++ - '0');
    }
    if (*text == '.')
    {
        ++text;
        while (*text >= '0' && *text <= '9')
        {
            if (digits < 6)
            {
                fraction = fraction * 10 + (*text - '0');
                ++digits;
            }
            ++text;
        }
    }
    while (digits < 6)
    {
        fraction *= 10;
        ++digits;
    }
    while (*text == ' ')
        ++text;
    if (*text != '\0')
        return 0;
    *ok = true;
    whole = whole * 1000000 + fraction;
    return negative ? -whole : whole;
}

void ps_format_coordinate(char *buffer, size_t size, int microdegrees)
{
    unsigned int magnitude;

    magnitude = microdegrees < 0 ? -microdegrees : microdegrees;
    rb->snprintf(buffer, size, "%s%u.%06u",
                 microdegrees < 0 ? "-" : "",
                 magnitude / 1000000,
                 magnitude % 1000000);
}
