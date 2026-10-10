#!/usr/bin/env python3
"""Recover the shipped i960 symbol table from a Virtua Fighter 2 port DLL.

Sega's later ports of the Model 2 titles keep the original board-side symbol
table inside the host DLL: a run of 16-byte records in `.rdata`, each one a
little-endian `u64` i960 program offset followed by a `u64` pointer to the
NUL-terminated C string that names it. Nothing in the DLL points at those
strings -- they are only reachable through the table -- so a plain string
search recovers the names and loses the addresses. This walks the records.

The table is located by shape rather than by address, so the script is not
keyed to a particular build: a record is a plausible board offset next to a
pointer to an identifier-shaped string, and the table is the longest unbroken
run of such records whose offsets never decrease. A DLL with no such run is
reported as such instead of being guessed at.

    python tools/python/extract_original_symbols.py --dll <path>
    python tools/python/extract_original_symbols.py --dll <path> --tables
    python tools/python/extract_original_symbols.py --dll <path> \
        --out decomp/i960/original_symbols.csv
    python tools/python/extract_original_symbols.py --dll <path> \
        --check decomp/i960/original_symbols.csv

`--out` writes the `decomp/i960/original_symbols.csv` overlay consumed by
`vf2_i960_apply_symbol_overlays`; `--check` compares a DLL against a committed
overlay and exits non-zero on any disagreement, which is how the committed file
stays honest without the DLL being in the repository.

The DLL is not redistributable and is not part of this repository. Only the
recovered `(address, name)` pairs are committed. See `docs/ORIGINAL_SYMBOLS.md`.
"""

from __future__ import annotations

import argparse
import csv
import re
import struct
import sys
from pathlib import Path

# A record holds a board address, so the bound is the i960's 32-bit space
# rather than the size of any one Model 2 region. It still excludes a host
# pointer, which lives above 0x180000000.
MAX_OFFSET = 0x100000000
# Shorter than this is a coincidence, not a symbol table.
MIN_RUN = 32
# Assembler labels are wider than C identifiers; some ports carry a trailing
# colon on the label and dropping those records would split the table in two.
NAME_RE = re.compile(rb"^[A-Za-z_][A-Za-z0-9_.$@]*:?$")
MAX_NAME = 79
# What separates a symbol table from the other (value, string) tables a C++
# runtime leaves in .rdata: the locale tables next door have the same shape but
# every name in them is one letter. A symbol table's names are words.
MIN_MEAN_NAME = 4.0


class Section:
    __slots__ = ("name", "va", "raw", "size")

    def __init__(self, name, va, raw, size):
        self.name = name
        self.va = va
        self.raw = raw
        self.size = size


def read_pe(buf):
    """Just enough of a PE32+ to map a virtual address back to a file offset."""
    if buf[:2] != b"MZ":
        raise ValueError("not a PE: no MZ signature")
    pe = struct.unpack_from("<I", buf, 0x3C)[0]
    if buf[pe : pe + 4] != b"PE\0\0":
        raise ValueError("not a PE: no PE signature")
    section_count = struct.unpack_from("<H", buf, pe + 6)[0]
    optional_size = struct.unpack_from("<H", buf, pe + 20)[0]
    optional = pe + 24
    magic = struct.unpack_from("<H", buf, optional)[0]
    if magic != 0x20B:
        raise ValueError("not PE32+ (optional header magic 0x%x)" % magic)
    image_base = struct.unpack_from("<Q", buf, optional + 24)[0]
    sections = []
    for index in range(section_count):
        entry = optional + optional_size + index * 40
        name = buf[entry : entry + 8].rstrip(b"\0").decode("latin1")
        virtual_size = struct.unpack_from("<I", buf, entry + 8)[0]
        virtual_address = struct.unpack_from("<I", buf, entry + 12)[0]
        raw_size = struct.unpack_from("<I", buf, entry + 16)[0]
        raw = struct.unpack_from("<I", buf, entry + 20)[0]
        # Only what the file actually carries is addressable: a section that is
        # longer virtually than on disk would otherwise read the next section.
        size = min(virtual_size or raw_size, raw_size)
        sections.append(Section(name, image_base + virtual_address, raw, size))
    return sections


def file_offset(sections, va):
    """The file offset a virtual address names, and the end of its section."""
    for section in sections:
        if section.va <= va < section.va + section.size:
            return section.raw + (va - section.va), section.raw + section.size
    return None


def name_at(sections, buf, va):
    """The NUL-terminated identifier at a virtual address, or None."""
    located = file_offset(sections, va)
    if located is None:
        return None
    start, section_end = located
    end = buf.find(b"\0", start)
    if end < 0 or end == start or end > start + MAX_NAME or end >= section_end:
        return None
    text = buf[start:end]
    return text.decode("latin1") if NAME_RE.match(text) else None


def find_tables(sections, buf):
    """Every run of consecutive well-formed records, longest first."""
    runs = []

    def close(run):
        if run is None or len(run["rows"]) < MIN_RUN:
            return
        mean = sum(len(name) for _, name in run["rows"]) / len(run["rows"])
        if mean >= MIN_MEAN_NAME:
            runs.append(run)

    for section in sections:
        if not section.size:
            continue
        limit = section.raw + section.size
        # Records are 16 bytes and 8-byte aligned, so both phases are walked.
        for phase in (0, 8):
            run = None
            offset = section.raw + phase
            while offset + 16 <= limit:
                value, pointer = struct.unpack_from("<QQ", buf, offset)
                name = name_at(sections, buf, pointer) if value < MAX_OFFSET else None
                if name is None:
                    close(run)
                    run = None
                    offset += 16
                    continue
                # An offset that goes backwards ends the run and starts a new
                # one at the same record rather than dropping it.
                if run is not None and value < run["last"]:
                    close(run)
                    run = None
                if run is None:
                    run = {
                        "va": section.va + (offset - section.raw),
                        "section": section.name,
                        "rows": [],
                        "last": 0,
                    }
                run["rows"].append((value, name))
                run["last"] = value
                offset += 16
            close(run)
    return sorted(runs, key=lambda run: len(run["rows"]), reverse=True)


def load_overlay(path):
    with path.open(newline="", encoding="utf-8") as handle:
        return [
            (int(row["address"], 16), row["name"])
            for row in csv.DictReader(handle)
            if row.get("address")
        ]


def render_overlay(rows, provisional):
    lines = ["address,name,kind,status,notes"]
    for index, (address, name) in enumerate(rows, start=1):
        note = "Shipped i960 symbol table record %d" % index
        if address in provisional and provisional[address] != name:
            note += "; supersedes provisional name %s" % provisional[address]
        lines.append(
            "0x%08x,%s,original-symbol,verified,%s" % (address, name, note)
        )
    return "\n".join(lines) + "\n"


def repository_provisional_names(root):
    """Existing repository names keyed by address, for the notes column."""
    names = {}
    sources = (
        root / "decomp" / "i960" / "functions.csv",
        root / "decomp" / "i960" / "symbols.csv",
    )
    for path in sources:
        if not path.is_file():
            continue
        with path.open(newline="", encoding="utf-8") as handle:
            for row in csv.DictReader(handle):
                address = row.get("address")
                value = row.get("name")
                if address and value:
                    names.setdefault(int(address, 16), value)
    return names


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--dll", required=True, help="port DLL carrying the table")
    parser.add_argument("--out", help="write the CSV overlay to this path")
    parser.add_argument("--check", help="compare the DLL against a committed overlay")
    parser.add_argument(
        "--tables",
        action="store_true",
        help="report every candidate run found and emit nothing",
    )
    parser.add_argument(
        "--repo-root",
        default=Path(__file__).resolve().parents[2],
        type=Path,
        help="repository root used to look up provisional names",
    )
    args = parser.parse_args(argv)

    buf = Path(args.dll).read_bytes()
    tables = find_tables(read_pe(buf), buf)

    if args.tables:
        if not tables:
            print("no symbol table found")
        for table in tables:
            first, last = table["rows"][0], table["rows"][-1]
            print(
                "%x in %s: %d records, 0x%X %s .. 0x%X %s"
                % (
                    table["va"],
                    table["section"],
                    len(table["rows"]),
                    first[0],
                    first[1],
                    last[0],
                    last[1],
                )
            )
        return 0

    if not tables:
        print(
            "%s: no run of at least %d symbol records; this DLL does not carry "
            "a table or it is not shaped like one" % (args.dll, MIN_RUN),
            file=sys.stderr,
        )
        return 1
    rows = tables[0]["rows"]

    if args.check:
        want = load_overlay(Path(args.check))
        if want == rows:
            print("%s: %d symbols, all matching" % (args.check, len(rows)))
            return 0
        for index in range(max(len(want), len(rows))):
            a = want[index] if index < len(want) else None
            b = rows[index] if index < len(rows) else None
            if a != b:
                print("record %d: committed %s != recovered %s" % (index + 1, a, b))
        return 1

    text = render_overlay(rows, repository_provisional_names(args.repo_root))
    if args.out:
        Path(args.out).write_text(text, encoding="utf-8")
        print("%s: %d symbols" % (args.out, len(rows)), file=sys.stderr)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
