#!/usr/bin/env python3
"""Check the relative links and images of the Markdown files.

Usage: check_md_links.py [--root DIR] [FILE.md ...]
       (default: every *.md under the repository root, except build/,
        .git/, .worktrees/ and .superpowers/)

Checked, outside fenced code blocks and `code spans`:
  - inline links and images: [text](target), ![alt](target "title");
  - reference definitions: [label]: target;
  - HTML in Markdown: <img src="...">, <a href="...">, <source srcset>.
A relative target must exist (file or directory, relative to the file
that links it; a leading / means the repository root). A #fragment must
name a heading of the target Markdown file (GitHub's anchors: lower case,
punctuation dropped, spaces as -, -1, -2 ... for repeated headings) or an
explicit <a id="..."> / <a name="..."> in it; a link to a fragment of a
non-Markdown file is not checked beyond the file.
Not checked: http(s), mailto and other URL schemes, and links that
contain the repository placeholder OWNER/REPO (filled in when the
repository exists).

Exit 0 when every link resolves (the number of links checked is
printed), 1 when one does not (each printed as FILE:LINE) or when no
relative link was found at all, 2 on usage.
"""
import os
import re
import sys
import unicodedata
from urllib.parse import unquote

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKIP_DIRS = {"build", ".git", ".worktrees", ".superpowers", "node_modules"}
PLACEHOLDER = "OWNER/REPO"
FENCE = re.compile(r"^ {0,3}(`{3,}|~{3,})")
INLINE = re.compile(r"!?\[(?:[^\[\]]|\[[^\[\]]*\])*\]\(\s*(<[^>]*>|[^)\s]*)(?:\s+\"[^\"]*\"|\s+'[^']*')?\s*\)")
REFDEF = re.compile(r"^ {0,3}\[[^\]]+\]:\s*(<[^>]*>|\S+)")
HTMLATTR = re.compile(r"<(?:img|a|source)\b[^>]*?\b(?:src|href|srcset)\s*=\s*\"([^\"]*)\"", re.I)
HEADING = re.compile(r"^ {0,3}(#{1,6})\s+(.*?)\s*#*\s*$")
EXPLICIT = re.compile(r"<a\s+[^>]*\b(?:id|name)\s*=\s*\"([^\"]+)\"", re.I)


def md_files(root):
    out = []
    for d, dirs, files in os.walk(root):
        dirs[:] = sorted(x for x in dirs if x not in SKIP_DIRS)
        for f in sorted(files):
            if f.endswith(".md"):
                out.append(os.path.join(d, f))
    return out


def strip_code_spans(line):
    return re.sub(r"(`+)(.+?)\1", lambda m: " " * len(m.group(0)), line)


def text_lines(path):
    """(line number, text) of the lines outside fenced code blocks."""
    with open(path, encoding="utf-8") as f:
        lines = f.read().split("\n")
    fence = None
    for n, l in enumerate(lines, 1):
        m = FENCE.match(l)
        if fence:
            if m and m.group(1)[0] == fence[0] and len(m.group(1)) >= len(fence) \
                    and not l.strip()[len(m.group(1)):].strip():
                fence = None
            continue
        if m:
            fence = m.group(1)
            continue
        yield n, l


def slug(heading):
    """GitHub's anchor for a heading text."""
    t = re.sub(r"!?\[([^\]]*)\]\([^)]*\)", r"\1", heading)   # links: their text
    t = re.sub(r"<[^>]+>", "", t)                             # inline HTML
    t = t.replace("`", "")
    t = t.strip().lower()
    out = []
    for ch in t:
        cat = unicodedata.category(ch)
        if ch in "-_" or cat[0] in "LN":
            out.append(ch)
        elif ch == " ":
            out.append("-")
    return "".join(out)


_anchor_cache = {}


def anchors(path):
    if path not in _anchor_cache:
        seen = {}
        names = set()
        for _, l in text_lines(path):
            m = HEADING.match(l)
            if m:
                s = slug(m.group(2))
                k = seen.get(s, 0)
                names.add(s if k == 0 else "%s-%d" % (s, k))
                seen[s] = k + 1
            for a in EXPLICIT.findall(l):
                names.add(a)
        _anchor_cache[path] = names
    return _anchor_cache[path]


def targets(path):
    for n, l in text_lines(path):
        bare = strip_code_spans(l)
        for m in INLINE.finditer(bare):
            yield n, m.group(1).strip("<>")
        m = REFDEF.match(bare)
        if m:
            yield n, m.group(1).strip("<>")
        for v in HTMLATTR.findall(bare):
            for part in v.split(","):           # srcset: "a.png 1x, b.png 2x"
                part = part.strip().split()[0] if part.strip() else ""
                if part:
                    yield n, part


def check(path, root, problems):
    count = 0
    for n, t in targets(path):
        if not t or PLACEHOLDER in t:
            continue
        if re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", t):     # http:, mailto:, ...
            continue
        count += 1
        where = "%s:%d" % (os.path.relpath(path, root), n)
        file_part, _, frag = t.partition("#")
        file_part = unquote(file_part)
        if file_part == "":
            dest = path
        elif file_part.startswith("/"):
            dest = os.path.join(root, file_part.lstrip("/"))
        else:
            dest = os.path.normpath(os.path.join(os.path.dirname(path), file_part))
        if not os.path.exists(dest):
            problems.append("%s: broken link %s (no %s)" % (where, t, os.path.relpath(dest, root)))
            continue
        if frag and dest.endswith(".md") and os.path.isfile(dest):
            if unquote(frag) not in anchors(dest):
                problems.append("%s: broken anchor %s (no heading #%s in %s)"
                                % (where, t, frag, os.path.relpath(dest, root)))
    return count


def main(argv):
    root = ROOT
    files = []
    i = 0
    while i < len(argv):
        if argv[i] == "--root" and i + 1 < len(argv):
            root = os.path.abspath(argv[i + 1])
            i += 2
        elif argv[i].startswith("-"):
            print("usage: check_md_links.py [--root DIR] [FILE.md ...]")
            return 2
        else:
            files.append(os.path.abspath(argv[i]))
            i += 1
    if not files:
        files = md_files(root)
    problems = []
    total = 0
    for f in files:
        total += check(f, root, problems)
    if total == 0:
        problems.append("no relative links found: nothing was checked")
    for p in problems:
        print("check_md_links: " + p)
    if problems:
        return 1
    print("markdown links ok (%d relative links in %d files)" % (total, len(files)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
