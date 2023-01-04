
#include <nux/syscalls.h>

int
putchar (const char c)
{
  (void) syscall1 (4096, c);
  return 0;
}

void
exit (int status)
{
  syscall1 (0, status);
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

  return 42;
}
