#!/usr/bin/env python3
"""Self-test of tools/vice_screen.py's argument checks and screen matching
(no VICE needed; run by `make test`)."""
import os
import subprocess
import sys

sys.dont_write_bytecode = True
here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, here)
from vice_screen import match, parse_args  # noqa: E402

fails = 0


def check(ok, what):
    global fails
    if not ok:
        fails += 1
        print("test_vice_screen: FAIL: " + what)


def exits(argv):
    try:
        parse_args(argv)
    except SystemExit as e:
        return e.code not in (None, 0)
    return False


# empty or blank expected text, empty fail text: usage error, not a vacuous pass
check(exits(["--out", "d", "p.prg", "", "1000"]), "empty EXPECTED accepted")
check(exits(["--out", "d", "p.prg", "  ", "1000"]), "blank EXPECTED accepted")
check(exits(["--out", "d", "p.prg", "PASS", "1000", ""]), "empty FAILTEXT accepted")
check(not exits(["--out", "d", "p.prg", "PASS", "1000", "FAIL"]), "valid arguments rejected")
# the same through the command line: exits nonzero before starting VICE
r = subprocess.run([sys.executable, os.path.join(here, "vice_screen.py"), "--out", "d", "p.prg", "", "1000"],
                   capture_output=True)
check(r.returncode != 0, "command line with empty EXPECTED did not fail")

screen = "hello\nPASS   \n\nFAIL 3\n"
check(match("a\nPASS  \nb\n", "PASS", None) == 0, "whole line (trailing spaces) not matched")
check(match("a\nno PASS here\nPASSED\n", "PASS", None) is None, "substring matched")
check(match("a\n\nb\n", "PASS", "FAIL") is None, "blank lines matched")
check(match(screen, "PASS", "FAIL") == 1, "fail text not checked first")
check(match("FAIL 3\n", "PASS", "FAIL") == 1, "fail text is not a line prefix")
check(match("x FAIL\nPASS\n", "PASS", "FAIL") == 0, "fail text matched inside a line")

if fails:
    sys.exit(1)
print("test_vice_screen: all checks passed")
