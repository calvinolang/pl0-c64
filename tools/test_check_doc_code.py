#!/usr/bin/env python3
"""Self-test of tools/check_doc_code.py (run by `make test`, needs
build/pl0host): good blocks of every kind pass; each kind of mistake
fails with its own message."""
import os
import subprocess
import sys
import tempfile

here = os.path.dirname(os.path.abspath(__file__))
tool = os.path.join(here, "check_doc_code.py")
host = os.path.join(os.path.dirname(here), "build", "pl0host")
fails = 0
F = "```"


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("test_check_doc_code: FAIL: " + what)


def run(md, argv=None):
    with tempfile.TemporaryDirectory() as d:
        p = os.path.join(d, "doc.md")
        with open(p, "w") as f:
            f.write(md)
        r = subprocess.run([sys.executable, tool, "--host", host, "--timeout", "1"] + (argv or [p]),
                           capture_output=True, text=True)
        return r.returncode, r.stdout


GOOD = "\n".join([
    "# Doc", "",
    F + "pl0", "var x; ?x.", F, "",                      # runtime error: fine
    F + "pl0", "var i; while 1 = 1 do i := i + 1.", F, "",   # loops: fine
    F + "pl0 run", "begin !6*7; ! end.", F, "",
    F + "output", "42", F, "",
    F + "pl0 error:undeclared name", "!x.", F, "",
    F + "pl0 error:ERROR line 2: division by zero", "begin", "!1/0 end.", F, "",
    F + "text", "not PL/0, not checked {", F, "",
    "  " + F + "pl0 run", "  !1.", "  " + F, "",           # indented (in a list)
    F + "pl0 example:square",
] + open(os.path.join(os.path.dirname(here), "examples", "square.pl0")).read().rstrip("\n").split("\n") + [
    F, "",
])
rc, out = run(GOOD)
check(rc == 0 and "7 PL/0 blocks" in out, "good document rejected: " + out)

cases = [
    ("compile error in a pl0 block", F + "pl0\nbegin !1 !2 end.\n" + F + "\n", "does not compile"),
    ("runtime error in a pl0 run block", F + "pl0 run\n!1/0.\n" + F + "\n", "does not run cleanly"),
    ("endless pl0 run block", F + "pl0 run\nwhile 1 = 1 do .\n" + F + "\n", "still running"),
    ("wrong output", F + "pl0 run\n!7.\n" + F + "\n\n" + F + "output\n8\n" + F + "\n", "output differs"),
    ("output block alone", F + "output\n8\n" + F + "\n", "without a `pl0 run`"),
    ("output after text between", F + "pl0 run\n!7.\n" + F + "\nSome words.\n\n" + F + "output\n7\n" + F + "\n",
     "without a `pl0 run`"),
    ("wrong error message", F + "pl0 error:division by zero\n!x.\n" + F + "\n", "expected `division by zero`"),
    ("wrong error line", F + "pl0 error:ERROR line 2: undeclared name\n!x.\n" + F + "\n", "got `ERROR line 1"),
    ("error expected, none", F + "pl0 error:stopped\n!1.\n" + F + "\n", "got no error"),
    ("empty error marker", F + "pl0 error:\n!1.\n" + F + "\n", "without a message"),
    ("unknown marker", F + "pl0 runs\n!1.\n" + F + "\n", "unknown marker"),
    ("example copy that differs", F + "pl0 example:square\nforward(10).\n" + F + "\n", "differs from examples/square.pl0"),
    ("example that does not exist", F + "pl0 example:nothere\nforward(10).\n" + F + "\n", "no example"),
    ("unterminated fence", F + "pl0\n!1.\n", "unterminated"),
    ("no pl0 blocks at all", "# nothing\n", "nothing was checked"),
]
for name, md, msg in cases:
    rc, out = run(md)
    check(rc == 1 and msg in out, "%s: rc %d, %r" % (name, rc, out))

rc, out = run(GOOD, ["--host", "/nonexistent/pl0host"])
check(rc == 2, "missing host accepted")

if fails:
    print("test_check_doc_code: %d failure(s)" % fails)
    sys.exit(1)
print("test_check_doc_code: all checks passed")
