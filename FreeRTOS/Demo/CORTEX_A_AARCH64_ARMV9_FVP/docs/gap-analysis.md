# FreeRTOS Armv9 / Cortex-A320 Gap Analysis

## Source Examined

- Repository: https://github.com/FreeRTOS/FreeRTOS (cloned recursively)
- Kernel submodule: FreeRTOS-Kernel V11.3.0 (`fcc665325`)
- Ports examined:
  - `portable/GCC/ARM_AARCH64_SRE` — current recommended AArch64 port (system register GIC)
  - `portable/GCC/ARM_AARCH64` — AArch64 port (memory-mapped GIC)
  - `portable/GCC/ARM_CA53_64_BIT_SRE` — deprecated, redirects to `ARM_AARCH64_SRE`
  - `portable/GCC/ARM_CA53_64_BIT` — deprecated, redirects to `ARM_AARCH64`
- Heap implementations: `portable/MemMang/heap_1.c` through `heap_5.c`
- Demo: `Demo/CORTEX_A53_64-bit_UltraScale_MPSoC` (only upstream AArch64 demo)

## Executive Summary

FreeRTOS has **zero Armv9 awareness**. The AArch64 port targets Armv8-A exclusively. There is no code, configuration option, or documentation referencing SVE/SVE2, MTE, PAC, BTI, or any Armv9 system register. Every gap identified below requires new work.

---

## Gap 1: SVE2 Context Save/Restore

**Current state**: The port saves/restores 32 × 128-bit NEON/FP registers (Q0–Q31) plus FPSR/FPCR. This is controlled by `ullPortTaskHasFPUContext` and `configUSE_TASK_FPU_SUPPORT` (values 1 or 2).

**Gap**: SVE2 extends the register file to scalable vector registers Z0–Z31 (each up to 2048 bits, implementation-defined), plus:
- 16 predicate registers P0–P15 (each VL/8 bits)
- FFR (First Fault Register)
- ZCR_EL1 (vector length control register)

None of these are saved or restored. If a task uses SVE2 instructions, its vector state will be silently corrupted on context switch.

**What's needed**:
- Read `ZCR_EL1.LEN` at boot to determine the implementation's vector length (VL)
- Dynamically size the SVE context frame on the stack: `(32 × VL) + (16 × VL/8) + FFR + ZCR_EL1`
- Add SVE save/restore assembly using `STR Z` / `LDR Z` and `STR P` / `LDR P` instructions
- Add a `configUSE_TASK_SVE_SUPPORT` option (analogous to `configUSE_TASK_FPU_SUPPORT`) to allow lazy or eager SVE context switching
- On Cortex-A320, VL is expected to be 128 bits (matching NEON width), so the overhead is bounded, but the port must handle the general case

**Complexity**: High — requires new assembly in `portASM.S`, changes to `pxPortInitialiseStack()`, and a new per-task SVE context indicator.

---

## Gap 2: MTE (Memory Tagging Extension) Support

**Current state**: The heap implementations (`heap_1.c` through `heap_5.c`) perform standard pointer arithmetic with no awareness of memory tags. `heap_4.c` uses a first-fit free-list allocator with 16-byte alignment (`portBYTE_ALIGNMENT`). No system registers related to MTE are configured anywhere.

**Gap**: MTE requires:
1. **System register setup** — `SCTLR_EL1.ATA` and `SCTLR_EL1.ATA0` must be set to enable tag checking. `TCR_EL1.TBI0/TBI1` must enable top-byte-ignore. `GCR_EL1` and `RGSR_EL1` configure random tag generation.
2. **Heap tagging** — `pvPortMalloc()` must assign a tag to each allocated block using `IRG` (insert random tag) and `STG`/`ST2G` (store allocation tags). `vPortFree()` must clear or re-tag freed memory.
3. **Granule alignment** — MTE operates on 16-byte granules. `heap_4.c` already uses 16-byte alignment, which is compatible, but the block header (`BlockLink_t`) sits inline and must not share a granule with user data.
4. **Stack tagging** (optional) — task stacks could be tagged to detect stack buffer overflows.

**What's needed**:
- Startup code to configure MTE system registers at EL1
- A new heap implementation (e.g., `heap_4_mte.c`) or hooks in `heap_4.c` that call `IRG`/`STG` on allocate and `STG` with a zero/different tag on free
- Ensure `BlockLink_t` header and user payload are in separate 16-byte granules (already likely given alignment, but must be verified)
- A `configUSE_MTE` option to gate the feature

**Complexity**: Medium — the system register setup is straightforward; the heap changes are localized but require careful granule-boundary handling.

---

## Gap 3: PAC (Pointer Authentication) Support

**Current state**: The port saves/restores `X30` (link register) as a plain 64-bit value. The initial stack frame sets `X30` to `0x00`. No PAC keys are configured, and no `PACIA`/`AUTIA` instructions are used. The compiler flag `-mbranch-protection=standard` is not referenced anywhere.

**Gap**: When PAC is enabled (via `-mbranch-protection=standard`), the compiler inserts `PACIASP` at function entry and `AUTIASP` at function return. This requires:
1. **PAC key initialization** — `APIAKeyLo_EL1` / `APIAKeyHi_EL1` (and optionally B-key) must be programmed at boot. `SCTLR_EL1.EnIA` must be set.
2. **Context switch** — If per-task PAC keys are desired (for isolation), the keys must be saved/restored per task. If a single system-wide key is used, no context switch change is needed.
3. **Initial stack frame** — The `X30` value pushed by `pxPortInitialiseStack()` will not have a valid PAC signature. When the task first returns through `AUTIASP`, the authentication will fail. The initial LR must either be signed with `PACIA`, or the task entry must be structured to avoid an `AUTIASP` on the first return.
4. **Exception return** — `ERET` implicitly strips PAC from `ELR_ELx` on some configurations. The port's `ERET` usage needs validation.

**What's needed**:
- Boot code to program PAC keys and enable `SCTLR_EL1.EnIA`
- Decide on per-task vs. system-wide keys (system-wide is simpler and sufficient for our demo)
- Fix `pxPortInitialiseStack()` to sign the initial `X30` or structure task entry to avoid auth failure
- Validate that the context switch save/restore of `X30` preserves the PAC bits in the upper bytes

**Complexity**: Medium — system-wide key is simple; per-task keys add context switch overhead.

---

## Gap 4: BTI (Branch Target Identification) Support

**Current state**: No BTI landing pads (`BTI C`, `BTI J`, `BTI JC`) are present in `portASM.S`. The assembly functions (`FreeRTOS_SWI_Handler`, `FreeRTOS_IRQ_Handler`, `vPortRestoreTaskContext`) do not begin with BTI instructions. The `SCTLR_EL1.BT0/BT1` bits are not configured.

**Gap**: When BTI is enabled (via `-mbranch-protection=standard`), indirect branches (BLR, BR) must land on a `BTI` instruction. The compiler handles this for C code, but hand-written assembly must be manually annotated.

**What's needed**:
- Add `BTI C` or `BTI JC` landing pads at the entry of every assembly function that can be reached via indirect branch: `FreeRTOS_SWI_Handler`, `FreeRTOS_IRQ_Handler`, `vPortRestoreTaskContext`, `vApplicationIRQHandler`
- Enable `SCTLR_EL1.BT0` in startup code
- Ensure the vector table entries (which are reached via exception, not branch) are compatible — `ERET` to a BTI-enabled page requires a `BTI` at the target, but exception entry vectors are exempt from BTI checks
- Compile assembly files with `.note.gnu.property` section marking BTI compatibility

**Complexity**: Low — primarily annotation of existing assembly entry points.

---

## Gap 5: Exception Level Support (EL1 vs. EL3)

**Current state**: The port defaults to EL3 operation. EL1 is supported only when the `GUEST` preprocessor macro is defined. There is no EL2 support. The port uses `SPSR_EL3`/`ELR_EL3` by default, switching to `SPSR_EL1`/`ELR_EL1` under `GUEST`.

**Gap**: On a real Cortex-A320 system with trusted firmware (TF-A), FreeRTOS would typically run at EL1 (normal world) or EL1 under S-EL2 (secure world). The `GUEST` macro enables EL1 but the naming is misleading and the EL1 path has not been validated for Armv9 features. MTE, PAC, and BTI system registers are EL1-accessible but must be enabled by higher exception levels first.

**What's needed**:
- Validate and potentially rename the `GUEST` path to a clearer `configRUN_AT_EL1` option
- Ensure startup code (or trusted firmware) enables MTE/PAC/BTI access from EL1 via `HCR_EL2` and `SCR_EL3` trap bits
- For our QEMU-based development, running at EL1 with `-machine virt` is the practical path

**Complexity**: Low-Medium — mostly configuration and validation.

---

## Gap 6: GIC and Timer for Cortex-A320

**Current state**: The `ARM_AARCH64_SRE` port uses GICv3 system register interface (`ICC_PMR_EL1`, `ICC_IAR1_EL1`, `ICC_EOIR1_EL1`, `ICC_RPR_EL1`). Timer setup is delegated to the application via `configSETUP_TICK_INTERRUPT()`.

**Gap**: The Cortex-A320 uses GICv3 (or GIC-600AE in some configurations) and the standard AArch64 generic timer. The existing GIC system register interface should be compatible. However:
- The specific GIC distributor/redistributor base addresses are platform-dependent
- Timer configuration (CNTFRQ, CNTP_CTL_EL0 vs. CNTV_CTL_EL0) needs to match the platform
- No existing demo provides a Cortex-A320 BSP

**What's needed**:
- A platform BSP (`platform.h` / `platform.c`) with Cortex-A320 memory map
- Timer setup implementation for `configSETUP_TICK_INTERRUPT()`
- GIC distributor initialization (the port only handles the CPU interface)
- For QEMU development, the `virt` machine's GIC and timer are well-documented

**Complexity**: Medium — standard BSP work, but no existing reference for Cortex-A320.

---

## Gap 7: No Cortex-A320 or Armv9 Demo

**Current state**: The only upstream AArch64 demo is `CORTEX_A53_64-bit_UltraScale_MPSoC`, which targets a Xilinx Zynq UltraScale+ with Cortex-A53. A partner-supported demo exists for TI AM64 (also Cortex-A53). No Armv9 demo exists anywhere in the repository.

**What's needed**:
- A complete demo project targeting Cortex-A320 (or Armv9 QEMU virt machine)
- Startup assembly (reset vector, MMU/cache setup, EL transition)
- Linker script for the target memory map
- FreeRTOSConfig.h tuned for the platform

**Complexity**: High — this is the integration of all the above gaps into a working system.

---

## Summary Table

| Gap | Feature | Current State | Effort | Priority |
|-----|---------|--------------|--------|----------|
| 1 | SVE2 context switch | Not implemented — only NEON/FP Q0–Q31 saved | High | P0 — required for SVE2 workloads |
| 2 | MTE heap tagging | No MTE awareness in heap or system registers | Medium | P0 — core security demo |
| 3 | PAC support | No key setup, no signed LR in initial stack | Medium | P1 — security demo |
| 4 | BTI landing pads | No BTI instructions in assembly | Low | P1 — security demo |
| 5 | EL1 operation | Supported via `GUEST` macro but not validated for Armv9 | Low-Medium | P0 — required for realistic deployment |
| 6 | GIC/Timer BSP | GIC SRE compatible, but no Cortex-A320 BSP | Medium | P0 — required to boot |
| 7 | Demo/integration | No Armv9 demo exists | High | P0 — the deliverable |

## Recommended Approach

1. **Start with QEMU `virt` machine at EL1** — get FreeRTOS booting on AArch64 QEMU with the existing `ARM_AARCH64_SRE` port using `GUEST` mode. This validates the base port before adding Armv9 features.
2. **Add BTI landing pads** (Gap 4) — lowest effort, highest compatibility impact when compiling with `-mbranch-protection=standard`.
3. **Add PAC key setup and initial stack fix** (Gap 3) — enables PAC demo without per-task key overhead.
4. **Add MTE system register setup and heap tagging** (Gap 2) — enables the security demo.
5. **Add SVE2 context save/restore** (Gap 1) — enables the image processing workload.
6. **Build the integrated demo** (Gap 7) — ties everything together with the processing pipeline.
