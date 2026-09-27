import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from pr1_board_sweep import (  # noqa: E402
    SWEEP_GAPS_US,
    analyze_sweep,
    build_run_metadata,
    parse_run_logs,
    write_sweep_outputs,
    render_tx_sweep_config,
    build_flash_command,
)

RX_LOG = """\
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
PR1T v=1 t_us=100 field=rssi_dbm value=-49
PR1T v=1 t_us=100 field=crc_good value=990
PR1T v=1 t_us=100 field=crc_bad value=6
PR1T v=1 t_us=100 field=missing value=10
PR1T v=1 t_us=100 field=queue_depth value=0
PR1T v=1 t_us=100 field=max_queue_depth value=1
PR1T v=1 t_us=100 field=irq_to_spi_us value=28
PR1T v=1 t_us=100 field=spi_duration_us value=61
PR1T v=1 t_us=100 field=rx_processing_us value=94
PR1T v=1 t_us=100 field=spi_end_to_rearm_start_us value=18
PR1T v=1 t_us=100 field=rx_rearm_us value=24
PR1T v=1 t_us=100 field=irq_to_rx_ready_us value=131
"""

TX_LOG = """\
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
tx_gap_us=200
packet_bytes=116
adaptive_layers=off
PR1_RUNTIME_LIVE_READY
PR1T v=1 t_us=200 field=scheduler_misses value=3
"""


class BoardSweepTests(unittest.TestCase):
    def test_sweep_order_is_frozen(self):
        self.assertEqual(SWEEP_GAPS_US, [5000, 1000, 500, 300, 250, 225, 200, 175, 150, 125, 0])

    def test_metadata_contains_reproducibility_fields(self):
        meta = build_run_metadata(200, target_packets=1000, phase="baseline", firmware_sha="abc123")
        self.assertEqual(meta["gap_us"], 200)
        self.assertEqual(meta["target_packets"], 1000)
        self.assertEqual(meta["phase"], "baseline")
        self.assertEqual(meta["firmware_sha"], "abc123")
        self.assertEqual(meta["frozen_baseline"]["sx1280_spi_hz"], 2_000_000)
        self.assertEqual(meta["frozen_baseline"]["packet_bytes"], 116)
        self.assertFalse(meta["frozen_baseline"]["adaptive_layers"])

    def test_parser_merges_rx_and_tx_and_does_not_double_count_crc_bad(self):
        meta = build_run_metadata(200, target_packets=1000, phase="baseline", firmware_sha="abc123")
        result = parse_run_logs(meta, RX_LOG.splitlines(), TX_LOG.splitlines())
        self.assertEqual(result["packet_count"], 1000)
        self.assertEqual(result["metrics"]["crc_good"], 990)
        self.assertEqual(result["metrics"]["crc_bad"], 6)
        self.assertEqual(result["metrics"]["missing"], 10)
        self.assertEqual(result["metrics"]["scheduler_misses"], 3)
        self.assertEqual(result["metrics"]["irq_to_rx_ready_us_p99"], 131)
        self.assertAlmostEqual(result["derived"]["loss_rate"], 0.01)
        self.assertTrue(result["derived"]["target_reached"])
        self.assertEqual(result["evidence"]["packet_count_source"], "rx_sequence_span=crc_good+missing")

    def test_parser_rejects_wrong_tx_gap_or_baseline(self):
        meta = build_run_metadata(175, target_packets=1000, phase="baseline", firmware_sha="abc123")
        with self.assertRaisesRegex(ValueError, "tx_gap_us"):
            parse_run_logs(meta, RX_LOG.splitlines(), TX_LOG.splitlines())
        bad_rx = RX_LOG.replace("sx1280_spi_hz=2000000", "sx1280_spi_hz=4000000")
        meta = build_run_metadata(200, target_packets=1000, phase="baseline", firmware_sha="abc123")
        with self.assertRaisesRegex(ValueError, "sx1280_spi_hz"):
            parse_run_logs(meta, bad_rx.splitlines(), TX_LOG.splitlines())

    def test_transition_detector_marks_only_bracketing_gaps_for_10k(self):
        rows = []
        for gap, packet_count, missing in [
            (500, 1000, 0), (300, 1000, 1), (250, 1000, 1),
            (225, 1000, 2), (200, 1000, 8), (175, 1000, 11),
            (150, 1000, 500), (125, 1000, 501), (0, 1000, 502),
        ]:
            rows.append({
                "gap_us": gap,
                "packet_count": packet_count,
                "metrics": {"missing": missing, "crc_bad": 0},
                "derived": {"loss_rate": missing / packet_count},
                "classification": {"label": "test"},
            })
        analysis = analyze_sweep(rows)
        self.assertIn({"higher_gap_us": 175, "lower_gap_us": 150}, [
            {"higher_gap_us": t["higher_gap_us"], "lower_gap_us": t["lower_gap_us"]}
            for t in analysis["transitions"]
        ])
        self.assertEqual(analysis["revalidate_10000_gaps_us"], [175, 150])

    def test_transition_detector_does_not_bridge_missing_sweep_points(self):
        rows = [
            {"gap_us": 225, "packet_count": 1000, "derived": {"loss_rate": 0.001}, "classification": {"label": "no_loss_observed"}},
            {"gap_us": 175, "packet_count": 1000, "derived": {"loss_rate": 0.5}, "classification": {"label": "receiver_turnaround_mixed"}},
        ]
        analysis = analyze_sweep(rows)
        self.assertEqual(analysis["transitions"], [])
        self.assertEqual(analysis["revalidate_10000_gaps_us"], [])
        self.assertEqual(analysis["missing_gaps_us"], [5000, 1000, 500, 300, 250, 200, 150, 125, 0])

    def test_render_tx_sweep_config_changes_only_gap_macro(self):
        base = "[env:rf_tx_compile]\nbuild_flags =\n    -D PR1_RF_ENABLED=1\n    -D PR1_RUNTIME_ROLE=1\n    -D PR1_TX_GAP_US=5000\n"
        rendered = render_tx_sweep_config(base, 175)
        self.assertIn("-D PR1_TX_GAP_US=175", rendered)
        self.assertIn("-D PR1_RF_ENABLED=1", rendered)
        self.assertNotIn("-D PR1_TX_GAP_US=5000", rendered)
        with self.assertRaisesRegex(ValueError, "exactly one frozen gap macro"):
            render_tx_sweep_config(base.replace("5000", "1000"), 175)

    def test_flash_command_uses_custom_config_without_editing_project_file(self):
        cmd = build_flash_command(
            project_dir=Path("firmware/t3s3_sx1280_runtime"),
            config_path=Path("runs/generated-175.ini"),
            environment="rf_tx_compile",
            port="COM7",
        )
        self.assertEqual(cmd[:2], ["pio", "run"])
        self.assertIn("--project-dir", cmd)
        self.assertIn("--project-conf", cmd)
        self.assertIn("rf_tx_compile", cmd)
        self.assertIn("--upload-port", cmd)
        self.assertEqual(cmd[-1], "COM7")

    def test_flash_tx_cli_dry_run_writes_generated_config(self):
        import subprocess
        with tempfile.TemporaryDirectory() as td:
            project = Path(td) / "runtime"
            project.mkdir()
            (project / "platformio.ini").write_text(
                "[env:rf_tx_compile]\nbuild_flags =\n    -D PR1_RF_ENABLED=1\n    -D PR1_RUNTIME_ROLE=1\n    -D PR1_TX_GAP_US=5000\n",
                encoding="utf-8",
            )
            generated = Path(td) / "generated.ini"
            proc = subprocess.run([
                sys.executable, str(ROOT / "tools" / "pr1_board_sweep.py"),
                "flash-tx", str(project), "--gap-us", "175", "--port", "COM7",
                "--config-out", str(generated), "--dry-run",
            ], text=True, capture_output=True, check=False)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            self.assertIn("PR1_TX_GAP_US=175", generated.read_text(encoding="utf-8"))
            self.assertIn("pio run", proc.stdout)

    def test_sweep_bottleneck_summary_uses_transition_evidence(self):
        rows = []
        for gap, missing in [(225, 1), (200, 2), (175, 4), (150, 500)]:
            rows.append({
                "gap_us": gap,
                "packet_count": 1000,
                "metrics": {"missing": missing, "crc_bad": 0},
                "derived": {"loss_rate": missing / 1000},
                "classification": {"label": "spi_transaction" if gap == 150 else "no_loss_observed", "confidence": "low"},
            })
        analysis = analyze_sweep(rows)
        self.assertEqual(analysis["bottleneck_summary"]["label"], "spi_transaction")
        self.assertEqual(analysis["bottleneck_summary"]["confidence"], "medium")

    def test_outputs_json_csv_and_svg_without_plotting_dependency(self):
        meta = build_run_metadata(200, target_packets=1000, phase="baseline", firmware_sha="abc123")
        result = parse_run_logs(meta, RX_LOG.splitlines(), TX_LOG.splitlines())
        with tempfile.TemporaryDirectory() as td:
            out = Path(td)
            write_sweep_outputs([result], out)
            self.assertTrue((out / "results.json").exists())
            self.assertTrue((out / "results.csv").exists())
            self.assertTrue((out / "loss_transition.svg").exists())
            self.assertTrue((out / "timing_p99.svg").exists())
            doc = json.loads((out / "results.json").read_text(encoding="utf-8"))
            self.assertEqual(doc["runs"][0]["gap_us"], 200)
            self.assertIn("<svg", (out / "loss_transition.svg").read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
