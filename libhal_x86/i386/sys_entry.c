#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>
#include <nux/hal.h>

#include "i386.h"
#include "../internal.h"

#if 0
static char *exceptions[] = {
  "Divide by zero exception",
  "Debug exception",
  "NMI",
  "Overflow exception",
  "Breakpoint exception",
  "Bound range exceeded",
  "Invalid opcode",
  "No math coprocessor",
  "Double fault",
  "Coprocessor segment overrun",
  "Invalid TSS",
  "Segment not present",
  "Stack segment fault",
  "General protection fault",
  "Page fault",
  "Reserved exception",
  "Floating point error",
  "Alignment check fault",
  "Machine check fault",
  "SIMD Floating-Point Exception",
};
#endif


struct hal_frame *
do_nmi (uint32_t vect, struct hal_frame *f)
{
  return hal_entry_nmi (f);
}

struct hal_frame *
do_xcpt (uint32_t vect, struct hal_frame *f)
{
  struct hal_frame *rf;

  if (vect == 14)
    {
      unsigned xcpterr;

      /* Page Fault. */

      if (f->err & 1)
	{
	  xcpterr = HAL_PF_REASON_PROT;
	}
      else
	{
	  xcpterr = HAL_PF_REASON_NOTP;
	}

      if (f->err & 2)
	xcpterr |= HAL_PF_INFO_WRITE;

      if (f->err & 4)
	xcpterr |= HAL_PF_INFO_USER;

      rf = hal_entry_pf (f, f->cr2, xcpterr);
    }
  else
    {
      rf = hal_entry_xcpt (f, vect);
    }

  return rf;
}

struct hal_frame *
do_syscall (struct hal_frame *f)
{
  assert (f->cs == UCS);

  return hal_entry_syscall (f, f->eax, f->edi, f->esi, f->ecx, f->edx, f->ebx);
}

struct hal_frame *
do_intr (uint32_t vect, struct hal_frame *f)
{

  return hal_entry_vect (f, vect);
}

void
hal_frame_init (struct hal_frame *f)
{
  memset(f, 0, sizeof(*f));
  f->eip = 0;
  f->esp = 0;

  f->cs = UCS;
  f->ds = UDS;
  f->es = UDS;
  f->fs = UDS;
  f->gs = UDS;
  f->ss = UDS;

  f->eflags = 0x202;
}

bool
hal_frame_isuser (struct hal_frame *f)
{
  return f->cs == UCS;
}

vaddr_t
hal_frame_getip (struct hal_frame *f);

void
hal_frame_setip (struct hal_frame *f, vaddr_t ip)
{
  f->eip = ip;
}

vaddr_t
hal_frame_getsp (struct hal_frame *f);

void
hal_frame_setsp (struct hal_frame *f, vaddr_t sp)
{
  f->esp = sp;
}

void
hal_frame_seta0 (struct hal_frame *f, unsigned long a0)
{
  f->eax = a0;
}

void
hal_frame_seta1 (struct hal_frame *f, unsigned long a1)
{
  f->edx = a1;
}

void
hal_frame_seta2 (struct hal_frame *f, unsigned long a2)
{
  f->ecx = a2;
}

void
hal_frame_setret (struct hal_frame *f, unsigned long r)
{
  f->eax = r;
}

bool
hal_frame_signal (struct hal_frame *f, unsigned long ip, unsigned long arg)
{
  vaddr_t start, end;
  struct stackframe {
    uint32_t arg;
    uint32_t eip;
  } __packed sf = {
    .arg = arg,
    .eip = f->eip,
  };

  /*
    Check that the addresses we will write are all in userspace.
  */
  start = f->esp - sizeof (sf);
  end = f->esp;

  if ((start > end)
      || (start < hal_virtmem_userbase ())
      || (start > (hal_virtmem_userbase () + hal_virtmem_usersize ()))
      || (end < hal_virtmem_userbase ())
      || (end > (hal_virtmem_userbase () + hal_virtmem_usersize ())))
    return false;

  /* Write to userspace. Kernel is responsible for recovering from faults. */
  memcpy ((void *)start, &sf, sizeof (sf));

  /* Change frame with new IP and SP. */
  f->esp = (uint32_t)start;
  f->eip = ip;
  return true;
}

void
hal_frame_print (struct hal_frame *f)
{

  hallog ("EAX: %08x EBX: %08x ECX: %08x EDX:%08x",
	  f->eax, f->ebx, f->ecx, f->edx);
  hallog ("EDI: %08x ESI: %08x EBP: %08x ESP:%08x",
	  f->edi, f->esi, f->ebp, f->esp);
  hallog (" CS: %04x     EIP: %08x EFL: %08x",
	  (int) f->cs, f->eip, f->eflags);
  hallog (" DS: %04x      ES: %04x     FS: %04x      GS: %04x",
	  f->ds, f->es, f->fs, f->gs);
  hallog ("CR3: %08x CR2: %08x err: %08x", f->cr3, f->cr2, f->err);
}
