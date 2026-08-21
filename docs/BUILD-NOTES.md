# Build, flash, and console notes (Windows)

Verified 2026-08-21 on e² studio 25.10.0 (2025-10), FSP 6.3.1 + 6.5.1 packs,
LLVM ATfE 21.1.1, J-Link V9.38a, EK-RA8P1.

## Headless e² studio build

```powershell
$env:PATH = 'C:\Renesas\e2_studio\toolchains\llvm_arm\ATfE-21.1.1-Windows-x86_64\bin;' + $env:PATH
& C:\Renesas\e2_studio\eclipse\e2studioc.exe -nosplash --launcher.suppressErrors `
    -application org.eclipse.cdt.managedbuilder.core.headlessbuild `
    -data <workspace-dir> -import <project-dir> -build '<project-name>/Debug'
```

- The FSP (DDSC) builder runs automatically and generates `ra_gen/`/`ra_cfg/`.
- clang must be on PATH — e² studio does not export its own toolchain dir headlessly.
- **Keep project paths short.** GNU make fails ("No rule to make target") on
  paths > 260 chars; the deep TFLM/CMSIS trees inside `ra/` hit this easily.
  `git config core.longpaths true` is also required to check out ra-fsp-examples.

## Migrating a Renesas example project to the installed FSP/toolchain

Renesas examples pin exact versions. Three edits make them build here:

1. `configuration.xml`: replace `+fsp.<old>` → `+fsp.<installed>`,
   `version="<old>"` → `version="<installed>"`, and
   `<option key="#FSPVersion#" value="..."/>`.
2. `.cproject`: `<option id="toolchain.version" value="18.1.3"/>` → `21.1.1`
   (else headless build errors "Toolchain … not currently available").
3. FSP 6.0.0 → 6.3.1 specifically: `rm_ethosu.c` now needs
   `BSP_CFG_OPTION_SETTING_OFS2`. In `configuration.xml`'s
   `<config id="config.bsp.ra8p1.linker">` block set:
   `option_setting.ofs2 = enabled`, `ofs2.dcdc = enabled`,
   `ofs2.cvm_reset = disabled`, `ofs2.npusa = secure`,
   `ofs2.npupa = unprivilege` (FSP 6.3.1 pack defaults). Headless generation
   does NOT back-fill new pack defaults into old configs; the GUI migration does.

Known quirks of the r11an0995 `image_classification_ethosu_mobilenet_v1` project:
- Its **Release** config references a nonexistent `DualCore_Solution` smart
  bundle — build **Debug** only (or delete the SmartBundle macro).
- Its builder buildPath references a stale project name `start_with_imaging`
  (harmless).

## Flash + run over J-Link (no IDE)

The EK-RA8P1 on-board debugger enumerates as "J-Link OB-RA4M2". Device name in
the SEGGER database is **`R7KA8P1KF_CPU0`** (Cortex-M85; `_CPU1` is the M33).
Needs a recent J-Link (V9.x knows RA8P1; V8.74 does not).

```powershell
# commands.txt:  r / h / loadfile <path>.elf / r / g / q
& 'C:\Program Files\SEGGER\JLink\JLink.exe' -USB <OB-serial> `
    -device R7KA8P1KF_CPU0 -if SWD -speed 4000 -CommandFile commands.txt
```

MRAM programs at ~127 KB/s; option bytes are handled by the flashloader.

## Phase 1 additions (Ethernet merge)

- `r_layer3_switch.h` (FSP 6.3.1 and 6.5.1) is not C++-clean: clang hard-errors
  (C++ DR2229) on `volatile uintN_t : n;` anonymous bit-fields and on tagged
  struct definitions inside anonymous unions. Patched in-repo (qualifiers
  dropped on reserved padding, tags stripped; neither is referenced anywhere).
  If FSP regeneration ever redeploys this file, re-apply the patch.
- picolibc's TLS `errno` (R_ARM_TLS_IE32 in `bsp_sbrk.o` and TFLM kernels)
  makes lld synthesize a `.got`; once the TCP stack shifted the layout, lld
  failed with "__flash_.got$$ is not contiguous with other relro sections".
  Fix: `-Wl,-z,norelro`. The managed-build plugin silently drops the linker
  "User defined options" field, so the flag is carried as a **tool command
  override** in `.cproject`:
  `<tool command="clang++ --target=arm-none-eabi -Wl,-z,norelro" ... cpp.linker ...>`.
- FSP generates a stub `src/<thread>_entry.cpp` whenever no `<thread>_entry.*`
  exists at `src/` root — keep thread entry files there (`src/net_thread_entry.c`).
- After deleting/moving source files outside the IDE, run `-cleanBuild`
  (stale makefiles reference removed files otherwise).

## Serial console

The r11an0995 demo console is the J-Link OB CDC UART at **230400** 8N1
(not 115200). Periodic output: camera vsync (55 fps), AI pre-processing and
inference time (~1 ms on the NPU), LCD vsync (29 fps).
