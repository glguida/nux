/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida <glguida@tlbflush.org>

  SPDX-License-Identifier:	BSD-2-Clause
*/

#ifndef PLTACPI_INTERNAL_H
#define PLTACPI_INTERNAL_H

#include <stdint.h>
#include <nux/plt.h>

extern unsigned pltacpi_hpet_irq;

void lapic_init (uint64_t, unsigned);
void lapic_add (uint16_t, uint16_t);
void lapic_add_nmi (uint8_t, int);
void lapic_eoi (void);

void ioapic_init (unsigned no);
void ioapic_add (unsigned num, uint64_t base, unsigned irqbase);

void gsi_init (void);
void gsi_setup (unsigned i, unsigned irq, enum plt_irq_type mode);
void gsi_start (void);


void acpi_init (paddr_t rdsp);
void acpi_madt_scan (void);

void hw_cmos_write (uint8_t addr, uint8_t val);
void hw_delay (void);
void hw_reset_vector (uint32_t start);

bool hpet_init (paddr_t hpetpa);
void hpet_doirq (void);
bool acpi_hpet_scan (void);

#endif
