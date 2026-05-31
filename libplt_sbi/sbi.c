#include <nux/plt.h>
#include <nux/nmiemul.h>
#include <nux/apxh.h>
#include <nux/hal.h>
#include <nux/nux.h>
#include <libfdt.h>
#include <string.h>

static uint64_t timebase_frequency = 0;

#define PLIC_PRIORITY_BASE      0x000000UL
#define PLIC_ENABLE_BASE        0x002000UL
#define PLIC_ENABLE_STRIDE      0x80UL
#define PLIC_CONTEXT_BASE       0x200000UL
#define PLIC_CONTEXT_STRIDE     0x1000UL
#define PLIC_CONTEXT_THRESHOLD  0x0UL
#define PLIC_CONTEXT_CLAIM      0x4UL

/* The standard SiFive/RISC-V PLIC layout leaves source 0 unused. */
#define PLIC_MAX_SOURCES        1023U
#define PLIC_FALLBACK_SOURCES   31U
#define PLIC_MAX_MAP_LENGTH     (16ULL << 20)
#define PLIC_INVALID_CONTEXT    ((unsigned) -1)

static struct plt_cpu
{
  bool present;
  uint64_t hartid;
  bool plic_ctx_valid;
  unsigned plic_ctx;          /* S-mode PLIC context. */
} pltcpus[HAL_MAXCPUS];

static unsigned pltcpu_count;

static struct plic_state
{
  void *base;
  uint64_t pa;
  uint64_t length;
  unsigned source_count;
} plic;

static int
pltcpu_find_hart (uint64_t hartid)
{
  unsigned i;

  for (i = 0; i < pltcpu_count; i++)
    if (pltcpus[i].present && pltcpus[i].hartid == hartid)
      return (int) i;

  return -1;
}

static void
pltcpu_add (uint64_t hartid)
{
  if (pltcpu_find_hart (hartid) >= 0)
    return;

  if (pltcpu_count >= HAL_MAXCPUS)
    {
      warn ("DT: hart %" PRIu64 " exceeds HAL_MAXCPUS; ignoring", hartid);
      return;
    }

  pltcpus[pltcpu_count].present = true;
  pltcpus[pltcpu_count].hartid = hartid;
  pltcpus[pltcpu_count].plic_ctx_valid = false;
  pltcpus[pltcpu_count].plic_ctx = PLIC_INVALID_CONTEXT;
  pltcpu_count++;
}

static void
_get_cells (const void *fdt, int noff, unsigned *addr, unsigned *size)
{
  unsigned a, s;
  int len;
  const void *prop;

  /* Initialise to spec default. */
  a = 2, s = 1;

  noff = fdt_parent_offset (fdt, noff);

  prop = fdt_getprop (fdt, noff, "#address-cells", &len);
  if (prop && len == sizeof (uint32_t))
    {
      a = fdt32_to_cpu (*(uint32_t *) prop);
    }
  else
    {
      warn ("DT: warning: using default #address-cells %d for node %s\n", a,
	    fdt_get_name (fdt, noff, NULL));
    }

  prop = fdt_getprop (fdt, noff, "#size-cells", &len);
  if (prop && len == sizeof (uint32_t))
    {
      s = fdt32_to_cpu (*(uint32_t *) prop);
    }
  else
    {
      warn ("DT: warning: using default #size-cells %d for node %s\n", s,
	    fdt_get_name (fdt, noff, NULL));
    }

  *addr = a;
  *size = s;
}

static bool
_get_reg (const void *fdt, int noff, unsigned idx, uint64_t * base,
	  uint64_t * length)
{
  int len;
  const void *prop;
  unsigned addrsz, sizesz, regsz;
  uint64_t b, l;

  _get_cells (fdt, noff, &addrsz, &sizesz);
  regsz = addrsz + sizesz;;

  prop = fdt_getprop (fdt, noff, "reg", &len);
  if (!prop)
    return false;

  if (len < (idx + 1) * regsz * sizeof (uint32_t))
    return false;

  prop += idx * regsz * sizeof (uint32_t);

  b = 0;
  for (unsigned i = 0; i < addrsz; i++)
    {
      b = (b << 32) | fdt32_to_cpu (*(uint32_t *) prop);
      prop += sizeof (uint32_t);
    }

  l = 0;
  for (unsigned i = 0; i < sizesz; i++)
    {
      l = (l << 32) | fdt32_to_cpu (*(uint32_t *) prop);
      prop += sizeof (uint32_t);
    }

  if (base)
    *base = b;
  if (length)
    *length = l;
  return true;
}

static bool
plic_mmio_valid (uint64_t off, size_t size)
{
  if (plic.base == NULL)
    return false;
  if (off > plic.length)
    return false;
  if ((uint64_t) size > plic.length - off)
    return false;
  return true;
}

static uint32_t
plic_read32 (uint64_t off)
{
  volatile uint32_t *reg;

  if (!plic_mmio_valid (off, sizeof (uint32_t)))
    return 0;

  reg = (volatile uint32_t *) ((uint8_t *) plic.base + off);
  return *reg;
}

static void
plic_write32 (uint64_t off, uint32_t val)
{
  volatile uint32_t *reg;

  if (!plic_mmio_valid (off, sizeof (uint32_t)))
    return;

  reg = (volatile uint32_t *) ((uint8_t *) plic.base + off);
  *reg = val;
}

static uint64_t
plic_priority_offset (unsigned irq)
{
  return PLIC_PRIORITY_BASE + (uint64_t) irq * sizeof (uint32_t);
}

static uint64_t
plic_enable_offset (unsigned ctx, unsigned irq)
{
  return PLIC_ENABLE_BASE + (uint64_t) ctx * PLIC_ENABLE_STRIDE
    + (uint64_t) (irq / 32) * sizeof (uint32_t);
}

static uint64_t
plic_threshold_offset (unsigned ctx)
{
  return PLIC_CONTEXT_BASE + (uint64_t) ctx * PLIC_CONTEXT_STRIDE
    + PLIC_CONTEXT_THRESHOLD;
}

static uint64_t
plic_claim_offset (unsigned ctx)
{
  return PLIC_CONTEXT_BASE + (uint64_t) ctx * PLIC_CONTEXT_STRIDE
    + PLIC_CONTEXT_CLAIM;
}

static bool
plic_valid_irq (unsigned irq)
{
  return plic.base != NULL && irq > 0 && irq <= plic.source_count;
}

static bool
plic_context_valid (unsigned ctx)
{
  if (plic.base == NULL || plic.source_count == 0)
    return false;

  return plic_mmio_valid (plic_threshold_offset (ctx), sizeof (uint32_t))
    && plic_mmio_valid (plic_claim_offset (ctx), sizeof (uint32_t))
    && plic_mmio_valid (plic_enable_offset (ctx, plic.source_count),
			       sizeof (uint32_t));
}

static bool
plic_cpu_context (unsigned cpu, unsigned *ctx)
{
  if (cpu >= HAL_MAXCPUS || !pltcpus[cpu].present
      || !pltcpus[cpu].plic_ctx_valid)
    return false;

  if (!plic_context_valid (pltcpus[cpu].plic_ctx))
    return false;

  if (ctx != NULL)
    *ctx = pltcpus[cpu].plic_ctx;
  return true;
}

static bool
plic_current_context (unsigned *ctx)
{
  return plic_cpu_context (plt_pcpu_id (), ctx);
}

static void
plic_complete_context (unsigned ctx, unsigned irq)
{
  plic_write32 (plic_claim_offset (ctx), irq);
}

static unsigned
plic_claim_current (void)
{
  unsigned ctx;

  if (!plic_current_context (&ctx))
    return 0;

  return plic_read32 (plic_claim_offset (ctx));
}

static void
plic_context_init (unsigned ctx)
{
  unsigned word;

  for (word = 0; word <= plic.source_count / 32; word++)
    plic_write32 (PLIC_ENABLE_BASE + (uint64_t) ctx * PLIC_ENABLE_STRIDE
		  + (uint64_t) word * sizeof (uint32_t), 0);

  /* Threshold 0 permits every enabled source with non-zero priority. */
  plic_write32 (plic_threshold_offset (ctx), 0);
}

static unsigned
plic_source_count (const void *fdt, int noff, uint64_t length)
{
  bool fallback = false;
  int len;
  const void *prop;
  unsigned ndev;
  uint64_t max_by_length64;
  unsigned max_by_length;

  prop = fdt_getprop (fdt, noff, "riscv,ndev", &len);
  if (prop != NULL && len == sizeof (uint32_t))
    {
      ndev = fdt32_to_cpu (*(const uint32_t *) prop);
    }
  else
    {
      if (prop != NULL)
	warn ("PLIC: invalid riscv,ndev length %d; using fallback", len);
      fallback = true;
      ndev = PLIC_FALLBACK_SOURCES;
    }

  if (ndev == 0)
    {
      warn ("PLIC: zero interrupt sources; disabling external IRQs");
      return 0;
    }

  if (fallback)
    warn ("PLIC: missing riscv,ndev; using conservative %u-source fallback",
	  ndev);

  if (ndev > PLIC_MAX_SOURCES)
    {
      warn ("PLIC: riscv,ndev %u exceeds standard layout; capping at %u",
	    ndev, PLIC_MAX_SOURCES);
      ndev = PLIC_MAX_SOURCES;
    }

  if (length < sizeof (uint32_t))
    return 0;
  max_by_length64 = (length - sizeof (uint32_t)) / sizeof (uint32_t);
  max_by_length = max_by_length64 > PLIC_MAX_SOURCES
    ? PLIC_MAX_SOURCES : (unsigned) max_by_length64;
  if (ndev > max_by_length)
    {
      warn ("PLIC: source count %u exceeds MMIO priority window; capping at %u",
	    ndev, max_by_length);
      ndev = max_by_length;
    }

  return ndev;
}

static bool
plic_assign_context (uint64_t hartid, unsigned ctx)
{
  int cpu;

  cpu = pltcpu_find_hart (hartid);
  if (cpu < 0)
    {
      warn ("PLIC: S-mode context %u references unknown hart %" PRIu64,
	    ctx, hartid);
      return false;
    }

  if (!plic_context_valid (ctx))
    {
      warn ("PLIC: S-mode context %u for hart %" PRIu64
	    " is outside the MMIO window", ctx, hartid);
      return false;
    }

  pltcpus[cpu].plic_ctx = ctx;
  pltcpus[cpu].plic_ctx_valid = true;
  return true;
}

static void
plic_init (const void *fdt, int noff)
{
  int len;
  const void *prop;
  uint64_t base, length;
  unsigned valid_contexts = 0;
  const int pair_size = sizeof (uint32_t) * 2;

  if (plic.base != NULL)
    {
      warn ("PLIC: ignoring additional compatible node %s",
	    fdt_get_name (fdt, noff, NULL));
      return;
    }

  if (!_get_reg (fdt, noff, 0, &base, &length) || length == 0)
    {
      warn ("PLIC: node %s has no usable reg property",
	    fdt_get_name (fdt, noff, NULL));
      return;
    }

  printf ("PLIC: %s [%016" PRIx64 ":%016" PRIx64 "]\n",
	  fdt_get_name (fdt, noff, NULL), base, base + length);

  if (length > PLIC_MAX_MAP_LENGTH)
    {
      warn ("PLIC: MMIO window length %" PRIx64
	    " exceeds bounded mapping size; capping at %" PRIx64,
	    length, (uint64_t) PLIC_MAX_MAP_LENGTH);
      length = PLIC_MAX_MAP_LENGTH;
    }

  plic.pa = base;
  plic.length = length;
  plic.source_count = plic_source_count (fdt, noff, length);
  if (plic.source_count == 0)
    return;

  plic.base = kva_physmap (base, (size_t) length, HAL_PTE_P | HAL_PTE_W);
  if (plic.base == NULL)
    {
      warn ("PLIC: failed to map MMIO window; disabling external IRQs");
      plic.source_count = 0;
      return;
    }

  printf ("PLIC: %u sources mapped at %p\n", plic.source_count, plic.base);

  prop = fdt_getprop (fdt, noff, "interrupts-extended", &len);
  if (prop == NULL || len < pair_size)
    {
      warn ("PLIC: no interrupts-extended S-mode contexts found");
      return;
    }
  if ((len % pair_size) != 0)
    warn ("PLIC: ignoring trailing bytes in interrupts-extended");

  printf ("PLIC: External Interrupts Contexts: ");
  for (int i = 0; i + pair_size <= len; i += pair_size)
    {
      const uint32_t *cells;
      uint32_t phandle, intr;

      cells = (const uint32_t *) ((const uint8_t *) prop + i);
      phandle = fdt32_to_cpu (cells[0]);
      intr = fdt32_to_cpu (cells[1]);

      /* External interrupts for S-mode are delivered as cause 9. */
      if (intr == 9)
	{
	  uint64_t hartid;
	  unsigned ctx;
	  int hoff, poff;

	  hoff = fdt_node_offset_by_phandle (fdt, phandle);
	  if (hoff < 0)
	    continue;
	  poff = fdt_parent_offset (fdt, hoff);
	  if (poff < 0)
	    continue;
	  if (!_get_reg (fdt, poff, 0, &hartid, NULL))
	    continue;

	  ctx = (unsigned) (i / pair_size);
	  printf ("%" PRIu64 "[%u] ", hartid, ctx);
	  if (plic_assign_context (hartid, ctx))
	    valid_contexts++;
	}
    }
  printf ("\n");

  if (valid_contexts == 0)
    warn ("PLIC: no usable S-mode contexts; IRQ APIs stay disabled");
}

void
plt_init (void)
{
  const struct apxh_pltdesc *desc;
  struct fdt_header *fdth;
  const void *fdt, *prop;
  int len, cpus_off;
  uint32_t size;

  desc = hal_pltinfo ();
  if (desc == NULL)
    fatal ("Invalid PLT Boot Table.");

  if (desc->type != PLT_DTB)
    fatal ("No Device Tree Found.");

  info ("DT: DTB at %016" PRIx64, desc->pltptr);

  fdth =
    (struct fdt_header *) kva_physmap (desc->pltptr, sizeof (*fdth),
				       HAL_PTE_P);
  if (fdt_check_header (fdth) != 0)
    fatal ("Invalid DTB Header.");

  size = fdt32_to_cpu (fdth->totalsize);

  kva_unmap (fdth, sizeof (*fdth));

  fdt = (const void *) kva_physmap (desc->pltptr, size, HAL_PTE_P);

  /*
   * Scan the /cpus node, gathering information about HARTs, timer and
   * interrupts.
   */
  cpus_off = fdt_path_offset (fdt, "/cpus");
  if (cpus_off < 0)
    {
      fatal ("Device tree does not contain '/cpus' node.");
    }

  /*
   * Technically the Device Tree specification says that
   * 'timebase-frequency' should be a property of a single CPU node.
   * Practically in RV this is often found in /cpus.
   */
  prop = fdt_getprop (fdt, cpus_off, "timebase-frequency", &len);
  if (prop != NULL)
    {
      if (len == sizeof (uint32_t))
	{
	  timebase_frequency = fdt32_to_cpu (*(uint32_t *) prop);
	}
      else if (len == sizeof (uint64_t))
	{
	  timebase_frequency = fdt32_to_cpu (*(uint64_t *) prop);
	}
      else
	{
	  warn ("Unexpected length %d in %s/timebase-frequency\n", len,
		fdt_get_name (fdt, cpus_off, NULL));
	}
    }

  printf ("DT: ");

  for (int _cpu_off = fdt_first_subnode (fdt, cpus_off);
       _cpu_off >= 0; _cpu_off = fdt_next_subnode (fdt, _cpu_off))
    {
      const char *name;
      name = fdt_get_name (fdt, _cpu_off, NULL);
      if (name == NULL)
	continue;

      if (strncmp (name, "cpu@", 4) != 0)
	continue;

      printf ("%s ", name);

      {
	uint64_t hartid;

	if (_get_reg (fdt, _cpu_off, 0, &hartid, NULL))
	  pltcpu_add (hartid);
	else
	  warn ("DT: CPU node %s has no usable hart reg", name);
      }

      prop = fdt_getprop (fdt, cpus_off, "timebase-frequency", &len);
      if (prop != NULL)
	{
	  uint64_t freq;
	  if (len == sizeof (uint32_t))
	    {
	      freq = fdt32_to_cpu (*(uint32_t *) prop);
	    }
	  else if (len == sizeof (uint64_t))
	    {
	      freq = fdt32_to_cpu (*(uint64_t *) prop);
	    }
	  else
	    continue;		/* Let's ignore an invalid
				 * value. Should be fatal? */

	  if (timebase_frequency == 0)
	    timebase_frequency = freq;
	  else if (timebase_frequency != freq)
	    fatal
	      ("Inconsistent timebase frequencies: previous %lx, found %lx\n",
	       timebase_frequency, freq);
	}
    }

  printf ("\n");

  printf ("DT: timebase-frequency: %ld\n", timebase_frequency);


  /* Search for PLIC. */
  for (int noff = fdt_next_node (fdt, -1, NULL);
       noff >= 0; noff = fdt_next_node (fdt, noff, NULL))
    {
      int pos = 0;
      prop = fdt_getprop (fdt, noff, "compatible", &len);
      if (prop)
	{
	  while (pos < len)
	    {
	      if (!strncmp ((char *) prop + pos, "sifive,plic-1.0.0", 17)
		  || !strncmp ((char *) prop + pos, "riscv,plic0", 11))
		{
		  plic_init (fdt, noff);
		  break;
		}
	      pos += strlen ((char *) prop + pos) + 1;
	    }
	}
    }

  kva_unmap ((void *) fdt, size);
}

void
plt_pcpu_enter (void)
{
  unsigned cpu, ctx;

  cpu = plt_pcpu_id ();
  if (plic_cpu_context (cpu, &ctx))
    {
      plic_context_init (ctx);
      info ("PLIC: CPU%u hart %" PRIu64 " S-mode context %u ready",
	    cpu, pltcpus[cpu].hartid, ctx);
    }
}

int
plt_pcpu_iterate (void)
{
  static int next_pcpu = 0;

  /* TODO */

  if (next_pcpu++ == 0)
    return 0;
  else
    return PLT_PCPU_INVALID;
}

void
riscv_ipi (unsigned long mask)
{
  asm volatile ("mv a0, %0\n"
		"li a7, 4\n" "ecall\n"::"r" (&mask):"a0", "a1", "a7");
}

void
plt_pcpu_ipiall (void)
{
  nmiemul_ipi_setall ();
  asm volatile ("csrsi sip, 2\n");
  riscv_ipi (-1);
}

void
plt_pcpu_ipi (int cpu)
{
  nmiemul_ipi_set (cpu);
  if (cpu == cpu_id ())
    asm volatile ("csrsi sip, 2\n");
  else
  riscv_ipi (1L << cpu);
}

void
plt_pcpu_nmiall (void)
{
  nmiemul_nmi_setall ();
  asm volatile ("csrsi sip, 2\n");
  riscv_ipi (-1);
}

void
plt_pcpu_nmi (int cpu)
{
  nmiemul_nmi_set (cpu);
  if (cpu == cpu_id ())
    asm volatile ("csrsi sip, 2\n");
  else
  riscv_ipi (1L << cpu);
}

void
plt_pcpu_start (unsigned cpu, unsigned long startaddr)
{
  /* TODO */
}

unsigned
plt_pcpu_id (void)
{
  return 0;
}

bool
plt_vect_process (unsigned vect)
{
  /* TODO */
  return false;
}

enum plt_irq_type
plt_irq_type (unsigned irq)
{
  if (!plic_valid_irq (irq) || !plic_current_context (NULL))
    return PLT_IRQ_INVALID;

  /*
   * The PLIC itself does not expose trigger/polarity metadata in its DTB
   * node.  Until device-specific metadata is plumbed through a future
   * driver path, treat valid external sources as active-high level lines,
   * matching the common QEMU virt MMIO-device wiring.  Completion still uses
   * the PLIC claim/complete register for every dispatched source.
   */
  return PLT_IRQ_LVLHI;
}

void
plt_irq_enable (unsigned irq)
{
  unsigned ctx;
  uint64_t off;
  uint32_t val;

  if (!plic_valid_irq (irq) || !plic_current_context (&ctx))
    return;

  /* Priority 0 means "never interrupt"; use the lowest active priority. */
  plic_write32 (plic_priority_offset (irq), 1);

  off = plic_enable_offset (ctx, irq);
  val = plic_read32 (off);
  val |= 1U << (irq & 31);
  plic_write32 (off, val);
}

void
plt_irq_disable (unsigned irq)
{
  unsigned ctx;
  uint64_t off;
  uint32_t val;

  if (!plic_valid_irq (irq) || !plic_current_context (&ctx))
    return;

  off = plic_enable_offset (ctx, irq);
  val = plic_read32 (off);
  val &= ~(1U << (irq & 31));
  plic_write32 (off, val);
}

unsigned
plt_irq_max (void)
{
  if (!plic_current_context (NULL))
    return 0;

  /* Source ID 0 is reserved by the PLIC and never a valid NUX IRQ. */
  return plic.source_count + 1;
}

void
plt_eoi_ipi (void)
{
  /* Nothing. */
}

void
plt_eoi_irq (unsigned irq)
{
  unsigned ctx;

  if (!plic_valid_irq (irq) || !plic_current_context (&ctx))
    return;

  plic_complete_context (ctx, irq);
}

void
plt_eoi_timer (void)
{
  /* Nothing. */
}

struct hal_frame *
plt_interrupt (unsigned vect, struct hal_frame *f)
{
  struct hal_frame *r;

  switch (vect)
    {
    case 1:			/* Supervisor Software Interrupt. */
      r = nmiemul_entry (f);
      break;

    case 5:			/* Supervisor Timer Interrupt. */
      plt_tmr_clralm ();
      r = hal_entry_timer (f);
      break;

    case 9:			/* Supervisor External Interrupt. */
      {
	unsigned irq, ctx;

	irq = plic_claim_current ();
	if (irq == 0)
	  {
	    r = f;
	    break;
	  }

	if (!plic_valid_irq (irq))
	  {
	    warn ("PLIC: claimed invalid source %u", irq);
	    if (plic_current_context (&ctx))
	      plic_complete_context (ctx, irq);
	    r = f;
	    break;
	  }

	r = hal_entry_irq (f, irq, plt_irq_islevel (irq));
	break;
      }

    default:
      r = f;
    }

  return r;
}

uint64_t tmr_offset = 0;

uint64_t
plt_tmr_ctr (void)
{
  uint64_t time;

  asm volatile ("rdtime %0\n":"=r" (time));
  return time + tmr_offset;
}

void
plt_tmr_setctr (uint64_t alm)
{
  uint64_t time;

  asm volatile ("rdtime %0\n":"=r" (time));
  tmr_offset = alm - time;
}

void
plt_tmr_setalm (uint64_t alm)
{
  alm += plt_tmr_ctr ();
  asm volatile ("mv a0, %0\n"
		"mv a6, x0\n"
		"mv a7, x0\n" "ecall\n"::"r" (alm):"a0", "a6", "a7");
}


uint64_t
plt_tmr_period (void)
{
  return 1000000000000000L / timebase_frequency;
}

void
plt_tmr_clralm (void)
{
  asm volatile ("li a0, -1\n"
		"mv a6, x0\n" "mv a7, x0\n" "ecall\n":::"a0", "a6", "a7");
}
