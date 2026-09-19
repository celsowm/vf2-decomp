#!/usr/bin/env python3
"""Stream phase5 FIFO/object-table memory trace into a scene timeline JSONL.

ANALYSIS tool — measured submit correlation only. Not recovered C.
Fail-closed logo: numeric object ids only; no named mesh/title claim.

For each object-table read at 0x020e0004[id*16] and each geo/FIFO word0 write
that reverse-maps via main_data table w0, emit:

  {"type":"object_event","kind":"table_read"|"submit","step":N,
   "id":"0x148","id_int":328,"w0":"0x000b026a","word_index":"0x40430",
   "addr":"0x020e1484","port":"table"|"geo"|"geo_port"|"fifo","val":"0x..."}

Also emit protocol-like FIFO color/tag words as analysis fills (not proven RGB):

  {"type":"fifo_tag","step":N,"addr":"...","w0":"0x14802929","port":"fifo"}

and a final summary record with counts + unique ids.

CLI:
  $MIMO_PYTHON tools/python/build_phase5_timeline.py \
    --trace out/attr-long/fifo-phase5.jsonl \
    --output out/attr-render/scene/phase5_timeline.jsonl
"""
from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parent))
from scene_decode_helper import (  # noqa: E402
    classify_port,
    is_protocol_color_like,
    iter_jsonl,
    load_object_table,
    u32le,
)

TABLE = 0x020E0004
TABLE_LO = 0x020E0000
TABLE_HI = 0x020F0000


def build_timeline(
    trace: Path,
    output: Path,
    *,
    table: dict[str, Any],
    max_tag_events: int = 4000,
    max_object_events: int = 200000,
) -> dict[str, Any]:
    w0_to_id = table["w0_to_id"]
    by_id = table["by_id"]
    output.parent.mkdir(parents=True, exist_ok=True)

    n_lines = 0
    n_mem = 0
    n_table = 0
    n_submit = 0
    n_tags = 0
    id_kind: Counter[str] = Counter()
    id_counts: Counter[int] = Counter()
    step_min: int | None = None
    step_max: int | None = None
    port_counts: Counter[str] = Counter()
    tag_values: Counter[int] = Counter()

    # Correlate recent protocol tags near object submits (analysis fill).
    recent_tags: list[tuple[int, int, str]] = []  # (step, val, port)

    with output.open("w", encoding="utf-8") as out_fh:
        def emit(rec: dict[str, Any]) -> None:
            out_fh.write(json.dumps(rec, separators=(",", ":")) + "\n")

        for ev in iter_jsonl(trace):
            n_lines += 1
            t = ev.get("type")
            if t != "memory":
                continue
            n_mem += 1
            step = int(ev.get("step") or 0)
            addr = int(ev.get("address") or 0)
            kind = ev.get("kind")
            val = u32le(ev.get("bytes"))
            if step_min is None or step < step_min:
                step_min = step
            if step_max is None or step > step_max:
                step_max = step

            port = classify_port(addr)

            # Object-table read at record base: id = (addr - TABLE) / 16
            if kind == "read" and TABLE_LO <= addr < TABLE_HI and (addr - TABLE) % 16 == 0:
                obj_id = (addr - TABLE) // 16
                rec_tbl = by_id.get(obj_id)
                w0 = rec_tbl["w0"] if rec_tbl else val
                word_index = rec_tbl["word_index"] if rec_tbl else None
                # Prefer measured table w0; the memory read value is also useful.
                emit(
                    {
                        "type": "object_event",
                        "kind": "table_read",
                        "step": step,
                        "id": f"0x{obj_id:03x}",
                        "id_int": obj_id,
                        "w0": f"0x{w0:08x}",
                        "w0_read": f"0x{val:08x}",
                        "word_index": None if word_index is None else f"0x{word_index:x}",
                        "addr": f"0x{addr:08x}",
                        "port": "table",
                        "val": f"0x{val:08x}",
                        "near_tags": [
                            {"step": s, "w0": f"0x{tv:08x}", "port": p}
                            for s, tv, p in recent_tags[-4:]
                        ],
                    }
                )
                n_table += 1
                id_kind[f"table:{obj_id}"] += 1
                id_counts[obj_id] += 1
                continue

            if kind != "write":
                continue
            if port is None:
                continue
            port_counts[port] += 1

            # Protocol color/tag immediates — analysis fills, not proven RGB.
            if is_protocol_color_like(val) and n_tags < max_tag_events:
                tag_values[val] += 1
                recent_tags.append((step, val, port))
                if len(recent_tags) > 16:
                    recent_tags.pop(0)
                emit(
                    {
                        "type": "fifo_tag",
                        "step": step,
                        "addr": f"0x{addr:08x}",
                        "w0": f"0x{val:08x}",
                        "port": port,
                        "analysis_fill": True,
                        "note": "protocol tag family; NOT proven RGB",
                    }
                )
                n_tags += 1
                continue

            # Geo/FIFO word0 reverse-map via table w0.
            if val in w0_to_id and val != 0xFFFFFFFF and n_submit < max_object_events:
                obj_id = w0_to_id[val]
                rec = by_id.get(obj_id) or {}
                word_index = rec.get("word_index")
                emit(
                    {
                        "type": "object_event",
                        "kind": "submit",
                        "step": step,
                        "id": f"0x{obj_id:03x}",
                        "id_int": obj_id,
                        "w0": f"0x{val:08x}",
                        "word_index": None if word_index is None else f"0x{word_index:x}",
                        "addr": f"0x{addr:08x}",
                        "port": port,
                        "val": f"0x{val:08x}",
                        "near_tags": [
                            {"step": s, "w0": f"0x{tv:08x}", "port": p}
                            for s, tv, p in recent_tags[-4:]
                        ],
                    }
                )
                n_submit += 1
                id_kind[f"submit:{obj_id}"] += 1
                id_counts[obj_id] += 1

        unique_ids = sorted(id_counts)
        summary = {
            "type": "summary",
            "source": str(trace).replace("\\", "/"),
            "output": str(output).replace("\\", "/"),
            "lines": n_lines,
            "memory_events": n_mem,
            "table_reads": n_table,
            "submits": n_submit,
            "fifo_tags": n_tags,
            "object_events": n_table + n_submit,
            "unique_ids": [f"0x{i:03x}" for i in unique_ids],
            "unique_id_count": len(unique_ids),
            "id_counts": {f"0x{i:03x}": id_counts[i] for i in unique_ids},
            "port_write_counts": dict(port_counts),
            "tag_values_top": [
                {"w0": f"0x{k:08x}", "n": v} for k, v in tag_values.most_common(32)
            ],
            "step_min": step_min,
            "step_max": step_max,
            "table_w0_index_size": len(w0_to_id),
            "fail_closed": {
                "logo_named": False,
                "composite_is_host_assembly": True,
                "fifo_tags_are_analysis_fills_not_rgb": True,
                "transforms_invented": False,
                "note": (
                    "Timeline correlates measured object-table reads and geo/FIFO "
                    "word0 submits via main_data w0 reverse map. No game camera, "
                    "no CRT framebuffer claim, no named mesh."
                ),
            },
        }
        emit(summary)

    return summary


def main() -> int:
    ap = argparse.ArgumentParser(description="Build phase5 scene timeline from FIFO/object-table trace")
    ap.add_argument("--trace", type=Path, default=Path("out/attr-long/fifo-phase5.jsonl"))
    ap.add_argument("--output", type=Path, default=Path("out/attr-render/scene/phase5_timeline.jsonl"))
    ap.add_argument("--main-data", type=Path, default=Path("out/main_data.bin"))
    ap.add_argument("--rom-dir", type=Path, default=Path("roms/vf2"))
    ap.add_argument("--max-id", type=lambda s: int(s, 0), default=0x2000)
    args = ap.parse_args()

    if not args.trace.is_file():
        print(f"ERROR: trace missing: {args.trace}", file=sys.stderr)
        return 2

    table = load_object_table(
        main_data_path=args.main_data,
        rom_dir=args.rom_dir,
        max_id=args.max_id,
    )
    print(
        f"table: by_id={len(table['by_id'])} w0_to_id={len(table['w0_to_id'])} "
        f"main_len={table['main_len']:#x}"
    )
    print(f"streaming {args.trace} ({args.trace.stat().st_size} bytes) ...")
    summary = build_timeline(args.trace, args.output, table=table)
    print(
        f"events: table_reads={summary['table_reads']} submits={summary['submits']} "
        f"fifo_tags={summary['fifo_tags']} unique_ids={summary['unique_id_count']}"
    )
    print(f"unique: {', '.join(summary['unique_ids'][:40])}"
          + (" ..." if summary["unique_id_count"] > 40 else ""))
    print(f"wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
