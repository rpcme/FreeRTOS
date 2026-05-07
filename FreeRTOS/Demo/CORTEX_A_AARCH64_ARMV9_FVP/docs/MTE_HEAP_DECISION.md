# MTE Heap Integration — Design Decision Record

**Date**: 2026-05-06
**Decision**: Use wrapper approach, no heap_4 fork
**Status**: Accepted

## Context

MTE (Memory Tagging Extension) detects buffer overflows and use-after-free by associating a 4-bit tag with each 16-byte memory granule. When a pointer's tag (bits [59:56]) doesn't match the stored allocation tag for the accessed granule, the hardware raises a synchronous fault.

The question: does integrating MTE into FreeRTOS require forking `heap_4.c` (the default FreeRTOS allocator), or can it be done externally?

## MTE Hardware Requirements

| Aspect | Detail |
|--------|--------|
| Granule size | 16 bytes (smallest taggable unit) |
| Tag width | 4 bits (values 0–15) |
| Tag storage | Separate metadata memory (transparent to software) |
| Memory attribute | Pages must be mapped as "Normal Tagged" (MAIR index with inner=Tagged) |
| Key instructions | `IRG Xd, Xn` — generate random tag for pointer |
| | `STG Xt, [Xn]` — store tag to 16-byte granule |
| | `ST2G Xt, [Xn]` — store tag to 32 bytes (2 granules) |
| | `STZG Xt, [Xn]` — store tag + zero 16 bytes |
| | `LDG Xt, [Xn]` — load tag from granule |
| System registers | `SCTLR_EL1.ATA` (bit 43) — enable tag checking |
| | `GCR_EL1` — tag exclusion mask (which tags to avoid) |
| | `RGSR_EL1` — random tag seed |
| Alignment | Allocations MUST be 16-byte aligned for tagging to work |

## What an MTE-Aware Allocator Must Do

### On allocation (`pvPortMalloc` equivalent):
1. Return a 16-byte aligned pointer
2. Generate a random tag: `IRG Xd, Xn` (hardware RNG, excludes tag 0 and any in GCR_EL1 mask)
3. Store the tag to all granules in the allocation: `STG` loop (or `ST2G` for 32-byte steps)
4. Return the tagged pointer (tag in bits [59:56])

### On free (`vPortFree` equivalent):
1. Strip the tag from the pointer to find the real address
2. Re-tag the freed region with tag 0 (or a different random tag) so use-after-free is detected
3. Proceed with normal free-list operations using the untagged address

### Adjacent memory:
- The granules immediately before and after an allocation must have a DIFFERENT tag
- This happens naturally: freed/unallocated memory retains tag 0, allocated memory gets a non-zero random tag

## Options Evaluated

### Option A: Fork heap_4.c (REJECTED)

Modify heap_4 internals to tag during allocation and re-tag during free.

**Pros:**
- Slightly more efficient: can tag during block split (memory is already being touched)
- Can ensure metadata blocks (BlockLink_t headers) have tag 0 (protecting allocator internals)
- Single point of integration

**Cons:**
- Maintenance burden: must track upstream heap_4 changes across FreeRTOS releases
- Invasive: heap_4's free-list manipulation uses untagged pointers internally; mixing tagged/untagged is error-prone
- Unnecessary: the tagging operations are independent of allocator internals
- Premature: no evidence that the ~5 extra instructions per allocation matter for our workloads

### Option B: Wrapper around pvPortMalloc (ACCEPTED)

```c
void *pvPortMallocTagged(size_t xSize)
{
    void *pv = pvPortMalloc(xSize);
    if (pv == NULL) return NULL;

    /* Generate random tag (excludes 0 via GCR_EL1) */
    __asm volatile("irg %0, %1" : "=r"(pv) : "r"(pv));

    /* Tag all granules in the allocation */
    size_t xRounded = (xSize + 15) & ~15UL;
    for (size_t i = 0; i < xRounded; i += 16)
        __asm volatile("stg %0, [%0]" :: "r"((char *)pv + i));

    return pv; /* Tagged pointer — bits [59:56] carry the tag */
}

void vPortFreeTagged(void *pv)
{
    if (pv == NULL) return;

    /* Strip tag to get real address for the allocator */
    void *pvReal;
    __asm volatile("mov %0, %1" : "=r"(pvReal) : "r"(pv));
    pvReal = (void *)((uintptr_t)pvReal & 0x00FFFFFFFFFFFFFFULL);

    /* Re-tag freed region with tag 0 (use-after-free detection).
     * We don't know the exact size here — could store it or
     * read from heap_4's BlockLink_t header. For now, skip. */

    vPortFree(pvReal);
}
```

**Pros:**
- Zero changes to heap_4 internals
- Trivial to maintain across FreeRTOS upgrades
- Clear separation of concerns
- Can be conditionally compiled (`#if configUSE_MTE`)

**Cons:**
- Slightly less efficient (extra loop after allocation)
- `vPortFreeTagged` doesn't know allocation size without reading heap_4 internals or storing it separately
- Adjacent allocations may share the same tag (probabilistic — 1/15 chance)

### Option C: Manual tagging in demo code (USED FOR DEMO)

For the demo, we don't even use the wrapper. We manually tag a buffer (stack or heap) with inline assembly, then deliberately overflow it. This proves MTE works without any allocator changes.

## Configuration Change Required

Regardless of approach, one change is needed in FreeRTOS config:

```c
/* portmacro.h or FreeRTOSConfig.h */
#define portBYTE_ALIGNMENT  16  /* Was 8; MTE requires 16-byte granule alignment */
```

This ensures `pvPortMalloc` returns 16-byte aligned pointers, which is the minimum for MTE tagging to work correctly.

## Decision Rationale

1. **The demo doesn't need allocator integration** — manual `IRG`/`STG` on a buffer proves the hardware feature works.

2. **Production integration is a thin wrapper** — no internal knowledge of heap_4 needed. The wrapper is ~10 lines of code.

3. **Forking heap_4 is premature** — the overhead of a post-allocation STG loop is negligible compared to the allocation itself (heap_4 does free-list traversal, block splitting, etc.).

4. **If revisited**: the trigger would be profiling showing that the STG loop after allocation is a measurable bottleneck in a real workload. In that case, integrating STG into heap_4's block-split path would eliminate the double-touch. But this is unlikely to matter for the target use case (frame buffer allocation at task startup, not hot-path per-frame allocation).

## References

- ARM Architecture Reference Manual, D17.2: Memory Tagging Extension
- Linux kernel MTE documentation: `Documentation/arm64/memory-tagging-extension.rst`
- FreeRTOS heap_4.c: first-fit allocator with block coalescing
- Project file: `src/demo/mte_demo.c` — working demo using manual tagging
