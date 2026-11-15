#!/usr/bin/env python3
"""Self-test of tools/check_error_docs.py (run by `make test`): a complete
document passes; a missing message, a message only as plain text, a
message only inside a longer code span, a missing table and a bad
argument fail."""
import os
import subprocess
import sys
import tempfile

here = os.path.dirname(os.path.abspath(__file__))
tool = os.path.join(here, "check_error_docs.py")
fails = 0

ERRORS_C = r'''
static const char *const err_msgs[3] = {
    /* E_NONE */ "ok",
    /* E_A */    "\";\" expected",
    /* E_B */    "stopped"
};
'''


def run(errors_text, doc_text, extra=None):
    with tempfile.TemporaryDirectory() as d:
        e = os.path.join(d, "errors.c")
        m = os.path.join(d, "LANGUAGE.md")
        with open(e, "w") as f:
            f.write(errors_text)
        if doc_text is not None:
            with open(m, "w") as f:
                f.write(doc_text)
        argv = extra if extra is not None else ["--errors", e, "--doc", m]
        p = subprocess.run([sys.executable, tool] + argv, capture_output=True, text=True)
        return p.returncode, p.stdout


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("test_check_error_docs: FAIL: " + what)


good = "| `ok` | x |\n| `\";\" expected` | y |\n\nThe program `stopped`.\n"
rc, out = run(ERRORS_C, good)
check(rc == 0 and "3 messages" in out, "complete document rejected: %s" % out)

rc, out = run(ERRORS_C, "| `ok` |\n| `\";\" expected` |\n")
check(rc == 1 and "stopped" in out, "missing message accepted")

rc, out = run(ERRORS_C, "`ok` `\";\" expected` and the program stopped.\n")
check(rc == 1 and "stopped" in out, "message as plain text accepted")

rc, out = run(ERRORS_C, "`ok` `\";\" expected` `stopped early`\n")
check(rc == 1, "message inside a longer code span accepted")

rc, out = run("int x;\n", good)
check(rc == 2, "missing table not reported")

rc, out = run(ERRORS_C, good, extra=["--bogus"])
check(rc == 2, "bad argument accepted")

rc, out = run(ERRORS_C, None)
check(rc == 0 and "skipped" in out, "missing document not skipped")

# the real files pass
p = subprocess.run([sys.executable, tool], capture_output=True, text=True)
check(p.returncode == 0, "the repository's docs/LANGUAGE.md fails: " + p.stdout)

if fails:
    print("test_check_error_docs: %d failure(s)" % fails)
    sys.exit(1)
print("test_check_error_docs: all checks passed")
