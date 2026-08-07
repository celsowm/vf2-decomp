#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 WORK_DIR SOURCE_DIR" >&2
  exit 2
fi

work_dir="$1"
source_dir="$2"

cp "$source_dir/src/recovered/native_runtime_state.c" \
   "$work_dir/src/recovered/native_runtime_state.c"
cp "$source_dir/src/recovered/native_differential_step.c" \
   "$work_dir/src/recovered/native_differential_step.c"
cp "$source_dir/tests/recovered/test_native_runtime_state.c" \
   "$work_dir/tests/recovered/test_native_runtime_state.c"
cp "$source_dir/tools/vf2cycles/main_v2.c" \
   "$work_dir/tools/vf2cycles/main.c"

python3 "$source_dir/automation/apply_runtime_state_checkpoints.py" "$work_dir"

git -C "$work_dir" diff --check

CC=gcc cmake -S "$work_dir" -B "$work_dir/build-gcc" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DVF2_BUILD_TESTS=ON \
  -DVF2_WARNINGS_AS_ERRORS=ON \
  -DVF2_ROM_DIR=/tmp/no-proprietary-roms
cmake --build "$work_dir/build-gcc" --parallel
ctest --test-dir "$work_dir/build-gcc" --output-on-failure

CC=clang cmake -S "$work_dir" -B "$work_dir/build-clang" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DVF2_BUILD_TESTS=ON \
  -DVF2_WARNINGS_AS_ERRORS=ON \
  -DVF2_ROM_DIR=/tmp/no-proprietary-roms
cmake --build "$work_dir/build-clang" --parallel
ctest --test-dir "$work_dir/build-clang" --output-on-failure

CC=clang cmake -S "$work_dir" -B "$work_dir/build-asan" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DVF2_BUILD_TESTS=ON \
  -DVF2_WARNINGS_AS_ERRORS=ON \
  -DVF2_ENABLE_SANITIZERS=ON \
  -DVF2_ROM_DIR=/tmp/no-proprietary-roms
cmake --build "$work_dir/build-asan" --parallel
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir "$work_dir/build-asan" --output-on-failure

git -C "$work_dir" config user.name "github-actions[bot]"
git -C "$work_dir" config user.email \
  "41898282+github-actions[bot]@users.noreply.github.com"
git -C "$work_dir" add \
  CMakeLists.txt README.md docs/STATUS.md \
  include/vf2/native_runtime.h \
  include/vf2/native_differential.h \
  src/recovered/native_runtime_state.c \
  src/recovered/native_differential_step.c \
  tests/recovered/test_native_runtime_state.c \
  tools/vf2cycles/main.c tools/vf2i960/main.c

git -C "$work_dir" commit -m "runtime: persist exact failure checkpoints"
git -C "$work_dir" push origin HEAD:master
