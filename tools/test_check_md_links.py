#!/usr/bin/env python3
"""Self-test of tools/check_md_links.py (run by `make test`): a small tree
of good links passes; each kind of broken link fails, and links the
checker must ignore are ignored."""
import os
import subprocess
import sys
import tempfile

here = os.path.dirname(os.path.abspath(__file__))
tool = os.path.join(here, "check_md_links.py")
fails = 0
F = "```"


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("test_check_md_links: FAIL: " + what)


def run(files):
    """files: {relative path: text}; returns (rc, output) for the tree."""
    with tempfile.TemporaryDirectory() as d:
        for rel, text in files.items():
            p = os.path.join(d, rel)
            os.makedirs(os.path.dirname(p), exist_ok=True)
            with open(p, "w") as f:
                f.write(text)
        r = subprocess.run([sys.executable, tool, "--root", d], capture_output=True, text=True)
        return r.returncode, r.stdout


BASE = {
    "img/pic.png": "x",
    "docs/guide.md": "\n".join([
        "# User guide", "",
        "## Keys and `F1`", "",
        "## Keys and `F1`", "",              # repeated heading: -1
        "## 1. Intent, and why?", "",
        '<a id="custom"></a>', "",
        "[up](../README.md#the-readme)",
    ]),
}
GOOD = dict(BASE)
GOOD["README.md"] = "\n".join([
    "# The README", "",
    "![pic](img/pic.png) [guide](docs/guide.md) [dir](docs/)",
    "[keys](docs/guide.md#keys-and-f1) [again](docs/guide.md#keys-and-f1-1)",
    "[intent](docs/guide.md#1-intent-and-why) [custom](docs/guide.md#custom)",
    "[self](#the-readme) [titled](docs/guide.md \"Guide\") [root](/img/pic.png)",
    '<img src="img/pic.png" alt="a" width="10"> <a href="docs/guide.md">g</a>',
    "[ref]: docs/guide.md",
    "[web](https://example.com/missing) [mail](mailto:a@b.c)",
    "[placeholder](https://github.com/OWNER/REPO/blob/main/x.md)",
    "`[code](missing.md)` and:", "",
    F, "[in a fence](missing.md)", F,
])
rc, out = run(GOOD)
check(rc == 0 and "14 relative links in 2 files" in out, "good tree rejected: " + out)

broken = [
    ("missing file", "[x](docs/nothere.md)", "broken link"),
    ("missing image", "![x](img/none.png)", "broken link"),
    ("missing html image", '<img src="img/none.png">', "broken link"),
    ("missing anchor", "[x](docs/guide.md#nothing)", "broken anchor"),
    ("missing own anchor", "[x](#nothing)", "broken anchor"),
    ("third repeat not present", "[x](docs/guide.md#keys-and-f1-2)", "broken anchor"),
    ("missing reference target", "[r]: docs/none.md", "broken link"),
    ("wrong relative depth", "[x](../docs/guide.md)", "broken link"),
]
for name, line, msg in broken:
    tree = dict(BASE)
    tree["README.md"] = "# The README\n\n" + line + "\n"
    rc, out = run(tree)
    check(rc == 1 and msg in out, "%s: rc %d, %r" % (name, rc, out))

rc, out = run({"README.md": "# Nothing\n\n[web](https://example.com)\n"})
check(rc == 1 and "nothing was checked" in out, "tree without relative links accepted")

if fails:
    print("test_check_md_links: %d failure(s)" % fails)
    sys.exit(1)
print("test_check_md_links: all checks passed")
