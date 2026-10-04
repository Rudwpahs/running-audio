"""Gate C1/C2 statistics: interleaved B1 / C1 / C(old Gate C image) / C2, raw run logs only.

  python analysis/gate_c1_stats.py [set_dir]     (default gateC1-interleave; gateC2-interleave for C2)

Per run: loss, CRC, no-IRQ frames, control events, losses 0..3 frames after a control
event (and after a map activation), slow RX post-read events (RxSlowReady, C1 images
only), TX slow transmits, timing p99s, control handling time, core-0 work, map churn.
Pooled pairwise two-proportion z-tests with 95 % CI. Unavailable metrics are None.
"""
import json
import math
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parents[1]
SET = HERE / (sys.argv[1] if len(sys.argv) > 1 else "gateC1-interleave")
W = 3

HA = re.compile(r"PR1HA t_us=(\d+) kind=(\d+) logical=(\d+) seq=(\d+) ch=(\d+) rssi=(-?\d+)")


def kv_line(ln):
    out = {}
    for k, _, v in (t.partition("=") for t in ln.split()[1:]):
        out[k] = int(v) if v.lstrip("-").isdigit() else v
    return out


def first(text, prefix):
    for ln in text.splitlines():
        if ln.startswith(prefix):
            return kv_line(ln)
    return None


def run_end_us(rx):
    return int(re.findall(r"PR1T v=1 t_us=(\d+) field=crc_good", rx)[-1])


def anomalies(text, t_end=None):
    out = []
    for m in HA.finditer(text):
        t, k, L, n, ch, r = map(int, m.groups())
        out.append((t, k, L, n, ch, r))
    return out


def lost_frames(an, t_end):
    out = []
    for t, k, L, n, ch, r in an:
        if t <= t_end and k in (3, 5, 8):
            out += [(L + i, k) for i in range(1 if k == 3 else n)]
    return out


def ctrl_frames(ctrl):
    return sorted({int(x) for x in re.findall(r" (?:RX|TX) PR1(?:PROP|COMMIT|ACK|STAGED|ABORTED)[^\n]*_logical=(\d+)", ctrl)})


def activations(rx):
    acts = {int(a) for a in re.findall(r"PR1QA v=\d+ activation=(\d+)", rx)}
    acts |= {int(L) for L in re.findall(r"PR1QE t_ms=\d+ kind=4 logical=(\d+)", rx)}
    return sorted(acts)


def near(L, evs):
    return any(0 <= L - e <= W for e in evs)


def ztest(m1, n1, m2, n2):
    p1, p2 = m1 / n1, m2 / n2
    p = (m1 + m2) / (n1 + n2)
    se = math.sqrt(p * (1 - p) * (1 / n1 + 1 / n2))
    z = (p1 - p2) / se if se else 0.0
    d = p1 - p2
    se_d = math.sqrt(p1 * (1 - p1) / n1 + p2 * (1 - p2) / n2)
    return z, (d - 1.96 * se_d, d + 1.96 * se_d)


def analyse(d: Path) -> dict:
    gate = d.name.split("-")[0]
    r = json.loads((d / "result.json").read_text())
    rx = (d / "rx.log").read_text(errors="replace")
    tx = (d / "tx.log").read_text(errors="replace")
    t_end = run_end_us(rx)
    an = anomalies(rx)
    ha_hdr = re.findall(r"PR1HA_BEGIN total=(\d+) kept=(\d+)", rx)
    ha_total, ha_kept = (map(int, ha_hdr[-1]) if ha_hdr else (None, None))
    lost = lost_frames(an, t_end)
    ctrl = ctrl_frames((d / "ctrl.log").read_text(errors="replace")) if (d / "ctrl.log").exists() else []
    acts = activations(rx) if gate != "B1" else []
    slow = [L for t, k, L, n, ch, rr in an if k == 12 and t <= t_end and L > 400]  # skip acquisition
    lost_set = {L for L, _ in lost}
    m = r["metrics"]
    qs = first(rx, "PR1QS ")
    tqs = first(tx, "PR1QS ")
    cp_rx, cp_tx = first(rx, "PR1CP "), first(tx, "PR1CP ")
    tx_an = anomalies(tx)
    tx_end = None
    tx_slow = [L for t, k, L, n, ch, rr in tx_an if k == 10]
    q = r.get("quality", {})
    agree = q.get("map_agreement", {})
    rx_events = q.get("rx_event_counts", {})
    is_c1 = gate in ("B1", "C1", "C2", "C3n2", "C3n3")
    row = {
        "run": d.name, "gate": gate, "packets": r["packet_count"], "missing": m["missing"],
        "loss_pct": round(100 * m["missing"] / r["packet_count"], 4), "crc": m["crc_bad"],
        "timeout_frames": sum(1 for _, k in lost if k == 5), "jump_frames": sum(1 for _, k in lost if k == 8),
        "anomalies_total": ha_total, "anomalies_kept": ha_kept,
        "anomaly_list_truncated": None if ha_total is None else ha_total > ha_kept,
        "ctrl_events": len(ctrl) if gate != "B1" else 0,
        "lost_near_ctrl": sum(1 for L, _ in lost if near(L, ctrl)) if gate != "B1" else 0,
        "activations": len(acts),
        "lost_near_activation": sum(1 for L, _ in lost if near(L, acts)),
        "rx_slow_ready": len(slow) if is_c1 else None,
        "rx_slow_ready_then_lost": sum(1 for L in slow if L in lost_set) if is_c1 else None,
        "tx_slow_transmits": len(tx_slow) if is_c1 else None,
        "sched_miss": m["scheduler_misses"], "irq_to_ready_p99": m["irq_to_rx_ready_us_p99"],
        "spi_p99": m["spi_duration_us_p99"], "rearm_p99": m["rx_rearm_us_p99"],
        "spi_end_to_rearm_p99": m["spi_end_to_rearm_start_us_p99"],
        "rx_ctrl_us": None if not qs else {k: qs.get(k) for k in ("ctrl_us_n", "ctrl_us_p50", "ctrl_us_p95", "ctrl_us_p99", "ctrl_us_max") if k in qs},
        "tx_ctrl_us": None if not tqs else {k: tqs.get(k) for k in ("ctrl_us_n", "ctrl_us_p50", "ctrl_us_p95", "ctrl_us_p99", "ctrl_us_max") if k in tqs},
        "core0_rx": cp_rx, "core0_tx": cp_tx,
        "map_versions": None if not qs else qs.get("map_version"),
        "min_active": None if not qs else qs.get("min_active_seen"),
        "final_active": None if not qs else qs.get("active"),
        "exclusions": rx_events.get("excluded") if gate != "B1" else None,
        "reinclusions": rx_events.get("reincluded", 0) if gate != "B1" else None,
        "probe_results": rx_events.get("probe_result", 0) if gate != "B1" else None,
        "recovered": rx_events.get("recovered", 0) if gate != "B1" else None,
        "events_truncated": None if not qs else qs.get("events", 0) > 160,
        "in_window_version_mismatch": agree.get("in_window_version_mismatch"),
        "post_run_version_mismatch": agree.get("version_mismatch"),
        "relay": None if not q else {k: v for k, v in q.get("relay", {}).items() if k not in ("relay_ms", "ids")},
    }
    # ctrl_us p50/p95 absent on the old Gate C image: keep None (never 0)
    if gate == "C" and row["rx_ctrl_us"]:
        for k in ("ctrl_us_n", "ctrl_us_p50", "ctrl_us_p95"):
            row["rx_ctrl_us"].setdefault(k, None)
    return row


def main():
    dirs = sorted((p for p in SET.iterdir() if p.is_dir() and (p / "result.json").exists()),
                  key=lambda p: int(p.name.rsplit("-i", 1)[1]))
    rows = [analyse(d) for d in dirs]
    for x in rows:
        print({k: x[k] for k in ("run", "packets", "missing", "loss_pct", "crc", "timeout_frames", "jump_frames",
                                 "ctrl_events", "lost_near_ctrl", "activations", "lost_near_activation",
                                 "rx_slow_ready", "rx_slow_ready_then_lost", "tx_slow_transmits", "sched_miss",
                                 "irq_to_ready_p99", "anomaly_list_truncated", "in_window_version_mismatch")})
    pooled = {}
    for g in sorted({x["gate"] for x in rows}):
        xs = [x for x in rows if x["gate"] == g]
        pooled[g] = {k: sum(x[k] for x in xs) for k in ("missing", "packets", "crc", "timeout_frames", "jump_frames",
                                                         "ctrl_events", "lost_near_ctrl", "lost_near_activation")}
        pooled[g]["loss_pct"] = round(100 * pooled[g]["missing"] / pooled[g]["packets"], 4)
        print(g, pooled[g])
    tests = {}
    for a, b in (("C1", "B1"), ("C", "B1"), ("C1", "C"), ("C2", "B1"), ("C2", "C1"), ("C3n2", "B1"),
                 ("C3n3", "B1"), ("C3n2", "C1"), ("C3n3", "C1"), ("C3n3", "C3n2")):
        if a in pooled and b in pooled:
            pa, pb = pooled[a], pooled[b]
            out = {}
            for k in ("missing", "crc", "timeout_frames"):
                z, ci = ztest(pa[k], pa["packets"], pb[k], pb["packets"])
                rel = None if pb[k] == 0 else round(100 * ((pa[k] / pa["packets"]) / (pb[k] / pb["packets"]) - 1), 1)
                out[k] = {"z": round(z, 2), "diff_pp": round(100 * (pa[k] / pa["packets"] - pb[k] / pb["packets"]), 3),
                          "ci95_pp": [round(100 * ci[0], 3), round(100 * ci[1], 3)], "relative_pct": rel}
            tests[f"{a}_vs_{b}"] = out
            print(f"{a} vs {b}:", out)
    (SET / "stats.json").write_text(json.dumps({"runs": rows, "pooled": pooled, "tests": tests}, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
