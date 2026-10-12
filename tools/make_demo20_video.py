#!/usr/bin/env python3
"""GHOULBOX 2.0 demo video: build/host/demo_2_0 DIR/piece.wav DIR/frames.rgb, then  python3 tools/make_demo20_video.py DIR
-> DIR/ghoulbox-demo-2.0.mp4 (1920x1080: the device screen, 4x, beside each section's title, how it is done and its
sounds, from piece.wav.cues; the address over the hall's tail)."""
import os
import subprocess
import sys

D = sys.argv[1] if len(sys.argv) > 1 else "."
F = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "fonts") + "/"
os.chdir(D)
pir, vt = F + "PirataOne-Regular.ttf", F + "VT323-Regular.ttf"
cues = [line.rstrip("\n").split("\t") for line in open("piece.wav.cues")]
end = float(cues[-1][0])
tail = end - 8.0                                        # (demo_2_0.c: the hall's tail, 8 s)


def esc(s):
    return s.replace("\\", "\\\\").replace(":", "\\:").replace("'", "’").replace("%", "\\%")


f = ["[0:v]scale=960:960:flags=neighbor,pad=1920:1080:80:60:color=0x0d0b08[v0]"]
d = [f"drawtext=fontfile={pir}:text='GHOULBOX 2.0':fontsize=104:fontcolor=0xee8a3c:x=1110:y=86",
     f"drawtext=fontfile={vt}:text='DUNGEON-SYNTH FIRMWARE FOR THE M-VAVE FM-1':fontsize=34:fontcolor=0xa4967e:x=1114:y=226"]
for k in range(len(cues) - 1):
    t0, num, title, how, sounds = cues[k]
    t1 = min(float(cues[k + 1][0]), tail) if k == len(cues) - 2 else float(cues[k + 1][0])
    en, al = f"enable='between(t,{t0},{t1})'", f"alpha='min(1,(t-{t0})/1.2)'"
    d.append(f"drawtext=fontfile={vt}:text='{num}':fontsize=44:fontcolor=0xa4967e:x=1114:y=370:{en}:{al}")
    d.append(f"drawtext=fontfile={pir}:text='{esc(title)}':fontsize=84:fontcolor=0xd9584a:x=1110:y=420:{en}:{al}")
    d.append(f"drawtext=fontfile={vt}:text='{esc(how)}':fontsize=36:fontcolor=0xe4d8bf:x=1114:y=552:{en}:{al}")
    for j, s in enumerate(sounds.split("  ·  ")):
        d.append(f"drawtext=fontfile={vt}:text='{esc(s)}':fontsize=40:fontcolor=0xee8a3c:x=1114:y={640 + j * 56}:{en}:{al}")
en, al = f"enable='gte(t,{tail})'", f"alpha='min(1,(t-{tail})/1.5)'"
d.append(f"drawtext=fontfile={pir}:text='Free and open source':fontsize=72:fontcolor=0xd9584a:x=1110:y=430:{en}:{al}")
d.append(f"drawtext=fontfile={vt}:text='play it in your browser, install it on your FM-1':fontsize=36:fontcolor=0xe4d8bf:x=1114:y=540:{en}:{al}")
d.append(f"drawtext=fontfile={vt}:text='ghoulbox.messy.quest':fontsize=60:fontcolor=0xee8a3c:x=1114:y=620:{en}:{al}")
d.append(f"drawtext=fontfile={vt}:text='ghoulbox.messy.quest':fontsize=30:fontcolor=0x6b6052:x=1114:y=990:enable='lt(t,{tail})'")
fc = f[0] + ";[v0]" + ",".join(d) + "[v];[1:a]loudnorm=I=-16:TP=-1.5:LRA=11,aresample=48000[a]"
subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", "240x240", "-r", "30",
                "-i", "frames.rgb", "-i", "piece.wav", "-filter_complex", fc, "-map", "[v]", "-map", "[a]",
                "-c:v", "libx264", "-crf", "20", "-preset", "medium", "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "192k",
                "-movflags", "+faststart", "-shortest", "ghoulbox-demo-2.0.mp4"], check=True)
print(f"{D}/ghoulbox-demo-2.0.mp4")
