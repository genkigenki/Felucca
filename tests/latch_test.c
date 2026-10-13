/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GHOULBOX (on Felucca by Leo Kuroshita, Hügelton Instruments) */
/* The drone latch (seq.c latch_on, core.h latch_t, GLO + F#4..C#5) through the real keyboard scan, MIDI queues,
 * sequencer and voices (the UI test's harness):
 * 1. off: a key's note ends with the key (as ever).
 * 2. on: a chord sounds on after its keys; MIDI OUT has its note-ons, the note-offs wait.
 * 3. a new chord (no key down) replaces the drone: the old notes end, their MIDI OUT note-offs go then.
 * 4. a droning note pressed: the drone ends, that press silent (and not sent) until every key is up; the next plays.
 * 5. what cannot end it: the sequencer's note-off of the same note, a MIDI note-off, the pedal (CC64).
 * 6. what ends it: CC123, CC120, a panic (sound / project load), the latch off, the ARP on, a DRUM engine.
 * 7. recording: the key's own length (the held note let go at the key-up), the drone sounding on.
 * 8. the voices: a busy track's 12 held notes take none of another's drone (without the latch they do); more than LT_KEEP
 *    latched: down to LT_KEEP, the oldest first; a latched voice at a silent sustain is not kept.
 * 9. the GLO keys: F#4..C#5 switch T1..T4 (not a DRUM track); the map shows it. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void lt_reset(void)
{
    ui_power_on();
    memset(midi_ch, 0, sizeof midi_ch); memset(midi_notes, 0, sizeof midi_notes);
    memset(midi_owners, 0, sizeof midi_owners);
    mi_r = mi_w = 0; midi_in_overflow = 0;
    fm1_in.notes = kb_prev = 0; song.sel = 0;
    memset(lt, 0, sizeof lt);
    for (uint32_t i = 0; i < NTRK; i++)
        trk_all_off(&trk[i]);
    for (uint32_t i = 0; i < 8u; i++)
        events_block(CTL);                              /* (released voices let go) */
    for (uint32_t p = 0; p < NPART; p++)
        for (uint32_t i = 0; i < NVOICE; i++)
            trk[p].v[i].active = 0;
    usb.config = 1; mo_r = mo_w;
}
static void q(uint32_t st, uint32_t d1, uint32_t d2)
{
    midi_enqueue(st >> 4 | st << 8 | d1 << 16 | d2 << 24, 1);
    events_block(CTL);
}
static void keys(uint32_t m) { fm1_in.notes = m; events_block(CTL); }
static int lgated(const track_t *t, uint32_t note)
{
    for (uint32_t i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].note == note && t->v[i].stage != 4u)
            return 1;
    return 0;
}
static uint32_t ngated(const track_t *t)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < NVOICE; i++)
        n += t->v[i].active && t->v[i].gate && t->v[i].stage != 4u;
    return n;
}
/* MIDI OUT since mo_r: count of note-ons (st 9) / note-offs (st 8) of note n */
static uint32_t sent(uint32_t st, uint32_t n)
{
    uint32_t c = 0;
    for (uint32_t i = mo_r; i != mo_w; i++) {
        uint32_t p = midi_out_q[i % MQ];
        c += (p & 0xFu) == st && ((p >> 16) & 0x7Fu) == n;
    }
    return c;
}

int main(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t a, b, c, i;
    char what[160];

    lt_reset();                                         /* 1 */
    keys(1u); a = kb_note[0]; keys(0);
    bad += check("latch off: the note ends with its key, MIDI OUT on and off", !lgated(t, a) && sent(9, a) == 1u && sent(8, a) == 1u);

    lt_reset(); lt[0].on = 1; events_block(CTL);       /* 2 */
    t->p[P_VOICE] = V_MONO;
    keys(1u | 1u << 4); a = kb_note[0]; b = kb_note[4]; keys(1u << 4); keys(0);
    bad += check("MONO: the drone is the last note (the stack keeps both)", !lgated(t, a) && lgated(t, b) && t->nmono == 2u);
    lt_reset(); lt[0].on = 1; events_block(CTL);
    t->p[P_VOICE] = V_POLY;
    keys(1u | 1u << 4); a = kb_note[0]; b = kb_note[4]; keys(1u << 4); keys(0);
    bad += check("latch on: a chord sounds on after its keys", lgated(t, a) && lgated(t, b) && lt[0].phys == 0u);
    bad += check("  MIDI OUT: the note-ons, no note-off yet", sent(9, a) == 1u && sent(9, b) == 1u && !sent(8, a) && !sent(8, b));

    mo_r = mo_w;                                        /* 3 */
    keys(1u << 7); c = kb_note[7]; keys(0);
    bad += check("a new chord replaces the drone (the old notes end, the new one sounds on)", !lgated(t, a) && !lgated(t, b) && lgated(t, c));
    bad += check("  MIDI OUT: the old notes' note-offs then, the new one's on", sent(8, a) == 1u && sent(8, b) == 1u && sent(9, c) == 1u && !sent(8, c));

    mo_r = mo_w;                                        /* 4 */
    keys(1u << 7 | 1u << 9);
    bad += check("a droning note pressed: the drone ends, that press silent", !lgated(t, c) && !lgated(t, kb_note[9]) && lt[0].stop);
    bad += check("  and not sent: only the drone's note-off", sent(8, c) == 1u && !sent(9, c) && !sent(9, kb_note[9]));
    keys(1u << 9); keys(0);
    bad += check("  every key up: nothing sounds, nothing more sent", !ngated(t) && !lt[0].stop && !sent(8, kb_note[9]));
    keys(1u << 2); a = kb_note[2]; keys(0);
    bad += check("  the next press plays and latches", lgated(t, a));

    trk_note_on(t, a, 100); trk_note_off(t, a);         /* 5 */
    bad += check("the sequencer's note-off of a droning note: it sounds on", lgated(t, a));
    q(0x90, 70, 100); q(0x80, 70, 0);
    bad += check("MIDI in latches too (a new chord: the key's note ended), its note-off: sounds on", lgated(t, 70) && !lgated(t, a));
    q(0xB0, 64, 127); q(0xB0, 64, 0);
    bad += check("the pedal down and up: sounds on", lgated(t, 70));
    q(0xB0, 123, 0);                                    /* 6 */
    bad += check("CC123: the drone ends", !lgated(t, 70) && !lt[0].m[2]);
    q(0x90, 71, 100); q(0x80, 71, 0); q(0xB0, 120, 0);
    bad += check("CC120: the drone ends", !lgated(t, 71) && !lt[0].m[2]);
    keys(1u); a = kb_note[0]; keys(0); mo_r = mo_w;
    panic_req |= 1u; events_block(CTL);
    bad += check("a panic (sound / project load): the drone ends, its MIDI OUT note-off sent", !lgated(t, a) && sent(8, a) == 1u);
    keys(1u); keys(0); mo_r = mo_w;
    lt[0].on = 0; events_block(CTL);
    bad += check("the latch off: the drone ends, its note-off sent", !lgated(t, a) && sent(8, a) == 1u && !lt[0].onp);
    lt[0].on = 1; events_block(CTL);
    keys(1u); keys(0);
    t->p[P_AMODE] = 1; events_block(CTL);
    bad += check("the ARP on: the drone ends (ARP HOLD latches there)", !lgated(t, a) && !lt[0].onp);
    keys(1u); keys(0);
    bad += check("  with the ARP on, keys do not latch", !lt[0].m[0] && !lt[0].m[1] && !lt[0].m[2] && !lt[0].m[3]);
    t->p[P_AMODE] = 0; events_block(CTL);
    lt[3].on = 1; events_block(CTL);
    bad += check("a DRUM track: the latch never takes effect", drum_track(&trk[3]) && !lt[3].onp);
    lt[3].on = 0;

    lt_reset(); lt[0].on = 1; events_block(CTL);       /* 7 */
    song.rec = 1; seq_start(); events_block(CTL);
    keys(1u); a = kb_note[0];
    for (i = 0; i < 20u; i++) events_block(CTL);
    c = t->rh_n;
    keys(0);
    bad += check("recording: the key's note recorded and let go at the key-up, the drone sounding on", c == 1u && !t->rh_n && lgated(t, a));
    seq_stop(); song.rec = 0; events_block(CTL);
    bad += check("  STOP: the drone sounds on", lgated(t, a));

    for (int with = 0; with < 2; with++) {              /* 8 */
        uint32_t keep;
        lt_reset(); lt[0].on = (uint8_t)with; events_block(CTL);
        t->p[P_VOICE] = trk[2].p[P_VOICE] = V_POLY;
        keys(1u | 1u << 4 | 1u << 7); a = kb_note[0]; b = kb_note[4]; c = kb_note[7];
        if (with) keys(0);
        for (i = 0; i < 12u; i++) {                     /* T3 (12 voices): a busy part, its notes piling up */
            trk_note_on(&trk[2], 60u + i, 100);
            events_block(CTL);
        }
        keep = (uint32_t)lgated(t, a) + (uint32_t)lgated(t, b) + (uint32_t)lgated(t, c);
        snprintf(what, sizeof what, "%s: T3's 12 held notes leave T1 %u of its 3", with ? "the drone (latched)" : "keys held, no latch", keep);
        bad += check(what, with ? keep == 3u : keep < 3u);
        keys(0);
    }
    {
        uint32_t np = trk_nvoice(t), n0;
        lt_reset(); lt[0].on = 1; events_block(CTL);
        t->p[P_VOICE] = trk[2].p[P_VOICE] = V_POLY;
        keys(0x3FFu); keys(0);                          /* 10 keys: 10 latched notes (up to the engine's voices) */
        n0 = ngated(t);
        for (i = 0; i < 24u; i++) {
            trk_note_on(&trk[2], 40u + i, 100);
            events_block(CTL);
        }
        snprintf(what, sizeof what, "more than LT_KEEP (%u) latched (%u, the engine's voices %u): T3 takes down to %u, no lower (%u)",
                 LT_KEEP, n0, np, LT_KEEP, ngated(t));
        bad += check(what, n0 > LT_KEEP && ngated(t) == LT_KEEP);
        for (i = 0; i < NVOICE; i++)
            if (t->v[i].active && t->v[i].gate) {
                t->v[i].stage = 2; t->v[i].env_out = 0;
                break;
            }
        bad += check("  a latched voice at a silent sustain is not kept (lt_voice)", i < NVOICE && !lt_voice(t, &t->v[i]));
        keys(0);
    }

    lt_reset();                                         /* 9 */
    ui.layer = ui.ly = LAYER_GLO;
    for (i = 0; i < 27u; i++)
        if (key_black(i) && key_place(i) >= GLO_LT && key_place(i) < GLO_LT + NTRK)
            layer_key(LAYER_GLO, i);
    bad += check("GLO + F#4 G#4 A#4 C#5: T1..T3 latched, T4 (DRUM) not", lt[0].on && lt[1].on && lt[2].on && !lt[3].on);
    bad += check("  the LEDs: T1..T3's lit", (layer_leds(0) >> 13 & 1u) && (layer_leds(0) >> 15 & 1u) &&
                 (layer_leds(0) >> 17 & 1u) && !(layer_leds(0) >> 20 & 1u));
    layer_key(LAYER_GLO, 13);
    bad += check("  F#4 again: T1's off", !lt[0].on);
    ui.layer = ui.ly = 0;
    usb.config = 0;
    printf("%s\n", bad ? "LATCH TEST FAILED" : "latch test passed");
    return bad != 0;
}
