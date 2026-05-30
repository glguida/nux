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

  rsdp =
    (struct acpi_rsdp_thdr *) kva_physmap (root, ACPI_MAX_TBL, HAL_PTE_P);

  info ("TABLE: '%8.8s' [%6.6s] rev: %d", rsdp->signature, rsdp->oemid,
	rsdp->revision);

  if (rsdp->revision == 0)
    {
      pasdt = rsdp->rsdt;
      debug ("SDT found at addr %" PRIx64, pasdt);
      entrylen = 4;
    }
  else
    {
      pasdt = rsdp->xsdt;
      debug ("XSDT found at addr %" PRIx64, pasdt);
      entrylen = 8;
    }

  kva_unmap (rsdp, ACPI_MAX_TBL);

  pa_root_table = pasdt;
  roottable = load_table (pasdt);

  /* Iterate through ACPI tables. */
  ptr = (void *) (roottable + 1);
  length = (int64_t) roottable->length - sizeof (*roottable);
  while (length > 0)
    {
      pasdt = entrylen == 8 ? *(uint64_t *) ptr : *(uint32_t *) ptr;
      sdtable = load_table (pasdt);

      print_table (sdtable);

      if (!memcmp (sdtable->signature, "APIC", 4))
	pa_apic_table = pasdt;
      else if (!memcmp (sdtable->signature, "HPET", 4))
	pa_hpet_table = pasdt;

      unload_table (sdtable);
      length -= entrylen;
      ptr += entrylen;
    }

  unload_table (roottable);

  debug ("RDST table at pa %" PRIx64, pa_root_table);
  debug ("APIC table at pa %" PRIx64, pa_apic_table);
  debug ("HPET table at pa %" PRIx64, pa_hpet_table);
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

  acpi_madt = load_table (pa_apic_table);
  if (acpi_madt == NULL)
    {
      error ("Could not load ACPI MADT Table.");
      return;
    }

  lapic_addr = acpi_madt->lapic;

  /* Search for APICs. Output of this stage is number of Local
     and I/O APICs and Lapic address. */
  /* *INDENT-OFF* */
  madt_foreach({
      case ACPI_MADT_TYPE_LAPICOVERRIDE:
	info("ACPI MADT LAPICOVR %"PRIx64, _.lavr->address);
	lapic_addr = _.lavr->address;
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
	info ("ACPI MADT LAPICNMI LINT%01d FL:%04x PROC:%02d",
	       _.lanmi->lint, _.lanmi->flags, _.lanmi->acpiid);
	/* Ignore IntiFlags as NMI vectors ignore
	 * polarity and trigger */
	lapic_add_nmi(_.lanmi->acpiid, _.lanmi->lint);
	break;
      case ACPI_MADT_TYPE_LX2APICNMI:
	warn ("LX2APICNMI ENTRY IGNORED");
	break;
      case ACPI_MADT_TYPE_INTOVERRIDE:
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
      return false;
    }

  hpet = load_table (pa_hpet_table);
  if (hpet == NULL)
    {
      error ("Error loading HPET table");
      return false;
    }

  rc = hpet_init (hpet->address.address);

  unload_table (hpet);


  return rc;
}
