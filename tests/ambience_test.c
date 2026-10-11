/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GHOULBOX (on Felucca by Leo Kuroshita, Hügelton Instruments) */
/* The ambience (firmware/src/ambience.c, G_AMB / G_CRKL) on the host:
 * 1. AMB 0: nothing (an empty mix stays all zero; the goldens hold every other render).
 * 2. the wind (CRKL 0): bounded, no DC, its energy around 200..400 Hz (not hiss: > 20 dB over 3 kHz), the two sides
 *    alike in level but not the same signal, and no pops.
 * 3. the crackle: pops (sharp transients) at about CRKL's rate, ~2 / s at 40 and ~20 / s at 127.
 * 4. AMB's taper: 64 about 12 dB under 127 (a level squared).
 * 5. cost: instructions per sample of the generator (the Linux counter), against a NOISE voice (CAVE WIND). */
#define main hostsim_main
#include "hostsim.c"
#undef main
#ifdef __linux__
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

static int bad;
static void check(const char *what, int ok)
{
    printf("ambience: %-96s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static uint64_t instr_now(void)
{
#ifdef __linux__
    static int fd = -2;
    uint64_t v;
    if (fd == -2) {
        struct perf_event_attr a;
        memset(&a, 0, sizeof a);
        a.type = PERF_TYPE_HARDWARE;
        a.size = sizeof a;
        a.config = PERF_COUNT_HW_INSTRUCTIONS;
        a.exclude_kernel = a.exclude_hv = 1;
        fd = (int)syscall(SYS_perf_event_open, &a, 0, -1, -1, 0);
    }
    if (fd >= 0 && read(fd, &v, sizeof v) == (ssize_t)sizeof v)
        return v;
#endif
    return 0;
}

#define SECS 10u
#define N (SECS * FS / CTL * CTL)
static double yl[N], yr[N];

/* the generator alone (its own buffers: no tracks, no reverb), AMB a, CRKL k, SECS seconds after 1 s */
static void gen(int16_t a, int16_t k)
{
    static int32_t l[CTL], r[CTL], rv[CTL];
    uint32_t i, j;
    host_tracks_init();
    song.g[G_AMB] = a;
    song.g[G_CRKL] = k;
    amb.on = 0;
    for (i = 0; i < FS / CTL; i++) {
        memset(l, 0, sizeof l); memset(r, 0, sizeof r); memset(rv, 0, sizeof rv);
        amb_block(l, r, rv, CTL);
    }
    for (i = 0; i < N; i += CTL) {
        memset(l, 0, sizeof l); memset(r, 0, sizeof r); memset(rv, 0, sizeof rv);
        amb_block(l, r, rv, CTL);
        for (j = 0; j < CTL; j++) {
            yl[i + j] = l[j] / 32768.0;
            yr[i + j] = r[j] / 32768.0;
        }
    }
}
static double tone(const double *y, double hz)          /* Goertzel power at hz over the first second, Hann */
{
    double w = 2 * M_PI * hz / FS, c = 2 * cos(w), s0, s1 = 0, s2 = 0;
    uint32_t i, n = FS;
    for (i = 0; i < n; i++) {
        s0 = y[i] * (0.5 - 0.5 * cos(2 * M_PI * i / n)) + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return (s1 * s1 + s2 * s2 - c * s1 * s2) / ((double)n * n);
}
static double band(const double *y, double lo, double hi)   /* mean power of 10 Hz bins lo..hi */
{
    double s = 0;
    uint32_t m = 0;
    for (double f = lo; f <= hi; f += 10, m++)
        s += tone(y, f);
    return s / m;
}
static double rms(const double *y)
{
    double s = 0;
    for (uint32_t i = 0; i < N; i++)
        s += y[i] * y[i];
    return sqrt(s / N);
}
/* pops: |second difference| (left + right) over thr, 3 ms apart at least; floor(): the largest of the wind alone */
static double sd(uint32_t i) { return fabs(yl[i] - 2 * yl[i - 1] + yl[i - 2]) + fabs(yr[i] - 2 * yr[i - 1] + yr[i - 2]); }
static double floor_sd(void)
{
    double m = 0;
    for (uint32_t i = 2; i < N; i++)
        m = fmax(m, sd(i));
    return m;
}
static double thr = 0.01;
static uint32_t pops(void)
{
    uint32_t i, n = 0, last = 0;
    for (i = 2; i < N; i++) {
        if (sd(i) > thr && i - last > FS * 3u / 1000u) {
            n++;
            last = i;
        }
    }
    return n;
}

int main(void)
{
    char what[200];
    uint32_t i;
    {   /* 1 */
        static int32_t o[2 * CTL];
        int z = 1;
        host_tracks_init();
        song.g[G_AMB] = 0;
        for (i = 0; i < FS / CTL; i++) {
            mix_block(o, CTL);
            for (uint32_t j = 0; j < 2u * CTL; j++)
                z &= o[j] == 0;
        }
        check("AMB 0: an empty mix stays all zero (nothing runs)", z && !amb.on);
    }
    {   /* 2 */
        double pk = 0, dc = 0, lo, hi, cl = 0, rl, rr;
        gen(127, 0);
        for (i = 0; i < N; i++) {
            pk = fmax(pk, fmax(fabs(yl[i]), fabs(yr[i])));
            dc += yl[i] + yr[i];
            cl += yl[i] * yr[i];
        }
        dc /= 2.0 * N;
        rl = rms(yl);
        rr = rms(yr);
        lo = band(yl, 200, 400);
        hi = band(yl, 2900, 3100);
        cl /= N * rl * rr;
        snprintf(what, sizeof what, "the wind: peak %.3f, DC %.4f %%, RMS L %.4f R %.4f, 200..400 Hz %.1f dB over 3 kHz, L / R corr. %.2f",
                 pk, dc * 100, rl, rr, 10 * log10(lo / hi), cl);
        check(what, pk < 0.5 && fabs(dc) < 0.001 && rl > 0.002 && fabs(20 * log10(rl / rr)) < 2.0 && lo > hi * 100 && cl < 0.95);
        thr = 2 * floor_sd();                           /* a pop: twice the wind's sharpest */
        snprintf(what, sizeof what, "  CRKL 0: no pops (%u in %u s)", pops(), SECS);
        check(what, pops() == 0u);
    }
    {   /* 3 */
        uint32_t p40, p127;
        gen(127, 40);
        p40 = pops();
        gen(127, 127);
        p127 = pops();
        snprintf(what, sizeof what, "the crackle: %.1f pops / s at CRKL 40 (~2), %.1f at 127 (~20)", (double)p40 / SECS, (double)p127 / SECS);
        check(what, p40 >= SECS * 1u && p40 <= SECS * 3u && p127 >= SECS * 14u && p127 <= SECS * 26u);
    }
    {   /* 4 */
        double a127, a64;
        gen(127, 0);
        a127 = rms(yl);
        gen(64, 0);
        a64 = rms(yl);
        snprintf(what, sizeof what, "AMB's taper: 64 is %.1f dB under 127 (a level squared: ~-12)", 20 * log10(a64 / a127));
        check(what, fabs(20 * log10(a64 / a127) + 11.9) < 1.5);
    }
    {   /* 5 */
        static int32_t l[CTL], r[CTL], rv[CTL];
        uint64_t i0;
        gen(127, 127);
        i0 = instr_now();
        for (i = 0; i < 4u * FS / CTL; i++)
            amb_block(l, r, rv, CTL);
        if (i0) {
            double c = (double)(instr_now() - i0) / (4.0 * FS);
            snprintf(what, sizeof what, "cost: %.0f instructions per sample (limit 80; a NOISE voice is more)", c);
            check(what, c <= 80);
        } else
            printf("ambience: cost: no instruction counter on this host\n");
    }
    printf("%s\n", bad ? "AMBIENCE TEST FAILED" : "ambience test passed");
    return bad != 0;
}
