#!/usr/bin/env python3
"""Link checker for the showcase page.

usage: check_site.py SITE_DIR        a built site (make web: build/site)
       check_site.py --source        the committed sources (make test):
                                     web/, with img/X read from
                                     docs/img/c64/X, LICENSE.txt from
                                     LICENSE, and dl/X checked by name
                                     against the files make web copies

For every HTML page in the site (index.html and the pages it links to):
  - every local src/href (img, a, link) must exist; #fragments must name
    an id on the target page; a link starting with "/" fails (the site
    lives under /<repo>/ on GitHub Pages, so absolute paths break);
  - no external resources: src= and <link href> must be local (no CDN,
    no web fonts, no remote scripts); only <a href> may point outside;
  - no <script>, no srcset and no <source> (the page is static and
    every reference is checked here); url(...) in stylesheets, <style>
    blocks and style= attributes must be local and exist;
  - every <img> has a non-empty alt, width and height;
  - files are what their extension says (PNG, GIF, CSS, D64 size, PRG
    load address $0801) and within the size caps below;
  - the page references everything it must (REQUIRED below): the demo
    GIF, a capture per INDEX entry and the five UI screens, both
    downloads, LICENSE.txt, and at least one repository link
    (<a data-repo-path>);
  - a WARNING (not a failure) when the repository links still hold the
    placeholder https://github.com/OWNER/REPO (make web without
    REPO_URL); the Pages workflow fails on it.
Prints the files with their sizes and the total page weight (the page
and everything it references), and fails on any problem.
"""
import os
import re
import subprocess
import sys
from html.parser import HTMLParser
from urllib.parse import urlsplit, unquote

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# size caps (bytes); docs/architecture/media.md lists them
MAX_GIF = 2_500_000          # each .gif (the demo)
MAX_PNG = 300_000            # each .png (a capture)
MAX_TEXT = 200_000           # each .html / .css / .txt
MAX_MEDIA = 6_000_000        # all .png and .gif together
MAX_WEIGHT = 8 * 1024 * 1024  # the page and everything it references

DL = ("pl0.d64", "pl0.prg")  # what make web copies into dl/
D64_SIZES = (174848, 175531)
UI = ("editor", "editor-error", "picker", "help", "save")
PLACEHOLDER = "https://github.com/OWNER/REPO"


def required(examples_dir=os.path.join(ROOT, "examples")):
    """The site-relative paths the page must reference."""
    out = subprocess.run([sys.executable, os.path.join(ROOT, "tools/mkdisk.py"), "--index",
                          examples_dir], check=True, capture_output=True, text=True).stdout
    names = [l.split()[0].lower() for l in out.splitlines() if l.strip()]
    return (["img/demo.gif", "LICENSE.txt"] + ["dl/" + d for d in DL]
            + [f"img/{n}.png" for n in list(UI) + names])


class Page(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.refs, self.ids, self.problems = [], set(), []
        self.css, self.repo_links, self.in_style = [], 0, False

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        line = self.getpos()[0]
        if "id" in a:
            self.ids.add(a["id"])
        if tag == "script":
            self.problems.append(f"{line}: a <script> element (the page must be static)")
        if tag == "source" or "srcset" in a:
            self.problems.append(f"{line}: <{tag}> with srcset/<source>: not supported, "
                                 "keep plain src so every reference is checked")
        if tag == "style":
            self.in_style = True
        if a.get("style"):
            self.css.append((line, a["style"]))
        if tag == "a" and "data-repo-path" in a:
            self.repo_links += 1
        if tag == "img":
            if not (a.get("alt") or "").strip():
                self.problems.append(f"{line}: <img src={a.get('src')!r}> without alt text")
            if not a.get("width") or not a.get("height"):
                self.problems.append(f"{line}: <img src={a.get('src')!r}> without width/height")
        for attr in ("src", "href"):
            if attr in a and a[attr] is not None:
                self.refs.append((tag, attr, a[attr], line))

    def handle_endtag(self, tag):
        if tag == "style":
            self.in_style = False

    def handle_data(self, data):
        if self.in_style:
            self.css.append((self.getpos()[0], data))


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
    size = os.path.getsize(path)
    ext = os.path.splitext(rel)[1].lower()
    if ext == ".png" and not data.startswith(b"\x89PNG\r\n\x1a\n"):
        return "not a PNG"
    if ext == ".gif" and data[:6] not in (b"GIF87a", b"GIF89a"):
        return "not a GIF"
    if ext == ".d64" and size not in D64_SIZES:
        return f"not a 35-track D64 ({size} bytes)"
    if ext == ".prg" and data[:2] != b"\x01\x08":
        return "not a C64 program loading at $0801"
    if ext in (".css", ".html", ".txt"):
        try:
            open(path, encoding="utf-8").read()
        except UnicodeDecodeError:
            return "not UTF-8 text"
    cap = {".gif": MAX_GIF, ".png": MAX_PNG, ".html": MAX_TEXT, ".css": MAX_TEXT,
           ".txt": MAX_TEXT}.get(ext)
    if cap and size > cap:
        return f"{size} bytes, over the {ext} cap of {cap}"
    return None


def css_urls(text):
    return [m.group(1).strip() for m in re.finditer(r"url\(\s*['\"]?([^'\")]+)", text)]


def check(site, need):
    """Returns (problems, warnings, files {rel: path})."""
    problems, warnings, files = [], [], {}
    pages, seen_pages, ids, anchors, referenced = ["index.html"], set(), {}, [], set()
    repo_links = 0

    def local(where, ref, base_rel, css=False):
        """A local reference: problems or the target's site path."""
        if ref.startswith("/"):
            problems.append(f"{where}: {ref!r} starts with '/' (breaks under the "
                            "/<repo>/ path of GitHub Pages; use a relative path)")
            return None
        t = os.path.normpath(os.path.join(os.path.dirname(base_rel), ref)).replace(os.sep, "/")
        if t.startswith(".."):
            problems.append(f"{where}: {ref!r} leaves the site")
            return None
        return t

    def visit(where, target):
        referenced.add(target)
        fpath = site.resolve(target)
        if fpath is None:
            if os.path.dirname(target) != "dl" or os.path.basename(target) not in DL:
                problems.append(f"{where}: {target!r}: dl/ holds only {', '.join(DL)}")
            return
        if not os.path.isfile(fpath):
            problems.append(f"{where}: {target!r} does not exist ({fpath})")
            return
        files[target] = fpath
        bad = filetype_problem(fpath, target)
        if bad:
            problems.append(f"{where}: {target!r}: {bad}")
        if target.endswith(".html"):
            pages.append(target)
        if target.endswith(".css"):
            check_css(target, open(fpath, encoding="utf-8", errors="replace").read(), target)

    def check_css(where, text, base_rel):
        for ref in css_urls(text):
            if ref.startswith("data:"):
                continue
            if re.match(r"[a-z]+:", ref) or ref.startswith("//"):
                problems.append(f"{where}: external url({ref})")
                continue
            t = local(where, ref, base_rel)
            if t:
                visit(where, t)

    while pages:
        rel = pages.pop()
        if rel in seen_pages:
            continue
        seen_pages.add(rel)
        path = site.resolve(rel)
        if not path or not os.path.isfile(path):
            problems.append(f"{rel}: the page does not exist")
            continue
        files[rel] = path
        text = open(path, encoding="utf-8").read()
        if PLACEHOLDER in text:
            warnings.append(f"{rel}: the repository links still point at the placeholder "
                            f"{PLACEHOLDER} (make web REPO_URL=...)")
        p = Page()
        p.feed(text)
        ids[rel] = p.ids
        repo_links += p.repo_links
        problems += [f"{rel}:{x}" for x in p.problems]
        for line, css in p.css:
            check_css(f"{rel}:{line}", css, rel)
        for tag, attr, url, line in p.refs:
            u = urlsplit(url)
            where = f"{rel}:{line}"
            if u.scheme or url.startswith("//"):
                if tag != "a":
                    problems.append(f"{where}: external resource <{tag} {attr}={url!r}>")
                elif u.scheme not in ("http", "https", "mailto"):
                    problems.append(f"{where}: odd link {url!r}")
                continue
            if not u.path:
                if u.fragment:
                    anchors.append((where, rel, u.fragment))
                continue
            target = local(where, unquote(u.path), rel)
            if target is None:
                continue
            if u.fragment:
                anchors.append((where, target, u.fragment))
            visit(where, target)
    for where, target, frag in anchors:
        if target in ids and frag not in ids[target]:
            problems.append(f"{where}: #{frag} is not an id in {target}")
    for r in need:
        if r not in referenced:
            problems.append(f"index.html: no reference to {r} (required)")
    if repo_links == 0:
        problems.append("index.html: no repository link (<a data-repo-path=...>)")
    media = sum(os.path.getsize(f) for r, f in files.items() if r.endswith((".png", ".gif")))
    if media > MAX_MEDIA:
        problems.append(f"media {media} bytes, over {MAX_MEDIA}")
    total = sum(os.path.getsize(f) for f in files.values())
    if total > MAX_WEIGHT:
        problems.append(f"page weight {total} bytes, over {MAX_WEIGHT}")
    return problems, warnings, files


def main(argv):
    if argv == ["--source"]:
        site = Site(True)
    elif len(argv) == 1 and os.path.isdir(argv[0]):
        site = Site(os.path.abspath(argv[0]))
    else:
        sys.exit(__doc__)
    problems, warnings, files = check(site, required())
    total = 0
    for rel in sorted(files):
        size = os.path.getsize(files[rel])
        total += size
        print(f"  {size:>9}  {rel}")
    print(f"  {total:>9}  total ({total / 1048576:.2f} MB, {len(files)} files)")
    for w in warnings:
        print("check_site: WARNING: " + w)
    if problems:
        for x in problems:
            print("check_site: " + x)
        sys.exit(f"check_site: {len(problems)} problem(s)")
    print("check_site: OK")


if __name__ == "__main__":
    main(sys.argv[1:])
