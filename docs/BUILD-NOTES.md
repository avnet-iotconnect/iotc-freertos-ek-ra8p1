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

## OSPI flash / model store (Phase 5)

- **The MX25LW51245G stays in octal-DDR mode across MCU resets** once any
  firmware (e.g. the factory demo) switches it - only a power cycle or a
  RESET# pulse restores 1S SPI mode. Symptom: every r_ospi_b op returns
  FSP_ERR_DEVICE_BUSY (40002) and XIP reads are 0xFF. `iotc_fs_init()`
  pulses P106 (om_0_reset) before opening the OSPI. Keep this.
- `rm_littlefs_spi_flash.c` is patched in-repo to invalidate D-cache before
  XIP reads (the port memcpy's from the cacheable XIP window while the OSPI
  driver programs the array behind the cache).
- **Open issue:** `lfs_format` on the OSPI volume returns LFS_ERR_NOSPC with
  both the donor geometry (read 1 / prog 4) and 64/64; every block appears
  "bad" to littlefs. The PKCS#11 credential store depends on littlefs, so
  device credentials currently re-provision at each boot (works; does not
  persist). The model store does NOT use littlefs - it writes a raw OSPI
  slot at +56 MB (verified across reboots).
- Serial-flash writes must go through `g_ospi0.p_api` in <=64-byte chunks
  (combination-buffer size) with status polling between chunks.

## Ethernet bring-up findings (Phase 1 completion)

Five independent faults, any one of which kills networking. All fixed in-repo:

1. **Clock tree**: ESWCLK + ESWPHYCLK must be enabled (PLL1P); ETHPHYCLK
   must be **exactly 25 MHz** — here sourced from the otherwise-unused
   PLL1R (400 MHz) ÷16, so camera/display/SDRAM clocks stay untouched.
   Without these the ESWM is unclocked: MDIO reads zeros, link error 4001.
2. **P708 (ETHERNET_RST)** must be GPIO output high — the GPY111 is
   otherwise held in reset (PHY ID reads 0000; healthy = d565:a401).
3. **D-cache must be disabled** (`config.bsp.fsp.dcache`): FSP
   r_rmac/r_layer3_switch do no cache maintenance on their DMA
   descriptors; every Renesas EK-RA8P1 ethernet example ships cache-off.
   Vision cost: preprocess 2→10 ms; camera/NPU/LCD rates unchanged.
   (A future refinement: nocache-section placement for the ether arrays;
   note lld INSERT loses input-section pattern matching to the main
   script, so this needs the FSP linker-section-mapping mechanism.)
4. **xApplicationGetRandomNumber**: the linked weak default returns
   pdFALSE, which aborts DHCP with no error. Strong override in
   `src/net_thread_entry.c` (TODO: RSIP TRNG).
5. **Vendor spin-bugs** (patched in-repo, look for "patched" comments):
   `rm_freertos_plus_tcp/NetworkInterface.c` treats NO_DATA as a received
   frame (stale recycled buffer → infinite zero-byte events);
   `FreeRTOS_DHCP.c` vDHCPProcess re-PEEKs an unconsumed message forever.
   Both starve everything below the IP task's priority the moment real
   traffic arrives — symptom is the console dying ~11 s after boot while
   the scheduler stays alive.

Console: `print_to_console` is now mutex-serialized with a bounded TX
wait; concurrent prints previously hit APP_ERROR_TRAP. MAC is the
locally-administered 02:8a:9b:71:04:d2 (set in configuration.xml AND
net_thread_entry.c). Note many home routers isolate Wi-Fi from wired —
test connectivity from the board side (gateway ping), not from a Wi-Fi PC.

## Serial console

The r11an0995 demo console is the J-Link OB CDC UART at **230400** 8N1
(not 115200). Periodic output: camera vsync (55 fps), AI pre-processing and
inference time (~1 ms on the NPU), LCD vsync (29 fps).
