"""C3 offline replay, step 1: per-frame outcome streams (same format as c2_streams/).

  python analysis/c3_extract_streams.py   -> analysis/c3_streams/*.txt + index.json

Corpus = the C2 corpus (c2_extract_streams.CORPUS) + the three current-session (Gate C2
interleave) B1 runs. Uses the C2 extractor unchanged; runs whose anomaly list overflowed
(gateC1-interleave B1 i4 / i7) are still skipped, because their loss list is incomplete.
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import c2_extract_streams as c2  # noqa: E402

OUT = Path(__file__).resolve().parent / "c3_streams"
CORPUS = c2.CORPUS + [
    "gateC2-interleave/B1-gap-150us-10000-i1", "gateC2-interleave/B1-gap-150us-10000-i4",
    "gateC2-interleave/B1-gap-150us-10000-i7",
]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    index = []
    for rel in CORPUS:
        s, why = c2.extract(rel)
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
