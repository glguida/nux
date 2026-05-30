# Murgia modern-hardware boundary roadmap

This page is the source-backed NUX inventory for Murgia's modern-hardware,
bootstrap, AHCI, and filesystem direction. It records the approved NUX/Murgia
boundary: APXH passes a typed platform descriptor (`struct apxh_pltdesc` with
`type` and `pltptr`) to the selected platform stack, and the selected HAL/PLT
code consumes only the boot data it needs internally. NUX does not export raw
ACPI tables, an ACPI table inventory, MCFG/ECAM records, DMAR/IVRS records, or a
public ACPI/platform-fact substrate for Murgia. ACPI parsing above the basic
platform pointer, PCI/PCIe policy, device policy, AHCI, and filesystem decisions
belong to the kernel/userspace side; for this program that means Murgia.

## Evidence scope

This inventory inspected the current tracked NUX tree at base commit
`8e524502fe06d285304193badd666d90daf8e696`, including:

- APXH and configure paths: `configure.ac`, `apxh/configure.ac`, `apxh/src/*`,
  `apxh/multiboot/*`, `apxh/efi/*`, `apxh/sbi/*`, and `README.md`.
- Public headers: `include/nux/apxh.h`, `include/nux/hal.h`,
  `include/nux/plt.h`, `include/nux/nux.h`, and `include/nux/types.h`.
- Runtime code: `libnux/*`.
- x86 HAL/platform: `libhal_x86/*` and `libplt_acpi/*`.
- RISC-V HAL/platform: `libhal_riscv/*` and `libplt_sbi/*`.
- Harness/docs: `example/Makefile.in`, `tools/qemu-smoke-i386.sh`,
  `docs/build-and-run.md`, `docs/hardware-support.md`, and `docs/backlog.md`.

Source/doc searches for ACPI, PCI/PCIe, MCFG/ECAM, HPET/APIC/IOAPIC,
MSI/MSI-X, DMAR/IVRS/IOMMU, DMA remapping, MMIO, physical memory maps, AHCI,
storage, filesystem, and real-disk QEMU support found the capabilities and gaps
below.

## Boundary Murgia can depend on today

- **Configured architecture/platform split.** Top-level `configure.ac` selects
  `libhal_x86` + `libplt_acpi` for `i386` and `amd64`, and `libhal_riscv` +
  `libplt_sbi` for `riscv64`. APXH `apxh/configure.ac` selects `multiboot` for
  `i386`, `multiboot efi` for `amd64`, and `sbi efi` for `riscv64`. The
  currently verified runtime path is still i386/multiboot; non-i386 paths need
  separate build/runtime verification.
- **APXH boot contract.** APXH writes `struct apxh_bootinfo`,
  `struct apxh_region`, `struct apxh_stree`, and `struct apxh_pltdesc` from
  `include/nux/apxh.h`. Common APXH code handles APXH ELF program headers for
  boot info, S-tree, physical-memory regions, PFN map, physmap, framebuffer,
  and page-table allocation areas in `apxh/src/elf.c` and `apxh/src/main.c`.
- **Typed platform pointer handoff.** The x86 multiboot path fills a `PLT_ACPI`
  descriptor with an RSDP pointer (`apxh/multiboot/mb.c`), EFI records an RSDP
  as `PLT_ACPI` (`apxh/efi/efi-main.c`, `apxh/efi/apxhefi/efi_md.c`), and SBI
  records a DTB as `PLT_DTB` (`apxh/sbi/md.c`). The descriptor is a private
  APXH-to-HAL/PLT boot handoff, not a Murgia-visible ACPI or DTB export API.
- **Internal platform consumption.** `libplt_acpi/plt.c` accepts only
  `PLT_ACPI`, maps the RSDP internally, and initializes the x86 APIC/IOAPIC/HPET
  platform stack. `libplt_sbi/sbi.c` accepts only `PLT_DTB` and maps the DTB
  internally for RISC-V timer/PLIC setup. Neither path publishes a raw table
  inventory or a general hardware-description API to kernels.
- **Memory and MMIO primitives.** `include/nux/nux.h` exposes PFN, KVA, KMAP,
  UMAP, and `kva_physmap()` primitives. These are low-level kernel memory
  mapping primitives, not device discovery or ACPI interpretation services.
- **Interrupt/timer interfaces.** `include/nux/plt.h` provides platform CPU,
  IPI/NMI, IRQ type/enable/disable/max, timer counter/alarm, and EOI operations.
  x86 has LAPIC/IOAPIC/HPET implementations; RISC-V has SBI timer calls and
  software-interrupt-based NMI/IPI emulation but incomplete external IRQ/PLIC
  wiring.
- **Current smoke coverage.** `tools/qemu-smoke-i386.sh` performs the reviewed
  out-of-tree i386 configure/build/QEMU serial-marker smoke path.
  `tools/qemu-smoke-amd64.sh` captures the standardized local amd64 override
  path with `TOOLCHAIN=x86_64-linux-gnu` and `TOOLCHAIN32=i686-unknown-elf`.
  The checked-in `example/Makefile.in` QEMU targets use `-kernel example_qemu
  -serial mon:stdio -nographic`; there is no checked-in real-disk-image QEMU
  path.

## Boundary matrix

| Area | Current NUX status | Approved boundary | Evidence |
| --- | --- | --- | --- |
| Boot-path selection and entry handoff | Present for configured paths; i386/multiboot has default-toolchain smoke coverage, and amd64/multiboot has reviewed override-path smoke coverage plus a checked-in harness. | Preserve the APXH boot contract and verify target paths before relying on them. | `configure.ac`, `apxh/configure.ac`, `README.md`, `docs/build-and-run.md`, `docs/hardware-support.md`, `tools/qemu-smoke-i386.sh`, `tools/qemu-smoke-amd64.sh` |
| APXH boot info, physical-memory regions, PFN map, S-tree, physmap, framebuffer | Present. APXH normalizes boot data into `include/nux/apxh.h` structures and HAL linker-script areas. | This is the public boot-data contract; keep it explicit and tested. | `include/nux/apxh.h`, `apxh/src/elf.c`, `apxh/src/main.c`, `libhal_x86/*/exe.ld`, `libhal_riscv/exe.ld` |
| Typed platform descriptor | Present as `struct apxh_pltdesc` with `PLT_ACPI`/`PLT_DTB` and `pltptr`. | Internal APXH-to-HAL/PLT handoff only; do not turn it into raw ACPI/DTB export or a table inventory. | `include/nux/apxh.h`, `apxh/multiboot/mb.c`, `apxh/efi/apxhefi/efi_md.c`, `apxh/sbi/md.c` |
| Public kernel memory mapping primitives for MMIO | Present as low-level primitives, not a device model. | Kernels may use mapping primitives, but NUX does not infer PCI/ACPI/device policy for Murgia. | `include/nux/nux.h`, `libnux/kva.c`, `libnux/kmap.c`, `libnux/pfncache.c`, `include/nux/hal.h` |
| x86 ACPI MADT/HPET handling | Present and internal. NUX loads ACPI tables needed by `libplt_acpi`, records/uses APIC and HPET today, and ignores several modern APIC variants. | Keep ACPI table use inside the selected platform library; do not publish a table inventory, MCFG/ECAM facts, or DMAR/IVRS facts as NUX API. | `libplt_acpi/acpi.c`, `libplt_acpi/plt.c`, `lapic.c`, `ioapic.c`, `hpet.c` |
| RISC-V DTB and timer | Partially present. DTB `/cpus`, `timebase-frequency`, and PLIC contexts are read; timer calls exist. | DTB use remains internal to `libplt_sbi`; finish or bound PLIC/external IRQ and secondary CPU work separately. | `apxh/sbi/md.c`, `libplt_sbi/sbi.c`, `libhal_riscv/riscv.c` |
| PCI/PCIe device discovery and ACPI MCFG / PCIe ECAM | Absent. No tracked source implementation of PCI bus walking, PCIe enumeration, MCFG parsing, or ECAM access was found. | Murgia/kernel/userspace owns PCIe/MCFG interpretation above the NUX platform pointer boundary. Do not make this depend on NUX exporting ACPI tables. | Negative tracked-source search; adjacent current code is only ACPI APIC/HPET in `libplt_acpi/acpi.c` |
| MSI/MSI-X and device IRQ policy | Absent. No MSI/MSI-X controller or PCI capability handling is present. | NUX may improve internal IRQ-controller support, but device policy and PCI capability interpretation are not a NUX ACPI-fact API. | Negative tracked-source search for MSI/MSI-X implementation; current IRQ code is LAPIC/IOAPIC/GSI. |
| Intel DMAR, AMD IVRS, IOMMU policy | Absent. No DMAR/IVRS table parser, IOMMU abstraction, or device-to-remapper policy is present. | Murgia owns IOMMU discovery/policy above the boundary unless a separate reviewed NUX primitive is requested; never export raw ACPI tables for it. | Negative tracked-source search for `DMAR`, `IVRS`, and `IOMMU`; `docs/murgia-integration.md` records the transparent user-facing ABI constraint. |
| DMA-remapping map/unmap API | Absent. Current memory mapping primitives map CPU virtual memory, not device DMA translations. | Do not describe this as an ACPI/platform-fact substrate. Any future generic DMA helper must be specified separately from ACPI table export. | `include/nux/nux.h`, `include/nux/hal.h`, `libnux/kva.c`, negative source search for DMA-remap APIs |
| AHCI, block storage, filesystem | Absent. No AHCI/SATA/NVMe/virtio block/filesystem implementation was found in tracked source. | Murgia storage/filesystem work must not wait for or require NUX to expose ACPI/PCI facts; NUX does not implement storage policy here. | Negative tracked-source search; `example/Makefile.in` has no disk image/device options. |
| Real-disk-image QEMU coverage | Absent. Checked-in QEMU targets are serial-only `-kernel` runs without drive images. | Add future disk-image harnesses only as bounded tasks with explicit ownership; they are not evidence of a NUX ACPI export API. | `example/Makefile.in`, `tools/qemu-smoke-i386.sh`, `docs/build-and-run.md` |

## Current gaps and ownership

1. **Raw ACPI tables are not a NUX export.** NUX can load the ACPI tables needed
   internally by `libplt_acpi` to initialize x86 LAPIC/IOAPIC/HPET support, but
   it does not publish a table inventory or MCFG/DMAR/IVRS handoff for Murgia.
   Do not add one as the next NUX slice.
2. **PCIe/MCFG interpretation belongs above the boundary.** No NUX PCIe ECAM
   support exists today. Future Murgia PCIe/AHCI work must parse or obtain the
   needed device information on the Murgia/kernel/userspace side, using NUX only
   for existing boot data and low-level memory/interrupt primitives.
3. **IRQ/controller support is narrower than modern storage needs.** x86
   GSI/IOAPIC support is present, but PCI INTx routing, MSI/MSI-X, and device
   interrupt policy are not a published NUX hardware-description service.
4. **IOMMU behavior is a Murgia policy requirement, not current NUX support.**
   Murgia can keep one user-facing device/export ABI for IOMMU-present and
   no-IOMMU systems, but NUX currently has no DMAR/IVRS parser, IOMMU policy, or
   DMA-remapping primitive, and it must not expose raw ACPI tables to fill that
   gap.
5. **Storage and filesystem work are not NUX capabilities.** No tracked AHCI,
   block, filesystem, or real-disk-image QEMU support exists. Murgia can plan
   those pieces without treating NUX as an ACPI/PCI fact provider.
6. **RISC-V is not a complete modern hardware target yet.** DTB and timer use
   exist inside `libplt_sbi`, but `libhal_riscv/riscv.c` and `libplt_sbi/sbi.c`
   still have TODOs for secondary CPU startup, platform CPU enter, IRQ
   type/enable/disable/max, EOI, and external interrupt dispatch.

## Prioritized plan under the approved boundary

1. **Guard the typed platform pointer boundary.** Keep `struct apxh_pltdesc` as
   the APXH-to-HAL/PLT handoff (`PLT_ACPI`, `PLT_DTB`, and `pltptr`). Add or
   update tests only to verify that selected platform libraries receive and use
   their expected descriptor internally; do not log or export ACPI table
   inventories for Murgia.
2. **Keep NUX platform work internal and hardware-abstraction focused.** x86
   APIC/IOAPIC/HPET/x2APIC work and RISC-V PLIC/timer work should improve the
   selected platform library and `include/nux/plt.h` behavior, not create a
   general hardware-description API.
3. **Route Murgia PCIe/MCFG/IOMMU/AHCI/filesystem policy to Murgia.** Future
   Murgia modern-storage work must not depend on NUX exposing ACPI tables,
   MCFG/ECAM records, DMAR/IVRS records, or a public platform-fact inventory.
4. **Specify any new generic NUX primitive separately.** If Murgia later needs a
   concrete NUX memory, interrupt, or DMA helper, record an explicit NUX-side
   API proposal and acceptance criteria. Keep that proposal separate from raw
   ACPI/device-policy export.
5. **Preserve current i386 harness coverage and add future harnesses as bounded
   tasks.** Keep `tools/qemu-smoke-i386.sh` green. Add amd64/riscv64 and later
   disk-image QEMU tests only after their target toolchains, boot paths, and
   ownership boundaries are independently verified.

## Murgia handoff / blocker

Murgia's AHCI/filesystem direction is not blocked on NUX exposing ACPI tables or
publishing a platform-fact inventory, because NUX must not do either. The stable
handoff is the existing APXH boot data plus typed platform descriptor consumed by
the selected HAL/PLT internally. Murgia/kernel/userspace owns ACPI parsing,
PCIe/MCFG interpretation, IOMMU/device policy, AHCI, filesystem, and other
higher-level hardware decisions above that boundary.
