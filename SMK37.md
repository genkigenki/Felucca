# RAINBOW37: Rainbow mode on the M-Vave SMK-37

Rainbow mode (see [RAINBOW.md](RAINBOW.md)) on the SMK-37 Pro, SMK-37 Elite and MKE-P37 (the Starrykey 37 Play
uses the Pro build). Unofficial; the FM-1 build is unchanged. SMK-37 port by genkigenki.

## Controls (the toy)

| SMK-37 | Does |
|---|---|
| Keys F3..G5 | the FM-1's 27 keys: the friend sings, the letter shows |
| Keys C3..E3, G#5..C6 | more notes; the friend hops |
| 16 pads | the white keys F3..G5 (low row at the bottom), in their note colours; bright while held or when FOLLOW, GUESS or CHORDS light the key |
| K1 | volume (never above the grown-ups' VOLUME) |
| K2 / K3 / K4 | beat speed / how the keys play / next friend |
| K5..K8 and faders 1..4 | BIG/SMALL, DAY/NIGHT, ECHO, WIGGLE (a fader takes over once moved) |
| Pitch wheel | a slide, up to 2 semitones |
| Mod wheel | more wiggle |
| FX, SCALE, CHORD, NOTE REPEAT, PARA, GLOBE | HICCUP, BACKWARDS, SLEEPY, SQUEAKY, GIANT, FREEZE |
| ARP / SEQ / SEQ PLAY / SEQ REC | sparkle / next beat (or song, lesson) / beat on-off / next sky |
| PATCH (HOME) | a surprise friend |
| BT (SAVE) | WRITE her own song, and back |
| OCTAVE- / OCTAVE+ | bigger / smaller |
| KNOB BANK | a surprise friend |
| FADER BANK | the next sky |
| < / > | the friend before / after |

All keys and pads play at one fixed velocity. No menu opens from the toy.

Grown-ups: PATCH held 1 s, then OCTAVE- / OCTAVE+ sets the VOLUME cap (PRESETS or < > the look-alikes);
PATCH + BT held 3 s opens the full Felucca until the next power-on.

## Build

```sh
BOARD=smk37 sh build.sh                 # build/rainbow37.fwsc (SMK-37 Pro, Starrykey)
BOARD=smk37 MODEL=elite sh build.sh     # build/rainbow37-elite.fwsc
BOARD=smk37 MODEL=mke sh build.sh       # build/rainbow37-mke.fwsc
sh build.sh                             # the FM-1, last: the tests use its files
sh tests/run_tests.sh
```

## Install and roll back

Make a full flash backup first (see the fm1-to-smk37 guide, `docs/02-update-and-recovery.md`). Install with
`tools/fm1_install.py PACKAGE` or the web installer. The first start clears the stock firmware's settings area and
any other ported firmware's store (calibration and the way back to stock are kept). To go back, install M-Vave's
factory package for your model with M-Vave's updater.

No TRS MIDI IN on the SMK (that pin is a key row).
