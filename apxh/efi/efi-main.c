/*
  APXH: An ELF boot-loader.
  Copyright (C) 2019 Gianluca Guida <glguida@tlbflush.org>

  SPDX-License-Identifier:	BSD-2-Clause
*/

#include <efi.h>
#include <efilib.h>

#include "internal.h"

static EFI_HANDLE image_handle;
static EFI_LOADED_IMAGE *img = NULL;

unsigned long
efi_allocate_maxaddr (unsigned long maxaddr)
{
  EFI_STATUS efi_status;
  void *addr;

  efi_status = uefi_call_wrapper (BS->AllocatePages, 4,
				  AllocateMaxAddress,
				  EfiLoaderData, 1, &maxaddr);
  if (EFI_ERROR (efi_status))
    {
      Print (L"Allocate Pages Failed: %r\n", efi_status);
      exit (-1);
    }

  addr = (void *) maxaddr;
  memset (addr, 0, 4096);
  return (unsigned long) addr;
}

int
putchar (int c)
{
  Print (L"%c", c);
  return c;
}

static void *payload_start;
static unsigned long payload_size;

EFI_STATUS
efi_getframebuffer (void)
{
  extern EFI_GUID GraphicsOutputProtocol;
  EFI_STATUS rc;
  UINTN infosize;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
  EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
  uint32_t r_mask, g_mask, b_mask, bpp, pitch, width, height;
  uint64_t addr, size;

  rc = LibLocateProtocol (&GraphicsOutputProtocol, (void **) &gop);
  if (rc != EFI_SUCCESS)
    {
      Print (L"Cannot locate Graphic Output Proto (%r)\n", rc);
      return rc;
    }

  if (gop->Mode == NULL)
    {
      Print (L"Mode not found in GOP.\n");
      return EFI_SUCCESS;
    }

  UINTN i;
  UINTN mmax = 0;
  UINTN rmax = 0;
  UINTN imax = gop->Mode->MaxMode;

  /* Search for highest (horizontal) resolution. */
  for (i = 0; i < imax; i++)
    {
      UINTN infosz;
      rc = uefi_call_wrapper (gop->QueryMode, 4, gop, i, &infosz, &info);
      if (rc != EFI_SUCCESS)
	continue;

      Print (L"EFI GFX Mode %d: %ldx%ld.\n",
	     i, info->HorizontalResolution, info->VerticalResolution);

      if ((UINTN) info->HorizontalResolution * info->VerticalResolution >
	  rmax)
	{
	  rmax =
	    (UINTN) info->HorizontalResolution * info->VerticalResolution;
	  mmax = i;
	}
    }

  Print (L"Setting mode %d\n", mmax);
  rc = uefi_call_wrapper (gop->SetMode, 2, gop, mmax);

  info = gop->Mode->Info;
  if (info == NULL)
    {
      Print (L"Info not found in GOP.\n");
      return EFI_SUCCESS;
    }

  switch (info->PixelFormat)
    {
    case PixelRedGreenBlueReserved8BitPerColor:
      r_mask = 0x0000ff;
      g_mask = 0x00ff00;
      b_mask = 0xff0000;
      bpp = 32;
      break;

    case PixelBlueGreenRedReserved8BitPerColor:
      r_mask = 0xff0000;
      g_mask = 0x00ff00;
      b_mask = 0x0000ff;
      bpp = 32;
      break;

    case PixelBitMask:
      {
	uint32_t mask;

	r_mask = info->PixelInformation.RedMask;
	g_mask = info->PixelInformation.GreenMask;
	b_mask = info->PixelInformation.BlueMask;

	mask = (r_mask | g_mask | b_mask
		| info->PixelInformation.ReservedMask);
	bpp = __builtin_popcountl ((long) mask);
	break;
      }
    case PixelBltOnly:
    default:
      Print (L"No Framebuffer (pixel format is %d)\n", info->PixelFormat);
      return EFI_SUCCESS;
    }

  addr = gop->Mode->FrameBufferBase;
  size = gop->Mode->FrameBufferSize;
  pitch = (uint32_t) ((uint64_t) info->PixelsPerScanLine * bpp / 8);
  width = info->HorizontalResolution;
  height = info->VerticalResolution;

  Print (L"Framebuffer found:\n"
	 "        ADDR: %lx\n        SIZE: %lx\n"
	 "        WIDTH: %d\n        HEIGHT: %d\n"
	 "        PITCH: %d\n        BPP: %d\n"
	 "        RMASK: %08lx        GMASK: %08lx        BMASK: %08lx\n",
	 addr, size, width, height, pitch, bpp, r_mask, g_mask, b_mask);

  apxhefi_add_framebuffer (addr, size, width, height, pitch, bpp, r_mask,
			   g_mask, b_mask);

}

EFI_STATUS
efi_getpayload (CHAR16 * name, void **ptr, unsigned long *size)
{
  EFI_STATUS rc;
  EFI_HANDLE hdl;
  SIMPLE_READ_FILE rdhdl;
  EFI_DEVICE_PATH *filepath;

  filepath = FileDevicePath (img->DeviceHandle, name);

  rc = OpenSimpleReadFile (TRUE, NULL, 0, &filepath, &hdl, &rdhdl);
  if (EFI_ERROR (rc))
    {
      Print (L"OpenSimpleReadFile failed %r\n", rc);
      return rc;
    }

  rc = EFI_SUCCESS;
  *ptr = NULL;
  *size = 8 * 1024 * 1024;

  while (GrowBuffer (&rc, ptr, *size))
    {
      Print (L"GrowBuffer!\n");
      rc = ReadSimpleReadFile (rdhdl, 0, size, *ptr);
    }

  if (EFI_ERROR (rc))
    {
      Print (L"ReadSimpleReadFile failed: %r\n", rc);
      return rc;
    }

  CloseSimpleReadFile (rdhdl);

  return EFI_SUCCESS;
}

void
efi_getmemorymap (void)
{
  UINTN num, i;
  UINTN key;
  UINTN descsize;
  UINT32 descver;
  EFI_MEMORY_DESCRIPTOR *md, *ptr;


  md = LibMemoryMap (&num, &key, &descsize, &descver);

  Print (L"Found %ld memory entries, Key: %ld, Size: %ld, Version: %d\n",
	 num, key, descsize, descver);

  for (i = 0; i < num; i++)
    {
      int ram, bsy;
      unsigned len;
      unsigned long pfn;

      ptr = (void *) md + i * descsize;

      switch (ptr->Type)
	{
	case EfiReservedMemoryType:
	case EfiUnusableMemory:
	case EfiACPIReclaimMemory:
	case EfiACPIMemoryNVS:
	case EfiMemoryMappedIO:
	  ram = 0;
	  bsy = 1;
	  break;

	case EfiLoaderCode:
	case EfiLoaderData:
	case EfiBootServicesCode:
	case EfiBootServicesData:
	case EfiConventionalMemory:
	  ram = 1;
	  bsy = 0;
	  break;

	case EfiRuntimeServicesCode:
	case EfiRuntimeServicesData:
	  ram = 1;
	  bsy = 1;
	  break;

	case EfiMemoryMappedIOPortSpace:
	case EfiPalCode:
	default:
	  continue;
	}

      pfn = ptr->PhysicalStart >> 12;
      len = ptr->NumberOfPages;

      apxhefi_add_memregion (ram, bsy, pfn, len);
    }
}

EFI_STATUS
efi_getrsdp (void)
{
  EFI_GUID guid_rsdp20 = ACPI_20_TABLE_GUID;
  EFI_GUID guid_rsdp = ACPI_TABLE_GUID;
  void *rsdp;

  LibGetSystemConfigurationTable (&guid_rsdp20, &rsdp);
  if (rsdp == NULL)
    LibGetSystemConfigurationTable (&guid_rsdp, &rsdp);

  if (rsdp == NULL)
    Print (L"No RSDP found!\n");

  apxhefi_add_rsdp (rsdp);
}


EFI_STATUS EFIAPI
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE * SystemTable)
{
  void *ptr;
  EFI_STATUS rc;
  EFI_GUID img_prot = LOADED_IMAGE_PROTOCOL;

  InitializeLib (ImageHandle, SystemTable);
  image_handle = ImageHandle;

  rc = uefi_call_wrapper (BS->OpenProtocol, 6, ImageHandle, &img_prot, &ptr,
			  ImageHandle, NULL,
			  EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
  if (rc != EFI_SUCCESS)
    {
      Print (L"Open Protocol failed: %r\n", rc);
      return rc;
    }

  img = ptr;

  /*
     Get payloads.
   */
  rc = efi_getpayload (L"kernel.elf", &payload_start, &payload_size);
  if (rc != EFI_SUCCESS)
    {
      Print (L"Could not load kernel.elf: %r\n", rc);
      return rc;
    }
  apxhefi_add_kernel_payload (payload_start, payload_size);

  rc = efi_getpayload (L"user.elf", &payload_start, &payload_size);
  if (rc == EFI_SUCCESS)
    {
      Print (L"Loading user.elf");
      apxhefi_add_user_payload (payload_start, payload_size);
    }

  /*
     Populate memory map.
   */
  efi_getmemorymap ();

  /*
     Get Framebuffer info.
   */
  efi_getframebuffer ();

  /*
     Get RSDP.
   */
  efi_getrsdp ();

  /*
     Launch APXH.
   */
  apxh_main (0, 0);

  return EFI_SUCCESS;
}

void
efi_exitbs (void)
{
  EFI_STATUS rc;
  UINTN num, i;
  UINTN key;
  UINTN descsize;
  UINT32 descver;
  EFI_MEMORY_DESCRIPTOR *md;


  md = LibMemoryMap (&num, &key, &descsize, &descver);

  rc = uefi_call_wrapper (BS->ExitBootServices, 2, image_handle, key);
  if (rc != EFI_SUCCESS)
    {
      Print (L"EBS failed: %r\n", rc);
      /* XXX: efi_exit */
      return;
    }
}

void
efi_exit (int st)
{
}
