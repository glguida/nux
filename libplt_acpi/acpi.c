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

  if (pa == 0)
    {
      warn ("ACPI table at PA 0 is absent");
      return NULL;
    }

  tbl = (struct acpi_thdr *) kva_physmap (pa, ACPI_MAX_TBL, HAL_PTE_P);
  if (tbl == NULL)
    {
      error ("Could not map ACPI table at pa %" PRIx64, pa);
      return NULL;
    }

  if (tbl->length < sizeof (*tbl))
    {
      warn ("ACPI table %4.4s length %u < header length %u",
	    tbl->signature, (unsigned) tbl->length, (unsigned) sizeof (*tbl));
      kva_unmap (tbl, ACPI_MAX_TBL);
      return NULL;
    }

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
  uint8_t *ptr;
  size_t entrylen;
  int64_t length;
  paddr_t pasdt;
  struct acpi_rsdp_thdr *rsdp;
  struct acpi_thdr *roottable, *sdtable;

  if (root == 0)
    {
      error ("No ACPI RSDP physical address");
      return;
    }

  rsdp =
    (struct acpi_rsdp_thdr *) kva_physmap (root, ACPI_MAX_TBL, HAL_PTE_P);
  if (rsdp == NULL)
    {
      error ("Could not map ACPI RSDP at pa %" PRIx64, root);
      return;
    }

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
  if (roottable == NULL)
    {
      error ("Could not load ACPI root table.");
      return;
    }
  if (roottable->length > ACPI_MAX_TBL)
    {
      error ("ACPI root table length %u > mapped limit %u",
	     (unsigned) roottable->length, (unsigned) ACPI_MAX_TBL);
      unload_table (roottable);
      return;
    }

  /* Iterate through ACPI tables. */
  ptr = (uint8_t *) (roottable + 1);
  length = (int64_t) roottable->length - sizeof (*roottable);
  while (length > 0)
    {
      if (length < (int64_t) entrylen)
	{
	  warn ("ACPI root table has %d trailing byte(s)", (int) length);
	  break;
	}

      pasdt = entrylen == 8 ? *(uint64_t *) ptr : *(uint32_t *) ptr;
      sdtable = load_table (pasdt);
      if (sdtable == NULL)
	{
	  length -= entrylen;
	  ptr += entrylen;
	  continue;
	}

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

#define ACPI_MADT_ENTRY_HEADER_LEN 2

static const char *
madt_entry_name (uint8_t type)
{
  switch (type)
    {
    case ACPI_MADT_TYPE_LAPIC:
      return "LAPIC";
    case ACPI_MADT_TYPE_IOAPIC:
      return "IOAPIC";
    case ACPI_MADT_TYPE_INTOVERRIDE:
      return "INTOVR";
    case ACPI_MADT_TYPE_LAPICNMI:
      return "LAPICNMI";
    case ACPI_MADT_TYPE_LAPICOVERRIDE:
      return "LAPICOVR";
    case ACPI_MADT_TYPE_LSAPIC:
      return "LSAPIC";
    case ACPI_MADT_TYPE_LX2APIC:
      return "LX2APIC";
    case ACPI_MADT_TYPE_IOSAPIC:
      return "IOSAPIC";
    case ACPI_MADT_TYPE_LX2APICNMI:
      return "LX2APICNMI";
    default:
      return "UNKNOWN";
    }
}

static unsigned
madt_entry_min_length (uint8_t type)
{
  switch (type)
    {
    case ACPI_MADT_TYPE_LAPIC:
      return sizeof (struct acpi_madt_lapic);
    case ACPI_MADT_TYPE_IOAPIC:
      return sizeof (struct acpi_madt_ioapic);
    case ACPI_MADT_TYPE_INTOVERRIDE:
      return sizeof (struct acpi_madt_intoverride);
    case ACPI_MADT_TYPE_LAPICNMI:
      return sizeof (struct acpi_madt_lapicnmi);
    case ACPI_MADT_TYPE_LAPICOVERRIDE:
      return sizeof (struct acpi_madt_lapicoverride);
    default:
      return ACPI_MADT_ENTRY_HEADER_LEN;
    }
}

static bool
madt_entry_is_long_enough (uint8_t type, unsigned entry_len)
{
  unsigned min_len = madt_entry_min_length (type);

  if (entry_len < min_len)
    {
      warn ("ACPI MADT %s entry length %u < %u; skipping",
	    madt_entry_name (type), entry_len, min_len);
      return false;
    }
  return true;
}

void
acpi_madt_scan (void)
{
  unsigned len, entry_len;
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
			if (len < ACPI_MADT_ENTRY_HEADER_LEN) {		\
				warn ("ACPI MADT truncated entry header (%u byte(s) remain); stopping scan", len); \
				break;					\
			}						\
			type = _.ptr[0];				\
			entry_len = _.ptr[1];				\
			if (entry_len == 0) {				\
				warn ("ACPI MADT zero-length entry; stopping scan"); \
				break;					\
			}						\
			if (entry_len > len) {				\
				warn ("ACPI MADT %s entry length %u exceeds remaining payload %u; stopping scan", \
				      madt_entry_name (type), entry_len, len); \
				break;					\
			}						\
			if (!madt_entry_is_long_enough (type, entry_len)) { \
				len -= entry_len;			\
				_.ptr += entry_len;			\
				continue;				\
			}						\
			switch (type) {					\
				_cases;					\
			}						\
			len -= entry_len;				\
			_.ptr += entry_len;				\
		}							\
	} while (0)

  if (pa_apic_table == 0)
    {
      error ("No ACPI MADT table found.");
      return;
    }

  acpi_madt = load_table (pa_apic_table);
  if (acpi_madt == NULL)
    {
      error ("Could not load ACPI MADT Table.");
      return;
    }
  if (acpi_madt->hdr.length < sizeof (*acpi_madt))
    {
      warn ("ACPI MADT length %u < header length %u; skipping",
	    (unsigned) acpi_madt->hdr.length,
	    (unsigned) sizeof (*acpi_madt));
      unload_table (acpi_madt);
      return;
    }
  if (acpi_madt->hdr.length > ACPI_MAX_TBL)
    {
      warn ("ACPI MADT length %u > mapped limit %u; skipping",
	    (unsigned) acpi_madt->hdr.length, (unsigned) ACPI_MAX_TBL);
      unload_table (acpi_madt);
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

  if (nioapic == 0)
    {
      warn ("ACPI MADT has no IOAPIC entries; skipping GSI setup");
      unload_table (acpi_madt);
      return;
    }

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
	info ("ACPI MADT INTOVR BUS %02d IRQ: %02d GSI: %02u FL: %04x",
	       _.intovr->bus, _.intovr->irq, (unsigned) _.intovr->gsi,
	       _.intovr->flags);
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
