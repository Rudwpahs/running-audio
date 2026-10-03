"""C2 offline replay, step 1: per-frame outcome streams from static-map Gate B runs.

Each Gate B run hops over all 40 channels, so every channel is observed about every
40 frames. For each run this writes analysis/c2_streams/<set>__<run>.txt:

  # run=<id> L0=<first measured frame> L1=<last> period_us=<P> t0_ms=<lock time>
  <logical> <kind>        one line per lost frame (3 CRC, 5 no-IRQ timeout, 8 jump)

Every other frame in [L0, L1] was received. The replay tool recomputes each frame's
channel with the firmware scheduler (static all-40 map). Runs whose anomaly list
overflowed (losses incomplete) are skipped and listed.
"""
import json
import re
from pathlib import Path

HERE = Path(__file__).resolve().parents[1]
OUT = HERE / "analysis" / "c2_streams"
HA = re.compile(r"PR1HA t_us=(\d+) kind=(\d+) logical=(\d+) seq=(\d+) ch=(\d+)")

CORPUS = [  # plain static-map Gate B family at 150 us (no diagnostic TX/RX variants)
    "gate-B/B-gap-150us-10000-run1", "gate-B/B-gap-150us-10000-run2", "gate-B/B-gap-150us-10000-run3",
    "gate-B/B-gap-150us-10000-i2", "gate-B/B-gap-150us-10000-i4", "gate-B/B-gap-150us-10000-i9",
    "gate-B/B-gap-150us-10000-i10",
    "preC/B-gap-150us-10000-preC1", "preC/B-gap-150us-10000-preC2",
    "gateC-interleave/B-gap-150us-10000-i1", "gateC-interleave/B-gap-150us-10000-i3",
    "gateC-interleave/B-gap-150us-10000-i5",
    "gateC1-interleave/B1-gap-150us-10000-i1", "gateC1-interleave/B1-gap-150us-10000-i4",
    "gateC1-interleave/B1-gap-150us-10000-i7",
]


def extract(rel: str):
    d = HERE / rel
    rx = (d / "rx.log").read_text(errors="replace")
    r = json.loads((d / "result.json").read_text())
    t_end = int(re.findall(r"PR1T v=1 t_us=(\d+) field=crc_good", rx)[-1])
    hdr = re.findall(r"PR1HA_BEGIN total=(\d+) kept=(\d+)", rx)[-1]
    if int(hdr[0]) > int(hdr[1]):
        return None, f"anomaly list truncated ({hdr[0]}>{hdr[1]})"
    an = [tuple(map(int, m.groups())) for m in HA.finditer(rx)]
    locks = [(t, L) for t, k, L, n, ch in an if k == 7]
    if not locks:
        return None, "no lock event"
    t_lock, L0 = locks[0]
    L1 = L0 + r["packet_count"] - 1
    period = r["hop"]["rx"]["period_est_us"]
    lost = []
    for t, k, L, n, ch in an:
        if t > t_end or k not in (3, 5, 8):
            continue
        for i in range(1 if k == 3 else n):
            if L0 <= L + i <= L1:
                lost.append((L + i, k))
    lost = sorted(set(lost))
    return {"run": rel.replace("/", "__"), "L0": L0, "L1": L1, "period_us": period, "t0_ms": t_lock // 1000,
            "lost": lost, "missing_metric": r["metrics"]["missing"], "packets": r["packet_count"]}, None


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    index = []
    for rel in CORPUS:
        s, why = extract(rel)
        if s is None:
            print(f"SKIP {rel}: {why}")
            index.append({"run": rel, "skipped": why})
            continue
        with (OUT / f"{s['run']}.txt").open("w") as fh:
            fh.write(f"# run={s['run']} L0={s['L0']} L1={s['L1']} period_us={s['period_us']} t0_ms={s['t0_ms']}\n")
            for L, k in s["lost"]:
                fh.write(f"{L} {k}\n")
        print(f"{s['run']}: frames={s['L1'] - s['L0'] + 1} lost={len(s['lost'])} (metric missing={s['missing_metric']})")
        index.append({k: v for k, v in s.items() if k != "lost"} | {"lost_frames": len(s["lost"])})
    (OUT / "index.json").write_text(json.dumps(index, indent=2))


if __name__ == "__main__":
    main()
