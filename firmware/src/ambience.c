/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GHOULBOX (on Felucca by Leo Kuroshita, Hügelton Instruments) */
/* The ambience (G_AMB, G_CRKL, GHOULBOX): a cave's air and a torch's crackle on the mix, with no track and no voice
 * (CAVE WIND and TORCH, the NOISE sounds, take a track each). Into the dry mix and a share into the reverb's send,
 * before TAPE / CRSH and MASTER (fx.c mix_block); it plays whether the song plays or not (room tone). AMB 0: nothing
 * runs, the mix bit for bit as without it; turned on, it starts from silence.
 *   the wind     white noise, its own each side, through a band-pass (left ~200..350 Hz, right a little above), each
 *                centre on its own slow random walk (seconds), the level on a slower one (the gusts, ~-6 dB .. 0).
 *                All of it under ~1 kHz: worked out at a quarter of the rate (AMB_DIV), linear between
 *   the crackle  pops at random times, CRKL^2 up to ~20 a second (40: ~2): a 0.3 .. 1.3 ms burst of the noise's first
 *                difference (bright), panned at random, one in eight a louder snap
 * AMB is a level squared: the first half of the knob quiet. */
#define AMB_DIV 4u                                      /* the wind's rate: FS / 4 */
#define AMB_CUT_L ((43 + 28) << 8)                      /* the band-passes' centres (CUTOFF_HZ, + 28 steps: x4 for */
#define AMB_CUT_R ((45 + 28) << 8)                      /* FS / 4): ~260 Hz, ~290 Hz */
#define AMB_WALK (6 << 8)                               /* the centres wander up to +-6 steps (~+-30 %) */
static struct {
    int32_t rng, wl1, wl2, wr1, wr2, cl, cr, gust, gust_to, env, dec, gl, gr, prev, bl, br;   /* (bl, br: the wind's last) */
    uint32_t wait;
    uint8_t on;
} amb;

static void amb_block(int32_t *l, int32_t *r, int32_t *rev, uint32_t n)
{
    int32_t a = song.g[G_AMB], k = song.g[G_CRKL], lvl, g, wl = 0, wr = 0, dl = 0, dr = 0;
    tsvf_t fl, fr;
    uint32_t i;
    if (!a) {
        amb.on = 0;
        return;
    }
    if (!amb.on) {                                      /* from silence */
        memset(&amb, 0, sizeof amb);
        amb.rng = 0x1F123BB5;
        amb.gust = amb.gust_to = 24576;
        amb.on = 1;
    }
    /* once a block: the centres' walks (leaky, ~1.5 s) and the gust's (toward a new level when it gets there) */
    amb.cl = clamp(amb.cl + (int32_t)(noise32(&amb.rng) >> 25) - 64 - (amb.cl >> 11), -AMB_WALK, AMB_WALK);
    amb.cr = clamp(amb.cr + (int32_t)(noise32(&amb.rng) >> 25) - 64 - (amb.cr >> 11), -AMB_WALK, AMB_WALK);
    if (amb.gust - amb.gust_to < 256 && amb.gust_to - amb.gust < 256)
        amb.gust_to = 16384 + (int32_t)(noise32(&amb.rng) >> 18);   /* -6 dB .. 0 */
    amb.gust += (amb.gust_to - amb.gust) >> 10;
    tsvf_coef(&fl, AMB_CUT_L + amb.cl, 70);
    tsvf_coef(&fr, AMB_CUT_R + amb.cr, 70);
    lvl = a * a * 23170 / (127 * 127);                  /* 127: ~18 dB under a full track (-3 dB of unity) */
    g = mulq15(amb.gust, lvl);
    for (i = 0; i < n; i++) {
        int32_t cl = 0, cr = 0;
        if (!(i % AMB_DIV)) {                           /* the wind's next quarter-rate step (eng_gurdy.c gurdy_bp: the
                                                         * SVF's band-pass), at its level, and the way to it */
            int32_t nl = (int32_t)(noise32(&amb.rng) >> 17) - 16384, nr = (int32_t)(noise32(&amb.rng) >> 17) - 16384;
            wl = amb.bl;
            wr = amb.br;
            amb.bl = mulq15(gurdy_bp(&fl, nl, &amb.wl1, &amb.wl2), g);
            amb.br = mulq15(gurdy_bp(&fr, nr, &amb.wr1, &amb.wr2), g);
            dl = (amb.bl - wl) / (int32_t)AMB_DIV;
            dr = (amb.br - wr) / (int32_t)AMB_DIV;
        }
        wl += dl;
        wr += dr;
        if (amb.wait)
            amb.wait--;
        else if (k) {                                   /* a pop, and the time to the next (uniform, mean FS / rate) */
            uint32_t u = noise32(&amb.rng), rate = (uint32_t)(k * k) * 20u * 256u / (127u * 127u) + 1u;   /* per s, x256 */
            int32_t pan = (int32_t)(noise32(&amb.rng) >> 17);
            amb.env = 6000 + (int32_t)(u & 0x3FFFu);
            amb.dec = 32767 - 32767 / (13 + (int32_t)((u >> 14) % 45u));   /* ~0.3 .. 1.3 ms */
            amb.gl = mulq15(32767 - pan, lvl) >> ((u >> 29) ? 1 : 0);   /* one in eight a snap (louder) */
            amb.gr = mulq15(pan, lvl) >> ((u >> 29) ? 1 : 0);
            amb.prev = (int32_t)(noise32(&amb.rng) >> 17) - 16384;
            amb.wait = noise32(&amb.rng) % (2u * FS * 256u / rate) + 1u;
        }
        if (amb.env) {                                  /* the pop: the noise's first difference (bright), fading */
            int32_t x = (int32_t)(noise32(&amb.rng) >> 17) - 16384, c = mulq15(x - amb.prev, amb.env);
            amb.prev = x;
            amb.env = amb.env > 64 ? mulq15(amb.env, amb.dec) : 0;
            cl = mulq15(c, amb.gl);
            cr = mulq15(c, amb.gr);
        }
        l[i] += wl + cl;
        r[i] += wr + cr;
        rev[i] += (wl + cl + wr + cr) >> 3;             /* a share into the reverb: the same room as the tracks */
    }
}
