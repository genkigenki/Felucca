/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* AMBIENT.md: the screen while MAKU is on, 240 x 240 in four 119 x 119 quadrants, laid out like the knobs:
 *   top left      the focused track (its number and name), the waveform, the key (ROOT and SCALE, always: OCT- / OCT+
 *                 move it) and BPM on one line, the four tracks' volumes (KNOB 1..4 left to right), the focus
 *   top right     SELECT's macro of the focused track (DENSITY: the kick's 16 steps too)
 *   bottom left   PRESETS' macro
 *   bottom right  ALGORITHM's macro
 * A verb held (GLO tempo, SCL root, PLAY sink; ui_input.c maku_verb_now): the three quadrants show its three parameters
 * instead (SELECT / PRESETS / ALGORITHM move them). One style for every mode. The scope is drawn each
 * frame, the other three only when something they show changes (or ui.force). */
static int maku_knobs_on(void);
#define MKQ 119
static const char *const MAKU_TRK_NAME[4] = {"PULSE", "FLOOR", "HAZE", "VOICE"};
static const char *const MAKU_ROLE[3] = {"SELECT", "PRESETS", "ALGORITHM"};
static const char MAKU_NUM[4][2] = {"1", "2", "3", "4"};

/* each channel's colour (the black and white MONO: the theme's; GREY too shows them, this screen's one exception to
 * its all-gray rule), shown wherever that channel is: its volume bar, its number, the
 * waveform and the macros while it is focused */
static uint16_t mk_col(uint32_t i)
{
    static const uint16_t C[4] = {RGB(255, 112, 88), RGB(255, 190, 56), RGB(72, 214, 130), RGB(92, 168, 255)};
    return settings.palette == UI_BW_INDEX ? T_THEME : C[i & 3u];
}

/* a toward b by pct %, per channel (ux_mix would gray it in GREY) */
static uint16_t mk_mix(uint16_t a, uint16_t b, int32_t pct)
{
    uint32_t r = (uint32_t)((a >> 11) * (100 - pct) + (b >> 11) * pct) / 100u;
    uint32_t g = (uint32_t)(((a >> 5) & 63u) * (100 - pct) + ((b >> 5) & 63u) * pct) / 100u;
    uint32_t bl = (uint32_t)((a & 31u) * (100 - pct) + (b & 31u) * pct) / 100u;
    return (uint16_t)((r << 11) | (g << 5) | bl);
}

static void mq_gauge(int32_t x, int32_t y, int32_t w, int32_t v, uint16_t c)
{
    cv_rect(x, y, w, 4, T_LINE);
    cv_rect(x, y, w * v / 127, 4, c);
}

/* the macro j of the focused track, in its quadrant */
static void mq_macro(uint32_t j, uint32_t f, uint32_t tempo)
{
    char b[8];
    const char *name = tempo ? "BPM" : MAKU_MAC[f][j].name;
    int32_t v = tempo ? song.g[G_BPM] : maku.m[f][j];
    uint16_t cc = mk_col(f), c = ui.hot_t && ui.hot_col == j ? T_ACCENT : cc;
    cv_text(8, 6, &AF_S, MAKU_ROLE[j], T_DIM);          /* (the knob's name only: the track is in the top left) */
    cv_text(8, 24, &AF_M, name, T_MID);
    fmt_int(b, v);
    cv_text_in(0, 48, MKQ, &AF_L, b, c, T_BG);
    mq_gauge(8, 92, MKQ - 16, tempo ? (v - 40) * 127 / 200 : v, c);
    if (!tempo && f == MAKU_KICK && j == 0) {           /* the kick's 16 steps: set, ghost, the playhead */
        uint32_t m = maku_kick_mask(), g = maku_kick_ghosts(), i, ph = song.playing ? trk[MAKU_KICK].seq_idx % 16u : 0xFFu;
        for (i = 0; i < 16u; i++) {
            int32_t x = 5 + (int32_t)i * 7;
            uint16_t col = (m >> i) & 1u ? cc : (g >> i) & 1u ? T_MID : T_LINE;
            cv_rect(x, 104, 5, i == ph ? 12 : 8, i == ph ? T_ACCENT : col);
        }
    }
}

/* the verbs that hold three parameters (maku.c VB_*): their names, as printed on the button where there is one */
static const struct { const char *title, *name[3]; } MAKU_VB[VB_N] = {
    {"", {"", "", ""}},
    {"SINK", {"SINK", "WASH", "HUSH"}},                 /* PLAY: the reverb feeds back, the sends rise, the kick steps out */
    {"TEMPO", {"BPM", "SWING", "DUCK"}},                /* GLO */
    {"ROOT", {"ROOT", "SCALE", "REG"}},                 /* SCL */
    {"KEEP", {"LEN", "HOLD", "ECHO"}},                  /* SAVE: the arp loops its last steps, the drones stay */
    {"SCRAMBLE", {"ARP", "KICK", "SWING"}},             /* SEQ */
    {"CASCADE", {"DENSE", "SPAN", "ECHO"}},             /* ARP */
    {"DRIFT", {"RATE", "RANGE", "REACH"}},            /* FX: the macros (and from REACH 43 the space, from 86 the tempo) wander while held */
    {"TWIST", {"DENS", "TONE", "WASH"}},                /* EDIT: every track's macros of that knob */
    {"SWELL", {"ATTACK", "RELEASE", "REV"}},            /* ENV: longer attacks and releases, a deeper reverse swell */
    {"WOBBLE", {"RATE", "DEPTH", "CHORUS"}},            /* LFO: the filter LFO faster and deeper, more chorus */
};
static const char *const MAKU_REG[3] = {"LOW", "MID", "HIGH"};

/* the value of parameter j of verb v: the text, and the gauge 0..127 */
static int32_t maku_vb_value(uint32_t v, uint32_t j, char *b)
{
    int32_t g = maku.vp[v][j], i;
    if (v == VB_GLO && j == 0u) {
        fmt_int(b, song.g[G_BPM]);
        return (song.g[G_BPM] - 40) * 127 / 200;
    }
    if (v == VB_ROOT) {
        if (j == 0u) {
            str_cpy(b, N_NOTE[(uint32_t)trk[MAKU_DRA].p[P_ROOT] % 12u], 8);
            return (trk[MAKU_DRA].p[P_ROOT] % 12) * 127 / 11;
        }
        if (j == 1u) {
            for (i = 0; i < (int32_t)MAKU_NSCALE - 1 && MAKU_SCALES[i] != (uint32_t)trk[MAKU_DRA].p[P_SCALE]; i++)
                ;
            str_cpy(b, N_SCALE[clamp(trk[MAKU_DRA].p[P_SCALE], 0, 15)], 8);
            return i * 127 / ((int32_t)MAKU_NSCALE - 1);
        }
        str_cpy(b, MAKU_REG[g % 3], 8);
        return g * 63;
    }
    if (v == VB_KEEP && j == 0u)
        fmt_int(b, 2 + g * 14 / 127);                    /* LEN: steps */
    else
        fmt_int(b, g);
    return g;
}

/* parameter j of the verb v held, in its quadrant (the same style as a macro) */
static void mq_verb(uint32_t j, uint32_t v)
{
    char b[8];
    int32_t g = maku_vb_value(v, j, b);
    cv_text(8, 6, &AF_S, MAKU_ROLE[j], T_DIM);
    if (j == 0u)                                        /* (the verb's name once, where SELECT is) */
        cv_text(MKQ - 6 - text_w(&AF_S, MAKU_VB[v].title), 6, &AF_S, MAKU_VB[v].title, T_ACCENT);
    cv_text(8, 24, &AF_M, MAKU_VB[v].name[j], T_MID);
    cv_text_in(0, 48, MKQ, &AF_L, b, T_ACCENT, T_BG);
    mq_gauge(8, 92, MKQ - 16, g, T_ACCENT);
}

static void mq_top_left(void)
{
    char b[8];
    uint32_t i, f = song.sel < 4u ? song.sel : 0u;
    int32_t peak = 1500, x, py, cy = 34, a = 10;
    static int16_t snap[SCOPE_N];
    uint32_t w = scope_w, trig = 0;
    for (i = 0; i < SCOPE_N; i++) {
        snap[i] = scope_buf[(w + i) & (SCOPE_N - 1u)];
        if (snap[i] > peak)
            peak = snap[i];
        else if (-snap[i] > peak)
            peak = -snap[i];
    }
    for (i = 1; i < SCOPE_N - (uint32_t)MKQ; i++)
        if (snap[i - 1] < 0 && snap[i] >= 0) {
            trig = i;
            break;
        }
    cv_begin(MKQ, MKQ, T_BG);
    cv_rect(0, cy, MKQ, 1, T_RAISE);
    py = cy;
    for (x = 0; x < MKQ; x++) {
        int32_t y = cy - snap[trig + (uint32_t)x] * a / peak;
        if (x)
            cv_line_t(x - 1, py, x, y, mk_col(f), 2);
        py = y;
    }
    {                                                   /* the focused track, "4 VOICE" (a message in its place a while) */
        char t[16];
        str_cpy(t, MAKU_NUM[f], sizeof t);
        str_cpy(t + str_len(t), " ", sizeof t - str_len(t));
        str_cpy(t + str_len(t), MAKU_TRK_NAME[f], sizeof t - str_len(t));
        if (ui.msg_t)                                   /* (the small face: a message may be long) */
            cv_text(6, 8, &AF_S, ui.msg, T_ACCENT);
        else
            cv_text(6, 4, &AF_M, t, mk_col(f));
    }
    {                                                   /* the key, whatever else the screen shows, on BPM's line */
        char k[16];
        str_cpy(k, N_NOTE[(uint32_t)trk[MAKU_DRA].p[P_ROOT] % 12u], sizeof k);
        str_cpy(k + str_len(k), " ", sizeof k - str_len(k));
        str_cpy(k + str_len(k), N_SCALE[clamp(trk[MAKU_DRA].p[P_SCALE], 0, (int32_t)(sizeof N_SCALE / sizeof N_SCALE[0]) - 1)],
                sizeof k - str_len(k));
        cv_text(6, 52, &AF_S, k, T_TEXT);
    }
    fmt_int(b, song.g[G_BPM]);
    {
        int32_t bw = text_w(&AF_M, b);
        cv_text(MKQ - 6 - bw, 48, &AF_M, b, ui.bpm_t ? T_ACCENT : T_TEXT);
        cv_text(MKQ - 6 - bw - 4 - text_w(&AF_S, "BPM"), 52, &AF_S, "BPM", T_DIM);
    }
    for (i = 0; i < 4u; i++) {                          /* the four volumes: wide bars, numbered, each its colour */
        int32_t bx = 4 + (int32_t)i * 29, bh = 38, h = trk[i].p[P_LEVEL] * bh / 127;
        uint16_t cc = mk_col(i), c = i == f ? (ui.hot_t && ui.hot_col == i ? T_ACCENT : cc) : mk_mix(cc, T_BG, 55);
        cv_rect(bx, 64, 25, bh, T_LINE);
        cv_rect(bx, 64 + bh - h, 25, h, c);
        cv_text(bx + (25 - text_w(&AF_S, MAKU_NUM[i])) / 2, 104, &AF_S, MAKU_NUM[i], i == f ? cc : T_MID);
        if (i == f)
            cv_rect(bx, 116, 25, 2, cc);
    }
    cv_blit(0, 0);
}

static void maku_draw(void)
{
    static uint32_t sig;
    uint32_t j, f = song.sel < 4u ? song.sel : 0u, v = ui.layer == LAYER_GLO ? VB_GLO : ui.layer == LAYER_SCL ? VB_ROOT : (uint32_t)maku.verb, s;
    s = f * 977u + v * 13u + (uint32_t)song.g[G_BPM] * 31u + (uint32_t)(ui.hot_t != 0u) * 7u + ui.hot_col;
    for (j = 0; j < 3u; j++)
        s = s * 31u + maku.m[f][j] + (v ? (uint32_t)maku.vp[v][j] * 7u : 0u);
    s = s * 31u + (uint32_t)trk[MAKU_DRA].p[P_ROOT] * 17u + (uint32_t)trk[MAKU_DRA].p[P_SCALE];
    s = s * 31u + (uint32_t)maku_kick_mask() + (song.playing ? trk[MAKU_KICK].seq_idx % 16u + 1u : 0u) * 4099u +
        maku_kick_ghosts() * 7u;
    for (j = 0; j < 4u; j++)
        s = s * 31u + (uint32_t)trk[j].p[P_LEVEL];
    if (ui.force)
        lcd_fill(0, 0, 240, 240, T_BG);
    mq_top_left();
    if (ui.force || s != sig) {
        sig = s;
        for (j = 0; j < 3u; j++) {
            cv_begin(MKQ, MKQ, T_BG);
            if (v)
                mq_verb(j, v);
            else
                mq_macro(j, f, 0u);
            cv_blit(j == 1u ? 0u : 121u, j == 0u ? 0u : 121u);
        }
        lcd_fill(119, 0, 2, 240, T_LINE);
        lcd_fill(0, 119, 240, 2, T_LINE);
    }
    if (ui.msg_t && !--ui.msg_t && ui.msg2[0]) {
        str_cpy(ui.msg, ui.msg2, sizeof ui.msg);
        ui.msg2[0] = 0;
        ui.msg_t = 60;
    }
    if (ui.hot_t)
        ui.hot_t--;
    if (ui.bpm_t)
        ui.bpm_t--;
    ui.force = 0;
}
