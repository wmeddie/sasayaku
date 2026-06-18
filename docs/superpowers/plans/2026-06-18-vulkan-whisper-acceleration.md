# Vulkan Whisper Acceleration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make whisper transcription run on the AMD GPU via the Vulkan ggml backend, with CPU fallback intact.

**Architecture:** The C++ code already requests GPU (`whisper_wrapper.cpp` sets `ctx_params.use_gpu = config.whisper.use_gpu`, default `true`). The only missing piece is a whisper.cpp build that *contains* a GPU backend. We rebuild whisper.cpp with `-DGGML_VULKAN=ON` (producing `libggml-vulkan.so`), link that library into the Sasayaku build via Meson, and verify the Radeon device is selected and faster than CPU.

**Tech Stack:** whisper.cpp / ggml Vulkan backend, Vulkan 1.4 loader + `libvulkan_radeon` (Mesa RADV), `glslc` (shader compiler), CMake, Meson.

## Global Constraints

- Keep the CPU backend compiled in (`libggml-cpu.so`) — Vulkan must fall back to CPU when no device is available (spec §8, §10).
- whisper.cpp built with `-DBUILD_SHARED_LIBS=ON`; the app links the resulting `.so`s with an rpath into `whisper.cpp/build/src` and `whisper.cpp/build/ggml/src`.
- Do **not** commit build artifacts — `build/` is gitignored and `whisper.cpp/` is a pinned submodule. Only `meson.build` and docs are committed.
- Hardware/runtime: AMD Radeon RX 9060 XT (Navi 44), GNOME Wayland, Ubuntu 26.04.
- Benchmark model: the already-downloaded `~/.local/share/sasayaku/models/ggml-large-v3-turbo.bin`; sample `whisper.cpp/samples/jfk.wav`.
- Commit messages end with the trailer: `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.

---

## File Structure

- **Modify:** `meson.build` — add `ggml-vulkan` to the whisper dependency `find_library` set (the `whisper_lib` block, currently around lines 34–58).
- **Modify:** `README.md` — Linux build section: replace the CUDA whisper build command with the Vulkan one; note `glslc`/Vulkan deps.
- **Modify:** `QUICKSTART.md` — same Linux build-command update if it duplicates the whisper build step.
- **Build artifacts (not committed):** `whisper.cpp/build/` (rebuilt with Vulkan), `build/` (app rebuilt).

---

## Task 1: Build whisper.cpp with the Vulkan backend

**Files:**
- Build only (no tracked files change). Produces `whisper.cpp/build/ggml/src/libggml-vulkan.so*` and `whisper.cpp/build/bin/whisper-cli`.

**Interfaces:**
- Consumes: nothing.
- Produces: `libggml-vulkan.so` (linked by Task 2) and a working `whisper-cli` (used for verification in Tasks 1 and 3).

- [ ] **Step 1: Confirm the "before" state — no Vulkan backend exists**

Run:
```bash
ls whisper.cpp/build/ggml/src/libggml-vulkan.so 2>/dev/null || echo "NO VULKAN BACKEND (expected before this task)"
```
Expected: prints `NO VULKAN BACKEND (expected before this task)`.

- [ ] **Step 2: Confirm the shader compiler and Vulkan loader are present**

Run:
```bash
command -v glslc && pkg-config --modversion vulkan && ls /usr/lib/x86_64-linux-gnu/libvulkan_radeon.so
```
Expected: a `glslc` path, a Vulkan version (e.g. `1.4.341`), and the `libvulkan_radeon.so` path. If `glslc` is missing, install `glslc`/`shaderc` (`sudo apt install glslc` or `shaderc`).

- [ ] **Step 3: Reconfigure whisper.cpp with Vulkan ON (and examples ON for the benchmark CLI)**

Run:
```bash
cmake -B whisper.cpp/build -S whisper.cpp \
  -DGGML_VULKAN=ON -DGGML_CUDA=OFF -DBUILD_SHARED_LIBS=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DWHISPER_BUILD_TESTS=OFF -DWHISPER_BUILD_EXAMPLES=ON
```
Expected: configuration succeeds and the output includes a Vulkan line such as `-- Vulkan found` / `-- Including Vulkan backend`. (The previous CPU-only build dir is reused and reconfigured.)

- [ ] **Step 4: Build (Vulkan shader generation makes this slower than the CPU build)**

Run:
```bash
cmake --build whisper.cpp/build -j"$(nproc)"
```
Expected: exit 0.

- [ ] **Step 5: Verify the Vulkan library was produced**

Run:
```bash
ls -1 whisper.cpp/build/ggml/src/ggml-vulkan/libggml-vulkan.so* whisper.cpp/build/ggml/src/libggml-cpu.so*
```
Expected: both `libggml-vulkan.so*` (new — note it lands in a `ggml-vulkan/` subdir) and `libggml-cpu.so*` (CPU fallback retained) are listed. `libggml.so` carries `NEEDED libggml-vulkan.so.0` plus a RUNPATH into that subdir, so any consumer of `libggml.so` gets the GPU backend automatically.

- [ ] **Step 6: Verify the Radeon device is detected at runtime**

Run:
```bash
whisper.cpp/build/bin/whisper-cli \
  -m ~/.local/share/sasayaku/models/ggml-large-v3-turbo.bin \
  -f whisper.cpp/samples/jfk.wav 2>&1 | grep -i "vulkan\|radeon"
```
Expected: a line like `ggml_vulkan: Found 1 Vulkan devices:` and the device name containing `Radeon RX 9060 XT` (or `AMD ... RADV`). If empty, the GPU is not being seen — stop and investigate (driver/Mesa) before proceeding; the CPU build still works as fallback.

*(No commit — only build artifacts changed, which are gitignored.)*

---

## Task 2: Link the Vulkan library into the Sasayaku build

**Files:**
- Modify: `meson.build` (the `whisper_lib` dependency block, ~lines 34–58).

**Interfaces:**
- Consumes: `libggml-vulkan.so` from Task 1.
- Produces: `sasayaku-daemon` linked against the Vulkan backend with a runtime rpath that resolves it.

> **Implementation note (from execution):** the Vulkan lib is in the `ggml-vulkan/` subdir, so define `ggml_vulkan_libdir = ggml_libdir / 'ggml-vulkan'` and pass that to `find_library` and the rpath (not `ggml_libdir`). The code below uses that variable.

- [ ] **Step 1: Add `ggml-vulkan` to the whisper dependency block**

In `meson.build`, find the block that finds the whisper/ggml libraries and add the Vulkan library. Change:
```meson
whisper_found = cpp_compiler.find_library('whisper', dirs: [whisper_libdir], required: false)
ggml_found = cpp_compiler.find_library('ggml', dirs: [ggml_libdir], required: false)
ggml_base_found = cpp_compiler.find_library('ggml-base', dirs: [ggml_libdir], required: false)
ggml_cpu_found = cpp_compiler.find_library('ggml-cpu', dirs: [ggml_libdir], required: false)

if whisper_found.found()
  # Bundle whisper + ggml and bake in an rpath so the daemon finds the .so's at runtime.
  whisper_lib = declare_dependency(
    dependencies: [whisper_found, ggml_found, ggml_base_found, ggml_cpu_found],
    include_directories: whisper_include,
    link_args: [
      '-Wl,-rpath,' + whisper_libdir,
      '-Wl,-rpath,' + ggml_libdir,
    ],
  )
```
to:
```meson
whisper_found = cpp_compiler.find_library('whisper', dirs: [whisper_libdir], required: false)
ggml_found = cpp_compiler.find_library('ggml', dirs: [ggml_libdir], required: false)
ggml_base_found = cpp_compiler.find_library('ggml-base', dirs: [ggml_libdir], required: false)
ggml_cpu_found = cpp_compiler.find_library('ggml-cpu', dirs: [ggml_libdir], required: false)
# Optional GPU backend; present only when whisper.cpp was built with -DGGML_VULKAN=ON.
ggml_vulkan_libdir = ggml_libdir / 'ggml-vulkan'
ggml_vulkan_found = cpp_compiler.find_library('ggml-vulkan', dirs: [ggml_vulkan_libdir], required: false)

whisper_deps = [whisper_found, ggml_found, ggml_base_found, ggml_cpu_found]
whisper_rpaths = ['-Wl,-rpath,' + whisper_libdir, '-Wl,-rpath,' + ggml_libdir]
if ggml_vulkan_found.found()
  whisper_deps += ggml_vulkan_found
  whisper_rpaths += '-Wl,-rpath,' + ggml_vulkan_libdir
endif

if whisper_found.found()
  # Bundle whisper + ggml and bake in an rpath so the daemon finds the .so's at runtime.
  whisper_lib = declare_dependency(
    dependencies: whisper_deps,
    include_directories: whisper_include,
    link_args: [
      '-Wl,-rpath,' + whisper_libdir,
      '-Wl,-rpath,' + ggml_libdir,
    ],
  )
```

- [ ] **Step 2: Reconfigure and confirm Meson finds the Vulkan library**

Run:
```bash
meson setup --reconfigure build 2>&1 | grep -i "ggml-vulkan"
```
Expected: `Library ggml-vulkan found: YES`.

- [ ] **Step 3: Rebuild the app**

Run:
```bash
meson compile -C build 2>&1 | tail -5
```
Expected: exit 0; daemon and cli relink.

- [ ] **Step 4: Verify the daemon resolves the Vulkan library at runtime**

Run:
```bash
ldd build/src/sasayaku-daemon | grep -E "ggml-vulkan|vulkan"
```
Expected: `libggml-vulkan.so.0 => .../whisper.cpp/build/ggml/src/libggml-vulkan.so.0` resolves (no "not found").

- [ ] **Step 5: Commit the Meson change**

Run:
```bash
git add meson.build
git commit -m "$(cat <<'EOF'
Link Vulkan ggml backend into the build

find_library picks up libggml-vulkan when whisper.cpp is built with
GGML_VULKAN=ON; CPU backend stays linked as fallback.

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
EOF
)"
```
Expected: commit succeeds.

---

## Task 3: Verify GPU speedup end-to-end and update build docs

**Files:**
- Modify: `README.md` (Linux build section).
- Modify: `QUICKSTART.md` (if it duplicates the whisper build command).

**Interfaces:**
- Consumes: `whisper-cli` (Task 1), rebuilt daemon (Task 2).
- Produces: documented, verified Vulkan build path.

- [ ] **Step 1: Benchmark CPU vs Vulkan on the same model + sample**

Run (CPU via `-ng` = no GPU, then GPU default):
```bash
echo "== CPU ==" && whisper.cpp/build/bin/whisper-cli -ng \
  -m ~/.local/share/sasayaku/models/ggml-large-v3-turbo.bin \
  -f whisper.cpp/samples/jfk.wav 2>&1 | grep -iE "total time|encode time"
echo "== VULKAN ==" && whisper.cpp/build/bin/whisper-cli \
  -m ~/.local/share/sasayaku/models/ggml-large-v3-turbo.bin \
  -f whisper.cpp/samples/jfk.wav 2>&1 | grep -iE "total time|encode time"
```
Expected: the `VULKAN` `total time` is substantially lower than `CPU` (large-v3-turbo on a discrete Radeon should be several times faster). Record both numbers in the commit message.

- [ ] **Step 2: Confirm the daemon itself uses the GPU**

Run (start the daemon briefly and inspect its log for the Vulkan device):
```bash
timeout 5 ./build/src/sasayaku-daemon 2>&1 | grep -iE "vulkan|radeon|gpu" || echo "no GPU log at startup (device init happens on first transcription)"
```
Expected: either a Vulkan/Radeon line, or the fallback note (whisper initializes the backend on model load; the benchmark in Step 1 is the authoritative proof). Do not treat the fallback note as failure if Step 1 showed the device.

- [ ] **Step 3: Update the README Linux build instructions to Vulkan**

In `README.md`, in the Linux build section, replace the whisper build command:
```bash
cmake .. -DGGML_CUDA=ON -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
```
with:
```bash
# Build whisper.cpp with the Vulkan GPU backend (AMD/Intel/NVIDIA via Mesa/loader)
cmake .. -DGGML_VULKAN=ON -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
```
and add `glslc` (or `shaderc`), `libvulkan-dev`, and `mesa-vulkan-drivers` to the Linux dependency `apt install` list in that section.

- [ ] **Step 4: Mirror the change in QUICKSTART.md if present**

Run:
```bash
grep -n "GGML_CUDA" QUICKSTART.md || echo "no CUDA build command in QUICKSTART.md — skip"
```
If found, apply the same `GGML_CUDA=ON` → `GGML_VULKAN=ON` replacement there.

- [ ] **Step 5: Commit the docs**

Run:
```bash
git add README.md QUICKSTART.md
git commit -m "$(cat <<'EOF'
Document Vulkan whisper build for Linux

Switch the Linux whisper.cpp build to GGML_VULKAN=ON and list the
Vulkan/shaderc dependencies. Benchmark (large-v3-turbo, jfk.wav):
CPU <X>s vs Vulkan <Y>s.

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
EOF
)"
```
Replace `<X>`/`<Y>` with the measured times from Step 1. Expected: commit succeeds.

---

## Self-Review

- **Spec coverage:** Implements spec §8 (Vulkan workstream): rebuild with `GGML_VULKAN=ON` (Task 1), update Meson `find_library`/rpath (Task 2), runtime selection via existing `use_gpu` (verified Task 3), CPU fallback retained (Global Constraints + Task 1 Step 5). Build/packaging doc update per §9 (Task 3).
- **Placeholders:** `<X>`/`<Y>` in Task 3 Step 5 are intentional measured-value fill-ins, instructed explicitly — not unspecified work.
- **Type consistency:** Meson identifiers (`whisper_libdir`, `ggml_libdir`, `whisper_deps`, `ggml_vulkan_found`, `whisper_lib`) match the existing block being edited.
