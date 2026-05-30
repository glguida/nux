/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida, glguida@tlbflush.org

  SPDX-License-Identifier:	BSD-2-Clause
*/


#include <string.h>
#include <nux/defs.h>
#include <nux/types.h>
#include <nux/nux.h>

#include "acpitbl.h"
#include "internal.h"

#define ACPI_MAX_TBL (16 << 12)

static paddr_t pa_root_table;
static paddr_t pa_apic_table;
static paddr_t pa_hpet_table;

struct acpi_platform_facts
{
  paddr_t rsdp;
  paddr_t root_table;
  const char *root_kind;
  uint8_t revision;
  unsigned root_entries;
  bool apic_present;
  bool hpet_present;
  paddr_t apic_table;
  paddr_t hpet_table;
  paddr_t lapic_base;
  unsigned lapic_count;
  unsigned ioapic_count;
  unsigned int_override_count;
  unsigned lapic_nmi_count;
  unsigned ignored_lsapic_count;
  unsigned ignored_x2apic_count;
  unsigned ignored_iosapic_count;
  unsigned ignored_x2apic_nmi_count;
};

static struct acpi_platform_facts acpi_facts;

static const char *
acpi_fact_root_kind (void)
{
  return acpi_facts.root_kind != NULL ? acpi_facts.root_kind : "none";
}

static void
acpi_log_table_facts (void)
{
  info ("NUX ACPI FACTS: rsdp=%" PRIx64 " revision=%u root=%s"
	" root_pa=%" PRIx64 " root_entries=%u apic=%u apic_pa=%" PRIx64
	" hpet=%u hpet_pa=%" PRIx64,
	(uint64_t) acpi_facts.rsdp, acpi_facts.revision,
	acpi_fact_root_kind (), (uint64_t) acpi_facts.root_table,
	acpi_facts.root_entries,
	acpi_facts.apic_present ? 1 : 0, (uint64_t) acpi_facts.apic_table,
	acpi_facts.hpet_present ? 1 : 0, (uint64_t) acpi_facts.hpet_table);
}

static void
acpi_log_madt_facts (bool loaded)
{
  info ("NUX ACPI MADT FACTS: table=%u loaded=%u lapic_count=%u"
	" ioapic_count=%u lapic_base=%" PRIx64
	" int_override_count=%u lapic_nmi_count=%u"
	" ignored_lsapic_count=%u ignored_x2apic_count=%u"
	" ignored_iosapic_count=%u ignored_x2apic_nmi_count=%u",
	acpi_facts.apic_present ? 1 : 0, loaded ? 1 : 0,
	acpi_facts.lapic_count, acpi_facts.ioapic_count,
	(uint64_t) acpi_facts.lapic_base,
	acpi_facts.int_override_count, acpi_facts.lapic_nmi_count,
	acpi_facts.ignored_lsapic_count, acpi_facts.ignored_x2apic_count,
	acpi_facts.ignored_iosapic_count,
	acpi_facts.ignored_x2apic_nmi_count);
}

void
acpi_gsi_facts (unsigned gsi_count)
{
  if (gsi_count == 0)
    info ("NUX ACPI GSI FACTS: count=0 range=none");
  else
    info ("NUX ACPI GSI FACTS: count=%u range=0-%u", gsi_count,
	  gsi_count - 1);
}

static void
acpi_log_hpet_facts (bool init_ok)
{
  info ("NUX ACPI HPET FACTS: table=%u hpet_pa=%" PRIx64 " init=%u",
	acpi_facts.hpet_present ? 1 : 0, (uint64_t) acpi_facts.hpet_table,
	init_ok ? 1 : 0);
}

static void *
load_table (paddr_t pa)
{
  int i;
  uint8_t sum, *ptr;
  struct acpi_thdr *tbl;

  tbl = (struct acpi_thdr *) kva_physmap (pa, ACPI_MAX_TBL, HAL_PTE_P);

  if (tbl->length >= ACPI_MAX_TBL)
    {
      error
	("Table %4.4s [%6.6s %8.8s rev%d] size %d > ACPI_MAX_TBL. Skipping checks.",
	 tbl->signature, tbl->oemid, tbl->oemtableid, tbl->oemrevision,
	 tbl->length);
      return tbl;
    }

  sum = 0;
  ptr = (uint8_t *) tbl;

  for (i = 0; i < tbl->length; i++)
    {
      sum += ptr[i];
    }
  if (sum != 0)
    {
      warn ("Wrong checksum %d != 0 for ACPI table", sum);
      kva_unmap (tbl, ACPI_MAX_TBL);
      return NULL;
    }

  debug ("loaded table '%4.4s' [%6.6s %8.8s rev%d]", tbl->signature,
	 tbl->oemid, tbl->oemtableid, tbl->oemrevision);
  return tbl;
}

static void
unload_table (void *tbl)
{
  kva_unmap (tbl, ACPI_MAX_TBL);
}

static void
print_table (struct acpi_thdr *tbl)
{
  info ("TABLE '%4.4s' [%6.6s %8.8s rev%d]", tbl->signature, tbl->oemid,
	tbl->oemtableid, tbl->oemrevision);
}

void
acpi_init (paddr_t root)
{
  void *ptr;
  size_t entrylen;
  int64_t length;
  paddr_t pasdt;
  struct acpi_rsdp_thdr *rsdp;
  struct acpi_thdr *roottable, *sdtable;

  memset (&acpi_facts, 0, sizeof (acpi_facts));
  pa_root_table = 0;
  pa_apic_table = 0;
  pa_hpet_table = 0;
  acpi_facts.rsdp = root;

  rsdp =
    (struct acpi_rsdp_thdr *) kva_physmap (root, ACPI_MAX_TBL, HAL_PTE_P);

  info ("TABLE: '%8.8s' [%6.6s] rev: %d", rsdp->signature, rsdp->oemid,
	rsdp->revision);
  acpi_facts.revision = rsdp->revision;

  if (rsdp->revision == 0)
    {
      pasdt = rsdp->rsdt;
      acpi_facts.root_kind = "RSDT";
      debug ("SDT found at addr %" PRIx64, pasdt);
      entrylen = 4;
    }
  else
    {
      pasdt = rsdp->xsdt;
      acpi_facts.root_kind = "XSDT";
      debug ("XSDT found at addr %" PRIx64, pasdt);
      entrylen = 8;
    }

  kva_unmap (rsdp, ACPI_MAX_TBL);

  pa_root_table = pasdt;
  acpi_facts.root_table = pasdt;
  roottable = load_table (pasdt);

  /* Iterate through ACPI tables. */
  ptr = (void *) (roottable + 1);
  length = (int64_t) roottable->length - sizeof (*roottable);
  while (length > 0)
    {
      pasdt = entrylen == 8 ? *(uint64_t *) ptr : *(uint32_t *) ptr;
      sdtable = load_table (pasdt);

      print_table (sdtable);
      acpi_facts.root_entries++;

      if (!memcmp (sdtable->signature, "APIC", 4))
	{
	  pa_apic_table = pasdt;
	  acpi_facts.apic_present = true;
	  acpi_facts.apic_table = pasdt;
	}
      else if (!memcmp (sdtable->signature, "HPET", 4))
	{
	  pa_hpet_table = pasdt;
	  acpi_facts.hpet_present = true;
	  acpi_facts.hpet_table = pasdt;
	}

      unload_table (sdtable);
      length -= entrylen;
      ptr += entrylen;
    }

  unload_table (roottable);

  debug ("RDST table at pa %" PRIx64, pa_root_table);
  debug ("APIC table at pa %" PRIx64, pa_apic_table);
  debug ("HPET table at pa %" PRIx64, pa_hpet_table);
  acpi_log_table_facts ();
}

void
acpi_madt_scan (void)
{
  int len;
  unsigned flags, nlapic = 0, nioapic = 0;
  uint8_t type;
  paddr_t lapic_addr;
  struct acpi_madt *acpi_madt;

  union
  {
    uint8_t *ptr;
    struct acpi_madt_lapic *lapic;
    struct acpi_madt_ioapic *ioapic;
    struct acpi_madt_lapicoverride *lavr;
    struct acpi_madt_lapicnmi *lanmi;
    struct acpi_madt_intoverride *intovr;
  } _;

#define madt_foreach(_cases)						\
	do {								\
		len = acpi_madt->hdr.length - sizeof(*acpi_madt);	\
		_.ptr = (uint8_t *) acpi_madt + sizeof(*acpi_madt);	\
		while (len > 0) {					\
			type = *_.ptr;					\
			switch (type) {					\
				_cases;					\
			}						\
			len -= *(_.ptr + 1);				\
			_.ptr += *(_.ptr + 1);				\
		}							\
	} while (0)

  acpi_facts.lapic_base = 0;
  acpi_facts.lapic_count = 0;
  acpi_facts.ioapic_count = 0;
  acpi_facts.int_override_count = 0;
  acpi_facts.lapic_nmi_count = 0;
  acpi_facts.ignored_lsapic_count = 0;
  acpi_facts.ignored_x2apic_count = 0;
  acpi_facts.ignored_iosapic_count = 0;
  acpi_facts.ignored_x2apic_nmi_count = 0;

  acpi_madt = load_table (pa_apic_table);
  if (acpi_madt == NULL)
    {
      error ("Could not load ACPI MADT Table.");
      acpi_log_madt_facts (false);
      return;
    }

  lapic_addr = acpi_madt->lapic;
  acpi_facts.lapic_base = lapic_addr;

  /* Search for APICs. Output of this stage is number of Local
     and I/O APICs and Lapic address. */
  /* *INDENT-OFF* */
  madt_foreach({
      case ACPI_MADT_TYPE_LAPICOVERRIDE:
	info("ACPI MADT LAPICOVR %"PRIx64, _.lavr->address);
	lapic_addr = _.lavr->address;
	acpi_facts.lapic_base = lapic_addr;
	break;
      case ACPI_MADT_TYPE_LAPIC:
	if (_.lapic->flags & ACPI_MADT_LAPIC_ENABLED)
	  {
	    info("ACPI MADT LAPIC %02d %02d %08x",
		 _.lapic->lapicid, _.lapic->acpiid, _.lapic->flags);
	    nlapic++;
	  }
	break;
      case ACPI_MADT_TYPE_IOAPIC:
	info("ACPI MADT IOAPIC %02d %08x %02d",
	       _.ioapic->ioapicid, _.ioapic->address, _.ioapic->gsibase);
	nioapic++;
	break;
      case ACPI_MADT_TYPE_LSAPIC:
	{
	  static int warn = 0;
	  acpi_facts.ignored_lsapic_count++;
	  if (!warn)
	    {
	      info("Warning: LSAPIC ENTRIES IGNORED");
	      warn = 1;
	    }
	  break;
	}
      case ACPI_MADT_TYPE_LX2APIC:
	{
	  static int warn = 0;
	  acpi_facts.ignored_x2apic_count++;
	  if (!warn)
	    {
	      info("Warning: X2APIC ENTRY IGNORED");
	      warn = 1;
	    }
	}
	break;
      case ACPI_MADT_TYPE_IOSAPIC:
	{
	  static int warn = 0;
	  acpi_facts.ignored_iosapic_count++;
	  if (!warn)
	    {
	      info("Warning: IOSAPIC ENTRY IGNORED");
	      warn = 1;
	    }
	  break;
	}
      default:
	break;
    });
  /* *INDENT-ON* */
  if (nlapic == 0)
    {
      info ("Warning: NO LOCAL APICS, ACPI SAYS");
      nlapic = 1;
    }

  acpi_facts.lapic_count = nlapic;
  acpi_facts.ioapic_count = nioapic;

  lapic_init (lapic_addr, nlapic);
  ioapic_init (nioapic);

  /* Add APICs. Local and I/O APICs existence is notified to the
   * kernel after this. */
  nioapic = 0;
  /* *INDENT-OFF* */
  madt_foreach({
      case ACPI_MADT_TYPE_LAPIC:
	if (_.lapic->flags & ACPI_MADT_LAPIC_ENABLED)
	  lapic_add(_.lapic->lapicid, _.lapic->acpiid);
	break;
      case ACPI_MADT_TYPE_IOAPIC:
	ioapic_add(nioapic, _.ioapic->address, _.ioapic->gsibase);
	nioapic++;
	break;
      default:
	break;
    });
  /* *INDENT-ON* */

  gsi_init ();
  /* *INDENT-OFF* */
  madt_foreach({
      case ACPI_MADT_TYPE_LAPICNMI:
	acpi_facts.lapic_nmi_count++;
	info ("ACPI MADT LAPICNMI LINT%01d FL:%04x PROC:%02d",
	       _.lanmi->lint, _.lanmi->flags, _.lanmi->acpiid);
	/* Ignore IntiFlags as NMI vectors ignore
	 * polarity and trigger */
	lapic_add_nmi(_.lanmi->acpiid, _.lanmi->lint);
	break;
      case ACPI_MADT_TYPE_LX2APICNMI:
	acpi_facts.ignored_x2apic_nmi_count++;
	warn ("LX2APICNMI ENTRY IGNORED");
	break;
      case ACPI_MADT_TYPE_INTOVERRIDE:
	acpi_facts.int_override_count++;
	info ("ACPI MADT INTOVR BUS %02d IRQ: %02d GSI: %02d FL: %04x",
	       _.intovr->bus, _.intovr->irq, _.intovr->gsi, _.intovr->flags);
	flags = _.intovr->flags;
	switch (flags & ACPI_MADT_TRIGGER_MASK) {
	case ACPI_MADT_TRIGGER_RESERVED:
	  warn ("reserved trigger value");
	  /* Passtrhough to edge. */
	case ACPI_MADT_TRIGGER_CONFORMS:
	  /* ISA is EDGE */
	case ACPI_MADT_TRIGGER_EDGE:
	  gsi_setup(_.intovr->gsi, _.intovr->irq, PLT_IRQ_EDGE);
	  break;
	case ACPI_MADT_TRIGGER_LEVEL:
	  switch(flags &ACPI_MADT_POLARITY_MASK) {
	  case ACPI_MADT_POLARITY_RESERVED:
	    warn ("Warning: reserved polarity value");
	    /* Passthrough to Level Low */
	  case ACPI_MADT_POLARITY_CONFORMS:
	    /* Default for EISA is LOW */
	  case ACPI_MADT_POLARITY_ACTIVE_LOW:
	    gsi_setup(_.intovr->gsi, _.intovr->irq, PLT_IRQ_LVLLO);
	    break;
	  case ACPI_MADT_POLARITY_ACTIVE_HIGH:
	    gsi_setup(_.intovr->gsi, _.intovr->irq, PLT_IRQ_LVLHI);
	    break;
	  }
	  break;
	}
	break;
      default:
	break;
    });
  /* *INDENT-ON* */

  acpi_log_madt_facts (true);
  unload_table (acpi_madt);
}

bool
acpi_hpet_scan (void)
{
  bool rc;
  struct acpi_hpet *hpet;

  if (pa_hpet_table == 0)
    {
      warn ("No HPET found");
      acpi_log_hpet_facts (false);
      return false;
    }

  hpet = load_table (pa_hpet_table);
  if (hpet == NULL)
    {
      error ("Error loading HPET table");
      acpi_log_hpet_facts (false);
      return false;
    }

  rc = hpet_init (hpet->address.address);

  unload_table (hpet);
  acpi_log_hpet_facts (rc);

  return rc;
}
