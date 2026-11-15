#!/usr/bin/env python3
"""Self-test of tools/check_site.py (run by make test).

Builds a site like make web does in a temporary directory (web/, the
committed media, LICENSE.txt, and stand-in downloads: a blank 35-track
D64 and a 3-byte PRG loading at $0801), checks that check_site.py accepts
it, then breaks one thing at a time and checks that each is rejected
(exit status nonzero) with the expected message.
"""
import os
import shutil
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CHECK = os.path.join(ROOT, "tools/check_site.py")


def build(d):
    shutil.copy(os.path.join(ROOT, "web/index.html"), d)
    shutil.copy(os.path.join(ROOT, "web/style.css"), d)
    shutil.copy(os.path.join(ROOT, "LICENSE"), os.path.join(d, "LICENSE.txt"))
    shutil.copytree(os.path.join(ROOT, "docs/img/c64"), os.path.join(d, "img"))
    os.makedirs(os.path.join(d, "dl"))
    open(os.path.join(d, "dl/pl0.d64"), "wb").write(bytes(174848))
    open(os.path.join(d, "dl/pl0.prg"), "wb").write(b"\x01\x08\x00")


def edit(old, new):
    def f(d):
        p = os.path.join(d, "index.html")
        s = open(p).read()
        assert old in s, f"test setup: {old!r} not in the page"
        open(p, "w").write(s.replace(old, new))
    return f


def write(rel, data):
    def f(d):
        open(os.path.join(d, rel), "wb").write(data)
    return f


def remove(rel):
    return lambda d: os.remove(os.path.join(d, rel))


CASES = [
    ("empty page", write("index.html", b"<!doctype html><title>x</title>"),
     "no reference to img/demo.gif"),
    ("missing gallery image reference",
     edit('<a href="img/koch.png"><img src="img/koch.png"', '<a href="img/tree.png"><img src="img/tree.png"'),
     "no reference to img/koch.png"),
    ("missing download link", edit('href="dl/pl0.prg"', 'href="dl/pl0.d64"'),
     "no reference to dl/pl0.prg"),
    ("download typo", edit('href="dl/pl0.prg"', 'href="dl/pl0.prog"'), "no reference to dl/pl0.prg"),
    ("broken href", edit('href="#examples"', 'href="#nope"'), "#nope is not an id"),
    ("missing local file", remove("img/star.png"), "img/star.png' does not exist"),
    ("external URL", edit('<link rel="stylesheet" href="style.css">',
                          '<link rel="stylesheet" href="style.css">'
                          '<link rel="stylesheet" href="https://cdn.example.com/x.css">'),
     "external resource"),
    ("leading-slash link", edit('href="style.css"', 'href="/style.css"'), "starts with '/'"),
    ("oversize GIF", write("img/demo.gif", b"GIF89a" + bytes(2_600_000)), "over the .gif cap"),
    ("oversize PNG", write("img/koch.png", b"\x89PNG\r\n\x1a\n" + bytes(310_000)), "over the .png cap"),
    ("script", edit("</body>", "<script>1</script></body>"), "<script>"),
    ("srcset", edit('<img src="img/demo.gif"', '<img srcset="img/demo.gif 2x" src="img/demo.gif"'),
     "srcset"),
    ("external url() in a <style> block", edit("</head>", "<style>body{background:url(https://x.example/a.png)}</style></head>"),
     "external url("),
    ("missing url() in a style= attribute", edit("<main id=\"main\">", "<main id=\"main\" style=\"background:url(img/nope.png)\">"),
     "img/nope.png' does not exist"),
    ("no repository link", edit("data-repo-path=", "data-x="), "no repository link"),
    ("repository link to a missing document",
     edit('data-repo-path="docs/LANGUAGE.md"', 'data-repo-path="docs/LANGUAGE2.md"'),
     "is not a file of the repository"),
]


def run(d):
    r = subprocess.run([sys.executable, CHECK, d], capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def main():
    fail = 0
    top = tempfile.mkdtemp(prefix="check_site_test_")
    try:
        base = os.path.join(top, "ok")
        os.makedirs(base); build(base)
        rc, out = run(base)
        if rc != 0:
            print("test_check_site: FAIL the good site is rejected:\n" + out); fail += 1
        for i, (name, breakit, msg) in enumerate(CASES):
            d = os.path.join(top, f"case{i}")
            shutil.copytree(base, d)
            breakit(d)
            rc, out = run(d)
            if rc == 0 or msg not in out:
                print(f"test_check_site: FAIL {name}: exit {rc}, expected {msg!r} in:\n{out[-1500:]}")
                fail += 1
        # source mode (make test, before make web copies dl/): a typo in
        # a download name fails on the name alone
        sys.path.insert(0, os.path.dirname(CHECK))
        import check_site
        d = os.path.join(top, "source")
        os.makedirs(d)
        shutil.copy(os.path.join(ROOT, "web/style.css"), d)
        page = open(os.path.join(ROOT, "web/index.html")).read()
        open(os.path.join(d, "index.html"), "w").write(page.replace('href="dl/pl0.prg"', 'href="dl/pl0.prog"'))
        site = check_site.Site(True); site.base = d
        problems, _, _ = check_site.check(site, check_site.required())
        if not any("dl/ holds only" in p for p in problems):
            print(f"test_check_site: FAIL source-mode download typo not caught: {problems}"); fail += 1
        site.base = os.path.join(ROOT, "web")
        problems, _, _ = check_site.check(site, check_site.required())
        if problems:
            print(f"test_check_site: FAIL the committed sources are rejected: {problems}"); fail += 1
    finally:
        shutil.rmtree(top, ignore_errors=True)
    if fail:
        sys.exit(f"test_check_site: {fail} case(s) failed")
    print(f"test_check_site: the good site passes, {len(CASES) + 1} broken sites rejected")


if __name__ == "__main__":
    main()
