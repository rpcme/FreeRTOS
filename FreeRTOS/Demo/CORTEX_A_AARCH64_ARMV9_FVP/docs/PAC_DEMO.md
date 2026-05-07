# PAC (Pointer Authentication) — Feature Analysis & Demo

## What PAC Does

PAC uses hardware cryptography (QARMA3 on the CIX P1) to sign pointers —
primarily the return address stored on the stack when a function is called.

**On function entry**: the CPU computes a signature using the pointer value, a
context value (the stack pointer), and a secret key. The signature is stored in
the unused upper bits of the 64-bit pointer.

**On function return**: the CPU recomputes the signature and compares. If they
don't match — because an attacker modified the return address — the pointer is
poisoned and the CPU faults immediately.

This happens in a single instruction (`PACIASP` / `AUTIASP`) with no software
overhead beyond the instruction itself.

## What It Protects Against

**Return-Oriented Programming (ROP)** — the dominant exploit technique for
embedded systems. An attacker who overwrites the stack (via buffer overflow, DMA
corruption, or fault injection) cannot redirect execution by modifying return
addresses. The PAC signature will be wrong and the CPU traps before the
corrupted address is used.

Also protects **function pointer hijacking** (JOP) when PAC is applied to data
pointers.

## How It's Used in Practice

### For an RTOS developer shipping a product:

1. **Compile with `-mbranch-protection=pac-ret`** — that's it for C code.
   GCC/Clang automatically insert PACIASP/AUTIASP. Zero source code changes.

2. **Program PAC keys at boot** — one-time setup. In production, use a hardware
   RNG for per-boot keys. The CIX P1 has a TRNG (`CRYPTO_LITE TRNG internal
   source: Exist`).

3. **Enable in SCTLR_EL1** — set the EnIA bit. Done during EL2→EL1 transition.

4. **Hand-written assembly** — any assembly that manipulates LR must preserve
   the PAC bits in the upper bytes. The FreeRTOS port saves/restores X30 as a
   raw 64-bit value, which preserves PAC bits correctly.

### What the developer does NOT need to do:

- No source code annotations
- No special memory allocator
- No runtime library
- No performance tuning

## Cost

- **Performance**: ~1 cycle per function call/return. Negligible.
- **Code size**: ~8 bytes per function (two instructions).
- **Complexity**: Near zero for C code. Boot setup + assembly awareness only.

## Limitations

- Only protects return addresses by default (not arbitrary data pointers)
- Keys are per-CPU, not per-task (shared key across all FreeRTOS tasks)
- Does not prevent calling legitimate functions at legitimate addresses — BTI
  complements PAC by restricting valid branch targets

## Real-World Relevance for IoT/Edge

For a Cortex-A320 edge device running FreeRTOS with network-facing code (MQTT,
HTTP, video decode), PAC provides hardware-enforced protection against the most
common class of remote code execution exploits — stack buffer overflows leading
to ROP chains.

This is the same protection Apple ships on every iPhone (since A12), now
available on microcontroller-class Arm cores for the first time with Armv9.

---

## Demo: Buffer Overflow — With and Without PAC

### Scenario

A FreeRTOS task receives MQTT messages over the network. A malformed message
contains 48 bytes of data, but the receiving function only has a 32-byte buffer.
The extra 16 bytes overflow past the buffer and overwrite the saved return
address on the stack.

### Act 1: Without PAC — Attacker Wins

The function was compiled without PAC (simulating legacy code). The overflow
overwrites the return address with the attacker's chosen address. When the
function returns, execution jumps to the attacker's code.

**What the audience sees:**

```
=== ACT 1: No PAC ===
Recv 48B MQTT msg             ← Malformed message arrives
memcpy(buf32,msg,48)          ← Buffer overflow — 16 bytes past the end
LR overwritten!               ← Return address on stack is now attacker's address
!! HACKED !!                  ← Attacker's code executes
Attacker ctrl exec            ← Attacker has full control
```

**Key point**: The developer made one mistake (no bounds check on memcpy).
Without PAC, that single bug gives the attacker complete control of the device.

### Act 2: With PAC — Attack Stopped by Hardware

The same function, same overflow, same corrupted return address. But this time
the function was compiled with `-mbranch-protection=pac-ret`. The CPU signed the
return address when the function was entered (PACIASP). When the function tries
to return, the CPU checks the signature (AUTIASP) — and it doesn't match,
because the attacker's address doesn't have a valid signature.

**What the audience sees:**

```
=== ACT 2: PAC ON ===
Recv 48B MQTT msg             ← Same malformed message
memcpy(buf32,msg,48)          ← Same buffer overflow
LR overwritten!               ← Same corruption of return address
PAC checking...               ← CPU is about to verify the return address
Processing msg                ← Function runs normally up to the return
Return...                     ← AUTIASP checks signature — MISMATCH
                              ← CPU faults. Execution stops. Attacker blocked.
```

**Key point**: The developer's code has the exact same bug. The only difference
is a compiler flag (`-mbranch-protection=pac-ret`) and a one-time key setup at
boot. The hardware catches the attack automatically — no security code needed.

### What the Developer Changed

| | Without PAC | With PAC |
|---|---|---|
| Source code | Identical | Identical |
| Compiler flag | (none) | `-mbranch-protection=pac-ret` |
| Boot setup | (none) | Program PAC keys (5 lines of assembly) |
| Runtime cost | N/A | ~1 cycle per function call |
| Result | **Attacker wins** | **Attack stopped** |

### How It Works Internally

```
Function entry:
  PACIASP          ← CPU signs LR using secret key + SP context
  STP X29, X30, [SP, #-16]!   ← Save signed LR to stack

  ... function body ...

  Buffer overflow writes 0x41414141 over saved LR on stack

Function return:
  LDP X29, X30, [SP], #16     ← Load corrupted LR (0x41414141)
  AUTIASP          ← CPU checks: is 0x41414141 signed with our key + SP?
                   ← NO — signature missing/wrong
                   ← CPU poisons the pointer and faults
                   ← Execution NEVER reaches 0x41414141
```

---

## Hardware Details (Orion O6)

| Parameter | Value |
|---|---|
| SoC | CIX P1 (Sky1) |
| CPU | Cortex-A720 (boot core) |
| PAC Algorithm | QARMA3 (ID_AA64ISAR1_EL1 API=2) |
| PAC Key | APIA (APIAKeyLo_EL1 / APIAKeyHi_EL1) |
| Enable Bit | SCTLR_EL1.EnIA (bit 31) |
| Compiler Flag | `-mbranch-protection=pac-ret` |
| FreeRTOS Version | V11.3.0 |
| Port | ARM_AARCH64_SRE (GUEST mode, EL1) |

## Files

| File | Category | Description |
|---|---|---|
| `src/armv9/pac_init.S` | Port (Armv9) | PAC key programming — works on any Armv9 chip |
| `src/demo/pac_demo.c` | Demo | Two-act buffer overflow demonstration |
| `src/bsp/el2_to_el1.S` | BSP | SCTLR_EL1.EnIA enable + HCR_EL2.API/APK |
| `Makefile` | Build | `-mbranch-protection=pac-ret` in RTOS_CFLAGS |

---

## Interactive Demo (Final Version)

The user types actual data via UART. The system shows real memory addresses
and hex dumps. Nothing is simulated — the audience sees their own input
in memory and watches the PAC-signed return address get corrupted.

### Safe Run (user types 14 characters)

```
=== PAC DEMO ===
32B buffer. Type > 32 to overflow.
buf@0x00000007fabb6090
LR @0x00000007fabb6048

buf:
0x00000007fabb6090: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0x00000007fabb60a0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
LR= 0x73730007faba6394          ← PAC signature in upper bits (0x7373)

Type text, Enter=done: dfslkmfsdmflsd

[14 chars]
buf:
0x00000007fabb6090: 64 66 73 6c 6b 6d 66 73 64 6d 66 6c 73 64 00 00
                    d  f  s  l  k  m  f  s  d  m  f  l  s  d  ← user's actual bytes!
LR= 0x73730007faba6394          ← Unchanged — buffer didn't overflow

Returning (AUTIASP)
PAC OK - return safe             ← AUTIASP verified signature, return succeeded
```

### Overflow Run (user types 84 characters, then corrupts LR)

```
=== PAC DEMO ===
32B buffer. Type > 32 to overflow.
buf@0x00000007fabb6090
LR @0x00000007fabb6048

Type text, Enter=done: dfslkmfsdmflsddfslkmfsdmflsd... (84 chars)

[84 chars]
buf:
0x00000007fabb6090: 64 66 73 6c 6b 6d 66 73 64 6d 66 6c 73 64 64 66
0x00000007fabb60a0: 73 6c 6b 6d 66 73 64 6d 66 6c 73 64 64 66 73 6c
LR= 0x73730007faba6394          ← Still signed (overflow past buffer, not at LR yet)

Overflow! Corrupt LR? (y/n) y   ← User explicitly triggers the attack

LR corrupted!
LR= 0x4141414141414141          ← PAC signature GONE — replaced with 0x41414141

Returning (AUTIASP)
                                 ← CPU FAULTS — AUTIASP detected invalid signature
                                 ← System halted. Attack stopped by hardware.
                                 ← Attacker's address (0x4141...) was NEVER executed.
```

### What Makes This Convincing

1. **User types their own data** — the hex dump shows their exact bytes, not pre-programmed values
2. **Real memory addresses** — `0x00000007fabb6090` is the actual stack address on the Cortex-A720
3. **PAC signature visible** — the upper bits of LR (`0x7373`) are the cryptographic signature
4. **Before/after comparison** — audience sees LR change from signed to `0x4141414141414141`
5. **User triggers the corruption** — typing `y` at the prompt, not automated
6. **System halts on return** — no "HACKED" message, no fake output, the CPU simply stops
