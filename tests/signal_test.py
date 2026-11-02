#!/usr/bin/env python3
"""Terminal-free signal handling test for FR-21."""

import os
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MYSH = Path(os.environ.get("MYSH", ROOT / "mysh")).resolve()


def test_foreground_sigint():
    with tempfile.TemporaryDirectory() as temp:
        script = Path(temp) / "signal_test.mysh"
        script.write_text("sleep 10\necho survived\n")

        process = subprocess.Popen(
            [str(MYSH), str(script)],
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            start_new_session=True,
        )

        try:
            child_pid = None
            deadline = time.monotonic() + 5

            while time.monotonic() < deadline:
                result = subprocess.run(
                    ["pgrep", "-P", str(process.pid)],
                    capture_output=True,
                    text=True,
                    check=False,
                )

                if result.stdout.strip():
                    child_pid = int(result.stdout.strip().splitlines()[0])
                    break

                if process.poll() is not None:
                    break

                time.sleep(0.05)

            if child_pid is None:
                raise AssertionError(
                    "Could not find the foreground child process"
                )

            os.kill(child_pid, signal.SIGINT)
            stdout, stderr = process.communicate(timeout=5)

            if process.returncode != 0:
                raise AssertionError(
                    f"Shell exited with status {process.returncode}; "
                    f"stderr: {stderr}"
                )

            if "survived" not in stdout:
                raise AssertionError(
                    f"Shell did not continue after SIGINT; stdout: {stdout}"
                )

        finally:
            if process.poll() is None:
                process.kill()
                process.communicate()

    print(
        "TC-90 FR-21 PASS "
        "foreground SIGINT terminates child but shell survives"
    )


if __name__ == "__main__":
    try:
        test_foreground_sigint()
    except Exception as error:
        print(f"TC-90 FR-21 FAIL {error}")
        sys.exit(1)
