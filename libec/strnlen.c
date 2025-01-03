/*	$NetBSD: strnlen.c,v 1.2 2014/01/09 11:25:11 apb Exp $	*/

/*-
 * Copyright (c) 2009 David Schultz <das@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _EC_SOURCE

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID ("$NetBSD: strnlen.c,v 1.2 2014/01/09 11:25:11 apb Exp $");
#endif /* LIBC_SCCS and not lint */
/* FreeBSD: src/lib/libc/string/strnlen.c,v 1.1 2009/02/28 06:00:58 das Exp */

#if !defined(_KERNEL) && !defined(_STANDALONE)
#include <string.h>
#else
#include <lib/libkern/libkern.h>
#endif

#else /* _EC_SOURCE */

#define _DIAGASSERT(_x)
#include <cdefs.h>
#include <inttypes.h>
#include <string.h>

#endif /* _EC_SOURCE */

#if !HAVE_STRNLEN
size_t
strnlen (const char *s, size_t maxlen)
{
  size_t len;

  for (len = 0; len < maxlen; len++, s++)
    {
      if (!*s)
	break;
    }
  return (len);
}
#endif /* !HAVE_STRNLEN */
