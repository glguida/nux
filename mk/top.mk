#
# top.mk
# Copyright (C) 2019 Gianluca Guida <glguida@tlbflush.org>
#  SPDX-License-Identifier:	GPL2.0+
#

.PHONY: all install clean
CC=@CC@
CCLD=@CC@
LD=@LD@
AR=@AR@
OBJCOPY=@OBJCOPY@
OBJDIR=.build/@MACHINE@
SRCDIR=@srcdir@/
SRCROOT=@top_srcdir@/
BUILDROOT=@top_builddir@/
MKDIR=$(SRCROOT)/@mk_dir@
INSTALLDIR=@INSTALLDIR@
CFLAGS+=@CONFIGURE_FLAGS@

all: do_all

