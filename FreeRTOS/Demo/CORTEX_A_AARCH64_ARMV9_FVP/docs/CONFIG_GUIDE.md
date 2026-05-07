# Armv9 FreeRTOS Port Configuration Guide

## Overview

The Armv9 port extends the existing `ARM_AARCH64_SRE` port with optional Armv9 features. Each feature is independently configurable via `FreeRTOSConfig.h` defines. All features default to disabled (0), making the port backward-compatible with Armv8 behavior.

---

## Configuration Flags

### SVE2 (Scalable Vector Extension 2)

| Flag | Default | Description |
|------|---------|-------------|
| `configARMV9_SVE2` | 0 | Enable SVE2 register save/restore in context switch (Z0-Z31, P0-P15, FFR) |
| `configARMV9_SSVE` | 0 | Save/restore SVCR for tasks using SME Streaming mode. Requires `configARMV9_SVE2=1` |
| `configARMV9_TASK_VL` | 0 | Save/restore ZCR_EL1 per task (allows different vector lengths per task). Requires `configARMV9_SVE2=1` |

**Port impact (portASM):**
- `configARMV9_SVE2=1`: Adds ~120 lines to portSAVE/RESTORE_CONTEXT (STR/LDR Z0-Z31, P0-P15, FFR with ADDVL/ADDPL frame management)
- `configARMV9_SSVE=1`: Adds ~15 lines (MRS/MSR SVCR, conditional streaming VL handling)
- `configARMV9_TASK_VL=1`: Adds ~4 lines (MRS/MSR ZCR_EL1)

**Per-task memory cost (VL=128):**
- SVE2: 32×16 + 3×16 = 560 bytes (Z-regs + predicate frame)
- Streaming SVE: +8 bytes (SVCR)
- Per-task VL: +8 bytes (ZCR_EL1)

**Prerequisites:**
- Hardware: FEAT_SVE2 implemented (ID_AA64PFR0_EL1.SVE ≥ 1)
- Startup: CPTR_EL3.EZ=1, ZCR_EL2/EL1 configured, CPACR_EL1.ZEN=3
- Task: Must call `portTASK_USES_FLOATING_POINT()` before using SVE2 instructions

---

### PAC (Pointer Authentication)

| Flag | Default | Description |
|------|---------|-------------|
| `configARMV9_PAC` | 0 | Load per-task PAC keys on context switch (APIA, APIB, APDA, APDB key pairs) |
| `configARMV9_PAC_FRAME` | 0 | Sign saved LR in context frame with PACIA; verify with AUTIA on restore. Requires `configARMV9_PAC=1` |

**Port impact:**
- `configARMV9_PAC=1`: port.c stores 4 key pairs (64 bytes) in TCB extension; portASM loads keys on restore (~10 lines)
- `configARMV9_PAC_FRAME=1`: portASM wraps LR save/restore with PACIA/AUTIA (~4 lines)

**Per-task memory cost:**
- PAC keys: 64 bytes (4 × 2 × 8-byte key halves)
- PAC on stack: 0 bytes (signs in-place)

**Prerequisites:**
- Hardware: FEAT_PAuth implemented
- Startup: SCR_EL3.API/APK=1, HCR_EL2.API/APK=1, SCTLR_EL1.EnIA=1
- Compile: `-mbranch-protection=standard` (inserts PACIASP/AUTIASP)

**Security note:** Without `configARMV9_PAC=1`, all tasks share a single global key set. An attacker who compromises one task can forge PAC signatures for any other task. Per-task keys provide isolation.

---

### MTE (Memory Tagging Extension)

| Flag | Default | Description |
|------|---------|-------------|
| `configARMV9_MTE_HEAP` | 0 | Wrap pvPortMalloc/vPortFree with IRG/STG tagging. Returns tagged pointers. |
| `configARMV9_MTE_STACK` | 0 | Tag task stack memory on creation with a random tag per task |

**Port impact:**
- `configARMV9_MTE_HEAP=1`: portmacro.h defines pvPortMallocTagged/vPortFreeTagged wrappers (~20 lines)
- `configARMV9_MTE_STACK=1`: port.c tags stack region during pxPortInitialiseStack (~10 lines)

**Per-task memory cost:**
- Heap: 0 (tagging is metadata, not data)
- Stack: 0 (tagging is metadata, not data)

**Runtime cost:**
- Heap: IRG (1 cycle) + STG loop (1 cycle per 16 bytes) per allocation
- Stack: STG loop at task creation only

**Prerequisites:**
- Hardware: FEAT_MTE2 implemented
- Startup: SCTLR_EL1.ATA=1, GCR_EL1 configured, TCR_EL1.TBI0=1
- MMU: Pages mapped as Tagged Normal (MAIR outer≠0, inner=0b0000)
- FVP: `bp.dram_metadata.is_enabled=1`

---

### BTI (Branch Target Identification)

| Flag | Default | Description |
|------|---------|-------------|
| `configARMV9_BTI` | 0 | Ensure all port assembly entry points have BTI landing pads; propagate compile flags |

**Port impact:**
- `configARMV9_BTI=1`: portASM adds `BTI C` or `BTI J` at all indirect branch targets (~5 lines); portmacro.h adds `-mbranch-protection=standard` to required CFLAGS

**Per-task memory cost:** 0

**Runtime cost:** 0 (BTI instructions are NOPs on non-GP pages; on GP pages they're checked by hardware at zero cycle cost)

**Prerequisites:**
- Hardware: FEAT_BTI implemented
- MMU: Pages mapped with GP (Guarded Page) bit set
- Compile: `-mbranch-protection=standard`

---

---

### RME (Realm Management Extension)

| Flag | Default | Description |
|------|---------|-------------|
| `configARMV9_RME` | 0 | Enable Realm Management — GPT configuration, granule transitions |
| `configARMV9_RME_EL2` | 0 | FreeRTOS operates at EL2 as hypervisor/RMM, managing realms for guest VMs. Requires `configARMV9_RME=1` |

**Port impact:**
- `configARMV9_RME=1`: port.c adds GPT (Granule Protection Table) initialization and granule state transition APIs; startup configures GPCCR_EL3/GPTBR_EL3
- `configARMV9_RME_EL2=1`: Port operates at EL2 instead of EL1. Context switch uses EL2 system registers (SPSR_EL2, ELR_EL2). Stage-2 page table management per VM/realm.

**Per-task memory cost:**
- RME: 0 (GPT is system-wide, not per-task)
- RME_EL2: Per-VM state (VTTBR, VMID) stored in task/VM control block

**Runtime cost:**
- Granule transitions: TLBI + DSB per transition (expensive, infrequent)
- EL2 operation: Stage-2 walk on every guest memory access (hardware cost)

**Prerequisites:**
- Hardware: FEAT_RME implemented (ID_AA64PFR0_EL1.RME ≥ 1)
- Execution level: EL3 for GPT setup; EL2 for realm/VM management
- Firmware: Secure Monitor at EL3 delegates RME to EL2

**Use cases:**
- FreeRTOS as Type-1 hypervisor at EL2, hosting guest VMs in realms
- Confidential computing: guest data isolated from hypervisor via hardware enforcement
- Mixed-criticality: safety-certified RTOS tasks + untrusted Linux guest in separate realms

**Architectural scope:**
- GPT (Granule Protection Table): assigns each physical page to a security world (Normal, Secure, Realm, Root)
- RIPAS (Realm IPA State): tracks per-page state within a realm
- RMI (Realm Management Interface): SMC-based API between RMM and Monitor
- RSI (Realm Services Interface): guest-to-RMM communication

---

## Configuration Profiles

### Minimal (Armv8 compatible)
```c
/* All Armv9 features disabled — identical to ARM_AARCH64_SRE */
#define configARMV9_SVE2                  0
#define configARMV9_PAC                   0
#define configARMV9_MTE_HEAP              0
#define configARMV9_MTE_STACK             0
#define configARMV9_BTI                   0
```

### Security-focused (no SVE2 overhead)
```c
#define configARMV9_SVE2                  0
#define configARMV9_PAC                   1
#define configARMV9_PAC_FRAME     1
#define configARMV9_MTE_HEAP              1
#define configARMV9_MTE_STACK             1
#define configARMV9_BTI                   1
```

### Compute-focused (SVE2, minimal security)
```c
#define configARMV9_SVE2                  1
#define configARMV9_PAC                   0
#define configARMV9_MTE_HEAP              0
#define configARMV9_MTE_STACK             0
#define configARMV9_BTI                   1
```

### Full Armv9 (all features)
```c
#define configARMV9_SVE2                  1
#define configARMV9_SSVE         1
#define configARMV9_TASK_VL           0  /* typically not needed */
#define configARMV9_PAC                   1
#define configARMV9_PAC_FRAME     1
#define configARMV9_MTE_HEAP              1
#define configARMV9_MTE_STACK             1
#define configARMV9_BTI                   1
```

---

## Test Matrix

Each flag doubles the test space. Recommended minimum test configurations:

| Config | SVE2 | PAC | MTE_HEAP | MTE_STACK | BTI | Purpose |
|--------|------|-----|----------|-----------|-----|---------|
| Armv8 baseline | 0 | 0 | 0 | 0 | 0 | Regression |
| SVE2 only | 1 | 0 | 0 | 0 | 0 | Vector compute |
| Security only | 0 | 1 | 1 | 1 | 1 | Security hardening |
| Full Armv9 | 1 | 1 | 1 | 1 | 1 | Complete feature set |

---

## Current Implementation Status

| Flag | Implemented | Tested |
|------|-------------|--------|
| `configARMV9_SVE2` | ✅ Yes | ✅ 2000 preemptions verified |
| `configARMV9_SSVE` | ❌ No | — |
| `configARMV9_TASK_VL` | ❌ No | — |
| `configARMV9_PAC` | ❌ No (global key only) | — |
| `configARMV9_PAC_FRAME` | ❌ No | — |
| `configARMV9_MTE_HEAP` | ❌ No (design exists) | — |
| `configARMV9_MTE_STACK` | ❌ No | — |
| `configARMV9_BTI` | ⚠️ Partial (compile flags yes, vector BTI incomplete) | — |
| `configARMV9_RME` | ❌ No | — |
| `configARMV9_RME_EL2` | ❌ No | — |
