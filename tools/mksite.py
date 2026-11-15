#!/usr/bin/env python3
"""The generated parts of the showcase page (web/index.html).

usage: mksite.py --write  [PAGE]     rewrite the generated parts in place
       mksite.py --check  [PAGE]     fail if they are out of date (make test)
       mksite.py --repo-url URL [--branch B] PAGE
                                     point the repository links of PAGE
                                     (a copy in build/site) at URL

PAGE defaults to web/index.html. Generated, between the markers
<!-- gallery:begin --> and <!-- gallery:end -->: one figure per example
in the INDEX order of tools/mkdisk.py (name, the one-line description
from INDEX, the capture img/NAME.png, the source in a <details> block).

Repository links: every <a data-repo-path="docs/X.md"> gets the href
REPO_URL/blob/BRANCH/docs/X.md. REPO_URL lives in one place, the page's
<meta name="repo-url" content="...">; `make web REPO_URL=...` (the Pages
workflow passes the real one) overrides it in the built copy.
"""
import html
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BEGIN, END = "<!-- gallery:begin -->", "<!-- gallery:end -->"


def index():
    out = subprocess.run([sys.executable, os.path.join(ROOT, "tools/mkdisk.py"), "--index",
                          os.path.join(ROOT, "examples")],
                         check=True, capture_output=True, text=True).stdout
    return [(l.split(None, 1)[0].lower(), l.split(None, 1)[1].strip())
            for l in out.splitlines() if l.strip()]


def gallery():
    parts = [BEGIN]
    for name, desc in index():
        drawing = os.path.exists(os.path.join(ROOT, "examples", name + ".lines"))
        src = open(os.path.join(ROOT, "examples", name + ".pl0")).read()
        nlines = src.count("\n")
        alt = (f"{name.upper()} on the C64: {desc}, drawn by the program on the bitmap screen"
               if drawing else
               f"{name.upper()} on the C64: {desc}, the program's output on the text screen")
        parts.append(f'''<figure class="example" id="ex-{name}">
  <a href="img/{name}.png"><img src="img/{name}.png" width="768" height="544" loading="lazy" alt="{html.escape(alt, quote=True)}"></a>
  <figcaption><strong>{name.upper()}</strong> <span class="kind">{"drawing" if drawing else "text"}</span><br>{html.escape(desc)}</figcaption>
  <details>
    <summary>Source ({nlines} lines)</summary>
    <pre><code>{html.escape(src)}</code></pre>
  </details>
</figure>''')
    parts.append(END)
    return "\n".join(parts)


def regenerate(page):
    i, j = page.find(BEGIN), page.find(END)
    if i < 0 or j < i:
        sys.exit(f"mksite: the page has no {BEGIN} ... {END} block")
    return page[:i] + gallery() + page[j + len(END):]


def repo_links(page, url, branch):
    url = url.rstrip("/")
    page = re.sub(r'(<meta name="repo-url" content=")[^"]*(")', lambda m: m.group(1) + url + m.group(2), page)
    return re.sub(r'<a data-repo-path="([^"]+)" href="[^"]*"',
                  lambda m: f'<a data-repo-path="{m.group(1)}" href="{url}/blob/{branch}/{m.group(1)}"',
                  page)


def meta_url(page):
    m = re.search(r'<meta name="repo-url" content="([^"]*)"', page)
    if not m:
        sys.exit("mksite: the page has no <meta name=\"repo-url\" content=\"...\">")
    return m.group(1)


def main(argv):
    mode = argv[0] if argv else ""
    if mode == "--repo-url":
        url, rest = argv[1], argv[2:]
        branch = "main"
        if rest[:1] == ["--branch"]:
            branch, rest = rest[1], rest[2:]
        path = rest[0]
        page = open(path).read()
        open(path, "w").write(repo_links(page, url, branch))
        return
    if mode not in ("--write", "--check"):
        sys.exit(__doc__)
    path = argv[1] if len(argv) > 1 else os.path.join(ROOT, "web/index.html")
    page = open(path).read()
    new = repo_links(regenerate(page), meta_url(page), "main")
    if mode == "--write":
        if new != page:
            open(path, "w").write(new)
            print(f"mksite: rewrote {path}")
    elif new != page:
        sys.exit(f"mksite: {path} is out of date (examples or INDEX changed): "
                 "run python3 tools/mksite.py --write")


if __name__ == "__main__":
    main(sys.argv[1:])
