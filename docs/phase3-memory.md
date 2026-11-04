# Phase 3: Memory management

## The problem

Until now the kernel only used memory the linker reserved for it. Processes need stacks, the file system needs space for file contents, and nothing should be able to scribble over kernel code. Phase 3 adds four layers:

| Layer | File | Job |
|---|---|---|
| RAM discovery | `memory/fdt.c` | Read the RAM size from the device tree QEMU provides |
| Page allocator | `memory/pmm.c` | Hand out and take back 4 KiB pages of physical RAM |
| Kernel heap | `memory/heap.c` | `kmalloc`/`kfree` for allocations of any size |
| MMU | `memory/mmu.c` | Page tables: memory types, permissions, and faults on bad addresses |

## 1. Finding the RAM: the device tree

QEMU describes the machine in a *flattened device tree* (FDT): a binary blob of nodes and properties. For an ELF kernel it puts the blob at the start of RAM (`0x40000000`), but only if it fits below the kernel. The virt board's blob is 1 MiB, and Phases 1–2 linked the kernel 512 KiB into RAM, so the blob was silently not loaded. Phase 3 links the kernel at `0x40200000` (2 MiB in) to fix this.

`fdt_find_memory()` walks the blob's tokens (`FDT_BEGIN_NODE`, `FDT_PROP`, `FDT_END_NODE`...) looking for the `/memory@...` node's `reg` property, using the root's `#address-cells`/`#size-cells` to decode it. All values are big-endian. Booting with `-m 512M` reports 512 MiB; nothing is hard-coded.

## 2. Page frame allocator

RAM is divided into 4 KiB pages. A bitmap holds one bit per page (1 = in use): 32,768 pages for 128 MiB, so 4 KiB of bitmap. Pages below `__kernel_end` (device tree and kernel image) are reserved at start.

- `pmm_alloc_pages(n)` finds `n` consecutive free pages (next-fit search), marks them used and zeroes them.
- `pmm_free_pages(addr, n)` clears the bits; it reports double frees and addresses it never handed out.

## 3. Kernel heap

The heap takes 256 pages (1 MiB) from the page allocator and divides them into blocks laid end to end, each with a 16-byte header (`magic`, `free`, `size`).

- `kmalloc`: first fit. The first free block that is big enough is split, and the remainder becomes a new free block.
- `kfree`: marks the block free, then merges adjacent free blocks (*coalescing*) so the heap does not fragment.
- The magic number catches corrupted headers and frees of pointers that never came from `kmalloc`.

## 4. The MMU

KERNELLED uses an *identity map*: every virtual address equals its physical address. The MMU is still essential because every mapping carries attributes.

| Region | Mapping | Permissions | Why |
|---|---|---|---|
| GIC `0x08000000`, UART `0x09000000` | 2 MiB blocks, Device-nGnRnE | read-write, never executable | Device registers must not be cached or merged |
| Everything else below 1 GiB, including address 0 | not mapped | (fault) | NULL pointers and wild addresses fault immediately |
| Kernel `.text` | 4 KiB pages | read-only, executable | Code cannot be modified |
| Kernel `.rodata` | 4 KiB pages | read-only, not executable | Constants are constants |
| All other RAM | 2 MiB blocks / 4 KiB pages, Normal write-back | read-write, never executable | W^X: writable memory is never executable |

Translation uses a 4 KiB granule and 39-bit addresses (`T0SZ = 25`), so tables start at level 1:

```
TTBR0_EL1 -> L1 table   [0] 0-1 GiB  -> L2 "devices"  (2 entries used)
                        [1] 1-2 GiB  -> L2 "RAM"      [kernel's 2 MiB] -> L3 (512 x 4 KiB pages)
                                                      [others] 2 MiB blocks
```

Registers: `MAIR_EL1` (attribute 0 = Device, 1 = Normal cacheable), `TCR_EL1` (granule and size), `TTBR0_EL1` (table address), then `SCTLR_EL1` M (MMU), C and I (caches), A (alignment checking stays on).

## Tests

At boot:
```
[test] page allocator (alloc, zero, free, refuse oversize): PASS
[test] kernel heap (64 blocks, data intact, fully coalesced): PASS
[test] mmu enabled: PASS
```

Deliberate faults (`make run FAULT=...`), all checked by `make test`:

| FAULT | Report |
|---|---|
| `null-pointer` | data abort, translation fault, address `0x0` |
| `write-rodata` | data abort, permission fault level 3, write |
| `data-abort` | data abort, translation fault level 2, address `0x7f000000` |
