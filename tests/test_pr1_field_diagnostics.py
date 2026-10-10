"""Host-only regression tests for PR1 field evidence (no boards or external packages)."""
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from pr1_field_diagnostics import analyze_run, compare_runs  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]


class FieldDiagnosticsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.dir = Path(self.temp.name)

    def run_dir(self, name='body', summary=None, log=''):
        target = self.dir / name
        target.mkdir()
        (target / 'summary.json').write_text(json.dumps(summary or {
            'label': name, 'window_s': 94.1, 'crc_good': 28054,
            'crc_bad': 1945, 'missing': 3105, 'rssi_dbm': -82,
            'loss_pct': 9.965, 'concealed_pct': 4.052,
            'resets': 0, 'i2s_errors': 0, 'queue_dropped': 0,
        }), encoding='utf-8')
        if log:
            (target / 'rx_pull.log').write_text(log, encoding='utf-8')
        return target

    def test_preserves_distinct_crc_and_missing_denominators(self):
        row = analyze_run(self.run_dir())
        self.assertAlmostEqual(row['rf_loss_pct'], 100 * 3105 / (28054 + 3105), places=4)
        self.assertAlmostEqual(row['crc_bad_among_decoded_pct'], 100 * 1945 / (28054 + 1945), places=4)
        self.assertNotEqual(row['rf_loss_pct'], row['crc_bad_among_decoded_pct'])

    def test_last_packet_rssi_is_not_distribution(self):
        row = analyze_run(self.run_dir())
        self.assertEqual(row['last_good_packet_rssi_dbm'], -82)
        self.assertNotIn('rssi_median_dbm', row)
        self.assertIn('last successful packet only', row['limitations'])

    def test_p99_metrics_are_recent_64_only(self):
        log = 'PR1T v=1 t_us=3 field=irq_to_rx_ready_us value=1789\nPR1T v=1 t_us=3 field=trace_overwrites value=270023\n'
        row = analyze_run(self.run_dir(log=log))
        self.assertEqual(row['recent_64_p99_us']['irq_to_rx_ready_us'], 1789)
        self.assertEqual(row['trace_ring_overwrites'], 270023)
        self.assertIn('recent 64', row['limitations'])

    def test_nested_audio_run_is_supported_without_claiming_missing_metrics(self):
        nested = {'label': 'nested', 'rf': {'crc_good': 100, 'crc_bad': 2,
                  'missing': 5, 'rssi_dbm': -61}, 'audio': {'missing_blocks': 1}}
        row = analyze_run(self.run_dir(summary=nested))
        self.assertAlmostEqual(row['rf_loss_pct'], 100 * 5 / 105)
        self.assertIsNone(row['concealed_pct'])
        self.assertIsNone(row['recent_64_p99_us']['irq_to_rx_ready_us'])

    def test_missing_and_corrupt_counts_fail_closed(self):
        bad = self.run_dir(summary={'label':'bad', 'crc_good': 12, 'missing': -1, 'crc_bad': 0})
        with self.assertRaises(ValueError):
            analyze_run(bad)
        invalid = self.run_dir(name='invalid', summary={'label':'x','crc_good':0,'crc_bad':0})
        with self.assertRaises(ValueError):
            analyze_run(invalid)

    def test_recorded_summary_discrepancy_is_flagged(self):
        data = {'label':'x','crc_good':100,'crc_bad':3,'missing':5,'loss_pct':1.0}
        row = analyze_run(self.run_dir(summary=data))
        self.assertTrue(any('differs' in x for x in row['warnings']))

    def test_compare_is_descriptive_not_causal(self):
        first = analyze_run(self.run_dir(name='los', summary={
            'label':'los', 'crc_good':30855, 'crc_bad':207, 'missing':295,
            'rssi_dbm':-71, 'concealed_pct':0.059}))
        second = analyze_run(self.run_dir(name='body'))
        diff = compare_runs(first, second)
        self.assertAlmostEqual(diff['rf_loss_change_pp'], second['rf_loss_pct'] - first['rf_loss_pct'])
        self.assertEqual(diff['causal_inference'], 'not established by these logs')

    def test_cli_json_and_md_no_raw_input_modification(self):
        run = self.run_dir(log='PR1T v=1 t_us=3 field=rssi_dbm value=-82\n')
        initial = (run / 'summary.json').read_bytes()
        for format_name in ('json', 'md'):
            cmd = [sys.executable, str(ROOT / 'tools/pr1_field_diagnostics.py'), str(run), '--format', format_name]
            result = subprocess.run(cmd, capture_output=True, text=True, check=False)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('body', result.stdout)
        self.assertEqual(initial, (run / 'summary.json').read_bytes())


if __name__ == '__main__':
    unittest.main()
