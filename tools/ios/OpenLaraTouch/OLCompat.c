/* Compatibility for the modern Linux-hosted SDK linker targeting iOS 6. */
void *memset(void *destination, int value, unsigned long length) {
    volatile unsigned char *bytes = (volatile unsigned char *)destination;
    while (length--) *bytes++ = (unsigned char)value;
    return destination;
}

void *memcpy(void *destination, const void *source, unsigned long length) {
    unsigned char *out = (unsigned char *)destination;
    const unsigned char *in = (const unsigned char *)source;
    while (length--) *out++ = *in++;
    return destination;
}

void *memmove(void *destination, const void *source, unsigned long length) {
    unsigned char *out = (unsigned char *)destination;
    const unsigned char *in = (const unsigned char *)source;
    if (out < in) {
        while (length--) *out++ = *in++;
    } else {
        out += length;
        in += length;
        while (length--) *--out = *--in;
    }
    return destination;
}

int memcmp(const void *left, const void *right, unsigned long length) {
    const unsigned char *a = (const unsigned char *)left;
    const unsigned char *b = (const unsigned char *)right;
    while (length--) {
        if (*a != *b) return *a < *b ? -1 : 1;
        ++a;
        ++b;
    }
    return 0;
}

int strcmp(const char *left, const char *right) {
    while (*left && *left == *right) {
        ++left;
        ++right;
    }
    return (unsigned char)*left - (unsigned char)*right;
}

void memset_pattern16(void *destination, const void *pattern,
                      unsigned long length) {
    unsigned char *out = (unsigned char *)destination;
    const unsigned char *bytes = (const unsigned char *)pattern;
    unsigned long index;
    for (index = 0; index < length; ++index) out[index] = bytes[index & 15];
}
