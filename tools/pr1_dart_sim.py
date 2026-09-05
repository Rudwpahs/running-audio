#!/usr/bin/env python3
"""Deterministic PR1-DART ablation simulator.

This is a structural regression harness, not an RF propagation model. It exists
so algorithm changes can be compared under the same synthetic random/burst-loss
inputs before hardware testing.
"""
from __future__ import annotations

import argparse
import json
import random
from dataclasses import dataclass, asdict

CHANNELS = 40
BAD_CHANNELS = set(range(8, 16))


@dataclass(frozen=True)
class Strategy:
    name: str
    hop: bool = False
    adaptive_map: bool = False
    xor: bool = False
    arq: bool = False
    blind_arq: bool = False
    phy: bool = False


STRATEGIES = [
    Strategy("A_fixed_no_recovery"),
    Strategy("B_hopping", hop=True),
    Strategy("C_adaptive_afh", hop=True, adaptive_map=True),
    Strategy("D_afh_xor", hop=True, adaptive_map=True, xor=True),
    Strategy("E_afh_arq", hop=True, adaptive_map=True, arq=True),
    Strategy("F_afh_xor_arq", hop=True, adaptive_map=True, xor=True, arq=True),
    Strategy("G_afh_phy", hop=True, adaptive_map=True, phy=True),
    Strategy("H_full_pr1_dart", hop=True, adaptive_map=True, xor=True, arq=True, phy=True),
]


@dataclass
class Result:
    strategy: str
    frames: int
    raw_losses: int
    final_losses: int
    max_raw_burst: int
    max_final_burst: int
    parity_frames: int
    arq_sent: int
    arq_useful: int
    arq_late: int

    @property
    def raw_loss_rate(self) -> float:
        return self.raw_losses / self.frames

    @property
    def final_loss_rate(self) -> float:
        return self.final_losses / self.frames

    @property
    def arq_useful_ratio(self) -> float:
        return self.arq_useful / self.arq_sent if self.arq_sent else 0.0


def channel_for(frame: int, strategy: Strategy) -> int:
    if not strategy.hop:
        return 10
    active = [c for c in range(CHANNELS) if not (strategy.adaptive_map and c in BAD_CHANNELS)]
    return active[(frame * 17 + frame // max(1, len(active))) % len(active)]


def simulate(strategy: Strategy, frames: int, seed: int) -> Result:
    rng = random.Random(seed)
    raw_lost = [False] * frames
    raw_burst = final_burst = 0
    max_raw_burst = max_final_burst = 0
    body_burst_until = -1
    for i in range(frames):
        if rng.random() < 0.0015:
            body_burst_until = max(body_burst_until, i + 2)
        ch = channel_for(i, strategy)
        p = 0.004
        if ch in BAD_CHANNELS:
            p += 0.28
        if i <= body_burst_until:
            p += 0.30 if strategy.phy else 0.62
        raw_lost[i] = rng.random() < min(p, 0.98)

    final_lost = raw_lost[:]
    parity_frames = 0
    if strategy.xor:
        for start in range(0, frames, 4):
            group = list(range(start, min(start + 4, frames)))
            if len(group) < 4:
                break
            parity_frames += 1
            missing = [idx for idx in group if final_lost[idx]]
            if rng.random() >= 0.004 and len(missing) == 1:
                final_lost[missing[0]] = False

    arq_sent = arq_useful = arq_late = 0
    if strategy.arq:
        for i, lost in enumerate(final_lost):
            if not lost:
                continue
            enough_slack = (i % 4) != 3
            if not enough_slack and not strategy.blind_arq:
                continue
            arq_sent += 1
            if not enough_slack:
                arq_late += 1
                continue
            if rng.random() < 0.84:
                final_lost[i] = False
                arq_useful += 1

    for lost in raw_lost:
        raw_burst = raw_burst + 1 if lost else 0
        max_raw_burst = max(max_raw_burst, raw_burst)
    for lost in final_lost:
        final_burst = final_burst + 1 if lost else 0
        max_final_burst = max(max_final_burst, final_burst)

    return Result(strategy.name, frames, sum(raw_lost), sum(final_lost), max_raw_burst,
                  max_final_burst, parity_frames, arq_sent, arq_useful, arq_late)


def run_matrix(frames: int, seed: int) -> list[Result]:
    return [simulate(s, frames, seed) for s in STRATEGIES]


def run_arq_ab(frames: int, seed: int) -> list[Result]:
    common = dict(hop=True, adaptive_map=True, arq=True, phy=False)
    blind = Strategy("ARQ_blind", blind_arq=True, **common)
    deadline = Strategy("ARQ_deadline", blind_arq=False, **common)
    return [simulate(blind, frames, seed), simulate(deadline, frames, seed)]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--frames", type=int, default=20000)
    parser.add_argument("--seed", type=int, default=12345)
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--arq-ab", action="store_true",
                        help="compare synthetic blind ARQ with deadline-aware ARQ")
    args = parser.parse_args()
    results = run_arq_ab(args.frames, args.seed) if args.arq_ab else run_matrix(args.frames, args.seed)
    if args.json:
        print(json.dumps([{**asdict(r), "raw_loss_rate": r.raw_loss_rate,
                           "final_loss_rate": r.final_loss_rate,
                           "arq_useful_ratio": r.arq_useful_ratio} for r in results], indent=2))
    else:
        print("strategy,raw_loss_pct,final_loss_pct,max_raw_burst,max_final_burst,parity,arq_sent,arq_useful,arq_late,arq_useful_pct")
        for r in results:
            print(f"{r.strategy},{r.raw_loss_rate*100:.3f},{r.final_loss_rate*100:.3f},"
                  f"{r.max_raw_burst},{r.max_final_burst},{r.parity_frames},"
                  f"{r.arq_sent},{r.arq_useful},{r.arq_late},{r.arq_useful_ratio*100:.1f}")

    if args.arq_ab:
        blind, deadline = results
        if blind.raw_losses != deadline.raw_losses:
            raise SystemExit("ARQ A/B inputs diverged")
        if deadline.arq_sent > blind.arq_sent:
            raise SystemExit("deadline ARQ sent more repairs than blind ARQ")
        if deadline.arq_late != 0:
            raise SystemExit("deadline ARQ scheduled a known-late repair")
        if deadline.arq_useful_ratio < blind.arq_useful_ratio:
            raise SystemExit("deadline ARQ useful ratio regressed against blind ARQ")
    elif results[-1].final_losses > results[0].final_losses:
        raise SystemExit("full PR1-DART regressed against synthetic baseline")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
