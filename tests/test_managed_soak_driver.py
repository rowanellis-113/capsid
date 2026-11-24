import contextlib
import io
import json
import subprocess
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().with_name("soak")))
import run_managed_soak as driver


class SoakDriverTests(unittest.TestCase):
    def setUp(self):
        args = SimpleNamespace(memory_waves="probe", worker="worker", fixture="fixture")
        self.soak = driver.Soak(args, None, 0, 0)
        sample = {"used": 1024, "objects": 10, "properties": 8}
        self.report = {
            "converged": True,
            "reason": "",
            "baseline": dict(sample, atoms=3),
            "waves": [sample.copy() for _ in range(6)],
        }

    def run_wave(self, report, code=0):
        child = subprocess.CompletedProcess([], code, json.dumps(report), "probe diagnostic")
        with patch.object(driver.subprocess, "run", return_value=child):
            self.soak.dim_memory_waves()

    def test_complete_measurement_is_recorded(self):
        self.run_wave(self.report)
        self.assertEqual(self.soak.memory_waves, [self.report])
        self.assertEqual(self.soak.counts, {"memory_waves": 1})

    def test_bad_exit_or_incomplete_evidence_is_never_counted(self):
        for report, code in [
            (self.report, 9),
            ({"converged": True}, 0),
            (dict(self.report, converged="true"), 0),
            (dict(self.report, waves=self.report["waves"][:-1]), 0),
            (dict(self.report, baseline={}), 0),
            (
                dict(
                    self.report,
                    waves=self.report["waves"][:-1] + [dict(self.report["waves"][0], used=True)],
                ),
                0,
            ),
        ]:
            with self.subTest(report=report, code=code), contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaises(SystemExit) as error:
                    self.run_wave(report, code)
                self.assertNotEqual(error.exception.code, 0)
                self.assertEqual(self.soak.memory_waves, [])
                self.assertEqual(self.soak.counts, {})

    def test_duration_must_be_positive_before_startup(self):
        for minutes in ("0", "-1"):
            args = [
                "soak",
                "--host",
                "host",
                "--worker",
                "worker",
                "--fixture",
                "app",
                "--memory-waves",
                "probe",
                "--work-dir",
                ".",
                "--listen-port",
                "1234",
                "--minutes",
                minutes,
            ]
            with self.subTest(minutes=minutes), patch.object(sys, "argv", args):
                with (
                    contextlib.redirect_stderr(io.StringIO()) as err,
                    self.assertRaises(SystemExit) as caught,
                ):
                    driver.main()
                self.assertEqual(caught.exception.code, 2)
                self.assertIn("--minutes must be positive", err.getvalue())


if __name__ == "__main__":
    unittest.main()
