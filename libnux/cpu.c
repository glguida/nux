/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida, glguida@tlbflush.org

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  See COPYING file for the full license.

  SPDX-License-Identifier:	GPL2.0+
*/


#include <assert.h>
#include <string.h>

#include <nux/types.h>
#include <nux/hal.h>
#include <nux/plt.h>
#include <nux/nux.h>
#include <nux/cpumask.h>

#include "internal.h"

static unsigned number_cpus = 0;
static unsigned cpu_phys_to_id[HAL_MAXCPUS] = { -1, };
static struct cpu_info *cpus[HAL_MAXCPUS] = { 0, };

static cpumask_t tlbmap = 0;
static cpumask_t cpus_active = 0;

/* We use this struct during bootstrap before the cpu infrastructure has been initialised. The CPU number is zero. */
struct cpu_info __boot_cpuinfo = { 0, };

/* NUXST: OKPLT */
static int
cpu_add (uint16_t physid)
{
  int id;
  struct cpu_info *cpuinfo;

  assert (nux_status () & NUXST_OKPLT);
  if (physid >= HAL_MAXCPUS)
    {
      warn ("CPU Phys ID %02x too big. Skipping.", physid);
      return -1;
    }

  if (number_cpus >= HAL_MAXCPUS)
    {
      warn ("Too many CPUs. Skipping.");
      return -1;
    }

  id = number_cpus++;
  info ("Found CPU %d (PHYS:%d)", id, physid);

  /* We are at init-time. We use LOW KMEM via BRK. */
  cpuinfo = (struct cpu_info *) kmem_brkgrow (1, sizeof (struct cpu_info));
  cpuinfo->cpu_id = id;
  cpuinfo->phys_id = physid;
  cpuinfo->self = cpuinfo;
  hal_pcpu_add (physid, &cpuinfo->hal_cpu);

  cpus[id] = cpuinfo;
  cpu_phys_to_id[physid] = id;
  return id;
}

/* NUXST: OKPLT */
void
cpu_init (void)
{
  unsigned pcpu;

  /* Add all CPUs found in the platform. */
  while ((pcpu = plt_pcpu_iterate ()) != PLT_PCPU_INVALID)
    cpu_add (pcpu);

  hal_pcpu_init ();

  cpu_enter ();
}

/* NUXST: OKCPU */
static unsigned
cpu_idfromphys (unsigned physid)
{
  unsigned id;

  assert (physid < HAL_MAXCPUS);
  id = cpu_phys_to_id[physid];
  assert (id < HAL_MAXCPUS);
  return id;
}

/* NUXST: OKPLT */
static struct cpu_info *
cpu_getinfo (unsigned id)
{

  if (id >= HAL_MAXCPUS)
    {
      error ("CPU ID %d too big", id);
      return NULL;
    }
  else if (id >= number_cpus)
    {
      error ("Requested non-active cpu %d", id);
      return NULL;
    }
  return cpus[id];
}

/* NUXST: OKCPU */
static struct cpu_info *
cpu_curinfo (void)
{
  return (struct cpu_info *) hal_cpu_getdata ();
}

/* NUXST: OKPLT */
void
cpu_enter (void)
{
  struct cpu_info *cpu;
  unsigned pcpuid, cpuid;

  /* Setup Platform support for local CPU operations */
  plt_pcpu_enter ();

  pcpuid = plt_pcpu_id ();

  /* Setup local CPU HAL operations. */
  hal_pcpu_enter (pcpuid);

  /* Set per-cpu data. */
  cpuid = cpu_idfromphys (pcpuid);
  cpu = cpu_getinfo (cpuid);
  hal_cpu_setdata ((void *) cpu);

  /* Setup CPU idle loop. */
  if (setjmp (cpu->idlejmp))
    {
      /* From a longjmp, OKCPU post here. */
      cpu_curinfo ()->idle = true;
      hal_cpu_idle ();
    }

  /* Mark as active */
  atomic_cpumask_set (&cpus_active, cpuid);
  /* From now on we can receive NMIs. */

  /* Check if the system hit a panic before we could receive the NMI. */
  if (__predict_false (nux_status () & NUXST_PANIC))
    {
      hal_cpu_halt ();
      /* Unreachable */
    }

  info ("CPU %d set as active", cpuid);
}


/* NUXST: OKCPU */
bool
cpu_wasidle (void)
{
  return cpu_curinfo ()->idle;
}

/* NUXST: OKCPU */
void
cpu_clridle (void)
{
  cpu_curinfo ()->idle = false;
}

/* NUXST: OKCPU */
void
cpu_startall (void)
{
  unsigned pcpu;

  while ((pcpu = plt_pcpu_iterate ()) != PLT_PCPU_INVALID)
    {
      paddr_t start;

      if (pcpu == plt_pcpu_id ())
	continue;

      if (pcpu >= HAL_MAXCPUS)
	continue;

      info ("Starting CPU %d", cpu_idfromphys (pcpu));

      start = hal_pcpu_startaddr (pcpu);
      if (start != PADDR_INVALID)
	{
	  plt_pcpu_start (pcpu, start);
	}
      else
	{
	  warn ("HAL can't prepare for boot CPU %d", cpu_idfromphys (pcpu));
	}
    }
}


/* NUXST: OKCPU */
cpumask_t
cpu_activemask (void)
{
  cpumask_t mask = atomic_cpumask (&cpus_active);

  return mask;
}

/* NUXST: OKCPU */
unsigned
cpu_id (void)
{
  return cpu_curinfo ()->cpu_id;
}

/* NUXST: any */
unsigned
cpu_try_id (void)
{
  if (__predict_false (!(nux_status () & NUXST_OKCPU)))
    {
      return 0;
    }
  else
    {
      struct cpu_info *ci = cpu_curinfo ();
      return ci->cpu_id;
    }
}

/* NUXST: OKPLT */
void
cpu_setdata (void *ptr)
{
  cpu_curinfo ()->data = ptr;
}

/* NUXST: OKCPU */
void *
cpu_getdata (void)
{
  return cpu_curinfo ()->data;
}

/* NUXST: OKCPU */
unsigned
cpu_num (void)
{
  return number_cpus;
}

/* NUXST: OKCPU */
void
cpu_nmi (int cpu)
{
  struct cpu_info *ci = cpu_getinfo (cpu);

  if (ci != NULL)
    plt_pcpu_nmi (ci->phys_id);
}

/* NUXST: OKCPU */
void
cpu_nmi_mask (cpumask_t map)
{
  foreach_cpumask (map, cpu_nmi (i));
}

/* NUXST: any */
void
cpu_nmi_allbutself (void)
{
  if (__predict_true (nux_status () & NUXST_OKCPU))
    {
      cpumask_t mask = cpu_activemask ();
      cpumask_clear (&mask, cpu_id ());
      cpu_nmi_mask (mask);
    }
}

/* NUXST: OKCPU */
void
cpu_nmi_broadcast (void)
{
  cpu_tlbflush_mask (cpu_activemask ());
}

/* NUXST: any */
unsigned
cpu_ipi_base (void)
{
  return hal_vect_ipibase ();
}

/* NUXST: any */
unsigned
cpu_ipi_avail (void)
{
  unsigned vectmax = hal_vect_max ();
  unsigned ipibase = hal_vect_ipibase ();
  unsigned irqbase = hal_vect_irqbase ();
  unsigned ipilimit = ipibase < irqbase ? irqbase : vectmax;

  return ipilimit - ipibase;
}

/* NUXST: OKCPU */
void
cpu_ipi (int cpu, uint8_t vct)
{
  struct cpu_info *ci = cpu_getinfo (cpu);

  if (ci != NULL)
    plt_pcpu_ipi (ci->phys_id, vct);
}

/* NUXST: OKCPU */
void
cpu_ipi_broadcast (uint8_t vct)
{
  plt_pcpu_ipiall (vct);
}

/* NUXST: OKCPU */
void
cpu_ipi_mask (cpumask_t map, uint8_t vct)
{
  foreach_cpumask (map, cpu_ipi (i, vct));
}

/* NUXST: OKCPU */
void
cpu_idle (void)
{
  struct cpu_info *ci = cpu_curinfo ();

  longjmp (ci->idlejmp, 1);
}

/*
  NUXST: OKCPU
  Can be called by NMI.
*/
void
cpu_ktlb_update ()
{
  struct cpu_info *ci = cpu_curinfo ();
  tlbgen_t cpu_global, cpu_normal;
  __atomic_load (&ci->ktlb.global, &cpu_global, __ATOMIC_RELAXED);
  __atomic_load (&ci->ktlb.normal, &cpu_normal, __ATOMIC_RELAXED);
  tlbgen_t kglobal = ktlbgen_global ();
  tlbgen_t knormal = ktlbgen_normal ();

  if (tlbgen_cmp (kglobal, cpu_global) > 0)
    {
      hal_cpu_tlbop (HAL_TLBOP_FLUSHALL);
      /*
         Ignore if CPU's tlbgens have been modified. This means an NMI
         has modified it in the meanwhile.

         Both failure and success case are relaxed because these
         variable are per cpu and accessed with relaxed order.
       */
      __atomic_compare_exchange (&ci->ktlb.global, &cpu_global, &kglobal,
				 false, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
      __atomic_compare_exchange (&ci->ktlb.normal, &cpu_normal, &knormal,
				 false, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    }
  else if (tlbgen_cmp (knormal, cpu_normal) > 0)
    {
      hal_cpu_tlbop (HAL_TLBOP_FLUSH);
      __atomic_compare_exchange (&ci->ktlb.normal, &cpu_normal, &knormal,
				 false, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    }
}

/* NUXST: OKCPU */
void
cpu_ktlb_reach (tlbgen_t target)
{
  tlbgen_t cur = ktlbgen_normal ();

  if (tlbgen_cmp (target, cur) > 0)
    {
      cpu_ktlb_update ();
    }
}

/*
  NUXST: OKCPU 
  Called from NMI.
*/
void
cpu_nmiop (void)
{
  struct cpu_info *ci = cpu_curinfo ();
  unsigned nmiop = ci->nmiop;

  if (nmiop & NMIOP_TLBFLUSH)
    {
      cpu_ktlb_update ();
    }

  atomic_cpumask_clear (&tlbmap, cpu_id ());
}

/* NUXST: OKPLT */
void
cpu_tlbflush (int cpu)
{
  struct cpu_info *ci = cpu_getinfo (cpu);
  if (ci != NULL)
    return;

  __atomic_or_fetch (&ci->nmiop, NMIOP_TLBFLUSH, __ATOMIC_RELAXED);
  cpu_nmi (cpu);
}

/* NUXST: OKPLT */
void
cpu_tlbflush_mask (cpumask_t mask)
{
  foreach_cpumask (mask, cpu_tlbflush (i));
}

/* NUXST: any */
void
cpu_tlbflush_broadcast (void)
{
  if (__predict_true (nux_status () & NUXST_OKCPU))
    {
      cpu_tlbflush_mask (cpu_activemask ());
    }
  else
    {
      /*
         PLT code might call kva_map() to map pages at startup.
         When PLT starts the CPU subsystem hasn't started yet.
         Flush only the local TLBs, but globally.
       */
      hal_cpu_tlbop (HAL_TLBOP_FLUSHALL);
    }
}


static void
cpu_useraccess_reset (void)
{
  struct cpu_info *ci = cpu_curinfo ();

  ci->usrpgaddr = 0;
  ci->usrpginfo = 0;
  __insn_barrier ();
}

static void
cpu_useraccess_end (void)
{
  struct cpu_info *ci = cpu_curinfo ();

  ci->usrpgaddr = 0;
  ci->usrpginfo = 0;
  ci->usrpgfault = 0;
  __insn_barrier ();
}

bool
cpu_useraccess_copyfrom (void *dst, uaddr_t src, size_t size,
			 bool (*pf_handler) (uaddr_t va, hal_pfinfo_t info))
{
  struct cpu_info *ci = cpu_curinfo ();

  if (!uaddr_validrange (src, size))
    return false;

  ci->usrpgfault = 1;
  __insn_barrier ();
  if (setjmp (ci->usrpgfaultctx) != 0)
    {
      uaddr_t uaddr = ci->usrpgaddr;
      hal_pfinfo_t pfinfo = ci->usrpginfo;

      if (!pf_handler || !pf_handler (uaddr, pfinfo))
	{
	  cpu_useraccess_end ();
	  return false;
	}
      cpu_useraccess_reset ();
      // pass-through
    }

  memcpy (dst, (void *) src, size);

  cpu_useraccess_end ();
  return true;
}

bool
cpu_useraccess_copyto (uaddr_t dst, void *src, size_t size,
		       bool (*pf_handler) (uaddr_t va, hal_pfinfo_t info))
{
  struct cpu_info *ci = cpu_curinfo ();

  if (!uaddr_validrange (dst, size))
    return false;

  ci->usrpgfault = 1;
  __insn_barrier ();
  if (setjmp (ci->usrpgfaultctx) != 0)
    {
      uaddr_t uaddr = ci->usrpgaddr;
      hal_pfinfo_t pfinfo = ci->usrpginfo;

      if (!pf_handler || !pf_handler (uaddr, pfinfo))
	{
	  cpu_useraccess_end ();
	  return false;
	}
      cpu_useraccess_reset ();
      // pass-through
    }

  memcpy ((void *) dst, src, size);

  cpu_useraccess_end ();
  return true;
}

bool
cpu_useraccess_memset (uaddr_t dst, int ch, size_t size,
		       bool (*pf_handler) (uaddr_t va, hal_pfinfo_t info))
{
  struct cpu_info *ci = cpu_curinfo ();

  if (!uaddr_validrange (dst, size))
    return false;

  ci->usrpgfault = 1;
  __insn_barrier ();
  if (setjmp (ci->usrpgfaultctx) != 0)
    {
      uaddr_t uaddr = ci->usrpgaddr;
      hal_pfinfo_t pfinfo = ci->usrpginfo;

      if (!pf_handler || !pf_handler (uaddr, pfinfo))
	{
	  cpu_useraccess_end ();
	  return false;
	}
      cpu_useraccess_reset ();
      // pass-through
    }

  memset ((void *) dst, ch, size);

  cpu_useraccess_end ();
  return true;
}

void
cpu_useraccess_checkpf (uaddr_t addr, hal_pfinfo_t info)
{
  struct cpu_info *ci = cpu_curinfo ();

  if (ci->usrpgfault)
    {
      ci->usrpgaddr = addr;
      ci->usrpginfo = info;
      __insn_barrier ();
      longjmp (ci->usrpgfaultctx, 1);
      /* Not reached */
    }
}
