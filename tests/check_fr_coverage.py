#!/usr/bin/env python3
"""Verify terminal-free test mappings for FR-01 through FR-22."""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TEST_FILE = ROOT / "tests" / "run_tests.sh"
SIGNAL_FILE = ROOT / "tests" / "signal_test.py"

content = TEST_FILE.read_text(encoding="utf-8")
signal_content = SIGNAL_FILE.read_text(encoding="utf-8")

coverage = {f"FR-{number:02d}": set() for number in range(1, 23)}

# Recognize both t TC-xx FR-xx and record TC-xx FR-xx.
pattern = re.compile(
    r'\b(?:t|record)\s+["\']?(TC-\d+)["\']?\s+'
    r'["\']?((?:FR-\d{2}(?:,FR-\d{2})*|NFR-\d{2}))["\']?'
)

for match in pattern.finditer(content):
    test_id, requirements = match.groups()

    for requirement in requirements.split(","):
        if requirement in coverage:
            coverage[requirement].add(test_id)

# Include the dedicated terminal-free signal test.
for match in re.finditer(r"TC-(\d+)\s+(FR-\d{2})", signal_content):
    test_id, requirement = match.groups()
    if requirement in coverage:
        coverage[requirement].add(f"TC-{test_id}")

missing = []

print("TERMINAL-FREE FUNCTIONAL REQUIREMENT COVERAGE")
print("-" * 55)

for requirement, test_ids in coverage.items():
    if test_ids:
        print(f"{requirement}: PASS - {', '.join(sorted(test_ids))}")
    else:
        print(f"{requirement}: MISSING")
        missing.append(requirement)

print("-" * 55)
print(f"Covered: {22 - len(missing)}/22")
print(f"Missing: {len(missing)}")

if missing:
    print("Missing requirements:", ", ".join(missing))
    sys.exit(1)

print("All functional requirements have terminal-free test mappings.")
sys.exit(0)
