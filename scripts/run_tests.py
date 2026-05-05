#!/usr/bin/env python3
"""Run all test suites on the target."""

from _helpers import ok, run_cijoe

run_cijoe("tasks/test.yaml", out="cijoe-output-test")
ok("run_tests")
