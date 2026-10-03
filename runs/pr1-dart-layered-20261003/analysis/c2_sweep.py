"""C2 offline replay, step 2: run candidate estimator configs over all Gate B streams.

  python analysis/c2_sweep.py            -> analysis/c2_replay_results.json + table

Builds analysis/c2_replay.exe (g++) if missing or older than its sources. Candidates are
named parameter sets for the firmware Estimator (plus the runtime map-proposal interval cap).
Aggregates over runs: pooled replay loss vs Gate B loss, map activations per minute,
reversals (up/down oscillation), exclusions and the share caused by an isolated single loss
(<= 1 loss in the channel's last 8 data visits), reinclusions, minimum active, floor time.
"""
import json
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
EXE = HERE / "c2_replay.exe"
SRC = [HERE / "c2_replay.cpp", REPO / "firmware/common/pr1_channel_quality.hpp", REPO / "firmware/common/pr1_afh.hpp"]
GXX = os.environ.get("GXX", r"C:\Ruby33-x64\msys64\ucrt64\bin\g++.exe")

Q = 32767
def q15(x): return int(round(x * Q))

CANDIDATES = {
    # Gate C defaults (what ran on the boards in Gate C / C1).
    "K0_default": {},
    # Fast-EWMA threshold only: an isolated loss (fast 0.75) can suspect but never exclude;
    # exclusion needs >= 2 losses within ~5 visits (fast < 0.70) or 4 in a row.
    "K1_f070": {"exclude_q15": q15(0.70)},
    "K2_f060": {"exclude_q15": q15(0.60)},
    # K1 + conservative re-inclusion only.
    "K3_f070_rein3_p1600": {"exclude_q15": q15(0.70), "reinstate_successes": 3, "probe_init_ms": 1600,
                            "probe_max_ms": 12800},
    # K1 + slow-EWMA rule (persistent moderate loss): exclude a SUSPECT channel when slow < S.
    # One isolated loss leaves slow at 0.969, so S < 0.969 never excludes on it.
    "K4_f070_s096": {"exclude_q15": q15(0.70), "exclude_slow_q15": q15(0.96)},
    "K5_f070_s095": {"exclude_q15": q15(0.70), "exclude_slow_q15": q15(0.95)},
    "K6_f070_s093": {"exclude_q15": q15(0.70), "exclude_slow_q15": q15(0.93)},
    # K5 + hysteresis (3/3 probes, slower probing) and/or a map-update cap.
    "K7_f070_s095_rein3": {"exclude_q15": q15(0.70), "exclude_slow_q15": q15(0.95), "reinstate_successes": 3},
    "K8_f070_s095_rein3_p1600": {"exclude_q15": q15(0.70), "exclude_slow_q15": q15(0.95), "reinstate_successes": 3,
                                 "probe_init_ms": 1600, "probe_max_ms": 12800},
    "K9_f070_s095_rein3_p1600_cap3s": {"exclude_q15": q15(0.70), "exclude_slow_q15": q15(0.95),
                                       "reinstate_successes": 3, "probe_init_ms": 1600, "probe_max_ms": 12800,
                                       "map_min_interval_ms": 3000},
    "K10_f070_s096_rein3_p1600_cap3s": {"exclude_q15": q15(0.70), "exclude_slow_q15": q15(0.96),
                                        "reinstate_successes": 3, "probe_init_ms": 1600, "probe_max_ms": 12800,
                                        "map_min_interval_ms": 3000},
    "K11_f070_s095_cap3s": {"exclude_q15": q15(0.70), "exclude_slow_q15": q15(0.95), "map_min_interval_ms": 3000},
    # Faster persistent-loss evidence, still >= 2 losses: one loss leaves slow at 0.969
    # (shift 5) / 0.9375 (shift 4); the thresholds sit just below that.
    "K12_f070_s0965": {"exclude_q15": q15(0.70), "exclude_slow_q15": q15(0.965)},
    "K13_f070_s0965_rein3_p1600_cap3s": {"exclude_q15": q15(0.70), "exclude_slow_q15": q15(0.965),
                                         "reinstate_successes": 3, "probe_init_ms": 1600, "probe_max_ms": 12800,
                                         "map_min_interval_ms": 3000},
    "K14_sh4_f070_s093": {"slow_shift": 4, "exclude_q15": q15(0.70), "exclude_slow_q15": q15(0.93)},
    "K15_sh4_f070_s093_rein3_p1600_cap3s": {"slow_shift": 4, "exclude_q15": q15(0.70), "exclude_slow_q15": q15(0.93),
                                            "reinstate_successes": 3, "probe_init_ms": 1600, "probe_max_ms": 12800,
                                            "map_min_interval_ms": 3000},
    "K16_f070_s0965_rein3_p3200_cap3s": {"exclude_q15": q15(0.70), "exclude_slow_q15": q15(0.965),
                                         "reinstate_successes": 3, "probe_init_ms": 3200, "probe_max_ms": 25600,
                                         "map_min_interval_ms": 3000},
    "K17_f070_s0965_rein3_p3200_cap5s": {"exclude_q15": q15(0.70), "exclude_slow_q15": q15(0.965),
                                         "reinstate_successes": 3, "probe_init_ms": 3200, "probe_max_ms": 25600,
                                         "map_min_interval_ms": 5000},
    "K18_f070_s0965_rein3_p6400_cap5s": {"exclude_q15": q15(0.70), "exclude_slow_q15": q15(0.965),
                                         "reinstate_successes": 3, "probe_init_ms": 6400, "probe_max_ms": 51200,
                                         "map_min_interval_ms": 5000},
    "K19_f070_s0965_p3200_cap5s": {"exclude_q15": q15(0.70), "exclude_slow_q15": q15(0.965),
                                   "probe_init_ms": 3200, "probe_max_ms": 25600, "map_min_interval_ms": 5000},
    "K20_f070_s095_rein3_p1600_cap5s": {"exclude_q15": q15(0.70), "exclude_slow_q15": q15(0.95),
                                        "reinstate_successes": 3, "probe_init_ms": 1600, "probe_max_ms": 12800,
                                        "map_min_interval_ms": 5000},
    "K21_f070_s095_rein3_p3200_cap5s": {"exclude_q15": q15(0.70), "exclude_slow_q15": q15(0.95),
                                        "reinstate_successes": 3, "probe_init_ms": 3200, "probe_max_ms": 25600,
                                        "map_min_interval_ms": 5000},
    "K22_f070_s095_rein3_p3200_cap3s": {"exclude_q15": q15(0.70), "exclude_slow_q15": q15(0.95),
                                        "reinstate_successes": 3, "probe_init_ms": 3200, "probe_max_ms": 25600,
                                        "map_min_interval_ms": 3000},
}
# Floor stress (functional check only): the replay data rarely push the map to the floor of
# 12, so the same candidates are rerun with a floor of 30 to exercise floor behaviour.
for _k in ("K0_default", "K9_f070_s095_rein3_p1600_cap3s", "K20_f070_s095_rein3_p1600_cap5s",
           "K21_f070_s095_rein3_p3200_cap5s"):
    CANDIDATES["FLOOR30_" + _k] = dict(CANDIDATES[_k], min_active=30)
def build():
    if EXE.exists() and all(EXE.stat().st_mtime > s.stat().st_mtime for s in SRC):
        return
    env = dict(os.environ, PATH=str(Path(GXX).parent) + os.pathsep + os.environ.get("PATH", ""))
    subprocess.run([GXX, "-std=c++17", "-Wall", "-Wextra", "-O2", str(SRC[0]), "-o", str(EXE)], check=True, env=env)


def run(stream: Path, params: dict) -> dict:
    args = [str(EXE), str(stream)] + [f"{k}={v}" for k, v in params.items()]
    env = dict(os.environ, PATH=str(Path(GXX).parent) + os.pathsep + os.environ.get("PATH", ""))
    out = subprocess.run(args, check=True, capture_output=True, text=True, env=env).stdout
    return json.loads(out)


def aggregate(rows):
    f = sum(r["frames"] for r in rows)
    lb = sum(r["lost_b"] for r in rows)
    ls = sum(r["lost_sim"] for r in rows)
    dur = sum(r["duration_s"] for r in rows)
    ex = sum(r["exclusions"] for r in rows)
    return {
        "frames": f, "loss_b_pct": round(100 * lb / f, 4), "loss_sim_pct": round(100 * ls / f, 4),
        "rel_change_pct": round(100 * (ls / lb - 1), 1) if lb else None,
        "activations_per_min": round(60 * sum(r["activations"] for r in rows) / dur, 2),
        "reversals": sum(r["reversals"] for r in rows),
        "exclusions": ex, "isolated_exclusions": sum(r["isolated_exclusions"] for r in rows),
        "isolated_share": round(sum(r["isolated_exclusions"] for r in rows) / ex, 3) if ex else None,
        "single_loss_exclusions": sum(r["single_loss_exclusions"] for r in rows),
        "reinclusions": sum(r["reinclusions"] for r in rows), "probes": sum(r["probes"] for r in rows),
        "min_active": min(r["min_active"] for r in rows),
        "floor_frac": round(sum(r["floor_frac"] * r["frames"] for r in rows) / f, 4),
        "runs_reaching_floor": sum(1 for r in rows if r["floor_frac"] > 0),
        "excl_on_worst8_share": round(sum(r["excl_on_worst8"] for r in rows) / ex, 3) if ex else None,
    }


def main():
    build()
    streams = sorted((HERE / "c2_streams").glob("*.txt"))
    names = sys.argv[1:] or list(CANDIDATES)
    results = {}
    for name in names:
        rows = [run(s, CANDIDATES[name]) for s in streams]
        recent = [r for r in rows if "gateC" in r["run"]]  # sessions with a concentrated bad band
        results[name] = {"params": CANDIDATES[name], "aggregate": aggregate(rows), "aggregate_recent": aggregate(recent),
                         "runs": rows}
        a = results[name]["aggregate"]
        print(f"{name:24s} loss B {a['loss_b_pct']:.3f}% -> sim {a['loss_sim_pct']:.3f}% ({a['rel_change_pct']:+.1f}%) "
              f"act/min {a['activations_per_min']:5.2f} rev {a['reversals']:3d} excl {a['exclusions']:3d} "
              f"iso8 {a['isolated_share']} single32 {a['single_loss_exclusions']} rein {a['reinclusions']:3d} min {a['min_active']:2d} "
              f"floor {a['floor_frac']:.3f} ({a['runs_reaching_floor']} runs) worst8 {a['excl_on_worst8_share']}")
        b = results[name]["aggregate_recent"]
        print(f"{'':24s}   recent: B {b['loss_b_pct']:.3f}% -> {b['loss_sim_pct']:.3f}% ({b['rel_change_pct']:+.1f}%) "
              f"act/min {b['activations_per_min']:5.2f} rev {b['reversals']} excl {b['exclusions']} iso8 {b['isolated_share']} single32 {b['single_loss_exclusions']} "
              f"min {b['min_active']} floor {b['floor_frac']}")
    (HERE / "c2_replay_results.json").write_text(json.dumps(results, indent=2))


if __name__ == "__main__":
    main()
