#!/usr/bin/env python3
"""PR1 XOR-FEC structure simulation.

This is not an RF model. It only compares erasure recovery behavior for
source+parity schedules under controlled packet-loss patterns.
"""
from __future__ import annotations

import json
import random
from dataclasses import dataclass, asdict
from pathlib import Path


@dataclass
class Result:
    model: str
    mode: str
    loss_pct: float
    source_frames: int
    source_lost_before_fec: int
    source_lost_after_fec: int
    recovered: int
    parity_packets: int
    overhead_pct_vs_sources: float


def group_size(mode: str) -> int:
    if mode == "off":
        return 1
    if mode == "4+1":
        return 4
    if mode == "3+1":
        return 3
    raise ValueError(mode)


def schedule(source_frames: int, mode: str):
    """Yield (kind, source_index, group_sources)."""
    if mode == "off":
        for i in range(source_frames):
            yield ("source", i, (i,))
        return
    n = group_size(mode)
    for base in range(0, source_frames, n):
        group = tuple(range(base, min(base + n, source_frames)))
        for i in group:
            yield ("source", i, group)
        if len(group) == n:
            yield ("parity", None, group)


def evaluate_losses(source_frames: int, mode: str, lost_tx_indices: set[int]):
    tx = list(schedule(source_frames, mode))
    lost_sources: set[int] = set()
    parity_lost: dict[tuple[int, ...], bool] = {}
    parity_packets = 0
    for tx_i, (kind, source_i, group) in enumerate(tx):
        lost = tx_i in lost_tx_indices
        if kind == "source":
            if lost:
                lost_sources.add(source_i)
        else:
            parity_packets += 1
            parity_lost[group] = lost

    before = len(lost_sources)
    recovered = 0
    if mode != "off":
        n = group_size(mode)
        for base in range(0, source_frames, n):
            group = tuple(range(base, min(base + n, source_frames)))
            if len(group) != n:
                continue
            missing = [i for i in group if i in lost_sources]
            if len(missing) == 1 and not parity_lost.get(group, True):
                lost_sources.remove(missing[0])
                recovered += 1
    return before, len(lost_sources), recovered, parity_packets, len(tx)


def iid_case(loss_prob: float, mode: str, source_frames=100_000, seed=1) -> Result:
    rng = random.Random(seed)
    tx = list(schedule(source_frames, mode))
    lost = {i for i in range(len(tx)) if rng.random() < loss_prob}
    before, after, recovered, parity_packets, _ = evaluate_losses(source_frames, mode, lost)
    return Result(
        model=f"iid_{loss_prob:.0%}",
        mode=mode,
        loss_pct=100.0 * after / source_frames,
        source_frames=source_frames,
        source_lost_before_fec=before,
        source_lost_after_fec=after,
        recovered=recovered,
        parity_packets=parity_packets,
        overhead_pct_vs_sources=100.0 * parity_packets / source_frames,
    )


def burst_case(burst_len: int, mode: str, source_frames=40_000) -> Result:
    tx = list(schedule(source_frames, mode))
    # Sweep one burst across every possible schedule phase, spaced far enough
    # apart to avoid overlap. This measures deterministic burst sensitivity.
    period = 97
    lost = set()
    start = 0
    while start < len(tx):
        for j in range(burst_len):
            if start + j < len(tx):
                lost.add(start + j)
        start += period
    before, after, recovered, parity_packets, _ = evaluate_losses(source_frames, mode, lost)
    return Result(
        model=f"burst_{burst_len}",
        mode=mode,
        loss_pct=100.0 * after / source_frames,
        source_frames=source_frames,
        source_lost_before_fec=before,
        source_lost_after_fec=after,
        recovered=recovered,
        parity_packets=parity_packets,
        overhead_pct_vs_sources=100.0 * parity_packets / source_frames,
    )


def main():
    modes = ["off", "4+1", "3+1"]
    results = []
    for p in [0.01, 0.05, 0.10]:
        for mode in modes:
            # Average several seeds to reduce Monte-Carlo noise.
            seed_results = [iid_case(p, mode, seed=s) for s in range(1, 6)]
            first = seed_results[0]
            results.append(Result(
                model=first.model,
                mode=mode,
                loss_pct=sum(r.loss_pct for r in seed_results) / len(seed_results),
                source_frames=first.source_frames,
                source_lost_before_fec=round(sum(r.source_lost_before_fec for r in seed_results) / len(seed_results)),
                source_lost_after_fec=round(sum(r.source_lost_after_fec for r in seed_results) / len(seed_results)),
                recovered=round(sum(r.recovered for r in seed_results) / len(seed_results)),
                parity_packets=first.parity_packets,
                overhead_pct_vs_sources=first.overhead_pct_vs_sources,
            ))
    for b in [2, 3, 4]:
        for mode in modes:
            results.append(burst_case(b, mode))

    out = Path(__file__).resolve().parents[1] / "results" / "fec_loss_sim.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps([asdict(r) for r in results], indent=2) + "\n")
    print(f"{'model':<10} {'mode':<4} {'post-loss%':>10} {'recovered':>10} {'overhead%':>10}")
    for r in results:
        print(f"{r.model:<10} {r.mode:<4} {r.loss_pct:>10.3f} {r.recovered:>10} {r.overhead_pct_vs_sources:>10.1f}")
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
