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
