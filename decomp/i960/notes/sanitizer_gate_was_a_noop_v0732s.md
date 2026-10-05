# The sanitizer gate was a silent no-op on MSVC (v0732s)

**`VF2_ENABLE_SANITIZERS=ON` instrumented nothing on the canonical Windows
checkout.** Every "the sanitizer gate is still unrun" note in this repo —
including the one that had been carried across the whole v0732b–v0732r series —
was reporting on a gate that could not have caught anything, because on MSVC it
never turned on.

## What was wrong

`cmake/VF2Warnings.cmake` put the sanitizer flags inside the `else()` of
`if(MSVC)`:

```cmake
if(MSVC)
    target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    ...
else()
    ...
    if(VF2_ENABLE_SANITIZERS)
        target_compile_options(${target_name} PRIVATE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target_name} PRIVATE -fsanitize=address,undefined)
    endif()
endif()
```

Those are GCC/Clang flags. On MSVC the option was accepted, the build
succeeded, every test ran, and nothing was instrumented. Nothing warned. The
gate reported a pass it had no capacity to fail.

**Why it survived so long:** ASan produces no output when it is absent, so
"green" and "not instrumented" are indistinguishable from the outside. That
timing is *not* a reliable tell, either — see the correction at the end.

## The fix

`cmake/VF2Warnings.cmake` now applies `/fsanitize=address` and `/Zi` in the MSVC
branch, with `/INCREMENTAL:NO` at link (MSVC's documented ASan requirement), and
`CMakeLists.txt` states at configure time which path is in effect:

```text
-- VF2_ENABLE_SANITIZERS=ON: MSVC AddressSanitizer (/fsanitize=address)
-- VF2_ENABLE_SANITIZERS=OFF: no sanitizer instrumentation
```

The OFF case is now reported too. A gate that cannot say whether it is armed
should not be trusted when it says it passed.

## Two environment gotchas on this machine

Both cost real time and both fail *silently-ish*, so they are worth writing down.

### 1. The default toolset has no x64 ASan link libraries

`cmake` here resolves to **Visual Studio 18 BuildTools, MSVC 14.50.35717**. That
toolset ships `clang_rt.asan*` for **arm64 and i386 only** — its `lib\x64`
directory exists but contains no ASan libraries. `/fsanitize=address` therefore
fails at link with

```text
LINK : fatal error LNK1104: cannot open file
       'clang_rt.asan_dynamic_runtime_thunk-x86_64.lib'
```

The **VS 2022 Community** toolset (14.44.35207) on the same machine *does* have
the x64 libraries. So the sanitizer tree must be configured explicitly:

```sh
cmake -S . -B build-san -G "Visual Studio 17 2022" -A x64 \
  -DVF2_BUILD_TESTS=ON -DVF2_WARNINGS_AS_ERRORS=ON \
  -DVF2_ENABLE_SANITIZERS=ON -DVF2_ROM_DIR=/path/to/vf2
cmake --build build-san --config Debug --parallel
```

The pre-existing `build-san/` was a September cache pinned to the BuildTools
toolset, which is why reconfiguring in place did not help — it had to be removed
and recreated.

### 2. The ASan runtime DLL must be on `PATH` at run time

With the libraries linked, the instrumented binaries still exited immediately:

```text
exit = -1073741515     (0xC0000135, STATUS_DLL_NOT_FOUND)
```

`clang_rt.asan_dynamic-x86_64.dll` is not on the default `PATH`. Add the toolset
bin directory before running ctest, or ctest hangs rather than failing:

```powershell
$env:PATH = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64;" + $env:PATH
ctest --test-dir build-san -C Debug --output-on-failure
```

**A hang is the worst failure mode for a gate** — it reads as "still running",
not "broken". The `ctest` process and the test child both sat at 0.02 s CPU.

## What the gate now reports

`vf2_coli_2396c_live_tests` under real ASan, both legs, no sanitizer
diagnostics:

```text
fixture premise: 21/30 non-identity permutation bytes, 30/30 non-zero 4th words
fighter 0x00510980: FULL MATCH (3023 insn, 3 calls, 4 returns, all 120 cluster words, all 30 4th words)
fighter 0x00512980: FULL MATCH (3023 insn, 3 calls, 4 returns, all 120 cluster words, all 30 4th words)
coli-2396c-live differential tests passed
```

Full-suite result under real ASan:

```text
100% tests passed, 0 tests failed out of 119
Total Test time (real) = 2542.61 sec
```

No AddressSanitizer diagnostics from any test, including the two `0x2396c` legs
above. That closes the gate that had been carried as "unrun" across the whole
v0732b–v0732r series.

## Standing rule this adds

**A gate must be able to fail, and you must prove it can.** Before trusting any
validation step, confirm it actually ran:

- does the build print which instrumentation is in effect?
- does the binary actually depend on the sanitizer runtime?
- does it exit non-zero when you break something on purpose?

If any of those is missing, the gate is decorative. The same question applies to
the differential contract: `vf2_i960_compare_live_state` is only worth having if
something compares against it, which is precisely what `test_coli_2396c_poly_cluster`
was not doing.

## Correction: wall-clock is NOT a valid check

I first wrote this note claiming the tell was that the sanitizer tree ran the
`coli` subset in 31.10 s against the plain build's 31.44 s. **That reasoning was
wrong, and the full run disproved it.**

Once genuinely instrumented, the sanitizer suite turned out to cost almost
nothing extra. The dominating test, `vf2_player_4505_live_tests`, took
**1497 s of CPU under ASan against roughly 1380 s uninstrumented** — about 8%,
and within the run-to-run noise of a machine that was simultaneously being polled.
A 2–3× ASan slowdown is the folklore, and this workload (an interpreter stepping
guest instructions over a mostly-flat memory image) simply is not
allocation-heavy enough for it to apply.

So "the sanitizer run should be measurably slower" is a bad test: on this codebase
it would have reported the working gate as broken. The checks that actually
discriminate are the other two:

- **the configure-time line** says which instrumentation is in effect, and
- **the binary's dependency on the sanitizer runtime** is observable — before the
  toolset `bin` directory was on `PATH`, the instrumented binary exited
  `0xC0000135`, which an uninstrumented binary never would.

A third, strongest check is to plant a deliberate out-of-bounds access and confirm
the gate catches it. That was not done here, so the honest claim is: the gate is
**armed and reporting**, and the suite is **clean under it** — not that the gate
has been proven able to fail. Given it was silently inert for this long, proving
that is the obvious next step rather than a footnote.
