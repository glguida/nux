/*
  APXH: An ELF boot-loader.
  Copyright (C) 2019 Gianluca Guida, glguida@tlbflush.org

  SPDX-License-Identifier:	BSD-2-Clause
*/

#include "project.h"

#define SERIAL_PORT 0x3f8

int
inb (int port)
{
  int ret;

  asm volatile ("xor %%eax, %%eax; inb %%dx, %%al":"=a" (ret):"d" (port));
  return ret;
}

void
outb (int port, int val)
{
  asm volatile ("outb %%al, %%dx"::"d" (port), "a" (val));
}

int
putchar (int c)
{
  const unsigned char *vptr = (const void *) 0xb8000;
  static int init = 0;
  static int x = 0;
  static int y = 0;

  if (!init)
    {
      int i;
      for (i = 0; i < 80 * 25; i++)
	*(unsigned char *) (vptr + i * 2) = 0;

      outb (SERIAL_PORT + 1, 0);
      outb (SERIAL_PORT + 3, 0x80);
      outb (SERIAL_PORT + 0, 3);
      outb (SERIAL_PORT + 1, 0);
      outb (SERIAL_PORT + 3, 3);
      outb (SERIAL_PORT + 2, 0xc7);
      outb (SERIAL_PORT + 4, 0xb);

      init = 1;
    }

  while (!(inb (SERIAL_PORT + 5) & 0x20));
  outb (SERIAL_PORT, c);

  if (c == '\n')
    {
      y += x / 80 + 1;
      x = 0;
      return c;
    }

  if (80 * y + x >= 80 * 25)
    {
      int i;
      memmove ((void *) vptr, (void *) vptr + 80 * 2, 80 * 2 * (25 - 1));
      for (i = 0; i < 80; i++)
	*(unsigned char *) (vptr + 80 * 2 * (25 - 1) + i * 2) = 0;
      y = 25 - 1;
      x = 0;
    }

  *(unsigned char *) ((void *) 0xb8000 + x++ * 2 + y * 80 * 2) = c;
  return c;
}


void
exit (int st)
{
  printf ("exit(%d) called.\n", st);

  while (1);
}
