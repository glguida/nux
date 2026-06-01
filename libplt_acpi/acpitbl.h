/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida <glguida@tlbflush.org>

  SPDX-License-Identifier:	BSD-2-Clause
*/

#ifndef PLTACPI_ACPITBL_H
#define PLTACPI_ACPITLB_H

#include <cdefs.h>
#include <stdint.h>

#define ACPI_MADT_TYPE_LAPIC 0
#define ACPI_MADT_TYPE_IOAPIC 1
#define ACPI_MADT_TYPE_INTOVERRIDE 2
#define ACPI_MADT_TYPE_NMISRC 3
#define ACPI_MADT_TYPE_LAPICNMI 4
#define ACPI_MADT_TYPE_LAPICOVERRIDE 5
#define ACPI_MADT_TYPE_IOSAPIC 6
#define ACPI_MADT_TYPE_LSAPIC 7
#define ACPI_MADT_TYPE_INTSRC 8
#define ACPI_MADT_TYPE_LX2APIC 9
#define ACPI_MADT_TYPE_LX2APICNMI 10
#define ACPI_MADT_TYPE_GENINT 11
#define ACPI_MADT_TYPE_GENDISTR 12

struct acpi_rsdp_thdr
{
  char signature[8];
  uint8_t checksum;
  char oemid[6];
  uint8_t revision;
  uint32_t rsdt;

  /* ACPI >= 2.0 (revision != 0) */
  uint32_t length;
  uint64_t xsdt;
  uint8_t xchecksum;
  uint8_t reserved[3];
} __packed;

struct acpi_thdr
{
  char signature[4];
  uint32_t length;
  uint8_t revision;
  uint8_t checksum;
  char oemid[6];
  char oemtableid[8];
  uint32_t oemrevision;
  uint32_t creatid;
  uint32_t creatrev;
} __packed;

struct acpi_genaddr
{
  uint8_t spaceid;
  uint8_t bitwidth;
  uint8_t bitoffset;
  uint8_t accesswidth;
  uint64_t address;
} __packed;

struct acpi_madt
{
  struct acpi_thdr hdr;
  uint32_t lapic;
  uint32_t flags;
} __packed;

struct acpi_madt_lapic
{
  uint8_t type;
  uint8_t length;
  uint8_t acpiid;
  uint8_t lapicid;
#define ACPI_MADT_LAPIC_ENABLED 1
  uint32_t flags;
} __packed;

struct acpi_madt_ioapic
{
  uint8_t type;
  uint8_t length;
  uint8_t ioapicid;
  uint8_t reserved;
  uint32_t address;
  uint32_t gsibase;
} __packed;

struct acpi_madt_lapicoverride
{
  uint8_t type;
  uint8_t length;
  uint16_t reserved;
  uint64_t address;
} __packed;

struct acpi_madt_lapicnmi
{
  uint8_t type;
  uint8_t length;
  uint8_t acpiid;
  uint16_t flags;
  uint8_t lint;
} __packed;

struct acpi_madt_intoverride
{
  uint8_t type;
  uint8_t length;
  uint8_t bus;
  uint8_t irq;
  uint32_t gsi;
#define ACPI_MADT_TRIGGER_MASK     0x0C
#define ACPI_MADT_TRIGGER_CONFORMS 0x00
#define ACPI_MADT_TRIGGER_EDGE     0x04
#define ACPI_MADT_TRIGGER_RESERVED 0x08
#define ACPI_MADT_TRIGGER_LEVEL    0x0C
#define ACPI_MADT_POLARITY_MASK        0x03
#define ACPI_MADT_POLARITY_CONFORMS    0x00
#define ACPI_MADT_POLARITY_ACTIVE_HIGH 0x01
#define ACPI_MADT_POLARITY_RESERVED    0x02
#define ACPI_MADT_POLARITY_ACTIVE_LOW  0x03
  uint16_t flags;
} __packed;

struct acpi_hpet
{
  struct acpi_thdr hdr;
  uint32_t id;
  struct acpi_genaddr address;
  uint8_t sequence;
  uint8_t mintick;
  uint8_t flags;
} __packed;

#endif
