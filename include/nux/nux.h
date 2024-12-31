/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida <glguida@tlbflush.org>

  SPDX-License-Identifier:	BSD-2-Clause
*/

#ifndef _NUX_H
#define _NUX_H

#include <config.h>
#include <nux/defs.h>
#include <nux/types.h>
#include <nux/locks.h>

/*
  Stop all CPUs and panic.
 */
void __dead nux_panic (const char *message, struct hal_frame *f);


/*
  Temporary PFN access.

  Obtain a temporary pointer to a physical page. This is used for
  accessing physical pages for a short period of time, on a single
  CPU.

  Every pointer obtained with a `pfn_get()` call should be released
  with a `pfn_put()` call.

  If the page falls in the direct physical map of the HAL, a pointer
  to the directmap will be returned. Otherwhise a mapping will be
  created in the kernel address space, and this mapping will be cached
  to help in short, frequent mappings of the same page.
*/
void *pfn_get (pfn_t pfn);
void pfn_put (pfn_t pfn, void *va);

void nux_set_allocator (pfn_t (*alloc) (int), void (*free) (pfn_t));
pfn_t pfn_alloc (int low);
void pfn_free (pfn_t pfn);
unsigned long pfn_avail (void);

/*
  The S-tree page allocator.

  Used by default by pfn_alloc and pfn_free, can be changed via
  'nux_set_allocator()'.
*/
pfn_t stree_pfnalloc (int low);
void stree_pfnfree (pfn_t pfn);

vaddr_t kva_alloc (size_t size);
void kva_free (vaddr_t va, size_t size);
void *kva_map (pfn_t pfn, unsigned prot);
void *kva_physmap (paddr_t paddr, size_t size, unsigned prot);
void kva_unmap (void *va, size_t size);

pfn_t kmap_getpfn (vaddr_t va);
pfn_t kmap_map (vaddr_t va, pfn_t pfn, unsigned prot);
pfn_t kmap_map_noalloc (vaddr_t va, pfn_t pfn, unsigned prot);
pfn_t kmap_unmap (vaddr_t va);
int kmap_mapped (vaddr_t va);
int kmap_mapped_range (vaddr_t va, size_t size);
int kmap_ensure (vaddr_t va, unsigned reqprot);
int kmap_ensure_range (vaddr_t va, size_t size, unsigned reqprot);
volatile tlbgen_t kmap_tlbgen (void);
volatile tlbgen_t kmap_tlbgen_global (void);
void kmap_commit (void);

int kmem_brk (int low, vaddr_t vaddr);
vaddr_t kmem_sbrk (int low, long inc);
vaddr_t kmem_brkgrow (int low, unsigned size);
int kmem_brkshrink (int low, unsigned size);
vaddr_t kmem_alloc (int low, size_t size);
void kmem_free (int low, vaddr_t vaddr, size_t size);
#define TRIM_NONE 0
#define TRIM_BRK  1
#define TRIM_HEAP 2
void kmem_trim_setmode (unsigned trim_mode);
void kmem_trim_one (unsigned trim_mode);

void cpu_startall (void);
unsigned cpu_id (void);
unsigned cpu_num (void);
cpumask_t cpu_activemask (void);
void cpu_setdata (void *ptr);
void *cpu_getdata (void);

void cpu_idle (void);

void cpu_nmi (int cpu);
void cpu_nmi_mask (cpumask_t map);
void cpu_nmi_allbutself (void);
void cpu_nmi_broadcast (void);

void cpu_ipi (int cpu);
void cpu_ipi_mask (cpumask_t map);
void cpu_ipi_broadcast (void);

void cpu_tlbflush (int cpu);
void cpu_tlbflush_mask (cpumask_t mask);
void cpu_tlbflush_broadcast (void);
void cpu_tlbflush_broadcast_sync (void);

void cpu_ktlb_update (void);
void cpu_ktlb_reach (tlbgen_t target);

void cpu_stop (int cpu);
void cpu_stop_mask (cpumask_t mask);
void cpu_stop_broadcast (void);

bool cpu_useraccess_copyfrom (void *dst, uaddr_t src, size_t size,
			      bool (*pf_handler) (uaddr_t va,
						  hal_pfinfo_t info));
bool cpu_useraccess_copyto (uaddr_t dst, void *src, size_t size,
			    bool (*pf_handler) (uaddr_t va,
						hal_pfinfo_t info));
bool cpu_useraccess_memset (uaddr_t dst, int ch, size_t size,
			    bool (*pf_handler) (uaddr_t va,
						hal_pfinfo_t info));

umap_t *cpu_umap_current (void);
void cpu_umap_enter (struct umap *umap);
umap_t *cpu_umap_exit (void);


void timer_alarm (uint32_t time_ns);
void timer_clear (void);
uint64_t timer_gettime (void);

void umap_bootstrap (struct umap *umap);
void umap_init (struct umap *umap);
void umap_free (struct umap *umap);
bool umap_map (struct umap *umap, vaddr_t va, pfn_t pfn, unsigned prot,
	       pfn_t * opfn);
unsigned umap_chflags (struct umap *umap, vaddr_t va,
		   unsigned prot_set, unsigned prot_clr);
pfn_t umap_unmap (struct umap *umap, vaddr_t va);
void umap_commit (struct umap *umap);

bool uaddr_valid (uaddr_t);
bool uaddr_validrange (uaddr_t a, size_t size);
bool uaddr_copyfrom (void *dst, uaddr_t src, size_t size,
		     bool (*pf_handler) (uaddr_t va, hal_pfinfo_t info));
bool uaddr_copyto (uaddr_t dst, void *src, size_t size,
		   bool (*pf_handler) (uaddr_t va, hal_pfinfo_t info));
bool uaddr_memset (uaddr_t dst, int ch, size_t size,
		   bool (*pf_handler) (uaddr_t va, hal_pfinfo_t info));

/*
  Get the uctxt of the boot-time user process. Returns false if not present.
*/
bool uctxt_bootstrap (uctxt_t * uctxt);

/*
  Initialize a user context specifying instruction and stack
  pointer.
*/
void uctxt_init (uctxt_t * uctxt, vaddr_t ip, vaddr_t sp, vaddr_t gp);

/*
  Set the instruction pointer of a user context.
*/
void uctxt_setip (uctxt_t * uctxt, vaddr_t ip);

/*
  Get the instruction pointer of a user context.
*/
vaddr_t uctxt_getip (uctxt_t * uctxt);

/*
  Set the stack pointer of a user context.
*/
void uctxt_setsp (uctxt_t * uctxt, vaddr_t sp);

/*
  Get the stack pointer of a user context.
*/
vaddr_t uctxt_getsp (uctxt_t * uctxt);

/*
  Set the global pointer of a user context.
*/
void uctxt_setgp (uctxt_t * uctxt, vaddr_t gp);

/*
  Get the global pointer of a user context.
*/
vaddr_t uctxt_getgp (uctxt_t * uctxt);

void uctxt_setret (uctxt_t * uctxt, unsigned long ret);
void uctxt_seta0 (uctxt_t * uctxt, unsigned long a0);
void uctxt_seta1 (uctxt_t * uctxt, unsigned long a1);
void uctxt_seta2 (uctxt_t * uctxt, unsigned long a2);
void uctxt_settls (uctxt_t * uctxt, unsigned long tls);
void uctxt_print (uctxt_t * uctxt);

#define LOGL_DEBUG -1
#define LOGL_INFO  0
#define LOGL_WARN  1
#define LOGL_ERROR 2
#define LOGL_FATAL 3

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

static inline void
__printflike (2, 3)
__log (const int level, const char *fmt, ...)
{
  va_list ap;

  if (level == LOGL_WARN)
    printf ("Warning: ");
  else if (level == LOGL_FATAL)
    printf ("Fatal: ");
  else if (level == LOGL_ERROR)
    printf ("ERROR: ");

  va_start (ap, fmt);
  vprintf (fmt, ap);
  va_end (ap);

  putchar ('\n');

  if (level == LOGL_FATAL)
    exit (-1);
}

#ifdef DEBUG
#define debug(...) __log(LOGL_DEBUG, __VA_ARGS__)
#else
#define debug(...)
#endif
#define info(...) __log(LOGL_INFO, __VA_ARGS__)
#define warn(...) __log(LOGL_WARN, __VA_ARGS__)
#define error(...) __log(LOGL_ERROR, __VA_ARGS__)
#define fatal(...) __log(LOGL_FATAL, __VA_ARGS__)

/*
  External Interface of NUX.

  This is the interface that has to be implemented by the main program
  in order to use NUX.
*/

/*
  NUX Exit Values

  The libec call exit () will halt the system except for the exit
 values listed below.
*/

/* Halt the current cpu. */
#define EXIT_HALT 0

/* Set the current cpu to idle. */
#define EXIT_IDLE 1

/*
  NUX Kernel main functions.

  A NUX kernel has two main functions:

  1. main: this is the function called at initialisation by the
     primary processor. When main starts, no other processor is
     running. Its role is to initialise the whole kernel, start
     secondary processors (if desired) and exit.

  2 main_ap: this is the function called when a secondary processor
    start. At this time it is safe to assume that all other processors
    are running, with the primary being fully initialised. The role of
    this function is to initialise cpu-specific kernel state, and exit.

  The return value of these functions is important because it gets
  passed to the exit() call. See Exit Values.
*/

/*
  The main program entry.

  This will be called at startup, after HAL and PLT have been
  initialised.
*/
int main (int argc, char *argv[]);

/*
  Entry for Secondary Processors.

  If the primary CPU, running the main program, starts secondary
  processors, after initialization is complete will call this
  function.
*/
int main_ap (void);

/*
  NUX kernel entries.

  The following functions are entries in the system. These are the events
  to which a kernel has to react.

  A NUX kernel is essentially an event based system, and the following are
  the events a NUX kernel will have to respond to.

  Every entry function takes a uctxt in input and returns a
  uctxt. The two uctxt doesn't have to be the same.

  The uctxt in input contains the user context of the interrupted
  process, and will be NULL if the entry was generated while the CPU
  was idle.

  The uctxt returned contains the user context of the process that
  will be restored on re-entry. If NULL, the CPU will be put in
  idle mode.
*/

/*
  Entry for Syscall
*/
uctxt_t *entry_sysc (uctxt_t * u,
		     unsigned long a1, unsigned long a2, unsigned long a3,
		     unsigned long a4, unsigned long a5, unsigned long a6,
		     unsigned long a7);

/*
  Entry for Page Fault
*/
uctxt_t *entry_pf (uctxt_t * u, vaddr_t va, hal_pfinfo_t pfi);

/*
  Entry for Generic Exception
*/
uctxt_t *entry_ex (uctxt_t * u, unsigned ex);

/*
  Entry for Platform Alarm.

  This function will be called whenever the platform alarm is fired.
*/
uctxt_t *entry_alarm (uctxt_t * f);

/*
  Entry for IPI.

  This function will be called on IPI.
*/
uctxt_t *entry_ipi (uctxt_t * f);

/*
 Entry for IRQ.

 This function will be called on IRQ.
*/
uctxt_t *entry_irq (uctxt_t * f, unsigned irq, bool lvl);



#endif
