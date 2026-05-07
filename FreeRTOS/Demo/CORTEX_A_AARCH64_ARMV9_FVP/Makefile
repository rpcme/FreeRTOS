TOOLCHAIN := $(HOME)/toolchains/arm-gnu-toolchain-15.2.rel1-x86_64-aarch64-none-elf/bin/aarch64-none-elf-
GNUEFI    := $(HOME)/toolchains/gnu-efi
FVP       := $(HOME)/toolchains/FVP_Base_RevC_AEMvA/bin/FVP_Base_RevC-2xAEMvA

CC      := $(TOOLCHAIN)gcc
LD      := $(TOOLCHAIN)ld
OBJCOPY := $(TOOLCHAIN)objcopy

# --- Paths ---
EFIINC  := $(GNUEFI)/inc
EFILIB  := $(GNUEFI)/aarch64/lib
EFICRT  := $(GNUEFI)/aarch64/gnuefi
EFILDS  := $(GNUEFI)/gnuefi

FREERTOS     := FreeRTOS/FreeRTOS/Source
FREERTOS_PORT := $(FREERTOS)/portable/GCC/ARM_AARCH64_SRE
FREERTOS_MEM  := $(FREERTOS)/portable/MemMang

# --- FreeRTOS kernel sources ---
FREERTOS_SRCS := \
	$(FREERTOS)/tasks.c \
	$(FREERTOS)/queue.c \
	$(FREERTOS)/list.c \
	$(FREERTOS)/timers.c \
	$(FREERTOS)/event_groups.c \
	$(FREERTOS_PORT)/port.c \
	$(FREERTOS_MEM)/heap_4.c

FREERTOS_ASM := $(FREERTOS_PORT)/portASM.S

# --- Include paths ---
PROJ_INCLUDES := -Iinclude -Isrc/bsp

RTOS_INCLUDES := $(PROJ_INCLUDES) \
	-I$(FREERTOS)/include -I$(FREERTOS_PORT)

EFI_INCLUDES := $(RTOS_INCLUDES) \
	-I$(EFIINC) -I$(EFIINC)/aarch64 -I$(EFIINC)/protocol

# --- Compiler flags ---
COMMON_CFLAGS := -ffreestanding -fno-stack-protector -fPIC -fPIE \
	-g -O2 -Wall -Wextra -std=gnu11 \
	-fno-strict-aliasing -fno-merge-all-constants

EFI_CFLAGS := $(COMMON_CFLAGS) $(EFI_INCLUDES) \
	-DCONFIG_aarch64 -D__GNU_EFI_NO_GLIBC

RTOS_CFLAGS := $(COMMON_CFLAGS) $(RTOS_INCLUDES) \
	-DGUEST -include string.h \
	-mbranch-protection=standard

RTOS_ASFLAGS := $(PROJ_INCLUDES) \
	-I$(FREERTOS)/include -I$(FREERTOS_PORT) \
	-DGUEST -g

LDFLAGS := -nostdlib -shared -Bsymbolic \
	-L$(EFILIB) -L$(EFICRT) \
	-T $(EFILDS)/elf_aarch64_efi.lds \
	-z common-page-size=4096 -z max-page-size=4096

TARGET := freertos.efi

# --- Object lists ---
EFI_OBJS := build/bsp/efi_entry.o

RTOS_OBJS := $(patsubst $(FREERTOS)/%.c,build/freertos/%.o,$(FREERTOS_SRCS))
RTOS_ASM_OBJS := build/freertos/portASM.o

BSP_C_OBJS := build/bsp/uart.o build/bsp/timer.o build/bsp/gic.o
BSP_ASM_OBJS := build/bsp/el2_to_el1.o

ARMV9_ASM_OBJS := build/armv9/pac_init.o
ARMV9_C_OBJS := build/armv9/mte_init.o

DEMO_OBJS := build/demo/main.o build/demo/pac_demo.o build/demo/mte_demo.o

ALL_OBJS := $(EFI_OBJS) $(RTOS_OBJS) $(RTOS_ASM_OBJS) \
	$(BSP_C_OBJS) $(BSP_ASM_OBJS) $(ARMV9_ASM_OBJS) $(ARMV9_C_OBJS) $(DEMO_OBJS)

.PHONY: all clean

all: $(TARGET)

# --- EFI entry (needs gnu-efi headers) ---
build/bsp/efi_entry.o: src/bsp/efi_entry.c | build/bsp
	$(CC) $(EFI_CFLAGS) -c $< -o $@

# --- BSP C sources ---
build/bsp/%.o: src/bsp/%.c | build/bsp
	$(CC) $(RTOS_CFLAGS) -c $< -o $@

# --- BSP assembly ---
build/bsp/%.o: src/bsp/%.S | build/bsp
	$(CC) $(RTOS_ASFLAGS) -c $< -o $@

# --- Armv9 port assembly ---
build/armv9/%.o: src/armv9/%.S | build/armv9
	$(CC) $(RTOS_ASFLAGS) -c $< -o $@

# --- Armv9 port C sources (need -march for MTE intrinsics) ---
build/armv9/%.o: src/armv9/%.c | build/armv9
	$(CC) $(RTOS_CFLAGS) -march=armv8.5-a+memtag -c $< -o $@

# --- Demo sources ---
build/demo/%.o: src/demo/%.c | build/demo
	$(CC) $(RTOS_CFLAGS) -c $< -o $@

# --- Demo MTE source (needs memtag arch) ---
build/demo/mte_demo.o: src/demo/mte_demo.c | build/demo
	$(CC) $(RTOS_CFLAGS) -march=armv8.5-a+memtag -c $< -o $@

# --- FreeRTOS kernel C sources ---
build/freertos/%.o: $(FREERTOS)/%.c | build/freertos/portable/GCC/ARM_AARCH64_SRE build/freertos/portable/MemMang
	$(CC) $(RTOS_CFLAGS) -c $< -o $@

# --- FreeRTOS port assembly ---
build/freertos/portASM.o: $(FREERTOS_ASM) | build/freertos
	$(CC) $(RTOS_ASFLAGS) -c $< -o $@

# --- Link ---
build/freertos.so: $(ALL_OBJS)
	$(LD) $(LDFLAGS) $(EFICRT)/crt0-efi-aarch64.o $^ -o $@ -lefi -lgnuefi

$(TARGET): build/freertos.so
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym \
	           -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc \
	           --target efi-app-aarch64 $< $@

# --- Directory creation ---
build/bsp build/armv9 build/demo build/freertos:
	mkdir -p $@

build/freertos/portable/GCC/ARM_AARCH64_SRE: | build/freertos
	mkdir -p $@

build/freertos/portable/MemMang: | build/freertos
	mkdir -p $@

clean:
	rm -rf build $(TARGET)

# ===========================================================================
# FVP bare-metal target
# ===========================================================================

FVP_TARGET := freertos_fvp.elf

FVP_CFLAGS := -ffreestanding -fno-stack-protector -fno-builtin \
	-g -O2 -Wall -Wextra -std=gnu11 \
	-fno-strict-aliasing -fno-merge-all-constants \
	$(RTOS_INCLUDES) \
	-DGUEST -DFVP_PLATFORM $(FVP_EXTRA_DEFS) -include string.h \
	-mbranch-protection=standard

FVP_ASFLAGS := $(PROJ_INCLUDES) \
	-I$(FREERTOS)/include -I$(FREERTOS_PORT) \
	-DGUEST -DFVP_PLATFORM -g -march=armv9-a+sve2

FVP_LDFLAGS := -nostdlib -T linker/fvp.ld

# FVP objects — reuse FreeRTOS kernel + BSP (uart, timer, gic) + demo + armv9
FVP_OBJS := build/fvp/startup.o \
	$(patsubst build/%,build/fvp/%,$(RTOS_OBJS)) \
	build/fvp/freertos/portASM.o \
	build/fvp/bsp/uart.o build/fvp/bsp/timer.o build/fvp/bsp/gic.o \
	build/fvp/bsp/libc_stubs.o build/fvp/bsp/semihost.o \
	build/fvp/armv9/pac_init.o build/fvp/armv9/mte_init.o build/fvp/armv9/mmu_init.o \
	build/fvp/demo/main.o build/fvp/demo/pac_demo.o build/fvp/demo/mte_demo.o build/fvp/demo/bti_demo.o build/fvp/demo/security_demo.o build/fvp/demo/demo_setjmp.o build/fvp/demo/sve2_demo.o build/fvp/demo/sobel_demo.o build/fvp/demo/histogram_demo.o build/fvp/demo/blur_demo.o build/fvp/demo/sad_demo.o build/fvp/demo/match_demo.o build/fvp/demo/filter_demo.o build/fvp/tests/sve_frame_test.o build/fvp/tests/sve_ctx_test.o

.PHONY: fvp fvp-run

fvp: $(FVP_TARGET)

# --- FVP startup assembly ---
build/fvp/startup.o: src/bsp/fvp_startup.S | build/fvp
	$(CC) $(FVP_ASFLAGS) -c $< -o $@

# --- FVP BSP C sources ---
build/fvp/bsp/%.o: src/bsp/%.c | build/fvp/bsp
	$(CC) $(FVP_CFLAGS) -c $< -o $@

# --- FVP Armv9 assembly ---
build/fvp/armv9/%.o: src/armv9/%.S | build/fvp/armv9
	$(CC) $(FVP_ASFLAGS) -c $< -o $@

# --- FVP Armv9 C (MTE) ---
build/fvp/armv9/%.o: src/armv9/%.c | build/fvp/armv9
	$(CC) $(FVP_CFLAGS) -march=armv8.5-a+memtag -c $< -o $@

# --- FVP Demo sources ---
build/fvp/demo/%.o: src/demo/%.c | build/fvp/demo
	$(CC) $(FVP_CFLAGS) -c $< -o $@

build/fvp/demo/mte_demo.o: src/demo/mte_demo.c | build/fvp/demo
	$(CC) $(FVP_CFLAGS) -march=armv8.5-a+memtag -c $< -o $@

build/fvp/demo/security_demo.o: src/demo/security_demo.c | build/fvp/demo
	$(CC) $(FVP_CFLAGS) -march=armv8.5-a+memtag -c $< -o $@

build/fvp/demo/sve2_demo.o: src/demo/sve2_demo.c | build/fvp/demo
	$(CC) $(FVP_CFLAGS) -march=armv9-a+sve2 -c $< -o $@

build/fvp/demo/sobel_demo.o: src/demo/sobel_demo.c | build/fvp/demo
	$(CC) $(FVP_CFLAGS) -march=armv9-a+sve2 -c $< -o $@

build/fvp/demo/histogram_demo.o: src/demo/histogram_demo.c | build/fvp/demo
	$(CC) $(FVP_CFLAGS) -march=armv9-a+sve2 -c $< -o $@

build/fvp/demo/blur_demo.o: src/demo/blur_demo.c | build/fvp/demo
	$(CC) $(FVP_CFLAGS) -march=armv9-a+sve2 -c $< -o $@

build/fvp/demo/sad_demo.o: src/demo/sad_demo.c | build/fvp/demo
	$(CC) $(FVP_CFLAGS) -march=armv9-a+sve2 -c $< -o $@

build/fvp/demo/match_demo.o: src/demo/match_demo.c | build/fvp/demo
	$(CC) $(FVP_CFLAGS) -march=armv9-a+sve2 -c $< -o $@

build/fvp/demo/filter_demo.o: src/demo/filter_demo.c | build/fvp/demo
	$(CC) $(FVP_CFLAGS) -march=armv9-a+sve2 -c $< -o $@

build/fvp/demo/demo_setjmp.o: src/demo/demo_setjmp.S | build/fvp/demo
	$(CC) -c $< -o $@

build/fvp/tests/sve_frame_test.o: tests/sve_frame_test.S | build/fvp/tests
	$(CC) -march=armv9-a+sve2 -c $< -o $@

build/fvp/tests/sve_ctx_test.o: tests/sve_ctx_test.S | build/fvp/tests
	$(CC) -march=armv9-a+sve2 -c $< -o $@

build/fvp/tests:
	mkdir -p $@

# --- FVP FreeRTOS kernel ---
build/fvp/freertos/%.o: $(FREERTOS)/%.c | build/fvp/freertos/portable/GCC/ARM_AARCH64_SRE build/fvp/freertos/portable/MemMang
	$(CC) $(FVP_CFLAGS) -c $< -o $@

build/fvp/freertos/portASM.o: src/bsp/portASM_fvp.S | build/fvp/freertos
	$(CC) $(FVP_ASFLAGS) -c $< -o $@

# --- FVP link ---
$(FVP_TARGET): $(FVP_OBJS)
	$(CC) $(FVP_LDFLAGS) $^ -o $@ -lgcc

# --- FVP directories ---
build/fvp build/fvp/bsp build/fvp/armv9 build/fvp/demo build/fvp/freertos:
	mkdir -p $@

build/fvp/freertos/portable/GCC/ARM_AARCH64_SRE: | build/fvp/freertos
	mkdir -p $@

build/fvp/freertos/portable/MemMang: | build/fvp/freertos
	mkdir -p $@

# --- FVP run (interactive) ---
fvp-run: $(FVP_TARGET)
	$(FVP) \
		-C cluster0.cpu0.RVBAR=0x80000000 \
		-C cluster0.has_arm_v9-2=1 \
		-C cluster0.has_sve=1 \
		-C cluster0.sve.veclen=2 \
		-C cluster0.memory_tagging_support_level=3 \
		-C cluster0.has_pointer_authentication=1 \
		-C cluster0.bti_support_level=1 \
		-C bp.dram_metadata.is_enabled=1 \
		-C bp.secure_memory=false \
		-C bp.refcounter.non_arch_start_at_default=1 \
		-C bp.pl011_uart0.out_file=- \
		-C bp.pl011_uart0.shutdown_on_eot=1 \
		-C bp.vis.disable_visualisation=1 \
		-C bp.terminal_0.start_telnet=0 \
		-C bp.terminal_1.start_telnet=0 \
		-C bp.terminal_2.start_telnet=0 \
		-C bp.terminal_3.start_telnet=0 \
		--stat \
		-a $(FVP_TARGET)

# --- FVP automated test (non-interactive, cycle-limited) ---
.PHONY: fvp-test
fvp-test:
	$(MAKE) fvp FVP_EXTRA_DEFS=-DFVP_NONINTERACTIVE
	$(FVP) \
		-C cluster0.cpu0.RVBAR=0x80000000 \
		-C cluster0.has_arm_v9-2=1 \
		-C cluster0.has_sve=1 \
		-C cluster0.sve.veclen=2 \
		-C cluster0.memory_tagging_support_level=3 \
		-C cluster0.has_pointer_authentication=1 \
		-C cluster0.bti_support_level=1 \
		-C bp.dram_metadata.is_enabled=1 \
		-C bp.secure_memory=false \
		-C bp.refcounter.non_arch_start_at_default=1 \
		-C bp.pl011_uart0.out_file=- \
		-C bp.pl011_uart0.shutdown_on_eot=1 \
		-C bp.vis.disable_visualisation=1 \
		-C bp.terminal_0.start_telnet=0 \
		-C bp.terminal_1.start_telnet=0 \
		-C bp.terminal_2.start_telnet=0 \
		-C bp.terminal_3.start_telnet=0 \
		--stat \
		--cyclelimit 500000000 \
		-a $(FVP_TARGET)
