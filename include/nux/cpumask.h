/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida <glguida@tlbflush.org>

  SPDX-License-Identifier:	BSD-2-Clause
*/


#ifndef NUX_CPUMASK_H
#define NUX_CPUMASK_H

#include <nux/types.h>
#include <nux/nux.h>

static inline void
atomic_cpumask_set (cpumask_t * cpumask, unsigned cpu)
{
  __sync_fetch_and_or (cpumask, (cpumask_t) 1 << cpu);
}

static inline void
atomic_cpumask_and (cpumask_t * cpumask, cpumask_t mask)
{
  __sync_fetch_and_and (cpumask, mask);
}

static inline void
atomic_cpumask_clear (cpumask_t * cpumask, unsigned cpu)
{
  cpumask_t mask = ~((cpumask_t) 1 << cpu);

  atomic_cpumask_and (cpumask, mask);
}

static inline cpumask_t
atomic_cpumask (cpumask_t * cpumask)
{
  return __sync_add_and_fetch (cpumask, 0);
}

static inline bool
atomic_cpumask_get (cpumask_t * cpumask, unsigned cpu)
{
  cpumask_t mask = ((cpumask_t) 1 << cpu);

  return !!(atomic_cpumask (cpumask) & mask);
}

static inline bool
atomic_cpumask_empty (cpumask_t * cpumask)
{
  return atomic_cpumask (cpumask) == 0;
}

static inline bool
cpumask_get (cpumask_t * cpumask, unsigned cpu)
{
  return !!(*cpumask & (1 << cpu));
}

static inline void
cpumask_set (cpumask_t * cpumask, unsigned cpu)
{
  *cpumask |= (1 << cpu);
}

static inline void
cpumask_clear (cpumask_t * cpumask, unsigned cpu)
{
  *cpumask &= ~(1 << cpu);
}

/*
 * FFS, use ffs! 
 */
#define once_cpumask(__mask, __op)					\
	do {								\
		int i = 0;						\
		cpumask_t m = (__mask) & cpu_activemask ();		\
									\
		while (m != 0)						\
		  {							\
		    if (m & 1)						\
		      {							\
			__op;						\
			break;						\
		      }							\
		    else						\
		      {							\
			i++;						\
			m >>= 1;					\
		      }							\
		  }							\
	} while (0)

#define _foreach_cpumask(__mask, __op, __label)				\
	do {								\
		int i = 0;						\
		cpumask_t m = (__mask) & cpu_activemask ();		\
									\
		do {							\
		  if (m & 1) {__op;};					\
		  m >>= 1;						\
		  i++;							\
		} while (m != 0);					\
	} while(0)

#define foreach_cpumask(__mask, __op) _foreach_cpumask((__mask), (__op), )

#endif
