/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida <glguida@tlbflush.org>

  SPDX-License-Identifier:	BSD-2-Clause
*/

#ifndef _HAL_H
#define _HAL_H

#include <nux/types.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>

/* HAL module specific definitions. */
#include <nux/hal_config.h>


/*
  HAL Abstracted CPU Instructions. 
 */

/*
  I/O ops for architectures with I/O Space. 
 */
unsigned long hal_cpu_in (uint8_t size, uint32_t port);
void hal_cpu_out (uint8_t size, uint32_t port, unsigned long val);

/*
  Relax CPU (spin-wait). 
 */
void hal_cpu_relax (void);

/*
  Trap and generate an exception. 
 */
void hal_cpu_trap (void);

/*
  Return CPU's current cycle counter.
*/
uint64_t hal_cpu_cycles (void);

/*
  Set CPU in idle mode 
 */
void __dead hal_cpu_idle (void);

/*
  Halt the cpu. 
 */
void __dead hal_cpu_halt (void);

/*
  Flush CPU's TLBs 
 */
void hal_cpu_tlbop (hal_tlbop_t op);

/*
  Set current CPU's local data pointer. 
 */
void hal_cpu_setdata (void *data);

/*
  Return current CPU's local data pointer. 
 */
void *hal_cpu_getdata (void);


/*
  Allow reading from user mapped memory.
 */
void hal_useraccess_start (void);

/*
  Do not allow reading from user mapped memory.
 */
void hal_useraccess_end (void);

/*
  HAL CPU Interrupt Vector description.
 */

/*
  Number of vectors available for platform interrupt controller use.
*/
unsigned hal_vect_max (void);


/*
  HAL Physical Memory Description.
 */

/*
  Maximum Physical Page Number.  
 */
unsigned long hal_physmem_maxpfn (void);

/*
  Maximum RAM Physical Page Number.
*/
unsigned long hal_physmem_maxrampfn (void);

/*
  Maximum Number of Memory Regions.
*/
unsigned hal_physmem_numregions (void);

/*
  Retrieve Memory Region.
*/
struct apxh_region *hal_physmem_region (unsigned i);

/*
  S-Tree allocation bitmap.

  The HAL is responsible for building at boot
  time a searchable bitmap (see stree.h) that
  contains an up-to date map of free pages.
*/
void *hal_physmem_stree (unsigned *order);


/*
  HAL Virtual Memory Description.
 */

/*
  User Area description. 
 */
vaddr_t hal_virtmem_userbase (void);
const size_t hal_virtmem_usersize (void);

/*
  Direct Memory Map description. 
 */
vaddr_t hal_virtmem_dmapbase (void);
const size_t hal_virtmem_dmapsize (void);

/*
  PFN Cache Area.

  NB: no, this is not VMS.
*/
vaddr_t hal_virtmem_pfn$base (void);
const size_t hal_virtmem_pfn$size (void);

/*
  Kernel Virtual Area Map Description. 
 */
vaddr_t hal_virtmem_kvabase (void);
const size_t hal_virtmem_kvasize (void);

/*
  Kernel Memory Area Description.
*/
vaddr_t hal_virtmem_kmembase (void);
const size_t hal_virtmem_kmemsize (void);

/*
  Boot-time user entry point.

  If zero there's no boot-time user process.
*/
const vaddr_t hal_virtmem_userentry (void);


/*
  HAL Virtual Memory Mapping.

  The virtual address space is divided in two parts: the kernel
  mappings (KMAP), which are static and shared across all CPUs, and
  user mappings (UMAP), which can be loaded into a CPU's page table.
 */

/*
  Get a pointer to a leaf page-table (L1P) for a kernel mapping.

  VA must be a user address or L1P will be set to L1P_INVALID and
  false will be returned.

  Do a page walk (populating pagetables if ALLOC is true).

  If an l1p exists, save it into L1P and return true.
  If no l1p is found, set L1P to L1P_INVALID and return false.
  If ALLOC=true and there's not enough memory, return false.
*/
bool hal_kmap_getl1p (unsigned long va, bool alloc, hal_l1p_t * l1p);

/*
  Initialize an empty UMAP.
*/
void hal_umap_init (struct hal_umap *umap);

/*
  Initialize the UMAP to contain the user mappings currently mapped.

  Called at boot to save the user mappings loaded by the bootloader
  into a UMAP.
*/
void hal_umap_bootstrap (struct hal_umap *umap);

/*
  Load UMAP mappings in the current CPU.

  Unmap previous user mappings (if any) and set UMAP mappings into the
  current CPU's page table.

  If UMAP == NULL, the user mappings in the current CPUs will be
  unmapped.

  Note that the HAL does not track which UMAP is mapped into a CPU. A
  HAL_UMAP can be mapped in multiple CPUs simultaneously.

  Returns the required TLB operation to update the CPU's TLBs.
*/
hal_tlbop_t hal_umap_load (struct hal_umap *umap);

/*
  Get a pointer to a leaf page-table (L1P) for a user mapping.

  Do a UMAP page walk (populating pagetables if ALLOC is true).

  if UMAP == NULL, the current CPU mappings are retrieved.

  VA must be a user address or L1P will be set to L1P_INVALID and
  false will be returned.

  If an l1p exists, save it into L1P and return true.
  If no l1p is found, set L1P to L1P_INVALID and return false.
  If ALLOC=true and there's not enough memory, return false.
*/
bool hal_umap_getl1p (struct hal_umap *umap, uaddr_t uaddr, bool alloc,
		      hal_l1p_t * l1p);


/*
  Return the next UADDR whose L1P is non-zero.

  if UMAP == NULL, the current CPU mappings are searched.

  If L1P is not NULL, save the L1P of UADDR.
  If L1E is not NULL, save the L1E pointed by the L1P.
 */
uaddr_t hal_umap_next (struct hal_umap *umap, uaddr_t uaddr, hal_l1p_t * l1p,
		       hal_l1e_t * l1e);

/*
  Free all memory associated with this UMAP.

  Doesn't free the pages mapped by this UMAP.
  The umap structure can be discarded after this call.

  NOTE: This function does not check whether the UMAP is mapped to any CPU. 
*/
void hal_umap_free (struct hal_umap *umap);


/*
  HAL L1 Pagetable Entry.

  In NUX L1 page-tables are the leaf page-tables.
*/

/*
  HAL PTE permission bits.
*/
#define HAL_PTE_P      (1 << 0)	/* The data page is present. */
#define HAL_PTE_W      (1 << 1)	/* The data page is writable. */
#define HAL_PTE_X      (1 << 2)	/* The data page is executable. */
#define HAL_PTE_U      (1 << 3)	/* The data page is a user mapping. */
#define HAL_PTE_GLOBAL (1 << 4)	/* The data page is global (will persist across normal tlb flushes). */
#define HAL_PTE_A      (1 << 5) /* Page has been accessed. */
#define HAL_PTE_D      (1 << 6) /* Page has been written to. */
#define HAL_PTE_AVL0   (1 << 7)	/* Available bit */
#define HAL_PTE_AVL1   (1 << 8)	/* Available bit */
#define HAL_PTE_AVL2   (1 << 9)	/* Available bit */

/*
  Create a l1e value 
 */
hal_l1e_t hal_l1e_box (unsigned long pfn, unsigned flags);

/*
  Decompose a l1e value 
 */
void hal_l1e_unbox (hal_l1e_t l1e, unsigned long *pfn, unsigned *prot);

/*
  TLB flush operation for a PT change. 
 */
hal_tlbop_t hal_l1e_tlbop (hal_l1e_t old, hal_l1e_t new);

/*
  Get L1E pointed by L1P.
*/
hal_l1e_t hal_l1e_get (hal_l1p_t l1p);

/*
  Set a L1E in the location pointed by L1P.
 */
hal_l1e_t hal_l1e_set (hal_l1p_t l1p, hal_l1e_t new);



/*
  HAL PCPU: Physical CPU bringup and setup.
 */

/*
  Initialization of HAL PCPU subsystem.

  This is called when the platform and memory management is
  initialized, for operations that cannot be done early at boot but
  must be done before secondary CPUs are started.
*/
void hal_pcpu_init (void);

/*
  Initialisation of HAL CPU <PCPUID>.
  
  Provide the requested data structure to the HAL for its internal use. 
 */
void hal_pcpu_add (unsigned pcpuid, struct hal_cpu *haldata);

/*
  Load HAL-specific status for the current CPU..
  
  This will be called, once for each CPU, at initialisation time. 
 */
void hal_pcpu_enter (unsigned pcpuid);

/*
  Returns address where PCPU bootstrap code starts, or PADDR_INVALID if
  PCPU cannot boot.
 */
paddr_t hal_pcpu_startaddr (unsigned pcpu);


/*
  Hardware Abstraction Layer: Interrupt Frame.

  The HAL communicates information about the status of the CPU when
  an interrupt/exception occurred with the struct hal_frame, which
  contains all the CPU register status.

  This structure contains all the information required to restore the
  CPU on interrupt return, and the HAL provides some abstracted
  modifiers.
*/

/*
  Initialise a HAL frame.

  Instruction Pointer and Stack Pointer are left cleared.
*/
void hal_frame_init (struct hal_frame *f);

/*
  Check that the HAL frame originated in user space.

  NMI, exception and idle can generate frame from kernel mode.
*/
bool hal_frame_isuser (struct hal_frame *f);

/* Set frame instruction pointer */
void hal_frame_setip (struct hal_frame *f, unsigned long ip);

/* Get frame instruction pointer */
unsigned long hal_frame_getip (struct hal_frame *f);

/* Set frame stack pointer */
void hal_frame_setsp (struct hal_frame *f, unsigned long sp);

/* Get frame stack pointer */
unsigned long hal_frame_getsp (struct hal_frame *f);

/*
  Set frame global pointer

  RISCV-64 defines this in their ABI. Other architectures return zero.
*/
void hal_frame_setgp (struct hal_frame *f, unsigned long gp);

/*
  Get frame global pointer

  RISCV-64 defines this in their ABI. Other architectures return zero.
*/
unsigned long hal_frame_getgp (struct hal_frame *f);


/*
  Set frame argument registers

  We define here three HAL-dependent registers, a0, a1, a2, that can be used
  as argument on entry.
*/
void hal_frame_seta0 (struct hal_frame *f, unsigned long a0);
void hal_frame_seta1 (struct hal_frame *f, unsigned long a1);
void hal_frame_seta2 (struct hal_frame *f, unsigned long a2);


/*
  Set frame return value register.

  In case of a syscall, this will allow user code to see a return value.
  The return value register might be one of the arguments.
*/
void hal_frame_setret (struct hal_frame *f, unsigned long r);


/*
  Set TLS pointer.
*/
void hal_frame_settls (struct hal_frame *f, unsigned long r);


/* Print frame information to log. */
void hal_frame_print (struct hal_frame *);

/*
  Hardware Abstraction Layer: System Entries.
 
  The following are callbacks to the main program. These calls are
  the input to the kernel.
 */

#define HAL_PF_REASON_PROT 0
#define HAL_PF_REASON_NOTP 1
#define HAL_PF_REASON_MASK 1
#define HAL_PF_INFO_WRITE (1 << 4)
#define HAL_PF_INFO_USER  (1 << 5)
#define HAL_PF_INFO_EXE   (1 << 6)

typedef unsigned hal_pfinfo_t;

/*
  Page Fault. (can be in kernel) 
 */
struct hal_frame *hal_entry_pf (struct hal_frame *, unsigned long,
				hal_pfinfo_t);

/*
  Exception. (can be in kernel) 
 */
struct hal_frame *hal_entry_xcpt (struct hal_frame *, unsigned);


/*
  IRQ entry.
 */
struct hal_frame *hal_entry_irq (struct hal_frame *f, unsigned irq,
				 bool level);

/*
  Timer.
 */
struct hal_frame *hal_entry_timer (struct hal_frame *f);

/*
  IPIs
*/
struct hal_frame *hal_entry_ipi (struct hal_frame *f);

/*
  System Call 
 */
struct hal_frame *hal_entry_syscall (struct hal_frame *,
				     unsigned long, unsigned long,
				     unsigned long, unsigned long,
				     unsigned long, unsigned long,
				     unsigned long);

/*
  NMI: interrupt to CPU. (can be in kernel)

  Note: This is special as it is not allowed to switch frame.
 */
void hal_entry_nmi (struct hal_frame *f);

/*
  Secondary CPUs start up. 
 */
void hal_main_ap (void);


/*
  Miscellaneous. 
 */

/*
  Boot-time is over.

  This is called by NUX when all initialization is done, to allow HAL
  to clean up boot-time structures.
*/
void hal_init_done (void);

/*
  Boot-time put character to screen. 
 */
int hal_putchar (int c);

/*
  Platform Information acquired from bootloader.

*/
const struct apxh_pltdesc *hal_pltinfo (void);

/*
  Stop all CPUs and panic.
*/
__dead void hal_panic (unsigned cpu, const char *error,
		       struct hal_frame *frame);

#endif /* _HAL_H */
