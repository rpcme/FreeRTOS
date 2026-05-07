# FreeRTOS Armv9 Readiness Assessment

## Date: 2026-05-07
## Purpose: Evaluate completeness of Armv9-A architecture support in this FreeRTOS port

---

## Top-Level Summary

| Feature | Status | Coverage |
|---------|--------|----------|
| SVE2 (Scalable Vector Extension 2) | ⚠️ Partial | Full context switch, VL-agnostic, lazy save; Streaming SVE mode (SME interaction) not implemented |
| MTE (Memory Tagging Extension) | ⚠️ Partial | System registers + page tables complete; heap integration designed but not wired |
| PAC (Pointer Authentication) | ⚠️ Partial | APIA key + EnIA + compiler integration complete; per-task keys missing |
| BTI (Branch Target Identification) | ⚠️ Partial | GP pages, MMU, compiler landing pads enabled; exception vector stubs missing BTI |
| RME (Realm Management Extension) | ❌ Not implemented | Required for EL2 hypervisor use case; GPT and realm lifecycle not addressed |
| SME (Scalable Matrix Extension) | ❌ Not implemented | Optional in Armv9.0; not addressed |
| ETE/TRBE (Trace) | N/A | Debug infrastructure — RTOS does not interfere |
| BRBE (Branch Record Buffer) | N/A | Profiling infrastructure — RTOS does not interfere |
| GICv3 (Interrupt Controller) | ✅ Implemented | System register interface, PMR-based masking |
| Generic Timer | ✅ Implemented | Virtual timer at EL1, CNTFRQ configured |
| MMU | ⚠️ Partial | Identity map with Tagged Normal + GP; no RO/RW separation |

---

## 1. SVE2 (Scalable Vector Extension 2) — FEAT_SVE2

| Aspect | Status | Detail |
|--------|--------|--------|
| Z-register context switch (Z0-Z31) | ✅ Fully implemented | portSAVE/RESTORE_CONTEXT in portASM_fvp.S |
| P-register context switch (P0-P15) | ✅ Fully implemented | Saved/restored with ADDVL-aligned frame |
| FFR (First Fault Register) | ✅ Fully implemented | RDFFR/WRFFR in context switch |
| ZCR_EL1/EL2 configuration | ✅ Fully implemented | Set in fvp_startup.S |
| CPTR_EL3.EZ trap disable | ✅ Fully implemented | Set in fvp_startup.S |
| CPACR_EL1.ZEN enable | ✅ Fully implemented | Set in fvp_startup.S |
| VL-agnostic context switch | ✅ Fully implemented | Uses ADDVL/ADDPL, works at any VL |
| Lazy save (only when task uses SVE) | ✅ Fully implemented | configUSE_TASK_FPU_SUPPORT=1 |
| SVE2-specific instructions (HISTCNT, MATCH) | ✅ Demonstrated | Demo options 6, 9 |
| Streaming SVE mode (SME interaction) | ❌ Not implemented | FEAT_SME not addressed |

---

## 2. MTE (Memory Tagging Extension) — FEAT_MTE2

| Aspect | Status | Detail |
|--------|--------|--------|
| SCTLR_EL1.ATA/ATA0 enable | ✅ Fully implemented | mte_init.c |
| TCF synchronous fault mode | ✅ Fully implemented | TCF=0b01 in SCTLR_EL1 |
| GCR_EL1 tag exclusion mask | ✅ Fully implemented | Excludes tag 0 |
| RGSR_EL1 random seed | ✅ Fully implemented | Fixed seed for demo |
| TCR_EL1.TBI0 (Top Byte Ignore) | ✅ Fully implemented | Required for tagged pointers |
| Tagged Normal memory (MAIR) | ✅ Fully implemented | MAIR index 2 = 0xF0 |
| IRG/STG/LDG instructions | ✅ Demonstrated | MTE demo (option 3) |
| Heap allocator integration | ⚠️ Partial | Manual tagging demonstrated; pvPortMallocTagged wrapper designed but not integrated into heap_4 |
| Stack tagging | ❌ Not implemented | Task stacks not tagged |
| MTE tag storage (DRAM metadata) | ✅ Configured | FVP bp.dram_metadata.is_enabled=1 |
| Asynchronous tag check mode | ❌ Not implemented | Only synchronous mode used |

---

## 3. PAC (Pointer Authentication) — FEAT_PAuth

| Aspect | Status | Detail |
|--------|--------|--------|
| SCTLR_EL1.EnIA enable | ✅ Fully implemented | pac_init.S |
| SCR_EL3.API/APK (no trap from EL3) | ✅ Fully implemented | fvp_startup.S |
| HCR_EL2.API/APK (no trap from EL2) | ✅ Fully implemented | fvp_startup.S |
| APIA key programming | ✅ Fully implemented | pac_init.S (fixed keys for demo) |
| PACIASP/AUTIASP (compiler-generated) | ✅ Fully implemented | -mbranch-protection=standard |
| PAC in context switch (LR signing) | ⚠️ Implicit | Compiler inserts PACIASP in port functions; PAC bits preserved in saved LR on stack |
| APIB/APDA/APDB/APGA keys | ❌ Not implemented | Only APIA key programmed |
| Per-task PAC keys | ❌ Not implemented | All tasks share same key |
| FEAT_FPAC (faulting PAC) | ⚠️ Platform-dependent | Works on FVP (faults on auth failure); behavior depends on implementation |

---

## 4. BTI (Branch Target Identification) — FEAT_BTI

| Aspect | Status | Detail |
|--------|--------|--------|
| SCTLR_EL1.BT0/BT1 | ⚠️ Implicit | BTI enforcement via GP bit in page tables, not SCTLR |
| GP (Guarded Page) bit in page tables | ✅ Fully implemented | All DRAM pages have GP=1 in mmu_init.c |
| BTI landing pads in assembly | ✅ Fully implemented | `bti c` / `bti j` in all hand-written ASM entry points |
| Compiler BTI generation | ✅ Fully implemented | -mbranch-protection=standard inserts BTI at function entries |
| MMU required for enforcement | ✅ Fully implemented | MMU enabled with identity map |
| BTI in exception vectors | ⚠️ Partial | Vector table entries branch to handlers; handlers have BTI but vector stubs do not (they use direct B instructions which are exempt) |

---

## 5. RME (Realm Management Extension) — FEAT_RME

| Aspect | Status | Detail |
|--------|--------|--------|
| Realm world support | ❌ Not implemented | FreeRTOS runs at EL1 in Normal world only |
| Granule Protection Table (GPT) | ❌ Not implemented | Not relevant for single-world RTOS |
| Realm Management Monitor (RMM) | ❌ Not implemented | Would require EL2 firmware |

**Note**: RME is a hypervisor/firmware feature. A bare-metal RTOS at EL1 is not expected to implement RME itself — it would run *within* a realm managed by higher-privilege software.

---

## 6. ETE/TRBE (Embedded Trace Extension / Trace Buffer Extension) — FEAT_ETE, FEAT_TRBE

| Aspect | Status | Detail |
|--------|--------|--------|
| ETE configuration | ❌ Not implemented | Trace is a debug/profiling feature |
| TRBE buffer management | ❌ Not implemented | Not required for RTOS operation |

**Note**: Trace is a development tool feature, not a runtime requirement. An "Armv9 Ready" RTOS does not need to implement trace — it needs to not *break* trace (i.e., not clobber trace registers during context switch). Our port does not touch ETE/TRBE registers.

---

## 7. BRBE (Branch Record Buffer Extension) — FEAT_BRBE

| Aspect | Status | Detail |
|--------|--------|--------|
| BRBE register save/restore | ❌ Not implemented | Profiling feature, not saved per-task |

**Note**: Same as ETE — profiling infrastructure, not runtime RTOS requirement.

---

## 8. General AArch64 Port Correctness

| Aspect | Status | Detail |
|--------|--------|--------|
| EL1 operation (GUEST mode) | ✅ Fully implemented | All system register access at EL1 |
| GICv3 system register interface | ✅ Fully implemented | ICC_PMR_EL1, ICC_IAR1_EL1, ICC_EOIR1_EL1 |
| Generic timer (virtual) | ✅ Fully implemented | CNTV_TVAL_EL0, CNTV_CTL_EL0 |
| FPU/NEON context switch | ✅ Fully implemented | Subsumed by SVE2 save (Z[127:0] = Q registers) |
| Critical section (PMR-based) | ⚠️ Modified | Removed portENABLE_INTERRUPTS from uxPortSetInterruptMask for FVP compatibility (timer PPI priority 0) |
| configASSERT | ✅ Enabled | With assert_failed handler |
| Stack overflow detection | ❌ Not enabled | configCHECK_FOR_STACK_OVERFLOW=0 |

---

## 9. MMU Configuration

| Aspect | Status | Detail |
|--------|--------|--------|
| Identity map (VA=PA) | ✅ Fully implemented | Level 1 + Level 2 block descriptors |
| Device memory for MMIO | ✅ Fully implemented | 0x00000000-0x3FFFFFFF |
| Normal Tagged memory for DRAM | ✅ Fully implemented | 0x80000000+ with MAIR 0xF0 |
| GP bit on all code pages | ✅ Fully implemented | BTI enforcement |
| TBI0 for MTE pointers | ✅ Fully implemented | TCR_EL1 bit 37 |
| Cache enabled (I+D) | ✅ Fully implemented | SCTLR_EL1 bits 0, 2, 12 |
| Separate RO/RW/XN regions | ❌ Not implemented | All DRAM is RWX+GP (simplified for demo) |

---

## 10. Summary

### Fully Implemented (ready for assertion)
- SVE2 context switching (all registers, VL-agnostic)
- MTE synchronous tag checking (system registers, page tables, TBI)
- PAC return address signing (APIA key, EnIA, compiler integration)
- BTI enforcement (GP pages, MMU, compiler landing pads)
- GICv3 + Generic Timer + FPU/NEON (subsumed by SVE2)

### Partially Implemented (functional but incomplete)
- MTE heap integration (manual tagging works; no automatic pvPortMalloc tagging)
- PAC keys (only APIA; not per-task; fixed value not RNG)
- Critical section mechanism (modified from upstream for FVP timer priority issue)
- MMU permissions (no RO/RW separation — all DRAM is RWX)
- Stack overflow detection (disabled)

### Not Implemented (not required for "Armv9 Ready" RTOS assertion)
- RME (hypervisor/firmware feature — RTOS runs within a realm, doesn't manage them)
- SME (Scalable Matrix Extension — optional, not mandatory in Armv9.0)
- ETE/TRBE/BRBE (debug/profiling infrastructure — RTOS must not break, not implement)
- Asynchronous MTE mode (synchronous is the security-relevant mode)
- Per-task PAC keys (security hardening, not architectural requirement)
- Multiple PAC key algorithms (APIB/APDA/APDB/APGA)

### Gaps to Address Before Assertion
1. **Per-task PAC keys** — Currently all tasks share one key. For a security-credible assertion, each task should have unique keys loaded on context switch.
2. **MTE heap wrapper** — The design exists (documented in MTE_HEAP_DECISION.md) but isn't wired into pvPortMalloc. Should be implemented as the thin wrapper.
3. **Stack overflow detection** — Should enable configCHECK_FOR_STACK_OVERFLOW=2 for production readiness.
4. **MMU RO/RW separation** — Code should be read-only, data read-write. Current all-RWX is a demo shortcut.
5. **Critical section upstream compatibility** — The uxPortSetInterruptMask modification diverges from the upstream port. Should be reconciled or documented as platform-specific.
