#!/usr/bin/env python3
"""Check that every error message in src/core/errors.c appears in docs/LANGUAGE.md.

Parsing: the messages are the C string literals inside the err_msgs[] table
(from the line containing 'err_msgs[' up to the closing '};'). Each literal is
matched with a regex and unescaped (\\" and \\\\). Comments are ignored.
"""
import os, re, sys

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
doc = os.path.join(root, "docs", "LANGUAGE.md")
if not os.path.exists(doc):
    print("skipped: docs/LANGUAGE.md not found")
    sys.exit(0)

src = open(os.path.join(root, "src", "core", "errors.c")).read()
m = re.search(r"err_msgs\[[^\]]*\]\s*=\s*\{(.*?)\};", src, re.S)
if not m:
    print("error: message table not found in errors.c")
    sys.exit(2)
body = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
msgs = [re.sub(r'\\(["\\])', r"\1", s)
        for s in re.findall(r'"((?:[^"\\]|\\.)*)"', body)]
text = open(doc).read()
missing = [s for s in msgs if s not in text]
if missing:
    print("error messages missing from docs/LANGUAGE.md:")
    for s in missing:
        print("  " + s)
    sys.exit(1)
print("error docs ok (%d messages)" % len(msgs))
