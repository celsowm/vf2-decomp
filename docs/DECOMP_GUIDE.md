# Decompilation workflow

## Static evidence

```sh
build/vf2i960 analyze roms/vf2 out/analysis
```

Inspect the function list, CFG, xrefs, abstract values, indirect targets and
pseudocode before assigning names or writing recovered C.

### Optional liftkit static scaffold

The generic i960 lifter from `segamodel2-tools` can provide an additional IR,
CFG, ABI and C scaffold view for one measured VF2 slice. Keep that checkout
outside this repository and configure it with `--liftkit-root` or
`SEGAMODEL2_TOOLS_ROOT`:

```powershell
python tools/python/liftkit_vf2.py `
  --rom-dir roms/vf2 `
  --liftkit-root C:/path/to/segamodel2-tools `
  --vf2i960 build/Debug/vf2i960.exe `
  --address 0x27b5c `
  --count 128 `
  --name fa_player_27b5c
```

The adapter obtains the listing from the local `vf2i960` command, normalizes
only the address delimiter expected by liftkit, and writes all outputs below
`out/liftkit/`. It does not use the external MAME frontend, which is tied to
Sega Rally ROM names and layout.

The generated `*.lifted.c` file is a deterministic navigation scaffold only.
Unresolved condition-state expressions such as `ac`, raw `ldt`/`stt`, `cvtri`
and any other liftkit placeholder remain evidence gaps. Never copy a scaffold
directly into `src/recovered`; recover the smallest behavior from measured
state and prove it with the ROM-backed differential contract below.

### Optional model2recomp static hints

`model2recomp` can be used as a second static analyst for entry-point discovery.
Its generic i960 lifter exposes candidates found through IAC reinitialization,
interrupt tables, static function discovery and previously measured runtime
hints. The bridge loads only `tools/i960_lifter.py` from an external checkout;
it does not import the Model 2 runtime, geometry engine, TGP or generated game
code.

When a flat program image is available, run:

```powershell
python tools/python/model2recomp_hints.py `
  --program-bin C:/path/to/program.bin `
  --model2recomp-root C:/path/to/model2recomp `
  --out out/model2recomp-vf2-hints.json
```

The report is a candidate index only. Validate every address with the local
`vf2i960` disassembler and the reference executor before using it to guide a
recovery. A discovered interrupt handler or IAC target is not evidence that
its semantics are understood, and no generated external C may be promoted to
`src/recovered`.

## Dynamic evidence

Execute a bounded path:

```sh
build/vf2i960 execute roms/vf2 0x000001b0 2000000
```

Capture local evidence:

```sh
build/vf2i960 trace roms/vf2 out/path.csv 10000
build/vf2i960 snapshot roms/vf2 out/path.vf2snap
```

## Function lifecycle

1. Identify a candidate using static analysis.
2. Record direct calls, strings, hardware addresses and structure offsets.
3. Create a deterministic entry-state checkpoint.
4. Extend the executor only when verified instruction semantics are missing.
5. Write the smallest semantically equivalent C function.
6. Run the original path and the recovered C path from equivalent state.
7. Compare modified memory, registers, return values and hardware commands.
8. Add a ROM-independent unit test for the recovered behavior.
9. Move the function into `src/recovered` only after validation.
10. Update the symbol/evidence CSV and notes.

## Evidence levels

- **verified:** directly encoded by vectors/tables or reproduced by comparison;
- **high:** independent static and dynamic evidence agree;
- **medium:** strong static-analysis hypothesis;
- **low:** provisional organizational name.

## Generated pseudocode policy

Files in `out/analysis/pseudo-c` are disposable navigation aids. They may
contain register variables, gotos, unresolved calls and placeholder memory
operations. Never copy a generated function wholesale into `src/recovered` and
label it recovered.

## Current limitations

- execution coverage is intentionally limited to verified startup semantics;
- static propagation remains mainly intraprocedural;
- computed continuations depending on caller state can remain unresolved;
- no complete scheduler/game task, polygon TGP program or sound program has
  been recovered yet; the evidence-bounded TGP scalar host boundary is now
  available as `vf2_tgp`;
- the Model 2 memory model contains only regions required by current paths.

## Files that must not be committed

- ROM archives or reconstructed ROM regions;
- extracted textures, models, samples or substantial strings;
- full traces containing proprietary behavior data;
- `.vf2snap` state files;
- generated pseudocode copied from ROM analysis.
