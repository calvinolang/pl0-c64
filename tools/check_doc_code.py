#!/usr/bin/env python3
"""Check the PL/0 code blocks of the documentation with the host driver.

Usage: check_doc_code.py [--host PATH] [--timeout SECONDS] [FILE.md ...]
       (default files: README.md docs/USER-GUIDE.md docs/LANGUAGE.md;
        default host: build/pl0host, built by `make host`)

Every fenced code block whose info string starts with `pl0` is a PL/0
program, and the rest of the info string says what it must do:

  ```pl0                 it compiles. It is also run (pl0host always runs
                         what it compiles): a runtime error, or still
                         running after the time limit, is fine; a compile
                         error fails the check.
  ```pl0 run             it compiles and runs to the end without an error
                         within the time limit. If the next fenced block
                         (only blank lines between) has the info string
                         `output`, the program's output must equal it
                         (trailing blanks of each line and trailing empty
                         lines are ignored).
  ```pl0 example:NAME    it is examples/NAME.pl0, byte for byte (so a copy
                         of an example in a guide cannot drift from the
                         file), and it runs cleanly like `pl0 run`.
  ```pl0 error:TEXT      compiling and running it ends with exactly this
                         error: TEXT is the whole line (`ERROR line 3:
                         undeclared name`) or only the message
                         (`undeclared name`), which then must match
                         whatever the line.

Any other text after `pl0` fails (a typo would silently weaken a check),
and so does an `output` block that does not follow a `pl0 run` block.
The step limit is a wall-clock limit (pl0host has no instruction limit):
5 seconds for `pl0 run` and `pl0 error:` blocks (--timeout changes it),
half a second (or less, with a smaller --timeout) for `pl0` blocks, which
only need to get past the compiler. pl0host has no keyboard: `?x` always ends with
`bad input`, so programs that read input can only be `pl0` blocks.

Exit 0 if every block passes and there is at least one block, 1 if a
check fails (each failure printed with file and line), 2 on usage.
"""
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_FILES = ["README.md", "docs/USER-GUIDE.md", "docs/LANGUAGE.md"]
TIMEOUT = 5.0           # pl0 run, pl0 error: (--timeout)
COMPILE_TIMEOUT = 0.5   # pl0: running on is fine, only the compile counts
# the messages the VM reports while running (src/core/errors.c); a `pl0`
# block may end with one of these, never with a compile error
RUNTIME = ["division by zero", "domain error", "number overflow",
           "stack overflow (too deep)", "stopped", "bad input"]
FENCE = re.compile(r"^( {0,3})(`{3,}|~{3,})(.*)$")
ERRLINE = re.compile(r"^ERROR line (\d+): (.*)$")


def blocks(path):
    """Yield (line number, info string, text) for every fenced block."""
    with open(path, encoding="utf-8") as f:
        lines = f.read().split("\n")
    i = 0
    while i < len(lines):
        m = FENCE.match(lines[i])
        if not m:
            i += 1
            continue
        indent, fence, info = len(m.group(1)), m.group(2), m.group(3).strip()
        start = i + 1
        body = []
        i += 1
        closed = False
        while i < len(lines):
            c = re.match(r"^ {0,3}(`{3,}|~{3,})\s*$", lines[i])
            if c and c.group(1)[0] == fence[0] and len(c.group(1)) >= len(fence):
                closed = True
                break
            body.append(lines[i][indent:] if lines[i][:indent].strip() == "" else lines[i])
            i += 1
        if not closed:
            yield start, info, None          # unterminated fence
            return
        yield start, info, "\n".join(body) + "\n"
        i += 1


def norm(text):
    out = [l.rstrip() for l in text.split("\n")]
    while out and out[-1] == "":
        out.pop()
    return "\n".join(out)


def run(host, src, limit):
    fd, path = tempfile.mkstemp(suffix=".pl0")
    try:
        with os.fdopen(fd, "w", encoding="ascii") as f:
            f.write(src)
        try:
            p = subprocess.run([host, path], capture_output=True, text=True,
                               timeout=limit)
        except subprocess.TimeoutExpired:
            return None, "", ""
        return p.returncode, p.stdout, p.stderr
    finally:
        os.unlink(path)


def error_of(stderr):
    for l in stderr.split("\n"):
        if ERRLINE.match(l):
            return l
    return None


def check_file(host, path, problems):
    n = 0
    pending = None          # (line, expected-output check) of a pl0 run block
    prev_end_run = None
    items = list(blocks(path))
    for idx, (line, info, text) in enumerate(items):
        where = "%s:%d" % (os.path.relpath(path, ROOT) if path.startswith(ROOT) else path, line)
        if text is None:
            problems.append("%s: unterminated code fence" % where)
            return n
        words = info.split(None, 1)
        lang = words[0] if words else ""
        if lang == "output":
            if prev_end_run is None:
                problems.append("%s: `output` block without a `pl0 run` block before it" % where)
            else:
                got = prev_end_run
                if norm(got) != norm(text):
                    problems.append("%s: output differs from the program's:\n--- expected\n%s\n--- got\n%s"
                                    % (where, norm(text), norm(got)))
            prev_end_run = None
            continue
        prev_end_run = None
        if lang != "pl0":
            continue
        n += 1
        if text.isascii() is False:
            problems.append("%s: not plain ASCII" % where)
            continue
        mode = words[1].strip() if len(words) > 1 else ""
        rc, out, err = run(host, text, min(COMPILE_TIMEOUT, TIMEOUT) if mode == "" else TIMEOUT)
        e = error_of(err)
        if mode == "":
            if rc is None or rc == 0:
                continue
            if rc == 1 and e and ERRLINE.match(e).group(2) in RUNTIME:
                continue
            problems.append("%s: does not compile: %s" % (where, (e or err.strip() or "exit %s" % rc)))
        elif mode == "run":
            if rc is None:
                problems.append("%s: still running after %g s" % (where, TIMEOUT))
            elif rc != 0 or err.strip():
                problems.append("%s: does not run cleanly: %s" % (where, e or err.strip() or "exit %d" % rc))
            else:
                # an output block may follow (blank lines only between)
                if idx + 1 < len(items):
                    nline = items[idx + 1][0]
                    with open(path, encoding="utf-8") as f:
                        all_lines = f.read().split("\n")
                    end = line + text.count("\n")      # closing fence (0-based index)
                    between = all_lines[end + 1:nline - 1]
                    if all(not b.strip() for b in between):
                        prev_end_run = out
        elif mode.startswith("example:"):
            name = mode[len("example:"):].strip()
            ex = os.path.join(ROOT, "examples", name + ".pl0")
            if not re.fullmatch(r"[a-z0-9]+", name) or not os.path.exists(ex):
                problems.append("%s: no example examples/%s.pl0" % (where, name))
                continue
            with open(ex, encoding="ascii") as f:
                want = f.read()
            if norm(want) != norm(text):
                problems.append("%s: differs from examples/%s.pl0" % (where, name))
            elif rc is None:
                problems.append("%s: still running after %g s" % (where, TIMEOUT))
            elif rc != 0 or err.strip():
                problems.append("%s: does not run cleanly: %s" % (where, e or err.strip() or "exit %d" % rc))
        elif mode.startswith("error:"):
            want = mode[len("error:"):].strip()
            if not want:
                problems.append("%s: `error:` without a message" % where)
            elif rc is None:
                problems.append("%s: expected `%s`, still running after %g s" % (where, want, TIMEOUT))
            elif e is None:
                problems.append("%s: expected `%s`, got no error" % (where, want))
            elif want.startswith("ERROR line "):
                if e != want:
                    problems.append("%s: expected `%s`, got `%s`" % (where, want, e))
            elif ERRLINE.match(e).group(2) != want:
                problems.append("%s: expected `%s`, got `%s`" % (where, want, e))
        else:
            problems.append("%s: unknown marker `pl0 %s` (use pl0, pl0 run, pl0 example:NAME or pl0 error:MESSAGE)"
                            % (where, mode))
    return n


def main(argv):
    global TIMEOUT
    host = os.path.join(ROOT, "build", "pl0host")
    files = []
    i = 0
    while i < len(argv):
        if argv[i] == "--host" and i + 1 < len(argv):
            host = argv[i + 1]
            i += 2
        elif argv[i] == "--timeout" and i + 1 < len(argv):
            TIMEOUT = float(argv[i + 1])
            i += 2
        elif argv[i].startswith("-"):
            print(__doc__.split("\n\n")[1])
            return 2
        else:
            files.append(argv[i])
            i += 1
    if not files:
        files = [os.path.join(ROOT, f) for f in DEFAULT_FILES]
    if not os.access(host, os.X_OK):
        print("check_doc_code: %s not built (run make host)" % host)
        return 2
    problems = []
    total = 0
    for f in files:
        if not os.path.exists(f):
            problems.append("%s: missing" % f)
            continue
        total += check_file(host, f, problems)
    if total == 0:
        problems.append("no ```pl0 blocks found: nothing was checked")
    for p in problems:
        print("check_doc_code: " + p)
    if problems:
        return 1
    print("doc code ok (%d PL/0 blocks in %d files)" % (total, len(files)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
