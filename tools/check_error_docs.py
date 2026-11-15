#!/usr/bin/env python3
"""Check that every error message in src/core/errors.c is documented.

Usage: check_error_docs.py [--errors errors.c] [--doc LANGUAGE.md]
       (defaults: src/core/errors.c and docs/LANGUAGE.md)

Parsing: the messages are the C string literals inside the err_msgs[] table
(from 'err_msgs[' up to the closing '};'). Each literal is unescaped (\\"
and \\\\); comments are ignored. Each message must appear in the document
as a code span of its own, exactly: `undeclared name`, `";" expected`
(a plain substring match would let a short message such as "stopped" or
"ok" pass on any page that happens to contain the word).

Exit 0 ok, 1 a message is missing (each printed), 2 when a file or the
table cannot be read. If the document does not exist the check is
skipped (exit 0), as before docs/LANGUAGE.md was written.
"""
import os
import re
import sys

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def messages(src):
    m = re.search(r"err_msgs\[[^\]]*\]\s*=\s*\{(.*?)\};", src, re.S)
    if not m:
        return None
    body = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
    return [re.sub(r'\\(["\\])', r"\1", s)
            for s in re.findall(r'"((?:[^"\\]|\\.)*)"', body)]


def code_spans(text):
    """Contents of the `single-backtick` code spans of a Markdown text."""
    return set(s.strip() for s in re.findall(r"(?<!`)`([^`\n]+)`(?!`)", text))


def main(argv):
    errors = os.path.join(root, "src", "core", "errors.c")
    doc = os.path.join(root, "docs", "LANGUAGE.md")
    i = 0
    while i < len(argv):
        if argv[i] in ("--errors", "--doc") and i + 1 < len(argv):
            if argv[i] == "--errors":
                errors = argv[i + 1]
            else:
                doc = argv[i + 1]
            i += 2
        else:
            print("usage: check_error_docs.py [--errors FILE] [--doc FILE]")
            return 2
    if not os.path.exists(doc):
        print("skipped: %s not found" % doc)
        return 0
    try:
        with open(errors) as f:
            src = f.read()
        with open(doc, encoding="utf-8") as f:
            text = f.read()
    except OSError as e:
        print("error: %s" % e)
        return 2
    msgs = messages(src)
    if not msgs:
        print("error: message table not found in %s" % errors)
        return 2
    spans = code_spans(text)
    missing = [s for s in msgs if s not in spans]
    if missing:
        print("error messages missing from %s (each as a `code span`):" % os.path.basename(doc))
        for s in missing:
            print("  " + s)
        return 1
    print("error docs ok (%d messages)" % len(msgs))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
