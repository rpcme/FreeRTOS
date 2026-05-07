/*
 * UEFI entry — probe, ExitBootServices, EL2→EL1, FreeRTOS.
 * No .rodata string literals (PIC relocation broken in EFI binary).
 */

#include <efi.h>
#include <efilib.h>

extern void drop_to_el1(void (*entry)(void), void *stack_top);
extern void freertos_main(void);

static unsigned char el1_stack[16384] __attribute__((aligned(16)));

/* ---- UEFI ConOut (pre-ExitBootServices) ---- */
static SIMPLE_TEXT_OUTPUT_INTERFACE *con;

static void out1(CHAR16 c)
{
    CHAR16 b[2];
    b[0] = c;
    b[1] = 0;
    uefi_call_wrapper(con->OutputString, 2, con, b);
}

static void out_hex(UINT64 val)
{
    int i;
    out1('0'); out1('x');
    for (i = 60; i >= 0; i -= 4) {
        unsigned n = (unsigned)((val >> i) & 0xf);
        out1((CHAR16)(n < 10 ? '0' + n : 'a' + n - 10));
    }
}

static void out_nl(void) { out1('\r'); out1('\n'); }

/* ---- ExitBootServices ---- */
static EFI_STATUS exit_boot_services(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *systab)
{
    EFI_STATUS status;
    UINTN map_size = 0, map_key, desc_size;
    UINT32 desc_ver;
    EFI_MEMORY_DESCRIPTOR *map = NULL;

    status = uefi_call_wrapper(systab->BootServices->GetMemoryMap, 5,
                               &map_size, map, &map_key, &desc_size, &desc_ver);
    map_size += 2 * desc_size;
    status = uefi_call_wrapper(systab->BootServices->AllocatePool, 3,
                               EfiLoaderData, map_size, (void **)&map);
    if (EFI_ERROR(status)) return status;

    status = uefi_call_wrapper(systab->BootServices->GetMemoryMap, 5,
                               &map_size, map, &map_key, &desc_size, &desc_ver);
    if (EFI_ERROR(status)) return status;

    status = uefi_call_wrapper(systab->BootServices->ExitBootServices, 2,
                               image_handle, map_key);
    return status;
}

EFI_STATUS
efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *systab)
{
    EFI_STATUS status;
    con = systab->ConOut;
    InitializeLib(image_handle, systab);

    /* ---- Pre-EBS probe via UEFI ConOut ---- */
    out1('E'); out1('L'); out1('=');
    UINT64 el;
    asm volatile("MRS %0, CurrentEL" : "=r"(el));
    el = (el >> 2) & 3;
    out1((CHAR16)('0' + el));
    out_nl();

    if (el == 2) {
        UINT64 hcr;
        asm volatile("MRS %0, HCR_EL2" : "=r"(hcr));
        out1('H'); out1('C'); out1('R'); out1('=');
        out_hex(hcr);
        out_nl();
    }

    out1('E'); out1('B'); out1('S');
    out_nl();

    status = exit_boot_services(image_handle, systab);
    if (EFI_ERROR(status)) {
        for (;;) asm volatile("wfi");
    }

    /* ---- Post-EBS: drop to EL1 ---- */
    asm volatile("MRS %0, CurrentEL" : "=r"(el));
    el = (el >> 2) & 3;

    if (el == 2) {
        void *stack_top = &el1_stack[sizeof(el1_stack)];
        drop_to_el1(freertos_main, stack_top);
    }

    freertos_main();

    for (;;) asm volatile("wfi");
    return 0;
}
