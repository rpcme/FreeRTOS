# Project Status — 2026-04-16

## Phase 2: COMPLETE ✅ — FreeRTOS Running on Orion O6

FreeRTOS scheduler running preemptively at EL1 on the CIX P1 (Sky1) Armv9 SoC,
with timer tick interrupts and UART output. Validated on physical Radxa Orion O6 hardware.

### What's Working

- **FreeRTOS scheduler** — preemptive multitasking with context switching
- **EL2→EL1 transition** — UEFI boots at EL2, we drop to EL1 via ERET
- **Virtual timer** — CNTV at 1 GHz, 1 kHz tick rate (PPI 27)
- **GIC-700** — system register interface, redistributor PPI config
- **PL011 UART2** — direct MMIO at 0x040d0000, 115200 baud after ExitBootServices
- **Heartbeat task** — prints incrementing counter every 1 second

### Confirmed Hardware Features (from probe)

| Feature | Value | Status |
|---------|-------|--------|
| CurrentEL | EL2 (UEFI) → EL1 (FreeRTOS) | ✅ |
| HCR_EL2 | 0x88000000 (E2H=0, TGE=1) | nVHE mode |
| MIDR_EL1 | 0x410fd811 (Cortex-A720) | Boot core = CPU10 |
| CNTFRQ_EL0 | 1,000,000,000 Hz | 1 GHz ✅ |
| SVE | PFR0[35:32] = 1 | Present ✅ |
| MTE | PFR1[11:8] = 3 | MTE3 (full) ✅ |
| BTI | PFR1[3:0] = 1 | Present ✅ |
| PAC | ISAR1 API = 2 | QARMA3 ✅ |
| UART IBRD/FBRD | 54/16 | 100 MHz clock ✅ |

### Key Technical Challenges Solved

1. **PIC .rodata relocation** — EFI binary's GOT-relative string references break after ExitBootServices. All string output uses immediate character values.
2. **EL2→EL1 drop** — UEFI runs at EL2 with TGE=1. Custom assembly configures HCR_EL2, CNTHCTL_EL2, CPACR_EL1, SCTLR_EL1, then ERETs to EL1.
3. **Vector table** — `_freertos_vector_table` defined in application assembly with 2KB-aligned AArch64 exception vectors. Installed via PC-relative ADR (same translation unit).
4. **UART clock** — SCMI clock management kills UART clock after ExitBootServices. Fixed by re-initializing PL011 baud rate registers.
5. **Timer interrupt priority** — FreeRTOS port restores PMR to 0xF0. Timer PPI priority set to 0xE0 (< PMR) for delivery.
6. **Virtual timer** — EL1 must use CNTV (PPI 27), not CNTP (PPI 30).
7. **UEFI console encoding** — CIX UEFI serial driver converts CHAR16 to `?`. Bypassed with immediate-built characters.

### Build Artifacts

```
cortex-a320-freertos/
├── freertos.efi              # 365 KB PE32+ AArch64 EFI application
├── Makefile                  # Cross-compile + link FreeRTOS kernel + BSP
├── src/
│   ├── efi_entry.c           # UEFI entry, probe, ExitBootServices, EL2→EL1
│   ├── el2_to_el1.S          # EL drop assembly + vector table
│   ├── main.c                # FreeRTOS entry, GIC init, heartbeat task
│   ├── uart.c                # PL011 polling driver
│   └── timer.c               # Virtual timer tick setup
├── include/
│   ├── FreeRTOSConfig.h      # AArch64 SRE port config (EL1, GUEST mode)
│   ├── platform.h            # CIX P1 register map
│   └── uart.h                # UART API
├── FreeRTOS/                 # Upstream FreeRTOS (kernel V11.3.0)
└── docs/
    ├── status.md             # This file
    ├── gap-analysis.md       # FreeRTOS Armv9 gap analysis
    ├── orion-o6-bringup.md   # Hardware bringup + register map
    └── board1-debug-log.md   # Physical board debug notes
```

### Firmware Versions (Orion O6 Board #1)

- TF-A: v2.7 Beta_2.0.3_release (2025-03-11)
- OP-TEE: Beta_2.0.3_release (2025-02-07)
- UEFI: EDK2 v0.3.0-1

---

## Phase 3: Armv9 Security Features (Next)

### Goals

1. **PAC (Pointer Authentication)** — Enable via compile flags + system register setup
2. **BTI (Branch Target Identification)** — Add landing pads to assembly, enable SCTLR_EL1.BT0
3. **MTE (Memory Tagging Extension)** — Tag FreeRTOS heap allocations, demonstrate fault detection

### Prerequisites Met

- All three features confirmed present on hardware (PFR0, PFR1, ISAR1)
- Running at EL1 with full system register access
- Working UART for diagnostic output
- FreeRTOS scheduler operational for task-based demonstrations

---

## Phase 1: COMPLETE ✅ — EFI Hello World (2026-04-06)

Initial EFI application built and tested. Confirmed UEFI boot path, UART2 output,
and system register access. See git history for Phase 1 code.
