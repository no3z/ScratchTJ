/*
 * Platter tracker: unwrapped encoder count + velocity estimate.
 *
 * Velocity is the least-squares slope of recent samples. Compared with
 * differencing two samples this rejects sensor quantisation noise (1 LSB
 * over 0.5ms would otherwise read as a large speed error), and because
 * every sample carries its own timestamp it is immune to jitter in the
 * polling loop.
 *
 * Two fits run side by side: a long window that is very quiet but slow
 * to notice a change, and a short one that is quick but noisier. While
 * they agree the platter is moving steadily and the quiet estimate is
 * used, so a steady drag plays without pitch flutter. When they
 * disagree the platter is accelerating (a scratch reversing direction)
 * and the estimate moves over to the fast fit, so the sound stays
 * locked to the hand.
 */

#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#include "platter.h"

/* Fit windows, in seconds. Each estimate lags the true velocity by
 * about half its window. */
#define WINDOW_SLOW 0.016
#define WINDOW_FAST 0.004

/* Disagreement between the two fits, as a fraction of a revolution per
 * second, at which the fast fit starts to be used and at which it is
 * used exclusively. Below BLEND_START the difference is just noise. */
#define BLEND_START 0.02
#define BLEND_FULL 0.08

/* A gap this long between samples means the history is useless
 * (sensor dropped out, thread stalled); start again from standstill */
#define STALE_GAP 0.025

void platter_tracker_init(struct platter_tracker *p, int cpr)
{
    p->cpr = cpr;
    p->count = 0;
    platter_tracker_reset(p);
}

void platter_tracker_reset(struct platter_tracker *p)
{
    p->last_raw = -1;
    p->velocity = 0.0;
    p->head = 0;
    p->n = 0;
}

int platter_tracker_delta(const struct platter_tracker *p, int raw)
{
    int d;

    if (p->last_raw < 0)
        return 0;

    d = (raw - p->last_raw) % p->cpr;
    if (d > p->cpr / 2)
        d -= p->cpr;
    else if (d < -(p->cpr / 2))
        d += p->cpr;

    return d;
}

static void push(struct platter_tracker *p, double t)
{
    p->head = (p->head + 1) % PLATTER_HISTORY;
    p->t[p->head] = t;
    p->c[p->head] = p->count;
    if (p->n < PLATTER_HISTORY)
        p->n++;
}

/*
 * Least-squares slope of the samples within the given window of the
 * newest one. Time and count are taken relative to the newest sample to
 * keep the sums small and well conditioned.
 *
 * Return: true and the slope in *out, or false if there is not enough
 * history for a fit
 */

static bool fit(const struct platter_tracker *p, double window, double *out)
{
    int i, used = 0;
    double st = 0.0, sc = 0.0, stt = 0.0, stc = 0.0, denom;
    double t = p->t[p->head];

    for (i = 0; i < p->n; i++) {
        int idx = (p->head - i + PLATTER_HISTORY) % PLATTER_HISTORY;
        double dt = p->t[idx] - t;
        double dc = (double)(p->c[idx] - p->count);

        /* Always take at least three samples, however slow the bus */
        if (dt < -window && used >= 3)
            break;

        st += dt;
        sc += dc;
        stt += dt * dt;
        stc += dt * dc;
        used++;
    }

    if (used < 2)
        return false;

    denom = used * stt - st * st;
    if (denom <= 0.0)
        return false;

    *out = (used * stc - st * sc) / denom;
    return true;
}

int platter_tracker_update(struct platter_tracker *p, int raw, double t)
{
    int d;
    double slow, fast, gap, w;

    d = platter_tracker_delta(p, raw);
    p->last_raw = raw;
    p->count += d;

    /* Time went backwards or stood still: keep the previous estimate */
    if (p->n > 0 && t <= p->t[p->head])
        return d;

    if (p->n > 0 && t - p->t[p->head] > STALE_GAP) {
        p->n = 0;
        p->velocity = 0.0;
    }

    push(p, t);

    if (!fit(p, WINDOW_SLOW, &slow) || !fit(p, WINDOW_FAST, &fast))
        return d; /* not enough history yet; velocity stays as it was */

    gap = fabs(fast - slow) / p->cpr;
    if (gap <= BLEND_START)
        w = 0.0;
    else if (gap >= BLEND_FULL)
        w = 1.0;
    else
        w = (gap - BLEND_START) / (BLEND_FULL - BLEND_START);

    p->velocity = slow + w * (fast - slow);

    return d;
}
