# Deliverables — FreeRTOS Armv9 Port

## High-Level Integration Plan

### Goal
Deliver a FreeRTOS port that is fully "Armv9 Ready" — supporting all Armv9-A architectural features through configurable flags, validated on ARM FVP, and structured for upstream contribution.

### Deliverable Structure

| Deliverable | Repository | Branch | Description |
|-------------|-----------|--------|-------------|
| Armv9 Port Layer | `rpcme/FreeRTOS-Kernel` | `armv9` | Extended `portable/GCC/ARM_AARCH64_SRE/` with Armv9 config flags |
| Demo Application | `rpcme/FreeRTOS` | `armv9` | 11-option interactive demo + BSP + docs |

### Configuration Flags (10 total)

See `docs/CONFIG_GUIDE.md` for full specification.

| Flag | Feature | Status |
|------|---------|--------|
| `configARMV9_SVE2` | SVE2 context switch | ✅ Implemented |
| `configARMV9_SSVE` | SME Streaming mode | ❌ Not started |
| `configARMV9_TASK_VL` | Per-task vector length | ❌ Not started |
| `configARMV9_PAC` | Per-task PAC keys | ❌ Not started |
| `configARMV9_PAC_FRAME` | Signed context frame | ❌ Not started |
| `configARMV9_MTE_HEAP` | Tagged heap allocations | ❌ Not started (design exists) |
| `configARMV9_MTE_STACK` | Tagged task stacks | ❌ Not started |
| `configARMV9_BTI` | BTI landing pads | ⚠️ Partial |
| `configARMV9_RME` | Realm Management | ❌ Not started |
| `configARMV9_RME_EL2` | EL2 hypervisor mode | ❌ Not started |

---

## Phase Plan

### Phase 1: FVP Bring-up ✅ COMPLETE
- Multi-task preemptive scheduling on FVP
- Timer, GIC, UART working
- Context switch verified

### Phase 2: Security Features Demo ✅ COMPLETE
- PAC fault detection (interactive)
- BTI fault detection (automatic)
- MTE tag check fault (interactive)
- Recoverable fault handler (demo_setjmp)

### Phase 3: SVE2 Demo ✅ COMPLETE
- SVE2 context switch (Z0-Z31, P0-P15, FFR)
- 5 SVE2 algorithm demos (Sobel, Histogram, Blur, SAD, MATCH, Sharpen, Emboss)
- Semihosting I/O + webcam capture pipeline

### Phase 4: Port Completeness (NEXT)
Close all gaps identified in `docs/ARMV9_READINESS_ASSESSMENT.md`:

| Task | Priority | Effort | Dependency |
|------|----------|--------|------------|
| BTI exception vector landing pads | High | Low | None |
| Per-task PAC keys (`configARMV9_PAC`) | High | Medium | TCB extension |
| MTE heap wrapper (`configARMV9_MTE_HEAP`) | High | Low | Design exists |
| MTE stack tagging (`configARMV9_MTE_STACK`) | Medium | Low | None |
| PAC signed context frame (`configARMV9_PAC_FRAME`) | Medium | Low | `configARMV9_PAC` |
| MMU RO/RW separation | Medium | Medium | Linker script changes |
| Streaming SVE (`configARMV9_SSVE`) | Low | Medium | SME hardware understanding |
| Per-task VL (`configARMV9_TASK_VL`) | Low | Low | `configARMV9_SVE2` |
| Stack overflow detection | Low | Low | configCHECK_FOR_STACK_OVERFLOW=2 |
| Critical section upstream reconciliation | Medium | Low | Document as platform note |

### Phase 5: RME Integration
- `configARMV9_RME`: GPT initialization, granule state transitions
- `configARMV9_RME_EL2`: Port at EL2, stage-2 page tables, VMID per task
- Requires EL3 Secure Monitor cooperation (ATF or minimal stub)

### Phase 6: Upstream Preparation
- Factor port changes into `#if configUSE_*` guards
- Regression test matrix (4 minimum configurations)
- Upstream PR to `FreeRTOS-Kernel` with documentation
- Demo PR to `FreeRTOS` with FVP BSP

### Phase 7: Hardware Validation (Orion O6)
- Port BSP from FVP to Orion O6 (CIX P1)
- Validate all features on real silicon
- Measure SVE2 speedups at VL=256 (Cortex-A720)

---

## Current File Inventory

### Port Layer (candidates for FreeRTOS-Kernel upstream)
| File | Content |
|------|---------|
| `src/bsp/portASM_fvp.S` | Context switch with SVE2 save/restore |
| `FreeRTOS/.../port.c` | Tick handler fix, uxPortSetInterruptMask fix |
| `src/armv9/pac_init.S` | PAC key programming + SCTLR_EL1.EnIA |
| `src/armv9/mte_init.c` | MTE system register configuration |
| `src/armv9/mmu_init.c` | MMU with GP + Tagged Normal + TBI |

### BSP (FVP-specific)
| File | Content |
|------|---------|
| `src/bsp/fvp_startup.S` | EL3→EL1 drop, CPTR_EL3, ZCR, CNTFRQ |
| `src/bsp/timer.c` | Virtual timer tick |
| `src/bsp/gic.c` | GICv3 redistributor init |
| `src/bsp/uart.c` | PL011 UART driver |
| `src/bsp/semihost.c` | ARM semihosting file I/O |

### Demo Application
| File | Content |
|------|---------|
| `src/demo/main.c` | Entry point, task creation |
| `src/demo/security_demo.c` | Interactive menu + PAC/BTI/MTE demos |
| `src/demo/sve2_demo.c` | SVE2 context switch verification |
| `src/demo/sobel_demo.c` | Sobel edge detection |
| `src/demo/histogram_demo.c` | Histogram with HISTCNT |
| `src/demo/blur_demo.c` | Gaussian blur |
| `src/demo/sad_demo.c` | SAD motion detection |
| `src/demo/match_demo.c` | MATCH byte search |
| `src/demo/filter_demo.c` | Sharpen + Emboss filters |
| `src/demo/demo_setjmp.S` | Fault recovery mechanism |

### Documentation
| File | Content |
|------|---------|
| `docs/ARMV9_READINESS_ASSESSMENT.md` | Feature-by-feature completeness |
| `docs/CONFIG_GUIDE.md` | All 10 configuration flags |
| `docs/MTE_HEAP_DECISION.md` | Design decision: no heap_4 fork |
| `docs/FVP_EXECUTION_STATE.md` | Current demo state + engineering practices |

### Test Infrastructure
| File | Content |
|------|---------|
| `tests/sve_frame_test.S` | Standalone SVE frame save/restore |
| `tests/sve_ctx_test.S` | Full context switch frame layout |
| `capture.sh` | Webcam → PGM capture |
| `run_fvp.sh` | Non-interactive FVP launch |
| `run_fvp_interactive.sh` | Interactive FVP launch |
