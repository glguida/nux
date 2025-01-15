#
# top.mk
# Copyright (C) 2019 Gianluca Guida <glguida@tlbflush.org>
#  SPDX-License-Identifier:	BSD-2-Clause
#

.PHONY: all install clean
CC=@TARGET_CC@
CXX=@TARGET_CXX@
CCLD=@TARGET_CC@
LD=@TARGET_LD@
AR=@TARGET_AR@
OBJCOPY=@TARGET_OBJCOPY@
OBJDIR=@builddir@/@MACHINE@
SRCDIR=@srcdir@/
SRCROOT=@top_srcdir@/
BUILDDIR=@builddir@
BUILDROOT=@top_builddir@/
MKDIR=$(SRCROOT)/@mk_dir@
INSTALLDIR=@INSTALLDIR@
CFLAGS+=@CONFIGURE_FLAGS@

all: do_all

