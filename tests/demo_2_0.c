/* SPDX-License-Identifier: GPL-3.0-only */
/* GHOULBOX 2.0 demo: a ~2.5 min tour of what 2.0 added, through the firmware's own audio AND screen code
 * (tests/ui_render.c), each feature on the screen while it is heard:
 *   I   THE CAVE         AMB and CRKL rise from silence, nothing playing (FX > TAPE)
 *   II  THE DRONE LATCH  a CATHEDRAL chord latched on T4 (the GLO map), a HURDY GURDY melody over it; the drone
 *                        sounds on to the end
 *   III THE CRANK        T1's GURDY on COUP CRANK: the wheel turned by "hand" (crank_clicks), swells, stops, wrist
 *                        strokes kick the trompette (EDIT 2: COUP CRANK)
 *   IV  PURE TUNINGS     PIPE ORGAN chords in EQUAL, then the same in JUST (SCL > CHORD: TEMP)
 *   V   DUNGEON SCALES   D Hungarian minor: VIELLE, MONKS, TOMB DRUMS over the drone (SCL: D HUNG, then HOME)
 *   build/host/demo_2_0 OUT.wav [FRAMES.rgb]   -> the mix; with FRAMES.rgb the screen, 240x240 RGB24 at 30 fps;
 *   OUT.wav.cues: each section's start, number, title, how, sounds (tools/make_demo20_video.py). Not a test. */
#define UI_RENDER_NO_MAIN
#include "ui_render.c"

#define T_ 255u                                        /* hold the previous note (a TIE step) */
#define MAXS 64u
#define AT(i, ...) [i] = {__VA_ARGS__}
typedef struct { uint8_t n[MAXS][4]; } steps_t;

static uint32_t preset_of(uint32_t e, const char *name)
{
    uint32_t i;
    for (i = 0; i < ENGINES[e]->npresets; i++)
        if (!strcmp(ENGINES[e]->presets[i].name, name))
            return i;
    fprintf(stderr, "no preset %s\n", name);
    exit(1);
}
static void part(uint32_t k, uint32_t eng, const char *name, uint32_t div, uint32_t len, uint32_t rev, uint32_t level,
                 const steps_t *st)
{
    track_t *t = &trk[k];
    uint32_t i, n;
    memset(t->step, 0, sizeof t->step);
    host_preset(t, eng, preset_of(eng, name));
    t->p[P_SDIV] = (int16_t)div;                       /* params.c N_DIV: 0 1/4, 1 1/8, 6 1/2, 7 1/1 */
    t->p[P_SLEN] = (int16_t)len;
    t->p[P_SGATE] = 127;
    t->p[P_REV] = (int16_t)rev;
    t->p[P_LEVEL] = (int16_t)level;
    t->p[P_MUTE] = 0;
    for (i = 0; i < len; i++) {
        const uint8_t *s = st ? st->n[i] : (const uint8_t *)"\0\0\0";
        for (n = 0; n < 4u && s[n] && s[n] != T_; n++)
            ;
        if (s[0] == T_)
            put_step(t, i, 0, s, ST_TIE, 0);
        else
            put_step(t, i, n, s, n ? ST_NOTE : ST_REST, 0);
    }
}
static void silence(uint32_t k) { part(k, 0, "CRYPT PAD", 0, 1, 0, 0, 0); }
static void show_page(const char *title)
{
    uint32_t i;
    for (i = 0; i < NPAGES && strcmp(PAGES[i].title, title); i++)
        ;
    ui.home = 0;
    ui.page = (uint8_t)i;
    page_entered();
}
static void show_home(void) { ui.home = 1; }

/* ---------------------------------------------------------------- the music (D) */
#define DM {50, 53, 57}
#define BB {46, 50, 53}
#define GM {43, 46, 50}
#define AA {45, 49, 52}
#define FF {45, 48, 53}
/* II: the theme on the gurdy, 1/4, 8 bars; the pad 1/1 */
static const steps_t II_GURDY = {{{69}, {T_}, {74}, {72}, {69}, {T_}, {T_}, {0}, {70}, {69}, {67}, {65}, {67}, {T_}, {T_}, {0},
                                  {65}, {67}, {69}, {70}, {69}, {T_}, {67}, {65}, {64}, {T_}, {65}, {64}, {62}, {T_}, {T_}, {0}}};
static const steps_t II_PAD = {{DM, {T_}, BB, {T_}, GM, {T_}, AA, {T_}}};
/* III: long tones for the crank, 1/4, 8 bars */
static const steps_t III_GURDY = {{{69}, {T_}, {T_}, {T_}, {67}, {T_}, {65}, {T_}, {64}, {T_}, {T_}, {T_}, {62}, {T_}, {T_}, {T_},
                                   {65}, {T_}, {67}, {T_}, {69}, {T_}, {T_}, {T_}, {67}, {65}, {64}, {65}, {62}, {T_}, {T_}, {T_}}};
static const steps_t III_PAD = {{DM, {T_}, BB, {T_}, DM, {T_}, AA, {T_}}};
/* IV: the organ, 1/1, the same four chords twice (EQUAL, then JUST) */
static const steps_t IV_ORGAN = {{{50, 57, 62, 65}, {46, 53, 58, 62}, {43, 50, 55, 58}, {45, 52, 57, 61},
                                  {50, 57, 62, 65}, {46, 53, 58, 62}, {43, 50, 55, 58}, {45, 52, 57, 61}}};
/* V: D Hungarian minor (D E F G# A Bb C#): the vielle 1/4, 12 bars; the monks 1/1; tomb drums 1/8 */
static const steps_t V_VIELLE = {{{69}, {T_}, {68}, {69}, {70}, {T_}, {69}, {68}, {65}, {T_}, {64}, {65}, {62}, {T_}, {T_}, {0},
                                  {74}, {T_}, {73}, {74}, {70}, {T_}, {69}, {70}, {68}, {69}, {70}, {73}, {74}, {T_}, {T_}, {0},
                                  {73}, {70}, {69}, {68}, {65}, {64}, {65}, {68}, {69}, {T_}, {T_}, {T_}, {62}, {T_}, {T_}, {T_}}};
static const steps_t V_MONKS = {{DM, {T_}, BB, AA, DM, {T_}, BB, AA, BB, AA, DM, {T_}}};
static const steps_t V_DRUM = {{AT(0, 36), AT(11, 45), AT(16, 36), AT(29, 36), AT(32, 36), AT(43, 45), AT(48, 36), AT(56, 56),
                                AT(61, 36)}};

/* ---------------------------------------------------------------- the timeline */
enum { S_CAVE, S_LATCH, S_CRANK, S_TEMP, S_SCALE, NSEC };
static const struct { const char *num, *title, *how, *sounds; uint32_t bpm; double secs; } SEC[NSEC] = {
    {"I", "The Cave", "FX > TAPE: AMB and CRKL", "cave wind  ·  torch crackle", 60, 14.0},
    {"II", "The Drone Latch", "hold GLO, press MONO: T4's chord stays", "T4 CATHEDRAL  ·  T1 HURDY GURDY  ·  T2 CRYPT PAD", 66, 8 * 4 * 60.0 / 66},
    {"III", "The Crank", "GURDY COUP > CRANK: SELECT turns the wheel", "T1 HURDY GURDY, cranked by hand", 60, 8 * 4 * 60.0 / 60},
    {"IV", "Pure Tunings", "SCL > CHORD: TEMP  (equal, then just)", "T2 PIPE ORGAN  ·  the drone", 54, 8 * 4 * 60.0 / 54},
    {"V", "Dungeon Scales", "D Hungarian minor  ·  12 voices", "T1 VIELLE  ·  T2 MONKS  ·  T3 TOMB DRUMS", 72, 12 * 4 * 60.0 / 72},
};
static const uint8_t DRONE[3] = {38, 45, 50};          /* D2 A2 D3 */

static void frame_out(FILE *v)
{
    static uint8_t rgb[240u * 240u * 3u];
    uint32_t i;
    ui.force = 1;
    ui.hot_t = 0;
    ui.bpm_t = 0;
    ui_draw();
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = swap16(host_screen[i]);
        rgb[3 * i] = (uint8_t)((c >> 11) * 255u / 31u);
        rgb[3 * i + 1] = (uint8_t)(((c >> 5) & 63u) * 255u / 63u);
        rgb[3 * i + 2] = (uint8_t)((c & 31u) * 255u / 31u);
    }
    fwrite(rgb, 1, sizeof rgb, v);
}

/* the crank's speed (clicks a second) at s seconds into III: a slow start, a swell, wrist strokes, a stop, again */
static double crank_speed(double s, double beat)
{
    double b = s / beat;                               /* beats into the section */
    if (b < 1.0) return 0;                             /* the notes play, nothing sounds: the wheel is still */
    if (b < 4.0) return 3 + 9 * (b - 1) / 3;           /* turning up */
    if (b < 11.0) return 12 + 2 * sin(b * 1.3);
    if (b < 12.0) return 12 * (12 - b);                /* slowing to a stop */
    if (b < 13.0) return 0;                            /* a breath */
    if (b < 14.0) return 14;
    if (b < 26.0) return 14 + 1.5 * sin(b * 0.9);
    if (b < 28.0) return 14 + ((int)(b * 2) % 2 ? 12 : 0);   /* strokes on the eighths: the trompette chatters */
    if (b < 31.0) return 14 - 4 * (b - 28);
    return 0;
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "demo_2_0.wav";
    FILE *f, *v = argc > 2 ? fopen(argv[2], "wb") : 0, *cue;
    const double tail = 8.0;
    double start[NSEC + 1], t_s, next_click = 0;
    uint32_t s, i, t, frames, next_frame = 0, cur = NSEC, fr0 = 0;
    int32_t o[2 * CTL];
    char cpath[600];
    rep = fopen("/dev/null", "w");
    ui_power_on();
    pal(UI_CRYPT_INDEX);
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    song.g[G_RTYPE] = 2;                               /* HALL */
    fx.rtype = 2;
    song.g[G_RDAMP] = 70;
    song.g[G_RSIZE] = 105;
    song.g[G_TAPE] = 30;
    song.g[G_AMB] = 0;
    song.g[G_CRKL] = 55;
    song.batt_raw = 600;
    usb.config = 0;
    rev_clear();
    hall_clear();
    memset(lt, 0, sizeof lt);
    for (i = 0; i < NTRK; i++)
        silence(i);
    for (s = 0, start[0] = 0; s < NSEC; s++)
        start[s + 1] = start[s] + SEC[s].secs + (s ? 0.6 : 0);
    frames = (uint32_t)((start[NSEC] + tail) * FS);
    if (!(f = fopen(path, "wb")))
        return 1;
    wav_hdr(f, frames);
    snprintf(cpath, sizeof cpath, "%s.cues", path);
    if ((cue = fopen(cpath, "w"))) {
        for (s = 0; s < NSEC; s++)
            fprintf(cue, "%.3f\t%s\t%s\t%s\t%s\n", start[s], SEC[s].num, SEC[s].title, SEC[s].how, SEC[s].sounds);
        fprintf(cue, "%.3f\tEND\t-\t-\t-\n", frames / (double)FS);
        fclose(cue);
    }
    show_page("TAPE");                                  /* I: the cave, on its page */
    for (t = 0; t < frames; t += CTL) {
        t_s = t / (double)FS;
        for (s = 0; s < NSEC && t_s >= start[s + 1]; s++)
            ;
        if (s != cur && s < NSEC) {                    /* a section starts: what a player would load and press */
            cur = s;
            song.g[G_BPM] = (int16_t)SEC[s].bpm;
            transport_req = 2;
            switch (s) {
            case S_LATCH:
                part(3, 7, "CATHEDRAL", 7, 1, 100, 70, 0);
                trk[3].p[P_VOICE] = V_POLY;
                part(0, ENGI_GURDY, "HURDY GURDY", 0, 32, 75, 92, &II_GURDY);
                part(1, 0, "CRYPT PAD", 7, 8, 100, 70, &II_PAD);
                lt[3].on = 1;                          /* GLO + OP6: T4 latched (the map shows it) */
                song.sel = 3;
                ui.layer = ui.ly = LAYER_GLO;
                break;
            case S_CRANK:
                part(0, ENGI_GURDY, "HURDY GURDY", 0, 32, 80, 100, &III_GURDY);
                trk[0].p[P_E5] = (int16_t)GURDY_CRANK;
                part(1, 0, "CRYPT PAD", 7, 8, 100, 56, &III_PAD);
                song.sel = 0;
                show_page("EDIT 2");                   /* E5: COUP CRANK on screen */
                break;
            case S_TEMP:
                silence(0);
                part(1, 12, "PIPE ORGAN", 7, 8, 110, 80, &IV_ORGAN);
                trk[1].p[P_ROOT] = 2;                  /* D: TEMP tunes from the root */
                trk[3].p[P_ROOT] = 2;
                trk[1].p[P_TEMP] = trk[3].p[P_TEMP] = 0;   /* EQUAL */
                song.sel = 1;
                show_page("CHORD");
                break;
            case S_SCALE:
                part(0, ENGI_GURDY, "VIELLE", 0, 48, 80, 96, &V_VIELLE);
                trk[0].p[P_ROOT] = 2;
                trk[0].p[P_SCALE] = 17;                /* HUNG */
                part(1, 5, "MONKS", 7, 12, 110, 74, &V_MONKS);
                part(2, ENGI_DRUM, "TOMB DRUMS", 1, 64, 95, 90, &V_DRUM);
                trk[1].p[P_TEMP] = trk[3].p[P_TEMP] = 0;
                song.sel = 0;
                show_page("SCL");
                break;
            }
            if (s != S_CAVE)
                transport_req = 1;
            fr0 = t;
        }
        {   /* within a section */
            double in = t_s - start[cur], beat = 60.0 / SEC[cur].bpm;
            if (cur == S_CAVE) {
                song.g[G_AMB] = (int16_t)clamp((int32_t)(in / 9.0 * 70), 0, 70);
            } else if (cur == S_LATCH) {
                if (t == fr0)                              /* the chord pressed, held, let go: it stays */
                    for (i = 0; i < 3u; i++)
                        input_on(&trk[3], DRONE[i], 100);
                if (in >= 0.5 && in < 0.5 + (double)CTL / FS)
                    for (i = 0; i < 3u; i++)
                        input_off(&trk[3], DRONE[i]);
                if (in > 4.0 && ui.layer) {                /* GLO let go: HOME, the melody's track */
                    ui.layer = ui.ly = 0;
                    show_home();
                    song.sel = 0;
                }
                if (in > 12.0 && in < 20.0) song.sel = 1;
                else if (in >= 20.0) song.sel = 0;
                song.g[G_AMB] = (int16_t)clamp((int32_t)(70 - in * 2), 40, 70);
            } else if (cur == S_CRANK) {
                double sp = crank_speed(in, beat);
                uint32_t fr = (uint32_t)(t_s * CRANK_FPS);
                if (sp > 0.5 && t_s >= next_click) {
                    crank_clicks(1, fr);
                    next_click = t_s + 1.0 / sp;
                } else if (sp <= 0.5) {
                    next_click = t_s;
                }
                crank_idle(fr);
            } else if (cur == S_TEMP) {
                int16_t tmp = in >= 4 * 4 * beat ? 3 : 0;  /* bars 5..8: JUST */
                trk[1].p[P_TEMP] = trk[3].p[P_TEMP] = tmp;
            } else if (cur == S_SCALE) {
                if (in > 3 * 4 * beat && !ui.home) {
                    show_home();
                }
                song.sel = in > 6 * 4 * beat && in < 9 * 4 * beat ? 1 : 0;
            }
        }
        if (cur == S_SCALE && t_s >= start[NSEC] && lt[3].on) {   /* the end: the latch let go, the drone ends */
            lt[3].on = 0;
            transport_req = 2;
        }
        if (t_s >= start[NSEC] && song.g[G_AMB] > 0)              /* the wind dies away in the hall's tail */
            song.g[G_AMB] = (int16_t)clamp((int32_t)(40 - (t_s - start[NSEC]) * 7), 0, 40);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            wav_put(f, o[2 * i], o[2 * i + 1]);
            scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)clamp(o[2 * i], -32768, 32767);
        }
        if (v && t + CTL > next_frame) {
            fm1_ms = (uint32_t)((uint64_t)t * 1000u / FS);
            frame_out(v);
            next_frame += FS / 30u;
        }
    }
    fclose(f);
    if (v)
        fclose(v);
    printf("%s: %.1f s, voices given up %u\n", path, frames / (double)FS, voice_kills);
    return 0;
}
