"""PR1-DART layered board campaign (issue #52), host orchestration only.

  build                         build every image used by Gates A/B (parallel)
  run <gate> <gap_us> <target>  flash (if needed) + reset both + capture + hop stats
                                gate: A (frozen fixed-channel images) | B (static AFH)
  summary                       rebuild matrix.csv / matrix.json from run results

Gate A uses the frozen 2026-09-29 prebuilt images (runs/pr1-board-test/build), byte-identical
to the baseline. Gate B images are built from this branch with -D PR1_ENABLE_AFH=1 only.
"""
import csv
import json
import os
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools"))

import pr1_experiment_controller as ctl  # noqa: E402
from pr1_board_sweep import FROZEN_BASELINE, _parse_kv_and_telemetry  # noqa: E402
from serial.tools import list_ports  # noqa: E402

HERE = Path(__file__).resolve().parent
PROJECT = REPO / "firmware" / "t3s3_sx1280_runtime"
USERP = Path(os.environ["USERPROFILE"])
PIO = str(USERP / ".platformio/penv/Scripts/pio.exe")
PY = str(USERP / ".platformio/penv/Scripts/python.exe")
BOOT_APP0 = USERP / ".platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
MAC = {"rx": "E8:06:90:96:83:38", "tx": "B8:F8:62:D9:26:B4"}
FROZEN_BUILD = REPO / "runs" / "pr1-board-test" / "build"
GAPS_B = [5000, 1000, 300, 150]
# Diagnostic RX variants (Gate B only; TX images are shared with gate "B").
RX_VARIANTS = {"Bm": "-D PR1_AFH_LOSS_MARGIN_MIN_US=1000\n    -D PR1_AFH_LOSS_MARGIN_MAX_US=1000",
               "Bs": "-D PR1_AFH_DIAG_SAME_FREQ=1"}
# Variants that also need their own TX image (same flags on TX).
TX_VARIANTS = {"Bs": "-D PR1_AFH_DIAG_SAME_FREQ=1", "Bt": "-D PR1_AFH_TX_SETTLE_US=200",
               "Bf29": "-D PR1_AFH_DIAG_FIXED_CHANNEL=29", "Bf20": "-D PR1_AFH_DIAG_FIXED_CHANNEL=20"}
RX_VARIANTS.update({"Bf29": "-D PR1_AFH_DIAG_FIXED_CHANNEL=29", "Bf20": "-D PR1_AFH_DIAG_FIXED_CHANNEL=20"})
# Gate C: adaptive channel map ON (both roles); everything else identical to final Gate B.
RX_VARIANTS["C"] = "-D PR1_ENABLE_ADAPTIVE_MAP=1"
TX_VARIANTS["C"] = "-D PR1_ENABLE_ADAPTIVE_MAP=1"
# Gate C1: same flags as B / C, built from the C1 tree (control plane on core 0). Separate
# image dirs keep the Gate C binaries intact.
RX_VARIANTS["B1"] = ""
TX_VARIANTS["B1"] = ""
RX_VARIANTS["C1"] = "-D PR1_ENABLE_ADAPTIVE_MAP=1"
TX_VARIANTS["C1"] = "-D PR1_ENABLE_ADAPTIVE_MAP=1"
# Diagnostic (C1 cache test): B1 + rarely-run code every 600 frames on RX only.
RX_VARIANTS["B1k"] = "-D PR1_DIAG_COLD_WORK_EVERY=600"
TX_VARIANTS["B1k"] = ""
# Gate C2: C1 infrastructure + the estimator configuration chosen by the offline replay
# (analysis/c2_sweep.py candidate K21_f070_s095_rein3_p3200_cap5s).
C2_FLAGS = ("-D PR1_ENABLE_ADAPTIVE_MAP=1\n    -D PR1_Q_EXCLUDE_PDR_Q15=22937\n    -D PR1_Q_EXCLUDE_SLOW_PDR_Q15=31129"
            "\n    -D PR1_Q_REINSTATE_SUCCESSES=3\n    -D PR1_Q_PROBE_INITIAL_MS=3200\n    -D PR1_Q_PROBE_MAX_MS=25600"
            "\n    -D PR1_MAP_MIN_INTERVAL_MS=5000")
RX_VARIANTS["C2"] = C2_FLAGS
TX_VARIANTS["C2"] = C2_FLAGS
ADAPTIVE_GATES = ("C", "C1", "C2")
C_PROFILE = {"adaptive_layers_text": "channel_map"}  # Bt: TX settle diag; RX uses the plain B-rx image
PLACEMENT = ("2026-10-03 operator photo placement_20261003.jpg: RX on desk top (antenna ~vertical, iron and "
             "bottles nearby), TX on white box below (antenna vertical). Unchanged for all gates.")


def image_dir(gate: str, role: str, gap: int | None) -> Path:
    env = "rf_tx_compile" if role == "tx" else "rf_rx_compile"
    if gate == "A":
        return FROZEN_BUILD / ("rx" if role == "rx" else f"tx-{gap}us") / env
    if role == "rx" and gate in RX_VARIANTS:
        return HERE / "build" / f"{gate}-rx" / env
    if role == "tx" and gate in TX_VARIANTS:
        return HERE / "build" / f"{gate}-tx-{gap}us" / env
    return HERE / "build" / (f"B-{role}" + (f"-{gap}us" if role == "tx" else "")) / env


def image_name(role: str, gap: int | None, variant: str) -> str:
    if role == "rx":
        return f"{variant}-rx"
    return f"{variant if variant in TX_VARIANTS else 'B'}-tx-{gap}us"


def render_ini(role: str, gap: int | None, variant: str = "B") -> Path:
    text = (PROJECT / "platformio.ini").read_text(encoding="utf-8")
    if role == "tx":
        extra = ("\n    " + TX_VARIANTS[variant]) if TX_VARIANTS.get(variant) else ""
        text = text.replace("-D PR1_TX_GAP_US=5000", f"-D PR1_TX_GAP_US={gap}\n    -D PR1_ENABLE_AFH=1" + extra, 1)
    else:
        extra = ("\n    " + RX_VARIANTS[variant]) if RX_VARIANTS.get(variant) else ""
        text = text.replace("-D PR1_RUNTIME_ROLE=2", "-D PR1_RUNTIME_ROLE=2\n    -D PR1_ENABLE_AFH=1" + extra, 1)
    name = image_name(role, gap, variant)
    path = HERE / "generated" / (name + ".ini")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return path


def build_one(item) -> tuple[str, int]:
    role, gap, variant = item
    ini = render_ini(role, gap, variant)
    out = image_dir(variant, role, gap)
    env = dict(os.environ, PLATFORMIO_BUILD_DIR=str(out.parent))
    log = out.parent.parent / f"{out.parent.name}.log"
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("w", encoding="utf-8") as fh:
        rc = subprocess.run([PIO, "run", "-d", str(PROJECT), "-c", str(ini), "-e", out.name],
                            env=env, stdout=fh, stderr=subprocess.STDOUT).returncode
    return out.parent.name, rc


def cmd_build() -> None:
    only = sys.argv[2:]  # optional subset, e.g. "B-tx-300us Bm-rx"
    items = ([("rx", None, "B")] + [("rx", None, v) for v in RX_VARIANTS] + [("tx", g, "B") for g in GAPS_B]
             + [("tx", 150, v) for v in TX_VARIANTS])
    if only:
        items = [i for i in items if image_name(i[0], i[1], i[2]) in only]
    with ThreadPoolExecutor(max_workers=2) as pool:
        for name, rc in pool.map(build_one, items):
            print(f"BUILD {name} rc={rc}", flush=True)
            if rc != 0:
                raise SystemExit(f"build failed: {name}")


def port_of(role: str) -> str:
    for p in list_ports.comports():
        if MAC[role] in (p.hwid or "").upper():
            return p.device
    raise RuntimeError(f"{role} board ({MAC[role]}) not connected")


STATE_FILE = HERE / "flashed.json"


def flash(gate: str, role: str, gap: int | None) -> dict:
    b = image_dir(gate, role, gap)
    fw = b / "firmware.bin"
    key = f"{gate}:{b}"
    state = json.loads(STATE_FILE.read_text()) if STATE_FILE.exists() else {}
    sha = ctl._sha256(fw)
    if state.get(role, {}).get("key") == key and state[role].get("sha256") == sha:
        return state[role]
    out = subprocess.run([PY, "-m", "esptool", "--chip", "esp32s3", "--port", port_of(role), "--baud", "921600",
                          "--before", "default_reset", "--after", "hard_reset", "write_flash", "-z",
                          "--flash_mode", "dio", "--flash_freq", "80m", "--flash_size", "4MB",
                          "0x0", str(b / "bootloader.bin"), "0x8000", str(b / "partitions.bin"),
                          "0xe000", str(BOOT_APP0), "0x10000", str(fw)], capture_output=True, text=True)
    if out.returncode != 0 or out.stdout.count("Hash of data verified") != 4 or MAC[role].lower() not in out.stdout.lower():
        raise RuntimeError(f"flash failed {role} {b}: {out.stdout[-300:]}{out.stderr[-300:]}")
    state[role] = {"key": key, "image": str(b.relative_to(REPO)), "sha256": sha, "flashed_utc": ctl._utc_now()}
    STATE_FILE.write_text(json.dumps(state, indent=2), encoding="utf-8")
    print(f"FLASH {role} <- {b.relative_to(REPO)} sha256 {sha[:12]}", flush=True)
    return state[role]


def pull(ser, fh, command: bytes, end_marker: str | None, timeout_s: float = 3.0) -> list[str]:
    """Send a pull command and log its reply (hop stats / ring dump)."""
    ctl._drain_stale_input(ser, settle_s=0.05)
    ser.write(command)
    ser.flush()
    deadline = time.monotonic() + timeout_s
    pending = bytearray()
    lines: list[str] = []
    idle = 0
    while time.monotonic() < deadline:
        line = ctl._read_complete_line(ser, pending)
        if line is None:
            idle += 1
            if end_marker is None and lines and idle >= 3:
                break
            continue
        idle = 0
        fh.write(line + "\n")
        fh.flush()
        lines.append(line)
        if end_marker and line.strip().startswith(end_marker):
            break
    return lines


def parse_hop(lines: list[str]) -> dict:
    out: dict = {}
    for ln in lines:
        if ln.startswith("PR1H ") or ln.startswith("PR1HQ "):
            for tok in ln.split()[1:]:
                k, _, v = tok.partition("=")
                out[k] = int(v) if v.lstrip("-").isdigit() else v
        elif ln.startswith("PR1HC "):
            for tok in ln.split()[1:]:
                k, _, v = tok.partition("=")
                out[f"channel_{k}"] = [int(x) for x in v.split(",")]
    return out


def make_relay(rx, tx, log_path: Path, stats: dict):
    """Gate C bench control plane (two-phase), run as capture's sleep_fn.

    RX PR1PROP -> host "MAP/PRB" -> TX validates (PR1ACK) -> host "ACK" -> RX commits
    (PR1COMMIT) -> host "COMMIT" (RX ok) or "ABORT" (RX not ok) -> TX stages (PR1STAGED).
    Any id that ends staged on only one side is a map/probe divergence and fails the run.
    Every line is logged with host time to ctrl.log.
    """
    def relay_sleep(seconds: float) -> None:
        end = time.monotonic() + seconds
        rx_buf, tx_buf = bytearray(), bytearray()
        old_rx, old_tx = rx.timeout, tx.timeout
        rx.timeout, tx.timeout = 0.005, 0.005
        ids = stats["ids"]
        try:
            with log_path.open("a", encoding="utf-8", buffering=1) as fh:
                def log(src: str, text: str) -> None:
                    fh.write(f"{time.monotonic():.4f} {src} {text}\n")

                def kvs(text: str) -> dict:
                    return dict(t.split("=", 1) for t in text.split()[1:] if "=" in t)

                def tx_wait(prefix: str, ident: str, timeout: float = 0.5):
                    t0 = time.monotonic()
                    while time.monotonic() - t0 < timeout:
                        tl = ctl._read_complete_line(tx, tx_buf)
                        if tl:
                            log("TX", tl)
                            if tl.startswith(prefix) and kvs(tl).get("id") == ident:
                                return tl, round(1000 * (time.monotonic() - t0), 2)
                    return None, round(1000 * (time.monotonic() - t0), 2)

                def send(dev, name: str, text: str) -> None:
                    dev.write((text + "\n").encode()); dev.flush()
                    log(name, text)

                while time.monotonic() < end:
                    tline = ctl._read_complete_line(tx, tx_buf)
                    if tline:
                        log("TX", tline)
                    line = ctl._read_complete_line(rx, rx_buf)
                    if not line:
                        continue
                    log("RX", line)
                    try:
                        kv = kvs(line)
                        if line.startswith("PR1PROP"):
                            ident = kv["id"]
                            stats["proposals"] += 1
                            if kv.get("type") == "map":
                                cmd = (f"MAP id={ident} v={kv['v']} old={kv['old']} oldbits={kv['oldbits']} "
                                       f"bits={kv['bits']} act={kv['act']}")
                            else:
                                cmd = f"PRB id={ident} ch={kv['ch']} at={kv['at']} v={kv['v']}"
                            ids[ident] = {"type": kv.get("type"), "prop": line, "tx_valid": None,
                                          "rx_commit": None, "tx_staged": None}
                            send(tx, "HOST->TX", cmd)
                            ack, ms = tx_wait("PR1ACK", ident)
                            stats["relay_ms"].append(ms)
                            ok = 1 if ack and kvs(ack).get("ok") == "1" else 0
                            ids[ident]["tx_valid"] = ok
                            if not ack:
                                stats["tx_no_ack"] += 1
                            send(rx, "HOST->RX", f"ACK id={ident} ok={ok}")
                        elif line.startswith("PR1COMMIT"):
                            ident = kv.get("id")
                            stats["commits"] += 1
                            entry = ids.setdefault(ident, {"type": None, "prop": None, "tx_valid": None,
                                                           "rx_commit": None, "tx_staged": None})
                            entry["rx_commit"] = kv.get("ok") == "1"
                            if entry["rx_commit"]:
                                send(tx, "HOST->TX", f"COMMIT id={ident}")
                                st, _ = tx_wait("PR1STAGED", ident)
                                entry["tx_staged"] = bool(st and kvs(st).get("ok") == "1")
                                if not entry["tx_staged"]:
                                    stats["divergence"] += 1  # RX staged, TX did not
                            else:
                                send(tx, "HOST->TX", f"ABORT id={ident}")
                                tx_wait("PR1ABORTED", ident, 0.3)
                                entry["tx_staged"] = False
                    except (KeyError, ValueError) as exc:
                        stats["bad_lines"] += 1
                        log("HOST", f"unparsed line ({exc!r})")
        finally:
            rx.timeout, tx.timeout = old_rx, old_tx
    return relay_sleep


def _kv_line(ln: str) -> dict:
    out = {}
    for k, _, v in (t.partition("=") for t in ln.split()[1:]):
        out[k] = v if k == "bits" else (int(v) if v.isdigit() else v)
    return out


def parse_quality(text: str) -> dict:
    out: dict = {"channels": [], "events": [], "activations": []}
    for ln in text.splitlines():
        if ln.startswith("PR1QS "):
            out["summary"] = _kv_line(ln)
        elif ln.startswith("PR1QC "):
            out["channels"].append(_kv_line(ln))
        elif ln.startswith("PR1QE "):
            out["events"].append(_kv_line(ln))
        elif ln.startswith("PR1QA "):
            out["activations"].append(_kv_line(ln))
    return out


QE_KIND = {0: "proposed", 1: "committed", 2: "commit_late", 3: "expired", 4: "activated", 5: "tx_staged",
           6: "tx_rejected", 7: "suspect", 8: "excluded", 9: "probe_result", 10: "reincluded", 11: "recovered"}


def map_agreement(rxq: dict, txq: dict, relay: dict, cut: int | None = None) -> dict:
    rx_act = [(a["v"], a["activation"]) for a in rxq["activations"]]
    tx_act = [(a["v"], a["activation"]) for a in txq["activations"]]
    rs, ts = rxq.get("summary", {}), txq.get("summary", {})
    one_sided = [i for i, e in relay["ids"].items() if bool(e.get("rx_commit")) != bool(e.get("tx_staged"))]
    out = {
        "rx_activations": rx_act, "tx_activations": tx_act,
        "activation_lists_equal": rx_act == tx_act,
        "final_version_rx": rs.get("map_version"), "final_version_tx": ts.get("map_version"),
        "final_bits_rx": rs.get("bits"), "final_bits_tx": ts.get("bits"),
        "pending_rx": [rs.get("pending"), rs.get("pending_v"), rs.get("pending_act")],
        "pending_tx": [ts.get("pending"), ts.get("pending_v"), ts.get("pending_act")],
        "final_map_equal": rs.get("map_version") == ts.get("map_version") and rs.get("bits") == ts.get("bits"),
        "one_sided_ids": one_sided,
    }
    out["version_mismatch"] = (not out["activation_lists_equal"]) or (not out["final_map_equal"]) or bool(one_sided) \
        or relay["divergence"] > 0
    # In-window view (C1+): only activations before the RX frame reached at the end of the
    # measured window; post-run pulls stall one side and can leave later activations one-sided.
    if cut is not None:
        rw = [x for x in rx_act if x[1] < cut]; tw = [x for x in tx_act if x[1] < cut]
        out["cut_logical"] = cut
        out["in_window_activations_equal"] = rw == tw
        out["in_window_version_mismatch"] = (rw != tw) or bool(one_sided) or relay["divergence"] > 0
    return out


def cmd_run(gate: str, gap: int, target: int, set_name: str | None = None, tag: str | None = None) -> None:
    run_id = f"{gate}-gap-{gap}us-{target}"
    run_dir = (HERE / set_name if set_name else HERE / f"gate-{gate}") / (run_id + (f"-{tag}" if tag else ""))
    run_dir.mkdir(parents=True, exist_ok=True)
    if (run_dir / "result.json").exists() and json.loads((run_dir / "result.json").read_text())["derived"]["target_reached"]:
        print(f"SKIP {run_id} (complete)", flush=True)
        return
    if (run_dir / "rx.log").exists():
        ctl.archive_partial_attempt(run_dir)
    rx_img = flash(gate, "rx", None)
    tx_img = flash(gate, "tx", gap)
    meta = {
        "schema_version": 1, "run_id": run_id, "created_utc": ctl._utc_now(), "gate": gate, "gap_us": gap,
        "target_packets": target, "phase": "revalidate", "firmware_sha": ctl.FROZEN_FIRMWARE_SHA,
        "frozen_baseline": dict(FROZEN_BASELINE),
        "rx_variant": RX_VARIANTS.get(gate),
        "feature_flags": {"PR1_ENABLE_AFH": 1 if gate != "A" else 0, "afh_map": "static all-40" if gate != "A" else None,
                          "channel_quality": 1 if gate in ADAPTIVE_GATES else 0, "fec": 0, "arq": 0, "phy_ladder": 0,
                          "controller": 0, "control_plane": ("usb_host_relay" + ("" if gate == "C" else ", serial/parse/format on core 0"))
                          if gate in ADAPTIVE_GATES else None,
                          "serial_core": 0 if gate in ("B1", "C1", "C2") else 1},
        "images": {"rx": rx_img, "tx": tx_img}, "placement": PLACEMENT,
        "files": {"rx_log": "rx.log", "tx_log": "tx.log"},
        "build_identity": ctl.collect_build_identity(PROJECT, source_root=REPO),
        "notes": "issue #52 layered activation",
    }
    (run_dir / "metadata.json").write_text(json.dumps(meta, indent=2), encoding="utf-8")
    # Measured frame period is gap + ~2850 us on this link (A/B period_est). No serial
    # I/O during the measured window: wait past the target, then one final snapshot.
    period_s = (gap + 2850) / 1e6
    meta["host_polling"] = "none during measured window; single final snapshot"
    (run_dir / "metadata.json").write_text(json.dumps(meta, indent=2), encoding="utf-8")
    rx = ctl._open_serial(port_of("rx"))
    tx = ctl._open_serial(port_of("tx"))
    hop = {}
    try:
        ctl._drain_stale_input(tx); ctl._pulse_reset(tx)
        ctl._drain_stale_input(rx); ctl._pulse_reset(rx)
        relay_stats = {"proposals": 0, "commits": 0, "divergence": 0, "tx_no_ack": 0, "bad_lines": 0, "relay_ms": [], "ids": {}}
        extra = {}
        if gate in ADAPTIVE_GATES:
            extra = {"sleep_fn": make_relay(rx, tx, run_dir / "ctrl.log", relay_stats), "profile": C_PROFILE}
        result = ctl.capture_run_from_serial(meta, run_dir, rx, tx,
                                             initial_wait_s=1.08 * target * period_s + 1.0,
                                             settle_s=0.0, max_polls=100, poll_progress=False, **extra)
        if gate != "A":
            with (run_dir / "rx.log").open("a", encoding="utf-8") as rfh, (run_dir / "tx.log").open("a", encoding="utf-8") as tfh:
                hop["rx"] = parse_hop(pull(rx, rfh, b"h", None))
                hop["tx"] = parse_hop(pull(tx, tfh, b"h", None))
                pull(rx, rfh, b"H", "PR1HE_END", timeout_s=6.0)
                pull(tx, tfh, b"H", "PR1HE_END", timeout_s=6.0)
            for role, log in (("rx", run_dir / "rx.log"), ("tx", run_dir / "tx.log")):
                meta_kv, _ = _parse_kv_and_telemetry(log.read_text(encoding="utf-8", errors="replace").splitlines())
                hop[role]["boot_schedule_fp"] = meta_kv.get("afh_schedule_fp")
                hop[role]["boot_session_seed"] = meta_kv.get("afh_session_seed")
                hop[role]["boot_session_id"] = meta_kv.get("afh_session_id")
                hop[role]["boot_map_version"] = meta_kv.get("afh_map_version")
            hop["schedule_fp_match"] = hop["rx"].get("boot_schedule_fp") == hop["tx"].get("boot_schedule_fp")
            result["hop"] = hop
            if gate in ADAPTIVE_GATES:
                rxq = parse_quality((run_dir / "rx.log").read_text(encoding="utf-8", errors="replace"))
                txq = parse_quality((run_dir / "tx.log").read_text(encoding="utf-8", errors="replace"))
                agree = map_agreement(rxq, txq, relay_stats, hop["rx"].get("logical"))
                kinds = {}
                for e in rxq["events"]:
                    kinds[QE_KIND.get(e["kind"], e["kind"])] = kinds.get(QE_KIND.get(e["kind"], e["kind"]), 0) + 1
                result["quality"] = {"rx": rxq, "tx_summary": txq.get("summary"), "tx_events": txq["events"],
                                     "map_agreement": agree, "relay": relay_stats, "rx_event_counts": kinds}
            (run_dir / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    finally:
        tx.close(); rx.close()
    m = result["metrics"]
    line = (f"RUN {run_id} packets={result['packet_count']} missing={m['missing']} "
            f"loss={100 * result['derived']['loss_rate']:.3f}% crc_bad={m['crc_bad']} rssi={m['rssi_dbm']} "
            f"sched_miss={m['scheduler_misses']} ready_p99={m['irq_to_rx_ready_us_p99']} "
            f"spi_to_rearm_p99={m['spi_end_to_rearm_start_us_p99']} polls={result['evidence']['progress_polls']}")
    if gate != "A":
        r, t = hop["rx"], hop["tx"]
        line += (f" | fp_match={hop['schedule_fp_match']} agree={r.get('agree')} disagree={r.get('disagree')} "
                 f"timeouts={r.get('timeout_advances')} resync={r.get('resync_entries')} locks={r.get('locks')} "
                 f"rx_retune_p99={r.get('retune_us_p99')} tx_retune_p99={t.get('retune_us_p99')} "
                 f"period_est={r.get('period_est_us')}")
    if "quality" in result:
        q = result["quality"]; s = q["rx"].get("summary", {}); a = q["map_agreement"]
        line += (f" | C: map_v={s.get('map_version')} active={s.get('active')} min_active={s.get('min_active_seen')} "
                 f"events={q['rx_event_counts']} map_equal={a['final_map_equal']} act_lists_equal={a['activation_lists_equal']} "
                 f"version_mismatch={a['version_mismatch']} in_window_mismatch={a.get('in_window_version_mismatch')} emit_dropped={s.get('emit_dropped')} "
                 f"relay={ {k: v for k, v in q['relay'].items() if k not in ('relay_ms', 'ids')} }")
        if a["version_mismatch"]:
            line += " | FAIL: TX/RX map divergence (evidence kept in ctrl.log / result.json)"
    print(line, flush=True)
    write_summary(run_dir, result, hop)


def write_summary(run_dir: Path, result: dict, hop: dict) -> None:
    m, d = result["metrics"], result["derived"]
    rows = [f"# {result['run_id']}", "", f"- packets {result['packet_count']}, missing {m['missing']}, "
            f"loss {100 * d['loss_rate']:.3f} %, CRC good {m['crc_good']}, CRC bad {m['crc_bad']}, RSSI {m['rssi_dbm']}",
            f"- queue {m['queue_depth']}/{m['max_queue_depth']}, scheduler misses {m['scheduler_misses']}",
            f"- p99 µs: irq→spi {m['irq_to_spi_us_p99']}, spi {m['spi_duration_us_p99']}, rx_proc {m['rx_processing_us_p99']}, "
            f"spi_end→rearm {m['spi_end_to_rearm_start_us_p99']}, rearm {m['rx_rearm_us_p99']}, irq→ready {m['irq_to_rx_ready_us_p99']}",
            f"- progress polls {result['evidence'].get('progress_polls')}"]
    if hop:
        r, t = hop["rx"], hop["tx"]
        rows += [f"- schedule fingerprint TX==RX: {hop['schedule_fp_match']}, session {r.get('boot_session_id')}/"
                 f"{t.get('boot_session_id')}, map_version {r.get('map_version')}/{t.get('map_version')}",
                 f"- RX: agree {r.get('agree')}, disagree {r.get('disagree')}, timeout advances {r.get('timeout_advances')}, "
                 f"resync {r.get('resync_entries')}, locks {r.get('locks')}, max consecutive timeouts {r.get('max_consecutive_timeouts')}, "
                 f"period est {r.get('period_est_us')} µs",
                 f"- retune p99/max µs: RX {r.get('retune_us_p99')}/{r.get('retune_us_max')}, TX {t.get('retune_us_p99')}/{t.get('retune_us_max')}; "
                 f"hop compute p99 RX {r.get('hop_compute_us_p99')} TX {t.get('hop_compute_us_p99')}",
                 f"- TX logical next {t.get('logical')}, RX expected {r.get('logical')}"]
    (run_dir / "summary.md").write_text("\n".join(rows) + "\n", encoding="utf-8")


def cmd_summary() -> None:
    rows = []
    for res in sorted(list(HERE.glob("gate-*/*/result.json")) + list(HERE.glob("gateC-interleave/*/result.json")) + list(HERE.glob("preC/*/result.json"))
                 + list(HERE.glob("gateC1-interleave/*/result.json")) + list(HERE.glob("smokeC1/*/result.json")) + list(HERE.glob("gateC2-interleave/*/result.json"))
                 + list(HERE.glob("gate-C2/*/result.json"))):
        if "partial" in res.parts: continue
        r = json.loads(res.read_text())
        if "metrics" not in r: continue
        m = r["metrics"]; h = r.get("hop", {})
        rows.append({"gate": r["run_id"].split("-")[0], "set": res.parent.parent.name, "dir": res.parent.name, "run_id": r["run_id"], "gap_us": r["gap_us"],
                     "packets": r["packet_count"], "missing": m["missing"], "loss_pct": round(100 * r["derived"]["loss_rate"], 4),
                     "crc_good": m["crc_good"], "crc_bad": m["crc_bad"], "rssi_dbm": m["rssi_dbm"],
                     "queue_depth": m["queue_depth"], "max_queue_depth": m["max_queue_depth"],
                     "scheduler_misses": m["scheduler_misses"], "irq_to_spi_us_p99": m["irq_to_spi_us_p99"],
                     "spi_duration_us_p99": m["spi_duration_us_p99"], "rx_processing_us_p99": m["rx_processing_us_p99"],
                     "spi_end_to_rearm_start_us_p99": m["spi_end_to_rearm_start_us_p99"], "rx_rearm_us_p99": m["rx_rearm_us_p99"],
                     "irq_to_rx_ready_us_p99": m["irq_to_rx_ready_us_p99"],
                     "afh_fp_match": h.get("schedule_fp_match"), "afh_agree": h.get("rx", {}).get("agree"),
                     "afh_disagree": h.get("rx", {}).get("disagree"), "afh_timeouts": h.get("rx", {}).get("timeout_advances"),
                     "afh_resync": h.get("rx", {}).get("resync_entries"), "afh_rx_retune_p99": h.get("rx", {}).get("retune_us_p99"),
                     "afh_tx_retune_p99": h.get("tx", {}).get("retune_us_p99"), "polls": r["evidence"].get("progress_polls")})
    (HERE / "matrix.json").write_text(json.dumps(rows, indent=2), encoding="utf-8")
    if rows:
        with (HERE / "matrix.csv").open("w", newline="", encoding="utf-8") as fh:
            w = csv.DictWriter(fh, fieldnames=list(rows[0])); w.writeheader(); w.writerows(rows)
    print(f"matrix rows: {len(rows)}")


if __name__ == "__main__":
    c = sys.argv[1]
    if c == "build":
        cmd_build()
    elif c == "run":
        cmd_run(sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), *(sys.argv[5:7]))
    elif c == "summary":
        cmd_summary()
