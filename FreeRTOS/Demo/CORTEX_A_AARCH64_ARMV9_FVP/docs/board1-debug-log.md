# Orion O6 Board #1 — Debug Log

Physical board testing notes. Separate from general spec/bringup docs.

## Board Identity

- **Board**: Radxa Orion O6
- **SoC**: CIX P1 (Sky1)
- **Firmware**: TF-A v2.7 Beta_2.0.3_release (built 2025-03-11), OP-TEE Beta_2.0.3_release (built 2025-02-07), UEFI EDK2 v0.3.0-1
- **Serial connection**: UART2 via FT232H at 115200 8N1

---

## 2026-04-16 — First Power-On

### Boot Chain (confirmed from UART log)

```
BootROM → BL2 (TF-A v2.7) → BL31 (TF-A EL3) → BL32 (OP-TEE S-EL1) → BL33 (UEFI EL1/EL2)
```

- BL2 loads at EL3, performs secure boot (RSA verify via CRYPTO_LITE CE)
- BL31 entry at `0x80200000`, SPSR=`0x3cd` (EL3h)
- BL32 (OP-TEE) entry — initializes, switches primary CPU to normal world
- BL33 (UEFI) entry at `0x84400000`, SPSR=`0x3c9` (EL2h — **UEFI runs at EL2**)

### Key Observations

**1. UEFI runs at EL2, not EL1**

SPSR `0x3c9` = `0b1111001001` → M[3:0]=`0b1001` = EL2h.

This means our EFI app will start at **EL2**. The `CurrentEL` read in `efi_entry.c` will confirm. This affects the FreeRTOS port — we're using `GUEST` mode which expects EL1. We may need to drop to EL1 before starting the scheduler, or run at EL2.

**Action needed**: Confirm EL from our EFI app output. If EL2, we need to add EL2→EL1 transition code before starting FreeRTOS.

**2. Boot core is CPU 10 (Cortex-A720)**

```
INFO: boot_core_index 10
INFO: cpu mask 0xa000
```

CPU 10 is a Cortex-A720 (big core), not a Cortex-A520 (little). MPIDR = `0xa00`.

**3. GICv4 detected (not just GICv3)**

```
INFO: GICv4 without legacy support detected.
INFO: ARM GICv4 driver initialized in EL3
INFO: Maximum SPI INTID supported: 543
```

GIC-700 with GICv4 support. 543 SPIs. GICv3 system register interface is a subset — our FreeRTOS port should be fine.

**4. OP-TEE security warnings**

```
I/TC: WARNING: This OP-TEE configuration might be insecure!
```

Expected for development firmware. Not a problem for us.

**5. CRYPTO_LITE: OTP device NOT exist**

```
CRYPTO_LITE OTP device: NOT exist
```

OTP (One-Time Programmable) fuses not present or not programmed. This is normal for development boards — means secure boot keys aren't fused.

**6. PCIe link-up failures**

```
Root Port 0 Link up fail
Root Port 2 Link up fail
```

Root Port 0 = PCIe x8 slot (nothing plugged in — expected).
Root Port 2 = PCIe x2 (M.2 E-Key Wi-Fi slot — may be empty).
Not a problem for FreeRTOS testing.

**7. Display detected on Type-C port 1**

```
One display device is found on typec port1!
Update Platform Config Param GopDisplayPort=0x1
total 4 modes (current pixel clock 2700, width 720, height 480)
```

UEFI GOP found a display on USB-C port 1 (DP alt-mode). 720x480 is likely a fallback mode. HDMI may also be available.

**8. 11 secondary CPUs initialized by OP-TEE**

CPUs 0–9 and 11 logged. CPU 10 is the boot core (primary). All 12 cores accounted for.

**9. TrustZone configured — 4 controllers, 2 regions each**

TF-A configured TrustZone address space controllers. This partitions DRAM into secure/non-secure regions. Our FreeRTOS runs in normal world — should have access to non-secure DRAM from `0x80000000` (minus the reserved firmware regions).

### Firmware Load Addresses

| Image | Load Address | Size | Notes |
|-------|-------------|------|-------|
| BL31 (TF-A) | `0x80200000` | ~100 KB | EL3 runtime |
| BL32 (OP-TEE) | `0x80500000` | ~820 KB | Secure world |
| BL33 (UEFI) | `0x84400000` | 2 MB | Normal world, EL2 |

### Next Steps

- [x] Boot `freertos.efi` and confirm exception level → **EL2 confirmed**
- [ ] Add EL2→EL1 drop before FreeRTOS scheduler
- [ ] Confirm UART2 output after ExitBootServices
- [ ] Confirm timer tick and heartbeat messages

## 2026-04-16 — EL / Feature Probe Results

Probe EFI app ran successfully. UEFI ConOut works on HDMI; serial output is garbled
(UEFI serial driver converts CHAR16 to `?` for each character, but hex values leak through).

### Confirmed Register Values

| Register | Raw Value | Decoded |
|---|---|---|
| **CurrentEL** | EL2 | UEFI runs at **EL2** — need EL2→EL1 drop for FreeRTOS |
| **MIDR_EL1** | `0x00000000410fd811` | Implementer=`0x41` (Arm), Part=`0xd81` (**Cortex-A720**), Variant=1, Rev=1 |
| **CNTFRQ_EL0** | `0x000000003b9aca00` | **1,000,000,000 Hz** (1 GHz) — matches device tree ✅ |
| **ID_AA64PFR0_EL1** | `0x1201111123111111` | SVE=1 (SVE present ✅), EL3/EL2/EL1/EL0 all AArch64 |
| **ID_AA64PFR1_EL1** | `0x0000000000010321` | MTE=3 (**MTE3** — full async+sync tagging ✅), BTI=1 ✅ |
| **ID_AA64ISAR1_EL1** | `0x0111111100211002` | PAC API=2 (QARMA3 ✅), APA=0 |

### Key Takeaways

1. **EL2 confirmed** — must add EL2→EL1 transition before starting FreeRTOS
2. **Boot core is Cortex-A720** (part 0xd81) — not A520, consistent with boot_core_index=10
3. **Timer is 1 GHz** — our FreeRTOSConfig.h TIMER_FREQ_HZ is correct
4. **All Armv9 security features present**: SVE, MTE3, BTI, PAC (QARMA3)
5. **UEFI serial console issue**: ConOut→serial converts CHAR16 to `?` — need to fix for post-ExitBootServices UART output (direct PL011 MMIO faulted under UEFI MMU; must use after ExitBootServices when MMU is off or identity-mapped)

## Hardware TODOs

- [ ] **CR1220 coin cell battery** — 12mm, 3V lithium. Needed for the on-board RTC holder (HYM8563 RTC on I2C3). Without it, UEFI date/time and boot settings reset on every power cycle. Order from Amazon/Digikey.

## 2026-04-16 — FreeRTOS Running! 🎉

### Successful Boot Chain

```
UEFI (EL2) → ExitBootServices → EL2→EL1 ERET → uart_init → FreeRTOS scheduler → heartbeat task
```

### What Works

- EL2→EL1 drop via ERET (HCR_EL2 configured for nVHE, TGE cleared)
- PL011 UART2 at 0x040d0000 (100 MHz, 115200 baud, re-initialized after EL drop)
- FreeRTOS scheduler with preemptive context switching
- Virtual timer (CNTV) at 1 GHz, 1 kHz tick rate
- GIC-700 system register interface + redistributor PPI configuration
- Heartbeat task printing every 1 second via vTaskDelay

### Key Issues Resolved

1. **PIC .rodata relocation** — EFI binary's GOT-relative string references broken after ExitBootServices. Fixed by using immediate character values instead of string literals.
2. **EL2→EL1 transition** — UEFI runs at EL2 with TGE=1. Needed proper HCR_EL2 setup and ERET.
3. **Vector table** — `_freertos_vector_table` must be defined by the application (not in the port). Created with proper 2KB-aligned AArch64 exception vectors routing SVC→FreeRTOS_SWI_Handler, IRQ→FreeRTOS_IRQ_Handler.
4. **UART after ExitBootServices** — SCMI clock management kills UART clock after EBS. Fixed by re-initializing PL011 baud rate registers in uart_init().
5. **Timer interrupt priority** — FreeRTOS port restores PMR to 0xF0 during context switch. Timer PPI priority must be < 0xF0 (set to 0xE0).
6. **Virtual vs physical timer** — At EL1, must use virtual timer (CNTV, PPI 27) not physical timer (CNTP, PPI 30).
7. **UEFI console encoding** — CIX UEFI serial console converts CHAR16 to `?`. Workaround: use immediate-built characters, not .rodata string literals.

## TODO: Graceful Shutdown

Currently requires 5-second hard power-off. Options investigated:

1. **UART RX command** (easiest) — Poll UART RX in idle hook, type 'q' to trigger PSCI SYSTEM_OFF via SMC
2. **EC power button** — Power button routed through EC on I2C6 (`cix_ec_pwrkey`). Requires EC protocol after ExitBootServices.
3. **Watchdog** — `0x16003000` (`cix,sky1-wdt`). Could reset board but not graceful.

Recommended: implement option 1 (UART RX) as a low-priority enhancement.
