#include <stdio.h>

void
putchar (int ch)
{
  asm volatile ("mv a0, %0\n" "li a7, 1\n" "ecall\n"::"r" (ch):"a0", "a7");
}

void
exit (int status)
{
  printf ("Exit %d\n", exit);
  /* Should definitely tell SBI about this. */
  while (1);
}
