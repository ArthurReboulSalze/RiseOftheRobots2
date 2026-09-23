"""Check the Git index for unexpected, binary or private content before publication."""
import argparse
from pathlib import PurePosixPath
import re
import subprocess
import sys


def allowed(path):
    p = PurePosixPath(path)
    if path in {".gitignore", ".gitattributes", "README.md", "LICENSE", "requirements.txt", "IMPORTER.cmd", "PORT/CMakeLists.txt"}:
        return True
    directories = {"TOOLS": {".py"}, "TOOLS/tests": {".py"}, "TOOLS/ghidra_scripts": {".py"},
                   "TOOLS/templates": {".html"}, "PORT/src": {".cpp", ".h"}, "PORT/tests": {".cpp"},
                   "documentation": {".md"}, ".github/workflows": {".yml"}}
    return p.suffix in directories.get(str(p.parent), set())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--staged", action="store_true", help="Always checks the complete index (explicit alias)")
    parser.parse_args()
    entries = subprocess.check_output(["git", "ls-files", "--stage", "-z"]).split(b"\0")
    errors, count, total = [], 0, 0
    for entry in entries:
        if not entry:
            continue
        meta, raw_path = entry.split(b"\t", 1)
        mode, oid, stage = meta.decode().split()
        path = raw_path.decode("utf-8")
        count += 1
        if mode != "100644" and mode != "100755" or stage != "0" or not allowed(path):
            errors.append(f"Disallowed path or file type: {path}")
            continue
        blob = subprocess.check_output(["git", "cat-file", "blob", oid])
        total += len(blob)
        if len(blob) > 1024 * 1024 or b"\0" in blob:
            errors.append(f"Binary or oversized file: {path}")
            continue
        try:
            text = blob.decode("utf-8-sig")
        except UnicodeDecodeError:
            errors.append(f"Non-UTF-8 text: {path}")
            continue
        # Report names only; never echo a matched credential.
        patterns = [r"-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY-----", r"gh[pousr]_[A-Za-z0-9]{30,}",
                    r"github_pat_[A-Za-z0-9_]{40,}", r"data:(?:image|audio|video)/[^;]+;base64,[A-Za-z0-9+/]{100}"]
        if any(re.search(pattern, text) for pattern in patterns):
            errors.append(f"Possible embedded secret or media: {path}")
    if not count:
        errors.append("Empty index: no file list to audit.")
    for error in errors:
        print(error, file=sys.stderr)
    print(f"Index audit: {count} files, {total:,} text bytes, {len(errors)} errors.")
    return bool(errors)


if __name__ == "__main__":
    sys.exit(main())
