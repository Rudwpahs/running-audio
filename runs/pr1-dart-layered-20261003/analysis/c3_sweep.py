"""C3 offline replay, step 2: candidate estimator configs (C2 K21 + C3 knobs) over the corpus.

  python analysis/c3_sweep.py                 final set   -> c3_replay_results.json, c3_sweep_output.txt
  python analysis/c3_sweep.py --grid          review grid -> c3_replay_results_grid.json, c3_sweep_output_grid.txt
  python analysis/c3_sweep.py NAME [NAME ...] named candidates (set C3_TAG to choose the output suffix)

Builds analysis/c3_replay.exe (g++) if missing or older than its sources.

Stream sets (analysis/c3_streams/, from c3_extract_streams.py):
  all       16 static-map Gate B/B1 runs at 150 us (~33 s each)
  recent     7 runs of the sessions with a concentrated bad band (gateC, gateC1, gateC2 interleave)
  c2sess     3 B1 runs of the current (Gate C2 interleave) session -- the same-session reference
  long       9 ~200 s stationary-stress streams: each recent run looped x6 (LOOP), plus two
             session stitches (gateC B i1+i3+i5, gateC2 B1 i1+i4+i7, each x2).
  stress     7 ~1800 s stationary-stress streams: each recent run looped x54 (SLOOP). Used for
             convergence (map changes / min per 300 s bin), re-exclusions and strike growth.
             ASSUMPTION: the 33 s loss pattern repeats unchanged for 30 min (stationary
             interference). This is a stress test of the algorithm, not a measurement.
  recover    long and stress with the synthetic "bad channel becomes clean" change: the run's
             top-8 Gate B channels (>= 2 losses) lose every loss from the half-way frame on;
             time-to-recover = until the channel is back in the applied map (window: 100 s
             for long, ~900 s for stress). Reported as recovered fraction, censored mean
             (unrecovered channels count with the full window) and median of the recovered.
Floor-30 stress: candidates rerun with minimum_active_channels = 30 on all / long / stress.
"""
import json
import os
import statistics
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
EXE = HERE / "c3_replay.exe"
SRC = [HERE / "c3_replay.cpp", REPO / "firmware/common/pr1_channel_quality.hpp", REPO / "firmware/common/pr1_afh.hpp"]
GXX = os.environ.get("GXX", r"C:\Ruby33-x64\msys64\ucrt64\bin\g++.exe")
STREAMS = HERE / "c3_streams"
POOL = ThreadPoolExecutor(max_workers=10)
LOOP = 6    # long stress: 6 x ~33 s segments (~200 s)
SLOOP = 54  # stress: 54 x ~33 s segments (~1800 s)
BIN_S = 300

Q = 32767
def q15(x): return int(round(x * Q))

K21 = {"exclude_q15": q15(0.70), "exclude_slow_q15": q15(0.95), "reinstate_successes": 3,
       "probe_init_ms": 3200, "probe_max_ms": 25600, "map_min_interval_ms": 5000}

# Building blocks (C3 knobs; see pr1_channel_quality.hpp Config).
def MEM(probe_ms=12800, shift=3, max_ms=204800, decay_ms=120000):
    """Exclusion memory: k>=2 strikes -> probe base max(init, probe_ms) * 2^min(k-1, shift), cap max_ms."""
    return {"strike_shift": shift, "strike_probe_ms": probe_ms, "strike_max_ms": max_ms, "strike_decay_ms": decay_ms}
def NB(radius=1, min_bad=1, bad_slow=0.0, direct=0.82):
    """Neighbour corroboration; bad_slow = 0 -> only excluded/probing neighbours count.
    direct 0.82 = one own loss (this frame); 0.60 = this frame lost and >= 2 own losses since inclusion."""
    return {"nb_radius": radius, "nb_min_bad": min_bad, "nb_bad_slow_q15": q15(bad_slow) if bad_slow else 0,
            "nb_direct_fast_q15": 26869 if direct == 0.82 else q15(direct)}
def WIN(window=6, need=6):
    return {"probe_window": window, "reinstate_successes": need}
def PROB(visits=32):
    return {"probation": visits}
RESET = {"reset_pdr": 1}
INIT6400 = {"probe_init_ms": 6400}

def c(*parts):
    out = dict(K21)
    for p in parts:
        out.update(p)
    return out

NBS = {"-": {}, "N1": NB(1, 1), "N2": NB(2, 2), "N3": NB(2, 1), "N5": NB(2, 2, 0.97), "N2d": NB(2, 2, 0.0, 0.60)}

# Review grid (after the neighbour-rule fix): neighbours of the old C3_SEL
# (base 25.6 s, shift 2, cap 102.4 s, decay 120 s, N2).
# Names: R_m<base_s>s<shift>[c<cap_s>]d<decay_s>_<nb>_<r0|r1>; cap = base << shift unless given.
GRID = {}
for base in (12800, 25600, 51200):
    for sh in (1, 2, 3, 4):
        for dec in (0, 120000, 300000):
            for nk in ("-", "N1", "N2", "N3", "N5", "N2d"):
                for rs in (0, 1):
                    GRID[f"R_m{base // 1000}s{sh}d{dec // 1000}_{nk}_r{rs}"] = c(
                        MEM(base, sh, base << sh, dec), NBS[nk], RESET if rs else {})
for nk in ("-", "N1", "N2", "N3", "N5", "N2d"):
    for rs in (0, 1):
        GRID[f"R_nomem_{nk}_r{rs}"] = c(NBS[nk], RESET if rs else {})

OLD_MEM = MEM(25600, 2, 102400, 120000)  # memory of the first C3_SEL; kept (see C3_REPLAY_NOTES.md)
SEL_MEM = OLD_MEM
SEL_NB = NBS["N3"]   # +/-2, >= 1 excluded/probing neighbour, one own loss on this frame
FINAL = {
    "K0_default": {},
    "K21_C2": dict(K21),
    "C3_old_N2": c(OLD_MEM, NBS["N2"]),               # the first C3_SEL values, with the fixed rule
    "C3_SEL": c(SEL_MEM, SEL_NB),                     # selected after the review
    "C3a_mem_only": c(SEL_MEM),
    "C3b_nb_only": c(SEL_NB),
    "C3c_sel_resetpdr": c(SEL_MEM, SEL_NB, RESET),
    "C3d_sel_nbN1": c(SEL_MEM, NBS["N1"]),
    "C3e_sel_nbN5": c(SEL_MEM, NBS["N5"]),
    "C3f_sel_nbN2d": c(SEL_MEM, NBS["N2d"]),
    "C3g_sel_nodecay": c(MEM(25600, 2, 102400, 0), SEL_NB),
    "C3h_sel_shift3": c(MEM(25600, 3, 204800, 120000), SEL_NB),
    "C3i_sel_shift4": c(MEM(25600, 4, 409600, 120000), SEL_NB),
    "C3j_sel_m12s2": c(MEM(12800, 2, 51200, 120000), SEL_NB),
    "C3k_sel_P32": c(SEL_MEM, SEL_NB, PROB(32)),
    "C3l_sel_W6": c(SEL_MEM, SEL_NB, WIN(6, 6)),
    "C3m_sel_init6400": c(SEL_MEM, SEL_NB, INIT6400),
}
CANDIDATES = {}
CANDIDATES.update(GRID)
CANDIDATES.update(FINAL)
FLOOR30 = ["K0_default", "K21_C2", "C3_old_N2", "C3_SEL", "C3c_sel_resetpdr", "C3h_sel_shift3"]


def env():
    return dict(os.environ, PATH=str(Path(GXX).parent) + os.pathsep + os.environ.get("PATH", ""))


def build():
    if EXE.exists() and all(EXE.stat().st_mtime > s.stat().st_mtime for s in SRC):
        return
    subprocess.run([GXX, "-std=c++17", "-Wall", "-Wextra", "-O2", str(SRC[0]), "-o", str(EXE)], check=True, env=env())


def run(stream: str, params: dict) -> dict:
    args = [str(EXE), stream] + [f"{k}={v}" for k, v in params.items()]
    return json.loads(subprocess.run(args, check=True, capture_output=True, text=True, env=env(), cwd=str(STREAMS)).stdout)


def sets():
    single = sorted(p.name for p in STREAMS.glob("*.txt"))  # relative names: run() uses cwd = STREAMS
    recent = [s for s in single if "gateC" in s]
    long = ["+".join([s] * LOOP) for s in recent]
    long.append("+".join([f"gateC-interleave__B-gap-150us-10000-i{i}.txt" for i in (1, 3, 5)] * (LOOP // 3)))
    long.append("+".join([f"gateC2-interleave__B1-gap-150us-10000-i{i}.txt" for i in (1, 4, 7)] * (LOOP // 3)))
    stress = ["+".join([s] * SLOOP) for s in recent]
    return {"all": single, "long": long, "stress": stress}


def bins(rows):
    """Map activations per minute in each BIN_S bin (bins shorter than half a bin are dropped)."""
    nb = max(len(r["act_bins"]) for r in rows)
    out = []
    for k in range(nb):
        acts = sum(r["act_bins"][k] for r in rows if k < len(r["act_bins"]))
        dur = sum(r["bin_frames"][k] * r["period_us"] / 1e6 for r in rows if k < len(r["bin_frames"]))
        full = sum(r["bin_ms"] / 1e3 for r in rows if k < len(r["bin_frames"]))
        if full and dur >= 0.5 * full:
            out.append(round(60 * acts / dur, 2))
    return out


def agg(rows):
    f = sum(r["frames"] for r in rows)
    lb = sum(r["lost_b"] for r in rows)
    ls = sum(r["lost_sim"] for r in rows)
    dur = sum(r["duration_s"] for r in rows)
    ex = sum(r["exclusions"] for r in rows)
    det = [x for r in rows for x in r["detect_ms"]]
    kb = sum(r["known_bad"] for r in rows)
    first = sum(r["act_first_third"] for r in rows)
    last = sum(r["act_last_third"] for r in rows)
    s = lambda k: sum(r[k] for r in rows)
    dsum = lambda key: {k: sum(r[key][k] for r in rows) for k in rows[0][key]}
    return {
        "runs": len(rows), "frames": f, "duration_s": round(dur, 1), "loss_b_pct": round(100 * lb / f, 4), "loss_sim_pct": round(100 * ls / f, 4),
        "rel_change_pct": round(100 * (ls / lb - 1), 1) if lb else None,
        "activations_per_min": round(60 * s("activations") / dur, 2),
        "act_per_min_first_third": round(60 * first / (dur / 3), 2),
        "act_per_min_last_third": round(60 * last / (dur / 3), 2),
        "act_per_min_by_300s": bins(rows),
        "reversals": s("reversals"), "exclusions": ex, "reinclusions": s("reinclusions"),
        "reexclusions": s("reexclusions"), "zero_loss_reexclusions": s("zero_loss_reexclusions"),
        "single_loss_exclusions": s("single_loss_exclusions"),
        "fresh_single_loss_exclusions": s("fresh_single_loss_exclusions"),
        "isolated8_exclusions": s("isolated_exclusions"),
        "excl_by_reason": dsum("excl_by_reason"),
        "zero_loss_by_reason": dsum("zero_loss_by_reason"),
        "one_loss_by_reason": dsum("one_loss_by_reason"),
        "zero_loss_exclusions": sum(dsum("zero_loss_by_reason").values()),
        "nb_excl": s("nb_excl"), "nb_zero_loss_since_inclusion": dsum("zero_loss_by_reason")["neighbor"],
        "nb_one_loss_since_inclusion": dsum("one_loss_by_reason")["neighbor"],
        "nb_direct8": s("nb_direct8"), "nb_direct4": s("nb_direct4"),
        "nb_only1_in32": s("nb_only1_in32"), "nb_no_direct": s("nb_no_direct"),
        "probes": s("probes"), "probe_ok": s("probe_ok"),
        "min_active": min(r["min_active"] for r in rows), "max_strikes": max(r["max_strikes"] for r in rows),
        "floor_frac": round(sum(r["floor_frac"] * r["frames"] for r in rows) / f, 4),
        "known_bad": kb, "known_bad_detected": sum(r["known_bad_detected"] for r in rows),
        "detect_median_ms": round(statistics.median(det)) if det else None,
        "detect_mean_censored_ms": round(sum(r["detect_mean_censored_ms"] * r["known_bad"] for r in rows) / kb) if kb else None,
        "excl_on_worst8_share": round(s("excl_on_worst8") / ex, 3) if ex else None,
    }


def agg_recover(rows):
    rec = [x for r in rows for x in r["recover_ms"]]
    out_n = sum(r["out_at_clean"] for r in rows)
    lb2 = sum(r["lost_b_h2"] for r in rows)
    ls2 = sum(r["lost_sim_h2"] for r in rows)
    n_rec = sum(r["recovered"] for r in rows)
    return {
        "cleaned": sum(r["cleaned"] for r in rows), "out_at_clean": out_n, "recovered": n_rec,
        "recovered_frac": round(n_rec / out_n, 3) if out_n else None,
        "recover_mean_censored_ms": round(sum(r["recover_mean_censored_ms"] * r["out_at_clean"] for r in rows) / out_n) if out_n else None,
        "recover_median_recovered_ms": round(statistics.median(rec)) if rec else None,
        "recover_max_recovered_ms": round(max(rec)) if rec else None,
        "recover_window_ms": round(statistics.mean(r["recover_window_ms"] for r in rows)),
        "post_clean_excl_of_cleaned": sum(r["post_clean_excl_of_cleaned"] for r in rows),
        "h2_rel_change_pct": round(100 * (ls2 / lb2 - 1), 1) if lb2 else None,
    }


def evaluate(name, params, S):
    res = {"params": params}
    P = dict(params, bin_ms=BIN_S * 1000)
    PR = dict(P, clean_top=8, clean_at_permille=500)
    rows = {"all": list(POOL.map(lambda x: run(x, P), S["all"])),
            "long": list(POOL.map(lambda x: run(x, P), S["long"])),
            "stress": list(POOL.map(lambda x: run(x, P), S["stress"]))}
    rows["recent"] = [r for r in rows["all"] if "gateC" in r["run"]]
    rows["c2sess"] = [r for r in rows["all"] if "gateC2" in r["run"]]
    for k in ("all", "recent", "c2sess", "long", "stress"):
        res[k] = agg(rows[k])
    res["recover_long"] = agg_recover(list(POOL.map(lambda x: run(x, PR), S["long"])))
    res["recover_stress"] = agg_recover(list(POOL.map(lambda x: run(x, PR), S["stress"])))
    res["runs_all"] = rows["all"]
    return res


def line(name, r):
    a, rc, c2, st = r["all"], r["recent"], r["c2sess"], r["stress"]
    rl, rs = r["recover_long"], r["recover_stress"]
    z = a["zero_loss_by_reason"]
    return (f"{name:24s} all {a['rel_change_pct']:+6.1f}% rec {rc['rel_change_pct']:+6.1f}% c2s {c2['rel_change_pct']:+6.1f}% "
            f"| ex {a['exclusions']:3d} rein {a['reinclusions']:3d} reex {a['reexclusions']:3d} "
            f"sgl {a['single_loss_exclusions']:3d} nb {a['nb_excl']:3d} nb0 {a['nb_zero_loss_since_inclusion']} "
            f"zero[f{z['fast']} s{z['slow']} n{z['neighbor']} p{z['probation']}] | act/min {a['activations_per_min']:5.2f} min {a['min_active']:2d} "
            f"det {rc['known_bad_detected']}/{rc['known_bad']} med {rc['detect_median_ms']} cens {rc['detect_mean_censored_ms']} | "
            f"1800s: {st['rel_change_pct']:+.1f}% act/min/300s {st['act_per_min_by_300s']} reex {st['reexclusions']} "
            f"nb0 {st['nb_zero_loss_since_inclusion']} zero {st['zero_loss_exclusions']} strikes {st['max_strikes']} min {st['min_active']} | "
            f"recov100s {rl['recovered']}/{rl['out_at_clean']} cens {rl['recover_mean_censored_ms']} | "
            f"recov900s {rs['recovered']}/{rs['out_at_clean']} cens {rs['recover_mean_censored_ms']} med {rs['recover_median_recovered_ms']} "
            f"max {rs['recover_max_recovered_ms']}")


def main():
    build()
    S = sets()
    argv = sys.argv[1:]
    tag = os.environ.get("C3_TAG", "")
    floor_names = FLOOR30
    if argv == ["--grid"]:
        argv = ["K0_default", "K21_C2"] + list(GRID)
        tag = tag or "_grid"
        floor_names = []
    elif argv:
        floor_names = [n for n in argv if n in FLOOR30]
    names = argv or list(FINAL)
    results = {}
    lines = []
    for name in names:
        results[name] = evaluate(name, CANDIDATES[name], S)
        lines.append(line(name, results[name]))
        print(lines[-1], flush=True)
    for name in floor_names:
        p = dict(CANDIDATES[name], min_active=30, bin_ms=BIN_S * 1000)
        fr = {"params": p}
        for k in ("all", "long", "stress"):
            fr[k] = agg(list(POOL.map(lambda x: run(x, p), S[k])))
        results["FLOOR30_" + name] = fr
        a, lg, st = fr["all"], fr["long"], fr["stress"]
        lines.append(f"FLOOR30_{name:16s} all {a['rel_change_pct']:+6.1f}% | ex {a['exclusions']} rein {a['reinclusions']} "
                     f"reex {a['reexclusions']} rev {a['reversals']} sgl {a['single_loss_exclusions']} act/min {a['activations_per_min']} "
                     f"floor_frac {a['floor_frac']} min {a['min_active']} | long rev {lg['reversals']} floor_frac {lg['floor_frac']} | "
                     f"1800s act/min/300s {st['act_per_min_by_300s']} rev {st['reversals']} rev/min "
                     f"{round(60 * st['reversals'] / st['duration_s'], 2)} floor_frac {st['floor_frac']} nb0 {st['nb_zero_loss_since_inclusion']}")
        print(lines[-1], flush=True)
    (HERE / f"c3_replay_results{tag}.json").write_text(json.dumps(results, indent=1))
    (HERE / f"c3_sweep_output{tag}.txt").write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
