#include <nux/syscalls.h>
#include <stdio.h>

#define UCTXT_SETA2_TEST_MAGIC 0x2a2a2a2UL
#define UADDR_MEMSET_TEST_BYTE 0xa5U
#define UADDR_MEMSET_TEST_SIZE 17U

void
putchar (int c)
{
  (void) syscall1 (4096, c);
}

void
exit (int status)
{
  syscall1 (4097, status);
}

int puts (const char *s);

static unsigned long
syscall_uctxt_seta2_probe (void)
{
#if defined(__i386__)
  unsigned long sys = 7;
  unsigned long a2 = 0;

  asm volatile ("int $0x21"
		: "+a" (sys), "+c" (a2)
		:
		: "memory");
  return a2;
#elif defined(__x86_64__)
  unsigned long sys = 7;
  unsigned long a2 = 0;

  asm volatile ("syscall"
		: "+a" (sys), "+d" (a2)
		:
		: "rcx", "r11", "memory");
  return a2;
#elif defined(__riscv) && __riscv_xlen == 64
  register unsigned long sys __asm__ ("a0") = 7;
  register unsigned long a2 __asm__ ("a2") = 0;

  asm volatile ("ecall"
		: "+r" (sys), "+r" (a2)
		:
		: "memory");
  return a2;
#else
#error Unsupported architecture for syscall_uctxt_seta2_probe
#endif
}

void
test (void)
{
  syscall0 (0);
  syscall1 (1, 1);
  syscall2 (2, 1, 2);
  syscall3 (3, 1, 2, 3);
  syscall4 (4, 1, 2, 3, 4);
  syscall5 (5, 1, 2, 3, 4, 5);
  syscall6 (6, 1, 2, 3, 4, 5, 6);

  if (syscall_uctxt_seta2_probe () != UCTXT_SETA2_TEST_MAGIC)
    {
      puts ("UCTXT_SETA2 user test failed.\n");
      exit (100);
    }

  puts ("UCTXT_SETA2 user test passed.\n");
}

static void
syscall_uaddr_memset_probe (void)
{
  volatile unsigned char buf[UADDR_MEMSET_TEST_SIZE];

  for (unsigned i = 0; i < sizeof (buf); i++)
    buf[i] = (unsigned char) i;

  (void) syscall3 (8, (unsigned long) buf, UADDR_MEMSET_TEST_BYTE,
		   sizeof (buf));

  for (unsigned i = 0; i < sizeof (buf); i++)
    {
      if (buf[i] != (unsigned char) UADDR_MEMSET_TEST_BYTE)
	{
	  puts ("UADDR_MEMSET user test failed.\n");
	  exit (101);
	}
    }

  puts ("UADDR_MEMSET user test passed.\n");
}

int
puts (const char *s)
{
  char c;

  while ((c = *s++) != '\0')
    putchar (c);

  return 0;
}

int
main (void)
{
  puts ("Hello from userspace, NUX!\n");

  test ();
  syscall_uaddr_memset_probe ();

  return 42;
}
