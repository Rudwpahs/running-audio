"""Gate C interleaved B/C statistics with loss attribution (reads only raw run logs)."""
import json, math, re, sys
from pathlib import Path

HERE = Path(__file__).resolve().parents[1]
RUNS = sorted((HERE / "gateC-interleave").glob("*-gap-150us-10000-i*"), key=lambda p: int(p.name.rsplit("-i", 1)[1]))


def lost_frames(rx: str):
    t_end = int(re.findall(r"PR1T v=1 t_us=(\d+) field=crc_good", rx)[-1])
    out = []
    for m in re.finditer(r"PR1HA t_us=(\d+) kind=(\d+) logical=(\d+) seq=(\d+) ch=(\d+)", rx):
        t, k, L, n, ch = map(int, m.groups())
        if t <= t_end and k in (3, 5, 8):
            out += [(L + i, k) for i in range(1 if k == 3 else n)]
    return out


def ctrl_frames(ctrl: str):
    ev = [int(x) for x in re.findall(r" (?:RX|TX) PR1(?:PROP|COMMIT|ACK|STAGED|ABORTED)[^\n]*_logical=(\d+)", ctrl)]
    return sorted(set(ev))


def ztest(m1, n1, m2, n2):
    p1, p2 = m1 / n1, m2 / n2
    p = (m1 + m2) / (n1 + n2)
    se = math.sqrt(p * (1 - p) * (1 / n1 + 1 / n2))
    z = (p1 - p2) / se if se else 0.0
    d = p1 - p2
    se_d = math.sqrt(p1 * (1 - p1) / n1 + p2 * (1 - p2) / n2)
    return z, (d - 1.96 * se_d, d + 1.96 * se_d)


rows = []
for d in RUNS:
    gate = d.name.split("-")[0]
    r = json.loads((d / "result.json").read_text())
    rx = (d / "rx.log").read_text(errors="replace")
    lost = lost_frames(rx)
    ctrl = ctrl_frames((d / "ctrl.log").read_text(errors="replace")) if (d / "ctrl.log").exists() else []
    near = [L for L, k in lost if any(0 <= L - e <= 3 for e in ctrl)]
    m = r["metrics"]
    rows.append({"run": d.name, "gate": gate, "packets": r["packet_count"], "missing": m["missing"],
                 "crc": m["crc_bad"], "timeout_frames": sum(1 for _, k in lost if k == 5),
                 "jump_frames": sum(1 for _, k in lost if k == 8), "ctrl_events": len(ctrl),
                 "lost_near_ctrl": len(near), "irq_to_ready_p99": m["irq_to_rx_ready_us_p99"],
                 "sched_miss": m["scheduler_misses"]})
for x in rows:
    print(x)
B = [x for x in rows if x["gate"] == "B"]; C = [x for x in rows if x["gate"] == "C"]
mb, nb = sum(x["missing"] for x in B), sum(x["packets"] for x in B)
mc, nc = sum(x["missing"] for x in C), sum(x["packets"] for x in C)
z, ci = ztest(mc, nc, mb, nb)
print(f"pooled B {mb}/{nb} = {100*mb/nb:.3f}%   pooled C {mc}/{nc} = {100*mc/nc:.3f}%   "
      f"C-B = {100*(mc/nc-mb/nb):+.3f} pp, 95% CI [{100*ci[0]:+.3f}, {100*ci[1]:+.3f}] pp, z = {z:.2f}")
cb, cc = sum(x["crc"] for x in B), sum(x["crc"] for x in C)
z2, ci2 = ztest(cc, nc, cb, nb)
print(f"CRC-bad: B {cb} ({100*cb/nb:.3f}%), C {cc} ({100*cc/nc:.3f}%), relative {100*(cc/nc)/(cb/nb)-100:+.0f}%, z = {z2:.2f}")
tb, tc = sum(x["timeout_frames"] for x in B), sum(x["timeout_frames"] for x in C)
z3, _ = ztest(tc, nc, tb, nb)
print(f"no-IRQ timeout frames: B {tb}, C {tc}, relative {100*(tc/nc)/(tb/nb)-100:+.0f}%, z = {z3:.2f}")
nn = sum(x["lost_near_ctrl"] for x in C)
print(f"C losses within 0..3 frames after a control-plane event: {nn}/{mc}; "
      f"C excluding those (diagnostic only): {mc-nn}/{nc} = {100*(mc-nn)/nc:.3f}%")
json.dump({"runs": rows, "pooled": {"B": [mb, nb], "C": [mc, nc], "z_C_minus_B": z, "ci95_pp": [100*ci[0], 100*ci[1]]}},
          open(HERE / "gateC-interleave" / "stats.json", "w"), indent=2)
