/* Display Remote volume packets have no release event. A short physical tap
 * sends a burst about 100 ms apart; navigation must not count every packet.
 * Cursor/volume synchronization still consumes every packet in iap-core.c. */
#ifndef IAP_VOLUME_GESTURE_H
#define IAP_VOLUME_GESTURE_H
struct iap_volume_gesture
{
    int direction;
    long last, start, emitted;
};

static inline bool iap_volume_gesture_step(struct iap_volume_gesture *g,
                                          int direction, long now)
{
    bool fresh = direction != g->direction ||
                 TIME_AFTER(now, g->last + MAX(1, HZ / 5));
    g->last = now;
    if (fresh)
    {
        g->direction = direction;
        g->start = g->emitted = now;
        return true;
    }
    if (TIME_BEFORE(now, g->start + HZ / 2) ||
        TIME_BEFORE(now, g->emitted + MAX(1, HZ / 8)))
        return false;
    g->emitted = now;
    return true;
}
#endif
