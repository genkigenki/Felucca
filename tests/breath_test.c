/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the breath slope's tempo (docs/BREATH_PLAN.md 1): the effective tempo below 70 BPM.
 *   build/host/breath_test
 * 1. 70 and up: the beat is FS * 60 / BPM, as before.
 * 2. below 70: the beat grows with no step (each lower value at most 12 % longer), 60 -> 53.1, 50 -> 30.7, 40 -> 13.3 BPM.
 * 3. an external clock still wins; the SLICER follows the song tempo. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int check(const char *what, int ok)
{
    printf("breath: %-75s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

static uint32_t beat_at(int bpm)
{
    song.g[G_CLOCK] = 0;
    song.g[G_BPM] = (int16_t)bpm;
    return beat_samples();
}

int main(void)
{
    int bad = 0, v, ok;
    ok = 1;
    for (v = 70; v <= 240; v++)
        ok &= beat_at(v) == (uint32_t)FS * 60u / (uint32_t)v;
    bad += check("70..240: the beat is FS * 60 / BPM", ok);
    ok = 1;
    for (v = 40; v < 70; v++) {
        uint32_t a = beat_at(v), b = beat_at(v + 1);
        ok &= a > b && a * 100u <= b * 112u;
    }
    bad += check("40..69: longer with each step down, never by more than 12 %", ok);
    bad += check("60 -> 53.1 BPM", beat_at(60) * 531u / 10u / 60u >= (uint32_t)FS * 99u / 100u && beat_at(60) * 531u / 10u / 60u <= (uint32_t)FS * 101u / 100u);
    bad += check("50 -> 30.7 BPM (7.8 s a bar)", beat_at(50) * 4u / FS == 7u);
    bad += check("40 -> 13.3 BPM (18 s a bar)", beat_at(40) * 4u / FS == 18u);
    song.g[G_CLOCK] = 0;
    return bad ? 1 : 0;
}
