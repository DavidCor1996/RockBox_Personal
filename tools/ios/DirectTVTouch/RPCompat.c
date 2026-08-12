/*
 * Keep the armv7 binary independent of newer compiler-runtime memset helpers.
 * This matches the compatibility shim used by RockPod Store on iOS 6.
 */
void *memset(void *destination, int value, unsigned long length) {
    volatile unsigned char *bytes = (volatile unsigned char *)destination;
    while (length--) {
        *bytes++ = (unsigned char)value;
    }
    return destination;
}
