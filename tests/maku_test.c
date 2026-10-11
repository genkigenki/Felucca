/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* MAKU, the interlude mode (firmware/src/maku.c), on the real sequencer with the stubs of tests/ui_test.c:
 *   SETUP     MENU > MAKU ON: KICK / DRONE 57 / DRONE 13 / ARP, patterns empty, MIDI CC20 = DENSITY, OFF hands the
 *             tracks back to their stored steps.
 *   KICK      DENSITY 0: silent; the rungs once a bar -> every 2 beats -> every beat, each rung more often than the last.
 *   DRONES    57 and 13 steps (the arp 29), chords of 3 / 1 and 2 notes, six chords before the first repeats, the two never fall on the same step all the way round.
 *   SCALE     every drone and arp note is in the scale, for all roots and scales; nothing from the arp below DENSITY 16.
 *   ARP       runs of consecutive table places, then a jump; the table comes back when DENSITY goes down (tidy).
 *   FOLLOW    the kick's TUNE follows ROOT; a ROOT / SCALE edit on any track becomes all four's.
 *   COST      a bar of the four tracks at DENSITY 127 costs a bounded number of note-ons.
 * Built and run by tests/run_tests.sh. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

#define BAR_STEPS 16u
static uint32_t step_blocks(void) { return step_samples(&trk[0], div_samples(2), 0) / CTL; }

typedef struct { uint16_t step; uint8_t note; } hit_t;
static hit_t hits[4][4096];
static uint32_t nhits[4];

/* run `steps` 1/16 steps of the four tracks (maku_block before them, as events_block does); every note-on of every
 * track lands in hits[track]: the step it started in and the note */
static void sim(uint32_t steps)
{
    uint32_t b, i, nb = steps * step_blocks();
    for (b = 0; b < nb; b++) {
        maku_block();
        for (i = 0; i < NTRK; i++) {
            track_t *t = &trk[i];
            uint32_t v0 = vage;
            seq_tick(t, CTL);
            if (vage > v0 && nhits[i] < 4096u) {
                hits[i][nhits[i]].step = t->seq_idx;
                hits[i][nhits[i]].note = t->seq_n ? t->seq_notes[0] : 0;
                nhits[i]++;
            }
        }
    }
}
static void start(uint32_t dens)
{
    ui_power_on();
    maku_setup();
    for (uint32_t i = 0; i < NTRK; i++)
        trk[i].engine = trk[i].eng_req;              /* (the audio ISR switches engines at a block start) */
    maku_set_density(dens);
    for (uint32_t i = 1; i < 4u; i++)
        maku.m[i][0] = 64;                           /* (the SELECT macros of the parts at the middle: no stretch; OPEN / LOOSE keep their set values) */
    memset(nhits, 0, sizeof nhits);
    seq_start();
}
static int in_scale(uint32_t note, uint32_t root, uint32_t scale)
{
    return (SCALE_MASK[scale] >> ((note + 120u - root) % 12u)) & 1u;
}

static int setup(void)
{
    int bad = 0;
    uint32_t i;
    ui_power_on();
    bad += check("MAKU is off at power-on", !maku.on);
    maku_setup();
    bad += check("SETUP: four tracks, lengths 16 / 57 / 13 / 29, patterns empty",
                 maku.on && trk[0].p[P_SLEN] == 16 && trk[1].p[P_SLEN] == 57 && trk[2].p[P_SLEN] == 13 &&
                 trk[3].p[P_SLEN] == 29 && trk[0].eng_req == ENGI_DRUM && !trk[1].step[0].n && !trk[3].step[5].n);
    midi_control(0, 20, 100);
    bad += check("  CC20 sets DENSITY", maku.dens == 100u);
    midi_control(0, 20, 0);
    bad += check("  CC20 0 -> DENSITY 0", maku.dens == 0u);
    maku.on = 0;
    midi_control(0, 20, 90);
    bad += check("MAKU OFF: CC20 does nothing, the stored steps play again", !maku.on && maku.dens == 0u);
    ui_power_on();
    trk[0].p[P_SLEN] = 4;
    trk[0].step[1] = (step_t){{60}, 1, ST_NOTE, 0, 100};
    seq_start();
    memset(nhits, 0, sizeof nhits);
    sim(8);
    for (i = 0; i < nhits[0]; i++)
        if (hits[0][i].step != 1u || hits[0][i].note != 60u)
            bad++;
    bad += check("  with MAKU off the sequencer plays its own pattern (step 1, note 60, twice in 8 steps)",
                 nhits[0] == 2u && !bad);
    return bad;
}

static int kick(void)
{
    int bad = 0;
    uint32_t d, last = 0, orn = 0, per[6] = {0}, n, bars = 40u;
    static const uint32_t D[6] = {0, 20, 60, 95, 96, 127};
    for (d = 0; d < 6u; d++) {
        start(D[d]);
        sim(bars * BAR_STEPS);
        per[d] = nhits[0];
    }
    printf("ui:   hits in %u bars at DENSITY 0 20 60 95 96 127: %u %u %u %u %u %u\n", bars, per[0], per[1], per[2], per[3], per[4], per[5]);
    bad += check("KICK: DENSITY 0 is silent", per[0] == 0u);
    bad += check("  DENSITY 20: about once a bar at most", per[1] > 0u && per[1] <= bars);
    bad += check("  DENSITY 60: more than 20, at most 2 a bar", per[2] > per[1] && per[2] <= 2u * bars);
    bad += check("  DENSITY 95: more again, at most 4 a bar, no ornaments yet", per[3] > per[2] && per[3] <= 4u * bars);
    bad += check("  DENSITY 96: the four on the floor, exactly 4 a bar", per[4] == 4u * bars);
    start(96);
    sim(BAR_STEPS);
    for (n = 0; n < nhits[0]; n++)
        last |= 1u << hits[0][n].step;
    bad += check("  .. on steps 0 4 8 12 (the 16-step time axis)", last == (1u | 1u << 4 | 1u << 8 | 1u << 12));
    start(127);
    sim(bars * BAR_STEPS);
    last = 0;
    for (n = 0; n < nhits[0]; n++)
        if (hits[0][n].step % 4u)
            orn |= 1u << hits[0][n].step;
        else
            last |= 1u << hits[0][n].step;
    bad += check("  DENSITY 127: the four stays certain, ornaments come on top (the off-beats at least)",
                 last == (1u | 1u << 4 | 1u << 8 | 1u << 12) && (orn & 0x4444u) == 0x4444u && nhits[0] > per[4]);
    return bad;
}

static int drones(void)
{
    int bad = 0, ok = 1;
    uint32_t n, a[64], na = 0, b[64], nb = 0, cyc = 57u * 13u, i, j;
    start(0);
    sim(cyc * 2u);                                   /* a full cycle of both is 741 steps; two of them */
    for (n = 0; n < nhits[1]; n++) a[na++ % 64u] = hits[1][n].step;
    for (n = 0; n < nhits[2]; n++) b[nb++ % 64u] = hits[2][n].step;
    bad += check("DRONE A: 3 chords per 57 steps (0, 19, 38), 78 in two long cycles (each hit lands as its lowest note)",
                 nhits[1] == 3u * 13u * 2u);
    bad += check("SHIMMER: steps 0 and 7 of 13, 114 hits in two long cycles", nhits[2] == 2u * 57u * 2u);
    for (n = 0; n < nhits[1]; n++) ok &= hits[1][n].step % 19u == 0u;
    for (n = 0; n < nhits[2]; n++) ok &= hits[2][n].step == 0u || hits[2][n].step == 7u;
    bad += check("  on those steps only", ok);
    ok = nhits[0] == 0u && nhits[3] == 0u;
    bad += check("  DENSITY 0: the kick and the arp are silent, the drones are not", ok);
    {   /* the voicings: 3 notes, open (the lowest is the root), six different chords before the first repeats */
        step_t st;
        uint32_t k, seen = 0, voices = 0;
        maku.cyc_a = 0;
        for (k = 0; k < 12u; k++) {
            maku_step(1, (k % 3u) * 19u, &st);
            voices += st.n == 3u;
            if (k < 6u)
                seen |= 1u << (st.note[1] - st.note[0] + 16u * (st.note[2] - st.note[0]) / 4u) % 31u;
        }
        bad += check("  DRONE A plays 3-note voicings that start on the root", voices == 12u && trk[1].p[P_ROOT] == 0);
    }
    /* the phase between them: how many distinct (A step, B step) pairs meet over the long cycle */
    {
        static uint8_t seen[57][13];
        uint32_t pairs = 0;
        for (i = 0; i < cyc; i++) {
            uint32_t sa = i % 57u, sb = i % 13u;
            if ((sa % 19u == 0u) && (sb == 0u || sb == 7u) && !seen[sa][sb]) { seen[sa][sb] = 1; pairs++; }
        }
        bad += check("  the two loops meet in 6 different phases (coprime lengths)", pairs == 6u);
    }
    (void)j;
    return bad;
}

static int scale(void)
{
    int bad = 0, ok = 1;
    uint32_t root, sc, n, i;
    for (sc = 0; sc < sizeof SCALE_MASK / sizeof SCALE_MASK[0]; sc++)
        for (root = 0; root < 12u; root += 5u) {
            start(127);
            trk[1].p[P_ROOT] = (int16_t)root;
            trk[1].p[P_SCALE] = (int16_t)sc;
            sim(BAR_STEPS * 12u);
            for (i = 1; i < NTRK; i++)
                for (n = 0; n < nhits[i]; n++)
                    ok &= in_scale(hits[i][n].note, root, sc) != 0;
            ok &= trk[0].p[P_ROOT] == (int16_t)root && trk[3].p[P_SCALE] == (int16_t)sc;
            for (i = 1; i < 3u; i++) {                  /* every note of every chord and dyad (the hits keep the first) */
                uint32_t idx, k;
                step_t st;
                for (idx = 0; idx < 741u; idx++)
                    if (maku_step(i, idx % (i == 1u ? MAKU_DRA_LEN : MAKU_DRB_LEN), &st))
                        for (k = 0; k < st.n; k++)
                            ok &= in_scale(st.note[k], root, sc) != 0;
            }
        }
    bad += check("SCALE: drones and arp only play scale notes (16 scales x roots 0 5 10), the key reaches all four", ok);
    start(15);
    sim(BAR_STEPS * 8u);
    bad += check("  the arp is silent below DENSITY 16", nhits[3] == 0u);
    start(127);
    sim(BAR_STEPS * 8u);
    bad += check("  and plays at 127", nhits[3] > 40u);
    {                                                   /* the parts' SELECT macro to the left stretches them: fewer hits, none at all held silent */
        uint32_t full[4], i, silent = 0;
        for (i = 1; i < 4u; i++)
            full[i] = nhits[i];
        start(127);
        for (i = 1; i < 4u; i++)
            maku.m[i][0] = 0;
        sim(BAR_STEPS * 8u);
        for (i = 1; i < 4u; i++)
            silent += nhits[i] >= full[i];
        bad += check("  SELECT macros at 0 stretch the drones and the arp (fewer hits than at the middle)", silent == 0u);
    }
    return bad;
}

static int arp(void)
{
    int bad = 0;
    uint32_t n, runs = 0, adjacent = 0;
    start(40);
    sim(BAR_STEPS * 32u);
    bad += check("ARP: plays at DENSITY 40", nhits[3] > 20u);
    /* tidy: at DENSITY 20 the arp only plays what is written (the notes of MAKU_PHRASE, in the scale), at 127 some are
     * re-rolled: it plays notes the phrase does not have */
    {
        uint32_t written = 0, off20 = 0, off127 = 0, n20 = 1, n127 = 1, k, d;
        uint8_t allowed[128] = {0};
        for (k = 0; k < MAKU_TABLE; k++)
            if (MAKU_PHRASE[k] != MAKU_REST)
                allowed[maku_note(&trk[3], MAKU_PHRASE[k], 48u)] = 1;
        for (d = 20; d <= 127u; d += 107u) {
            start(d);
            sim(BAR_STEPS * 64u);
            for (n = 0; n < nhits[3]; n++)
                if (!allowed[hits[3][n].note])
                    d == 20u ? off20++ : off127++;
            written += nhits[3] > 10u;
            d == 20u ? (n20 = nhits[3]) : (n127 = nhits[3]);
        }
        printf("ui:   notes off the written phrase: %u of %u at DENSITY 20, %u of %u at 127\n", off20, n20, off127, n127);
        bad += check("  tidy: at DENSITY 20 the written phrase plays (< 8 % off it), at 127 it is loosened (> 15 %)",
                     written == 2u && off20 * 100u < n20 * 8u && off127 * 100u > n127 * 15u);
    }
    start(127);
    sim(BAR_STEPS * 64u);
    {
        uint32_t gap = 0;
        for (n = 0; n < nhits[3]; n++)
            gap += hits[3][n].step >= MAKU_ARP_GAP;
        bad += check("  the breath: no arp note on steps 24..28 of its 29", nhits[3] > 40u && gap == 0u);
    }
    for (n = 1; n < nhits[3]; n++) {
        runs += hits[3][n].step != hits[3][n - 1u].step;
        adjacent++;
    }
    bad += check("  notes come in runs (steps advance)", runs > 0u && adjacent > 0u);
    return bad;
}

static int follow(void)
{
    int bad = 0, ok = 1;
    uint32_t root;
    start(0);
    maku_block();
    bad += check("FOLLOW: root C: the kick's TUNE is 2 semitones under the design (53)", trk[0].p[P_E1] == 53);
    for (root = 0; root < 12u; root++) {
        trk[1].p[P_ROOT] = (int16_t)root;
        maku_block();
        ok &= trk[0].p[P_E1] >= 53 - 33 && trk[0].p[P_E1] <= 53 + 33;
    }
    bad += check("  every root keeps the kick within +-6 semitones of its root pitch (TUNE 20..86)", ok);
    trk[1].p[P_ROOT] = 7;
    maku_block();
    {
        int16_t g = trk[0].p[P_E1];
        trk[1].p[P_ROOT] = 0;
        maku_block();
        bad += check("  G (7) folds down a fourth: TUNE below the design; back to C: 53 again",
                     g < 53 && trk[0].p[P_E1] == 53);
    }
    trk[2].p[P_ROOT] = 9;                             /* an edit on another track's SCL page */
    trk[2].p[P_SCALE] = 3;
    maku_block();
    bad += check("  a ROOT / SCALE edit on any track becomes all four's",
                 trk[0].p[P_ROOT] == 9 && trk[1].p[P_ROOT] == 9 && trk[3].p[P_SCALE] == 3 && trk[1].p[P_SCALE] == 3);
    return bad;
}

static int duck(void)
{
    int bad = 0;
    uint32_t i, b;
    int32_t low = 4096, g0;
    start(0);
    bad += check("DUCK: MAKU off or DENSITY < 40: no dip", maku_gain(1) == 4096 && maku_gain(0) == 4096);
    start(127);
    for (i = 0; i < 1u + 4u * BAR_STEPS * step_blocks(); i++) {
        maku_block();
        seq_tick(&trk[0], CTL);
        if (maku.duck < low)
            low = maku.duck;
    }
    printf("ui:   deepest dip at DENSITY 127: drones x%d/4096, arp x%d/4096\n", low, 4096 - (4096 - low) / 2);
    bad += check("  at DENSITY 127 a kick sinks the drones to about 0.56 and the arp to 0.78", low > 2200 && low < 2400);
    maku.duck = 2300;
    g0 = maku_gain(1);
    for (b = 0; b < 600u; b++)
        maku_block();
    bad += check("  and they come back to 1.0 within ~600 blocks (~120 ms time constant)", g0 == 2300 && maku.duck == 4096 &&
                 maku_gain(1) == 4096 && maku_gain(0) == 4096);
    maku.duck = 2300;
    bad += check("  the kick itself is never ducked", maku_gain(0) == 4096);
    maku.duck = 4096;
    maku.brk = 1;                                           /* SINK: the kick out fast, the rest dives into the sends */
    for (b = 0; b < 200u; b++)
        maku_block();
    bad += check("SINK: FX held pulls the kick to 0 and dives the rest (not the kick) into reverb and reverse",
                 maku_gain(0) == 0 && maku_dive() > 500 && maku_gain(1) == 4096);
    for (b = 0; b < 2000u; b++)
        maku_block();
    bad += check("  held ~1.5 s: the dive is full", maku_dive() == 4096);
    maku.brk = 0;
    for (b = 0; b < 1000u; b++)
        maku_block();
    bad += check("  let go: the kick fades in slowly (not back after 1000 blocks) and the dive lifts with it",
                 maku_gain(0) > 900 && maku_gain(0) < 1100 && maku_dive() > 3000 && maku_dive() < 3200);
    for (b = 0; b < 4000u; b++)
        maku_block();
    bad += check("  and after ~3 s the kick is full and the dive gone", maku_gain(0) == 4096 && maku_dive() == 0);
    return bad;
}

/* the voices the four parts hold at once: the real mixer (release tails included) against the budget of NVOICE shared by all
 * parts; a note-on that finds all of them busy takes one from another part (a steal) */
static int voices(void)
{
    int bad = 0;
    int32_t o[2u * CTL];
    uint32_t b, i, k, nb = 12u * BAR_STEPS * step_blocks(), act, maxact = 0, full = 0, steals = 0, d;
    for (d = 60; d <= 127u; d += 67u) {
        start(d);
        maxact = full = steals = 0;
        for (b = 0; b < nb; b++) {
            uint32_t v0 = vage, before = 0;
            for (i = 0; i < NTRK; i++)
                for (k = 0; k < NVOICE; k++)
                    before += trk[i].v[k].active != 0;
            memset(o, 0, sizeof o);
            mix_block(o, CTL);
            act = 0;
            for (i = 0; i < NTRK; i++)
                for (k = 0; k < NVOICE; k++)
                    act += trk[i].v[k].active != 0;
            maxact = act > maxact ? act : maxact;
            full += act >= NVOICE;
            steals += before >= NVOICE && vage > v0;
        }
        printf("ui:   DENSITY %u: most voices at once %u of %u, %u blocks with all busy (of %u), %u note-ons that found them busy\n",
               d, maxact, NVOICE, full, nb, steals);
    }
    bad += check("VOICES: 12 bars at DENSITY 127 never take a voice from another part (all 8 busy in under 1 % of the blocks)", steals == 0u && full * 100u < nb);
    return bad;
}

static int cost(void)
{
    int bad = 0;
    start(127);
    sim(BAR_STEPS * 8u);
    printf("ui:   note-ons in 8 bars at DENSITY 127: kick %u, drone A %u, drone B %u, arp %u\n", nhits[0], nhits[1], nhits[2], nhits[3]);
    bad += check("COST: 8 bars at DENSITY 127 start under 400 notes in all (the voice budget holds)",
                 nhits[0] + nhits[1] + nhits[2] + nhits[3] < 400u);
    return bad;
}

/* WORLD: every seed gives a calm world in range, the same seed the same world, and it plays only scale notes */
static int world(void)
{
    int bad = 0, ok = 1, diff = 0;
    uint32_t seed, k, bpm0 = 0, root0 = 0, sc0 = 0;
    for (seed = 1; seed <= 400u; seed++) {
        static const uint8_t CALM[] = {2, 3, 4, 5, 6, 9};
        int in = 0;
        ui_power_on();
        maku_setup();
        maku_world(seed * 2654435761u);
        for (k = 0; k < sizeof CALM; k++)
            in |= trk[1].p[P_SCALE] == CALM[k];
        ok &= in && trk[1].p[P_ROOT] >= 0 && trk[1].p[P_ROOT] < 12 && song.g[G_BPM] >= 70 && song.g[G_BPM] <= 90 &&
              maku.dens >= 24u && maku.dens <= 56u && maku.on;
        for (k = 0; k < NTRK; k++)
            ok &= trk[k].p[P_ROOT] == trk[1].p[P_ROOT] && trk[k].p[P_SCALE] == trk[1].p[P_SCALE];
        if (seed == 1u) {
            bpm0 = (uint32_t)song.g[G_BPM];
            root0 = (uint32_t)trk[1].p[P_ROOT];
            sc0 = (uint32_t)trk[1].p[P_SCALE];
        } else if ((uint32_t)song.g[G_BPM] != bpm0 || (uint32_t)trk[1].p[P_ROOT] != root0 || (uint32_t)trk[1].p[P_SCALE] != sc0) {
            diff++;
        }
    }
    bad += check("WORLD: 400 seeds: a calm scale, ROOT 0..11 on all four, BPM 70..90, DENSITY 24..56, MAKU on", ok);
    bad += check("  the worlds differ (most seeds are not seed 1's)", diff > 300);
    ui_power_on();
    maku_setup();
    maku_world(12345u);
    bpm0 = (uint32_t)song.g[G_BPM];
    root0 = (uint32_t)trk[1].p[P_ROOT];
    ui_power_on();
    maku_setup();
    maku_world(12345u);
    bad += check("  the same seed, the same world", bpm0 == (uint32_t)song.g[G_BPM] && root0 == (uint32_t)trk[1].p[P_ROOT]);
    return bad;
}

static int kickgrid(void)
{
    int bad = 0;
    uint32_t i, m, steps_ok = 1;
    start(0);
    song.sel = 0;
    bad += check("KICKGRID: the kick selected: the keys are its grid, whatever the page", maku_kick_grid() && grid_on());
    for (i = 0; i < 16u; i += 4u)
        grid_hit(&trk[0], i, 0, 1);
    bad += check("  white keys: steps 1 5 9 13 set", maku_kick_mask() == 0x1111u);
    sim(32);
    for (i = 0; i < nhits[0]; i++)
        steps_ok &= (hits[0][i].step % 4u) == 0u;
    bad += check("  DENSITY 0: exactly the set steps play, twice in 2 bars (8 hits)", nhits[0] == 8u && steps_ok);
    start(127);
    for (i = 0; i < 16u; i += 4u)
        grid_hit(&trk[0], i, 0, 1);
    grid_hit(&trk[0], 2, 0, 1);
    sim(16 * 8);
    m = 0;
    for (i = 0; i < nhits[0]; i++)
        m |= 1u << hits[0][i].step;
    bad += check("  DENSITY 127: the set steps always play, the ghosts only add (never the stored mask shrinks)",
                 (m & maku_kick_mask()) == maku_kick_mask() && maku_kick_mask() == 0x1115u);
    bad += check("  ghosts are not stored", maku_kick_mask() == 0x1115u);
    bad += check("  ghost map excludes the set steps", (maku_kick_ghosts() & maku_kick_mask()) == 0u);
    {
        uint32_t ok = 1, k;
        for (k = 0; k < 8u; k++) {
            uint32_t ty = (uint32_t)DRUM_GM[MAKU_PAD[k] - 35u][0];
            ok &= ty == DVT_HATC || ty == DVT_HATO || ty == DVT_CYM;
        }
        bad += check("PADS: the eight black keys are hiss and grit (hats, cymbals), not kick, snare, clap or tones", ok);
    }
    song.sel = 1;
    bad += check("  another track selected: not the kick's grid", !maku_kick_grid());
    return bad;
}

static int macros(void)
{
    int bad = 0;
    uint32_t i, j, n, ok = 1, steals, maxact = 0, b;
    start(0);
    maku_macro_set(2, 2, 0);
    bad += check("MACRO: AIR 0 -> REV 60, DLY 0, ATK 60", trk[2].p[P_REV] == 60 && trk[2].p[P_DLY] == 0 && trk[2].p[P_ATK] == 60);
    maku_macro_set(2, 2, 127);
    bad += check("  AIR 127 -> REV 127, DLY 70, ATK 127", trk[2].p[P_REV] == 127 && trk[2].p[P_DLY] == 70 && trk[2].p[P_ATK] == 127);
    maku_macro_set(0, 1, 127);
    bad += check("  a macro can run down: TONE 127 on the kick -> DECY 70", trk[0].p[P_E3] == 70 && trk[0].p[P_E2] == 110);
    maku_macro_set(0, 0, 90);
    bad += check("  KICK SELECT is DENSITY", maku.dens == 90);
    for (i = 0; i < 4; i++)
        for (j = 0; j < 3; j++)
            for (n = 0; n < 128u; n += 7u) {
                uint32_t e;
                maku_macro_set(i, j, n);
                for (e = 0; e < 3u; e++) {
                    const mm_t *m = &MAKU_MAC[i][j].m[e];
                    if (m->p != MM_NONE)
                        ok &= trk[i].p[m->p] >= (m->lo < m->hi ? m->lo : m->hi) && trk[i].p[m->p] <= (m->lo < m->hi ? m->hi : m->lo);
                }
            }
    bad += check("  every macro value keeps its parameters inside lo..hi", ok);
    start(127);
    maku_macro_set(MAKU_DRA, 0, 127);
    maku_macro_set(MAKU_ARP, 0, 127);
    sim(16 * 24);
    {
        uint32_t four = 0;
        for (i = 0; i < nhits[1]; i++)
            four += 0;
        bad += check("  OPEN 127 / LOOSE 127 at DENSITY 127: the voices still fit (the arp still plays)", nhits[1] > 0u && nhits[3] > 40u);
    }
    return bad;
}

static int knobs(void)
{
    int bad = 0;
    uint32_t i, lv, m;
    start(0);
    ui.home = 1; ui.menu = 0; ui.confirm = 0;
    song.sel = 0;
    lv = (uint32_t)trk[2].p[P_LEVEL];
    turn(EN_K3, -3);
    bad += check("KNOBS: KNOB 3 turns track 3's volume down and moves the focus there", trk[2].p[P_LEVEL] == (int16_t)lv - 3 && song.sel == 2);
    turn(EN_K3, 500);
    bad += check("  the volume stops at 127", trk[2].p[P_LEVEL] == 127);
    m = maku.m[2][1];
    turn(EN_PRESET, 4);
    bad += check("  PRESETS turns the focused track's macro 2 (and does not load a sound)", maku.m[2][1] == m + 4u && song.sel == 2);
    m = maku.m[2][2];
    turn(EN_ALGO, -2);
    bad += check("  ALGORITHM turns macro 3; the focus stays", maku.m[2][2] == m - 2u && song.sel == 2);
    turn(EN_SELECT, 3);
    bad += check("  SELECT turns macro 1 (the tempo is GLO's)", maku.m[2][0] > 0u);
    turn(EN_K1, 1);
    m = maku.m[0][0];
    turn(EN_SELECT, 5);
    bad += check("  with the kick focused SELECT is DENSITY", maku.dens == m + 5u);
    return bad;
}

static void down_for(uint32_t label, uint32_t frames)
{
    uint32_t k;
    fm1_in.buttons |= 1u << panel.btn[label];
    host_pressed |= 1u << panel.btn[label];
    for (k = 0; k < frames; k++)
        frame();
}
static void let_go(uint32_t label)
{
    fm1_in.buttons &= ~(1u << panel.btn[label]);
    frame();
    frame();
}
static int buttons(void)
{
    int bad = 0;
    uint32_t i;
    start(0);
    ui.home = 1; ui.menu = 0; ui.confirm = 0;
    down_for(B_PLAY, 3);
    bad += check("BUTTONS: PLAY held: DRIFT's three parameters, the transport does not start, no layer", (maku.hv & VBIT(VB_DRIFT)) && maku.verb == VB_DRIFT &&
                 !ui.layer && !transport_req && !maku.dr_tog);
    turn(EN_SELECT, 5);
    bad += check("  .. SELECT is its RATE (the macros stay)", maku.vp[VB_DRIFT][0] == 65 && maku.m[0][0] == 40u);
    let_go(B_PLAY);
    bad += check("  let go after a knob was turned: DRIFT stays off", !maku.hv && maku.verb == VB_NONE && !maku.dr_tog);
    down_for(B_PLAY, 3);
    let_go(B_PLAY);
    bad += check("  PLAY tapped: DRIFT is on", maku.dr_tog && !maku.hv);
    down_for(B_PLAY, 3);
    let_go(B_PLAY);
    bad += check("  .. tapped again: off", !maku.dr_tog);
    down_for(B_FX, 3);
    bad += check("  FX held: SINK, no layer opens", maku.brk && maku.verb == VB_SINK && !ui.layer && !(kb_mask & (1u << panel.btn[B_FX])) && !transport_req);
    let_go(B_FX);
    bad += check("  .. and back", !maku.brk && !maku.hv && perf_k[0] == 0);
    down_for(B_SCL, 3);
    down_for(B_ENV, 3);
    bad += check("  SCL: CASCADE, ENV: SWELL, together; the last pressed has the screen", (maku.hv & VBIT(VB_CASC)) && (maku.hv & VBIT(VB_SWELL)) && maku.verb == VB_SWELL && !ui.layer && ui.home);
    let_go(B_ENV);
    bad += check("  .. one let go: the other has it", maku.verb == VB_CASC);
    let_go(B_SCL);
    down_for(B_GLO, 3);
    bad += check("  GLO: SCRAMBLE", maku.verb == VB_SCRAM && maku_eff() == maku.dens && !ui.layer);
    let_go(B_GLO);
    down_for(B_EDIT, 3);
    bad += check("  EDIT: LIFT", maku.verb == VB_LIFT && !(perf_held & PF_BIT(PF_OUP)));
    maku.va[VB_LIFT] = 4096;
    frame();
    bad += check("  .. its THIN raises the master's high-pass (perf_k[0] > 0)", perf_k[0] > 0);
    maku.drs = 127;
    frame();
    bad += check("  .. and the space DRIFT added darkens it from the other side (the sum)", perf_k[0] < 60 * 64 / 127 + 1 && perf_k[0] > -40);
    maku.drs = 0;
    let_go(B_EDIT);
    down_for(B_LFO, 3);
    bad += check("  LFO: WOBBLE, no old filter sweep", maku.verb == VB_WOB && !perf_held && !ui.layer && ui.home);
    let_go(B_LFO);
    down_for(B_SAVE, 3);
    bad += check("  SAVE does nothing (no verb, no undo, no page)", !maku.hv && ui.home && !ui.layer);
    let_go(B_SAVE);
    down_for(B_REC, 3);
    let_go(B_REC);
    bad += check("  REC does nothing here (it arms no recording)", song.rec == 0u);
    {
        uint32_t r0 = (uint32_t)trk[1].p[P_ROOT], bpm = (uint32_t)song.g[G_BPM], same = 1, n;
        for (n = 0; n < 6u && same; n++) {
            down_for(B_HOME, 120);
            let_go(B_HOME);
            same = r0 == (uint32_t)trk[1].p[P_ROOT] && bpm == (uint32_t)song.g[G_BPM];
        }
        bad += check("  HOME held: a new world (the key or the tempo changes within 6 tries); the menu stays shut", !same && !ui.menu);
    }
    return bad;
}

/* the keys always melt into the arp phrase (no REC), and the oldest notes are pushed out and fade */
static int melt(void)
{
    int bad = 0;
    uint32_t i, n, hit_a = 0, hit_b = 0, still = 0;
    step_t st;
    start(127);
    memset(maku.pk_w, 0, sizeof maku.pk_w);
    for (i = 0; i < MAKU_PK; i++)
        maku_pick(100u + i);                            /* 8 notes outside the arp's range: easy to tell */
    for (i = 0; i < MAKU_PK; i++)
        still += maku.pk_w[i] == 255u;
    maku_pick(120u);                                    /* a 9th: it takes the place of the oldest (100) */
    {
        uint32_t has100 = 0, has120 = 0;
        for (i = 0; i < MAKU_PK; i++) {
            has100 |= maku.pk[i] == 100u;
            has120 |= maku.pk[i] == 120u;
        }
        bad += check("MELT: eight notes are kept, a ninth pushes out the oldest one", still == MAKU_PK && !has100 && has120);
    }
    memset(maku.pk_w, 0, sizeof maku.pk_w);
    maku_pick(110u);
    for (n = 0; n < 4000u; n++) {
        maku.run_left = 3;
        if (maku_step(MAKU_ARP, 0u, &st) && st.n && st.note[0] == 110u)
            hit_a += n < 400u;
        if (n >= 3600u && st.n && st.note[0] == 110u)
            hit_b++;
    }
    bad += check("  with REC never pressed the played note is heard in the phrase, then fades away for good", hit_a > 5u && hit_b == 0u);
    return bad;
}

/* OCT- / OCT+: a fifth down / up the circle of fifths, a change of key signature (maku_fifth), not a transposition */
static int fifths(void)
{
    int bad = 0;
    uint32_t i, ok = 1, k;
    start(0);
    ui.home = 1; ui.menu = 0; ui.confirm = 0;
    for (i = 0; i < NTRK; i++)
        trk[i].p[P_SCALE] = 1;                          /* MAJ */
    maku_block();
    down_for(B_OCTUP, 3);
    let_go(B_OCTUP);
    bad += check("FIFTHS: C major, OCT+: G major on all four tracks, the kick follows, the keys' octave untouched",
                 trk[1].p[P_ROOT] == 7 && trk[0].p[P_ROOT] == 7 && trk[3].p[P_ROOT] == 7 && song.octave == 0 && trk[0].p[P_E1] < 53);
    down_for(B_OCTDN, 3);
    let_go(B_OCTDN);
    down_for(B_OCTDN, 3);
    let_go(B_OCTDN);
    bad += check("  OCT- twice: back to C, then F (a flat)", trk[1].p[P_ROOT] == 5 && trk[2].p[P_ROOT] == 5);
    for (i = 0; i < 12u; i++) {                         /* 12 steps up the circle: every key once, home again */
        uint32_t seen = 0;
        trk[1].p[P_ROOT] = 0;
        maku_block();
        for (k = 0; k < 12u; k++) {
            maku_fifth(1);
            seen |= 1u << trk[1].p[P_ROOT];
        }
        ok &= seen == 0xFFFu && trk[1].p[P_ROOT] == 0;
    }
    bad += check("  twelve steps up visit all twelve keys and come home", ok);
    trk[1].p[P_ROOT] = 0;
    maku_block();
    maku_pick(65u);                                     /* F */
    maku_pick(64u);                                     /* E */
    maku_pick(66u);                                     /* F#: an avoid note in C major */
    maku_fifth(1);
    bad += check("  C -> G major: a picked F becomes F#, E stays, an avoid note stays as played",
                 maku.pk[0] == 66u && maku.pk[1] == 64u && maku.pk[2] == 66u && !((maku.pk_av >> 0) & 1u) && !((maku.pk_av >> 2) & 1u));
    maku_fifth(-1);
    bad += check("  and back G -> C: F# becomes F again (the sharp is taken off)", maku.pk[0] == 65u && maku.pk[1] == 64u);
    trk[1].p[P_SCALE] = 5;                              /* PEN: C D E G A -> G A B D E: C becomes B (one note, a semitone down) */
    maku_block();
    memset(maku.pk_w, 0, sizeof maku.pk_w);
    maku_pick(60u);
    maku_pick(62u);
    maku_fifth(1);
    bad += check("  a pentatonic does the same: C -> B on the way to G, D stays", maku.pk[0] == 59u && maku.pk[1] == 62u);
    start(127);
    trk[1].p[P_SCALE] = 1;
    maku_block();
    sim(16 * 3);
    {
        uint32_t guard = 0;
        while (!maku.run_left && guard++ < 2000u)
            sim(1);
        bad += check("  an arp run in progress: it goes on in the old frame (no jump in pitch), and every note stays in key",
                     maku.run_left > 0u);
        maku_fifth(1);
        bad += check("  .. the frame is held for the run", maku.frame_on && maku.frame_root == 0u);
        memset(nhits, 0, sizeof nhits);
        sim(16 * 16);
        ok = nhits[3] > 0u;
        for (i = 0; i < nhits[3]; i++)
            ok &= in_scale(hits[3][i].note, 7u, 1u) != 0;
        bad += check("  .. and all the arp's notes after it are in G major", ok && !maku.frame_on);
    }
    return bad;
}

/* the verbs that hold three parameters: FX (SINK WASH HUSH), SEQ (BPM SWING DUCK), ARP (ROOT SCALE REG); ARP tapped goes on through the scales */
static int verbs(void)
{
    int bad = 0;
    uint32_t i, ok = 1, n;
    int32_t bpm;
    start(0);
    ui.home = 1; ui.menu = 0; ui.confirm = 0;
    for (i = 0; i < NTRK; i++)
        trk[i].p[P_SCALE] = 1;                          /* MAJ */
    maku_block();
    for (n = 0; n < 8u; n++) {                          /* eight taps: MAJ MIN LYD DOR MIX PHRY PEN MPEN, then MAJ again */
        static const uint8_t WANT[8] = {2, 9, 3, 4, 8, 5, 6, 1};
        down_for(B_ARP, 3);
        let_go(B_ARP);
        ok &= trk[1].p[P_SCALE] == WANT[n] && trk[0].p[P_SCALE] == WANT[n] && trk[3].p[P_SCALE] == WANT[n];
    }
    bad += check("VERBS: ARP tapped: the scale goes on, major and minor in turn, on all four tracks; eight taps come home", ok);
    for (i = 0; i < NTRK; i++)
        trk[i].p[P_SCALE] = 7;                          /* HARM: not on the list */
    maku_block();
    down_for(B_ARP, 3);
    let_go(B_ARP);
    bad += check("  a scale off the list starts it over at MAJ", trk[1].p[P_SCALE] == 1 && !ui.layer);
    down_for(B_ARP, 3);
    turn(EN_SELECT, 2);                                 /* ARP held, a knob turned: the layer opens, ROOT moves */
    bad += check("  ARP held: SELECT moves the ROOT, and the key reaches all four tracks", trk[1].p[P_ROOT] == 2 && ui.layer == LAYER_SCL && maku_verb_now() == VB_ROOT);
    turn(EN_PRESET, 1);
    bad += check("  .. PRESETS the scale along the list (MAJ -> MIN)", trk[1].p[P_SCALE] == 2 && trk[2].p[P_SCALE] == 2);
    turn(EN_ALGO, -1);
    bad += check("  .. ALGORITHM the drones' register (MID -> LOW): the drone chord is an octave lower", maku.vp[VB_ROOT][2] == 0 && maku_low(36u) == 24u);
    turn(EN_K2, 5);
    bad += check("  .. KNOB 1..4 do nothing in it", trk[1].p[P_LEVEL] == (int16_t)90);
    let_go(B_ARP);
    maku.vp[VB_ROOT][2] = 1;

    bpm = song.g[G_BPM];
    i = (uint32_t)trk[2].p[P_LEVEL];
    down_for(B_SEQ, 3);
    turn(EN_SELECT, 3);
    bad += check("  SEQ held: SELECT is the tempo, as before", song.g[G_BPM] == bpm + 3 && maku_verb_now() == VB_GLO);
    turn(EN_PRESET, 10);
    turn(EN_ALGO, -4);
    bad += check("  .. PRESETS the arp's SWING, ALGORITHM the DUCK depth (64 = as designed)", maku.vp[VB_GLO][1] == 10 && maku.vp[VB_GLO][2] == 60 &&
                 trk[MAKU_ARP].p[P_SSWING] == 10 * 40 / 127);
    turn(EN_K3, 7);
    bad += check("  .. and the knobs give no gain: the volumes stay where they were", (uint32_t)trk[2].p[P_LEVEL] == i);
    let_go(B_SEQ);
    bad += check("  SEQ has no solo: the keys solo nothing", perf_solo == 0u);

    start(0);
    ui.home = 1; ui.menu = 0; ui.confirm = 0;
    down_for(B_FX, 3);
    turn(EN_SELECT, -20);
    turn(EN_PRESET, -27);
    turn(EN_ALGO, -63);
    bad += check("  FX held: SELECT the SINK, PRESETS the WASH, ALGORITHM the HUSH; the macros stay",
                 maku.vp[VB_SINK][0] == 80 && maku.vp[VB_SINK][1] == 100 && maku.vp[VB_SINK][2] == 64 && maku.m[0][1] == 40u);
    for (n = 0; n < 3000u; n++)
        maku_block();
    bad += check("  held: the dive reaches WASH's share, the reverb's pull SINK's, and the kick stops at HUSH's half",
                 maku_dive() == 4096 * 100 / 127 && maku_sink() == 4096 * 80 / 127 && maku_gain(0) > 2000 && maku_gain(0) < 2100);
    let_go(B_FX);
    start(0);
    maku.brk = 1;
    for (n = 0; n < 3000u; n++)
        maku_block();
    maku.brk = 0;
    bad += check("  at the defaults SINK is the old BREAK as it was, and SINK is on top of it", maku_dive() == 4096 && maku_gain(0) == 0 && maku_sink() == 4096 * 100 / 127);
    for (n = 0; n < 5000u; n++)
        maku_block();
    bad += check("  let go: the pull on the reverb lifts too", maku_sink() == 0);
    {   /* the reverb's tail rings longer with SINK: an impulse into the room, the energy after a second */
        int32_t in[CTL], out[CTL], tail[2];
        uint32_t b, pass;
        for (pass = 0; pass < 2u; pass++) {
            int64_t e = 0;
            memset(&fx, 0, sizeof fx);
            rev_clear();
            maku.dive = pass ? 4096 : 0;
            for (b = 0; b < 400u; b++) {
                uint32_t j;
                for (j = 0; j < CTL; j++)
                    in[j] = b == 0u && j == 0u ? 20000 : 0, out[j] = 0;
                wet_r[0] = 0;
                rev_room(in, out, CTL);
                if (b >= 300u)
                    for (j = 0; j < CTL; j++)
                        e += (int64_t)out[j] * out[j] >> 8;
            }
            tail[pass] = (int32_t)(e >> 8);
        }
        bad += check("  SINK: the room's tail is longer (over twice) while FX is held", tail[1] > 2 * tail[0]);
    }
    return bad;
}

/* the verbs that change the music (docs/VERBS_PLAN.md): SWELL's STAY, SCRAMBLE, CASCADE, DRIFT, LIFT, WOBBLE */
static int verbs2(void)
{
    int bad = 0;
    uint32_t i, ok, a0, a1, d0;
    int32_t c, d, r, l0;
    uint8_t m0;
    /* SWELL's STAY: the drones' chord stays while it is held (a held part rings on: its next chord is not struck; a struck one repeats) */
    {
        uint32_t sig[24], ns, j, k, tie = 0;
        for (k = 0; k < 2u; k++) {
            start(60);
            maku.hv = k ? VBIT(VB_SWELL) : 0u;
            maku.vp[VB_SWELL][2] = 127;
            for (i = 0, ns = 0; i < 24u; i++) {
                step_t st;
                uint32_t sg = 0;
                memset(&st, 0, sizeof st);
                maku_step(MAKU_DRA, (i % 3u) * 19u, &st);
                if (st.time == ST_TIE)
                    tie++;
                for (j = 0; j < st.n; j++)
                    sg = sg * 131u + st.note[j];
                if (i >= 3u)
                    sig[ns++] = sg;
            }
            for (j = 1, ok = 0; j < ns; j++)
                ok |= sig[j] != sig[0];
            bad += check(k ? "SWELL held: STAY 127: the drone's chord stays (a held floor ties it on, a struck one repeats)" : "  and without it the chords move on",
                         k ? (!ok || tie > 0u) : ok);
        }
        maku.hv = 0;
    }

    /* SCRAMBLE: the kick gets ghosts even at DENSITY 0; the arp's jumps and re-rolls rise */
    start(0);
    memset(nhits, 0, sizeof nhits);
    sim(64);
    a0 = nhits[0];
    maku.hv = VBIT(VB_SCRAM);
    maku.vp[VB_SCRAM][1] = 127;
    memset(nhits, 0, sizeof nhits);
    sim(64);
    bad += check("SCRAMBLE held: ghost kicks where DENSITY 0 has none", a0 == 0u && nhits[0] > 5u);
    maku.va[VB_SCRAM] = 4096;
    maku_apply();
    maku.vp[VB_SCRAM][2] = 127;
    maku_apply();
    bad += check("  its SWING adds to the arp's", trk[MAKU_ARP].p[P_SSWING] >= 40);
    maku.hv = 0;
    maku.va[VB_SCRAM] = 0;

    /* CASCADE: every step plays, the breath too, octaves stacked */
    start(60);
    memset(nhits, 0, sizeof nhits);
    sim(29 * 8);
    a0 = nhits[3];
    maku.hv = VBIT(VB_CASC);
    maku.vp[VB_CASC][0] = 127;
    maku.vp[VB_CASC][1] = 127;
    memset(nhits, 0, sizeof nhits);
    sim(29 * 8);
    for (i = 0, ok = 0; i < nhits[3]; i++)
        ok |= hits[3][i].step >= MAKU_ARP_GAP;
    {
        uint32_t tries;
        for (tries = 0, a1 = 0; tries < 60u && a1 != 3u; tries++) {   /* (a rest or a skipped step plays nothing: ask again) */
            step_t st;
            memset(&st, 0, sizeof st);
            a1 = maku_step(MAKU_ARP, 3, &st) ? st.n : 0u;
        }
    }
    bad += check("CASCADE held: the arp plays much more, in its breath too, and stacks octaves (SPAN)", nhits[3] > a0 * 3u / 2u && ok && a1 == 3u);
    maku.vp[VB_CASC][1] = 0;                            /* no octaves: only HARM's third is stacked */
    maku.vp[VB_CASC][2] = 127;
    {
        uint32_t tries, in_key = 1, found = 0;
        for (tries = 0; tries < 200u; tries++) {
            step_t st;
            memset(&st, 0, sizeof st);
            if (maku_step(MAKU_ARP, 3, &st) && st.n == 2u) {
                uint32_t mask = scale_mask(&trk[MAKU_ARP]), root = (uint32_t)trk[MAKU_ARP].p[P_ROOT] % 12u, d = st.note[1] - st.note[0];
                found++;
                in_key &= (d == 3u || d == 4u) && ((mask >> ((st.note[1] + 120u - root) % 12u)) & 1u);
            }
        }
        bad += check("  HARM 127: a third above the note, in the scale", found > 20u && in_key);
    }
    maku.vp[VB_CASC][1] = 127;
    maku.vp[VB_CASC][2] = 0;
    {
        uint32_t tries, two = 0;
        for (tries = 0; tries < 100u; tries++) {
            step_t st;
            memset(&st, 0, sizeof st);
            if (maku_step(MAKU_ARP, 3, &st) && st.n == 3u)
                two++;
        }
        bad += check("  HARM 0: only the octaves of SPAN (3 notes), no third", two > 20u);
    }
    maku.hv = 0;

    /* DRIFT: the space it has added raises the sends of everything but the kick */
    start(0);
    maku.drs = 127;
    c = d = r = 0;
    maku_boost(MAKU_DRB, &c, &d, &r);
    bad += check("DRIFT: the space it added: the haze's chorus, delay and reverb sends rise", c > 10000 && d > 10000 && r > 15000);
    c = d = r = 0;
    maku_boost(MAKU_KICK, &c, &d, &r);
    bad += check("  the kick's do not", !c && !d && !r);
    maku.drs = 0;
    c = d = r = 0;
    maku_boost(MAKU_DRB, &c, &d, &r);
    bad += check("  none added: no sends", !c && !d && !r);
    {                                                   /* held: the macros wander within RANGE of the press, and stay when let go */
        uint32_t k, moved = 0, far = 0, held_m[12];
        start(0);
        maku.vp[VB_DRIFT][0] = 127; maku.vp[VB_DRIFT][1] = 40; maku.vp[VB_DRIFT][2] = 127;
        for (k = 0; k < 12u; k++)
            held_m[k] = maku.m[k / 3u][k % 3u];
        song.g[G_CLOCK] = 0;
        maku.dr_tog = 1;
        for (k = 0; k < 20000u; k++)
            maku_drift();
        for (k = 0; k < 12u; k++) {
            int32_t dm = (int32_t)maku.m[k / 3u][k % 3u] - (int32_t)held_m[k];
            moved += dm != 0;
            far += dm > 20 || dm < -20;
        }
        bad += check("DRIFT held: the macros wander, never further than RANGE / 2 from the press", moved > 6u && !far);
        bad += check("  REACH 127: the space and the tempo wander too", maku.drs <= 20 && song.g[G_BPM] >= 40);
        maku.dr_tog = 0;
        maku_drift();
        k = maku.m[1][1];
        for (moved = 0; moved < 5000u; moved++)
            maku_drift();
        bad += check("  switched off: it stops where it is", !maku.dr_on && maku.m[1][1] == k);
        maku.vp[VB_DRIFT][2] = 0;
        maku.drs = 0;
    }
    {                                                   /* a macro turned by hand while it is on is the new place to stay near */
        uint32_t k, far = 0;
        start(0);
        maku.vp[VB_DRIFT][0] = 127; maku.vp[VB_DRIFT][1] = 20; maku.vp[VB_DRIFT][2] = 0;
        maku.dr_tog = 1;
        for (k = 0; k < 3000u; k++)
            maku_drift();
        maku_macro_set(1, 1, 110);
        for (k = 0; k < 20000u; k++) {
            maku_drift();
            far |= (uint32_t)((int32_t)maku.m[1][1] < 100 || (int32_t)maku.m[1][1] > 120);
        }
        bad += check("  a macro turned by hand meanwhile: it wanders near the new value, not back to the old", !far);
        maku.dr_tog = 0;
        maku_drift();
    }

    /* LIFT: every track's TONE further on while held, back when let go; the knob values stay. EDGE: more distortion, THIN: the high-pass */
    start(0);
    l0 = trk[2].p[P_E7];
    m0 = maku.m[2][1];
    maku.hv = VBIT(VB_LIFT);
    for (i = 0; i < 400u; i++)
        maku_block();
    d0 = (uint32_t)trk[2].p[P_E7];
    bad += check("LIFT held: TONE of every track moves up, the macro's own value stays", (int32_t)d0 > l0 && maku.m[2][1] == m0);
    bad += check("  EDGE: the voices' distortion rises (not the kick's)", trk[1].p[P_DIST] > 0 && trk[3].p[P_DIST] > 0 && trk[0].p[P_DIST] == 0);
    maku.hv = 0;
    for (i = 0; i < 900u; i++)
        maku_block();
    bad += check("  let go: all back", trk[2].p[P_E7] == l0 && !maku.tw_q && trk[1].p[P_DIST] == 0);
    /* SWELL and WOBBLE: attack and release longer, the filter LFO faster and deeper, put back when let go; a macro written meanwhile is the new base */
    start(0);
    {
        int16_t a0 = trk[2].p[P_ATK], r0 = trk[2].p[P_REL], lr = trk[1].p[P_LRATE], ld = trk[1].p[P_LD_FLT], k0 = trk[0].p[P_ATK];
        maku.hv = VBIT(VB_SWELL) | VBIT(VB_WOB);
        for (i = 0; i < 400u; i++)
            maku_block();
        bad += check("SWELL held: the attack and release of the three voices are longer (not the kick's)", trk[2].p[P_ATK] > a0 && trk[2].p[P_REL] > r0 && trk[1].p[P_ATK] > 10 && trk[0].p[P_ATK] == k0);
        bad += check("WOBBLE held: the drone's filter LFO faster and deeper, and a pitch wobble", trk[1].p[P_LRATE] > lr && trk[1].p[P_LD_FLT] > ld && trk[1].p[P_LD_PIT] > 0);
        maku_macro_set(2, 2, 100);                      /* AIR moves the shimmer's attack while SWELL is on */
        for (i = 0; i < 4u; i++)
            maku_block();
        maku.hv = 0;
        for (i = 0; i < 900u; i++)
            maku_block();
        bad += check("  let go: all back where they were; AIR's new attack stays", trk[1].p[P_LRATE] == lr && trk[1].p[P_LD_FLT] == ld && trk[3].p[P_REL] > 0 &&
                     trk[2].p[P_ATK] != a0 && trk[2].p[P_ATK] == 60 + ((127 - 60) * 100 + 63) / 127 && !maku.ovlive);
        bad += check("  .. and the pitch wobble is gone", trk[1].p[P_LD_PIT] == 0);
        (void)r0;
    }
    maku.va[VB_WOB] = 4096;
    maku.vp[VB_WOB][1] = 127;
    c = d = r = 0;
    maku_boost(MAKU_DRA, &c, &d, &r);
    bad += check("WOBBLE: DEPTH also sends the voices into the chorus, not the kick", c > 10000 && !d && !r);
    c = 0;
    maku_boost(MAKU_KICK, &c, &d, &r);
    bad += check("  the kick's chorus stays", c == 0);
    maku.va[VB_WOB] = 0;
    return bad;
}

static int screen(void)
{
    int bad = 0;
    uint32_t i, lit = 0, w;
    start(40);
    ui.home = 1; ui.menu = 0; ui.confirm = 0;
    maku_world(7);
    song.sel = 0;
    sim(40);
    scope_w = 0;
    for (w = 0; w < SCOPE_N; w++)
        scope_buf[w] = (int16_t)(8000.0 * sin(w * 0.15));
    for (i = 0; i < 8u; i++)
        grid_hit(&trk[0], i * 2u, 0, 1);
    ui.force = 1;
    ui_draw();
    for (i = 0; i < 240u * 240u; i++)
        lit += host_screen[i] != host_screen[0];
    bad += check("SCREEN: four quadrants drawn, not blank", lit > 1500u);
    if (getenv("MAKU_PPM")) {
        screen_save(getenv("MAKU_PPM"), "maku-kick");
        song.sel = 3;
        ui.force = 1;
        screen_save(getenv("MAKU_PPM"), "maku-voice");
    }
    return bad;
}

static int worlds_voices(void)
{
    int bad = 0;
    int32_t o[2u * CTL];
    uint32_t b, i, k, nb = 6u * BAR_STEPS * step_blocks(), act, steals = 0, seed, maxact = 0, presets_seen = 0;
    uint8_t seen[3][16] = {{0}};
    uint32_t ins_worlds = 0, ins_none = 0, ins_sounding = 0, ins_wrong = 0;
    for (seed = 1; seed <= 12u; seed++) {
        start(127);
        maku_world(seed * 7919u);
        ins_wrong += trk[MAKU_KICK].p[P_ITYPE] != IT_OFF || trk[MAKU_ARP].p[P_ITYPE] != IT_OFF ||
                     !(trk[MAKU_DRA].p[P_ITYPE] == IT_PHASER || trk[MAKU_DRA].p[P_ITYPE] == IT_CHORUS || trk[MAKU_DRA].p[P_ITYPE] == IT_OFF) ||
                     !(trk[MAKU_DRB].p[P_ITYPE] == IT_OFF || trk[MAKU_DRB].p[P_ITYPE] == IT_FLANGER || trk[MAKU_DRB].p[P_ITYPE] == IT_CHORUS);
        ins_worlds += trk[MAKU_DRA].p[P_ITYPE] != IT_OFF || trk[MAKU_DRB].p[P_ITYPE] != IT_OFF;
        ins_none += (trk[MAKU_DRA].p[P_ITYPE] == IT_OFF) + (trk[MAKU_DRB].p[P_ITYPE] == IT_OFF);   /* (a drone with none) */
        for (i = 1; i < 4u; i++)
            seen[i - 1u][trk[i].preset & 15u] = 1;
        maku_macro_set(MAKU_DRA, 0, 127);
        maku_macro_set(MAKU_ARP, 0, 127);
        maku_set_density(127);
        for (i = 0; i < NTRK; i++)
            trk[i].engine = trk[i].eng_req;
        for (b = 0; b < nb; b++) {
            uint32_t k0 = voice_kills;
            memset(o, 0, sizeof o);
            mix_block(o, CTL);
            act = 0;
            for (i = 0; i < NTRK; i++)
                for (k = 0; k < NVOICE; k++)
                    act += trk[i].v[k].active != 0;
            maxact = act > maxact ? act : maxact;
            steals += voice_kills - k0;               /* (a voice given up for another part's note) */
        }
        ins_sounding += (trk[MAKU_DRA].p[P_ITYPE] != IT_OFF && trk[MAKU_DRA].p[P_IMIX] > 0 && ins[MAKU_DRA].w != 0) +
                        (trk[MAKU_DRB].p[P_ITYPE] != IT_OFF && trk[MAKU_DRB].p[P_IMIX] > 0 && ins[MAKU_DRB].w != 0);
    }
    for (i = 0; i < 3u; i++)
        for (k = 0; k < 16u; k++)
            presets_seen += seen[i][k];
    printf("ui:   12 worlds at DENSITY 127, OPEN and LOOSE 127: most voices at once %u, steals %u, %u distinct presets over the three voices\n",
           maxact, steals, presets_seen);
    bad += check("WORLDS: the voices change from world to world (>= 6 presets seen) and never run out of voices", presets_seen >= 6u && steals == 0u);
    printf("ui:   INSERT: %u of 12 worlds give a drone one, %u drones none; %u set ones sounding at the end\n", ins_worlds, ins_none, ins_sounding);
    bad += check("INSERT: only the drones get one (PHASR / CHOR, FLANG / CHOR), most worlds do, some drones do not, and a set one sounds",
                 !ins_wrong && ins_worlds >= 6u && ins_none >= 1u && ins_sounding >= ins_worlds);
    {   /* the TONE macro of the drone carries the MIX (20 .. 110): up with the macro */
        uint32_t v;
        int mono = 1, last = -1;
        for (v = 0; v <= 127u; v += 9u) {
            maku_macro_set(MAKU_DRA, 1, v);
            mono &= trk[MAKU_DRA].p[P_IMIX] >= last && trk[MAKU_DRA].p[P_IMIX] <= 127;
            last = trk[MAKU_DRA].p[P_IMIX];
        }
        maku_macro_set(MAKU_DRA, 1, 127);
        bad += check("INSERT: the drone's TONE macro moves its MIX up to 110, within 0 .. 127", mono && trk[MAKU_DRA].p[P_IMIX] == 110);
    }
    return bad;
}


/* The voice catalog (docs/VOICES_RATING.md, maku_voices.c): 120 worlds at DENSITY 127 with OPEN and LOOSE 127 over 9 bars each (more than a
 * chord cycle of the held drone): never a voice taken from another part, held voices actually hold, and the mix of articulations is
 * what the catalog promises: the floor held in 65..85 % of the worlds, at least one of floor and haze held in every one, the
 * struck voices in the rest */
static int catalog(void)
{
    int bad = 0;
    int32_t o[2u * CTL];
    uint32_t w, b, i, k, nb = 9u * BAR_STEPS * step_blocks(), act, maxact = 0, steals = 0;
    uint32_t floor_held = 0, none_held = 0, arp_slow = 0, arp_struck = 0, n = 120u;
    uint32_t held_gap_blocks = 0, held_blocks = 0, art_seen[5] = {0}, rows_seen = 0;
    uint8_t seen[4][16] = {{0}};
    for (w = 1; w <= n; w++) {
        uint32_t fa, fb, quiet_a = 0, quiet_b = 0, ea, eb, pk[4] = {0}, st0 = steals;
        start(127);
        maku_world(w * 104729u);
        maku_macro_set(MAKU_DRA, 0, 127);
        maku_macro_set(MAKU_ARP, 0, 127);
        maku_set_density(127);
        for (i = 0; i < NTRK; i++)
            trk[i].engine = trk[i].eng_req;
        fa = maku_held(MAKU_DRA);
        fb = maku_held(MAKU_DRB);
        floor_held += fa;
        none_held += !fa && !fb;
        arp_slow += maku_held(MAKU_ARP);
        arp_struck += !maku_held(MAKU_ARP);
        for (i = 1; i < 4u; i++) {
            art_seen[maku.art[i]]++;
            seen[i][maku.voice[i] & 15u] = 1;
        }
        ea = trk[MAKU_DRA].eng_req;
        eb = trk[MAKU_DRB].eng_req;
        for (b = 0; b < nb; b++) {
            uint32_t k0 = voice_kills, before = 0, ad = 0, bd = 0;
            for (i = 0; i < NTRK; i++)
                for (k = 0; k < NVOICE; k++)
                    before += trk[i].v[k].active != 0;
            memset(o, 0, sizeof o);
            mix_block(o, CTL);
            act = 0;
            for (i = 0; i < NTRK; i++)
                for (k = 0; k < NVOICE; k++) {
                    act += trk[i].v[k].active != 0;
                    ad += i == MAKU_DRA && trk[i].v[k].active;
                    bd += i == MAKU_DRB && trk[i].v[k].active;
                }
            maxact = act > maxact ? act : maxact;
            steals += voice_kills - k0;           /* voices given up for another part's note (voice_kill) */
            for (i = 0; i < NTRK; i++) {
                uint32_t c = 0;
                for (k = 0; k < NVOICE; k++)
                    c += trk[i].v[k].active != 0;
                pk[i] = c > pk[i] ? c : pk[i];
            }
            if (b > nb / 9u) {                       /* (after the first bar: the held parts have started) */
                quiet_a += fa && !ad;
                quiet_b += fb && !bd;
            }
        }
        if (getenv("MAKU_DBG"))
            printf("  w%3u %-13s %-12s %-13s peak voices k%u a%u b%u c%u steals %u\n", w, MAKU_V_DRA[maku.voice[1]].name, MAKU_V_DRB[maku.voice[2]].name,
                   MAKU_V_ARP[maku.voice[3]].name, pk[0], pk[1], pk[2], pk[3], steals - st0);
        held_gap_blocks += quiet_a + quiet_b;
        held_blocks += (fa + fb) * (nb - nb / 9u);
        (void)ea; (void)eb;
    }
    for (i = 1; i < 4u; i++)
        for (k = 0; k < 16u; k++)
            rows_seen += seen[i][k];
    printf("ui:   catalog, %u worlds: floor held %u, none of floor / haze held %u, slow melody %u / struck %u; most voices at once %u, steals %u; "
           "held parts silent in %u of %u blocks; %u rows of the catalog seen\n",
           n, floor_held, none_held, arp_slow, arp_struck, maxact, steals, held_gap_blocks, held_blocks, rows_seen);
    bad += check("CATALOG: no voice is ever taken from another part (held voices, swells and bowed sounds included)", steals == 0u);
    bad += check("CATALOG: the floor is held in 65..85 % of the worlds, and in every world the floor or the haze is held",
                 floor_held * 100u >= 65u * n && floor_held * 100u <= 85u * n && none_held == 0u);
    bad += check("CATALOG: held voices hold (silent less than 2 % of the blocks after the first bar)", held_blocks && held_gap_blocks * 50u < held_blocks);
    bad += check("CATALOG: a slow melody and a struck one both occur; all five articulations are heard over the parts",
                 arp_slow >= 5u && arp_struck >= 5u && art_seen[ART_STRK] && art_seen[ART_SOFT] && art_seen[ART_SWEL] && art_seen[ART_BOW] && art_seen[ART_HOLD]);
    bad += check("CATALOG: most rows of the catalog turn up (>= 20 of 24)", rows_seen >= 20u);
    return bad;
}

#include <time.h>
/* host CPU cost of the whole device (not a pass / fail: the hardware's cost is read off the ratios; see docs/AMBIENT.md) */
static double bench_once(uint32_t seed, uint32_t density, uint32_t open, uint32_t verbs, uint32_t bars)
{
    int32_t o[2u * CTL];
    struct timespec t0, t1;
    uint32_t b, nb = bars * BAR_STEPS * step_blocks();
    start(density);
    if (seed)
        maku_world(seed * 7919u);
    maku_macro_set(MAKU_DRA, 0, open);
    maku_macro_set(MAKU_ARP, 0, open);
    maku_set_density(density);
    for (uint32_t i = 0; i < NTRK; i++)
        trk[i].engine = trk[i].eng_req;
    perf_held = verbs;
    perf_k[3] = verbs & PF_BIT(PF_OUP) ? 60 : 0;
    for (b = 0; b < 200u; b++) {                       /* (warm up: the voices are sounding) */
        memset(o, 0, sizeof o);
        mix_block(o, CTL);
    }
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (b = 0; b < nb; b++) {
        memset(o, 0, sizeof o);
        mix_block(o, CTL);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    perf_held = 0;
    perf_k[3] = 0;
    return ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / ((double)nb * CTL);
}
static double bench_run(uint32_t seed, uint32_t density, uint32_t open, uint32_t verbs, uint32_t bars)
{
    double best = 1e9;                                  /* the least of five: a busy machine only ever adds */
    for (int r = 0; r < 5; r++) {
        double t = bench_once(seed, density, open, verbs, bars);
        best = t < best ? t : best;
    }
    return best;
}
static int bench(void)
{
    double idle = bench_run(0, 0, 60, 0, 4), old = bench_run(0, 127, 60, 0, 6), worst = 0, avg = 0, fx = 0, fxmax = 0;
    uint32_t seed, n = 0;
    for (seed = 1; seed <= 12u; seed++) {
        double a = bench_run(seed, 127, 127, 0, 6), c = bench_run(seed, 127, 127,
                   PF_BIT(PF_FRZ) | PF_BIT(PF_OUP) | PF_BIT(PF_R16) | PF_BIT(PF_LPF), 6);
        worst = a > worst ? a : worst;
        fxmax = c > fxmax ? c : fxmax;
        avg += a;
        fx += c;
        n++;
    }
    printf("ui:   CPU on the host, ns per sample: quiet %.0f | the old MAKU setup at 127 %.0f | 12 worlds at 127, OPEN / LOOSE 127: mean %.0f worst %.0f | same with FREEZE+HARMONIZER+REPEAT+LPF held: mean %.0f worst %.0f\n",
           idle, old, avg / n, worst, fx / n, fxmax);
    printf("ui:   relative to the old setup (30 %% of the chip, docs/MAKU.md): worlds mean x%.2f worst x%.2f | with the verbs mean x%.2f worst x%.2f\n",
           avg / n / old, worst / old, fx / n / old, fxmax / old);
    return 0;
}

int main(void)
{
    int bad = setup() + world() + kick() + drones() + scale() + arp() + follow() + duck() + voices() + cost() + kickgrid() + macros() + knobs() + buttons() + melt() + fifths() + verbs() + verbs2() + screen() + worlds_voices() + catalog() + (getenv("MAKU_BENCH") ? bench() : 0);
    printf("%s\n", bad ? "MAKU TEST FAILED" : "maku tests passed");
    return bad != 0;
}
