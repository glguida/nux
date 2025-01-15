#include <nux/syscalls.h>
#include <stdio.h>

int
putchar (int c)
{
  (void) syscall1 (4096, c);
  return c;
}

void
exit (int status)
{
  syscall1 (4097, status);
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

  return 42;
}
