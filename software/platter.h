/*
 * Platter tracker: turns raw absolute encoder angles into an unwrapped
 * count plus a low-noise velocity estimate.
 *
 * Pure maths, no I/O, so it can be unit-tested and simulated off the Pi.
 */

#ifndef PLATTER_H
#define PLATTER_H

#include <stdint.h>

#define PLATTER_HISTORY 64

struct platter_tracker {
    int cpr;            /* counts per revolution */
    int last_raw;       /* last accepted raw angle, -1 until seeded */
    int64_t count;      /* unwrapped angle, in counts */
    double velocity;    /* counts per second */

    /* Ring of recent (time, count) samples for the velocity fit */
    double t[PLATTER_HISTORY];
    int64_t c[PLATTER_HISTORY];
    int head;           /* index of the newest sample */
    int n;              /* number of valid samples */
};

void platter_tracker_init(struct platter_tracker *p, int cpr);

/* Forget history; the next update re-seeds without producing a jump */
void platter_tracker_reset(struct platter_tracker *p);

/* Signed shortest-path distance from the last accepted angle to raw,
 * in counts (0 if the tracker is not yet seeded) */
int platter_tracker_delta(const struct platter_tracker *p, int raw);

/* Accept a new raw angle sampled at time t (seconds, monotonic).
 * Return: the signed number of counts moved since the previous sample */
int platter_tracker_update(struct platter_tracker *p, int raw, double t);

#endif
