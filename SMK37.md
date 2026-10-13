# MAKU on the M-Vave SMK-37

MAKU also builds for the M-Vave SMK-37, a 37-key controller with the same chip as the FM-1. The FM-1 build is
unchanged; everything for the SMK-37 is behind `BOARD_SMK37`. Status: builds and passes the host tests; not yet
tried on a unit.

Until this project's releases include SMK-37 packages, unofficial builds can be installed with the web installer:
https://genkigenki.github.io/smk37/

## Supported models

- **SMK-37 Pro**: the default build (package identity `SMK-37 Pro_900`).
- **SMK-37 Elite**: `MODEL=elite` (identity `SMK-37 Elite_900`).
- **MKE-P37**: `MODEL=mke` (identity `MKE-P37_900`).
- **Starrykey 37 Play**: uses the SMK-37 Pro build.

All of them are the same hardware. Each build carries its model's name, so M-Vave's updater still takes that
unit's stock package later: install the package for your model. The installer takes a package for any of these
models, and refuses an FM-1 package (and an SMK package on an FM-1).

## Controls

MAKU plays at power-on as on the FM-1; the SMK-37's extra controls only add to it.

| SMK-37 | MAKU |
| --- | --- |
| K1 | MASTER (output volume) |
| K2 / K3 / K4 | the focused part's three macros (SELECT, PRESETS = TONE, ALGORITHM); with a verb held: RISE / HOLD / FADE |
| K5..K8 | volume of parts 1..4 (pulse, floor, haze, voice); the part turned gets the focus |
| Top row: ARP, NOTE REPEAT, SCALE, CHORD, GLOBE, BT | the verbs SINK, CASCADE, SWELL, WOBBLE, LIFT, SCRAMBLE |
| PATCH | HOME: held, a new world; with OCTAVE-, the settings menu |
| PARA | the key (the FM-1's ARP: a tap steps the scale; held, a key sets the root) |
| FX | nothing (the FM-1's SAVE) |
| SEQ | held: BPM, SWING, DUCK |
| SEQ PLAY | DRIFT on / off (lit while on); held: RATE, RANGE, REACH |
| SEQ REC | tap: like this world; held 2 s: forget every like |
| OCTAVE- / OCTAVE+ | a fifth down / up |
| KNOB BANK | the settings menu (again: closes it) |
| < / > | the world before (up to 8 back) / a new world |
| FADER BANK | nothing |
| 16 pads | the 16 likes: tap a lit pad for its world as it was liked; with the pulse focused, the pulse's 16 steps |
| Faders 1..4 | CASCADE, SWELL, SCRAMBLE as standing amounts (a button press goes on top), SPACE (the space DRIFT adds) |
| Pitch wheel | down: SINK, up: LIFT; back at rest when let go |
| Mod wheel | WOBBLE, as far as the wheel is up |
| Keys F3..G5 (the middle 27) | as on the FM-1, with velocity |
| Keys C3..E3 and G#5..C6 (the outer 10) | more notes for the focused part |
| OCTAVE- + OCTAVE+ held at power-on | HARDWARE CALIBRATION: teach the buttons and knobs again |

A fader acts only once it moves. The SMK-37's TRS jack is a MIDI output and this build does not use it; there is
no TRS MIDI IN, so MIDI clock follows over USB only.

The decisions behind this mapping: [docs/smk37-port.yaml](docs/smk37-port.yaml).

## Build

See [BUILDING.md](BUILDING.md) for the toolchain.

```sh
BOARD=smk37 ./build.sh                 # build/maku-smk37.fwsc (SMK-37 Pro, Starrykey 37 Play)
BOARD=smk37 MODEL=elite ./build.sh     # build/maku-smk37-elite.fwsc
BOARD=smk37 MODEL=mke ./build.sh       # build/maku-smk37-mke.fwsc
./build.sh                             # the FM-1 build again before tests/run_tests.sh (the tests use its files)
```

## Install

Connect the keyboard by USB and run, with the package for your model:

```sh
python3 tools/fm1_install.py build/maku-smk37.fwsc
```

`fm1_install.py` needs `mido` and `python-rtmidi` (`pip3 install mido python-rtmidi`). It checks that the package
is for the connected model.

## Going back to the stock firmware

Install M-Vave's firmware package for your model with M-Vave's updater, M-UPGRADE. It works directly, because
the installed package keeps the keyboard's model name. `tools/fm1_install.py` can also install the stock SMK-37 Pro
V016 package. Either way your likes are erased.

## Safety

- Back up first: have M-Vave's stock package for your model and M-UPGRADE ready before you install.
- Keep the USB cable connected until the install has finished and the keyboard has restarted.
- MAKU writes only its settings and its likes. The first start clears the stock firmware's settings, and any
  other firmware's data, in the shared data area.
- The firmware never writes the keyboard's calibration, Bluetooth pairing or key areas; the flash map refuses
  them at compile time and again in the flash driver.
- Installing other firmware is at your own risk.
