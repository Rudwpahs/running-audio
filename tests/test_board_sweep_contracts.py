import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from pr1_board_sweep import (  # noqa: E402
    RESULT_CSV_FIELDS,
    analyze_sweep,
    classify_bottleneck,
    create_plan,
    experiment_status,
    prepare_next_run,
    write_sweep_outputs,
)


def captured_logs(gap_us: int) -> tuple[str, str]:
    rx = """\
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
PR1T v=1 t_us=100 field=crc_good value=995
PR1T v=1 t_us=100 field=crc_bad value=1
PR1T v=1 t_us=100 field=missing value=5
PR1T v=1 t_us=100 field=queue_depth value=0
PR1T v=1 t_us=100 field=max_queue_depth value=1
PR1T v=1 t_us=100 field=irq_to_spi_us value=28
PR1T v=1 t_us=100 field=spi_duration_us value=61
PR1T v=1 t_us=100 field=rx_processing_us value=94
PR1T v=1 t_us=100 field=spi_end_to_rearm_start_us value=18
PR1T v=1 t_us=100 field=rx_rearm_us value=24
PR1T v=1 t_us=100 field=irq_to_rx_ready_us value=131
"""
    tx = f"""\
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
tx_gap_us={gap_us}
packet_bytes=116
adaptive_layers=off
PR1_RUNTIME_LIVE_READY
PR1T v=1 t_us=200 field=scheduler_misses value=0
"""
    return rx, tx


class BoardSweepContractTests(unittest.TestCase):
    def test_machine_readable_result_schemas_exist_and_match_csv_order(self):
        csv_schema = json.loads(
            (ROOT / "docs" / "schemas" / "pr1_board_sweep_results_csv.schema.json").read_text(
                encoding="utf-8"
            )
        )
        json_schema = json.loads(
            (ROOT / "docs" / "schemas" / "pr1_board_sweep_results.schema.json").read_text(
                encoding="utf-8"
            )
        )
        self.assertEqual([column["name"] for column in csv_schema["columns"]], RESULT_CSV_FIELDS)
        self.assertIn("runs", json_schema["required"])
        self.assertIn("analysis", json_schema["required"])
        self.assertIn("packet_count", json_schema["$defs"]["run_result"]["required"])
        self.assertIn("irq_to_rx_ready_us_p99", csv_schema["required_metrics"])

    def test_bottleneck_classifier_exposes_requested_categories(self):
        def metrics(**overrides):
            values = {
                "irq_to_spi_us_p99": 10,
                "spi_duration_us_p99": 10,
                "spi_end_to_rearm_start_us_p99": 10,
                "rx_rearm_us_p99": 10,
                "irq_to_rx_ready_us_p99": 100,
            }
            values.update(overrides)
            return values

        self.assertEqual(
            classify_bottleneck(metrics(irq_to_spi_us_p99=60), 0.01)["label"],
            "scheduler_wakeup",
        )
        self.assertEqual(
            classify_bottleneck(metrics(spi_duration_us_p99=60), 0.01)["label"],
            "spi",
        )
        self.assertEqual(
            classify_bottleneck(metrics(spi_end_to_rearm_start_us_p99=60), 0.01)["label"],
            "rx_processing",
        )
        self.assertEqual(
            classify_bottleneck(metrics(rx_rearm_us_p99=60), 0.01)["label"],
            "rearm",
        )

    def test_transition_can_add_receiver_saturation_candidate_without_claiming_causality(self):
        rows = [
            {
                "gap_us": 175,
                "packet_count": 1000,
                "metrics": {"irq_to_rx_ready_us_p99": 100, "rssi_dbm": -50},
                "derived": {"loss_rate": 0.002, "crc_bad_rate": 0.001},
                "classification": {"label": "rx_processing", "confidence": "low"},
            },
            {
                "gap_us": 150,
                "packet_count": 1000,
                "metrics": {"irq_to_rx_ready_us_p99": 160, "rssi_dbm": -51},
                "derived": {"loss_rate": 0.5, "crc_bad_rate": 0.002},
                "classification": {"label": "rx_processing", "confidence": "low"},
            },
        ]
        summary = analyze_sweep(rows)["bottleneck_summary"]
        self.assertIn("receiver_saturation_candidate", summary["candidates"])
        self.assertEqual(summary["confidence"], "medium")
        self.assertIn("not_causal_proof", summary["evidence_boundary"])

    def test_report_writes_human_summary(self):
        result = {
            "schema_version": 1,
            "run_id": "08-gap-175us",
            "phase": "baseline",
            "gap_us": 175,
            "target_packets": 1000,
            "firmware_sha": "abc123",
            "packet_count": 1000,
            "metrics": {
                "missing": 10,
                "crc_good": 990,
                "crc_bad": 1,
                "rssi_dbm": -50,
                "queue_depth": 0,
                "max_queue_depth": 1,
                "scheduler_misses": 0,
                "irq_to_spi_us_p99": 20,
                "spi_duration_us_p99": 20,
                "rx_processing_us_p99": 50,
                "spi_end_to_rearm_start_us_p99": 50,
                "rx_rearm_us_p99": 10,
                "irq_to_rx_ready_us_p99": 100,
                "trace_overwrites": 0,
            },
            "derived": {"loss_rate": 0.01, "crc_bad_rate": 0.001, "target_reached": True},
            "classification": {"label": "rx_processing", "confidence": "low", "dominant_share": 0.5},
            "evidence": {},
            "revalidate_10000": False,
        }
        with tempfile.TemporaryDirectory() as td:
            out = Path(td)
            write_sweep_outputs([result], out)
            summary = (out / "summary.md").read_text(encoding="utf-8")
            self.assertIn("PR1 board sweep summary", summary)
            self.assertIn("Completed runs: 1/11", summary)
            self.assertIn("10,000-packet revalidation", summary)
            self.assertIn("Evidence boundary", summary)

    def test_status_and_next_prepare_the_first_incomplete_run_without_touching_baseline(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td) / "sweep"
            project = Path(td) / "runtime"
            project.mkdir(parents=True)
            baseline = (
                "[env:rf_tx_compile]\n"
                "build_flags =\n"
                "    -D PR1_RF_ENABLED=1\n"
                "    -D PR1_RUNTIME_ROLE=1\n"
                "    -D PR1_TX_GAP_US=5000\n"
            )
            (project / "platformio.ini").write_text(baseline, encoding="utf-8")
            create_plan(root, "frozen-sha", target_packets=1000)

            status = experiment_status(root)
            self.assertEqual(status["next_gap_us"], 5000)
            self.assertEqual(status["completed_runs"], 0)
            self.assertEqual(status["pending_runs"], 11)

            prepared = prepare_next_run(root, project, tx_port="COM7")
            self.assertEqual(prepared["gap_us"], 5000)
            self.assertEqual(prepared["run_id"], "01-gap-5000us")
            self.assertEqual(prepared["tx_log"], str(root / "runs" / "01-gap-5000us" / "tx.log"))
            generated = Path(prepared["generated_config"])
            self.assertTrue(generated.exists())
            self.assertEqual((project / "platformio.ini").read_text(encoding="utf-8"), baseline)
            self.assertIn("--upload-port", prepared["flash_command"])
            self.assertEqual(prepared["flash_command"][-1], "COM7")

    def test_status_advances_after_valid_captured_logs_without_manual_analyze_step(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td) / "sweep"
            create_plan(root, "frozen-sha", target_packets=1000)
            run_dir = root / "runs" / "01-gap-5000us"
            rx, tx = captured_logs(5000)
            (run_dir / "rx.log").write_text(rx, encoding="utf-8")
            (run_dir / "tx.log").write_text(tx, encoding="utf-8")

            status = experiment_status(root)
            self.assertEqual(status["captured_runs"], 1)
            self.assertEqual(status["completed_runs"], 1)
            self.assertEqual(status["pending_runs"], 10)
            self.assertEqual(status["next_gap_us"], 1000)


if __name__ == "__main__":
    unittest.main()
