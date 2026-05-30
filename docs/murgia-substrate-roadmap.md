# Murgia modern-hardware substrate roadmap

This page is the source-backed NUX inventory for Murgia's modern-hardware,
bootstrap, AHCI, and filesystem direction. It separates what the tracked NUX
source provides today from substrate gaps and future slices. It does not change
Murgia's user-facing ABI and does not claim device, storage, or filesystem
support that is not present in tracked source.

## Evidence scope

This inventory inspected the current tracked NUX tree at base commit
`93780903f6c3d4fa25be7f0ecd9f70eba67a4679` plus the ACPI/platform-facts slice, including:

- APXH and configure paths: `configure.ac`, `apxh/configure.ac`, `apxh/src/*`,
  `apxh/multiboot/*`, `apxh/efi/*`, `apxh/sbi/*`, and `README.md`.
- Public substrate headers: `include/nux/apxh.h`, `include/nux/hal.h`,
  `include/nux/plt.h`, `include/nux/nux.h`, and `include/nux/types.h`.
- Runtime substrate: `libnux/*`.
- x86 HAL/platform: `libhal_x86/*` and `libplt_acpi/*`.
- RISC-V HAL/platform: `libhal_riscv/*` and `libplt_sbi/*`.
- Harness/docs: `example/Makefile.in`, `tools/qemu-smoke-i386.sh`,
  `docs/build-and-run.md`, `docs/hardware-support.md`, and `docs/backlog.md`.

Source/doc searches for ACPI, PCI/PCIe, MCFG/ECAM, HPET/APIC/IOAPIC,
MSI/MSI-X, DMAR/IVRS/IOMMU, DMA remapping, MMIO, physical memory maps, AHCI,
storage, filesystem, and real-disk QEMU support found the capabilities and gaps
below.

## What Murgia can depend on today

- **Configured architecture/platform split.** Top-level `configure.ac` selects
  `libhal_x86` + `libplt_acpi` for `i386` and `amd64`, and `libhal_riscv` +
  `libplt_sbi` for `riscv64`. APXH `apxh/configure.ac` selects `multiboot` for
  `i386`, `multiboot efi` for `amd64`, and `sbi efi` for `riscv64`. The
  currently verified runtime path is still i386/multiboot; non-i386 paths need
  separate build/runtime verification.
- **APXH boot facts.** APXH writes `struct apxh_bootinfo`, `struct apxh_region`,
  `struct apxh_stree`, and `struct apxh_pltdesc` from `include/nux/apxh.h`.
  Common APXH code handles APXH ELF program headers for boot info, S-tree,
  physical-memory regions, PFN map, physmap, framebuffer, and page-table
  allocation areas in `apxh/src/elf.c` and `apxh/src/main.c`.
- **Boot-protocol facts.** The x86 multiboot path converts the multiboot memory
  map and framebuffer into APXH data and returns a `PLT_ACPI` descriptor with an
  RSDP pointer (`apxh/multiboot/mb.c`). APXH EFI records EFI memory-map entries,
  GOP framebuffer data, and an ACPI RSDP as `PLT_ACPI` (`apxh/efi/efi-main.c`,
  `apxh/efi/apxhefi/efi_md.c`). APXH SBI parses DTB memory and
  `reserved-memory` nodes and returns `PLT_DTB` (`apxh/sbi/md.c`).
- **Memory and MMIO primitives.** `include/nux/nux.h` exposes PFN, KVA, KMAP,
  UMAP, and `kva_physmap()` primitives. `libnux/kva.c` maps arbitrary physical
  ranges through KVA/KMAP, and `libnux/pfncache.c` uses the HAL direct map when
  possible. `include/nux/hal.h` exposes HAL physical-memory regions and virtual
  areas. The x86 HAL also appends pinned MMIO regions for PFN 0 and PFN `0xa0`
  length 96 and removes those PFNs from the S-tree allocator (`libhal_x86/x86.c`).
- **x86 ACPI/platform facts.** `libplt_acpi/plt.c` requires a `PLT_ACPI`
  descriptor and now emits `NUX ACPI PLT FACTS:` with the consumed RSDP
  physical address. `libplt_acpi/acpi.c` loads RSDT/XSDT entries, records APIC
  and HPET tables, scans MADT LAPIC/IOAPIC/LAPIC-NMI/interrupt-override entries,
  counts ignored LSAPIC, x2APIC, IOSAPIC, and LX2APICNMI entries, and emits
  deterministic `NUX ACPI FACTS:`, `NUX ACPI MADT FACTS:`,
  `NUX ACPI GSI FACTS:`, and `NUX ACPI HPET FACTS:` serial markers.
  `libplt_acpi/lapic.c`, `ioapic.c`, and `hpet.c` initialize LAPIC, IOAPIC/GSI,
  and HPET timer support.
- **Interrupt/timer interfaces.** `include/nux/plt.h` exposes platform CPU,
  IPI/NMI, IRQ type/enable/disable/max, timer counter/alarm, and EOI operations.
  `libnux/time.c` wraps the PLT timer for NUX users. x86 has LAPIC/IOAPIC/HPET
  implementations; RISC-V has SBI timer calls and software-interrupt-based
  NMI/IPI emulation but incomplete external IRQ/PLIC wiring.
- **RISC-V DTB facts.** `libplt_sbi/sbi.c` requires `PLT_DTB`, validates the DTB,
  reads `/cpus` and `timebase-frequency`, prints PLIC S-mode contexts, uses
  `rdtime` plus SBI calls for timers, and leaves CPU-start/external-IRQ pieces
  as TODO.
- **Current smoke coverage.** `tools/qemu-smoke-i386.sh` performs the reviewed
  out-of-tree i386 configure/build/QEMU serial-marker smoke path. The checked-in
  `example/Makefile.in` QEMU targets use `-kernel example_qemu -serial
  mon:stdio -nographic`; there is no checked-in real-disk-image QEMU path.

## Source-backed substrate matrix

| Murgia substrate need | Current NUX status | Evidence | Next direction |
| --- | --- | --- | --- |
| Boot-path selection and entry handoff | Present for configured paths; only i386/multiboot is currently runtime-smoke verified in this container. | `configure.ac`, `apxh/configure.ac`, `README.md`, `docs/build-and-run.md`, `docs/hardware-support.md` | Preserve i386 coverage; add bounded amd64/riscv64 verification before relying on those paths. |
| APXH boot info, physical-memory regions, PFN map, S-tree, physmap, framebuffer | Present. APXH normalizes boot data into `include/nux/apxh.h` structures and HAL linker-script areas. | `include/nux/apxh.h`, `apxh/src/elf.c`, `apxh/src/main.c`, `libhal_x86/*/exe.ld`, `libhal_riscv/exe.ld` | Keep this contract explicit; add tests that platform facts are present and sane. |
| Public kernel memory mapping primitives for MMIO | Present as low-level primitives, not a device model. | `include/nux/nux.h`, `libnux/kva.c`, `libnux/kmap.c`, `libnux/pfncache.c`, `include/nux/hal.h` | Document/guard intended MMIO usage and failure behavior before exposing higher device discovery to Murgia. |
| x86 ACPI RSDT/XSDT table access | Present internally and serial-inventory logged for the selected RSDT/XSDT root plus APIC/HPET table presence. There is still no public generic ACPI table inventory or MCFG/DMAR/IVRS handoff. | `libplt_acpi/acpi.c`, `libplt_acpi/plt.c`, `tools/qemu-smoke-i386.sh` | Next slice: parse/expose PCIe MCFG/ECAM facts and safe config-space access. |
| x86 MADT, LAPIC, IOAPIC, GSI, HPET | Present for legacy/local APIC, IOAPIC/GSI routing, and HPET timer, with deterministic serial facts for LAPIC/IOAPIC counts, selected LAPIC base, GSI range, HPET init result, and ignored modern APIC entry counts. Modern APIC variants remain unsupported. | `libplt_acpi/acpi.c`, `lapic.c`, `ioapic.c`, `hpet.c`, `include/nux/plt.h`, `tools/qemu-smoke-i386.sh` | Expose richer IRQ routing/controller facts needed by Murgia after MCFG/PCIe facts. |
| RISC-V DTB and timer | Partially present. DTB `/cpus`, `timebase-frequency`, and PLIC contexts are read; timer calls exist. | `apxh/sbi/md.c`, `libplt_sbi/sbi.c`, `libhal_riscv/riscv.c` | Finish or explicitly bound PLIC/external IRQ and secondary CPU work before treating RISC-V as a modern hardware target for Murgia. |
| PCI/PCIe device discovery | Absent. No tracked source implementation of PCI bus walking or PCIe enumeration was found. | Negative tracked-source search; adjacent current code is only ACPI APIC/HPET in `libplt_acpi/acpi.c` | After ACPI facts, parse/expose PCIe MCFG/ECAM facts and safe config-space access. |
| ACPI MCFG / PCIe ECAM | Absent. No MCFG table parsing or ECAM accessor is present. | Negative tracked-source search for `MCFG`/`ECAM`; existing docs already mark this absent. | Add a focused MCFG/ECAM substrate slice before AHCI. |
| MSI/MSI-X | Absent. No MSI/MSI-X controller or PCI capability handling is present. | Negative tracked-source search for MSI/MSI-X implementation; current IRQ code is LAPIC/IOAPIC/GSI. | Defer until PCIe device facts and IRQ-controller facts are explicit. |
| Intel DMAR, AMD IVRS, IOMMU facts | Absent. No DMAR/IVRS table parser, IOMMU abstraction, or current device-to-remapper facts are present. | Negative tracked-source search for `DMAR`, `IVRS`, and `IOMMU`; `docs/murgia-integration.md` records this as future requirement `MURGIA-IOMMU-006`. | Parse discovery facts first, then add DMA-remap primitives behind Murgia's stable ABI. |
| DMA-remapping map/unmap API | Absent. Current memory mapping primitives map CPU virtual memory, not device DMA translations. | `include/nux/nux.h`, `include/nux/hal.h`, `libnux/kva.c`, negative source search for DMA-remap APIs | Add minimal DMA map/unmap primitives only after IOMMU discovery facts are available. |
| AHCI, block storage, filesystem | Absent. No AHCI/SATA/NVMe/virtio block/filesystem implementation was found in tracked source. | Negative tracked-source search; `example/Makefile.in` has no disk image/device options. | Do not start Murgia AHCI/filesystem integration as if NUX PCIe/IRQ/IOMMU substrate already exists. |
| Real-disk-image QEMU coverage | Absent. Checked-in QEMU targets are serial-only `-kernel` runs without drive images. | `example/Makefile.in`, `tools/qemu-smoke-i386.sh`, `docs/build-and-run.md` | Add future disk-image harnesses only as bounded tasks after substrate/device scope is clear. |

## Current gaps for modern hardware bootstrap and storage

1. **ACPI facts are now explicit runtime evidence, but not yet a public device
   substrate.** NUX logs the consumed RSDP, selected RSDT/XSDT root, APIC/HPET
   table presence, MADT LAPIC/IOAPIC/GSI counts, ignored modern APIC entries,
   and HPET init result on the x86 serial path. There is still no public generic
   ACPI table inventory, MCFG/ECAM facts, PCI enumeration, or DMAR/IVRS handoff
   for Murgia to consume.
2. **No PCIe ECAM substrate exists.** Without MCFG parsing, segment/bus range
   facts, and safe config-space access, Murgia cannot reliably discover AHCI or
   other PCIe devices through NUX.
3. **IRQ facts are too narrow for storage devices.** x86 GSI/IOAPIC support is
   present, but PCI INTx routing facts, MSI/MSI-X support, and a clear
   controller/device interrupt handoff are absent.
4. **IOMMU support is only a requirement, not an implementation.** Future NUX
   work must keep IOMMU behavior transparent beneath Murgia's existing
   user-facing device/export ABI (`hwdev`, `sys_export`, `dexport`), but no
   DMAR/IVRS parser or DMA-remapping primitive exists today.
5. **Storage and filesystem work would be premature in NUX.** No tracked AHCI,
   block, filesystem, or real-disk-image QEMU support exists. Murgia can plan
   its stable device ABI, but NUX still needs PCIe/IRQ/IOMMU substrate before a
   modern AHCI/filesystem path has a source-backed foundation.
6. **RISC-V is not a complete modern hardware target yet.** DTB and timer facts
   exist, but `libhal_riscv/riscv.c` and `libplt_sbi/sbi.c` still have TODOs for
   secondary CPU startup, platform CPU enter, IRQ type/enable/disable/max, EOI,
   and external interrupt dispatch.

## Prioritized NUX substrate plan for Murgia

1. **Keep the ACPI/platform-fact markers covered.** The first slice logs a
   bounded table/platform-fact inventory from existing x86 ACPI discovery:
   RSDP/RSDT/XSDT selection, APIC presence, HPET presence/init result,
   LAPIC/IOAPIC counts, GSI range, and ignored modern APIC entry counts. The
   i386 smoke harness requires those serial markers so this does not remain
   prose-only.
2. **Expose PCIe MCFG/ECAM facts and safe config-space access.** Parse ACPI MCFG
   into segment/bus/window records, validate physical ranges through existing
   memory/MMIO primitives, and provide a minimal C-style accessor for config
   reads/writes. Do not implement AHCI policy in this slice.
3. **Expose IRQ routing/controller facts needed by Murgia.** Build on existing
   LAPIC/IOAPIC/GSI data and add the minimal facts Murgia needs to bind device
   interrupts. Treat MSI/MSI-X as a separate follow-up after PCI capability
   access is available.
4. **Add IOMMU discovery facts, then DMA-remap primitives.** First parse and
   report Intel DMAR / AMD IVRS facts without changing Murgia's user ABI. Then
   add internal DMA map/unmap primitives with no-IOMMU fallback behind the same
   Murgia-facing device/export behavior.
5. **Preserve current i386 harness coverage and add future harnesses as bounded
   tasks.** Keep `tools/qemu-smoke-i386.sh` green. Add amd64/riscv64 and later
   disk-image QEMU tests only after their target toolchains, boot paths, and
   substrate scope are independently verified.

## Murgia handoff / blocker

The source-backed blocker for Murgia's AHCI/filesystem direction is not a
Murgia ABI problem; it is the missing NUX modern-hardware substrate. NUX now has
explicit/tested x86 ACPI platform-fact serial evidence, but it still lacks PCIe
MCFG/ECAM access, richer device IRQ routing facts, and IOMMU discovery/DMA-remap
planning. Murgia should not depend on NUX for modern storage-device enumeration
or real-disk-image boot tests until those follow-up substrate slices exist. The
next NUX slice should therefore be MCFG/ECAM facts and safe PCIe config-space
access.
