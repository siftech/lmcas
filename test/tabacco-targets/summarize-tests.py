#!/usr/bin/env python3
from pathlib import Path
from sys import exit
import os
import json

log_path = Path("/logs")

print("====================== Summary of TaBaCCo Target Results ======================")

summary = {
    "collected": 0,
    "passed": 0,
    "failed": 0,
    "deselected": 0,
    "xfailed": 0,
    "xpassed": 0,
    "error": 0,
    "skipped": 0,
    "total": 0,
}

# Accumulate log results into summary
for log in os.listdir(log_path):
    f = open(log_path / log)
    data = json.load(f)
    test_summary = data["summary"]
    if "collected" in test_summary:
        summary["collected"] += test_summary["collected"]
    if "passed" in test_summary:
        summary["passed"] += test_summary["passed"]
    if "failed" in test_summary:
        summary["failed"] += test_summary["failed"]
    if "deselected" in test_summary:
        summary["deselected"] += test_summary["deselected"]
    if "xfailed" in test_summary:
        summary["xfailed"] += test_summary["xfailed"]
    if "xpassed" in test_summary:
        summary["xpassed"] += test_summary["xpassed"]
    if "error" in test_summary:
        summary["error"] += test_summary["error"]
    if "skipped" in test_summary:
        summary["skipped"] += test_summary["skipped"]
    if "total" in test_summary:
        summary["total"] += test_summary["total"]

print("Total: " + str(summary["total"]))
print("Collected: " + str(summary["collected"]))
print("Passed: " + str(summary["passed"]))
print("Failed: " + str(summary["failed"]))
print("Deselected: " + str(summary["deselected"]))
print("Skipped: " + str(summary["skipped"]))
print("Error: " + str(summary["error"]))
print("===============================================================================")

if (summary["failed"] != 0) or (summary["error"] != 0):
    exit(1)
