import json
import sys
import tempfile
import unittest
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from pr1_board_sweep import build_run_metadata
from pr1_experiment_controller import (  # noqa: E402
    FROZEN_FIRMWARE_SHA,
    archive_partial_attempt,
    build_dry_run_plan,
    build_session_metadata,
    _request_snapshot,
    capture_run_from_serial,
    estimate_initial_wait_s,
    prepare_build_matrix,
)


RX_BOOT = """\
PR1_RUNTIME_BOOT
runtime_profile=pr1-fixed-flrc-v1
runtime_role=rx
rf_enabled=1
sx1280_spi_hz=2000000
PR1_FIXED_FLRC_PROFILE
frequency_mhz=2404.000
bitrate_kbps=1300
coding_rate=3
output_dbm=0
tx_gap_us=5000
packet_bytes=116
adaptive_layers=off
PR1_RUNTIME_LIVE_READY
"""

TX_BOOT = """\
PR1_RUNTIME_BOOT
runtime_profile=pr1-fixed-flrc-v1
runtime_role=tx
rf_enabled=1
sx1280_spi_hz=2000000
PR1_FIXED_FLRC_PROFILE
frequency_mhz=2404.000
bitrate_kbps=1300
coding_rate=3
output_dbm=0
tx_gap_us=5000
packet_bytes=116
adaptive_layers=off
PR1_RUNTIME_LIVE_READY
"""


def rx_snapshot(crc_good: int, missing: int, timestamp: int) -> str:
    return f"""\
PR1T v=1 t_us={timestamp} field=rssi_dbm value=-49
PR1T v=1 t_us={timestamp} field=crc_good value={crc_good}
PR1T v=1 t_us={timestamp} field=crc_bad value=1
PR1T v=1 t_us={timestamp} field=missing value={missing}
PR1T v=1 t_us={timestamp} field=queue_depth value=0
PR1T v=1 t_us={timestamp} field=max_queue_depth value=1
PR1T v=1 t_us={timestamp} field=irq_to_spi_us value=28
PR1T v=1 t_us={timestamp} field=spi_duration_us value=61
PR1T v=1 t_us={timestamp} field=rx_processing_us value=94
PR1T v=1 t_us={timestamp} field=spi_end_to_rearm_start_us value=18
PR1T v=1 t_us={timestamp} field=rx_rearm_us value=24
PR1T v=1 t_us={timestamp} field=irq_to_rx_ready_us value=131
PR1T v=1 t_us={timestamp} field=trace_overwrites value=0
"""


TX_SNAPSHOT = "PR1T v=1 t_us=500 field=scheduler_misses value=0\n"


class FakeSerial:
    def __init__(self, boot: str, snapshots: list[str]):
        self._queue = deque((line + "\n").encode() for line in boot.splitlines())
        self._snapshots = deque(snapshots)
        self.writes: list[bytes] = []

    def readline(self):
        if self._queue:
            return self._queue.popleft()
        return b""

    def write(self, data: bytes):
        self.writes.append(data)
        if data.lower() == b"t" and self._snapshots:
            block = self._snapshots.popleft()
            self._queue.extend((line + "\n").encode() for line in block.splitlines())
        return len(data)

    def flush(self):
        return None


class ExperimentControllerTests(unittest.TestCase):
    def test_session_metadata_records_frozen_and_build_identity(self):
        identity = {
            "source_head_sha": "host-head",
            "git_dirty": False,
            "platformio_version": "PlatformIO Core 6.1.18",
            "platformio_ini_sha256": "abc",
        }
        meta = build_session_metadata(
            firmware_sha=FROZEN_FIRMWARE_SHA,
            target_packets=1000,
            rx_port="COM6",
            tx_port="COM7",
            identity=identity,
            created_utc="2026-09-27T04:00:00Z",
        )
        self.assertEqual(meta["firmware_sha"], FROZEN_FIRMWARE_SHA)
        self.assertEqual(meta["target_packets"], 1000)
        self.assertEqual(meta["ports"], {"rx": "COM6", "tx": "COM7"})
        self.assertEqual(meta["build_identity"]["source_head_sha"], "host-head")
        self.assertEqual(meta["created_utc"], "2026-09-27T04:00:00Z")
        self.assertEqual(meta["sweep_gaps_us"], [5000, 1000, 500, 300, 250, 225, 200, 175, 150, 125, 0])

    def test_build_matrix_uses_separate_cached_build_dirs_without_editing_project(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td) / "session"
            project = Path(td) / "runtime"
            project.mkdir(parents=True)
            baseline = """\
[platformio]
default_envs = safe
[env:rf_tx_compile]
build_flags =
    -D PR1_RF_ENABLED=1
    -D PR1_RUNTIME_ROLE=1
    -D PR1_TX_GAP_US=5000
"""
            (project / "platformio.ini").write_text(baseline, encoding="utf-8")
            matrix = prepare_build_matrix(root, project)
            self.assertEqual(len(matrix["tx"]), 11)
            self.assertEqual(matrix["tx"][0]["gap_us"], 5000)
            self.assertEqual(matrix["tx"][-1]["gap_us"], 0)
            self.assertNotEqual(matrix["tx"][0]["build_dir"], matrix["tx"][1]["build_dir"])
            self.assertEqual((project / "platformio.ini").read_text(encoding="utf-8"), baseline)
            self.assertIn("PLATFORMIO_BUILD_DIR", matrix["tx"][0]["env"])
            self.assertTrue(Path(matrix["tx"][0]["config_path"]).exists())

    def test_dry_run_plan_flashes_rx_once_and_tx_for_each_gap(self):
        plan = build_dry_run_plan(
            project_dir=Path("firmware/t3s3_sx1280_runtime"),
            session_root=Path("runs/pr1-2026-09-28"),
            rx_port="COM6",
            tx_port="COM7",
            include_safe=True,
        )
        kinds = [step["kind"] for step in plan["steps"]]
        self.assertEqual(kinds.count("safe_flash"), 2)
        self.assertEqual(kinds.count("rx_flash"), 1)
        self.assertEqual(kinds.count("tx_gap_flash"), 11)
        self.assertEqual(kinds.count("rx_reset"), 11)
        for index, kind in enumerate(kinds):
            if kind == "tx_gap_flash":
                self.assertEqual(kinds[index + 1], "rx_reset")
        self.assertEqual(plan["steps"][-1]["kind"], "analyze")

    def test_capture_auto_polls_then_settles_and_writes_result(self):
        with tempfile.TemporaryDirectory() as td:
            run_dir = Path(td)
            meta = build_run_metadata(
                5000,
                target_packets=1000,
                phase="baseline",
                firmware_sha=FROZEN_FIRMWARE_SHA,
                run_id="01-gap-5000us",
            )
            rx = FakeSerial(
                RX_BOOT,
                [
                    rx_snapshot(790, 10, 100),
                    rx_snapshot(995, 5, 200),
                    rx_snapshot(1095, 5, 300),
                ],
            )
            tx = FakeSerial(TX_BOOT, [TX_SNAPSHOT])
            sleeps: list[float] = []
            result = capture_run_from_serial(
                meta,
                run_dir,
                rx,
                tx,
                sleep_fn=sleeps.append,
                initial_wait_s=0,
                poll_wait_s=0,
                settle_s=0,
                read_timeout_s=0.01,
            )
            self.assertTrue(result["derived"]["target_reached"])
            self.assertEqual(result["packet_count"], 1100)
            self.assertTrue((run_dir / "result.json").exists())
            state = json.loads((run_dir / "run_state.json").read_text(encoding="utf-8"))
            self.assertEqual(state["status"], "complete")
            self.assertGreaterEqual(rx.writes.count(b"t"), 3)
            self.assertEqual(tx.writes.count(b"t"), 1)

    def test_interrupt_preserves_partial_logs_and_state(self):
        with tempfile.TemporaryDirectory() as td:
            run_dir = Path(td)
            meta = build_run_metadata(
                5000,
                target_packets=1000,
                phase="baseline",
                firmware_sha=FROZEN_FIRMWARE_SHA,
                run_id="01-gap-5000us",
            )
            rx = FakeSerial(RX_BOOT, [rx_snapshot(100, 0, 100)])
            tx = FakeSerial(TX_BOOT, [TX_SNAPSHOT])

            def interrupt(_seconds):
                raise KeyboardInterrupt()

            with self.assertRaises(KeyboardInterrupt):
                capture_run_from_serial(
                    meta,
                    run_dir,
                    rx,
                    tx,
                    sleep_fn=interrupt,
                    initial_wait_s=0.1,
                    read_timeout_s=0.01,
                )
            state = json.loads((run_dir / "run_state.json").read_text(encoding="utf-8"))
            self.assertEqual(state["status"], "interrupted")
            self.assertTrue((run_dir / "rx.log").exists())
            self.assertTrue((run_dir / "tx.log").exists())

            archived = archive_partial_attempt(run_dir, stamp="20260927T040000Z")
            self.assertTrue(Path(archived["rx_log"]).exists())
            self.assertTrue(Path(archived["tx_log"]).exists())
            self.assertFalse((run_dir / "rx.log").exists())

    def test_snapshot_joins_line_split_across_usb_reads(self):
        class SplitSerial:
            def __init__(self):
                self._reads = deque([
                    b"PR1T v=1 t_us=1 field=crc_good value=10\n",
                    b"PR1T v=1 t_us=1 field=miss",  # readline() timed out mid-line
                    b"",
                    b"ing value=2\n",
                    b"PR1T v=1 t_us=1 field=capability_mask value=12\n",
                ])

            def write(self, data):
                return len(data)

            def readline(self):
                return self._reads.popleft() if self._reads else b""

        with tempfile.TemporaryDirectory() as tmp:
            with (Path(tmp) / "rx.log").open("w", encoding="utf-8") as log_fh:
                lines = _request_snapshot(SplitSerial(), log_fh, timeout_s=1.0)
        self.assertIn("PR1T v=1 t_us=1 field=missing value=2", lines)
        self.assertTrue(lines[-1].endswith("field=capability_mask value=12"))

    def test_initial_wait_scales_with_target_and_gap(self):
        self.assertGreater(estimate_initial_wait_s(5000, 1000), estimate_initial_wait_s(0, 1000))
        self.assertGreater(estimate_initial_wait_s(5000, 10000), estimate_initial_wait_s(5000, 1000))


if __name__ == "__main__":
    unittest.main()
