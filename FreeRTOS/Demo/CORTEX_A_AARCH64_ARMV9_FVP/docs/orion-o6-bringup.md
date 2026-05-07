# Orion O6 Hardware Bring-up Guide

## UART2 Debug Console

UART2 is the primary debug serial port. It carries BIOS output and OS console logs.

### Location

UART2 is on a dedicated 3-pin header labeled **#5** on the board layout, located on the top edge of the board near the eDP connector. It is NOT on the 40-pin GPIO header.

### Pinout

| Pin | Function | Connect to USB-serial adapter |
|-----|----------|-------------------------------|
| 1 | GND | GND (often black wire) |
| 2 | UART2_TXD | → RXD on adapter |
| 3 | UART2_RXD | → TXD on adapter |

### Serial Parameters

- Baud rate: **115200**
- Data bits: **8**
- Stop bits: **1**
- Parity: **None**
- Flow control: **None**

### Warnings

- **Do NOT connect the VCC/red wire** from the USB-serial adapter. Radxa explicitly warns this can damage the board.
- Signal level is **3.3V**. Use a 3.3V USB-to-serial adapter (not 5V).

### Other UARTs on the Board

| UART | Purpose | Location |
|------|---------|----------|
| UART2 | BIOS + OS debug logs | Dedicated header (#5) |
| UART4 | Power management, voltage/frequency monitoring | Dedicated header (#5) |
| UART5 | Secure BootROM debug logs | Dedicated header (#5) |
| EC UART | Embedded controller debug | Dedicated header (#5) |
| UART0 | General purpose (TX pin 11, RX pin 13) | 40-pin GPIO header |
| UART1 | General purpose (TX pin 15, RX pin 16) | 40-pin GPIO header |
| UART3 | General purpose (TX pin 8, RX pin 10) | 40-pin GPIO header |

## 40-Pin GPIO Header

Located at position **#25** on the board. Pin 1 is marked on the silkscreen.

### Pinout (selected pins relevant to FreeRTOS blinky)

| Pin | Function1 | Function2 | Function3 |
|-----|-----------|-----------|-----------|
| 7 | — | GPIO071 | — |
| 8 | UART3_TXD | GPIO105 | — |
| 10 | UART3_RXD | GPIO106 | — |
| 11 | UART0_TXD | PWM0 | GPIO095 |
| 13 | UART0_RXD | PWM1 | GPIO096 |
| 15 | UART1_TXD | FAN_OUT0 | GPIO099 |
| 16 | UART1_RXD | FAN_TACH0 | GPIO100 |
| 18 | GPIO044 | — | — |

GPIO071 (pin 7) is a good candidate for an external LED blinky — it appears to be a simple GPIO without conflicting default functions.

## UEFI Boot Path

The board boots via UEFI (EDKII). FreeRTOS will be loaded as an EFI application.

### Boot sequence

```
Power On → CIX P1 BootROM → UEFI (EDKII) → Boot Manager
                                              └── "Boot From File" → select .efi from USB
```

### Loading an EFI application

1. Format a USB drive as FAT32
2. Place the `.efi` binary at the root (e.g., `freertos.efi`)
3. Power on the board, enter UEFI setup
4. Navigate to: Boot Maintenance Manager → Boot From File
5. Select the USB device, then the `.efi` file

## Status LEDs

| LED | Color | Meaning |
|-----|-------|---------|
| Power | Green | Board is powered |
| System | Blue (solid) | BIOS has booted |
| System | Blue (blinking) | OS has booted |

These LEDs are EC-controlled and not directly accessible from bare-metal code.

## Power

- USB Type-C PD (≥65W recommended)
- Or ATX 24-pin power connector

## CIX P1 (Sky1) Register Map — Extracted from Linux Device Tree

Source: [cixtech/cix_opensource__linux](https://github.com/cixtech/cix_opensource__linux/blob/cix_k6.6.89_2025q4/arch/arm64/boot/dts/cix/sky1.dtsi) (`sky1.dtsi` + `sky1-orion-o6.dts`)

The CIX P1 TRM is not yet available from the vendor. All register addresses below were extracted from the Linux kernel device tree on 2026-04-16.

### UART — ARM PL011

All UARTs are `"arm,pl011"`, `"arm,primecell"`. Register layout follows the standard ARM PL011 TRM (DDI0183). This is excellent for FreeRTOS — PL011 is the most widely supported UART IP in the Arm ecosystem.

| UART | Base Address | Size | Orion O6 Status | Notes |
|------|-------------|------|-----------------|-------|
| UART0 | `0x040b0000` | 0x1000 | disabled | 40-pin header (TX pin 11, RX pin 13) |
| UART1 | `0x040c0000` | 0x1000 | disabled | 40-pin header (TX pin 15, RX pin 16) |
| **UART2** | **`0x040d0000`** | **0x1000** | **okay** | **Debug console (stdout-path)** |
| UART3 | `0x040e0000` | 0x1000 | disabled | 40-pin header (TX pin 8, RX pin 10) |
| UART4 | `0x06540000` | 0x1000 | disabled | Power management debug |
| UART5 | `0x05020000` | 0x1000 | disabled | Secure BootROM debug |

Clock frequencies (fixed clocks in dtsi):
- `uartclk`: **100 MHz** (functional clock)
- `apb_pclk`: **200 MHz** (APB bus clock)

Key PL011 registers for FreeRTOS (offsets from base):
- `0x000` UARTDR — Data register
- `0x018` UARTFR — Flag register (TXFF, RXFE, BUSY)
- `0x024` UARTIBRD — Integer baud rate divisor
- `0x028` UARTFBRD — Fractional baud rate divisor
- `0x02C` UARTLCR_H — Line control (word length, FIFO enable)
- `0x030` UARTCR — Control register (UART enable, TX/RX enable)
- `0x038` UARTIMSC — Interrupt mask set/clear
- `0x044` UARTRIS — Raw interrupt status

Baud rate calculation: `BRD = uartclk / (16 × baud)` = 100000000 / (16 × 115200) = 54.253 → IBRD=54, FBRD=round(0.253×64)=16

### GIC — ARM GIC-700 (GICv3)

Compatible: `"arm,gic-700"`, `"arm,gic-v3"`

| Component | Base Address | Size | Notes |
|-----------|-------------|------|-------|
| GICD (Distributor) | `0x0e010000` | 0x10000 | Shared, one per SoC |
| GICR0 (Redistributor) | `0x0e090000` | 0x40000 | CPU0 (Cortex-A520) |
| GICR1 | `0x0e0d0000` | 0x40000 | CPU1 (Cortex-A520) |
| GICR2 | `0x0e110000` | 0x40000 | CPU2 (Cortex-A520) |
| GICR3 | `0x0e150000` | 0x40000 | CPU3 (Cortex-A520) |
| GICR4 | `0x0e190000` | 0x40000 | CPU4 (Cortex-A720) |
| GICR5 | `0x0e1d0000` | 0x40000 | CPU5 (Cortex-A720) |
| GICR6 | `0x0e210000` | 0x40000 | CPU6 (Cortex-A720) |
| GICR7 | `0x0e250000` | 0x40000 | CPU7 (Cortex-A720) |
| GICR8 | `0x0e290000` | 0x40000 | CPU8 (Cortex-A720) |
| GICR9 | `0x0e2d0000` | 0x40000 | CPU9 (Cortex-A720) |
| GICR10 | `0x0e310000` | 0x40000 | CPU10 (Cortex-A720) |
| GICR11 | `0x0e350000` | 0x40000 | CPU11 (Cortex-A720) |
| ITS | `0x0e050000` | 0x30000 | Interrupt Translation Service (MSI) |

The FreeRTOS `ARM_AARCH64_SRE` port uses the GICv3 **system register interface** (`ICC_*_EL1`), not MMIO. The GICD/GICR addresses are needed only for distributor initialization (enabling SPIs, setting routing). For single-core FreeRTOS at EL1, UEFI likely initializes the GIC distributor before handing off.

### Generic Timer

Compatible: `"arm,armv8-timer"`

- **Frequency**: `1,000,000,000` Hz (1 GHz) — set via `clock-frequency` property
- **Interrupts** (GIC PPI):
  - Secure physical timer: PPI 13
  - Non-secure physical timer: PPI 14 ← **FreeRTOS tick source at EL1**
  - Virtual timer: PPI 11
  - Hypervisor physical timer: PPI 10

FreeRTOS at EL1 uses `CNTP_CTL_EL0` / `CNTP_TVAL_EL0` (non-secure physical timer). At 1 GHz, a 1 ms tick = counter value of 1,000,000.

### GPIO — Cadence GPIO

Compatible: `"cdns,gpio-r1p02"`

| Controller | Base Address | Size | Pins | Orion O6 Status |
|-----------|-------------|------|------|-----------------|
| s5_gpio0 | `0x16004000` | 0x1000 | 32 | okay |
| s5_gpio1 | `0x16005000` | 0x1000 | 10 | okay |
| s5_gpio2 | `0x16006000` | 0x1000 | 10 | okay |
| fch_gpio0 | `0x04120000` | 0x1000 | 32 | okay |
| fch_gpio1 | `0x04130000` | 0x1000 | 32 | okay |
| fch_gpio2 | `0x04140000` | 0x1000 | 32 | okay |
| fch_gpio3 | `0x04150000` | 0x1000 | 17 | okay |

GPIO clock: s5 GPIOs at 100 MHz, fch GPIOs at 200 MHz.

Cadence GPIO r1p02 register offsets (from IP documentation):
- `0x00` GPIO_SWPORTA_DR — Port A data register
- `0x04` GPIO_SWPORTA_DDR — Port A data direction (0=input, 1=output)
- `0x30` GPIO_INTEN — Interrupt enable
- `0x38` GPIO_INTTYPE_LEVEL — Interrupt type (0=level, 1=edge)
- `0x50` GPIO_EXT_PORTA — External port A (read pin state)

### CPU Topology

12 cores, 1 cluster (big.LITTLE):

| CPUs | Core | MPIDR | DMIPS/MHz | Role |
|------|------|-------|-----------|------|
| CPU0–3 | Cortex-A520 | 0x000–0x300 | 403 | Little (efficiency) |
| CPU4–7 | Cortex-A720 | 0x400–0x700 | 1024 | Big (performance) |
| CPU8–11 | Cortex-A720 | 0x800–0xb00 | 1024 | Big (performance) |

FreeRTOS runs single-core. UEFI boots on one core (likely CPU0). The EFI app will confirm which core via `MIDR_EL1`.

### Memory Map

| Region | Start | Size | Notes |
|--------|-------|------|-------|
| DRAM | `0x80000000` | 4 GB (base dtsi) | Orion O6 has up to 64 GB LPDDR5 |
| Reserved (firmware) | `0x80000000` | 96 MB | `/memreserve/` in dtsi |
| FCH peripherals | `0x04000000` | — | UART, I2C, SPI, GPIO, timers |
| GIC | `0x0e010000` | — | Distributor + redistributors |
| S5 peripherals | `0x16000000` | — | PDC, watchdog, GPIO, pinctrl |

### Pin Mux Controller

| Controller | Base Address | Size | Notes |
|-----------|-------------|------|-------|
| iomuxc (FCH) | `0x04170000` | 0x1000 | FCH peripheral pin mux |
| iomuxc_s5 | `0x16007000` | 0x1000 | S5 domain pin mux |

### Additional Peripherals (for reference)

| Peripheral | Base Address | Compatible | Notes |
|-----------|-------------|------------|-------|
| Watchdog | `0x16003000` | `"cix,sky1-wdt"` | Two register regions |
| GPT Timer 0 | `0x04110000` | `"cix,sky1-gpt"` | 25 MHz functional clock |
| GPT Timer 1 | `0x04112000` | `"cix,sky1-gpt"` | |
| GPT Timer 2 | `0x04114000` | `"cix,sky1-gpt"` | |
| GPT Timer 3 | `0x04116000` | `"cix,sky1-gpt"` | status=okay |
| PWM0 | `0x04110000` | `"cix,sky1-pwm"` | Shares base with GPT Timer 0 |
| PWM1 | `0x04111000` | `"cix,sky1-pwm"` | |
| SRC (reset ctrl) | `0x16000000` | `"cix,sky1-src"` | System reset controller |
| SRC FCH | `0x04160000` | `"cix,sky1-src-fch"` | FCH reset controller |

## Toolchain Setup

### Cross-compiler ✅

Arm GNU Toolchain 15.2.Rel1 (GCC 15.2.1), bare-metal AArch64 target.

```
~/toolchains/arm-gnu-toolchain-15.2.rel1-x86_64-aarch64-none-elf/bin/aarch64-none-elf-gcc
```

### GNU-EFI ✅

Built from source against the cross-compiler (not the system `apt` package, to avoid version conflicts).

```
~/toolchains/gnu-efi/
```

Key artifacts:
- `aarch64/gnuefi/crt0-efi-aarch64.o` — EFI application entry stub
- `aarch64/gnuefi/libgnuefi.a` — EFI relocation support
- `aarch64/lib/libefi.a` — EFI library (console, protocols, etc.)
- `inc/` — EFI header files
