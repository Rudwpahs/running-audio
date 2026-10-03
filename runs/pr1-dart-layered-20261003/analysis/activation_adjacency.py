"""Gate C1 diagnostic: are control-adjacent losses really map-activation-adjacent?

In Gate C a map proposal is emitted right after the previous map activates, so "near a
control event" and "near an activation" overlap. This splits C losses by the nearest
preceding event of each kind (raw logs only).
"""
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parents[1]
W = 3  # frames after the event (0..W)


def lost_frames(rx: str):
    t_end = int(re.findall(r"PR1T v=1 t_us=(\d+) field=crc_good", rx)[-1])
    out = []
    for m in re.finditer(r"PR1HA t_us=(\d+) kind=(\d+) logical=(\d+) seq=(\d+) ch=(\d+)", rx):
        t, k, L, n, ch = map(int, m.groups())
        if t <= t_end and k in (3, 5, 8):
            out += [(L + i, k) for i in range(1 if k == 3 else n)]
    return out


def activations(rx: str):
    # PR1QA keeps the first 32; the PR1QE Activated events (kind 4) also carry the frame.
    acts = {int(a) for a in re.findall(r"PR1QA v=\d+ activation=(\d+)", rx)}
    acts |= {int(L) for L in re.findall(r"PR1QE t_ms=\d+ kind=4 logical=(\d+)", rx)}
    return sorted(acts)


def probes(rx: str):
    return sorted({int(L) for L in re.findall(r"PR1QE t_ms=\d+ kind=9 logical=(\d+)", rx)})


def ctrl_frames(ctrl: str):
    return sorted({int(x) for x in re.findall(r" (?:RX|TX) PR1(?:PROP|COMMIT|ACK|STAGED|ABORTED)[^\n]*_logical=(\d+)", ctrl)})


def near(L, evs):
    return any(0 <= L - e <= W for e in evs)


def main(dirs):
    tot = {"lost": 0, "act": 0, "ctrl_only": 0, "probe": 0, "act_windows": 0, "ctrl_windows": 0, "packets": 0}
    for d in dirs:
        rx = (d / "rx.log").read_text(errors="replace")
        ctrl = (d / "ctrl.log").read_text(errors="replace") if (d / "ctrl.log").exists() else ""
        lost = lost_frames(rx)
        acts, ctl, prb = activations(rx), ctrl_frames(ctrl), probes(rx)
        a = sum(1 for L, _ in lost if near(L, acts))
        c = sum(1 for L, _ in lost if near(L, ctl) and not near(L, acts))
        p = sum(1 for L, _ in lost if near(L, prb) and not near(L, acts))
        print(f"{d.name}: lost={len(lost)} activations={len(acts)} ctrl_events={len(ctl)} "
              f"near_activation={a} near_ctrl_not_activation={c} near_probe_not_activation={p}")
        for k, v in (("lost", len(lost)), ("act", a), ("ctrl_only", c), ("probe", p),
                     ("act_windows", len(acts)), ("ctrl_windows", len(ctl))):
            tot[k] += v
    print("TOTAL", tot)


if __name__ == "__main__":
    main([Path(p) for p in sys.argv[1:]])
