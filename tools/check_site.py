#!/usr/bin/env python3
"""Link checker for the showcase page.

usage: check_site.py SITE_DIR        a built site (make web: build/site)
       check_site.py --source        the committed sources (make test):
                                     web/, with img/X read from
                                     docs/img/c64/X and dl/X from the
                                     names make web copies there

For every HTML page in the site (index.html and the pages it links to):
  - every local src/href (img, a, link) must exist; #fragments must name
    an id on the target page;
  - no external resources: src= and <link href> must be local (no CDN,
    no web fonts, no remote scripts); only <a href> may point outside;
  - no <script> at all (the page is static);
  - every <img> has a non-empty alt, width and height;
  - files are what their extension says (PNG, GIF, CSS, D64 size, PRG
    load address $0801);
  - stylesheets: url(...) references must exist too.
Prints the files with their sizes and the total page weight (the page
and everything it references), and fails on any problem.
"""
import os
import re
import sys
from html.parser import HTMLParser
from urllib.parse import urlsplit, unquote

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DL = ("pl0.d64", "pl0.prg")
D64_SIZES = (174848, 175531)
MAX_WEIGHT = 8 * 1024 * 1024


class Page(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.refs, self.ids, self.problems = [], set(), []

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if "id" in a:
            self.ids.add(a["id"])
        if tag == "script":
            self.problems.append("a <script> element (the page must be static)")
        if tag == "img":
            if not (a.get("alt") or "").strip():
                self.problems.append(f"<img src={a.get('src')!r}> without alt text")
            if not a.get("width") or not a.get("height"):
                self.problems.append(f"<img src={a.get('src')!r}> without width/height")
        for attr in ("src", "href"):
            if attr in a and a[attr] is not None:
                self.refs.append((tag, attr, a[attr], self.getpos()[0]))


class Site:
    def __init__(self, source):
        self.source = source
        self.base = os.path.join(ROOT, "web") if source is True else source

    def resolve(self, rel):
        """Site-relative path -> file on disk (None: a download that only
        exists in a built site)."""
        if self.source is True:
            if rel.startswith("img/"):
                return os.path.join(ROOT, "docs/img/c64", rel[4:])
            if rel.startswith("dl/"):
                return None
            if rel == "LICENSE.txt":
                return os.path.join(ROOT, "LICENSE")
        return os.path.join(self.base, rel)


def filetype_problem(path, rel):
    data = open(path, "rb").read(16)
    ext = os.path.splitext(rel)[1].lower()
    if ext == ".png" and not data.startswith(b"\x89PNG\r\n\x1a\n"):
        return "not a PNG"
    if ext == ".gif" and not data[:6] in (b"GIF87a", b"GIF89a"):
        return "not a GIF"
    if ext == ".d64" and os.path.getsize(path) not in D64_SIZES:
        return f"not a 35-track D64 ({os.path.getsize(path)} bytes)"
    if ext == ".prg" and data[:2] != b"\x01\x08":
        return "not a C64 program loading at $0801"
    if ext in (".css", ".html", ".txt"):
        try:
            open(path, encoding="utf-8").read()
        except UnicodeDecodeError:
            return "not UTF-8 text"
    return None


def main(argv):
    if argv == ["--source"]:
        site = Site(True)
    elif len(argv) == 1 and os.path.isdir(argv[0]):
        site = Site(os.path.abspath(argv[0]))
    else:
        sys.exit(__doc__)
    problems, files = [], {}
    pages, seen_pages, ids = ["index.html"], set(), {}
    anchors = []
    while pages:
        rel = pages.pop()
        if rel in seen_pages:
            continue
        seen_pages.add(rel)
        path = site.resolve(rel)
        p = Page()
        p.feed(open(path, encoding="utf-8").read())
        ids[rel] = p.ids
        files[rel] = path
        problems += [f"{rel}: {x}" for x in p.problems]
        for tag, attr, url, line in p.refs:
            u = urlsplit(url)
            where = f"{rel}:{line}"
            if u.scheme or url.startswith("//"):
                if tag != "a":
                    problems.append(f"{where}: external resource <{tag} {attr}={url!r}>")
                elif u.scheme not in ("http", "https", "mailto"):
                    problems.append(f"{where}: odd link {url!r}")
                continue
            target = unquote(u.path) if u.path else rel
            target = os.path.normpath(os.path.join(os.path.dirname(rel), target)).replace(os.sep, "/")
            if target.startswith(".."):
                problems.append(f"{where}: {url!r} leaves the site")
                continue
            if u.fragment:
                anchors.append((where, target, u.fragment))
            if not u.path:
                continue
            fpath = site.resolve(target)
            if fpath is None:
                if os.path.basename(target) not in DL or os.path.dirname(target) != "dl":
                    problems.append(f"{where}: {url!r}: dl/ holds only {', '.join(DL)}")
                continue
            if not os.path.isfile(fpath):
                problems.append(f"{where}: {url!r} does not exist ({os.path.relpath(fpath, ROOT)})")
                continue
            files[target] = fpath
            bad = filetype_problem(fpath, target)
            if bad:
                problems.append(f"{where}: {url!r}: {bad}")
            if target.endswith(".html"):
                pages.append(target)
            if target.endswith(".css"):
                for m in re.finditer(r"url\(\s*['\"]?([^'\")]+)", open(fpath).read()):
                    ref = m.group(1)
                    if re.match(r"[a-z]+:", ref) and not ref.startswith("data:"):
                        problems.append(f"{target}: external url({ref})")
                    elif not ref.startswith("data:"):
                        t = os.path.normpath(os.path.join(os.path.dirname(target), ref))
                        if not os.path.isfile(site.resolve(t) or ""):
                            problems.append(f"{target}: url({ref}) does not exist")
    for where, target, frag in anchors:
        if target in ids and frag not in ids[target]:
            problems.append(f"{where}: #{frag} is not an id in {target}")
    total = 0
    for rel in sorted(files):
        size = os.path.getsize(files[rel])
        total += size
        print(f"  {size:>9}  {rel}")
    print(f"  {total:>9}  total ({total / 1048576:.2f} MB, {len(files)} files)")
    if total > MAX_WEIGHT:
        problems.append(f"page weight {total} bytes, over {MAX_WEIGHT}")
    if problems:
        for x in problems:
            print("check_site: " + x)
        sys.exit(f"check_site: {len(problems)} problem(s)")
    print("check_site: OK")


if __name__ == "__main__":
    main(sys.argv[1:])
